#include "MetalAmbientOcclusion.h"
#include "MetalDevice.h"
#include "MetalRenderer2DFrame.h"
#include "MetalFramebuffer.h"
#include "Hazel/Renderer/Scene/SceneRenderer.h"
#include <cstring>
#include <cstdio>
#include "Hazel/Core/Resources.h"
#include "Hazel/Core/FileSystem.h"
#include <dispatch/dispatch.h>
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>
#include <algorithm>
#include <stdexcept>
namespace Hazel {
namespace {
void Check(bool ok,const char* message) { if(!ok) throw std::runtime_error(message); }
}
MetalAOPipelines::MetalAOPipelines(MetalDevice& device)
{
    auto pool=NS::TransferPtr(NS::AutoreleasePool::alloc()->init());
    ScopedBuffer file(FileSystem::ReadFileBinary(Resources::Resolve("shaders/metal/XeGTAO.metallib")));
    Check(bool(file) && file.Size()<16*1024*1024,"Missing/oversized XeGTAO metallib; rebuild packaged shaders");
    auto data=dispatch_data_create(file.Data(),file.Size(),nullptr,DISPATCH_DATA_DESTRUCTOR_DEFAULT);
    NS::Error* error=nullptr;
    Library=NS::TransferPtr(device.NativeDevice()->newLibrary(data,&error));dispatch_release(data);
    Check(bool(Library),"Cannot load XeGTAO metallib");
    auto descriptor=NS::TransferPtr(MTL4::CompilerDescriptor::alloc()->init());
    auto compiler=NS::TransferPtr(device.NativeDevice()->newCompiler(descriptor.get(),&error));
    Check(bool(compiler),"Cannot create XeGTAO Metal 4 compiler");
    const char* names[]={"aoDepth","aoMip","aoMainLow","aoMainMedium","aoMainHigh","aoMainUltra","aoDenoise"};
    for(size_t i=0;i<States.size();++i) {
        auto function=NS::TransferPtr(MTL4::LibraryFunctionDescriptor::alloc()->init());
        function->setLibrary(Library.get());function->setName(NS::String::string(names[i],NS::UTF8StringEncoding));
        auto pipeline=NS::TransferPtr(MTL4::ComputePipelineDescriptor::alloc()->init());
        pipeline->setComputeFunctionDescriptor(function.get());
        auto options=NS::TransferPtr(MTL4::PipelineOptions::alloc()->init());
        options->setShaderReflection(MTL4::ShaderReflectionBindingInfo|MTL4::ShaderReflectionBufferTypeInfo);
        pipeline->setOptions(options.get());
        States[i]=NS::TransferPtr(compiler->newComputePipelineState(pipeline.get(),nullptr,&error));
        if(!States[i]) throw std::runtime_error(error?error->localizedDescription()->utf8String():"XeGTAO pipeline creation failed");
        const auto* reflection=States[i]->reflection();
        Check(reflection && reflection->bindings(),"XeGTAO reflection missing");
        unsigned buffers=0,textures=0;
        for(size_t j=0;j<reflection->bindings()->count();++j) {
            const auto* binding=reflection->bindings()->object<MTL::Binding>(j);
            if(!binding->isUsed())continue;
            if(binding->type()==MTL::BindingTypeBuffer) {
                const auto* buffer=static_cast<const MTL::BufferBinding*>(binding);
                Check(binding->index()==0 && binding->access()==MTL::BindingAccessReadOnly && buffer->bufferDataSize()==sizeof(MetalAOConstants),"XeGTAO constant ABI differs");
                ++buffers;
            } else if(binding->type()==MTL::BindingTypeTexture) {
                const auto* texture=static_cast<const MTL::TextureBinding*>(binding);
                const auto index=texture->index();
                Check(index<(i<2?2u:3u) && texture->textureType()==MTL::TextureType2D && texture->textureDataType()==MTL::DataTypeFloat && texture->arrayLength()==1 &&
                    texture->access()==((index==1 || (i>=2 && i<=5 && index==2))?MTL::BindingAccessWriteOnly:MTL::BindingAccessReadOnly),"XeGTAO texture ABI differs");
                ++textures;
            } else throw std::runtime_error("Unexpected XeGTAO binding");
        }
        Check(buffers==1 && textures==(i<2?2u:3u),"XeGTAO binding count differs");
        Check(States[i]->maxTotalThreadsPerThreadgroup()>=64,"XeGTAO requires 8x8 threadgroups");
    }
}
MetalAOResources::MetalAOResources(MetalDevice& device,std::shared_ptr<MetalAOPipelines> pipelines,uint32_t width,uint32_t height)
    : Width(width),Height(height),Pipelines(std::move(pipelines))
{
    auto descriptor=NS::TransferPtr(MTL::TextureDescriptor::alloc()->init());
    descriptor->setTextureType(MTL::TextureType2D);descriptor->setWidth(width);descriptor->setHeight(height);
    descriptor->setStorageMode(MTL::StorageModePrivate);
    descriptor->setUsage(MTL::TextureUsageShaderRead|MTL::TextureUsageShaderWrite|MTL::TextureUsagePixelFormatView);
    descriptor->setPixelFormat(MTL::PixelFormatR32Float);
    unsigned levels=1;for(unsigned extent=std::max(width,height);extent>1 && levels<5;extent/=2)++levels;
    descriptor->setMipmapLevelCount(levels);
    Depth=NS::TransferPtr(device.NativeDevice()->newTexture(descriptor.get()));Check(bool(Depth),"Cannot allocate XeGTAO depths");
    for(unsigned i=0;i<levels;++i) {
        auto view=NS::TransferPtr(Depth->newTextureView(MTL::PixelFormatR32Float,MTL::TextureType2D,NS::Range::Make(i,1),NS::Range::Make(0,1)));
        Check(bool(view),"Cannot allocate XeGTAO mip view");DepthMips.push_back(std::move(view));
    }
    descriptor->setMipmapLevelCount(1);descriptor->setPixelFormat(MTL::PixelFormatR16Float);
    for(auto* texture:{&Raw,&Scratch,&Visibility}) {
        *texture=NS::TransferPtr(device.NativeDevice()->newTexture(descriptor.get()));Check(bool(*texture),"Cannot allocate XeGTAO visibility");
    }
    descriptor->setPixelFormat(MTL::PixelFormatRGBA8Unorm);
    Edges=NS::TransferPtr(device.NativeDevice()->newTexture(descriptor.get()));Check(bool(Edges),"Cannot allocate XeGTAO edges");
    TimestampFrequency=device.NativeDevice()->queryTimestampFrequency();
    auto counters=NS::TransferPtr(MTL4::CounterHeapDescriptor::alloc()->init());
    counters->setType(MTL4::CounterHeapTypeTimestamp);counters->setCount(6);
    NS::Error* error=nullptr;Counters=NS::TransferPtr(device.NativeDevice()->newCounterHeap(counters.get(),&error));
    if(!Counters || !TimestampFrequency)Counters.reset();
}
MetalAOConstants AmbientOcclusionConstants(const SceneCameraView& camera,const AmbientOcclusionSettings& settings,uint32_t width,uint32_t height,unsigned temporalPhase)
{
    settings.Validate();
    MetalAOConstants c;c.InverseVP=glm::inverse(camera.ViewProjection);
    auto point=[&](glm::vec3 p) {auto v=c.InverseVP*glm::vec4(p,1);return glm::vec3(v)/v.w;};
    const auto origin=point({0,0,-1});
    const auto right=glm::normalize(point({1,0,-1})-origin);
    const auto up=glm::normalize(point({0,1,-1})-origin);
    const auto forward=temporalPhase?-glm::normalize(camera.ToCamera):glm::normalize(point({0,0,0})-origin);
    Check(std::isfinite(glm::dot(forward,forward)) && std::abs(glm::dot(right,up))<1e-4f &&
        std::abs(glm::dot(right,forward))<1e-4f && std::abs(glm::dot(up,forward))<1e-4f,
        "XeGTAO requires a finite symmetric perspective or orthographic camera");
    // Positive view Z, right/up matching Hazel's lower-left attachment rows.
    c.WorldToView=glm::mat4(1);
    for(int i=0;i<3;++i) {c.WorldToView[i][0]=right[i];c.WorldToView[i][1]=up[i];c.WorldToView[i][2]=forward[i];}
    const auto eye=camera.Orthographic?origin-forward*.001f:camera.Position;
    c.WorldToView[3]=glm::vec4(-glm::dot(right,eye),-glm::dot(up,eye),-glm::dot(forward,eye),1);
    const auto bottomLeft=glm::vec3(c.WorldToView*glm::vec4(point({-1,-1,-1}),1));
    const auto topRight=glm::vec3(c.WorldToView*glm::vec4(point({1,1,-1}),1));
    const float projectionScale=camera.Orthographic?1.f:bottomLeft.z;
    Check(std::isfinite(projectionScale) && projectionScale>0,"XeGTAO requires positive finite near depth");
    c.PositionScaleBias=glm::vec4((glm::vec2(topRight)-glm::vec2(bottomLeft))/projectionScale,glm::vec2(bottomLeft)/projectionScale);
    c.ExtentRadius={float(width),float(height),settings.Radius,settings.Falloff};
    constexpr unsigned slices[]={1,2,3,9},steps[]={2,2,3,3};
    c.Quality={slices[settings.Quality],steps[settings.Quality],camera.Orthographic?1u:0u,0};
    return c;
}
void MetalDevice::PrepareAmbientOcclusion(const std::shared_ptr<MetalRenderer2DFrame>& frame)
{
    auto& pass=*frame->AO;auto& r=*pass.Resources;
    pass.TimestampFrequency=r.TimestampFrequency;pass.Counters=r.Counters;
    pass.Trace=m_Trace;pass.Serial=m_Serial+1;
    if(pass.Counters)pass.Counters->invalidateCounterRange(NS::Range::Make(0,6));
    for(auto texture:{r.Depth,r.Raw,r.Edges,r.Scratch,r.Visibility})frame->Residency->addAllocation(texture.get());
    auto table=NS::TransferPtr(MTL4::ArgumentTableDescriptor::alloc()->init());
    table->setInitializeBindings(true);table->setMaxBufferBindCount(1);table->setMaxTextureBindCount(3);
    auto add=[&](MTL::Texture* input,MTL::Texture* output,MTL::Texture* edges,bool final) {
        auto constants=pass.Constants;if(final || (edges==r.Edges.get() && output!=r.Raw.get()))constants.Quality.w=final?1u:0u;
        auto buffer=AcquireRenderer2DBuffer(sizeof(constants),&constants);
        NS::Error* error=nullptr;auto args=NS::TransferPtr(m_Device->newArgumentTable(table.get(),&error));
        Check(bool(args),"Cannot allocate XeGTAO arguments");args->setAddress(buffer->gpuAddress(),0);
        args->setTexture(input->gpuResourceID(),0);args->setTexture(output->gpuResourceID(),1);
        if(edges)args->setTexture(edges->gpuResourceID(),2);
        frame->Residency->addAllocation(buffer.get());frame->Buffers.push_back(std::move(buffer));pass.Arguments.push_back(std::move(args));
    };
    add(frame->AOSource->Attachments->Targets[2].get(),r.DepthMips[0].get(),nullptr,false);
    for(size_t i=1;i<r.DepthMips.size();++i)add(r.DepthMips[i-1].get(),r.DepthMips[i].get(),nullptr,false);
    add(r.Depth.get(),r.Raw.get(),r.Edges.get(),false);
    if(pass.Constants.Quality.x>=3) {
        add(r.Raw.get(),r.Visibility.get(),r.Edges.get(),false);
        add(r.Visibility.get(),r.Scratch.get(),r.Edges.get(),false);
    } else add(r.Raw.get(),r.Scratch.get(),r.Edges.get(),false);
    add(r.Scratch.get(),r.Visibility.get(),r.Edges.get(),true);
}
void MetalDevice::EncodeAmbientOcclusion(const std::shared_ptr<MetalRenderer2DFrame>& frame)
{
    auto& pass=*frame->AO;auto& r=*pass.Resources;
    if(pass.Counters)m_Commands->writeTimestampIntoHeap(pass.Counters.get(),0);
    m_Commands->useResidencySet(frame->AOSource->Residency.get());EncodeRenderer2DFrame(frame->AOSource);
    auto* compute=m_Commands->computeCommandEncoder();
    if(!compute) {m_Commands->endCommandBuffer();throw std::runtime_error("Cannot encode XeGTAO");}
    compute->barrierAfterQueueStages(MTL::StageAll,MTL::StageDispatch,MTL4::VisibilityOptionDevice);
    if(pass.Counters)compute->writeTimestamp(MTL4::TimestampGranularityPrecise,pass.Counters.get(),1);
    for(size_t i=0;i<pass.Arguments.size();++i) {
        if(pass.Counters && (i==r.DepthMips.size() || i==r.DepthMips.size()+1))
            compute->writeTimestamp(MTL4::TimestampGranularityPrecise,pass.Counters.get(),i==r.DepthMips.size()?2:3);
        const auto slices=pass.Constants.Quality.x;
        const size_t preset=slices==1?0:slices==2?1:slices==3?2:3;
        const size_t kind=i==0?0:i<r.DepthMips.size()?1:i==r.DepthMips.size()?2+preset:6;
        compute->setComputePipelineState(r.Pipelines->States[kind].get());compute->setArgumentTable(pass.Arguments[i].get());
        uint32_t width=r.Width,height=r.Height;
        if(i<r.DepthMips.size()){width=uint32_t(r.DepthMips[i]->width());height=uint32_t(r.DepthMips[i]->height());}
        compute->dispatchThreads({width,height,1},{8,8,1});
        compute->barrierAfterEncoderStages(MTL::StageDispatch,MTL::StageDispatch,MTL4::VisibilityOptionDevice);
    }
    if(pass.Counters)compute->writeTimestamp(MTL4::TimestampGranularityPrecise,pass.Counters.get(),4);
    compute->endEncoding();
}
void MetalAOPass::ResolveTimings()
{
    if(TimingsResolved || !Counters || !Timing)return;
    TimingsResolved=true;
    auto pool=NS::TransferPtr(NS::AutoreleasePool::alloc()->init());
    auto values=NS::RetainPtr(Counters->resolveCounterRange(NS::Range::Make(0,6)));
    if(!values || values->length()!=6*sizeof(MTL4::TimestampHeapEntry))return;
    MTL4::TimestampHeapEntry entries[6];std::memcpy(entries,values->bytes(),sizeof(entries));
    for(unsigned i=0;i<5;++i)if(!entries[i].timestamp || entries[i+1].timestamp<entries[i].timestamp)return;
    for(unsigned i=0;i<5;++i)Timing->Milliseconds[i]=double(entries[i+1].timestamp-entries[i].timestamp)*1000/double(TimestampFrequency);
    Timing->WorkingBytes=0;const auto& r=*Resources;
    for(auto texture:{r.Depth,r.Raw,r.Edges,r.Scratch,r.Visibility})Timing->WorkingBytes+=texture->allocatedSize();
    Timing->Valid=true;
    if(Trace)std::printf("XEGTAO_GPU frame=%llu pixels=%ux%u depth_ms=%.6f prefilter_ms=%.6f main_ms=%.6f denoise_ms=%.6f forward_world_ms=%.6f working_bytes=%llu\n",
        (unsigned long long)Serial,r.Width,r.Height,Timing->Milliseconds[0],Timing->Milliseconds[1],Timing->Milliseconds[2],
        Timing->Milliseconds[3],Timing->Milliseconds[4],(unsigned long long)Timing->WorkingBytes);
}
}
