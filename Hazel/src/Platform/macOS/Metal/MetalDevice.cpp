#include "MetalDevice.h"
#include "MetalAmbientOcclusion.h"
#include "Hazel/ImGui/Backends/imgui_impl_metal4.h"
#include "MetalRenderer2DFrame.h"
#include "MetalShader.h"
#include "MetalFramebuffer.h"
#include "Hazel/Core/Log.h"
#include <algorithm>
#include <chrono>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <set>

namespace Hazel {
namespace {
MetalStartupFailure s_Failure=MetalStartupFailure::None;
constexpr size_t WordBytes=256, Side=8, RowBytes=256, ImageBytes=Side*RowBytes;
constexpr size_t TextureBase=2*WordBytes, ProofBytes=TextureBase+2*ImageBytes;
constexpr uint64_t ResourceLimit=1024ULL*1024*1024, AllocatorLimit=64*1024*1024;
NS::String* Label(const char* text) { return NS::String::string(text,NS::UTF8StringEncoding); }
uint32_t Pattern(uint64_t serial,unsigned version,size_t word) {
    return uint32_t(serial*2654435761ULL) ^ (0x9e3779b9u*(version+1)) ^ uint32_t(word*0x45d9f3bu);
}
void Require(bool value,const char* message) { if(!value) throw std::runtime_error(message); }
}
void MetalDevice::FailNextStartup(MetalStartupFailure point) { s_Failure=point; }
void MetalDevice::CheckStartupFailure(MetalStartupFailure point) {
    if(s_Failure==point) { s_Failure=MetalStartupFailure::None; throw std::runtime_error("Injected Metal startup failure"); }
}
std::string MetalDevice::AvailabilityReason() {
#if defined(__arm64__) || defined(__aarch64__)
    if (__builtin_available(macOS 27.0, *)) {} else return "Requires macOS 27 or newer";
    auto pool = NS::TransferPtr(NS::AutoreleasePool::alloc()->init());
    auto device = NS::TransferPtr(MTL::CreateSystemDefaultDevice());
    if (!device) return "No native Metal device found";
    if (!device->supportsFamily(MTL::GPUFamilyMetal4)) return "Device does not support Metal 4";
    return {};
#else
    return "Requires Apple Silicon";
#endif
}
MetalDevice::MetalDevice() {
    m_Trace = std::getenv("HAZEL_METAL_TRACE") != nullptr;
    auto pool=NS::TransferPtr(NS::AutoreleasePool::alloc()->init());
    m_Device=NS::TransferPtr(MTL::CreateSystemDefaultDevice());
    Require(bool(m_Device),"Metal device unavailable");
    Require(m_Device->supportsFamily(MTL::GPUFamilyMetal4),"Metal 4 is required; no classic Metal fallback");
    CheckStartupFailure(MetalStartupFailure::AfterDevice);
    m_Queue=NS::TransferPtr(m_Device->newMTL4CommandQueue());
    Require(bool(m_Queue),"Cannot create Metal 4 command queue");
    CheckStartupFailure(MetalStartupFailure::AfterQueue);
    m_Commands=NS::TransferPtr(m_Device->newCommandBuffer());
    m_Completed=NS::TransferPtr(m_Device->newSharedEvent());
    auto descriptor=NS::TransferPtr(MTL::ResidencySetDescriptor::alloc()->init());
    descriptor->setLabel(Label("Hazel M1 owned allocations")); descriptor->setInitialCapacity(12);
    NS::Error* error=nullptr;
    m_Residency=NS::TransferPtr(m_Device->newResidencySet(descriptor.get(),&error));
    Require(bool(m_Commands)&&bool(m_Completed)&&bool(m_Residency),"Cannot create Metal 4 frame infrastructure");
    m_Queue->addResidencySet(m_Residency.get());
    auto args=NS::TransferPtr(MTL4::ArgumentTableDescriptor::alloc()->init());
    args->setMaxBufferBindCount(2); args->setMaxTextureBindCount(1); args->setMaxSamplerStateBindCount(0);
    args->setInitializeBindings(true);
    for(auto& frame:m_Frames) {
        frame.Allocator=NS::TransferPtr(m_Device->newCommandAllocator());
        frame.Arguments=NS::TransferPtr(m_Device->newArgumentTable(args.get(),&error));
        Require(bool(frame.Allocator)&&bool(frame.Arguments),"Cannot allocate Metal 4 frame slot");
    }
    HZ_CORE_INFO("Metal 4 device: {}; one MTL4CommandQueue, three allocators; OpenGL not initialized",m_Device->name()->utf8String());
}
MetalDevice::~MetalDevice() {
    // Application drains while the surface and all resource owners are alive.
    // Also covers context-construction failures before Renderer::Init.
    Drain();
    m_Queue->removeResidencySet(m_Residency.get());
    m_Residency->removeAllAllocations(); m_Residency->commit();
    for(auto& f:m_Frames) { f=Frame{}; }
    m_DepthStates = {};
    m_Gate.reset(); m_Completed.reset(); m_Commands.reset(); m_Residency.reset(); m_Queue.reset(); m_Device.reset();
    std::lock_guard<std::mutex> lock(m_Observer->Mutex);
    m_Observer->Value.ResourceBytes=0; m_Observer->Value.AllocatorBytes=0;
    m_Observer->Value.ResidencyAllocations=0; m_Observer->Value.DeviceReleased=true;
}
void MetalDevice::SetClear(const glm::vec4& color) { m_Clear={color.r,color.g,color.b,color.a}; }
void MetalDevice::EnableTransferProof() {
    Require(!m_Serial&&!m_Transfers,"Enable transfer proof once before submitting frames");
    auto pool=NS::TransferPtr(NS::AutoreleasePool::alloc()->init());
    auto desc=NS::TransferPtr(MTL::TextureDescriptor::alloc()->init());
    desc->setTextureType(MTL::TextureType2D); desc->setPixelFormat(MTL::PixelFormatRGBA8Uint);
    desc->setWidth(Side); desc->setHeight(Side); desc->setStorageMode(MTL::StorageModePrivate);
    desc->setUsage(MTL::TextureUsageShaderRead); // blit access does not require shader-write usage
    for(auto& f:m_Frames) {
        f.Upload=NS::TransferPtr(m_Device->newBuffer(ProofBytes,MTL::ResourceStorageModeShared));
        f.Readback=NS::TransferPtr(m_Device->newBuffer(ProofBytes,MTL::ResourceStorageModeShared));
        f.Private=NS::TransferPtr(m_Device->newBuffer(WordBytes,MTL::ResourceStorageModePrivate));
        f.Texture=NS::TransferPtr(m_Device->newTexture(desc.get()));
        Require(bool(f.Upload)&&bool(f.Readback)&&bool(f.Private)&&bool(f.Texture),"M1 diagnostic allocation failed");
        f.Upload->setLabel(Label("M1 two immutable upload versions"));
        f.Private->setLabel(Label("M1 explicitly ordered private storage"));
        f.Readback->setLabel(Label("M1 completion-gated readback"));
        f.Texture->setLabel(Label("M1 frame-tagged offscreen texture"));
        for(auto* b:{f.Upload.get(),f.Private.get(),f.Readback.get()}) m_Residency->addAllocation(b);
        m_Residency->addAllocation(f.Texture.get());
        // Native Metal 4 namespaces: buffer 0 upload, buffer 1 readback, texture 0 diagnostic.
        // Per-slot table ownership; no global mutable table or classic encoder bindings.
        f.Arguments->setAddress(f.Upload->gpuAddress(),0);
        f.Arguments->setAddress(f.Readback->gpuAddress(),1);
        f.Arguments->setTexture(f.Texture->gpuResourceID(),0);
    }
    m_Residency->commit(); m_Transfers=true; Account();
}
void MetalDevice::Account(const MetalRenderer2DFrame* preparing) {
    uint64_t resources=0,allocators=0,quadAllocations=0;
    std::set<const MTL::Allocation*> allocations;
    std::vector<const MetalRenderer2DFrame*> scenes;
    for(auto& f:m_Frames) {
        for(auto* buffer:{f.Upload.get(),f.Private.get(),f.Readback.get()}) if(buffer) allocations.insert(buffer);
        for(const auto& buffer:f.Renderer2DBuffers) if(buffer) allocations.insert(buffer.get());
        if(f.Texture) allocations.insert(f.Texture.get());
        if(f.Allocator) allocators+=f.Allocator->allocatedSize();
        for (const auto& drawable : f.EditorDrawables) if (drawable) allocations.insert(drawable->texture());
        for (const auto& image : f.EditorTextures) if (image.Handle) allocations.insert(reinterpret_cast<MTL::Texture*>(image.Handle));
        for (const auto& editor : f.EditorFrames) scenes.push_back(editor.get());
        if(f.Quad) { scenes.push_back(f.Quad.get()); quadAllocations+=f.Quad->Residency->allocationCount(); }
    }
    if(preparing) scenes.push_back(preparing);
    while(!scenes.empty()) {
        const auto* scene=scenes.back();scenes.pop_back();
        scene->CollectAllocations(allocations);
    }
    m_AOGenerations.erase(std::remove_if(m_AOGenerations.begin(),m_AOGenerations.end(),[](const auto& weak){return weak.expired();}),m_AOGenerations.end());
    for(const auto& weak:m_AOGenerations)if(auto r=weak.lock())
        for(auto texture:{r->Depth,r->Raw,r->Edges,r->Scratch,r->Visibility})allocations.insert(texture.get());
    for(const auto* allocation:allocations) resources+=allocation->allocatedSize();
    const uint64_t uiBytes = m_Editor ? ImGui_ImplMetal4_ResourceBytes() : 0;
    Require(resources+uiBytes<=ResourceLimit&&allocators<=AllocatorLimit,"M1 bounded GPU storage limit exceeded");
    uint64_t attachmentBytes = 0, generations = 0;
    for (const auto& weak : m_AttachmentGenerations) if (auto generation = weak.lock()) {
        ++generations;
        for (const auto& target : generation->Targets) if (target) attachmentBytes += target->allocatedSize();
    }
    std::lock_guard<std::mutex> lock(m_Observer->Mutex); auto& s=m_Observer->Value;
    s.ImGuiResourceBytes=uiBytes; s.PeakImGuiResourceBytes=std::max(s.PeakImGuiResourceBytes,uiBytes);
    s.AttachmentBytes=attachmentBytes; s.AttachmentGenerations=generations;
    s.PeakAttachmentBytes=std::max(s.PeakAttachmentBytes,attachmentBytes);
    s.ResourceBytes=resources; s.AllocatorBytes=allocators; s.ResidencyAllocations=m_Residency->allocationCount()+quadAllocations;
    s.PeakResourceBytes=std::max(s.PeakResourceBytes,resources); s.PeakAllocatorBytes=std::max(s.PeakAllocatorBytes,allocators);
}
void MetalDevice::EncodeTransfers(Frame& f,uint64_t serial) {
    auto* words=static_cast<uint32_t*>(f.Upload->contents());
    std::memset(words,0,ProofBytes); std::memset(f.Readback->contents(),0xcd,ProofBytes);
    auto* encoder=m_Commands->computeCommandEncoder();
    Require(encoder!=nullptr,"Metal 4 transfer encoder unavailable");
    encoder->setLabel(Label("M1 Metal 4 transfer: write / barrier / read, twice"));
    encoder->setArgumentTable(f.Arguments.get());
    for(unsigned version=0;version<2;++version) {
        // The second CPU update occurs after encoding consumption of the first.
        // Separate slices preserve both values until the GPU completes this frame.
        for(size_t i=0;i<WordBytes/4;++i) words[version*WordBytes/4+i]=Pattern(serial,version,i);
        auto texOffset=TextureBase+version*ImageBytes;
        for(size_t y=0;y<Side;++y) for(size_t x=0;x<Side;++x)
            words[(texOffset+y*RowBytes)/4+x]=Pattern(serial,version,100+y*Side+x);
        encoder->copyFromBuffer(f.Upload.get(),version*WordBytes,f.Private.get(),0,WordBytes);
        encoder->copyFromBuffer(f.Upload.get(),texOffset,RowBytes,0,MTL::Size{Side,Side,1},f.Texture.get(),0,0,MTL::Origin{0,0,0});
        encoder->barrierAfterEncoderStages(MTL::StageBlit,MTL::StageBlit,MTL4::VisibilityOptionDevice);
        encoder->copyFromBuffer(f.Private.get(),0,f.Readback.get(),version*WordBytes,WordBytes);
        encoder->copyFromTexture(f.Texture.get(),0,0,MTL::Origin{0,0,0},MTL::Size{Side,Side,1},f.Readback.get(),texOffset,RowBytes,0);
        // Also covers previous-version reads before overwriting the same private storage.
        encoder->barrierAfterEncoderStages(MTL::StageBlit,MTL::StageBlit,MTL4::VisibilityOptionDevice);
    }
    encoder->endEncoding();
}
void MetalDevice::Verify(Frame& f) {
    bool valid=true;
    if(m_Transfers) {
        const auto* words=static_cast<const uint32_t*>(f.Readback->contents());
        for(unsigned v=0;v<2;++v) {
            for(size_t i=0;i<WordBytes/4;++i) valid &= words[v*WordBytes/4+i]==Pattern(f.Serial,v,i);
            for(size_t y=0;y<Side;++y) for(size_t x=0;x<Side;++x)
                valid &= words[(TextureBase+v*ImageBytes+y*RowBytes)/4+x]==Pattern(f.Serial,v,100+y*Side+x);
        }
    }
    std::lock_guard<std::mutex> lock(m_Observer->Mutex); auto& s=m_Observer->Value;
    ++s.Completed; --s.InFlight;
    if(m_Transfers&&valid) ++s.Verified;
    if(!valid) { ++s.Errors; s.Error="Frame-tagged Metal 4 transfer mismatch at frame "+std::to_string(f.Serial); }
    f.Completion->GPU=true; if(f.Completion->Presented) ++s.CompletedPresented;
    if(m_Transfers) std::printf("M1_GPU frame=%llu verified=%d\n",static_cast<unsigned long long>(f.Serial),int(valid));
}
void MetalDevice::Poll() {
    // Queue signalEvent is ordered AFTER all work in the corresponding commit.
    // Neither feedback delivery nor drawable acquisition alone permits CPU access.
    const uint64_t completed=m_Completed->signaledValue();
    for(auto& f:m_Frames) if(f.Pending&&completed>=f.Serial) {
        if (f.EditorFeedback && !f.EditorFeedback->load()) continue;
        if (f.EditorFeedback && f.EditorCounters && m_EditorTimestampFrequency) {
            auto pool = NS::TransferPtr(NS::AutoreleasePool::alloc()->init());
            // The slot completion event precedes all CPU counter access. Metal's
            // boundary timestamps may include later overlapping work; these are
            // completion intervals, never exclusive pass costs.
            auto counters = NS::RetainPtr(f.EditorCounters->resolveCounterRange(NS::Range{0,3}));
            if (counters && counters->length() == 3*sizeof(MTL4::TimestampHeapEntry)) {
                MTL4::TimestampHeapEntry entries[3]; std::memcpy(entries,counters->bytes(),sizeof(entries));
                std::printf("M5_EDITOR_GPU_PHASE frame=%llu frequency=%llu start=%llu scene_complete=%llu imgui_complete=%llu\n",
                    (unsigned long long)f.Serial,(unsigned long long)m_EditorTimestampFrequency,(unsigned long long)entries[0].timestamp,
                    (unsigned long long)entries[1].timestamp,(unsigned long long)entries[2].timestamp);
            }
        }
        if(f.Quad) {
            if(!f.Quad->Feedback->load()) continue;
            f.Quad->Complete();
            if (m_Trace) std::printf("M2C_GPU frame=%llu completed=1\n", (unsigned long long)f.Serial);
            f.Quad.reset(); // Only completed work releases shaders, versions and frame allocations.
            std::lock_guard<std::mutex> lock(m_Observer->Mutex);
            ++m_Observer->Value.Completed; --m_Observer->Value.InFlight;
            if (f.Completion) { f.Completion->GPU = true; if (f.Completion->Presented) ++m_Observer->Value.CompletedPresented; }
        } else Verify(f);
        for (const auto& editor : f.EditorFrames) editor->ResolveAmbientOcclusionTimings();
        for (const auto& editor : f.EditorFrames) if (editor->DisplaySource) editor->DisplaySource->Complete();
        if (f.EditorCapture) {
            for (size_t image = 0; image < f.EditorCapture->Images.size(); ++image) {
                auto& result = f.EditorCapture->Images[image];
                const size_t row = (size_t(result.Width)*4+255)&~size_t(255);
                const auto* bytes = static_cast<const uint8_t*>(f.EditorCaptureBuffers[image]->contents());
                for (size_t y=0; y<result.Height; ++y) std::memcpy(result.BGRA.data()+y*result.Width*4,bytes+y*row,size_t(result.Width)*4);
            }
            f.EditorCapture->Completed = true;
        }
        f.EditorCapture.reset(); f.EditorCaptureBuffers.clear();
        f.EditorFrames.clear(); f.EditorTextures.clear(); f.EditorDrawables.clear(); f.EditorLayers.clear(); f.EditorResidency.reset(); f.EditorFeedback.reset();
        f.Pending=false;
    }
    Account();
}
void MetalDevice::Present(CA::MetalLayer* layer,uint32_t w,uint32_t h,uint32_t lw,uint32_t lh,double scale,bool visible) {
    if(m_Stopped) return;
    if (m_Editor) {
        if (m_Override) { w=m_Width; h=m_Height; m_Override=false; layer->setDrawableSize(CGSize{double(w),double(h)}); }
        {
            std::lock_guard<std::mutex> lock(m_Observer->Mutex); auto& state=m_Observer->Value;
            state.PixelWidth=w; state.PixelHeight=h; state.LogicalWidth=lw; state.LogicalHeight=lh; state.BackingScale=scale; state.VSync=layer->displaySyncEnabled();
            if (!visible && m_Serial) ++state.SkippedHidden;
            if (state.Errors) throw std::runtime_error(state.Error);
        }
        PresentEditor(layer, visible); return;
    }
    const auto cpuStart=std::chrono::steady_clock::now();
    auto pool=NS::TransferPtr(NS::AutoreleasePool::alloc()->init());
    Poll();
    auto scene = std::move(m_PresentFrame);
    if(m_Override) { w=m_Width; h=m_Height; m_Override=false; layer->setDrawableSize(CGSize{double(w),double(h)}); }
    {
        std::lock_guard<std::mutex> lock(m_Observer->Mutex); auto& s=m_Observer->Value;
        s.PixelWidth=w; s.PixelHeight=h; s.LogicalWidth=lw; s.LogicalHeight=lh; s.BackingScale=scale;
        s.VSync=layer->displaySyncEnabled();
        if(s.Errors) throw std::runtime_error(s.Error);
        if(!w||!h) { ++s.SkippedZero; return; }
        if(!visible && m_Serial) { ++s.SkippedHidden; return; } // Submit the first frame before relying on Cocoa occlusion.
        if(m_Frames[m_Next].Pending) { ++s.SkippedBusy; return; } // no global CPU wait in normal frames
        ++s.Reservations;
    }
    auto drawable=NS::RetainPtr(m_SkipDrawable?nullptr:layer->nextDrawable()); m_SkipDrawable=false;
    if(!drawable) { std::lock_guard<std::mutex> lock(m_Observer->Mutex); ++m_Observer->Value.SkippedUnavailable; ++m_Observer->Value.AbortedReservations; return; }
    Require(drawable->texture()->width()==w&&drawable->texture()->height()==h,"CAMetalLayer drawable extent differs from framebuffer pixels");
    if (scene && (scene->Width != w || scene->Height != h)) {
        std::lock_guard<std::mutex> lock(m_Observer->Mutex); ++m_Observer->Value.AbortedReservations; return;
    }
    if (scene) {
        auto& slot=m_Frames[m_Next];
        slot.Renderer2DCursor=0;
        try { PrepareRenderer2DFrame(scene); slot.Renderer2DBuffers.resize(slot.Renderer2DCursor); Account(scene.get()); }
        catch (...) { slot.Renderer2DBuffers.clear(); Account(); throw; }
    }
    auto& f=m_Frames[m_Next];
    f.Allocator->reset(); // Poll proved the previous submission no longer uses this allocator/storage.
    const auto serial=m_Serial+1;
    m_Commands->beginCommandBuffer(f.Allocator.get());
    const auto label="Hazel Metal 4 frame "+std::to_string(serial);
    m_Commands->setLabel(Label(label.c_str()));
    if(m_Transfers) EncodeTransfers(f,serial);
    if (scene) {
        m_Commands->useResidencySet(scene->Residency.get());
        EncodeRenderer2DFrame(scene);
    }
    auto pass=NS::TransferPtr(MTL4::RenderPassDescriptor::alloc()->init());
    auto* color=pass->colorAttachments()->object(0);
    color->setTexture(drawable->texture()); color->setLoadAction(MTL::LoadActionClear);
    color->setStoreAction(MTL::StoreActionStore); color->setClearColor(m_Clear);
    auto* render=m_Commands->renderCommandEncoder(pass.get());
    Require(render!=nullptr,"Metal 4 presentation encoder unavailable");
    render->setLabel(Label(scene ? "Hazel DisplayOutput to BGRA8 drawable" : "M1 native CAMetalLayer clear/present"));
    if (scene) {
        render->barrierAfterQueueStages(MTL::StageAll, MTL::StageFragment, MTL4::VisibilityOptionDevice);
        render->setRenderPipelineState(scene->OutputShader->Pipeline());
        render->setViewport({0,0,double(w),double(h),0,1});
        render->setCullMode(MTL::CullModeNone);
        render->setArgumentTable(scene->OutputArguments.get(), MTL::RenderStageFragment);
        render->drawPrimitives(MTL::PrimitiveTypeTriangle, NS::UInteger(0), NS::UInteger(3));
    }
    render->endEncoding();
    m_Commands->endCommandBuffer();
    auto observer=m_Observer; auto receipt=std::make_shared<Receipt>();
    const bool proof=m_Transfers || m_Trace;
    drawable->addPresentedHandler(MTL::DrawablePresentedHandlerFunction([observer,receipt,serial,proof](MTL::Drawable* d) {
        const double time=d->presentedTime();
        std::lock_guard<std::mutex> lock(observer->Mutex); auto& s=observer->Value;
        ++s.PresentationCallbacks;
        if(time>0) { ++s.Presented; s.LastPresentedTime=time; receipt->Presented=true; if(receipt->GPU) ++s.CompletedPresented; }
        else ++s.ZeroPresentationTime;
        if(proof) std::printf("M1_PRESENT frame=%llu timestamp=%.9f\n",static_cast<unsigned long long>(serial),time);
    }));
    auto options=NS::TransferPtr(MTL4::CommitOptions::alloc()->init());
    options->addFeedbackHandler(MTL4::CommitFeedbackHandlerFunction([observer,serial,proof,scene](MTL4::CommitFeedback* feedback) {
        std::lock_guard<std::mutex> lock(observer->Mutex); ++observer->Value.Feedbacks;
        if(auto* error=feedback->error()) { ++observer->Value.Errors; observer->Value.Error=error->localizedDescription()->utf8String(); }
        if (scene) scene->Feedback->store(true);
        if(proof) std::printf("M1_FEEDBACK frame=%llu gpu_start=%.9f gpu_end=%.9f\n",static_cast<unsigned long long>(serial),feedback->GPUStartTime(),feedback->GPUEndTime());
        observer->Changed.notify_all();
    }));
    // Layer residency is supplied by MetalContext. Drawable synchronization is explicit in Metal 4.
    m_Queue->wait(drawable.get());
    const MTL4::CommandBuffer* buffers[]={m_Commands.get()};
    m_Queue->commit(buffers,1,options.get());
    if (scene) scene->MarkMipmapsSubmitted();
    m_Queue->signalDrawable(drawable.get());
    drawable->present();
    m_Queue->signalEvent(m_Completed.get(),serial);
    f.Completion=std::move(receipt); f.Quad=scene; f.Pending=true;
    {
        std::lock_guard<std::mutex> lock(m_Observer->Mutex); auto& s=m_Observer->Value;
        ++s.Submitted; ++s.InFlight; s.PeakInFlight=std::max(s.PeakInFlight,s.InFlight);
        if(f.Serial) ++s.SlotReuses;
    }
    f.Serial=serial; m_Serial=serial; m_Next=(m_Next+1)%m_Frames.size(); Account();
    if(proof) std::printf("M1_SUBMIT frame=%llu pixels=%ux%u cpu_ms=%.6f\n",static_cast<unsigned long long>(serial),w,h,
        std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-cpuStart).count());
    if(m_Trace && serial%120==0) {
        const auto state=m_Observer->Snapshot();
        std::printf("M3_RESOURCES frame=%llu frame_bytes=%llu attachment_bytes=%llu generations=%llu attachment_allocations=%llu attachment_reuses=%llu buffer_allocations=%llu buffer_reuses=%llu allocator_bytes=%llu inflight=%llu\n",
            (unsigned long long)serial,(unsigned long long)state.ResourceBytes,(unsigned long long)state.AttachmentBytes,
            (unsigned long long)state.AttachmentGenerations,(unsigned long long)state.AttachmentAllocations,
            (unsigned long long)state.AttachmentReuses,(unsigned long long)state.BufferAllocations,
            (unsigned long long)state.BufferReuses,(unsigned long long)state.AllocatorBytes,(unsigned long long)state.InFlight);
    }
}
void MetalDevice::Drain() {
    if(m_Stopped) return;
    auto pool=NS::TransferPtr(NS::AutoreleasePool::alloc()->init());
    { std::lock_guard<std::mutex> lock(m_Observer->Mutex); m_Observer->Value.PendingAtShutdown=m_Observer->Value.InFlight; }
    // A global drain is restricted to shutdown. Timeouts never authorize reuse.
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(10);
    if(m_Serial) while(!m_Completed->waitUntilSignaledValue(m_Serial,10)) {
        if(std::chrono::steady_clock::now()>=deadline) {
            HZ_CORE_CRITICAL("Metal 4 shutdown did not reach GPU completion; refusing to release in-use resources");
            std::abort();
        }
    }
    Poll(); StopCapture(); m_PresentFrame.reset(); m_OutputShader.reset(); m_OutlineShader.reset(); m_Stopped=true;
    std::unique_lock<std::mutex> lock(m_Observer->Mutex);
    if(!m_Observer->Changed.wait_for(lock,std::chrono::seconds(5),[&] { return m_Observer->Value.Feedbacks==m_Observer->Value.Submitted; })) {
        ++m_Observer->Value.Errors; m_Observer->Value.Error="Metal 4 commit feedback did not retire during shutdown";
    }
    lock.unlock(); Poll();
    for(auto& frame:m_Frames) frame.Renderer2DBuffers.clear();
    Account(); lock.lock();
    m_Observer->Value.ShutdownComplete=true;
    if (m_Trace) {
        const auto& s = m_Observer->Value;
        std::printf("METAL_FINAL submitted=%llu completed=%llu presented=%llu completed_presented=%llu errors=%llu peak_frame_bytes=%llu peak_attachment_bytes=%llu peak_allocator_bytes=%llu live_attachment_generations=%llu pending_bytes=%llu\n",
            (unsigned long long)s.Submitted, (unsigned long long)s.Completed, (unsigned long long)s.Presented,
            (unsigned long long)s.CompletedPresented, (unsigned long long)s.Errors, (unsigned long long)s.PeakResourceBytes,
            (unsigned long long)s.PeakAttachmentBytes, (unsigned long long)s.PeakAllocatorBytes,
            (unsigned long long)s.AttachmentGenerations, (unsigned long long)s.ResourceBytes);
    }
}
std::function<void()> MetalDevice::GateQueue() {
    Require(!m_Gate,"M1 queue gate is single-use");
    auto pool=NS::TransferPtr(NS::AutoreleasePool::alloc()->init());
    m_Gate=NS::TransferPtr(m_Device->newSharedEvent()); Require(bool(m_Gate),"Cannot allocate diagnostic queue gate");
    m_Queue->wait(m_Gate.get(),1);
    auto gate=m_Gate; return [gate] { gate->setSignaledValue(1); };
}
void MetalDevice::StartCapture(const std::filesystem::path& path) {
    auto pool=NS::TransferPtr(NS::AutoreleasePool::alloc()->init());
    auto* manager=MTL::CaptureManager::sharedCaptureManager();
    Require(manager->supportsDestination(MTL::CaptureDestinationGPUTraceDocument),"GPU trace capture unsupported; set MTL_CAPTURE_ENABLED=1 before launch");
    auto desc=NS::TransferPtr(MTL::CaptureDescriptor::alloc()->init());
    desc->setCaptureObject(m_Queue.get()); desc->setDestination(MTL::CaptureDestinationGPUTraceDocument);
    desc->setOutputURL(NS::URL::fileURLWithPath(Label(path.c_str())));
    NS::Error* error=nullptr;
    if(!manager->startCapture(desc.get(),&error)) throw std::runtime_error(error?error->localizedDescription()->utf8String():"Cannot start GPU capture");
    m_Capturing=true;
}
void MetalDevice::StopCapture() { if(m_Capturing) { MTL::CaptureManager::sharedCaptureManager()->stopCapture(); m_Capturing=false; } }
}
