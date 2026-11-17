#pragma once
#include <vector>
#include <cstdint>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>

namespace Hazel {
// M1 observations: GPU completion and actual drawable presentation are different boundaries.
// This observer owns no GPU objects and may outlive Application shutdown.
// Optional native qualification readback of the actual UI drawable. This only
// adds a completion-owned blit; it never substitutes another rendering path.
struct MetalEditorCapture {
    struct Image { uint32_t Viewport=0, Width=0, Height=0; std::vector<uint8_t> BGRA; };
    uint64_t Serial=0;
    bool Completed=false;
    std::vector<Image> Images;
};
struct MetalProofSnapshot {
    uint32_t EditorHosts=0, PeakEditorHosts=0;
    uint64_t ImGuiResourceBytes=0, PeakImGuiResourceBytes=0;
    uint64_t Submitted=0, Completed=0, Verified=0, Presented=0, CompletedPresented=0;
    uint64_t PresentationCallbacks=0, ZeroPresentationTime=0, Feedbacks=0, Errors=0;
    uint64_t InFlight=0, PeakInFlight=0, Reservations=0, AbortedReservations=0, SlotReuses=0;
    uint64_t SkippedZero=0, SkippedUnavailable=0, SkippedHidden=0, SkippedBusy=0;
    uint64_t ResourceBytes=0, PeakResourceBytes=0, AllocatorBytes=0, PeakAllocatorBytes=0;
    uint64_t ResidencyAllocations=0, PendingAtShutdown=0;
    uint64_t BufferAllocations=0, BufferReuses=0;
    uint64_t AttachmentBytes=0, PeakAttachmentBytes=0, AttachmentGenerations=0, AttachmentAllocations=0, AttachmentReuses=0;
    uint32_t PixelWidth=0, PixelHeight=0, LogicalWidth=0, LogicalHeight=0;
    double BackingScale=0, LastPresentedTime=0;
    bool VSync=true, ShutdownComplete=false, SurfaceDetached=false, DeviceReleased=false;
    std::string Error;
};
class MetalProofObserver {
public:
    MetalProofSnapshot Snapshot() const { std::lock_guard<std::mutex> lock(Mutex); return Value; }
private:
    friend class MetalDevice;
    friend class MetalContext;
    friend class MetalFramebuffer;
    mutable std::mutex Mutex;
    MetalProofSnapshot Value;
    std::condition_variable Changed;
};
enum class MetalStartupFailure { None, AfterDevice, AfterQueue, AfterSurface };
}
