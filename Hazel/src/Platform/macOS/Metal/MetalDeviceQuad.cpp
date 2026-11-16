#include "MetalDevice.h"
#include "MetalQuadFrame.h"
#include "MetalFramebuffer.h"
#include "MetalShader.h"
#include <algorithm>
#include <cstring>
#include <set>
#include <stdexcept>

namespace Hazel {
namespace {
size_t ColorRow(uint32_t width) { return (width * 16 + 255) & ~size_t(255); }
constexpr size_t ScalarRow = 256;
void Check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
}
uint64_t MetalQuadFrame::ResourceBytes() const
{
    std::set<const MTL::Allocation*> allocations;
    for (const auto& target : Attachments->Targets) allocations.insert(target.get());
    for (const auto& buffer : Buffers) allocations.insert(buffer.get());
    for (const auto& draw : Draws) for (const auto& texture : draw.Textures) allocations.insert(texture->Texture.get());
    uint64_t bytes = 0;
    for (const auto* allocation : allocations) bytes += allocation->allocatedSize();
    return bytes;
}
void MetalQuadFrame::Complete()
{
    // MetalDevice's ordered shared event has completed this submission.
    const auto* bytes = static_cast<const uint8_t*>(Buffers[0]->contents());
    const auto row = ColorRow(Width), idBase = row * Height, depthBase = idBase + ScalarRow * Height;
    auto& result = *Result;
    if (!FeedbackError->empty()) { result.Error = *FeedbackError; result.Completed = true; return; }
    for (size_t y = 0; y < Height; ++y) for (size_t x = 0; x < Width; ++x)
    {
        const auto pixel = y * Width + x;
        std::memcpy(&CompletedColor[pixel], bytes + y * row + x * 16, 16);
        std::memcpy(&CompletedEntity[pixel], bytes + idBase + y * ScalarRow + x * 4, 4);
        std::memcpy(&CompletedDepth[pixel], bytes + depthBase + y * ScalarRow + x * 4, 4);
    }
    result.Color = std::move(CompletedColor); result.Entity = std::move(CompletedEntity); result.Depth = std::move(CompletedDepth);
    result.Completed = true;
}
std::shared_ptr<const FramebufferReadback> MetalDevice::SubmitQuadFrame(const std::shared_ptr<MetalQuadFrame>& quad)
{
    Check(!m_Stopped, "Metal device is stopped");
    Check(quad && quad->Width && quad->Height && quad->Width <= 32 && quad->Height <= 32 &&
        quad->Draws.size() <= 8 && quad->Shader && quad->Shader->Artifact().Name == "Renderer2D_Quad" &&
        quad->Shader->Artifact().TextureSlots == 32, "Unsupported offscreen quad frame/profile");
    auto pool = NS::TransferPtr(NS::AutoreleasePool::alloc()->init());
    Poll();
    { std::lock_guard<std::mutex> lock(m_Observer->Mutex);
      Check(m_Observer->Value.Errors == 0, "Metal queue reported an earlier submission error"); }
    auto& slot = m_Frames[m_Next];
    if (slot.Pending) { std::lock_guard<std::mutex> lock(m_Observer->Mutex); ++m_Observer->Value.SkippedBusy; return {}; }
    Check(!quad->Result, "Cannot resubmit an already encoded quad frame");
    NS::Error* error = nullptr;
    Check(quad->Attachments && quad->Attachments->Width == quad->Width && quad->Attachments->Height == quad->Height,
        "Framebuffer generation does not match submitted extent");
    auto residency = NS::TransferPtr(MTL::ResidencySetDescriptor::alloc()->init());
    residency->setInitialCapacity(32);
    quad->Residency = NS::TransferPtr(m_Device->newResidencySet(residency.get(), &error));
    Check(bool(quad->Residency), "Cannot allocate quad residency set");
    for (const auto& target : quad->Attachments->Targets)
    {
        Check(target && target->device() == m_Device.get(), "Framebuffer attachment belongs to a different device");
        quad->Residency->addAllocation(target.get());
    }
    const auto colorRow = ColorRow(quad->Width), idBase = colorRow * quad->Height, depthBase = idBase + ScalarRow * quad->Height;
    quad->Buffers.push_back(NS::TransferPtr(m_Device->newBuffer(depthBase + ScalarRow * quad->Height, MTL::ResourceStorageModeShared)));
    std::array<uint32_t, 48> indices;
    for (uint32_t q = 0; q < 8; ++q) for (uint32_t i = 0; i < 6; ++i)
        indices[q * 6 + i] = q * 4 + Renderer2DQuadIndices[i];
    quad->Buffers.push_back(NS::TransferPtr(m_Device->newBuffer(indices.data(), sizeof(indices), MTL::ResourceStorageModeShared)));
    auto args = NS::TransferPtr(MTL4::ArgumentTableDescriptor::alloc()->init());
    args->setInitializeBindings(true); args->setMaxBufferBindCount(17);
    std::set<MTL::Texture*> textures;
    for (const auto& draw : quad->Draws)
    {
        const auto descriptorOffset = (256 + draw.Vertices.size() * sizeof(QuadVertex) + 255) & ~size_t(255);
        std::vector<uint8_t> upload(descriptorOffset + 512); // aligned immutable camera, vertex and descriptor slices
        std::memcpy(upload.data(), &draw.Camera, sizeof(draw.Camera));
        std::memcpy(upload.data() + 256, draw.Vertices.data(), draw.Vertices.size() * sizeof(QuadVertex));
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
        auto buffer = NS::TransferPtr(m_Device->newBuffer(upload.data(), upload.size(), MTL::ResourceStorageModeShared));
        auto vertex = NS::TransferPtr(m_Device->newArgumentTable(args.get(), &error));
        auto fragment = NS::TransferPtr(m_Device->newArgumentTable(args.get(), &error));
        Check(bool(buffer) && bool(vertex) && bool(fragment), "Cannot allocate immutable quad draw version");
        vertex->setAddress(buffer->gpuAddress(), 0); vertex->setAddress(buffer->gpuAddress() + 256, 16);
        fragment->setAddress(buffer->gpuAddress() + descriptorOffset, 0);
        quad->Buffers.push_back(std::move(buffer));
        quad->Arguments.push_back(std::move(vertex)); quad->Arguments.push_back(std::move(fragment));
    }
    for (const auto& buffer : quad->Buffers) { Check(bool(buffer), "Cannot allocate quad buffer"); quad->Residency->addAllocation(buffer.get()); }
    quad->Residency->commit();
    Check(quad->ResourceBytes() + m_Observer->Snapshot().ResourceBytes <= 1024 * 1024, "Quad frame exceeds M1 owned resource budget");
    auto depth = NS::TransferPtr(MTL::DepthStencilDescriptor::alloc()->init());
    depth->setDepthCompareFunction(MTL::CompareFunctionLess); depth->setDepthWriteEnabled(true);
    quad->DepthState = NS::TransferPtr(m_Device->newDepthStencilState(depth.get()));
    Check(bool(quad->DepthState), "Cannot create quad depth state");
    const auto serial = m_Serial + 1;
    auto result = std::make_shared<FramebufferReadback>();
    result->Serial = serial; result->Width = quad->Width; result->Height = quad->Height;
    quad->CompletedColor.resize(quad->Width * quad->Height); quad->CompletedEntity.resize(quad->CompletedColor.size()); quad->CompletedDepth.resize(quad->CompletedColor.size());
    auto options = NS::TransferPtr(MTL4::CommitOptions::alloc()->init());
    auto observer = m_Observer;
    auto feedbackDone = quad->Feedback;
    auto feedbackError = quad->FeedbackError;
    options->addFeedbackHandler(MTL4::CommitFeedbackHandlerFunction([observer, feedbackDone, feedbackError](MTL4::CommitFeedback* feedback) {
        std::lock_guard<std::mutex> lock(observer->Mutex); ++observer->Value.Feedbacks;
        if (auto* error = feedback->error()) { ++observer->Value.Errors; observer->Value.Error = error->localizedDescription()->utf8String(); *feedbackError = observer->Value.Error; }
        feedbackDone->store(true);
        observer->Changed.notify_all();
    }));
    slot.Allocator->reset(); // Poll proved completion before this slot can be selected.
    m_Commands->beginCommandBuffer(slot.Allocator.get());
    m_Commands->useResidencySet(quad->Residency.get());
    auto pass = NS::TransferPtr(MTL4::RenderPassDescriptor::alloc()->init());
    for (size_t i = 0; i < 2; ++i)
    {
        auto* attachment = pass->colorAttachments()->object(i);
        attachment->setTexture(quad->Attachments->Targets[i].get()); attachment->setLoadAction(MTL::LoadActionClear);
        attachment->setStoreAction(MTL::StoreActionStore);
        attachment->setClearColor(i == 0 ? MTL::ClearColor{quad->Clear.r, quad->Clear.g, quad->Clear.b, quad->Clear.a} : MTL::ClearColor{double(quad->ClearEntity), 0, 0, 0});
    }
    pass->depthAttachment()->setTexture(quad->Attachments->Targets[2].get()); pass->depthAttachment()->setLoadAction(MTL::LoadActionClear);
    pass->depthAttachment()->setStoreAction(MTL::StoreActionStore); pass->depthAttachment()->setClearDepth(1);
    auto* render = m_Commands->renderCommandEncoder(pass.get());
    if (!render) { m_Commands->endCommandBuffer(); throw std::runtime_error("Cannot create quad encoder"); }
    render->setRenderPipelineState(quad->Shader->Pipeline()); render->setDepthStencilState(quad->DepthState.get());
    render->setViewport({0, 0, double(quad->Width), double(quad->Height), 0, 1});
    render->setCullMode(MTL::CullModeNone); render->setFrontFacingWinding(MTL::WindingClockwise);
    for (size_t i = 0; i < quad->Draws.size(); ++i)
    {
        render->setArgumentTable(quad->Arguments[i * 2].get(), MTL::RenderStageVertex);
        render->setArgumentTable(quad->Arguments[i * 2 + 1].get(), MTL::RenderStageFragment);
        const auto offset = quad->Draws[i].First * sizeof(uint32_t);
        render->drawIndexedPrimitives(MTL::PrimitiveTypeTriangle, quad->Draws[i].Count, MTL::IndexTypeUInt32,
            quad->Buffers[1]->gpuAddress() + offset, quad->Buffers[1]->length() - offset);
    }
    render->endEncoding();
    auto* copy = m_Commands->computeCommandEncoder();
    if (!copy) { m_Commands->endCommandBuffer(); throw std::runtime_error("Cannot create quad readback encoder"); }
    copy->barrierAfterQueueStages(MTL::StageAll, MTL::StageBlit, MTL4::VisibilityOptionDevice);
    for (size_t i = 0; i < 3; ++i)
        copy->copyFromTexture(quad->Attachments->Targets[i].get(), 0, 0, {0, 0, 0}, {quad->Width, quad->Height, 1}, quad->Buffers[0].get(),
            i == 0 ? 0 : i == 1 ? idBase : depthBase, i == 0 ? colorRow : ScalarRow, 0);
    copy->endEncoding(); m_Commands->endCommandBuffer();
    quad->Result = result; slot.Quad = quad;
    const MTL4::CommandBuffer* buffers[] = {m_Commands.get()};
    m_Queue->commit(buffers, 1, options.get()); m_Queue->signalEvent(m_Completed.get(), serial);
    slot.Pending = true;
    { std::lock_guard<std::mutex> lock(m_Observer->Mutex);
      auto& s = m_Observer->Value; ++s.Submitted; ++s.InFlight;
      s.PeakInFlight = std::max(s.PeakInFlight, s.InFlight); if (slot.Serial) ++s.SlotReuses; }
    slot.Serial = serial; m_Serial = serial; m_Next = (m_Next + 1) % m_Frames.size(); Account();
    return result;
}
}
