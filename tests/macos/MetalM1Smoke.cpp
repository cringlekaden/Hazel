#include "Hazel/Core/Application.h"
#include "Hazel/Core/Log.h"
#include "Hazel/Core/Input.h"
#include "Hazel/Project/Project.h"
#include "Hazel/Scene/RuntimeSession.h"
#include "Hazel/Scene/Scene.h"
#include "Hazel/Scene/Entity.h"
#include <cmath>
#include "Hazel/Events/ApplicationEvent.h"
#include "Platform/macOS/Metal/MetalRendererAPI.h"
#include <GLFW/glfw3.h>
#include <glad/glad.h>
#include <chrono>
#include <thread>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace Hazel;
using Clock=std::chrono::steady_clock;
void MetalM1Environment(GLFWwindow* window,const char* output);
static void Check(bool value,const char* text) { if(!value) throw std::runtime_error(text); }
static MetalRendererAPI& Backend() { return static_cast<MetalRendererAPI&>(RenderCommand::Backend()); }
static ApplicationSpecification Specification(const std::filesystem::path& output) {
    ApplicationSpecification s; s.Name="Hazel Metal 4 M1"; s.EnableImGui=false;
    s.GraphicsAPI=RendererAPI::API::Metal; s.RendererMode=RendererInitialization::PresentationOnly;
    s.Resources.Root=std::filesystem::absolute("Hazel/Resources"); s.Resources.UserData=output/"profile";
    return s;
}
class EventProof final : public Layer {
public:
    unsigned Resizes=0;
    Ref<Project> QuadProject;
    RuntimeSession Session;
    Ref<Framebuffer> Display, Retirement;
    uint64_t Frames=0;
    void OnAttach() override {
        if (!QuadProject) return;
        Project::SetActive(QuadProject);
        Session.Resize(1280,720);
        Session.Start(QuadProject, QuadProject->LoadScene(QuadProject->GetConfig().StartScene));
    }
    void OnDetach() override { Session.Stop(); Display.reset(); Retirement.reset(); if(QuadProject) Project::SetActive(nullptr); }
    void OnUpdate(Timestep dt) override {
        if (!QuadProject) return;
        auto& window=Application::Get().GetWindow(); auto size=window.GetFramebufferSize();
        if (!size.first || !size.second) return;
        Session.Resize(window.GetWidth(),window.GetHeight()); Session.ResizeFramebuffer(size.first,size.second);
        Session.SetInput(Input::GetMousePosition(),window.IsFocused());
        if (!Display) {
            FramebufferSpecification spec; spec.Width=size.first;spec.Height=size.second;
            spec.Attachments={FramebufferTextureFormat::RGBA32F,FramebufferTextureFormat::RED_INTEGER,FramebufferTextureFormat::DEPTH32F};
            Display=Framebuffer::Create(spec);
        } else Display->Resize(size.first,size.second);
        auto camera=Session.GetScene()->GetPrimaryCameraEntity();
        camera.GetComponent<TransformComponent>().Translation.x=.35f*std::sin(float(Frames++)*.03f);
        Display->Bind();Display->Clear(Session.GetScene()->Presentation().Background);
        Session.SetRenderTarget(Display);Session.Update(dt);
        Display->Unbind();Display->EncodeOutput();
    }
    void OnEvent(Event& event) override { if(event.GetEventType()==EventType::WindowResize) ++Resizes;
        if(QuadProject)Session.SubmitUIEvent(event,Input::GetMousePosition(),Application::Get().GetWindow().IsFocused()); }
};
static void Print(const char* name,const MetalProofSnapshot& s) {
    std::cout<<name<<" submitted="<<s.Submitted<<" completed="<<s.Completed<<" verified="<<s.Verified
      <<" presented="<<s.Presented<<" completed_presented="<<s.CompletedPresented<<" feedbacks="<<s.Feedbacks
      <<" inflight="<<s.InFlight<<" peak_inflight="<<s.PeakInFlight<<" reuse="<<s.SlotReuses
      <<" bytes="<<s.ResourceBytes<<" peak_bytes="<<s.PeakResourceBytes<<" allocator_bytes="<<s.AllocatorBytes
      <<" peak_allocator_bytes="<<s.PeakAllocatorBytes<<" aborted="<<s.AbortedReservations
      <<" zero="<<s.SkippedZero<<" unavailable="<<s.SkippedUnavailable<<" hidden="<<s.SkippedHidden
      <<" pending_shutdown="<<s.PendingAtShutdown<<" shutdown_complete="<<s.ShutdownComplete
      <<" surface_detached="<<s.SurfaceDetached<<" device_released="<<s.DeviceReleased
      <<" residency_allocations="<<s.ResidencyAllocations<<" present_callbacks="<<s.PresentationCallbacks
      <<" zero_present_time="<<s.ZeroPresentationTime<<" pixels="<<s.PixelWidth<<'x'<<s.PixelHeight<<" logical="<<s.LogicalWidth<<'x'<<s.LogicalHeight
      <<" backing_scale="<<s.BackingScale<<" vsync="<<s.VSync<<" errors="<<s.Errors<<std::endl;
}
int main(int argc,char** argv) {
    try {
        const bool runtime=argc==4 && std::string(argv[2])=="--renderer2d";
        const bool quads=runtime || (argc==4 && std::string(argv[2])=="--quads");
        Check(argc==2||argc==3||quads,"Usage: MetalM1Smoke <private-output-directory> [capture.gputrace]");
        Log::Init(); auto output=std::filesystem::absolute(argv[1]); std::filesystem::create_directories(output);
        auto spec=Specification(output);
        if (quads) { spec.RendererMode=RendererInitialization::Drawing; spec.Resources.Root=std::filesystem::absolute(argv[3]); }
        Check(!RenderCommand::IsConfigured()&&glad_glGetString==nullptr,"Implicit OpenGL initialization before Metal startup");
        for(auto point:{MetalStartupFailure::AfterDevice,MetalStartupFailure::AfterQueue,MetalStartupFailure::AfterSurface}) {
            MetalRendererAPI::FailNextStartupForTesting(point);
            bool failed=false; try { Application rejected(spec); } catch(const std::runtime_error& e) { failed=std::string(e.what())=="Injected Metal startup failure"; }
            Check(failed&&!Application::TryGet()&&!RenderCommand::IsConfigured(),"Injected startup failure did not cleanly unwind");
            std::shared_ptr<MetalProofObserver> observer;
            { Application recovery(spec); observer=Backend().Observe(); }
            Check(observer->Snapshot().DeviceReleased&&observer->Snapshot().SurfaceDetached,"Recovery startup did not retire native owners");
        }
        std::cout<<"PASS: device/queue/attached-surface startup failures each followed by successful Application recovery\n";
        std::shared_ptr<MetalProofObserver> observer;
        std::thread releaseGate;
        bool minimized=false,restored=false,skip=false,zero=false,scaled=false,scaleRecovered=false,vsyncOff=false,vsyncOn=false;
        unsigned events=0; uint64_t heldSubmission=0,scaledSubmission=0; bool gated=false,captureStarted=false,captureStopped=false;
        auto start=Clock::now(), phase=start, lastReport=start;
        try {
            Application application(spec); observer=Backend().Observe(); if(!quads) Backend().EnableTransferProof();
            if(argc==3) { Backend().StartCapture(std::filesystem::absolute(argv[2])); captureStarted=true; }
            auto* window=static_cast<GLFWwindow*>(application.GetWindow().GetNativeWindow());
            MetalM1Environment(window,(output/"environment.json").c_str());
            int initialWidth=0,initialHeight=0; glfwGetWindowSize(window,&initialWidth,&initialHeight);
            Check(glfwGetWindowAttrib(window,GLFW_CLIENT_API)==GLFW_NO_API&&glfwGetCurrentContext()==nullptr&&glad_glGetString==nullptr,
                  "Metal startup created or loaded OpenGL");
            auto eventLayer=CreateScope<EventProof>(); auto* eventProof=eventLayer.get();
            if(quads) eventLayer->QuadProject=Project::LoadCandidate(std::filesystem::absolute(runtime ? "examples/Sandbox/Sandbox-Renderer2D.hproj" : "examples/Sandbox/Sandbox-Quads.hproj"));
            application.PushLayer(std::move(eventLayer));
            bool immutable=false; try { RenderCommand::Configure(RendererAPI::API::OpenGL); } catch(const std::logic_error&) { immutable=true; }
            Check(immutable&&Renderer::GetAPI()==RendererAPI::API::Metal,"Backend changed during a live graphics lifetime");
            application.SetBackgroundTick([&] {
                auto s=observer->Snapshot(); const auto now=Clock::now();
                if(now-lastReport>std::chrono::seconds(2)) { Print("M1_PROGRESS",s); lastReport=now; }
                Check(now-start<std::chrono::seconds(30),"M1 native presentation timed out");
                Check(s.Errors==0,"Metal 4 reported a GPU/transfer error");
                RenderCommand::SetClearColor({float((s.Submitted%97)+1)/100,.18f,.3f,1}); RenderCommand::Clear();
                if(s.Submitted>=30&&!skip) { Backend().SkipNextDrawableForTesting(); skip=true; }
                if(s.Submitted>=40&&!zero) { Backend().OverrideNextExtentForTesting(0,0); zero=true; }
                if(!quads&&s.Submitted>=50&&!scaled) { Backend().OverrideNextExtentForTesting(2*initialWidth,2*initialHeight); scaledSubmission=s.Submitted; scaled=true; }
                if(scaled&&s.Submitted>scaledSubmission&&s.PixelWidth==unsigned(2*initialWidth)&&s.PixelHeight==unsigned(2*initialHeight)) {
                    Check(s.LogicalWidth==unsigned(initialWidth)&&s.LogicalHeight==unsigned(initialHeight),"Physical-pixel diagnostic changed logical window dimensions"); scaleRecovered=true;
                }
                if(s.Submitted>=60&&!minimized) { glfwIconifyWindow(window); minimized=true; phase=now; }
                if(minimized&&!restored&&now-phase>std::chrono::milliseconds(250)) {
                    Check(glfwGetWindowAttrib(window,GLFW_ICONIFIED),"GLFW window did not minimize");
                    Check(s.SkippedHidden>0,"Minimized surface was not skipped");
                    glfwRestoreWindow(window); glfwSetWindowSize(window,960,540); restored=true;
                }
                if(restored&&s.Submitted>=90&&!vsyncOff) {
                    Check(s.PixelWidth==unsigned(960*s.BackingScale)&&s.PixelHeight==unsigned(540*s.BackingScale),"Resized native framebuffer extent mismatch");
                    application.GetWindow().SetVSync(false); vsyncOff=true;
                }
                if(vsyncOff&&s.Submitted>=110&&!vsyncOn) {
                    Check(!s.VSync&&!application.GetWindow().IsVSync(),"Metal VSync-off policy lost");
                    application.GetWindow().SetVSync(true); vsyncOn=true;
                }
                if(captureStarted&&s.Submitted>=4&&!captureStopped) { Backend().StopCapture(); captureStopped=true; }

                if(s.CompletedPresented>=320&&!gated) {
                    Check((quads || s.Verified>=320)&&s.SkippedZero&&s.SkippedUnavailable&&(quads || scaleRecovered)&&restored&&vsyncOn&&s.VSync,
                          "Presentation count reached before window/lifetime exercises passed");
                    auto release=Backend().GateQueueForTesting();
                    releaseGate=std::thread([release,quads] { std::this_thread::sleep_for(std::chrono::milliseconds(quads ? 1000 : 250)); release(); });
                    heldSubmission=s.Submitted; gated=true;
                }
                // Two pending drawables can exhaust CAMetalLayer's acquisition pool while the queue is gated.
                // An earlier GPU-completed slot may still await feedback; otherwise fill the third slot with offscreen work.
                if (quads && gated && s.Submitted>=heldSubmission+2 && !eventProof->Retirement) {
                    FramebufferSpecification size;size.Width=size.Height=8;
                    size.Attachments={FramebufferTextureFormat::RGBA32F,FramebufferTextureFormat::RED_INTEGER,FramebufferTextureFormat::DEPTH32F};
                    eventProof->Retirement=Framebuffer::Create(size);eventProof->Retirement->Bind();eventProof->Retirement->Unbind();
                    auto accepted = eventProof->Retirement->Submit();
                    Check(bool(accepted) || observer->Snapshot().InFlight==3, "Third shared M1 slot is neither busy nor accepted");
                    if (!accepted) eventProof->Retirement->Cancel();
                    Check(observer->Snapshot().InFlight==3,"Three gated scene/offscreen slots were not retained");
                    events=eventProof->Resizes;application.GetWindow().RequestClose();
                }
                if(!quads&&gated&&s.Submitted>=heldSubmission+3) { Check(s.InFlight==3,"Three pending frame slots were not held behind the diagnostic event"); events=eventProof->Resizes; application.GetWindow().RequestClose(); }
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
            });
            application.Run();
        } catch(...) { if(observer) Print("M1_FAILURE",observer->Snapshot()); if(releaseGate.joinable()) releaseGate.join(); throw; }
        if(releaseGate.joinable()) releaseGate.join();
        auto final=observer->Snapshot(); Print("M1_FINAL",final);
        Check(final.CompletedPresented>=300&&final.Submitted==final.Completed&&(quads || final.Completed==final.Verified),
              "Missing completed/presented/verified Metal 4 frames");
        Check(final.Feedbacks==final.Submitted&&final.Errors==0,"Metal 4 commit feedback errors or missing feedback");
        Check(final.InFlight==0&&final.PeakInFlight==3&&final.SlotReuses>=300&&final.PendingAtShutdown==3,
              "Frame reuse or pending-work shutdown not proven");
        Check(final.ResourceBytes==0&&final.AllocatorBytes==0&&final.ResidencyAllocations==0&&final.ShutdownComplete&&final.SurfaceDetached&&final.DeviceReleased,
              "GPU resources or native owners survived Application shutdown");
        Check(events>0&&!Application::TryGet()&&!RenderCommand::IsConfigured(),"GLFW events or application ownership contract failed");
        Check(argc!=3||(captureStarted&&captureStopped&&std::filesystem::exists(argv[2])),"GPU capture missing");
        if (quads) Check(final.AttachmentGenerations==0 && final.AttachmentBytes==0 && final.PeakAttachmentBytes>0 && final.AttachmentReuses>300 && final.AttachmentAllocations<=16,
                         "Scene framebuffer generations survived shutdown");
        // The same process must return to the default production OpenGL path after Metal.
        { ApplicationSpecification gl; gl.Name="M1 OpenGL recovery"; gl.EnableImGui=false;
          gl.Resources.Root=std::filesystem::absolute("Hazel/Resources"); gl.Resources.UserData=output/"gl-profile"; Application application(gl);
          Check(Renderer::GetAPI()==RendererAPI::API::OpenGL&&glfwGetCurrentContext()!=nullptr,"OpenGL default/context not restored");
          application.GetWindow().OnUpdate(); }
        std::ofstream report(output/"summary.txt");
        report<<(quads ? "PASS Metal 4 integrated quad scene\nsubmitted=" : "PASS Metal 4 M1\nsubmitted=")<<final.Submitted<<"\ncompleted="<<final.Completed<<"\nverified="<<final.Verified
          <<"\npresented="<<final.Presented<<"\ncompleted_presented="<<final.CompletedPresented<<"\npeak_inflight="<<final.PeakInFlight
          <<"\nslot_reuses="<<final.SlotReuses<<"\npeak_resource_bytes="<<final.PeakResourceBytes<<"\npeak_allocator_bytes="<<final.PeakAllocatorBytes
          <<"\npeak_attachment_bytes="<<final.PeakAttachmentBytes<<"\nattachment_allocations="<<final.AttachmentAllocations<<"\nattachment_reuses="<<final.AttachmentReuses
          <<"\npending_at_shutdown="<<final.PendingAtShutdown<<"\nresize_events="<<events<<"\nerrors="<<final.Errors
          <<(quads ? "\nIntegrated scene used actual native framebuffer dimensions; backing scale was " : "\nNote: 2x drawable extent is explicitly injected; physical display backing scale was ")<<final.BackingScale<<".\n";
        std::cout<<(quads ? "PASS: integrated Scene quad/world/output presentation, bounded reuse, window recovery and pending-work shutdown; OpenGL default restored\n" : "PASS: native Metal 4 queue/allocator/render/transfer/barrier/residency/event execution, real presentation timestamps, bounded reuse, recovery and pending-work shutdown; OpenGL default restored\n");
        return 0;
    } catch(const std::exception& e) { std::cerr<<"FAIL: "<<e.what()<<'\n'; return 1; }
}
