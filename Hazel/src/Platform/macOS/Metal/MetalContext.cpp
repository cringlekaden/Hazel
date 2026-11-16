#include "MetalContext.h"
#include "MetalDevice.h"
#include "MetalRendererAPI.h"
#include "MetalSurface.h"
#include "Hazel/Renderer/RenderCommand.h"
#include <stdexcept>
namespace Hazel {
struct MetalContext::Surface { NS::SharedPtr<CA::MetalLayer> Layer; bool Resident=false; };
MetalContext::MetalContext(GLFWwindow* window):m_Window(window),m_Device(static_cast<MetalRendererAPI&>(RenderCommand::Backend()).Device()),m_Surface(std::make_unique<Surface>()) {}
MetalContext::~MetalContext() {
    m_Device->Drain();
    if(m_Surface->Layer) {
        if(m_Surface->Resident) m_Device->Queue()->removeResidencySet(m_Surface->Layer->residencySet());
        DetachMetalLayer(m_Window,m_Surface->Layer.get());
        m_Surface->Layer.reset();
    }
    auto observer=m_Device->Observer(); std::lock_guard<std::mutex> lock(observer->Mutex); observer->Value.SurfaceDetached=true;
}
void MetalContext::Init() {
    auto pool=NS::TransferPtr(NS::AutoreleasePool::alloc()->init());
    m_Surface->Layer=NS::TransferPtr(AttachMetalLayer(m_Window,m_Device->NativeDevice()));
    if(!m_Surface->Layer->residencySet()) throw std::runtime_error("CAMetalLayer residency set unavailable");
    m_Device->Queue()->addResidencySet(m_Surface->Layer->residencySet());
    m_Surface->Resident=true;
    MetalDevice::CheckStartupFailure(MetalStartupFailure::AfterSurface);
}
void MetalContext::SetVSync(bool enabled) { m_Surface->Layer->setDisplaySyncEnabled(enabled); }
void MetalContext::SwapBuffers() {
    auto size=UpdateMetalLayer(m_Window,m_Surface->Layer.get());
    m_Device->Present(m_Surface->Layer.get(),size.Width,size.Height,size.LogicalWidth,size.LogicalHeight,size.Scale,size.Visible);
}
}
