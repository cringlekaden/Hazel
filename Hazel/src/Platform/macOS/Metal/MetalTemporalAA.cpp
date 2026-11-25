#include "MetalTemporalAA.h"
#include "MetalDevice.h"
#include "MetalRendererAPI.h"
#include "MetalRenderer2DFrame.h"
#include "Hazel/Renderer/Scene/SceneRenderer.h"
#include "Hazel/Core/FileSystem.h"
#include "Hazel/Core/Resources.h"
#include <dispatch/dispatch.h>
#include <algorithm>
#include <cstring>
#include <cstdio>
#include <cmath>
#include <stdexcept>
namespace Hazel {
namespace {void Check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}}
MetalTemporalPipeline::MetalTemporalPipeline(MetalDevice& device)
{
    ScopedBuffer file(FileSystem::ReadFileBinary(Resources::Resolve("shaders/metal/TemporalAA.metallib")));
    Check(bool(file)&&file.Size()<16*1024*1024,"Missing/oversized TemporalAA metallib");
    auto data=dispatch_data_create(file.Data(),file.Size(),nullptr,DISPATCH_DATA_DESTRUCTOR_DEFAULT);
    NS::Error* error=nullptr;
    Library=NS::TransferPtr(device.NativeDevice()->newLibrary(data,&error));dispatch_release(data);
    Check(bool(Library),"Cannot load TemporalAA library");
    auto cd=NS::TransferPtr(MTL4::CompilerDescriptor::alloc()->init());
    auto compiler=NS::TransferPtr(device.NativeDevice()->newCompiler(cd.get(),&error));
    Check(bool(compiler),"Cannot create temporal Metal 4 compiler");
    auto function=NS::TransferPtr(MTL4::LibraryFunctionDescriptor::alloc()->init());
    function->setLibrary(Library.get());function->setName(NS::String::string("temporalResolve",NS::UTF8StringEncoding));
    auto descriptor=NS::TransferPtr(MTL4::ComputePipelineDescriptor::alloc()->init());
    auto options=NS::TransferPtr(MTL4::PipelineOptions::alloc()->init());
    options->setShaderReflection(MTL4::ShaderReflectionBindingInfo|MTL4::ShaderReflectionBufferTypeInfo);
    descriptor->setOptions(options.get());descriptor->setComputeFunctionDescriptor(function.get());
    State=NS::TransferPtr(compiler->newComputePipelineState(descriptor.get(),nullptr,&error));
    if(!State)throw std::runtime_error(error?error->localizedDescription()->utf8String():"Cannot compile temporal resolve");
    auto* reflection=State->reflection();Check(reflection&&reflection->bindings(),"Temporal reflection missing");
    unsigned buffers=0,textures=0;
    for(size_t i=0;i<reflection->bindings()->count();++i) {
        auto* binding=reflection->bindings()->object<MTL::Binding>(i);if(!binding->isUsed())continue;
        if(binding->type()==MTL::BindingTypeBuffer) {
            Check(binding->index()==0 && binding->access()==MTL::BindingAccessReadOnly &&
                static_cast<MTL::BufferBinding*>(binding)->bufferDataSize()==sizeof(MetalTemporalConstants),"Temporal constant ABI differs");++buffers;
        } else if(binding->type()==MTL::BindingTypeTexture) {
            auto* texture=static_cast<MTL::TextureBinding*>(binding);auto index=texture->index();
            Check(index<12 && index!=4 && texture->textureType()==MTL::TextureType2D && texture->arrayLength()==1 &&
                texture->textureDataType()==((index==5||index==6||index==10||index==11)?MTL::DataTypeInt:MTL::DataTypeFloat) &&
                texture->access()==((index==7||index==8||index==11)?MTL::BindingAccessWriteOnly:MTL::BindingAccessReadOnly),"Temporal texture ABI differs");++textures;
        } else throw std::runtime_error("Unexpected temporal binding");
    }
    Check(buffers==1&&textures==11&&State->maxTotalThreadsPerThreadgroup()>=64,"Temporal resource/threadgroup ABI differs");
}
MetalTemporalCounters::MetalTemporalCounters(MetalDevice& device)
{
    auto descriptor=NS::TransferPtr(MTL4::CounterHeapDescriptor::alloc()->init());
    descriptor->setType(MTL4::CounterHeapTypeTimestamp);descriptor->setCount(4);
    NS::Error* error=nullptr;Heap=NS::TransferPtr(device.NativeDevice()->newCounterHeap(descriptor.get(),&error));
    Frequency=device.NativeDevice()->queryTimestampFrequency();
}
void MetalFramebuffer::SetTemporalProducer()
{
    Check(!m_Frame && m_Specification.Attachments.Attachments.size()==3,"Temporal scratch requires an empty HDR framebuffer");
    if(!m_TemporalProducer)Check(!m_SubmittedGeneration&&m_Available.empty(),"Temporal scratch must be configured before first use");
    m_TemporalProducer=true;
}
void MetalFramebuffer::ResolveTemporal(const Ref<Framebuffer>& radiance,const Ref<Framebuffer>& motion,
    glm::vec2 jitter,bool valid,unsigned debug,const glm::mat4& skyMotion,unsigned skyMode,const std::shared_ptr<std::atomic<bool>>& submitted,
    const std::shared_ptr<TemporalTiming>& timing)
{
    auto source=std::dynamic_pointer_cast<MetalFramebuffer>(radiance),vectors=std::dynamic_pointer_cast<MetalFramebuffer>(motion);
    Check(source&&vectors&&source.get()!=this&&vectors.get()!=this&&source!=vectors&&source->m_Device==m_Device&&vectors->m_Device==m_Device&&
        source->m_TemporalProducer&&vectors->m_TemporalProducer&&m_Frame&&source->m_Frame&&vectors->m_Frame&&Backend().IsFramebufferBound(m_Frame)&&
        !Backend().IsFramebufferBound(source->m_Frame)&&!Backend().IsFramebufferBound(vectors->m_Frame)&&m_Frame->Draws.empty()&&
        !m_TemporalProducer&&m_Specification.Attachments.Attachments.size()==3&&
        m_Frame->Width==source->m_Frame->Width&&m_Frame->Height==source->m_Frame->Height&&
        m_Frame->Width==vectors->m_Frame->Width&&m_Frame->Height==vectors->m_Frame->Height&&submitted&&timing&&debug<=5&&skyMode<=2&&std::isfinite(jitter.x)&&std::isfinite(jitter.y)&&std::abs(jitter.x)<=.5f&&std::abs(jitter.y)<=.5f,
        "Temporal resolve requires private equal-size unbound producers and empty bound destination");
    if(!m_TemporalPipeline)m_TemporalPipeline=std::make_shared<MetalTemporalPipeline>(*m_Device);
    if(m_TemporalPrevious&&(!m_TemporalPrevious->Submitted->load()||m_TemporalPrevious->Color->Width!=m_Frame->Width||m_TemporalPrevious->Color->Height!=m_Frame->Height))m_TemporalPrevious.reset();
    auto pass=std::make_shared<MetalTemporalPass>();pass->Pipeline=m_TemporalPipeline;pass->Timing=timing;
    pass->Previous=valid?m_TemporalPrevious:nullptr;
    // Private GPU-only ping-pong. Queue barriers retire every earlier reader
    // before either texture is overwritten; no CPU or presentation reads it.
    m_TemporalColors.erase(std::remove_if(m_TemporalColors.begin(),m_TemporalColors.end(),[&](auto& candidate){
        return candidate->Width!=m_Frame->Width||candidate->Height!=m_Frame->Height;
    }),m_TemporalColors.end());
    pass->Current=std::make_shared<MetalTemporalHistory>();pass->Current->Submitted=submitted;
    for(auto& candidate:m_TemporalColors)if(!pass->Previous||candidate!=pass->Previous->Color) {
        pass->Current->Color=candidate;++timing->Reuses;break;
    }
    if(!pass->Current->Color) {
        Check(m_TemporalColors.size()<2,"Temporal ping-pong history exceeds two generations");
        pass->Current->Color=Allocate(m_Frame->Width,m_Frame->Height,true);m_TemporalColors.push_back(pass->Current->Color);++timing->Allocations;
    }
    for(auto& candidate:m_TemporalCounters)if(candidate.use_count()==1){pass->CounterOwner=candidate;break;}
    if(!pass->CounterOwner) {
        Check(m_TemporalCounters.size()<4,"Temporal counter heaps exceed three slots and a CPU packet");
        pass->CounterOwner=std::make_shared<MetalTemporalCounters>(*m_Device);m_TemporalCounters.push_back(pass->CounterOwner);
    }
    pass->Counters=pass->CounterOwner->Heap;pass->Frequency=pass->CounterOwner->Frequency;
    pass->Constants.ExtentJitter={float(m_Frame->Width),float(m_Frame->Height),jitter.x,jitter.y};
    pass->Constants.Options={pass->Previous?1u:0u,debug,skyMode,0};pass->Constants.SkyMotion=skyMotion;
    m_TemporalPrevious=pass->Current;
    timing->HistoryBytes=0;for(auto& color:m_TemporalColors)for(auto& texture:color->Targets)if(texture)timing->HistoryBytes+=texture->allocatedSize();
    m_Frame->Temporal=std::move(pass);m_Frame->TemporalSource=std::move(source->m_Frame);m_Frame->MotionSource=std::move(vectors->m_Frame);
    m_Frame->TemporalSource->Readback=m_Frame->MotionSource->Readback=false;
    source->m_SubmittedGeneration=vectors->m_SubmittedGeneration=true;
}
void MetalDevice::PrepareTemporal(const std::shared_ptr<MetalRenderer2DFrame>& frame)
{
    auto& pass=*frame->Temporal;pass.Serial=m_Serial+1;pass.Trace=m_Trace;
    const auto& motion=*frame->MotionSource->Attachments;
    auto table=NS::TransferPtr(MTL4::ArgumentTableDescriptor::alloc()->init());
    table->setInitializeBindings(true);table->setMaxBufferBindCount(1);table->setMaxTextureBindCount(12);
    NS::Error* error=nullptr;pass.Arguments=NS::TransferPtr(m_Device->newArgumentTable(table.get(),&error));
    Check(bool(pass.Arguments),"Cannot allocate temporal arguments");
    auto buffer=AcquireRenderer2DBuffer(sizeof(pass.Constants),&pass.Constants);
    pass.Arguments->setAddress(buffer->gpuAddress(),0);frame->Residency->addAllocation(buffer.get());frame->Buffers.push_back(std::move(buffer));
    MTL::Texture* textures[]={frame->TemporalSource->Attachments->Targets[0].get(),motion.Targets[0].get(),motion.Targets[2].get(),
        pass.Previous?pass.Previous->Color->Targets[0].get():frame->TemporalSource->Attachments->Targets[0].get(),nullptr,
        motion.Targets[1].get(),pass.Previous?pass.Previous->Color->Targets[1].get():motion.Targets[1].get(),
        pass.Current->Color->Targets[0].get(),frame->Attachments->Targets[0].get(),
        frame->TemporalSource->Attachments->Targets[2].get(),frame->TemporalSource->Attachments->Targets[1].get(),pass.Current->Color->Targets[1].get()};
    for(unsigned i=0;i<12;++i)if(textures[i]){pass.Arguments->setTexture(textures[i]->gpuResourceID(),i);frame->Residency->addAllocation(textures[i]);}
    if(pass.Counters)pass.Counters->invalidateCounterRange(NS::Range::Make(0,4));
}
void MetalDevice::EncodeTemporal(const std::shared_ptr<MetalRenderer2DFrame>& frame)
{
    auto& pass=*frame->Temporal;
    if(pass.Counters)m_Commands->writeTimestampIntoHeap(pass.Counters.get(),0);
    m_Commands->useResidencySet(frame->TemporalSource->Residency.get());EncodeRenderer2DFrame(frame->TemporalSource);
    if(pass.Counters)m_Commands->writeTimestampIntoHeap(pass.Counters.get(),1);
    m_Commands->useResidencySet(frame->MotionSource->Residency.get());EncodeRenderer2DFrame(frame->MotionSource);
    auto* compute=m_Commands->computeCommandEncoder();
    if(!compute){m_Commands->endCommandBuffer();throw std::runtime_error("Cannot encode temporal resolve");}
    compute->barrierAfterQueueStages(MTL::StageAll,MTL::StageDispatch|MTL::StageBlit,MTL4::VisibilityOptionDevice);
    if(pass.Counters)compute->writeTimestamp(MTL4::TimestampGranularityPrecise,pass.Counters.get(),2);
    compute->setComputePipelineState(pass.Pipeline->State.get());compute->setArgumentTable(pass.Arguments.get());
    compute->dispatchThreads({frame->Width,frame->Height,1},{8,8,1});
    if(pass.Counters)compute->writeTimestamp(MTL4::TimestampGranularityPrecise,pass.Counters.get(),3);
    for(unsigned i=1;i<3;++i)compute->copyFromTexture(frame->MotionSource->Attachments->Targets[i].get(),frame->Attachments->Targets[i].get());
    compute->endEncoding();
}
void MetalTemporalPass::ResolveTimings()
{
    if(TimingsResolved||!Counters||!Timing||!Frequency)return;
    TimingsResolved=true;
    auto data=NS::RetainPtr(Counters->resolveCounterRange(NS::Range::Make(0,4)));
    if(!data||data->length()!=4*sizeof(MTL4::TimestampHeapEntry))return;
    MTL4::TimestampHeapEntry entries[4];std::memcpy(entries,data->bytes(),sizeof(entries));
    for(unsigned i=0;i<3;++i)if(!entries[i].timestamp||entries[i+1].timestamp<=entries[i].timestamp)return;
    Timing->ShadingMilliseconds=double(entries[1].timestamp-entries[0].timestamp)*1000/double(Frequency);
    Timing->MotionMilliseconds=double(entries[2].timestamp-entries[1].timestamp)*1000/double(Frequency);
    Timing->ResolveMilliseconds=double(entries[3].timestamp-entries[2].timestamp)*1000/double(Frequency);Timing->Valid=true;
    if(Trace)std::printf("TAA_GPU frame=%llu pixels=%ux%u resolve_ms=%.6f history_bytes=%llu allocations=%llu reuses=%llu shading_ms=%.6f motion_ms=%.6f\n",
        (unsigned long long)Serial,unsigned(Constants.ExtentJitter.x),unsigned(Constants.ExtentJitter.y),Timing->ResolveMilliseconds,
        (unsigned long long)Timing->HistoryBytes,(unsigned long long)Timing->Allocations,(unsigned long long)Timing->Reuses,Timing->ShadingMilliseconds,Timing->MotionMilliseconds);
}
}
