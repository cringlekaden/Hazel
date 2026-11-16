#include "MetalFramebuffer.h"
#include "MetalRendererAPI.h"
#include "MetalDevice.h"
#include "MetalQuadFrame.h"
#include "MetalShader.h"
#include "Hazel/Renderer/RenderCommand.h"
#include <cmath>
#include <stdexcept>
#include <utility>

namespace Hazel {
namespace {
void Check(bool value, const char* message) { if (!value) throw std::logic_error(message); }
[[noreturn]] void Unsupported() { throw std::logic_error("Operation unavailable on the qualified Metal offscreen framebuffer profile"); }
}
void MetalFramebuffer::Validate(const FramebufferSpecification& specification)
{
    const auto& attachments = specification.Attachments.Attachments;
    Check(specification.Width && specification.Height && specification.Width <= 32 && specification.Height <= 32 &&
        specification.Samples == 1 && !specification.SwapChainTarget && attachments.size() == 3 &&
        attachments[0].TextureFormat == FramebufferTextureFormat::RGBA32F &&
        attachments[1].TextureFormat == FramebufferTextureFormat::RED_INTEGER &&
        attachments[2].TextureFormat == FramebufferTextureFormat::DEPTH32F,
        "Metal framebuffer requires 1..32 extent, one sample, ordered RGBA32F/R32Sint/Depth32F attachments and no swapchain target");
}
MetalFramebuffer::MetalFramebuffer(std::shared_ptr<MetalDevice> device, const FramebufferSpecification& specification)
    : m_Device(std::move(device)), m_Specification(specification)
{
    Validate(specification);
    Check(bool(m_Device), "Metal framebuffer requires its renderer device");
    m_Attachments = Allocate(specification.Width, specification.Height);
}
std::shared_ptr<const MetalFramebufferAttachments> MetalFramebuffer::Allocate(uint32_t width, uint32_t height) const
{
    Check(!m_Device->m_Stopped, "Cannot allocate framebuffer attachments after Metal device shutdown");
    auto pool = NS::TransferPtr(NS::AutoreleasePool::alloc()->init());
    auto generation = std::make_shared<MetalFramebufferAttachments>();
    generation->Width = width; generation->Height = height;
    auto descriptor = NS::TransferPtr(MTL::TextureDescriptor::alloc()->init());
    descriptor->setTextureType(MTL::TextureType2D); descriptor->setWidth(width); descriptor->setHeight(height);
    descriptor->setStorageMode(MTL::StorageModePrivate); descriptor->setUsage(MTL::TextureUsageRenderTarget);
    const MTL::PixelFormat formats[] = {MTL::PixelFormatRGBA32Float, MTL::PixelFormatR32Sint, MTL::PixelFormatDepth32Float};
    for (size_t i = 0; i < generation->Targets.size(); ++i)
    {
        descriptor->setPixelFormat(formats[i]);
        generation->Targets[i] = NS::TransferPtr(m_Device->NativeDevice()->newTexture(descriptor.get()));
        if (!generation->Targets[i]) throw std::runtime_error("Cannot allocate Metal framebuffer attachment generation");
    }
    return generation;
}
MetalRendererAPI& MetalFramebuffer::Backend() const
{
    auto* backend = dynamic_cast<MetalRendererAPI*>(&RenderCommand::Backend());
    Check(backend && backend->Device() == m_Device, "Framebuffer belongs to a different or released Metal backend");
    return *backend;
}
void MetalFramebuffer::Bind()
{
    Check(!m_Frame, "Submit or cancel the prior framebuffer packet before binding again");
    auto& backend = Backend();
    auto shader = std::static_pointer_cast<MetalShader>(backend.LoadShader("Renderer2D_Quad"));
    auto generation = m_SubmittedGeneration ? Allocate(m_Specification.Width, m_Specification.Height) : m_Attachments;
    auto frame = std::make_shared<MetalQuadFrame>();
    frame->Width = generation->Width; frame->Height = generation->Height;
    frame->Shader = std::move(shader); frame->Attachments = generation;
    backend.BindFramebuffer(frame);
    m_Frame = std::move(frame); m_Attachments = std::move(generation); m_SubmittedGeneration = false;
    m_Readback.reset(); // this pass must not expose an older generation as current pixels
}
void MetalFramebuffer::Unbind() { Backend().UnbindFramebuffer(m_Frame); }
void MetalFramebuffer::Clear(glm::vec4 color)
{
    Backend().CheckFramebufferClear(m_Frame);
    for (int i = 0; i < 4; ++i) Check(std::isfinite(color[i]), "Framebuffer clear color must be finite");
    m_Frame->Clear = color; m_Frame->ClearEntity = -1; // all attachments clear on pass begin; depth is one
}
void MetalFramebuffer::ClearAttachment(uint32_t attachment, int value)
{
    Check(attachment == 1, "Only signed entity attachment one supports integer clear");
    Backend().CheckFramebufferClear(m_Frame);
    m_Frame->ClearEntity = value;
}
Ref<const FramebufferReadback> MetalFramebuffer::Submit()
{
    Check(m_Frame && !Backend().IsFramebufferBound(m_Frame), "Unbind framebuffer after EndScene before submitting");
    // Busy leaves this exact CPU packet/generation available for retry or cancellation.
    try
    {
        auto result = m_Device->SubmitQuadFrame(m_Frame);
        if (result) { m_Readback = result; m_Frame.reset(); m_SubmittedGeneration = true; }
        return result;
    }
    catch (...) { m_Frame.reset(); throw; }
}
void MetalFramebuffer::Poll()
{
    m_Device->Poll();
    const auto proof = m_Device->Observer()->Snapshot();
    if (proof.Errors) throw std::runtime_error(proof.Error);
}
void MetalFramebuffer::Cancel()
{
    if (m_Frame && Backend().IsFramebufferBound(m_Frame)) Backend().UnbindFramebuffer(m_Frame);
    m_Frame.reset(); m_Readback.reset(); // never abort or release a submitted generation
}
void MetalFramebuffer::Resize(uint32_t width, uint32_t height)
{
    Check(!m_Frame, "Submit or cancel the framebuffer packet before resize");
    auto specification = m_Specification;
    specification.Width = width; specification.Height = height;
    Validate(specification);
    if (width == m_Specification.Width && height == m_Specification.Height) return;
    auto generation = Allocate(width, height); // publish only on complete allocation success
    m_Attachments = std::move(generation); m_Specification = specification;
    m_SubmittedGeneration = false; m_Readback.reset();
}
size_t MetalFramebuffer::ReadIndex(int x, int y)
{
    Poll();
    Check(m_Readback && m_Readback->Completed && m_Readback->Error.empty(), "Framebuffer readback is unavailable or pending; poll without a GPU wait");
    Check(x >= 0 && y >= 0 && uint32_t(x) < m_Readback->Width && uint32_t(y) < m_Readback->Height, "Framebuffer readback pixel is out of bounds");
    return size_t(y) * m_Readback->Width + uint32_t(x);
}
glm::vec4 MetalFramebuffer::ReadColorPixel(uint32_t attachment, int x, int y)
{
    Check(attachment == 0, "Color readback requires RGBA32F attachment zero");
    const auto index = ReadIndex(x, y); return m_Readback->Color[index];
}
int MetalFramebuffer::ReadPixel(uint32_t attachment, int x, int y)
{
    Check(attachment == 1, "Integer readback requires signed attachment one");
    const auto index = ReadIndex(x, y); return m_Readback->Entity[index];
}
float MetalFramebuffer::ReadDepthPixel(int x, int y) { const auto index = ReadIndex(x, y); return m_Readback->Depth[index]; }
void MetalFramebuffer::EncodeOutput(const Ref<Framebuffer>&) { Unsupported(); }
void MetalFramebuffer::DrawEntityOutlineTo(const Ref<Framebuffer>&, int, int, glm::vec4) { Unsupported(); }
void MetalFramebuffer::CompositeWorldTo(const Ref<Framebuffer>&, float) { Unsupported(); }
uint32_t MetalFramebuffer::GetColorAttachmentRendererID(uint32_t) const { Unsupported(); }
}
