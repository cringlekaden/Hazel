#include "MetalExposureMeter.h"
#include "MetalDevice.h"
#include "MetalFramebuffer.h"
#include "MetalRenderer2DFrame.h"
#include "Hazel/Core/Resources.h"
#include <cstring>

namespace Hazel {
namespace {
void Check(bool ok, const char* message)
{
    if (!ok) throw std::runtime_error(message);
}
}

struct MetalExposurePipelines {
    NS::SharedPtr<MTL::Library> Library;
    std::array<NS::SharedPtr<MTL::ComputePipelineState>, 2> State;

    explicit MetalExposurePipelines(MTL::Device& device)
    {
        const auto path = Resources::Resolve("shaders/metal/ExposureMeter.metallib").generic_u8string();
        NS::Error* error = nullptr;
        Library = NS::TransferPtr(device.newLibrary(NS::String::string(path.c_str(), NS::UTF8StringEncoding), &error));
        Check(bool(Library), error ? error->localizedDescription()->utf8String() : "Load ExposureMeter metallib");
        auto compilerSpec = NS::TransferPtr(MTL4::CompilerDescriptor::alloc()->init());
        auto compiler = NS::TransferPtr(device.newCompiler(compilerSpec.get(), &error));
        Check(bool(compiler), "Create Metal 4 exposure compiler");
        auto function = NS::TransferPtr(MTL4::LibraryFunctionDescriptor::alloc()->init());
        function->setLibrary(Library.get());
        auto desc = NS::TransferPtr(MTL4::ComputePipelineDescriptor::alloc()->init());
        auto options = NS::TransferPtr(MTL4::PipelineOptions::alloc()->init());
        options->setShaderReflection(MTL4::ShaderReflectionBindingInfo | MTL4::ShaderReflectionBufferTypeInfo);
        desc->setOptions(options.get());
        const char* entries[]{"meterHistogram", "adaptExposure"};
        for (size_t pass = 0; pass < State.size(); ++pass) {
            function->setName(NS::String::string(entries[pass], NS::UTF8StringEncoding));
            desc->setComputeFunctionDescriptor(function.get());
            State[pass] = NS::TransferPtr(compiler->newComputePipelineState(desc.get(), nullptr, &error));
            Check(bool(State[pass]), error ? error->localizedDescription()->utf8String() : entries[pass]);
            const auto* reflection = State[pass]->reflection();
            Check(reflection && reflection->bindings(), "Missing Metal exposure reflection");
            unsigned mask = 0;
            for (size_t i = 0; i < reflection->bindings()->count(); ++i) {
                const auto* binding = reflection->bindings()->object<MTL::Binding>(i);
                if (!binding->isUsed()) continue;
                if (binding->type() == MTL::BindingTypeBuffer) {
                    Check(binding->index() == 0 && binding->access() == MTL::BindingAccessReadOnly &&
                        static_cast<const MTL::BufferBinding*>(binding)->bufferDataSize() == sizeof(ExposureMeterParameters),
                        "Invalid Metal exposure parameter binding");
                    mask |= 1;
                } else if (binding->type() == MTL::BindingTypeTexture) {
                    const auto* texture = static_cast<const MTL::TextureBinding*>(binding);
                    Check(texture->index() < 3 && texture->textureType() == MTL::TextureType2D &&
                        texture->textureDataType() == MTL::DataTypeFloat &&
                        texture->access() == (texture->index() == 2 ? MTL::BindingAccessWriteOnly : MTL::BindingAccessReadOnly),
                        "Invalid Metal exposure texture binding");
                    mask |= 2u << texture->index();
                } else throw std::runtime_error("Unexpected Metal exposure binding");
            }
            Check(mask == 15 && State[pass]->maxTotalThreadsPerThreadgroup() >= 128,
                "Metal exposure pipeline does not meet its binding and dispatch contract");
        }
    }
};

void MetalDevice::PrepareExposure(const std::shared_ptr<MetalRenderer2DFrame>& frame)
{
    if (!m_ExposurePipelines) m_ExposurePipelines = std::make_shared<MetalExposurePipelines>(*m_Device.get());
    auto& exposure = *frame->ExposureMeter;
    const auto& world = frame->World->Attachments;
    if (exposure.Timing) {
        auto counters = NS::TransferPtr(MTL4::CounterHeapDescriptor::alloc()->init());
        counters->setType(MTL4::CounterHeapTypeTimestamp);
        counters->setCount(2);
        NS::Error* error = nullptr;
        exposure.Counters = NS::TransferPtr(m_Device->newCounterHeap(counters.get(), &error));
        if (exposure.Counters) exposure.Counters->invalidateCounterRange(NS::Range::Make(0, 2));
        exposure.Frequency = m_Device->queryTimestampFrequency();
    }
    auto uniform = AcquireRenderer2DBuffer(sizeof(exposure.Parameters), &exposure.Parameters);
    auto options = NS::TransferPtr(MTL4::ArgumentTableDescriptor::alloc()->init());
    options->setInitializeBindings(true);
    options->setMaxBufferBindCount(1);
    options->setMaxTextureBindCount(3);
    NS::Error* error = nullptr;
    for (auto& arguments : exposure.Arguments) {
        arguments = NS::TransferPtr(m_Device->newArgumentTable(options.get(), &error));
        Check(bool(arguments), "Allocate Metal exposure bindings");
        arguments->setAddress(uniform->gpuAddress(), 0);
    }
    const auto bind = [&](size_t pass, MTL::Texture* first, MTL::Texture* second, MTL::Texture* output) {
        exposure.Arguments[pass]->setTexture(first->gpuResourceID(), 0);
        exposure.Arguments[pass]->setTexture(second->gpuResourceID(), 1);
        exposure.Arguments[pass]->setTexture(output->gpuResourceID(), 2);
    };
    bind(0, world->Targets[0].get(), world->Targets[2].get(), exposure.Histogram->Targets[0].get());
    bind(1, exposure.Histogram->Targets[0].get(), exposure.Previous->Targets[0].get(), exposure.Current->Targets[0].get());
    for (const auto& texture : {exposure.Histogram, exposure.Previous, exposure.Current})
        frame->Residency->addAllocation(texture->Targets[0].get());
    frame->Residency->addAllocation(uniform.get());
    frame->Buffers.push_back(std::move(uniform));
}

void MetalDevice::EncodeExposure(const std::shared_ptr<MetalRenderer2DFrame>& frame)
{
    auto* compute = m_Commands->computeCommandEncoder();
    if (!compute) { m_Commands->endCommandBuffer(); throw std::runtime_error("Encode Metal exposure"); }
    const auto label = NS::String::string("Hazel automatic camera exposure", NS::UTF8StringEncoding);
    compute->setLabel(label);
    // These private textures are shared in queue order. Wait for the world
    // producer and for readers of the previous two gain versions.
    compute->barrierAfterQueueStages(MTL::StageAll, MTL::StageDispatch, MTL4::VisibilityOptionDevice);
    auto* counters = frame->ExposureMeter->Counters.get();
    if (counters) compute->writeTimestamp(MTL4::TimestampGranularityPrecise, counters, 0);
    for (size_t pass = 0; pass < 2; ++pass) {
        compute->setComputePipelineState(m_ExposurePipelines->State[pass].get());
        compute->setArgumentTable(frame->ExposureMeter->Arguments[pass].get());
        compute->dispatchThreadgroups({pass == 0 ? 72u : 1u, 1, 1}, {128, 1, 1});
        compute->barrierAfterEncoderStages(MTL::StageDispatch, MTL::StageDispatch, MTL4::VisibilityOptionDevice);
    }
    if (counters) compute->writeTimestamp(MTL4::TimestampGranularityPrecise, counters, 1);
    compute->endEncoding();
}

void MetalExposureMeter::ResolveTiming()
{
    if (!Counters || !Frequency) return;
    auto values = NS::RetainPtr(Counters->resolveCounterRange(NS::Range::Make(0, 2)));
    Counters.reset();
    MTL4::TimestampHeapEntry entries[2];
    if (!values || values->length() != sizeof(entries)) return;
    std::memcpy(entries, values->bytes(), sizeof(entries));
    if (!entries[0].timestamp || entries[1].timestamp < entries[0].timestamp) return;
    uint64_t bytes = 0;
    for (const auto& surface : {Histogram, Previous, Current}) bytes += surface->Targets[0]->allocatedSize();
    *Timing = {double(entries[1].timestamp - entries[0].timestamp) * 1000 / Frequency, bytes};
}
}
