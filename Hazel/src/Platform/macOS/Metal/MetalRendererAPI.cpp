#include "MetalRendererAPI.h"
#include "MetalRenderer2DFrame.h"
#include "MetalDevice.h"
#include "MetalShader.h"
#include <stdexcept>
#include <algorithm>
#include <cmath>
namespace Hazel {
namespace { [[noreturn]] void Unavailable() { throw std::logic_error("Operation unavailable in the Metal 4 Renderer2D runtime"); } }
MetalRendererAPI::MetalRendererAPI():m_Device(std::make_shared<MetalDevice>()) {
    m_Caps.MaxTextureSlots=32; m_Caps.MaxTextureSize=4096; m_Caps.MaxColorAttachments=2; m_Caps.MaxDrawBuffers=2; m_Caps.Vendor="Apple"; m_Caps.Device=m_Device->NativeDevice()->name()->utf8String(); m_Caps.Driver="Metal 4";
}
MetalRendererAPI::~MetalRendererAPI() { Shutdown(); }
void MetalRendererAPI::Init(const RendererSettings& settings) {
    m_Resolution.Requested=settings; m_Resolution.Effective=settings;
    m_Resolution.Effective.TextureSlots=std::max(2u, std::min(settings.TextureSlots, 32u)); m_Resolution.Effective.PreferShaderBinaries=false;
    m_Resolution.TextureReason="Metal Renderer2D: tier-2 texture table; requested batch slot limit";
    m_Resolution.ShaderReason="Offline Metal 4 shader artifacts; Renderer2D/world/output passes";
}
void MetalRendererAPI::Shutdown() {
    std::lock_guard<std::mutex> lock(m_ShaderMutex);
    if(m_Stopped) return;
    m_Renderer2DFrame.reset(); m_Renderer2DScene=false;
    m_Device->Drain();
    m_Shaders.clear();
    m_Stopped=true;
}
std::shared_ptr<Shader> MetalRendererAPI::LoadShader(const std::string& name, uint32_t slots) {
    MetalShaderArtifact::ValidateName(name, slots);
    std::lock_guard<std::mutex> lock(m_ShaderMutex);
    if(m_Stopped) throw std::logic_error("Cannot load Metal shaders after backend shutdown");
    const auto key=name+"-"+std::to_string(slots);
    auto found=m_Shaders.find(key);
    if(found!=m_Shaders.end()) return found->second;
    auto shader=std::make_shared<MetalShader>(m_Device,name,slots);
    m_Shaders.emplace(key,shader); // Publish only after library/pipeline/reflection validation.
    return shader;
}
void MetalRendererAPI::PushState() { m_StateStack.push_back(m_Raster); }
void MetalRendererAPI::PopState() { if(m_StateStack.empty())throw std::logic_error("Metal raster state stack underflow"); m_Raster=m_StateStack.back(); m_StateStack.pop_back(); }
void MetalRendererAPI::SetPickingMask(glm::bvec4 mask) { m_Raster.PickingMask=mask; }
RasterState MetalRendererAPI::GetRasterState() const { return m_Raster; }
void MetalRendererAPI::SetRasterState(const RasterState& state) { m_Raster=state; }
void MetalRendererAPI::BeginTextCompositing() { Unavailable(); }
void MetalRendererAPI::SetTextCompositing(bool) { Unavailable(); }
void MetalRendererAPI::EndTextCompositing() { Unavailable(); }
void MetalRendererAPI::SetViewport(uint32_t x,uint32_t y,uint32_t width,uint32_t height) {
    if (!m_Renderer2DFrame.expired()) throw std::logic_error("Metal framebuffer viewport is fixed by its attachment extent");
    m_Raster.Viewport={x,y,width,height};
}
void MetalRendererAPI::SetClearColor(const glm::vec4& color) {
    if (!m_Renderer2DFrame.expired()) throw std::logic_error("Use Framebuffer::Clear for an explicit Metal offscreen pass");
    m_Device->SetClear(color);
}
void MetalRendererAPI::Clear() {
    if (!m_Renderer2DFrame.expired()) throw std::logic_error("Use Framebuffer::Clear for an explicit Metal offscreen pass");
    // Explicit Metal 4 load-action clear occurs at presentation.
}
void MetalRendererAPI::DrawIndexed(const Ref<VertexArray>&,uint32_t,uint32_t) { Unavailable(); }
void MetalRendererAPI::DrawLines(const Ref<VertexArray>&,uint32_t,uint32_t) { Unavailable(); }
void MetalRendererAPI::SetLineWidth(float width) {
    if(!std::isfinite(width)||width<=0)throw std::invalid_argument("Line width must be finite and positive");
    m_Raster.LineWidth=std::clamp(width,m_Caps.MinLineWidth,m_Caps.MaxLineWidth);
}
std::shared_ptr<MetalProofObserver> MetalRendererAPI::Observe() const { return m_Device->Observer(); }
void MetalRendererAPI::EnableTransferProof() { m_Device->EnableTransferProof(); }
void MetalRendererAPI::SkipNextDrawableForTesting() { m_Device->SkipNextDrawable(); }
void MetalRendererAPI::OverrideNextExtentForTesting(uint32_t w,uint32_t h) { m_Device->OverrideNextExtent(w,h); }
std::function<void()> MetalRendererAPI::GateQueueForTesting() { return m_Device->GateQueue(); }
void MetalRendererAPI::StartCapture(const std::filesystem::path& path) { m_Device->StartCapture(path); }
void MetalRendererAPI::StopCapture() { m_Device->StopCapture(); }
void MetalRendererAPI::FailNextStartupForTesting(MetalStartupFailure point) { MetalDevice::FailNextStartup(point); }
}
