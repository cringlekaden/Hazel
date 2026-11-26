#pragma once
#include "Hazel/Renderer/Scene/SceneRenderer.h"
#include "Hazel/Renderer/Drawing/Renderer3D.h"
#include "Hazel/Assets/ProjectAssets.h"
#include "Hazel/Renderer/Cameras/NavigationCamera.h"
#include "Hazel/Renderer/Drawing/Renderer2D.h"
#include "Hazel/Scene/Scene.h"
#include "Platform/macOS/Metal/MetalAmbientOcclusion.h"
class TemporalEditorViews final : public Hazel::Layer {
public:
    std::array<Hazel::SceneRenderView,2> Views;
    std::array<Hazel::Ref<Hazel::Framebuffer>,2> Sources,Outputs;
    Hazel::SceneSubmission Frame;
    unsigned Frames=0,Verified=0;
    bool LocalLighting=false;
    uint64_t StableBytes=0;
    std::shared_ptr<Hazel::MetalEditorCapture> Capture;
    struct Point {uint32_t Viewport;int X,Y;};std::array<Point,2> Points{};
    std::filesystem::path Output;
    void OnAttach() override {
        using namespace Hazel;
        auto project=Project::LoadCandidate(std::filesystem::absolute("examples/RenderingReference/RenderingReference-Direct-PerspectivePBR.hproj"));
        Frame.Assets=CreateRef<ProjectAssets>(project->GetAssetRoot());auto mesh=Frame.Assets->Mesh("Native/Direct_v1/UnitCube.hmesh");Check(bool(mesh.Accepted),"Temporal multiview mesh missing");
        auto material=CreateRef<MaterialDefinition>();material->Model=LocalLighting?MaterialModel::MetallicRoughness:MaterialModel::Unlit;material->BaseColor=LocalLighting?glm::vec4(.5f,.5f,.5f,1):glm::vec4(.7f,.3f,.05f,1);
        SubmittedMesh draw;draw.Geometry=mesh.Accepted;draw.Materials={material};draw.Picking=123456789;draw.Identity=1;Frame.Meshes={draw};Frame.SceneIdentity=99;
        Frame.Presentation.SceneReferred=true;Frame.Presentation.TemporalAA=true;
        FramebufferSpecification spec;auto& window=Application::Get().GetWindow();auto pixels=window.GetFramebufferSize();
        const float scale=float(pixels.first)/float(window.GetWidth());spec.Width=uint32_t(320*(LocalLighting?1.f:scale));spec.Height=uint32_t(180*(LocalLighting?1.f:scale));spec.Attachments={FramebufferTextureFormat::RGBA32F,FramebufferTextureFormat::RED_INTEGER,FramebufferTextureFormat::DEPTH32F};
        for(unsigned i=0;i<2;++i){Sources[i]=Framebuffer::Create(spec);auto encoded=spec;encoded.Attachments={FramebufferTextureFormat::RGBA8};Outputs[i]=Framebuffer::CreateEditor(encoded);}
        Backend().EnableEditorReadbackForTesting();
        auto* native=static_cast<GLFWwindow*>(Application::Get().GetWindow().GetNativeWindow());glfwSetWindowPos(native,0,24);glfwSetWindowSize(native,800,400);
    }
    void OnUpdate(Hazel::Timestep) override {
        using namespace Hazel;++Frames;
        if(Capture&&Capture->Completed&&!Verified) {
            std::cout<<"TAA_VIEWS_CAPTURE serial="<<Capture->Serial<<"\n";
            for(const auto &image:Capture->Images){std::ofstream file(Output/("drawable-"+std::to_string(image.Viewport)+".ppm"),std::ios::binary);file<<"P6\n"<<image.Width<<' '<<image.Height<<"\n255\n";
                for(size_t p=0;p<image.BGRA.size();p+=4){char rgb[]={char(image.BGRA[p+2]),char(image.BGRA[p+1]),char(image.BGRA[p])};file.write(rgb,3);}}
            for(unsigned i=0;i<2;++i) {
                auto image=std::find_if(Capture->Images.begin(),Capture->Images.end(),[&](const auto& image){return image.Viewport==Points[i].Viewport;});
                Check(image!=Capture->Images.end()&&Points[i].X>=0&&Points[i].Y>=0&&unsigned(Points[i].X)<image->Width&&unsigned(Points[i].Y)<image->Height,"Temporal view drawable sample missing");
                size_t pixel=(size_t(Points[i].Y)*image->Width+unsigned(Points[i].X))*4;
                std::cout<<"VIEW_SAMPLE i="<<i<<" xy="<<Points[i].X<<','<<Points[i].Y<<" bgr="<<unsigned(image->BGRA[pixel])<<','<<unsigned(image->BGRA[pixel+1])<<','<<unsigned(image->BGRA[pixel+2])<<"\n";
                Check(i==0?image->BGRA[pixel]>image->BGRA[pixel+2]+10:image->BGRA[pixel+2]>image->BGRA[pixel]+10,"Independent temporal view drawable colors mixed");++Verified;
            }
            const auto& image=*std::find_if(Capture->Images.begin(),Capture->Images.end(),[&](const auto& image){return image.Viewport==Points[0].Viewport;});std::ofstream file(Output/"two-temporal-views.ppm",std::ios::binary);file<<"P6\n"<<image.Width<<' '<<image.Height<<"\n255\n";
            for(size_t p=0;p<image.BGRA.size();p+=4){char rgb[]={char(image.BGRA[p+2]),char(image.BGRA[p+1]),char(image.BGRA[p])};file.write(rgb,3);}
        }
        if(LocalLighting && Frames==100)for(unsigned i=0;i<2;++i){Sources[i]->Resize(640,360);Outputs[i]->Resize(640,360);}
        for(unsigned i=0;i<2;++i) {
            if(LocalLighting) {
                Frame.LocalLightTileSize=i?16:32;
                Frame.LocalLights.resize(128);
                for(size_t l=0;l<Frame.LocalLights.size();++l){auto &light=Frame.LocalLights[l];
                    light.Identity=l+1;light.Position={float(int(l%7)-3)*.2f+std::sin(float(Frames)*.02f),float(int(l%11)-5)*.1f,1.5f};
                    light.Range=l%2?.5f:4;light.Incident=i?glm::vec3(.02f,.01f,.005f):glm::vec3(.005f,.01f,.02f);
                }
                Frame.LocalMetrics=Views[i].LocalMetrics;
            }
            float direction=i?1.f:-1.f;
            Frame.Camera.Position={direction*.2f*std::sin(float(Frames)*.02f),0,4};Frame.Camera.ToCamera={0,0,1};
            Frame.Camera.ViewProjection=glm::perspective(glm::radians(45.f),320.f/180.f,.1f,20.f)*glm::lookAt(Frame.Camera.Position,glm::vec3(0),glm::vec3(0,1,0));
            Frame.Meshes[0].SetWorld(glm::rotate(glm::mat4(1),direction*float(Frames)*.004f,glm::vec3(0,1,0)));
            Frame.Presentation.Background=i?glm::vec4(.08f,.02f,.01f,1):glm::vec4(.015f,.03f,.06f,1);
            Sources[i]->Bind();Sources[i]->Clear(Frame.Presentation.Background);SceneRenderer::Render(Frame,Views[i],Sources[i],[]{},[]{});Sources[i]->Unbind();Sources[i]->EncodeOutput(Outputs[i]);
        }
        Check(Views[0].Temporal.Submitted!=Views[1].Temporal.Submitted,"Active temporal views share submission state");
        if(LocalLighting && Frames==180)StableBytes=Backend().Observe()->Snapshot().ResourceBytes;
        if(LocalLighting && Frames==500)Check(Backend().Observe()->Snapshot().ResourceBytes<=StableBytes+65536,"Local multiview resources grew after warmup");
        if(LocalLighting && Frames==200)for(auto &view:Views)Check(view.LocalMetrics->Valid && view.LocalMetrics->Count==128,"Independent local light metrics did not complete");
        if(Frames==200)for(auto& view:Views)Check(view.TemporalMetrics->Valid&&view.TemporalMetrics->Reuses>10,"Active view did not resolve/reuse its own history");
        if(Frames>=(LocalLighting?600u:240u)&&Verified==2)Application::Get().GetWindow().RequestClose();
    }
    void OnImGuiRender() override {
        using namespace Hazel;
        if(Frames==90)Capture=Backend().CaptureEditorFrameForTesting();
        ImGui::SetNextWindowViewport(ImGui::GetMainViewport()->ID);
        ImGui::SetNextWindowPos({20,40},ImGuiCond_Always);ImGui::SetNextWindowSize({740,300},ImGuiCond_Always);
        ImGui::Begin("Two independent temporal views");
        for(unsigned i=0;i<2;++i) {
            if(i)ImGui::SameLine();ImGuiLayer::Image(Outputs[i]->GetEditorTexture(),320,180);
            if(Capture&&!Capture->Serial) {
                auto top=ImGui::GetItemRectMin();auto* viewport=ImGui::GetWindowViewport();auto scale=Application::Get().GetImGuiLayer()->CurrentWindowFramebufferScale();
                Points[i]={viewport->ID,int((top.x-viewport->Pos.x+(LocalLighting?160:32))*scale.first),int((top.y-viewport->Pos.y+(LocalLighting?90:18))*scale.second)};
            }
        }
        ImGui::Text("Independent camera/object motion; two view-owned histories; frame %u",Frames);ImGui::End();
    }
    void OnDetach() override {for(auto& view:Views)view.Reset();Sources={};Outputs={};Frame={};}
};
static int TemporalViewProof(const std::filesystem::path& output,const std::filesystem::path& resources,bool localLighting=false) {
    using namespace Hazel;Log::Init();std::filesystem::create_directories(output);
    auto spec=Specification(output);spec.Name="Hazel temporal multiview";spec.EnableImGui=true;spec.RendererMode=RendererInitialization::Drawing;spec.Resources.Root=resources;
    std::shared_ptr<MetalProofObserver> observer;
    {Application app(spec);observer=Backend().Observe();auto layer=CreateScope<TemporalEditorViews>();layer->Output=output;layer->LocalLighting=localLighting;app.PushLayer(std::move(layer));
    auto start=Clock::now();app.SetBackgroundTick([&]{Check(Clock::now()-start<std::chrono::seconds(30),"Temporal active-view test timed out");});app.Run();}
    auto proof=observer->Snapshot();Check(proof.CompletedPresented>=100&&proof.Errors==0&&proof.ResourceBytes==0&&proof.AttachmentGenerations==0,"Temporal active-view presentation/retirement failed");
    if(localLighting)std::cout<<"PASS 600 local-light frames, independent lit drawable samples, 320x180 to 640x360 resize, stable resources after warmup\n";
    std::cout<<"PASS two independently moving temporal views, actual drawable samples, positive presentation and retirement\n";return 0;
}

