#include "MetalDevice.h"
#include "MetalRenderer2DFrame.h"
#include "MetalFramebuffer.h"
#include "MetalShader.h"
#include "MetalComputePipeline.h"
#include "Hazel/Renderer/Drawing/Renderer2DQuad.h"
#include <algorithm>
#include <cstring>
#include <set>
#include <stdexcept>
#include <cstdio>

namespace Hazel {
namespace {
size_t ColorRow(uint32_t width) { return (width * 16 + 255) & ~size_t(255); }
size_t ScalarRow(uint32_t width) { return (size_t(width) * 4 + 255) & ~size_t(255); }
void Check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
MTL::CompareFunction DepthFunction(const RasterState& raster)
{
    return !raster.DepthTest || raster.Compare == DepthCompare::Always ? MTL::CompareFunctionAlways :
        raster.Compare == DepthCompare::Less ? MTL::CompareFunctionLess : MTL::CompareFunctionLessEqual;
}
}
void MetalRenderer2DFrame::CollectAllocations(std::set<const MTL::Allocation*>& allocations) const
{
    for (const auto& target : Attachments->Targets) if (target) allocations.insert(target.get());
    for (const auto& buffer : Buffers) if (buffer) allocations.insert(buffer.get());
    for (const auto& draw : Draws) {
        if (draw.MeshVertices) allocations.insert(draw.MeshVertices.get());
        if (draw.MeshIndices) allocations.insert(draw.MeshIndices.get());
        for (const auto& texture : draw.Textures) if (texture) allocations.insert(texture->Texture.get());
    }
    if (ComputeSource) ComputeSource->CollectAllocations(allocations);
    if (World) World->CollectAllocations(allocations);
    if (DisplaySource) DisplaySource->CollectAllocations(allocations);
}
void MetalRenderer2DFrame::MarkMipmapsSubmitted() const
{
    if (ComputeSource) ComputeSource->MarkMipmapsSubmitted();
    if (World) World->MarkMipmapsSubmitted();
    if (DisplaySource) DisplaySource->MarkMipmapsSubmitted();
    for (const auto& draw : Draws) for (const auto& texture : draw.Textures) if (texture) texture->MipsSubmitted = true;
}
void MetalRenderer2DFrame::Complete()
{
    if (!Readback) return;
    // MetalDevice's ordered shared event has completed this submission.
    const auto* bytes = static_cast<const uint8_t*>(Buffers[0]->contents());
    const auto row = ColorRow(Width), idBase = row * Height, depthBase = idBase + ScalarRow(Width) * Height;
    auto& result = *Result;
    if (!FeedbackError->empty()) { result.Error = *FeedbackError; result.Completed = true; return; }
    if (EditorPicking) {
        for (size_t y = 0; y < Height; ++y) std::memcpy(CompletedEntity.data() + y * Width, bytes + y * ScalarRow(Width), size_t(Width) * 4);
        result.Entity = std::move(CompletedEntity); result.Completed = true; return;
    }
    for (size_t y = 0; y < Height; ++y) for (size_t x = 0; x < Width; ++x)
    {
        const auto pixel = y * Width + x;
        std::memcpy(&CompletedColor[pixel], bytes + y * row + x * 16, 16);
        std::memcpy(&CompletedEntity[pixel], bytes + idBase + y * ScalarRow(Width) + x * 4, 4);
        std::memcpy(&CompletedDepth[pixel], bytes + depthBase + y * ScalarRow(Width) + x * 4, 4);
    }
    result.Color = std::move(CompletedColor); result.Entity = std::move(CompletedEntity); result.Depth = std::move(CompletedDepth);
    result.Completed = true;
}
std::shared_ptr<const FramebufferReadback> MetalDevice::SubmitRenderer2DFrame(const std::shared_ptr<MetalRenderer2DFrame>& quad)
{
    Check(!m_Stopped, "Metal device is stopped");
    Check(quad && quad->Width && quad->Height && quad->Width <= 4096 && quad->Height <= 4096 && uint64_t(quad->Width) * quad->Height <= 8388608 &&
        quad->Attachments, "Unsupported Renderer2D frame extent/attachments");
    auto pool = NS::TransferPtr(NS::AutoreleasePool::alloc()->init());
    Poll();
    { std::lock_guard<std::mutex> lock(m_Observer->Mutex);
      Check(m_Observer->Value.Errors == 0, "Metal queue reported an earlier submission error"); }
    auto& slot = m_Frames[m_Next];
    if (slot.Pending) { std::lock_guard<std::mutex> lock(m_Observer->Mutex); ++m_Observer->Value.SkippedBusy; return {}; }
    Check(!quad->Result, "Cannot resubmit an already encoded quad frame");
    slot.Renderer2DCursor = 0;
    try { PrepareRenderer2DFrame(quad); slot.Renderer2DBuffers.resize(slot.Renderer2DCursor); Account(quad.get()); }
    catch (...) { slot.Renderer2DBuffers.clear(); Account(); throw; }
    const auto serial = m_Serial + 1;
    auto result = std::make_shared<FramebufferReadback>();
    result->Serial = serial; result->Width = quad->Width; result->Height = quad->Height;
    if (quad->Readback) {
        quad->CompletedColor.resize(quad->Width * quad->Height);
        quad->CompletedEntity.resize(quad->CompletedColor.size()); quad->CompletedDepth.resize(quad->CompletedColor.size());
    }
    auto options = NS::TransferPtr(MTL4::CommitOptions::alloc()->init());
    auto observer = m_Observer;
    auto feedbackDone = quad->Feedback;
    auto feedbackError = quad->FeedbackError;
    const bool trace=m_Trace;
    options->addFeedbackHandler(MTL4::CommitFeedbackHandlerFunction([observer, feedbackDone, feedbackError, trace, serial](MTL4::CommitFeedback* feedback) {
        if(trace)std::fprintf(stderr,"M3_OFFSCREEN_FEEDBACK frame=%llu gpu_start=%.9f gpu_end=%.9f\n",
            (unsigned long long)serial,feedback->GPUStartTime(),feedback->GPUEndTime());
        std::lock_guard<std::mutex> lock(observer->Mutex); ++observer->Value.Feedbacks;
        if (auto* error = feedback->error()) { ++observer->Value.Errors; observer->Value.Error = error->localizedDescription()->utf8String(); *feedbackError = observer->Value.Error; }
        feedbackDone->store(true);
        observer->Changed.notify_all();
    }));
    slot.Allocator->reset(); // Poll proved completion before this slot can be selected.
    m_Commands->beginCommandBuffer(slot.Allocator.get());
    m_Commands->useResidencySet(quad->Residency.get());
    EncodeRenderer2DFrame(quad);
    m_Commands->endCommandBuffer();
    quad->Result = result; slot.Quad = quad; slot.Completion.reset();
    const MTL4::CommandBuffer* buffers[] = {m_Commands.get()};
    m_Queue->commit(buffers, 1, options.get()); quad->MarkMipmapsSubmitted(); m_Queue->signalEvent(m_Completed.get(), serial);
    slot.Pending = true;
    { std::lock_guard<std::mutex> lock(m_Observer->Mutex);
      auto& s = m_Observer->Value; ++s.Submitted; ++s.InFlight;
      s.PeakInFlight = std::max(s.PeakInFlight, s.InFlight); if (slot.Serial) ++s.SlotReuses; }
    slot.Serial = serial; m_Serial = serial; m_Next = (m_Next + 1) % m_Frames.size(); Account();
    return result;
}
NS::SharedPtr<MTL::Buffer> MetalDevice::AcquireRenderer2DBuffer(size_t bytes, const void* data)
{
    auto& slot = m_Frames[m_Next];
    Check(!slot.Pending && bytes, "Renderer2D storage reuse requires a retired frame slot");
    if (slot.Renderer2DCursor == slot.Renderer2DBuffers.size()) slot.Renderer2DBuffers.emplace_back();
    auto& buffer = slot.Renderer2DBuffers[slot.Renderer2DCursor++];
    const bool reuse = buffer && buffer->length() >= bytes && buffer->length() / 4 <= bytes;
    if (!reuse) buffer = NS::TransferPtr(m_Device->newBuffer(bytes, MTL::ResourceStorageModeShared));
    Check(bool(buffer), "Cannot allocate Renderer2D frame-slot storage");
    if (data) std::memcpy(buffer->contents(), data, bytes);
    { std::lock_guard<std::mutex> lock(m_Observer->Mutex);
      if (reuse) ++m_Observer->Value.BufferReuses; else ++m_Observer->Value.BufferAllocations; }
    return buffer;
}
void MetalDevice::PrepareRenderer2DFrame(const std::shared_ptr<MetalRenderer2DFrame>& quad)
{
    if (quad->ComputeSource) PrepareRenderer2DFrame(quad->ComputeSource);
    if (quad->World) PrepareRenderer2DFrame(quad->World);
    if (quad->DisplaySource) PrepareRenderer2DFrame(quad->DisplaySource);
    NS::Error* error = nullptr;
    Check(quad->Attachments && quad->Attachments->Width == quad->Width && quad->Attachments->Height == quad->Height,
        "Framebuffer generation does not match submitted extent");
    auto residency = NS::TransferPtr(MTL::ResidencySetDescriptor::alloc()->init());
    residency->setInitialCapacity(32);
    quad->Residency = NS::TransferPtr(m_Device->newResidencySet(residency.get(), &error));
    Check(bool(quad->Residency), "Cannot allocate quad residency set");
    for (const auto& target : quad->Attachments->Targets)
    {
        if (!target) continue;
        Check(target && target->device() == m_Device.get(), "Framebuffer attachment belongs to a different device");
        quad->Residency->addAllocation(target.get());
    }
    const auto colorRow = ColorRow(quad->Width), idBase = colorRow * quad->Height, depthBase = idBase + ScalarRow(quad->Width) * quad->Height;
    quad->Buffers.push_back(quad->Readback ? AcquireRenderer2DBuffer(quad->EditorPicking ? ScalarRow(quad->Width) * quad->Height : depthBase + ScalarRow(quad->Width) * quad->Height) : NS::SharedPtr<MTL::Buffer>{});
    uint32_t indexCount = 6;
    for (const auto& draw : quad->Draws) if (!draw.IndexedMesh) indexCount = std::max(indexCount, draw.Count);
    std::vector<uint32_t> indices(indexCount);
    for (uint32_t q = 0; q < indexCount / 6; ++q) for (uint32_t i = 0; i < 6; ++i)
        indices[q * 6 + i] = q * 4 + Renderer2DQuadIndices[i];
    quad->Buffers.push_back(AcquireRenderer2DBuffer(indices.size() * sizeof(uint32_t), indices.data()));
    auto args = NS::TransferPtr(MTL4::ArgumentTableDescriptor::alloc()->init());
    args->setInitializeBindings(true); args->setMaxBufferBindCount(17);
    args->setMaxTextureBindCount(8); args->setMaxSamplerStateBindCount(8);
    std::set<MTL::Texture*> textures;
    for (auto& draw : quad->Draws)
    {
        if (draw.IndexedMesh)
        {
            auto vertex = NS::TransferPtr(m_Device->newArgumentTable(args.get(), &error));
            auto fragment = NS::TransferPtr(m_Device->newArgumentTable(args.get(), &error));
            Check(bool(vertex) && bool(fragment), "Cannot allocate indexed draw argument tables");
            std::vector<uint8_t> upload;
            std::array<size_t, 8> offsets{};
            for (size_t binding = 0; binding < draw.Uniforms.size(); ++binding) if (!draw.Uniforms[binding].empty()) {
                offsets[binding] = upload.size();
                upload.insert(upload.end(), draw.Uniforms[binding].begin(), draw.Uniforms[binding].end());
                upload.resize((upload.size() + 255) & ~size_t(255));
            }
            Check(!upload.empty(), "Indexed draw has no captured uniforms");
            auto buffer = AcquireRenderer2DBuffer(upload.size(), upload.data());
            for (size_t binding = 0; binding < draw.Uniforms.size(); ++binding) if (!draw.Uniforms[binding].empty()) {
                vertex->setAddress(buffer->gpuAddress() + offsets[binding], binding);
                fragment->setAddress(buffer->gpuAddress() + offsets[binding], binding);
            }
            for (auto resource : {draw.MeshVertices, draw.MeshIndices}) if (resource) {
                Check(resource->device() == m_Device.get(), "Mesh buffer belongs to another device");
                quad->Residency->addAllocation(resource.get());
            }
            if (draw.MeshVertices) vertex->setAddress(draw.MeshVertices->gpuAddress(), 16);
            for (size_t binding = 0; binding < 8; ++binding) if (auto texture = draw.Textures[binding]) {
                Check(texture->Texture->device() == m_Device.get(), "Material texture belongs to another device");
                fragment->setTexture(texture->Texture->gpuResourceID(), binding);
                fragment->setSamplerState(texture->Sampler->gpuResourceID(), binding);
                if (textures.insert(texture->Texture.get()).second) quad->Residency->addAllocation(texture->Texture.get());
            }
            auto depth = NS::TransferPtr(MTL::DepthStencilDescriptor::alloc()->init());
            depth->setDepthCompareFunction(DepthFunction(draw.Raster));
            depth->setDepthWriteEnabled(draw.Raster.DepthTest && draw.Raster.DepthWrite);
            draw.DepthState = NS::TransferPtr(m_Device->newDepthStencilState(depth.get()));
            Check(bool(draw.DepthState), "Cannot allocate indexed draw depth state");
            quad->Buffers.push_back(std::move(buffer));
            quad->Arguments.push_back(std::move(vertex)); quad->Arguments.push_back(std::move(fragment));
            continue;
        }
        const auto descriptorOffset = (256 + draw.Vertices.size() + 255) & ~size_t(255);
        std::vector<uint8_t> upload(descriptorOffset + 512); // aligned immutable camera, vertex and descriptor slices
        std::memcpy(upload.data(), &draw.Camera, sizeof(draw.Camera));
        std::memcpy(upload.data() + 256, draw.Vertices.data(), draw.Vertices.size());
        std::array<MTL::ResourceID, 64> handles;
        for (size_t i = 0; i < 32; ++i)
        {
            const auto& texture = draw.Textures[i];
            Check(texture && texture->Texture->device() == m_Device.get(), "Quad texture belongs to a different device");
            handles[i] = texture->Texture->gpuResourceID(); handles[32 + i] = texture->Sampler->gpuResourceID();
            if (textures.insert(texture->Texture.get()).second) quad->Residency->addAllocation(texture->Texture.get());
        }
        static_assert(sizeof(handles) == 512, "Qualified tier-2 descriptor ABI");
        std::memcpy(upload.data() + descriptorOffset, handles.data(), sizeof(handles));
        auto buffer = AcquireRenderer2DBuffer(upload.size(), upload.data());
        auto vertex = NS::TransferPtr(m_Device->newArgumentTable(args.get(), &error));
        auto fragment = NS::TransferPtr(m_Device->newArgumentTable(args.get(), &error));
        Check(bool(buffer) && bool(vertex) && bool(fragment), "Cannot allocate immutable quad draw version");
        vertex->setAddress(buffer->gpuAddress(), 0); vertex->setAddress(buffer->gpuAddress() + 256, 16);
        if (draw.Shader->Artifact().Name == "Renderer2D_Quad")
            fragment->setAddress(buffer->gpuAddress() + descriptorOffset, 0);
        else if (draw.Shader->Artifact().Name == "Renderer2D_Text") {
            fragment->setTexture(draw.Textures[0]->Texture->gpuResourceID(), 0);
            fragment->setSamplerState(draw.Textures[0]->Sampler->gpuResourceID(), 0);
        }
        auto depth = NS::TransferPtr(MTL::DepthStencilDescriptor::alloc()->init());
        depth->setDepthCompareFunction(DepthFunction(draw.Raster));
        depth->setDepthWriteEnabled(draw.Raster.DepthTest && draw.Raster.DepthWrite);
        draw.DepthState = NS::TransferPtr(m_Device->newDepthStencilState(depth.get()));
        Check(bool(draw.DepthState), "Cannot allocate Renderer2D depth state");
        quad->Buffers.push_back(std::move(buffer));
        quad->Arguments.push_back(std::move(vertex)); quad->Arguments.push_back(std::move(fragment));
    }
    for (size_t i = quad->Readback ? 0 : 1; i < quad->Buffers.size(); ++i) {
        Check(bool(quad->Buffers[i]), "Cannot allocate quad buffer"); quad->Residency->addAllocation(quad->Buffers[i].get());
    }
    if (quad->OutputShader) {
        auto sampler = NS::TransferPtr(MTL::SamplerDescriptor::alloc()->init());
        sampler->setMinFilter(MTL::SamplerMinMagFilterNearest); sampler->setMagFilter(MTL::SamplerMinMagFilterNearest);
        sampler->setSAddressMode(MTL::SamplerAddressModeClampToEdge); sampler->setTAddressMode(MTL::SamplerAddressModeClampToEdge);
        quad->OutputSampler = NS::TransferPtr(m_Device->newSamplerState(sampler.get()));
        auto table = NS::TransferPtr(MTL4::ArgumentTableDescriptor::alloc()->init());
        table->setInitializeBindings(true); table->setMaxTextureBindCount(1); table->setMaxSamplerStateBindCount(1);
        quad->OutputArguments = NS::TransferPtr(m_Device->newArgumentTable(table.get(), &error));
        Check(bool(quad->OutputSampler) && bool(quad->OutputArguments), "Cannot allocate output pass arguments");
        quad->OutputArguments->setTexture((quad->DisplaySource ? quad->DisplaySource : quad)->Attachments->Targets[0]->gpuResourceID(), 0);
        quad->OutputArguments->setSamplerState(quad->OutputSampler->gpuResourceID(), 0);
    }
    if (quad->World) {
        auto table = NS::TransferPtr(MTL4::ArgumentTableDescriptor::alloc()->init());
        table->setInitializeBindings(true); table->setMaxBufferBindCount(6); table->setMaxTextureBindCount(2); table->setMaxSamplerStateBindCount(1);
        quad->PresentationArguments = NS::TransferPtr(m_Device->newArgumentTable(table.get(), &error));
        auto sampler = NS::TransferPtr(MTL::SamplerDescriptor::alloc()->init());
        sampler->setMinFilter(MTL::SamplerMinMagFilterNearest); sampler->setMagFilter(MTL::SamplerMinMagFilterNearest);
        sampler->setSAddressMode(MTL::SamplerAddressModeClampToEdge); sampler->setTAddressMode(MTL::SamplerAddressModeClampToEdge);
        if (!quad->OutputSampler) quad->OutputSampler = NS::TransferPtr(m_Device->newSamplerState(sampler.get()));
        glm::vec4 presentation(quad->Exposure, 0, 0, 0);
        auto uniform = AcquireRenderer2DBuffer(sizeof(presentation), &presentation);
        Check(bool(quad->PresentationArguments) && bool(quad->OutputSampler) && bool(uniform), "Cannot allocate world presentation bindings");
        quad->PresentationArguments->setAddress(uniform->gpuAddress(), 5);
        quad->PresentationArguments->setTexture(quad->World->Attachments->Targets[0]->gpuResourceID(), 0);
        quad->PresentationArguments->setTexture(quad->World->Attachments->Targets[1]->gpuResourceID(), 1);
        quad->PresentationArguments->setSamplerState(quad->OutputSampler->gpuResourceID(), 0);
        quad->Residency->addAllocation(uniform.get()); quad->Buffers.push_back(std::move(uniform));
    }
    if (quad->OutlineShader) {
        auto table = NS::TransferPtr(MTL4::ArgumentTableDescriptor::alloc()->init());
        table->setInitializeBindings(true); table->setMaxBufferBindCount(8); table->setMaxTextureBindCount(1);
        quad->OutlineArguments = NS::TransferPtr(m_Device->newArgumentTable(table.get(), &error));
        struct alignas(16) Parameters { int32_t Entity, Radius, FlipY, Padding; glm::vec4 Color; };
        const Parameters values{quad->OutlineEntity, quad->OutlineRadius, 1, 0, quad->OutlineColor};
        auto buffer = AcquireRenderer2DBuffer(sizeof(values), &values);
        Check(bool(quad->OutlineArguments), "Cannot allocate outline argument table");
        quad->OutlineArguments->setAddress(buffer->gpuAddress(), 7);
        quad->OutlineArguments->setTexture(quad->DisplaySource->Attachments->Targets[1]->gpuResourceID(), 0);
        quad->Residency->addAllocation(buffer.get()); quad->Buffers.push_back(std::move(buffer));
    }
    if (quad->Compute) {
        auto table = NS::TransferPtr(MTL4::ArgumentTableDescriptor::alloc()->init());
        table->setInitializeBindings(true); table->setMaxBufferBindCount(1); table->setMaxTextureBindCount(2);
        quad->ComputeArguments = NS::TransferPtr(m_Device->newArgumentTable(table.get(), &error));
        auto buffer = AcquireRenderer2DBuffer(quad->ComputeParameters.size(), quad->ComputeParameters.data());
        Check(bool(quad->ComputeArguments), "Cannot allocate color compute arguments");
        quad->ComputeArguments->setAddress(buffer->gpuAddress(), 0);
        quad->ComputeArguments->setTexture(quad->ComputeSource->Attachments->Targets[0]->gpuResourceID(), 0);
        quad->ComputeArguments->setTexture(quad->Attachments->Targets[0]->gpuResourceID(), 1);
        quad->Residency->addAllocation(buffer.get()); quad->Buffers.push_back(std::move(buffer));
    }
    quad->Residency->commit();

}
void MetalDevice::EncodeRenderer2DFrame(const std::shared_ptr<MetalRenderer2DFrame>& quad)
{
    if (quad->ComputeSource) {
        m_Commands->useResidencySet(quad->ComputeSource->Residency.get());
        EncodeRenderer2DFrame(quad->ComputeSource);
        auto* compute = m_Commands->computeCommandEncoder();
        Check(compute != nullptr, "Cannot encode native color compute pass");
        compute->barrierAfterQueueStages(MTL::StageAll, MTL::StageDispatch | MTL::StageBlit, MTL4::VisibilityOptionDevice);
        compute->setComputePipelineState(quad->Compute->Native());
        compute->setArgumentTable(quad->ComputeArguments.get());
        compute->dispatchThreads({quad->Width, quad->Height, 1}, {8,8,1});
        for (size_t i = 1; i < 3; ++i) compute->copyFromTexture(quad->ComputeSource->Attachments->Targets[i].get(), quad->Attachments->Targets[i].get());
        compute->barrierAfterStages(MTL::StageDispatch | MTL::StageBlit, MTL::StageAll, MTL4::VisibilityOptionDevice);
        compute->endEncoding();
    }
    if (quad->DisplaySource) {
        m_Commands->useResidencySet(quad->DisplaySource->Residency.get());
        EncodeRenderer2DFrame(quad->DisplaySource);
        auto pass = NS::TransferPtr(MTL4::RenderPassDescriptor::alloc()->init());
        auto* color = pass->colorAttachments()->object(0);
        color->setTexture(quad->Attachments->Targets[0].get()); color->setLoadAction(MTL::LoadActionDontCare); color->setStoreAction(MTL::StoreActionStore);
        auto* render = m_Commands->renderCommandEncoder(pass.get());
        Check(render != nullptr, "Cannot encode editor display output");
        render->barrierAfterQueueStages(MTL::StageAll, MTL::StageFragment, MTL4::VisibilityOptionDevice);
        render->setRenderPipelineState(quad->OutputShader->Pipeline());
        render->setArgumentTable(quad->OutputArguments.get(), MTL::RenderStageFragment);
        render->drawPrimitives(MTL::PrimitiveTypeTriangle, NS::UInteger(0), NS::UInteger(3));
        if (quad->OutlineShader) {
            render->setRenderPipelineState(quad->OutlineShader->Pipeline());
            render->setArgumentTable(quad->OutlineArguments.get(), MTL::RenderStageFragment);
            render->drawPrimitives(MTL::PrimitiveTypeTriangle, NS::UInteger(0), NS::UInteger(3));
        }
        render->endEncoding();
        return;
    }
    if (quad->World) {
        m_Commands->useResidencySet(quad->World->Residency.get());
        EncodeRenderer2DFrame(quad->World);
        auto* copy = m_Commands->computeCommandEncoder();
        if (!copy) { m_Commands->endCommandBuffer(); throw std::runtime_error("Cannot encode world depth transfer"); }
        copy->barrierAfterQueueStages(MTL::StageAll, MTL::StageBlit, MTL4::VisibilityOptionDevice);
        copy->copyFromTexture(quad->World->Attachments->Targets[2].get(), quad->Attachments->Targets[2].get());
        copy->barrierAfterStages(MTL::StageBlit, MTL::StageAll, MTL4::VisibilityOptionDevice);
        copy->endEncoding();
    }
    if (!quad->ComputeSource) {
    std::set<MTL::Texture*> mipmaps;
    for (const auto& draw : quad->Draws) for (const auto& texture : draw.Textures)
        if (texture && !texture->MipsSubmitted && texture->Texture->mipmapLevelCount() > 1) mipmaps.insert(texture->Texture.get());
    if (!mipmaps.empty()) {
        auto* compute = m_Commands->computeCommandEncoder();
        if (!compute) { m_Commands->endCommandBuffer(); throw std::runtime_error("Cannot encode texture mipmap generation"); }
        for (auto* texture : mipmaps) compute->generateMipmaps(texture);
        compute->endEncoding();
    }
    auto pass = NS::TransferPtr(MTL4::RenderPassDescriptor::alloc()->init());
    for (size_t i = 0; i < 2; ++i)
    {
        auto* attachment = pass->colorAttachments()->object(i);
        attachment->setTexture(quad->Attachments->Targets[i].get()); attachment->setLoadAction(MTL::LoadActionClear);
        attachment->setStoreAction(MTL::StoreActionStore);
        attachment->setClearColor(i == 0 ? MTL::ClearColor{quad->Clear.r, quad->Clear.g, quad->Clear.b, quad->Clear.a} : MTL::ClearColor{double(quad->ClearEntity), 0, 0, 0});
    }
    pass->depthAttachment()->setTexture(quad->Attachments->Targets[2].get()); pass->depthAttachment()->setLoadAction(quad->World ? MTL::LoadActionLoad : MTL::LoadActionClear);
    pass->depthAttachment()->setStoreAction(MTL::StoreActionStore); pass->depthAttachment()->setClearDepth(1);
    auto* render = m_Commands->renderCommandEncoder(pass.get());
    if (!render) { m_Commands->endCommandBuffer(); throw std::runtime_error("Cannot create quad encoder"); }
    if (quad->World || !mipmaps.empty()) render->barrierAfterQueueStages(MTL::StageAll, MTL::StageFragment, MTL4::VisibilityOptionDevice);
    auto* boundPipeline = quad->World ? quad->PresentationShader->Pipeline() : nullptr;
    if (boundPipeline) render->setRenderPipelineState(boundPipeline);
    // Metal starts each encoder with full-target viewport/scissor, no culling,
    // clockwise winding and disabled depth writes. Cache only within this pass.
    glm::ivec4 boundViewport{0,0,quad->Width,quad->Height}, boundClip=boundViewport;
    auto boundCull=MTL::CullModeNone;
    auto boundWinding=MTL::WindingClockwise;
    auto boundDepth=MTL::CompareFunctionAlways;
    bool boundDepthWrite=false;
    if (quad->World) {
        render->setArgumentTable(quad->PresentationArguments.get(), MTL::RenderStageFragment);
        render->drawPrimitives(MTL::PrimitiveTypeTriangle, NS::UInteger(0), NS::UInteger(3));
    }
    for (size_t i = 0; i < quad->Draws.size(); ++i)
    {
        const auto& draw = quad->Draws[i];
        if(draw.Shader->Pipeline()!=boundPipeline) {
            boundPipeline=draw.Shader->Pipeline();render->setRenderPipelineState(boundPipeline);
        }
        const auto depth=DepthFunction(draw.Raster);
        const bool depthWrite=draw.Raster.DepthTest && draw.Raster.DepthWrite;
        if(depth!=boundDepth || depthWrite!=boundDepthWrite) {
            render->setDepthStencilState(draw.DepthState.get());
            boundDepth=depth;boundDepthWrite=depthWrite;
        }
        const auto& viewport = draw.Raster.Viewport;
        if(viewport!=boundViewport) {
            render->setViewport({double(viewport.x),double(viewport.y),double(viewport.z),double(viewport.w),0,1});
            boundViewport=viewport;
        }
        const auto cull=draw.Raster.Cull == CullFace::None ? MTL::CullModeNone :
            draw.Raster.Cull == CullFace::Back ? MTL::CullModeBack : MTL::CullModeFront;
        if(cull!=boundCull) { render->setCullMode(cull);boundCull=cull; }
        const auto winding=draw.Raster.Clockwise ? MTL::WindingCounterClockwise : MTL::WindingClockwise;
        if(winding!=boundWinding) { render->setFrontFacingWinding(winding);boundWinding=winding; }
        auto clip = draw.Raster.ClipEnabled ? draw.Raster.ClipRect : glm::ivec4(0,0,quad->Width,quad->Height);
        const int left=std::clamp(clip.x,0,int(quad->Width)), bottom=std::clamp(clip.y,0,int(quad->Height));
        const int right=int(std::clamp<int64_t>(int64_t(clip.x)+std::max(clip.z,0),left,int(quad->Width)));
        const int top=int(std::clamp<int64_t>(int64_t(clip.y)+std::max(clip.w,0),bottom,int(quad->Height)));
        if (right == left || top == bottom || viewport.z <= 0 || viewport.w <= 0) continue;
        const glm::ivec4 clipped{left,bottom,right-left,top-bottom};
        if(clipped!=boundClip) {
            render->setScissorRect({size_t(left),size_t(bottom),size_t(right-left),size_t(top-bottom)});
            boundClip=clipped;
        }
        render->setArgumentTable(quad->Arguments[i * 2].get(), MTL::RenderStageVertex);
        render->setArgumentTable(quad->Arguments[i * 2 + 1].get(), MTL::RenderStageFragment);
        const auto& indices = draw.IndexedMesh ? draw.MeshIndices : quad->Buffers[1];
        const size_t offset = size_t(draw.FirstIndex) * 4;
        render->drawIndexedPrimitives(MTL::PrimitiveTypeTriangle, draw.Count, MTL::IndexTypeUInt32,
            indices->gpuAddress() + offset, indices->length() - offset);
    }
    render->endEncoding();
    }
    if (!quad->Readback) return;
    const auto colorRow = ColorRow(quad->Width), idBase = colorRow * quad->Height, depthBase = idBase + ScalarRow(quad->Width) * quad->Height;
    auto* copy = m_Commands->computeCommandEncoder();
    if (!copy) { m_Commands->endCommandBuffer(); throw std::runtime_error("Cannot create quad readback encoder"); }
    copy->barrierAfterQueueStages(MTL::StageAll, MTL::StageBlit, MTL4::VisibilityOptionDevice);
    if (quad->EditorPicking) {
        copy->copyFromTexture(quad->Attachments->Targets[1].get(), 0, 0, {0,0,0}, {quad->Width,quad->Height,1}, quad->Buffers[0].get(), 0, ScalarRow(quad->Width), 0);
    } else for (size_t i = 0; i < 3; ++i)
        copy->copyFromTexture(quad->Attachments->Targets[i].get(), 0, 0, {0, 0, 0}, {quad->Width, quad->Height, 1}, quad->Buffers[0].get(),
            i == 0 ? 0 : i == 1 ? idBase : depthBase, i == 0 ? colorRow : ScalarRow(quad->Width), 0);
    copy->endEncoding();
}

}
