#pragma once
#include "Hazel/Renderer/Scene/SceneRenderer.h"
#include "Hazel/Renderer/Drawing/Renderer3D.h"
#include "Hazel/Assets/ProjectAssets.h"
class TemporalEditorViews final : public Hazel::Layer {
public:
    std::array<Hazel::SceneRenderView,2> Views;
    std::array<Hazel::Ref<Hazel::Framebuffer>,2> Sources,Outputs;
    Hazel::SceneSubmission Frame;
    unsigned Frames=0,Verified=0;
    std::shared_ptr<Hazel::MetalEditorCapture> Capture;
    struct Point {uint32_t Viewport;int X,Y;};std::array<Point,2> Points{};
    std::filesystem::path Output;
    void OnAttach() override {
        using namespace Hazel;
        auto project=Project::LoadCandidate(std::filesystem::absolute("examples/RenderingReference/RenderingReference-Direct-PerspectivePBR.hproj"));
        Frame.Assets=CreateRef<ProjectAssets>(project->GetAssetRoot());auto mesh=Frame.Assets->Mesh("Native/Direct_v1/UnitCube.hmesh");Check(bool(mesh.Accepted),"Temporal multiview mesh missing");
        auto material=CreateRef<MaterialDefinition>();material->Model=MaterialModel::Unlit;material->BaseColor={.7f,.3f,.05f,1};
        SubmittedMesh draw;draw.Geometry=mesh.Accepted;draw.Materials={material};draw.Picking=123456789;draw.Identity=1;Frame.Meshes={draw};Frame.SceneIdentity=99;
        Frame.Presentation.SceneReferred=true;Frame.Presentation.TemporalAA=true;
        FramebufferSpecification spec;auto& window=Application::Get().GetWindow();auto pixels=window.GetFramebufferSize();
        const float scale=float(pixels.first)/float(window.GetWidth());spec.Width=uint32_t(320*scale);spec.Height=uint32_t(180*scale);spec.Attachments={FramebufferTextureFormat::RGBA32F,FramebufferTextureFormat::RED_INTEGER,FramebufferTextureFormat::DEPTH32F};
        for(unsigned i=0;i<2;++i){Sources[i]=Framebuffer::Create(spec);auto encoded=spec;encoded.Attachments={FramebufferTextureFormat::RGBA8};Outputs[i]=Framebuffer::CreateEditor(encoded);}
        Backend().EnableEditorReadbackForTesting();
        auto* native=static_cast<GLFWwindow*>(Application::Get().GetWindow().GetNativeWindow());glfwSetWindowPos(native,0,24);glfwSetWindowSize(native,800,400);
    }
    void OnUpdate(Hazel::Timestep) override {
        using namespace Hazel;++Frames;
        if(Capture&&Capture->Completed&&!Verified) {
            std::cout<<"TAA_VIEWS_CAPTURE serial="<<Capture->Serial<<"\n";
            for(unsigned i=0;i<2;++i) {
                auto image=std::find_if(Capture->Images.begin(),Capture->Images.end(),[&](const auto& image){return image.Viewport==Points[i].Viewport;});
                Check(image!=Capture->Images.end()&&Points[i].X>=0&&Points[i].Y>=0&&unsigned(Points[i].X)<image->Width&&unsigned(Points[i].Y)<image->Height,"Temporal view drawable sample missing");
                size_t pixel=(size_t(Points[i].Y)*image->Width+unsigned(Points[i].X))*4;
                Check(i==0?image->BGRA[pixel]>image->BGRA[pixel+2]+10:image->BGRA[pixel+2]>image->BGRA[pixel]+10,"Independent temporal view drawable colors mixed");++Verified;
            }
            const auto& image=*std::find_if(Capture->Images.begin(),Capture->Images.end(),[&](const auto& image){return image.Viewport==Points[0].Viewport;});std::ofstream file(Output/"two-temporal-views.ppm",std::ios::binary);file<<"P6\n"<<image.Width<<' '<<image.Height<<"\n255\n";
            for(size_t p=0;p<image.BGRA.size();p+=4){char rgb[]={char(image.BGRA[p+2]),char(image.BGRA[p+1]),char(image.BGRA[p])};file.write(rgb,3);}
        }
        for(unsigned i=0;i<2;++i) {
            float direction=i?1.f:-1.f;
            Frame.Camera.Position={direction*.2f*std::sin(float(Frames)*.02f),0,4};Frame.Camera.ToCamera={0,0,1};
            Frame.Camera.ViewProjection=glm::perspective(glm::radians(45.f),320.f/180.f,.1f,20.f)*glm::lookAt(Frame.Camera.Position,glm::vec3(0),glm::vec3(0,1,0));
            Frame.Meshes[0].SetWorld(glm::rotate(glm::mat4(1),direction*float(Frames)*.004f,glm::vec3(0,1,0)));
            Frame.Presentation.Background=i?glm::vec4(.08f,.02f,.01f,1):glm::vec4(.015f,.03f,.06f,1);
            Sources[i]->Bind();Sources[i]->Clear(Frame.Presentation.Background);SceneRenderer::Render(Frame,Views[i],Sources[i],[]{},[]{});Sources[i]->Unbind();Sources[i]->EncodeOutput(Outputs[i]);
        }
        Check(Views[0].Temporal.Submitted!=Views[1].Temporal.Submitted,"Active temporal views share submission state");
        if(Frames==200)for(auto& view:Views)Check(view.TemporalMetrics->Valid&&view.TemporalMetrics->Reuses>10,"Active view did not resolve/reuse its own history");
        if(Frames>=240&&Verified==2)Application::Get().GetWindow().RequestClose();
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
                Points[i]={viewport->ID,int((top.x-viewport->Pos.x+32)*scale.first),int((top.y-viewport->Pos.y+18)*scale.second)};
            }
        }
        ImGui::Text("Independent camera/object motion; two view-owned histories; frame %u",Frames);ImGui::End();
    }
    void OnDetach() override {for(auto& view:Views)view.Reset();Sources={};Outputs={};Frame={};}
};
static int TemporalViewProof(const std::filesystem::path& output,const std::filesystem::path& resources) {
    using namespace Hazel;Log::Init();std::filesystem::create_directories(output);
    auto spec=Specification(output);spec.Name="Hazel temporal multiview";spec.EnableImGui=true;spec.RendererMode=RendererInitialization::Drawing;spec.Resources.Root=resources;
    std::shared_ptr<MetalProofObserver> observer;
    {Application app(spec);observer=Backend().Observe();auto layer=CreateScope<TemporalEditorViews>();layer->Output=output;app.PushLayer(std::move(layer));
    auto start=Clock::now();app.SetBackgroundTick([&]{Check(Clock::now()-start<std::chrono::seconds(30),"Temporal active-view test timed out");});app.Run();}
    auto proof=observer->Snapshot();Check(proof.CompletedPresented>=100&&proof.Errors==0&&proof.ResourceBytes==0&&proof.AttachmentGenerations==0,"Temporal active-view presentation/retirement failed");
    std::cout<<"PASS two independently moving temporal views, actual drawable samples, positive presentation and retirement\n";return 0;
}
