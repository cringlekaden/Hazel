#include "MetalRendererAPI.h"
#include "MetalDevice.h"
#include <stdexcept>
namespace Hazel {
namespace { [[noreturn]] void Unavailable() { throw std::logic_error("Metal 4 drawing services are not implemented in M1"); } }
MetalRendererAPI::MetalRendererAPI():m_Device(std::make_shared<MetalDevice>()) {
    m_Caps.Vendor="Apple"; m_Caps.Device=m_Device->NativeDevice()->name()->utf8String(); m_Caps.Driver="Metal 4";
}
MetalRendererAPI::~MetalRendererAPI() = default;
void MetalRendererAPI::Init(const RendererSettings& settings) {
    m_Resolution.Requested=settings; m_Resolution.Effective=settings;
    m_Resolution.Effective.TextureSlots=0; m_Resolution.Effective.PreferShaderBinaries=false;
    m_Resolution.TextureReason="M1 presentation only; drawing resources unavailable";
    m_Resolution.ShaderReason="M2 shader qualification has not begun";
}
void MetalRendererAPI::Shutdown() { m_Device->Drain(); }
void MetalRendererAPI::PushState() { Unavailable(); }
void MetalRendererAPI::PopState() { Unavailable(); }
void MetalRendererAPI::SetPickingMask(glm::bvec4) { Unavailable(); }
RasterState MetalRendererAPI::GetRasterState() const { Unavailable(); }
void MetalRendererAPI::SetRasterState(const RasterState&) { Unavailable(); }
void MetalRendererAPI::BeginTextCompositing() { Unavailable(); }
void MetalRendererAPI::SetTextCompositing(bool) { Unavailable(); }
void MetalRendererAPI::EndTextCompositing() { Unavailable(); }
void MetalRendererAPI::SetViewport(uint32_t,uint32_t,uint32_t,uint32_t) { /* Surface uses physical GLFW extent, not logical resize events. */ }
void MetalRendererAPI::SetClearColor(const glm::vec4& color) { m_Device->SetClear(color); }
void MetalRendererAPI::Clear() { /* Explicit Metal 4 load-action clear occurs at presentation. */ }
void MetalRendererAPI::DrawIndexed(const Ref<VertexArray>&,uint32_t,uint32_t) { Unavailable(); }
void MetalRendererAPI::DrawLines(const Ref<VertexArray>&,uint32_t,uint32_t) { Unavailable(); }
void MetalRendererAPI::SetLineWidth(float) { Unavailable(); }
std::shared_ptr<MetalProofObserver> MetalRendererAPI::Observe() const { return m_Device->Observer(); }
void MetalRendererAPI::EnableTransferProof() { m_Device->EnableTransferProof(); }
void MetalRendererAPI::SkipNextDrawableForTesting() { m_Device->SkipNextDrawable(); }
void MetalRendererAPI::OverrideNextExtentForTesting(uint32_t w,uint32_t h) { m_Device->OverrideNextExtent(w,h); }
std::function<void()> MetalRendererAPI::GateQueueForTesting() { return m_Device->GateQueue(); }
void MetalRendererAPI::StartCapture(const std::filesystem::path& path) { m_Device->StartCapture(path); }
void MetalRendererAPI::StopCapture() { m_Device->StopCapture(); }
void MetalRendererAPI::FailNextStartupForTesting(MetalStartupFailure point) { MetalDevice::FailNextStartup(point); }
}
