#pragma once
#include "Hazel/Renderer/Resources/Framebuffer.h"
#include <Metal/Metal.hpp>
#include <array>
#include <memory>

namespace Hazel {
class MetalDevice;
class MetalRendererAPI;
struct MetalRenderer2DFrame;
// Immutable attachment identity. Each submitted generation is completion-retained.
struct MetalFramebufferAttachments
{
    uint32_t Width = 0, Height = 0;
    std::array<NS::SharedPtr<MTL::Texture>, 3> Targets;
};
class MetalFramebuffer final : public Framebuffer
{
public:
    MetalFramebuffer(std::shared_ptr<MetalDevice> device, const FramebufferSpecification& specification);
    ~MetalFramebuffer() override = default;
    MetalFramebuffer(const MetalFramebuffer&) = delete;
    MetalFramebuffer& operator=(const MetalFramebuffer&) = delete;
    void Bind() override;
    void Unbind() override;
    void Resize(uint32_t width, uint32_t height) override;
    void DispatchColorTo(const Ref<Framebuffer>& destination, const Ref<ComputePipeline>& pipeline, const std::vector<uint8_t>& parameters) override;
    void ClearEditor(glm::vec4 color) override { Clear(color); }
    void Clear(glm::vec4 color) override;
    Ref<const FramebufferReadback> Submit() override;
    void Poll() override;
    void Cancel() override;
    glm::vec4 ReadColorPixel(uint32_t attachment, int x, int y) override;
    int ReadPixel(uint32_t attachment, int x, int y) override;
    float ReadDepthPixel(int x, int y) override;
    void ClearAttachment(uint32_t attachment, int value) override;
    void EncodeOutput(const Ref<Framebuffer>& destination = {}) override;
    void DrawEntityOutlineTo(const Ref<Framebuffer>& destination, int entity, int radius, glm::vec4 color) override;
    void CompositeWorldTo(const Ref<Framebuffer>& destination, float exposure) override;
    EditorTexture GetEditorTexture() const override;
    void ResetEditorReadback() override { m_EditorReadback.reset(); m_EditorPendingReadbacks.clear(); }
    int ReadEditorEntity(int x, int y) override;
    uint32_t GetColorAttachmentRendererID(uint32_t index = 0) const override;
    const FramebufferSpecification& GetSpecification() const override { return m_Specification; }
    std::shared_ptr<const MetalFramebufferAttachments> Snapshot() const { return m_Attachments; }
private:
    static void Validate(const FramebufferSpecification& specification);
    std::shared_ptr<const MetalFramebufferAttachments> Allocate(uint32_t width, uint32_t height) const;
    MetalRendererAPI& Backend() const;
    size_t ReadIndex(int x, int y);
    std::shared_ptr<MetalDevice> m_Device;
    FramebufferSpecification m_Specification;
    std::shared_ptr<const MetalFramebufferAttachments> m_Attachments;
    std::shared_ptr<MetalRenderer2DFrame> m_Frame;
    Ref<const FramebufferReadback> m_Readback, m_EditorReadback;
    std::vector<Ref<const FramebufferReadback>> m_EditorPendingReadbacks;
    bool m_SubmittedGeneration = false;
    std::vector<std::shared_ptr<const MetalFramebufferAttachments>> m_Available;
};
}
