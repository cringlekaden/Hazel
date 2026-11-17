#pragma once
#include "Hazel/ImGui/ImGuiLayer.h"
#include "Hazel/Core/Resources.h"
#include "Hazel/Renderer/Drawing/Renderer2D.h"
#include "Platform/macOS/Metal/MetalFramebuffer.h"
#include "Platform/macOS/Metal/MetalDevice.h"
#include <imgui.h>
#include <imgui_internal.h>
#include "Hazel/Core/Input.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <fstream>
#include <functional>
#include <glm/gtc/matrix_transform.hpp>

// Extends the existing native Application/lifecycle fixture. Rendering, UI,
// surfaces, submission and retirement are entirely production owned.
class EditorConsumers final : public Hazel::Layer {
public:
    Hazel::Ref<Hazel::Framebuffer> Source, Output;
    Hazel::Ref<Hazel::Texture2D> Nearest, Mipped;
    bool NearestReplaced = false;
    std::weak_ptr<const void> OldImageVersion;
    std::shared_ptr<Hazel::MetalProofObserver> Observer;
    std::function<void()> Release;
    Hazel::Ref<const Hazel::FramebufferReadback> RetirementResult;
    std::weak_ptr<const Hazel::MetalFramebufferAttachments> RetirementGeneration;
    struct Sample { uint32_t Viewport; int X,Y; std::array<int,3> BGR; };
    struct Capture { std::shared_ptr<Hazel::MetalEditorCapture> Result; std::vector<Sample> Samples; };
    std::vector<Capture> Captures;
    unsigned Frames = 0, FinalFrames = 900, CaptureStart = 30, GateAt = 0, CloseAt = 0;
    bool Sampling = false;
    bool PendingShutdown = false;
    bool AbortUI = false;
    unsigned NativePhase = 0;
    std::chrono::steady_clock::time_point NativeChanged;
    GLFWwindow* FocusHost = nullptr;
    uint64_t BeforeRestore = 0;
    void NativeTick() {
        using namespace Hazel;
        const auto now=std::chrono::steady_clock::now();
        auto* main=static_cast<GLFWwindow*>(Application::Get().GetWindow().GetNativeWindow());
        if(NativePhase==0 && Frames>=250) {
            auto* window=ImGui::FindWindowByName("Detached consumer 1");
            Check(window && window->Viewport && window->Viewport->ID!=ImGui::GetMainViewport()->ID,"Native secondary host missing");
            FocusHost=static_cast<GLFWwindow*>(window->Viewport->PlatformHandle);
            glfwIconifyWindow(FocusHost);NativeChanged=now;NativePhase=1;
        } else if(NativePhase==1 && now-NativeChanged>std::chrono::milliseconds(250)) {
            Check(glfwGetWindowAttrib(FocusHost,GLFW_ICONIFIED) && Observer->Snapshot().EditorHosts<4,"Native secondary minimize did not exclude its GPU consumer");
            glfwRestoreWindow(FocusHost);glfwFocusWindow(FocusHost);NativeChanged=now;NativePhase=2;
        } else if(NativePhase==2 && now-NativeChanged>std::chrono::milliseconds(100)) {
            Check(glfwGetWindowAttrib(FocusHost,GLFW_FOCUSED) && Input::NativeWindow()==FocusHost && Input::IsFocused(),"Detached native focus did not own editor polling");
            Check(MetalM1ClipboardRoundTrip(),"Detached native clipboard round trip failed");
            std::cout<<"PASS detached native UTF-8 clipboard round trip and restoration\n";
            Input::SetPointerCaptured(true);Check(Input::IsPointerCaptured(),"Detached pointer capture failed");Input::SetPointerCaptured(false);
            glfwFocusWindow(main);NativePhase=3;
        } else if(NativePhase==3 && Frames>=700) {
            glfwIconifyWindow(main);NativeChanged=now;NativePhase=4;
        } else if(NativePhase==4 && now-NativeChanged>std::chrono::milliseconds(250)) {
            Check(glfwGetWindowAttrib(main,GLFW_ICONIFIED) && Observer->Snapshot().SkippedHidden>0,"Native main minimize did not skip its surface");
            BeforeRestore=Observer->Snapshot().Presented;
            glfwRestoreWindow(main);glfwSetWindowPos(main,0,24);glfwSetWindowSize(main,960,400);glfwFocusWindow(main);NativeChanged=now;NativePhase=5;
        } else if(NativePhase==5 && now-NativeChanged>std::chrono::milliseconds(100)) {
            if(glfwGetWindowAttrib(main,GLFW_ICONIFIED) || Observer->Snapshot().Presented<=BeforeRestore) {
                Check(now-NativeChanged<std::chrono::seconds(2),"Native main restore did not resume positive presentation within the window-animation deadline");
                return;
            }
            NativePhase=6;std::cout<<"PASS native multiwindow minimize/restore, focus, clipboard, pointer capture and main resize\n";
        }
    }
    void SampleImage(float width,float height) {
        if (!Sampling) return;
        const auto* viewport = ImGui::GetWindowViewport();
        const auto top = ImGui::GetItemRectMin();
        const auto scale = Hazel::Application::Get().GetImGuiLayer()->CurrentWindowFramebufferScale();
        for (bool center : {false,true}) Captures.back().Samples.push_back({viewport->ID,
            int((top.x-viewport->Pos.x+width*(center?.5f:.1f))*scale.first),
            int((top.y-viewport->Pos.y+height*(center?.5f:.1f))*scale.second),
            center ? std::array<int,3>{99,137,255} : std::array<int,3>{188,137,99}});
        Captures.back().Samples.push_back({viewport->ID,
            int((top.x-viewport->Pos.x+width*.5f)*scale.first),int((top.y-viewport->Pos.y+height*.56f)*scale.second),{0,128,255}});
    }
    std::chrono::steady_clock::time_point Started = std::chrono::steady_clock::now();
    void TextureSamples() {
        ImGui::GetWindowDrawList()->AddCallback(ImDrawCallback_ResetRenderState,nullptr);
        ImGui::SameLine(); ImGui::BeginGroup();
        ImGui::Image(Hazel::ImGuiLayer::Texture(Nearest->GetEditorTexture()),{16,16});
        if (Sampling) {
            const auto* viewport=ImGui::GetWindowViewport(); const auto top=ImGui::GetItemRectMin();
            const auto scale=Hazel::Application::Get().GetImGuiLayer()->CurrentWindowFramebufferScale();
            Captures.back().Samples.push_back({viewport->ID,int((top.x-viewport->Pos.x+6.4f)*scale.first),int((top.y-viewport->Pos.y+3.2f)*scale.second),NearestReplaced?std::array<int,3>{255,0,0}:std::array<int,3>{0,0,255}});
        }
        ImGui::Image(Hazel::ImGuiLayer::Texture(Mipped->GetEditorTexture()),{2,2});
        if (Sampling) {
            const auto* viewport=ImGui::GetWindowViewport(); const auto top=ImGui::GetItemRectMin();
            const auto scale=Hazel::Application::Get().GetImGuiLayer()->CurrentWindowFramebufferScale();
            Captures.back().Samples.push_back({viewport->ID,int((top.x-viewport->Pos.x+1)*scale.first),int((top.y-viewport->Pos.y+1)*scale.second),{128,128,128}});
        }
        ImGui::EndGroup();
    }
    uint64_t SampledUIBytes = 0;
    uint64_t BeforeLateExtent = 0;
    bool ShowConsumers = true, ClosedPending = false, Draining = false;
    void OnAttach() override {
        using namespace Hazel;
        Observer = Backend().Observe(); Backend().EnableEditorReadbackForTesting();
        ImGui::GetIO().ConfigViewportsNoAutoMerge = true;
        FramebufferSpecification specification;
        specification.Width = 96; specification.Height = 64;
        specification.Attachments = {FramebufferTextureFormat::RGBA32F, FramebufferTextureFormat::RED_INTEGER, FramebufferTextureFormat::DEPTH32F};
        Source = Framebuffer::Create(specification);
        TextureSpecification image; image.Width=image.Height=2; image.GenerateMips=false; image.MinFilter=image.MagFilter=TextureFilter::Nearest;
        image.WrapS=image.WrapT=TextureWrap::ClampToEdge;
        Nearest=Texture2D::Create(image);
        const uint32_t pixels[]={0xff0000ff,0xff00ff00,0xffff0000,0xffffffff}; Nearest->SetData(pixels,sizeof(pixels));
        image.Width=image.Height=16; image.GenerateMips=true; image.MinFilter=TextureFilter::NearestMipmapNearest;
        Mipped=Texture2D::Create(image); std::array<uint32_t,256> checker;
        for(size_t y=0;y<16;++y) for(size_t x=0;x<16;++x) checker[y*16+x]=(x+y)%2?0xffffffff:0xff000000;
        Mipped->SetData(checker.data(),sizeof(checker));
        specification.Attachments = {FramebufferTextureFormat::RGBA8}; Output = Framebuffer::CreateEditor(specification);
        glfwSetWindowSize(static_cast<GLFWwindow*>(Application::Get().GetWindow().GetNativeWindow()),800,600);
    }
    void OnUpdate(Hazel::Timestep) override {
        using namespace Hazel;
        Check(std::chrono::steady_clock::now()-Started < std::chrono::seconds(30), "Editor consumer qualification timed out");
        ++Frames;
        if (Frames == 10) MetalM1Environment(static_cast<GLFWwindow*>(Application::Get().GetWindow().GetNativeWindow()), (Hazel::Resources::Get().UserData.parent_path()/"environment.json").c_str());
        if (!GateAt && Frames >= 30 && Observer->Snapshot().EditorHosts == 4) Draining = true;
        if (Draining && Observer->Snapshot().InFlight == 0) {
            // The diagnostic queue wait can block unfinished work already on the
            // queue. Stop producing and poll normally before inserting it.
            Release = Backend().GateQueueForTesting(); GateAt = Frames; Draining = false;
        }
        if (Draining) Backend().SkipNextDrawableForTesting();
        if (!NearestReplaced && !Captures.empty() && Captures.front().Result->Serial && Release) {
            OldImageVersion=Nearest->GetEditorTexture().Owner;
            const uint32_t replacement[]={0xffff0000,0xff00ff00,0xffff0000,0xffffffff};
            Nearest->SetData(replacement,sizeof(replacement)); NearestReplaced=true;
            Check(!OldImageVersion.expired(),"Pending ImGui texture version released during replacement");
        }
        const bool captured = FinalFrames == 100 || (Captures.size() >= 2 && Captures[1].Result->Serial != 0);
        if (GateAt && captured && !RetirementResult) {
            // A compositor may still own one drawable in each three-buffer pool.
            // Fill the third allocator with production offscreen work rather than
            // waiting for a third new drawable while GPU progress is blocked.
            FramebufferSpecification specification; specification.Width=specification.Height=8;
            specification.Attachments={FramebufferTextureFormat::RGBA32F,FramebufferTextureFormat::RED_INTEGER,FramebufferTextureFormat::DEPTH32F};
            auto target=Framebuffer::Create(specification);
            RetirementGeneration=std::dynamic_pointer_cast<MetalFramebuffer>(target)->Snapshot();
            target->Bind(); target->Clear({.125f,.25f,.5f,1}); target->ClearAttachment(1,-2000000003); target->Unbind();
            RetirementResult=target->Submit();
            Check(bool(RetirementResult), "Third production allocator was not available for offscreen retirement proof");
        }
        if (GateAt && Frames >= GateAt+4 && !CloseAt && captured) {
            Check(Observer->Snapshot().InFlight == 3, "Detached consumers did not hold all three production slots");
            Check(!RetirementGeneration.expired() && !RetirementResult->Completed, "Pending offscreen owner retired before release");
            if(PendingShutdown) { ClosedPending=true;std::cout<<"UI_PENDING_SHUTDOWN pending="<<Observer->Snapshot().InFlight<<'\n';Application::Get().Close();return; }
            ShowConsumers = false; ClosedPending = true; CloseAt = Frames;
        }
        if (CloseAt && Frames == CloseAt+2) {
            Check(ClosedPending && ImGui::GetPlatformIO().Viewports.Size <= 2, "Detached windows did not close while GPU work was pending");
            std::cout<<"UI_PENDING_CLOSE frame="<<Frames<<" pending="<<Observer->Snapshot().InFlight<<'\n';
            Release(); Release = {};
        }
        if (CloseAt && Frames == CloseAt+11) ShowConsumers = true;
        if (Frames == 120) Check(ClosedPending && !Release, "Editor consumer gate made no bounded progress");
        if (Frames == 160) BeforeLateExtent = Observer->Snapshot().SkippedUnavailable;
        // Model a Cocoa backing-size change after ImGui builds its draw data.
        // Admission may skip a busy slot, so retry without waiting for the GPU.
        if (Frames >= 160 && Frames < 180 && Observer->Snapshot().SkippedUnavailable == BeforeLateExtent)
            Backend().OverrideNextExtentForTesting(1,1);
        if (Frames == 180) Check(Observer->Snapshot().SkippedUnavailable > BeforeLateExtent, "Late drawable extent was not rejected");
        if (Frames == 200) { Source->Resize(127,83); Output->Resize(127,83); }
        if (Frames == 400) { Source->Resize(96,64); Output->Resize(96,64); }
        if (Frames == 600) SampledUIBytes = Observer->Snapshot().ImGuiResourceBytes;
        if (Frames >= FinalFrames && (FinalFrames==100 || (NativePhase==6 && Captures.size()==4 && Captures.back().Result->Completed))) {
            const auto state = Observer->Snapshot();
            Check(state.Errors == 0 && state.Completed > FinalFrames-100 && state.PeakInFlight == 3 && ClosedPending && !Release, "Editor consumer completion/validation failed");
            Check(state.ImGuiResourceBytes <= SampledUIBytes + 4*1024*1024, "Editor UI resource use did not stabilize");
            Check(RetirementResult && RetirementResult->Completed && RetirementResult->Error.empty() && RetirementGeneration.expired(), "Third-slot offscreen retirement failed");
            for (size_t pixel=0;pixel<RetirementResult->Entity.size();++pixel) Check(RetirementResult->Entity[pixel]==-2000000003 && RetirementResult->Depth[pixel]==1.f && RetirementResult->Color[pixel]==glm::vec4(.125f,.25f,.5f,1),"Third-slot offscreen data changed");
            Check(NearestReplaced && OldImageVersion.expired(),"Completed ImGui texture version did not retire");
            if(FinalFrames!=100)Check(NativePhase==6,"Native multiwindow lifecycle did not complete");
            Check(Captures.size()==(FinalFrames==100?3:4), "UI frame captures before and after native lifecycle were not requested");
            for (const auto& capture : Captures) {
                std::cout<<"UI_CAPTURE serial="<<capture.Result->Serial<<" completed="<<capture.Result->Completed<<" hosts="<<capture.Result->Images.size()<<'\n';
                Check(capture.Result->Completed && capture.Result->Images.size()==4, "Actual drawable captures did not complete for every host");
                for (const auto& image : capture.Result->Images) {
                    std::ofstream file(Resources::Get().UserData.parent_path()/("ui-"+std::to_string(capture.Result->Serial)+"-"+std::to_string(image.Viewport)+".ppm"),std::ios::binary);
                    file<<"P6\n"<<image.Width<<' '<<image.Height<<"\n255\n";
                    for (size_t pixel=0;pixel<image.BGRA.size();pixel+=4) { const char rgb[]={char(image.BGRA[pixel+2]),char(image.BGRA[pixel+1]),char(image.BGRA[pixel])}; file.write(rgb,3); }
                }
                for (const auto& sample : capture.Samples) {
                    const auto image = std::find_if(capture.Result->Images.begin(),capture.Result->Images.end(),[&](const auto& image){return image.Viewport==sample.Viewport;});
                    Check(image!=capture.Result->Images.end() && sample.X>=0 && sample.Y>=0 && uint32_t(sample.X)<image->Width && uint32_t(sample.Y)<image->Height,"UI sample outside actual drawable");
                    const auto pixel = (size_t(sample.Y)*image->Width+uint32_t(sample.X))*4;
                    for (size_t channel=0;channel<3;++channel) if (std::abs(int(image->BGRA[pixel+channel])-sample.BGR[channel])>1) {
                        std::cerr<<"UI_PIXEL serial="<<capture.Result->Serial<<" viewport="<<sample.Viewport<<" extent="<<image->Width<<'x'<<image->Height<<" pixel="<<sample.X<<','<<sample.Y<<" channel="<<channel<<" actual="<<int(image->BGRA[pixel+channel])<<" expected="<<sample.BGR[channel]<<'\n';
                        throw std::runtime_error("Detached UI texture or draw-buffer reuse corrupted a sampled pixel");
                    }
                }

            }
            Application::Get().Close();
        }
        Source->Bind(); Source->Clear({.125f,.25f,.5f,1}); Source->ClearAttachment(1,-2000000003);
        Renderer2D::BeginScene(glm::mat4(1));
        Renderer2D::DrawQuad(glm::translate(glm::mat4(1),glm::vec3(0,.25f,0))*glm::scale(glm::mat4(1),glm::vec3(.8f)),glm::vec4(1,.25f,.125f,1),2000000003);
        Renderer2D::EndScene(); Source->Unbind(); Source->EncodeOutput(Output);
        Source->DrawEntityOutlineTo(Output,2000000003,2,{1,.5f,0,1});
    }
    void OnImGuiRender() override {
        using namespace Hazel;
        Sampling = false;
        const bool requestCapture = FinalFrames == 100 ? Frames >= CaptureStart : GateAt != 0;
        const bool afterReopen = CloseAt && Frames >= CloseAt+16 && ShowConsumers && !Release && Observer->Snapshot().EditorHosts == 4;
        const size_t captureLimit = FinalFrames!=100 && NativePhase==6 && Frames>=850 ? 4 : (FinalFrames == 100 || afterReopen ? 3 : 2);
        if (requestCapture && Captures.size() < captureLimit && (Captures.empty() || Captures.back().Result->Serial != 0)) {
            Captures.push_back({Backend().CaptureEditorFrameForTesting(),{}}); Sampling = true;
        } else if (!Captures.empty() && Captures.back().Result->Serial == 0) {
            // Admission can skip CPU frames. Refresh the sample locations until
            // this exact request receives a production submission receipt.
            Captures.back().Samples.clear(); Sampling = true;
        }
        const auto main = ImGui::GetMainViewport();
        ImGui::SetNextWindowViewport(main->ID);
        const auto displays = Application::Get().GetWindow().GetDisplayAreas();
        const bool compact = !displays.empty() && displays.front().Width < 2000;
        ImGui::SetNextWindowPos({main->Pos.x+(compact?410.f:20.f),main->Pos.y+60},ImGuiCond_Always);
        ImGui::SetNextWindowSize({380,270},ImGuiCond_Always);
        ImGui::Begin("Main consumer");
        if(AbortUI)ImGui::GetWindowDrawList()->AddCallback([](const ImDrawList*,const ImDrawCmd*){throw std::runtime_error("M5 injected UI encoder abort");},nullptr);
        ImGuiLayer::Image(Output->GetEditorTexture(),320,200); SampleImage(320,200); TextureSamples(); ImGui::Text("Frame %u",Frames); ImGui::End();
        const ImGuiID dock=ImHashStr("M5 lifecycle dock");
        if(Frames>=480 && Frames<660) {
            ImGui::SetNextWindowViewport(main->ID);ImGui::SetNextWindowPos({main->Pos.x+10,main->Pos.y+40},ImGuiCond_Always);ImGui::SetNextWindowSize({600,440},ImGuiCond_Always);
            ImGui::Begin("M5 dock host",nullptr,ImGuiWindowFlags_NoSavedSettings);ImGui::DockSpace(dock);ImGui::End();
            if(Frames==500 || Frames==550 || Frames==600)ImGui::DockBuilderDockWindow("Detached consumer 0",dock);
            if(Frames==520 || Frames==570 || Frames==620)ImGui::DockBuilderDockWindow("Detached consumer 0",0);
        }
        if(Frames==510 || Frames==560 || Frames==610) {auto* window=ImGui::FindWindowByName("Detached consumer 0");Check(window && window->DockId && window->Viewport->ID==main->ID,"Repeated docking did not move the native consumer into main");}
        if(Frames==530 || Frames==580 || Frames==630) {auto* window=ImGui::FindWindowByName("Detached consumer 0");Check(window && !window->DockId && window->Viewport->ID!=main->ID,"Repeated undocking did not recreate native consumer");}
        if (ShowConsumers) for (unsigned host = 0; host < 3; ++host) {
            const bool docked=host==0 && ((Frames>=500&&Frames<520)||(Frames>=550&&Frames<570)||(Frames>=600&&Frames<620));
            if(!docked) {
            ImGui::SetNextWindowPos({compact?30.f+host*300.f:1500.f,compact?450.f:70.f+host*350.f},ImGuiCond_Always);
            ImGui::SetNextWindowSize(compact?ImVec2(260,200):ImVec2(400,300),ImGuiCond_Always);
            }
            const auto name = "Detached consumer " + std::to_string(host);
            ImGui::Begin(name.c_str());
            // Distinct geometry exercises reuse; actual drawable pixels are checked
            // after completion, with the existing one-level encoded bound.
            const float width=compact?200.f:280+host*15.f, height=compact?140.f:180+host*12.f;
            ImGuiLayer::Image(Output->GetEditorTexture(),width,height);
            SampleImage(width,height); TextureSamples();
            for (unsigned line = 0; line <= host; ++line) ImGui::Text("Host %u line %u frame %u",host,line,Frames);
            ImGui::End();
        }
    }
    void OnDetach() override {
        if (Release) { Release(); Release = {}; }
        Source.reset(); Output.reset(); Nearest.reset(); Mipped.reset();
    }
};
static int EditorConsumerProof(const std::filesystem::path& output, const std::filesystem::path& resources, unsigned frames = 900, bool pendingShutdown = false, bool abortUI = false) {
    using namespace Hazel;
    Log::Init();
    auto specification = Specification(output);
    specification.Name = "Hazel M5 editor consumers";
    specification.EnableImGui = true; specification.RendererMode = RendererInitialization::Drawing;
    specification.Resources.Root = resources;
    std::shared_ptr<MetalProofObserver> observer;
    bool aborted=false;
    {
        Application application(specification);
        auto layer = CreateScope<EditorConsumers>(); layer->FinalFrames = frames;layer->PendingShutdown=pendingShutdown;layer->AbortUI=abortUI;auto* lifecycle=layer.get(); if (frames == 100) layer->CaptureStart = 70;
        application.PushLayer(std::move(layer)); observer = Backend().Observe();
        application.SetBackgroundTick([lifecycle]{lifecycle->NativeTick();});
        try { application.Run(); }
        catch(const std::runtime_error& error){if(!abortUI || std::string(error.what())!="M5 injected UI encoder abort")throw;aborted=true;}
    }
    const auto final = observer->Snapshot();
    Check(final.Errors == 0 && final.InFlight == 0 && final.Completed == final.Submitted && final.ResourceBytes == 0 && final.ImGuiResourceBytes == 0 && final.AttachmentGenerations == 0,
        "Editor consumers did not retire on shutdown");
    if(abortUI){Check(aborted && !final.Submitted,"UI encoder abort unexpectedly committed work");std::cout<<"PASS native editor UI encoder abort: unsubmitted resources retired\n";return 0;}
    if(pendingShutdown){Check(final.Submitted>=3 && final.PeakInFlight==3,"Pending shutdown did not exercise three slots");std::cout<<"PASS native editor pending shutdown: three gated production slots, all consumers/resources retired\n";return 0;}
    if (frames == 100) { std::cout << "PASS quick actual drawable pixel diagnostic (100 frames); extended lifecycle gates excluded\n"; return 0; }
    std::cout << "PASS production editor consumers: four hosts, shared producer, three gated slots, pending-window close/reopen, resize, docking, native minimize/restore/focus, 80 actual drawable sample checks and shutdown retirement\n";
    return 0;
}
