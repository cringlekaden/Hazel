#pragma once
#include "Hazel/Renderer/RendererAPI.h"
#include "MetalProof.h"
#include <filesystem>
#include <functional>
#include <mutex>
#include <unordered_map>

namespace Hazel {
class MetalDevice;
class Shader;
// Only presentation is implemented in M1. Drawing calls fail, never enter OpenGL.
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
    uint32_t GetMaxTextureSlots() const override { return 0; }
    std::shared_ptr<Shader> LoadShader(const std::string& name, uint32_t slots = 32);
    std::shared_ptr<MetalDevice> Device() const { return m_Device; }
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
    std::shared_ptr<MetalDevice> m_Device;
    std::mutex m_ShaderMutex;
    bool m_Stopped = false;
    std::unordered_map<std::string, std::shared_ptr<Shader>> m_Shaders;
    RendererCapabilities m_Caps;
    RendererResolution m_Resolution;
};
}
