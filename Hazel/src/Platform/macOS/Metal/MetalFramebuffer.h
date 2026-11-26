#pragma once
#include "Hazel/Renderer/Resources/Framebuffer.h"
#include <Metal/Metal.hpp>
#include <array>
#include <memory>
#include <atomic>

namespace Hazel {
class MetalDevice;
struct SceneCameraView;
struct AmbientOcclusionTiming;
struct AmbientOcclusionSettings;
struct MetalAOPipelines;
struct MetalAOResources;
struct MetalTemporalPipeline;
struct MetalTemporalHistory;
struct MetalTemporalCounters;
struct TemporalTiming;
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
    void PrepareAmbientOcclusion(const Ref<Framebuffer>& depth, const SceneCameraView& camera, const AmbientOcclusionSettings& settings, unsigned debugView, const std::shared_ptr<AmbientOcclusionTiming>& timing, unsigned temporalPhase = 0);
    void SetTemporalProducer(); // Private same-queue scratch; never a presentation/readback target.
    void SetWorldProducer(); // Private world intermediate, consumed only by CompositeWorldTo.
    void ResolveTemporal(const Ref<Framebuffer>& radiance, const Ref<Framebuffer>& motion,
        glm::vec2 jitter, bool valid, unsigned debug, const glm::mat4& skyMotion, unsigned skyMode,
        const std::shared_ptr<std::atomic<bool>>& submitted, const std::shared_ptr<TemporalTiming>& timing);
    void Bind() override;
    void Unbind() override;
    void PrepareDirectionalShadows(const Ref<Framebuffer>& depth) override;
    // Resume an unsubmitted CPU packet after a narrow producer pass.
    void Resume();
    void Resume(glm::vec4 clearColor); // Inspection color override preserves picking/depth clears.
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
    std::shared_ptr<const MetalFramebufferAttachments> Allocate(uint32_t width, uint32_t height, bool temporalHistory = false) const;
    MetalRendererAPI& Backend() const;
    size_t ReadIndex(int x, int y);
    std::shared_ptr<MetalDevice> m_Device;
    FramebufferSpecification m_Specification;
    std::shared_ptr<const MetalFramebufferAttachments> m_Attachments;
    std::shared_ptr<MetalRenderer2DFrame> m_Frame;
    Ref<const FramebufferReadback> m_Readback, m_EditorReadback;
    std::vector<Ref<const FramebufferReadback>> m_EditorPendingReadbacks;
    std::shared_ptr<MetalAOPipelines> m_AOPipelines;
    std::vector<std::shared_ptr<MetalAOResources>> m_AOResources;
    std::shared_ptr<MetalTemporalPipeline> m_TemporalPipeline;
    std::shared_ptr<MetalTemporalHistory> m_TemporalPrevious;
    std::vector<std::shared_ptr<const MetalFramebufferAttachments>> m_TemporalColors;
    std::vector<std::shared_ptr<MetalTemporalCounters>> m_TemporalCounters;
    bool m_TemporalProducer=false, m_WorldProducer=false, m_ShadowProducer=false;
    bool m_SubmittedGeneration = false;
    std::vector<std::shared_ptr<const MetalFramebufferAttachments>> m_Available;
};
}