// Real editor-style presentation avoids full FP32 test readbacks, whose three
// retained staging buffers would change the Retina memory workload materially.
class ShadowEditorSettings final : public Hazel::Layer {
public:
    Hazel::Ref<Hazel::Project> Project;
    Hazel::Ref<Hazel::Scene> Scene;
    Hazel::Ref<Hazel::Framebuffer> Source,Output;
    Hazel::SceneRenderView View;
    Hazel::NavigationCamera Camera{30,1600.f/900.f,.1f,1000};
    unsigned Frames=0;
    void OnAttach() override {
        using namespace Hazel;
        Project=Hazel::Project::LoadCandidate(std::filesystem::absolute("examples/RenderingReference/RenderingReference-Shadows.hproj"));
        Scene=Project->LoadScene(Project->GetConfig().StartScene);Scene->OnViewportResize(1600,900);
        Scene->SetUIFramebufferSize({3200,1800});Camera.SetViewportSize(1600,900);
        Scene->Presentation().Shadows.Resolution=2048;
        Camera.RestoreOrbit({-20.0864868f,14.6694498f,-25.6105156f},.609317422f,-4.08032942f,10);
        auto radicalInverse=[](unsigned index,unsigned base) {
            double value=0,weight=1;while(index){weight/=base;value+=weight*(index%base);index/=base;}return value;
        };
        for(unsigned phase=1;phase<=64;++phase) {
            glm::mat4 offset(1);offset[3][0]=float(2*(radicalInverse(phase,2)-.5)/3200);
            offset[3][1]=float(2*(radicalInverse(phase,3)-.5)/1800);
            SceneCameraView camera{offset*Camera.GetViewProjection(),Camera.GetPosition(),-Camera.GetForwardDirection(),false};
            const auto constants=AmbientOcclusionConstants(camera,Scene->Presentation().AmbientOcclusion,3200,1800,phase);
            for(unsigned axis=0;axis<3;++axis) {
                const auto reference=axis==0?Camera.GetRightDirection():axis==1?Camera.GetUpDirection():Camera.GetForwardDirection();
                const glm::vec3 actual(constants.WorldToView[0][axis],constants.WorldToView[1][axis],constants.WorldToView[2][axis]);
                Check(glm::length(actual-reference)<1e-5f,"Jittered AO basis differs from independent editor pose axes");
            }
        }
        std::cout<<"PASS all 64 Retina editor jitter phases against independent camera pose axes\n";
        FramebufferSpecification spec;spec.Width=3200;spec.Height=1800;
        spec.Attachments={FramebufferTextureFormat::RGBA32F,FramebufferTextureFormat::RED_INTEGER,FramebufferTextureFormat::DEPTH32F};
        Source=Framebuffer::Create(spec);spec.Attachments={FramebufferTextureFormat::RGBA8};Output=Framebuffer::CreateEditor(spec);
        auto* window=static_cast<GLFWwindow*>(Application::Get().GetWindow().GetNativeWindow());glfwSetWindowSize(window,800,450);
    }
    void OnUpdate(Hazel::Timestep) override {
        using namespace Hazel;++Frames;
        Scene->Presentation().AmbientOcclusion.Enabled=Frames>=4 && (Frames<48 || Frames>=52);
        Scene->Presentation().TemporalAA=Frames>=8 && (Frames<40 || Frames>=44);
        Source->Bind();Source->ClearEditor(Scene->Presentation().Background);Source->ClearAttachment(1,-1);
        Scene->OnUpdateEditor(Timestep(0),Camera,&View,Source);
        if(!Scene->RenderError().empty())throw std::runtime_error("Retina shadow editor frame "+std::to_string(Frames)+": "+Scene->RenderError());
        Renderer2D::BeginScene(Camera);Renderer2D::EndScene(); // Actual editor overlay entry contract.
        Source->Unbind();Source->EncodeOutput(Output);
        if(Frames>=60 && Backend().Observe()->Snapshot().CompletedPresented>=20)Application::Get().GetWindow().RequestClose();
    }
    void OnImGuiRender() override {
        ImGui::SetNextWindowViewport(ImGui::GetMainViewport()->ID);
        ImGui::SetNextWindowPos({10,30},ImGuiCond_Always);ImGui::SetNextWindowSize({760,390},ImGuiCond_Always);
        ImGui::Begin("Retina shadow / AO / TAA settings");
        Hazel::ImGuiLayer::Image(Output->GetEditorTexture(),640,360);ImGui::End();
    }
    void OnDetach() override {
        std::cout<<"SHADOW_EDITOR cpu_frames="<<Frames<<'\n';
        if(Source)Source->Cancel();View.Reset();Source.reset();Output.reset();Scene.reset();Project.reset();
    }
};
static int ShadowEditorSettingsProof(const std::filesystem::path& output,const std::filesystem::path& resources) {
    using namespace Hazel;Log::Init();std::filesystem::create_directories(output);
    auto spec=Specification(output);spec.Name="Hazel shadow settings regression";spec.EnableImGui=true;
    spec.RendererMode=RendererInitialization::Drawing;spec.Resources.Root=resources;
    std::shared_ptr<MetalProofObserver> observer;
    {Application app(spec);observer=Backend().Observe();app.PushLayer(CreateScope<ShadowEditorSettings>());
        auto start=Clock::now();app.SetBackgroundTick([&]{Check(Clock::now()-start<std::chrono::seconds(120),"Shadow editor settings timeout");});app.Run();}
    const auto proof=observer->Snapshot();
    std::cout<<"SHADOW_EDITOR presented="<<proof.CompletedPresented<<" errors="<<proof.Errors<<" resources="<<proof.ResourceBytes
        <<" attachments="<<proof.AttachmentGenerations<<" peak_resources="<<proof.PeakResourceBytes
        <<" peak_attachments="<<proof.PeakAttachmentBytes<<'\n';
    Check(proof.CompletedPresented>=20 && proof.Errors==0 && proof.ResourceBytes==0 && proof.AttachmentGenerations==0,"Shadow editor presentation/retirement failed");
    std::cout<<"PASS 3200x1800 editor shadow/AO/TAA toggles and overlays; positive presentations="<<proof.CompletedPresented
        <<" peak_resources="<<proof.PeakResourceBytes<<" peak_attachments="<<proof.PeakAttachmentBytes<<'\n';return 0;
}
