#pragma once
#include "Hazel/Renderer/RendererAPI.h"
#include "MetalProof.h"
#include <filesystem>
#include <functional>
#include <mutex>
#include <unordered_map>
#include <vector>
#include <array>
#include <memory>
#include <stdexcept>

namespace Hazel {
class MetalDevice;
class Shader;
class Texture2D;
struct TextureSpecification;
struct FramebufferSpecification;
class Framebuffer;
struct MetalRenderer2DFrame;
struct FramebufferReadback;
struct MetalTextureVersion;
struct MetalRenderer2DRun
{
    std::shared_ptr<Shader> Shader;
    const void* Vertices = nullptr;
    uint32_t First = 0, Count = 0;
    std::shared_ptr<Texture2D> Font;
    bool Picking = true;
};
// Native Metal 4; only explicitly qualified drawing/resource profiles are available.
class MetalRendererAPI final : public RendererAPI {
public:
    MetalRendererAPI();
    ~MetalRendererAPI() override;
    void Init(const RendererSettings& settings) override;
    void Shutdown() override;
    const RendererCapabilities& GetCapabilities() const override { return m_Caps; }
    const RendererSettings& GetSettings() const override { return m_Resolution.Effective; }
    const RendererResolution& GetResolution() const override { return m_Resolution; }
    void PushState() override;
    void PopState() override;
    void SetPickingMask(glm::bvec4) override;
    RasterState GetRasterState() const override;
    void SetRasterState(const RasterState&) override;
    void BeginTextCompositing() override;
    void SetTextCompositing(bool) override;
    void EndTextCompositing() override;
    void SetViewport(uint32_t, uint32_t, uint32_t, uint32_t) override;
    void SetClearColor(const glm::vec4&) override;
    void Clear() override;
    void DrawIndexed(const Ref<VertexArray>&, uint32_t, uint32_t) override;
    void DrawLines(const Ref<VertexArray>&, uint32_t, uint32_t) override;
    void SetLineWidth(float) override;
    uint32_t GetMaxTextureSlots() const override { return 32; }
    // Renderer2D owns packing/batching; native immutable runs own GPU versions.
    void BeginRenderer2DScene(const glm::mat4& camera);
    void EnqueueRenderer2DBatch(const std::vector<MetalRenderer2DRun>& runs,
        const std::array<std::shared_ptr<const MetalTextureVersion>, 32>& textures, uint32_t textureCount);
    void EndRenderer2DScene();
    std::shared_ptr<Framebuffer> CreateFramebuffer(const FramebufferSpecification& specification);
    std::shared_ptr<Texture2D> CreateTexture(const TextureSpecification& specification, const std::string& path = {});
    std::shared_ptr<Shader> LoadShader(const std::string& name, uint32_t slots = 32);
    void BindUniform(uint32_t binding, const std::shared_ptr<const std::vector<uint8_t>>& bytes);
    void BindTexture(uint32_t slot, std::shared_ptr<const MetalTextureVersion> texture);
    void BindShader(const std::string& name);
    void RetainResource(std::shared_ptr<void> resource);
    std::shared_ptr<MetalDevice> Device() const {
        if (m_Stopped) throw std::logic_error("Cannot access Metal resources after backend shutdown");
        return m_Device;
    }
    std::shared_ptr<MetalProofObserver> Observe() const;
    // Explicit native diagnostics; no production application/CLI enables these.
    void EnableTransferProof();
    void SkipNextDrawableForTesting();
    void OverrideNextExtentForTesting(uint32_t width, uint32_t height);
    std::function<void()> GateQueueForTesting();
    void StartCapture(const std::filesystem::path& path);
    void StopCapture();
    static void FailNextStartupForTesting(MetalStartupFailure point);
private:
    friend class MetalFramebuffer;
    void BindFramebuffer(const std::shared_ptr<MetalRenderer2DFrame>& frame);
    void UnbindFramebuffer(const std::shared_ptr<MetalRenderer2DFrame>& frame);
    bool IsFramebufferBound(const std::shared_ptr<MetalRenderer2DFrame>& frame) const;
    void CheckFramebufferClear(const std::shared_ptr<MetalRenderer2DFrame>& frame) const;
    std::shared_ptr<MetalDevice> m_Device;
    std::mutex m_ShaderMutex;
    bool m_Stopped = false, m_Renderer2DScene = false;
    glm::mat4 m_Renderer2DCamera{1};
    std::weak_ptr<MetalRenderer2DFrame> m_Renderer2DFrame;
    std::unordered_map<std::string, std::shared_ptr<Shader>> m_Shaders;
    std::array<std::weak_ptr<const std::vector<uint8_t>>, 8> m_Uniforms;
    std::array<std::shared_ptr<const MetalTextureVersion>, 8> m_Textures;
    std::string m_BoundShader;
    RasterState m_Raster;
    std::vector<RasterState> m_StateStack;
    RendererCapabilities m_Caps;
    RendererResolution m_Resolution;
};
}
