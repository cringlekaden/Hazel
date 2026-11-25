#include "MetalDevice.h"
#include "MetalFramebuffer.h"
#include "MetalRenderer2DFrame.h"
#include "MetalRendererAPI.h"
#include "MetalSurface.h"
#include "Hazel/Renderer/RenderCommand.h"
#include "Hazel/ImGui/Backends/imgui_impl_metal4.h"
#include <imgui.h>
#include <GLFW/glfw3.h>
#import <Metal/Metal.h>
#include <stdexcept>
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <utility>
#include <cmath>

#if !__has_feature(objc_arc)
#error Hazel Metal editor ownership requires Objective-C ARC
#endif

namespace Hazel {
namespace {
struct EditorSurface {
    GLFWwindow* Window = nullptr;
    NS::SharedPtr<CA::MetalLayer> Layer;
    bool First = true;
    ~EditorSurface() { if (Layer) DetachMetalLayer(Window, Layer.get()); }
};
void CreateEditorWindow(ImGuiViewport* viewport) {
    auto& backend = static_cast<MetalRendererAPI&>(RenderCommand::Backend());
    auto surface = std::make_unique<EditorSurface>();
    surface->Window = static_cast<GLFWwindow*>(viewport->PlatformHandle);
    surface->Layer = NS::TransferPtr(AttachMetalLayer(surface->Window, backend.Device()->NativeDevice()));
    viewport->RendererUserData = surface.release();
}
void DestroyEditorWindow(ImGuiViewport* viewport) {
    delete static_cast<EditorSurface*>(viewport->RendererUserData);
    viewport->RendererUserData = nullptr;
}
void Check(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
}
void MetalDevice::EnableEditorReadback() {
    Check(m_Editor && !m_Stopped && m_Serial == 0, "Configure diagnostic editor drawables before first submission");
    m_EditorReadbackEnabled = true;
}
std::shared_ptr<MetalEditorCapture> MetalDevice::CaptureEditorFrame() {
    Check(m_Editor && m_EditorReadbackEnabled && !m_Stopped && !m_EditorCapture, "Editor capture requires an initialized renderer and no pending request");
    m_EditorCapture = std::make_shared<MetalEditorCapture>(); return m_EditorCapture;
}
void MetalDevice::InitEditor() {
    Check(!m_Editor && !m_Stopped, "Metal editor initialization requires a live device");
    Check(ImGui_ImplMetal4_Init((__bridge id<MTLDevice>)m_Device.get(), (__bridge id<MTL4CommandQueue>)m_Queue.get(), int(m_Frames.size())), "Cannot initialize ImGui Metal 4 renderer");
    try { Check(ImGui_ImplMetal4_CreateDeviceObjects((__bridge id<MTLDevice>)m_Device.get()), "Cannot create ImGui Metal 4 device resources"); }
    catch (...) { ImGui_ImplMetal4_Shutdown(); throw; }
    if (m_Trace) {
        m_EditorTimestampFrequency = m_Device->queryTimestampFrequency();
        auto descriptor = NS::TransferPtr(MTL4::CounterHeapDescriptor::alloc()->init());
        descriptor->setType(MTL4::CounterHeapTypeTimestamp); descriptor->setCount(3);
        for (auto& frame : m_Frames) {
            NS::Error* error = nullptr;
            frame.EditorCounters = NS::TransferPtr(m_Device->newCounterHeap(descriptor.get(), &error));
            if (!frame.EditorCounters) m_EditorTimestampFrequency = 0;
        }
        if (!m_EditorTimestampFrequency) {
            for (auto& frame : m_Frames) frame.EditorCounters.reset();
            std::printf("M5_EDITOR_COUNTERS unavailable\n");
        }
    }
    auto& platform = ImGui::GetPlatformIO();
    platform.Renderer_CreateWindow = CreateEditorWindow;
    platform.Renderer_DestroyWindow = DestroyEditorWindow;
    m_Editor = true;
}
void MetalDevice::ShutdownEditor() {
    if (!m_Editor) return;
    m_EditorFrames.clear(); m_EditorTextures.clear(); // discard unsubmitted packets only
    Drain(); // shutdown only: retire all consumers before fonts, surfaces and UI storage
    for (auto& frame : m_Frames) frame.EditorCounters.reset();
    ImGui::DestroyPlatformWindows();
    ImGui_ImplMetal4_Shutdown();
    auto& platform = ImGui::GetPlatformIO();
    platform.Renderer_CreateWindow = nullptr;
    platform.Renderer_DestroyWindow = nullptr;
    m_EditorFrames.clear(); m_EditorTextures.clear(); m_EditorQueued = false; m_Editor = false;
    Account();
    if (m_Trace) {
        const auto state = m_Observer->Snapshot();
        std::printf("M5_IMGUI_FINAL bytes=%llu peak_bytes=%llu\n", (unsigned long long)state.ImGuiResourceBytes, (unsigned long long)state.PeakImGuiResourceBytes);
    }
}
void MetalDevice::QueueEditorTextures(std::vector<EditorTexture> textures) {
    Check(m_Editor && !m_EditorQueued, "Finish the prior Metal editor presentation before queuing another");
    m_EditorTextures = std::move(textures); m_EditorQueued = true;
}
void MetalDevice::PresentEditor(CA::MetalLayer* mainLayer, bool visible) {
    const auto cpuStart = std::chrono::steady_clock::now();
    Poll(); // retire completed consumers even when the main editor is minimized
    const auto retired = std::chrono::steady_clock::now();
    if (!m_EditorQueued) return;
    m_EditorQueued = false;
    auto scenes = std::exchange(m_EditorFrames, {});
    struct Pending {
        std::vector<std::shared_ptr<MetalRenderer2DFrame>>& Scenes;
        Frame* Slot = nullptr;
        bool Submitted = false;
        ~Pending() {
            if (Submitted) return;
            for (const auto& frame : Scenes) if (frame->DisplaySource && frame->DisplaySource->Result) {
                frame->DisplaySource->Result->Error = "Not submitted";
                frame->DisplaySource->Result->Completed = true;
            }
            if (Slot && !Slot->Pending) {
                Slot->EditorFrames.clear(); Slot->EditorTextures.clear(); Slot->EditorDrawables.clear(); Slot->EditorLayers.clear();
                Slot->EditorCapture.reset(); Slot->EditorCaptureBuffers.clear(); Slot->EditorFeedback.reset(); Slot->EditorResidency.reset();
                Slot->Renderer2DBuffers.clear();
            }
        }
    } pending{scenes};
    auto textures = std::move(m_EditorTextures);
    if (m_SkipDrawable) {
        m_SkipDrawable=false;
        std::lock_guard<std::mutex> lock(m_Observer->Mutex); ++m_Observer->Value.SkippedUnavailable;
        return;
    }
    auto& slot = m_Frames[m_Next];
    if (slot.Pending) return; // bounded nonblocking admission; no allocator or UI buffer reuse
    auto pool = NS::TransferPtr(NS::AutoreleasePool::alloc()->init());
    struct View { uint32_t ID; ImDrawData* Draw; NS::SharedPtr<CA::MetalDrawable> Drawable; NS::SharedPtr<CA::MetalLayer> Layer; };
    std::vector<View> views;
    uint64_t geometryBytes = 0;
    for (auto* viewport : ImGui::GetPlatformIO().Viewports) {
        if (!viewport->DrawData || viewport->DrawData->TotalVtxCount == 0) continue;
        geometryBytes += uint64_t(viewport->DrawData->TotalVtxCount) * sizeof(ImDrawVert) + uint64_t(viewport->DrawData->TotalIdxCount) * 4;
        Check(geometryBytes <= 64ULL * 1024 * 1024, "Editor geometry exceeds 64 MiB per frame");
        if (viewport == ImGui::GetMainViewport() && !visible && m_Serial) continue;
        auto layer = NS::RetainPtr(mainLayer);
        if (viewport != ImGui::GetMainViewport()) {
            auto* surface = static_cast<EditorSurface*>(viewport->RendererUserData);
            if (!surface) continue;
            const auto size = UpdateMetalLayer(surface->Window, surface->Layer.get());
            if (!size.Width || !size.Height || (!size.Visible && !surface->First)) continue;
            surface->First = false; layer = surface->Layer;
            layer->setDisplaySyncEnabled(mainLayer->displaySyncEnabled());
        }
        if (m_EditorReadbackEnabled && layer->framebufferOnly()) layer->setFramebufferOnly(false);
        const auto size = layer->drawableSize();
        if (size.width <= 0 || size.height <= 0) continue;
        auto drawable = NS::RetainPtr(layer->nextDrawable());
        if (!drawable) continue;
        // Cocoa may change the backing extent after ImGui built this frame.
        // Never encode stale viewport/scissor dimensions into the new drawable.
        // The next GLFW/ImGui frame rebuilds layout for the new native extent.
        const auto* draw = viewport->DrawData;
        const double expectedWidth = std::floor(double(draw->DisplaySize.x) * draw->FramebufferScale.x + .5);
        const double expectedHeight = std::floor(double(draw->DisplaySize.y) * draw->FramebufferScale.y + .5);
        if (expectedWidth != double(drawable->texture()->width()) || expectedHeight != double(drawable->texture()->height())) {
            if (m_Trace) std::printf("M5_EDITOR_EXTENT_SKIP viewport=%u expected=%.0fx%.0f actual=%lux%lu\n",viewport->ID,expectedWidth,expectedHeight,
                (unsigned long)drawable->texture()->width(),(unsigned long)drawable->texture()->height());
            std::lock_guard<std::mutex> lock(m_Observer->Mutex); ++m_Observer->Value.SkippedUnavailable;
            continue;
        }
        views.push_back({viewport->ID,viewport->DrawData, std::move(drawable), std::move(layer)});
    }
    if (views.empty()) return;
    const auto acquired = std::chrono::steady_clock::now();
    Check(views.size() <= 64 && scenes.size() <= 64, "Metal editor exceeds 64 visible hosts or producer passes");
    auto descriptor = NS::TransferPtr(MTL::ResidencySetDescriptor::alloc()->init());
    NS::Error* error = nullptr;
    auto residency = NS::TransferPtr(m_Device->newResidencySet(descriptor.get(), &error));
    Check(bool(residency), "Cannot allocate editor consumer residency");
    std::vector<std::shared_ptr<const MetalTextureVersion>> uiVersions;
    for (const auto& texture : textures) {
        auto* native = reinterpret_cast<MTL::Texture*>(texture.Handle);
        Check(native && native->device() == m_Device.get() && texture.Owner, "Editor texture requires an immutable native owner on this device");
        residency->addAllocation(native);
        if (texture.SamplerHandle) {
            // Nonzero sampler references are emitted only by MetalTexture2D;
            // Owner is its captured immutable MetalTextureVersion, not the wrapper.
            auto version=std::static_pointer_cast<const MetalTextureVersion>(texture.Owner);
            Check(version->Texture.get()==native && reinterpret_cast<uintptr_t>(version->Sampler.get())==texture.SamplerHandle, "Editor sampler and texture version differ");
            uiVersions.push_back(std::move(version));
        }
    }
    residency->commit();
    pending.Slot = &slot;
    slot.Renderer2DCursor = 0;slot.LocalLightCursor=0;
    for (const auto& scene : scenes) PrepareRenderer2DFrame(scene);
    if (m_EditorCapture) {
        for (const auto& view : views) {
            const auto width = uint32_t(view.Drawable->texture()->width()), height = uint32_t(view.Drawable->texture()->height());
            const size_t row = (size_t(width)*4+255)&~size_t(255);
            auto buffer = AcquireRenderer2DBuffer(row*height);
            residency->addAllocation(buffer.get());
            slot.EditorCaptureBuffers.push_back(std::move(buffer));
            m_EditorCapture->Images.push_back({view.ID,width,height,std::vector<uint8_t>(size_t(width)*height*4)});
        }
        residency->commit();
        slot.EditorCapture = std::move(m_EditorCapture);
    }
    slot.Renderer2DBuffers.resize(slot.Renderer2DCursor);
    // Include every producer in admission accounting before encoding/commit.
    slot.EditorFrames = scenes; slot.EditorTextures = std::move(textures); slot.EditorResidency = residency;
    // Retain every consumer before commit. No vector growth may fail after GPU
    // work starts while the slot still appears unsubmitted.
    slot.EditorDrawables.reserve(views.size()); slot.EditorLayers.reserve(views.size());
    for (const auto& view : views) { slot.EditorDrawables.push_back(view.Drawable); slot.EditorLayers.push_back(view.Layer); }
    Account();
    slot.Allocator->reset();
    m_Commands->beginCommandBuffer(slot.Allocator.get());
    m_Commands->useResidencySet(residency.get());
    if (slot.EditorCounters) {
        slot.EditorCounters->invalidateCounterRange(NS::Range{0,3});
        m_Commands->writeTimestampIntoHeap(slot.EditorCounters.get(),0);
    }
    const auto prepared = std::chrono::steady_clock::now();
    for (const auto& scene : scenes) {
        m_Commands->useResidencySet(scene->Residency.get());
        EncodeRenderer2DFrame(scene);
    }
    std::set<MTL::Texture*> mipmaps;
    for (const auto& version : uiVersions) if (!version->MipsSubmitted && version->Texture->mipmapLevelCount()>1) mipmaps.insert(version->Texture.get());
    if (!mipmaps.empty()) {
        auto* compute=m_Commands->computeCommandEncoder();
        Check(compute!=nullptr,"Cannot prepare editor texture mipmaps");
        compute->barrierAfterQueueStages(MTL::StageAll,MTL::StageBlit,MTL4::VisibilityOptionDevice);
        for (auto* texture : mipmaps) compute->generateMipmaps(texture);
        compute->barrierAfterStages(MTL::StageBlit,MTL::StageFragment,MTL4::VisibilityOptionDevice);
        compute->endEncoding();
    }
    const auto renderEncoded = std::chrono::steady_clock::now();
    if (slot.EditorCounters) m_Commands->writeTimestampIntoHeap(slot.EditorCounters.get(),1);
    auto format = NS::TransferPtr(MTL4::RenderPassDescriptor::alloc()->init());
    format->colorAttachments()->object(0)->setTexture(views.front().Drawable->texture());
    ImGui_ImplMetal4_NewFrame((__bridge MTL4RenderPassDescriptor*)format.get(), int(m_Next));
    for (const auto& version : uiVersions)
        ImGui_ImplMetal4_SetTextureSampler((__bridge id<MTLTexture>)version->Texture.get(),(__bridge id<MTLSamplerState>)version->Sampler.get());
    for (const auto& view : views) {
        m_Commands->useResidencySet(view.Layer->residencySet());
        auto pass = NS::TransferPtr(MTL4::RenderPassDescriptor::alloc()->init());
        auto* color = pass->colorAttachments()->object(0);
        color->setTexture(view.Drawable->texture()); color->setLoadAction(MTL::LoadActionClear); color->setStoreAction(MTL::StoreActionStore);
        auto* render = m_Commands->renderCommandEncoder(pass.get());
        Check(render != nullptr, "Cannot encode editor consumer");
        render->barrierAfterQueueStages(MTL::StageAll, MTL::StageFragment, MTL4::VisibilityOptionDevice);
        try {
            ImGui_ImplMetal4_RenderDrawData(view.Draw, (__bridge id<MTL4CommandBuffer>)m_Commands.get(), (__bridge id<MTL4RenderCommandEncoder>)render);
        } catch (...) {
            // No commit occurred. Close native encoding before Pending releases
            // its resources so a later frame can reuse this command buffer.
            render->endEncoding();
            m_Commands->endCommandBuffer();
            throw;
        }
        render->endEncoding();
    }
    if (slot.EditorCounters) m_Commands->writeTimestampIntoHeap(slot.EditorCounters.get(),2);
    if (slot.EditorCapture) {
        auto* copy = m_Commands->computeCommandEncoder();
        Check(copy != nullptr, "Cannot encode actual editor drawable readback");
        copy->barrierAfterQueueStages(MTL::StageAll, MTL::StageBlit, MTL4::VisibilityOptionDevice);
        for (size_t image=0; image<views.size(); ++image) {
            const auto& view = views[image];
            const auto width = uint32_t(view.Drawable->texture()->width()), height = uint32_t(view.Drawable->texture()->height());
            const size_t row = (size_t(width)*4+255)&~size_t(255);
            copy->copyFromTexture(view.Drawable->texture(),0,0,{0,0,0},{width,height,1},slot.EditorCaptureBuffers[image].get(),0,row,0);
        }
        copy->endEncoding();
    }
    m_Commands->endCommandBuffer();
    const auto uiEncoded = std::chrono::steady_clock::now();
    Account(); // includes official backend UI buffers before commit
    const auto serial = m_Serial + 1;
    if (slot.EditorCapture) slot.EditorCapture->Serial = serial;
    auto observer = m_Observer;
    const bool trace = m_Trace;
    auto receipt = std::make_shared<Receipt>();
    for (size_t host=0; host<views.size(); ++host) {
        const auto viewport = views[host].ID;
        const bool primary = viewport == ImGui::GetMainViewport()->ID;
        if (!primary && !trace) continue;
        views[host].Drawable->addPresentedHandler(MTL::DrawablePresentedHandlerFunction([observer, receipt, serial, trace, viewport, primary](MTL::Drawable* drawable) {
            const auto time = drawable->presentedTime();
            std::lock_guard<std::mutex> lock(observer->Mutex);
            if (primary) {
                auto& state = observer->Value; ++state.PresentationCallbacks;
                if (time > 0) { ++state.Presented; state.LastPresentedTime = time; receipt->Presented = true; if (receipt->GPU) ++state.CompletedPresented; }
                else ++state.ZeroPresentationTime;
                if (trace) std::printf("M5_EDITOR_PRESENT frame=%llu timestamp=%.9f\n", (unsigned long long)serial,time);
            }
            if (trace) std::printf("M5_EDITOR_HOST_PRESENT frame=%llu viewport=%u timestamp=%.9f\n",(unsigned long long)serial,viewport,time);
        }));
    }
    auto feedbackDone = std::make_shared<std::atomic<bool>>(false);
    slot.EditorFeedback = feedbackDone;
    auto options = NS::TransferPtr(MTL4::CommitOptions::alloc()->init());
    options->addFeedbackHandler(MTL4::CommitFeedbackHandlerFunction([observer, feedbackDone, serial, trace](MTL4::CommitFeedback* feedback) {
        std::lock_guard<std::mutex> lock(observer->Mutex); ++observer->Value.Feedbacks;
        if (auto* error = feedback->error()) { ++observer->Value.Errors; observer->Value.Error = error->localizedDescription()->utf8String(); }
        feedbackDone->store(true);
        if (trace) std::printf("M5_EDITOR_GPU frame=%llu start=%.9f end=%.9f\n", (unsigned long long)serial, feedback->GPUStartTime(), feedback->GPUEndTime());
        observer->Changed.notify_all();
    }));
    slot.Completion = std::move(receipt);
    const auto submitStart = std::chrono::steady_clock::now();
    for (const auto& view : views) m_Queue->wait(view.Drawable.get());
    const MTL4::CommandBuffer* commands[] = {m_Commands.get()};
    m_Queue->commit(commands, 1, options.get());
    const auto committed = std::chrono::steady_clock::now();
    pending.Submitted = true; slot.Pending = true;
    for (const auto& scene : scenes) scene->MarkMipmapsSubmitted();
    for (const auto& version : uiVersions) version->MipsSubmitted=true;
    for (const auto& view : views) {
        m_Queue->signalDrawable(view.Drawable.get()); view.Drawable->present();
    }
    // All producers and ALL secondary-window consumers precede this slot's event.
    m_Queue->signalEvent(m_Completed.get(), serial);
    for (const auto& scene : scenes) if (scene->DisplaySource && scene->DisplaySource->Result) scene->DisplaySource->Result->Serial = serial;
    { std::lock_guard<std::mutex> lock(observer->Mutex); auto& state = observer->Value;
      state.EditorHosts=uint32_t(views.size()); state.PeakEditorHosts=std::max(state.PeakEditorHosts,state.EditorHosts);
      ++state.Submitted; ++state.InFlight; state.PeakInFlight = std::max(state.PeakInFlight, state.InFlight); if (slot.Serial) ++state.SlotReuses; }
    if (m_Trace) {
        const auto milliseconds = [](auto start,auto end) { return std::chrono::duration<double,std::milli>(end-start).count(); };
        std::printf("M5_EDITOR_SUBMIT frame=%llu hosts=%zu producers=%zu geometry_bytes=%llu cpu_ms=%.6f acquire_ms=%.6f prepare_ms=%.6f render_encode_ms=%.6f imgui_encode_ms=%.6f submit_present_ms=%.6f queue_commit_ms=%.6f present_request_ms=%.6f retire_ms=%.6f\n",
            (unsigned long long)serial,views.size(),scenes.size(),(unsigned long long)geometryBytes,
            milliseconds(cpuStart,std::chrono::steady_clock::now()),milliseconds(retired,acquired),milliseconds(acquired,prepared),milliseconds(prepared,renderEncoded),milliseconds(renderEncoded,uiEncoded),milliseconds(uiEncoded,std::chrono::steady_clock::now()),milliseconds(submitStart,committed),milliseconds(committed,std::chrono::steady_clock::now()),milliseconds(cpuStart,retired));
        if (serial % 120 == 0) {
            const auto state = m_Observer->Snapshot();
            std::printf("M5_EDITOR_RESOURCES frame=%llu frame_bytes=%llu imgui_bytes=%llu attachment_bytes=%llu generations=%llu inflight=%llu pixels=%ux%u logical=%ux%u backing_scale=%.2f\n", (unsigned long long)serial,
                (unsigned long long)state.ResourceBytes,(unsigned long long)state.ImGuiResourceBytes,(unsigned long long)state.AttachmentBytes,(unsigned long long)state.AttachmentGenerations,(unsigned long long)state.InFlight,state.PixelWidth,state.PixelHeight,state.LogicalWidth,state.LogicalHeight,state.BackingScale);
        }
    }
    slot.Serial = serial; m_Serial = serial; m_Next = (m_Next + 1) % m_Frames.size();
}
}
