#include "Hazel.h"
#include "Hazel/Scene/Prefab.h"
#include "Hazel/Scene/SceneSerializer.h"
#include "Hazel/UI/RuntimeUI.h"
#include "Hazel/Utils/UTF8.h"
#include "Hazel/Core/Resources.h"
#include <fstream>
#include <GLFW/glfw3.h>
#include <filesystem>
#include <glad/glad.h>
#include <iostream>
#include <random>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <new>
#include <sstream>
#include <iomanip>
// Profile scope counts ordinary C++ new calls, excluding GL/OS malloc and setup.
static std::atomic<bool> s_CountAllocations{false};
static std::atomic<uint64_t> s_Allocations{0};
void* operator new(std::size_t bytes)
{
    if(auto p=std::malloc(bytes?bytes:1)) {if(s_CountAllocations.load(std::memory_order_relaxed))s_Allocations.fetch_add(1,std::memory_order_relaxed);return p;}
    throw std::bad_alloc();
}
void* operator new[](std::size_t bytes) {return ::operator new(bytes);}
void operator delete(void* p) noexcept {std::free(p);}
void operator delete[](void* p) noexcept {std::free(p);}
void operator delete(void* p,std::size_t) noexcept {std::free(p);}
void operator delete[](void* p,std::size_t) noexcept {std::free(p);}
using namespace Hazel;
using namespace Hazel::GameUI;
static void Check(bool value, const char *why)
{
    if (!value)
        throw std::runtime_error(why);
}
#include "MeasuredSceneChecks.h"
#include "ContainerSceneChecks.h"
#include "ImageFitSceneChecks.h"
struct UIFixture
{
    Ref<Scene> World=CreateRef<Scene>();Entity Canvas,Group,Control;
    UIFixture()
    {
        World->OnViewportResize(320,240);
        Canvas=World->CreateEntity("Canvas");auto& c=Canvas.AddComponent<UICanvasComponent>();c.ReferenceSize={320,240};c.SafeInsets=glm::vec4(0);
        Group=World->CreateEntity("Group");World->Reparent(Group,Canvas,TransformPolicy::KeepLocal);
        auto& r=Group.AddComponent<UIRectComponent>();r.Offset={10,10};r.FixedSize={150,80};
        Control=World->CreateEntity("Button");World->Reparent(Control,Group,TransformPolicy::KeepLocal);
        Control.AddComponent<UIRectComponent>().FixedSize={80,40};Control.AddComponent<UIButtonComponent>().Action="activate";
    }
    void Queue()
    {
        World->RefreshUI();auto n=World->GetUISnapshot().Find(Control.GetUUID());Check(n&&n->FocusEligible,"Fixture control not eligible");
        InputFrame input;input.Enabled=true;input.Mouse=n->PixelBounds.Position+n->PixelBounds.Size*.5f;
        World->UpdateUIInput(input);input.PointerPressed=input.PointerReleased=true;input.PressPosition=input.ReleasePosition=input.Mouse;input.HasReleasePosition=true;
        World->UpdateUIInput(input);
    }
};
static void HardeningSceneChecks()
{
    Action action;
    {UIFixture f;f.Queue();f.World->UpdateUIInput({});Check(!f.World->ConsumeUIAction(0,action),"D1: outer activation survived inactive viewport");
     InputFrame restored;restored.Enabled=true;f.World->UpdateUIInput(restored);Check(!f.World->ConsumeUIAction(0,action),"D1: restore delivered stale activation");}
    {UIFixture f;f.Queue();f.World->SetUIFramebufferSize({0,0});f.World->SetUIFramebufferSize({320,240});
     Check(!f.World->ConsumeUIAction(0,action)&&!f.World->GetUIDiagnostics().Focus,"Native zero/restore surface setter retained UI activation/focus");}
    {UIFixture f;f.Queue();f.Group.RemoveComponent<UIRectComponent>();Check(!f.World->ConsumeUIAction(0,action),"D4: parent rect removal retained child activation");
     f.World->RefreshUI();auto n=f.World->GetUISnapshot().Find(f.Control.GetUUID());Check(n&&!n->LayoutResolved&&!n->FocusEligible&&f.World->GetUIDiagnostics().Capture==0,"Interrupted chain retained interaction");}
    {UIFixture f;f.Queue();auto id=f.World->GetUISnapshot().Generation;f.Group.GetComponent<UIRectComponent>().Visible=false;
     Check(!f.World->ConsumeUIAction(0,action)&&f.World->GetUISnapshot().Generation>id,"Raw native hide escaped action validation");}
    {UIFixture f;f.Queue();auto holder=f.World->CreateEntity("Non-UI group");f.World->Reparent(f.Group,holder,TransformPolicy::KeepLocal);
     Check(!f.World->ConsumeUIAction(0,action),"D4: reparent retained child activation");f.World->RefreshUI();Check(!f.World->GetUISnapshot().Find(f.Control.GetUUID())->FocusEligible,"Reparented orphan remained eligible");}
    {UIFixture f;auto holder=f.World->CreateEntity("Interrupted chain");f.World->Reparent(f.Group,holder,TransformPolicy::KeepLocal);f.World->Reparent(holder,f.Canvas,TransformPolicy::KeepLocal);f.World->RefreshUI();
     auto generation=f.World->GetUISnapshot().Generation;auto other=f.World->CreateEntity("Hidden canvas");other.AddComponent<UICanvasComponent>().Visible=false;f.World->RefreshUI();generation=f.World->GetUISnapshot().Generation;
     f.World->Reparent(holder,other,TransformPolicy::KeepLocal);f.World->RefreshUI();auto n=f.World->GetUISnapshot().Find(f.Control.GetUUID());
     Check(f.World->GetUISnapshot().Generation>generation&&n->Canvas==uint64_t(other.GetUUID())&&!n->Visible&&!n->LayoutResolved,"Interrupted ancestor reparent left stale canvas/inherited state");}
    {UIFixture f;f.Queue();f.Group.AddOrReplaceComponent<UIRectComponent>();Check(!f.World->ConsumeUIAction(0,action),"Replacing layout ancestor retained work");}
    {UIFixture f;f.World->RefreshUI();auto generation=f.World->GetUISnapshot().Generation;auto bounds=f.World->GetUISnapshot().Find(f.Control.GetUUID())->PixelBounds;
     for(int i=0;i<5;++i){f.World->RefreshUI();}Check(f.World->GetUISnapshot().Generation==generation&&f.World->GetUIDiagnostics().Reuses>=5,"Unchanged geometry was rebuilt");
     f.Control.GetComponent<UIRectComponent>().Offset.x+=12;f.World->RefreshUI();Check(f.World->GetUISnapshot().Generation>generation&&f.World->GetUISnapshot().Find(f.Control.GetUUID())->PixelBounds.Position.x==bounds.Position.x+12,"Raw native offset escaped detection");
     InputFrame input;input.Enabled=true;f.World->UpdateUIInput(input);auto inputGeneration=f.World->GetUIDiagnostics().InputGeneration;
     f.Control.GetComponent<UIRectComponent>().Offset.y+=4;f.World->RefreshUI();Check(f.World->GetUISnapshot().Generation>inputGeneration&&f.World->GetUIDiagnostics().InputGeneration==inputGeneration,"Drawing a newer generation replayed input");}
    {UIFixture f;f.Queue();Check(f.World->ConsumeUIAction(f.Control.GetUUID(),action)&&!f.World->ConsumeUIAction(0,action),"Two consumers duplicated activation");f.Queue();Check(f.World->ConsumeUIAction(0,action)&&!f.World->ConsumeUIAction(f.Control.GetUUID(),action),"Global/entity consumers duplicated activation");}
    {UIFixture f;for(size_t i=0;i<InputRouter::MaxActions+5;++i)f.Queue();Check(f.World->GetUIDiagnostics().DroppedActions==5,"Outer queue did not report drop-newest overflow");
     f.World->CancelUIInput();InputFrame restored;restored.Enabled=true;f.World->UpdateUIInput(restored);
     Check(f.World->GetUIDiagnostics().DroppedActions==5&&!f.World->ConsumeUIAction(0,action),"Cancellation lost overflow diagnostics or retained work");}
    {UIFixture f;auto& r=f.Control.GetComponent<UIRectComponent>();r.Offset={300,300};f.World->RefreshUI();Check(!f.World->GetUISnapshot().Find(f.Control.GetUUID())->FocusEligible,"D2: fully clipped scene button eligible");
     f.Canvas.GetComponent<UICanvasComponent>().Modal=true;InputFrame input;input.Enabled=true;f.World->UpdateUIInput(input);input.AcceptDown=true;f.World->UpdateUIInput(input);Check(!f.World->ConsumeUIAction(0,action)&&!f.World->GetUIDiagnostics().Focus,"D2: clipped scene button activated");}
    {UIFixture f;auto& r=f.Control.GetComponent<UIRectComponent>();r.Offset={.1f,.1f};r.FixedSize={.4f,.4f};f.Group.GetComponent<UIRectComponent>().Offset={0,0};
     f.World->SetUIFramebufferSize({400,300});f.World->RefreshUI();auto n=f.World->GetUISnapshot().Find(f.Control.GetUUID());Check(!n->FocusEligible&&n->FramebufferClip.Size==glm::vec2(0),"Fractional empty framebuffer coverage remained eligible");
     r.FixedSize={2,2};f.World->RefreshUI();n=f.World->GetUISnapshot().Find(f.Control.GetUUID());Check(n->FocusEligible&&n->FramebufferClip.Position==glm::vec2(1)&&n->PixelClip.Position==glm::vec2(.8f),"Framebuffer/pixel clip conversion differs");
     f.World->SetUIFramebufferSize({0,0});f.World->RefreshUI();Check(f.World->GetUISnapshot().PainterOrder.empty()&&!f.World->GetUISnapshot().Find(f.Control.GetUUID())->FocusEligible,"Zero framebuffer did not suspend UI");}
    {UIFixture f;f.Canvas.GetComponent<UICanvasComponent>().Modal=true;f.World->RefreshUI();InputFrame input;input.Enabled=true;input.Mouse={25,25};f.World->UpdateUIInput(input);input.LeftDown=true;f.World->UpdateUIInput(input);
     f.Group.GetComponent<UIRectComponent>().Enabled=false;f.World->RefreshUI();Check(!f.World->GetUIDiagnostics().Capture,"Raw ancestor disable retained capture");
     input.LeftDown=false;input.AcceptDown=true;input.AcceptPressed=true;f.World->UpdateUIInput(input);Check(!f.World->ConsumeUIAction(0,action),"Retired pointer gesture became Accept");}
    {UIFixture f;auto original=f.World->GetUISnapshot().Generation;f.World->RefreshUI();f.World->ResetUI();f.World->RefreshUI();Check(f.World->GetUISnapshot().Generation>original&&!f.World->ConsumeUIAction(0,action),"Reset reused retired generation/actions");}
    {auto scene=CreateRef<Scene>();for(size_t i=0;i<InputRouter::MaxButtons+1;++i)scene->CreateEntity("Hidden").AddComponent<UIRectComponent>().Visible=false;
     bool rejected=false;try{scene->RefreshUI();}catch(const std::runtime_error&){rejected=true;}Check(rejected,"Hidden declared nodes escaped 4096 limit");}
    std::cout<<"PASS: scene D1/D2/D4/D5, generation reuse/raw writes, interrupted/reparented/replaced ancestors, fractional/zero surfaces and multiple consumers\n";
}
static void ProfileRuntimeUI()
{
    FramebufferSpecification spec;spec.Width=1280;spec.Height=720;
    spec.Attachments={FramebufferTextureFormat::RGBA8,FramebufferTextureFormat::RED_INTEGER,FramebufferTextureFormat::Depth};
    auto framebuffer=Framebuffer::Create(spec);framebuffer->Bind();
    using Clock=std::chrono::steady_clock;
    auto milliseconds=[](Clock::time_point a,Clock::time_point b){return std::chrono::duration<double,std::milli>(b-a).count();};
    auto stats=[](std::vector<double> values){std::sort(values.begin(),values.end());return std::pair<double,double>{values[values.size()/2],values[(values.size()*95)/100]};};
    for(int count:{64,512,2048})
    {
        // One staged import avoids repeated O(N) hierarchy reparent transactions
        // during fixture setup. It never writes a scene/project/save document.
        std::ostringstream yaml;yaml<<"SceneVersion: 2\nScene: UI profile\nEntities:\n- Entity: 1\n  UICanvasComponent: { ReferenceSize: [1280, 720], SafeInsets: [0, 0, 0, 0] }\n  Relationship: { Parent: 0, Order: 0 }\n";
        yaml<<"- Entity: 2\n  UIRectComponent: { LayoutVersion: 3, Offset: [16, 16], FixedSize: [1248, 688] }\n  Relationship: { Parent: 1, Order: 0 }\n";
        for(int i=0;i<count-1;++i)
        {
            int columns=count==2048?32:4;
            int x=(i%columns)*(count==2048?38:300),y=(i/columns)*(count==2048?10:36);
            yaml<<"- Entity: "<<i+3<<"\n  Relationship: { Parent: 2, Order: "<<i<<" }\n  UIRectComponent: { LayoutVersion: 3, Offset: ["<<x<<", "<<y<<"], FixedSize: ["<<(count==2048?36:280)<<", "<<(count==2048?10:32)<<"] }\n";
            if(i%2==0)yaml<<"  TextComponent: { TextString: 'Item "<<i<<"', Color: [1, 1, 1, 1], Kerning: 0, LineSpacing: 0 }\n  UITextStyleComponent: { Size: "<<(count==2048?8:18)<<", Wrap: false }\n";
            else yaml<<"  SpriteRendererComponent: { Color: [0.12, 0.24, 0.32, 1] }\n";
            bool control=count==64?i<8:i%4==0;
            if(control)yaml<<"  UIButtonComponent: { Action: 'activate', TabOrder: "<<i<<" }\n";
        }
        auto scene=CreateRef<Scene>();SceneSerializer loader(scene);
        Check(loader.DeserializeText(yaml.str()),"Profile staged fixture failed to import");
        scene->OnViewportResize(1280,720);scene->PrepareSprites(true);
        RuntimeUI ui(*scene);InputFrame frame;frame.Enabled=true;frame.Mouse={-1,-1};
        for(int i=0;i<20;++i){ui.Resolve({1280,720});ui.UpdateInput(frame);ui.Draw();}
        auto generation=ui.Snapshot().Generation;auto resolves=ui.Diagnostics().Resolves;
        std::vector<double> detect(300),route(300),renderDetect(300),draw(300),gpu(20),changed(30);
        uint64_t allocations=0;
        for(int i=0;i<300;++i)
        {
            s_Allocations=0;s_CountAllocations=true;auto a=Clock::now();ui.Resolve({1280,720});auto b=Clock::now();ui.UpdateInput(frame);auto c=Clock::now();ui.Resolve({1280,720});auto beforeDraw=Clock::now();s_CountAllocations=false;allocations+=s_Allocations;
            ui.Draw();auto d=Clock::now();detect[i]=milliseconds(a,b);route[i]=milliseconds(b,c);renderDetect[i]=milliseconds(c,beforeDraw);draw[i]=milliseconds(beforeDraw,d);
        }
        Check(ui.Snapshot().Generation==generation&&ui.Diagnostics().Resolves==resolves,"Idle profile rebuilt snapshot");
        GLuint query=0;glGenQueries(1,&query);
        for(int i=0;i<20;++i){glBeginQuery(GL_TIME_ELAPSED,query);ui.Draw();glEndQuery(GL_TIME_ELAPSED);GLuint64 nanos=0;glGetQueryObjectui64v(query,GL_QUERY_RESULT,&nanos);gpu[i]=double(nanos)/1e6;}
        glDeleteQueries(1,&query);
        auto moving=scene->GetEntityByUUID(3);
        for(int i=0;i<30;++i){moving.GetComponent<UIRectComponent>().Offset.x=float(i%2);auto a=Clock::now();ui.Resolve({1280,720});ui.UpdateInput(frame);auto b=Clock::now();changed[i]=milliseconds(a,b);}
        Renderer2D::ResetStats();ui.Draw();auto render=Renderer2D::GetStats();
        auto a=stats(detect),b=stats(route),c=stats(draw),d=stats(changed),g=stats(gpu),r=stats(renderDetect);
        std::cout<<std::fixed<<std::setprecision(4)<<"PROFILE nodes="<<count<<" controls="<<ui.Snapshot().Buttons.size()
            <<" detect_ms="<<a.first<<"/"<<a.second<<" route_ms="<<b.first<<"/"<<b.second<<" render_detect_ms="<<r.first<<"/"<<r.second<<" submit_cpu_ms="<<c.first<<"/"<<c.second
            <<" changed_layout+route_ms="<<d.first<<"/"<<d.second<<" gpu_ms="<<g.first<<"/"<<g.second
            <<" idle_cpp_new="<<allocations<<" draws="<<render.DrawCalls<<" quads="<<render.QuadCount<<" idle_generations=1\n";
    }
    framebuffer->Unbind();Check(glGetError()==GL_NO_ERROR,"Profile OpenGL error");
    std::cout<<"PASS: bounded UI profile; CPU median/p95, separate GL timer query, idle generation and C++ allocation count\n";
}
#include "ContainerProfile.h"
#include "RenderProfile.h"
int main(int argc,char** argv)
{
    try
    {
        Log::Init();
        ApplicationSpecification spec;
        spec.Name = "Migration Game UI";
        spec.EnableImGui = false;
        Application app(spec);
        glfwHideWindow(static_cast<GLFWwindow *>(app.GetWindow().GetNativeWindow()));
        if(argc==2&&std::string(argv[1])=="--profile-render"){ProfileRendering();return 0;}
        if(argc==2&&std::string(argv[1])=="--profile-layout"){ProfileContainers();return 0;}
        if(argc==2&&std::string(argv[1])=="--profile"){ProfileRuntimeUI();return 0;}
        HardeningSceneChecks();
        const auto root = std::filesystem::temp_directory_path() /
                          ("hazel-ui-" + std::to_string(std::random_device{}()));
        std::filesystem::create_directories(root);
        struct Cleanup
        {
            std::filesystem::path Root;
            ~Cleanup()
            {
                std::error_code e;
                std::filesystem::remove_all(Root, e);
            }
        } cleanup{root};
        std::filesystem::copy_file(Resources::Resolve("fonts/opensans/OpenSans-Regular.ttf"),
                                   root / "font.ttf");
        std::filesystem::copy_file("assets/textures/Checkerboard.png", root / "image.png");
        MeasuredSceneChecks(root);
        ContainerSceneChecks(root);
        ImageFitSceneChecks(root);
        auto scene = CreateRef<Scene>();
        scene->SetAssets(CreateRef<ProjectAssets>(root));
        scene->OnViewportResize(320, 240);
        auto canvas = scene->CreateEntity("Canvas");
        auto &c = canvas.AddComponent<UICanvasComponent>();
        c.ReferenceSize = {320, 240};
        c.SafeInsets = {0, 0, 0, 0};
        auto panel = scene->CreateEntity("Panel");
        scene->Reparent(panel, canvas, TransformPolicy::KeepLocal);
        auto &r = panel.AddComponent<UIRectComponent>();
        r.Offset = {20, 30};
        r.FixedSize = {180, 100};
        r.Padding = {10, 10, 10, 10};
        panel.AddComponent<SpriteRendererComponent>(glm::vec4(0, 0, 1, 1));
        auto button = scene->CreateEntity("Button");
        scene->Reparent(button, panel, TransformPolicy::KeepLocal);
        auto &b = button.AddComponent<UIRectComponent>();
        b.Offset = {-30, 10};
        b.FixedSize = {80, 40};
        button.AddComponent<SpriteRendererComponent>(glm::vec4(1, 0, 0, 1));
        button.AddComponent<UIButtonComponent>().Action = "play";
        auto text = scene->CreateEntity("Text");
        scene->Reparent(text, panel, TransformPolicy::KeepLocal);
        text.AddComponent<UIRectComponent>().Offset = {0, 55};
        text.AddComponent<TextComponent>().TextString = "Light\nkeeper";
        auto &style = text.AddComponent<UITextStyleComponent>();
        style.Size = 18;
        style.FontPath = "font.ttf";
        button.GetComponent<UIButtonComponent>().Images[1] =
            TextureSpriteSource{"image.png", 1, {}};
        scene->ValidateUI();
        scene->PrepareSprites(true);
        SceneSerializer(scene, root).Serialize((root / "ui.hazel").generic_u8string());
        auto dependencies = AuditSpriteAssets(root, {"ui.hazel"});
        Check(std::find(dependencies.begin(), dependencies.end(), "font.ttf") !=
                      dependencies.end() &&
                  std::find(dependencies.begin(), dependencies.end(), "image.png") !=
                      dependencies.end(),
              "UI font/state image escaped asset closure");
        FramebufferSpecification f;
        f.Width = 320;
        f.Height = 240;
        f.Attachments = {FramebufferTextureFormat::RGBA8, FramebufferTextureFormat::RED_INTEGER,
                         FramebufferTextureFormat::Depth};
        auto target = Framebuffer::Create(f);
        target->Bind();
        RenderCommand::SetClearColor({0, 0, 0, 1});
        RenderCommand::Clear();
        target->ClearAttachment(1, -1);
        auto raster = RenderCommand::GetRasterState();
        RuntimeUI ui(*scene);
        ui.Resolve({320, 240});
        ui.Draw();
        Check(target->ReadPixel(1, 20, 240 - 65) == int(uint32_t(button)),
              "Draw/hit clip inside disagreement");
        Check(target->ReadPixel(1, 10, 240 - 65) == -1, "Parent clipping leaked");
        auto after = RenderCommand::GetRasterState();
        Check(after.DepthTest == raster.DepthTest && after.ClipEnabled == raster.ClipEnabled &&
                  after.ClipRect == raster.ClipRect,
              "Overlay leaked raster state");
        InputFrame input;
        input.Enabled = true;
        input.Mouse = {25, 65};
        ui.Resolve({320, 240});
        ui.UpdateInput(input);
        input.LeftDown = true;
        ui.Resolve({320, 240});
        ui.UpdateInput(input);
        input.LeftDown = false;
        ui.Resolve({320, 240});
        ui.UpdateInput(input);
        Action action;
        Check(ui.Consume(button.GetUUID(), action) && action.Name == "play" &&
                  !ui.Consume(0, action),
              "Pointer release did not activate exactly once");
        input.FocusNavigation = true;
        input.NextDown = true;
        ui.Resolve({320, 240});
        ui.UpdateInput(input);
        input.FocusNavigation = input.NextDown = false;
        input.AcceptDown = true;
        ui.Resolve({320, 240});
        ui.UpdateInput(input);
        button.GetComponent<UIRectComponent>().Visible = false;
        Check(!ui.Consume(0, action), "Hidden control kept action");
        button.GetComponent<UIRectComponent>().Visible = true;
        auto modal = scene->CreateEntity("Modal");
        auto &modalCanvas = modal.AddComponent<UICanvasComponent>();
        modalCanvas.ReferenceSize = {320, 240};
        modalCanvas.SafeInsets = {0, 0, 0, 0};
        modalCanvas.SortOrder = 10;
        modalCanvas.Modal = true;
        ui.Resolve({320, 240});
        ui.UpdateInput({});
        Check(ui.BlocksGameplay(), "Modal does not block gameplay");
        const auto authored = SceneSerializer(scene, root).CaptureAuthoredState();
        auto copy = Scene::Copy(scene);
        Check(SceneSerializer(copy, root).CaptureAuthoredState() == authored,
              "Scene copy changed UI authored state");
        auto loaded = CreateRef<Scene>();
        SceneSerializer loader(loaded, root);
        Check(loader.DeserializeText(SceneSerializer(scene, root).SerializeText()),
              "UI roundtrip failed");
        Check(loader.CaptureAuthoredState() == authored, "UI roundtrip changed fields");
        Prefab::Save(root, "menu.hprefab", scene, canvas);
        auto instance = Prefab::Instantiate(root, "menu.hprefab", *loaded);
        Check(instance.GetUUID() != canvas.GetUUID() && instance.HasComponent<UICanvasComponent>(),
              "UI prefab root was not remapped");
        auto &changed = copy->GetEntityByUUID(button.GetUUID()).GetComponent<UIButtonComponent>();
        changed.Colors[2].x = .25f;
        Check(SceneSerializer(copy, root).CaptureAuthoredState() != authored,
              "UI style dirty tracking missed field");
        ui.Reset();
        Check(!ui.Consume(0, action), "Reset retained actions");
        ui.Resolve({0, 0});
        ui.UpdateInput(input);
        Check(!ui.Consume(0, action), "Minimized UI activated");
        auto font = Font::GetDefault();
        std::vector<Renderer2D::TextLine> lines;
        Renderer2D::LayoutText("A useful lantern", font, 2, true, 0, lines);
        Check(lines.size() > 1, "Text did not wrap");
        for (auto &line : lines)
            Check(std::abs(line.Width - Renderer2D::MeasureText(line.Text, font)) < .001f,
                  "Text measurement disagrees with layout");
        // D3 exercises actual shared measurement/layout/draw behavior, not only decoding.
        const auto bom=std::string("\xef\xbb\xbf")+"A";
        Renderer2D::LayoutText(bom,font,10,true,0,lines);
        Check(lines.size()==1&&lines[0].Text=="A"&&Renderer2D::MeasureText(bom,font)==Renderer2D::MeasureText("A",font),"BOM measurement/layout disagreement");
        Renderer2D::LayoutText(std::string("A\n\xef\xbb\xbf")+"B",font,10,true,0,lines);
        Check(lines.size()==2&&lines[1].Width==Renderer2D::MeasureText("B",font),"Nonleading BOM changed measurement after a line break");
        Camera textCamera(glm::mat4(1));Renderer2D::ResetStats();Renderer2D::BeginScene(textCamera,glm::mat4(1));
        Renderer2D::DrawString(bom,font,glm::mat4(1),{});Renderer2D::EndScene();Check(Renderer2D::GetStats().QuadCount==1,"Drawing BOM emitted a fallback glyph");
        for(auto invalid:std::vector<std::string>{std::string("A\0B",3),"\xc0\xaf","\xed\xa0\x80","\xf4\x90\x80\x80","\xe2\x82"})
        {
            int rejected=0;try{Renderer2D::MeasureText(invalid,font);}catch(const std::invalid_argument&){++rejected;}
            try{Renderer2D::LayoutText(invalid,font,10,true,0,lines);}catch(const std::invalid_argument&){++rejected;}
            try{Renderer2D::DrawString(invalid,font,glm::mat4(1),{});}catch(const std::invalid_argument&){++rejected;}
            Check(rejected==3,"D3: measurement/layout/drawing accepted malformed text inconsistently");
        }
        Check(glGetError() == GL_NO_ERROR, "UI OpenGL error");
        // Actual GPU coverage at 125% framebuffer scaling shares the input clip.
        FramebufferSpecification scaledSpec=f;scaledSpec.Width=400;scaledSpec.Height=300;
        auto scaledTarget=Framebuffer::Create(scaledSpec);scaledTarget->Bind();RenderCommand::Clear();scaledTarget->ClearAttachment(1,-1);
        modalCanvas.Visible=false;panel.GetComponent<UIRectComponent>().Offset={20.1f,30.1f};
        ui.ResolveSurface({{320,240},{400,300}});ui.Draw();
        Check(scaledTarget->ReadPixel(1,26,300-80)==int(uint32_t(button))&&scaledTarget->ReadPixel(1,25,300-80)==-1&&scaledTarget->ReadPixel(1,100,300-80)!=int(uint32_t(button)),"125% fractional framebuffer scissor/picking disagrees with clip");
        InputFrame scaledInput;scaledInput.Enabled=true;scaledInput.Mouse={20.4f,64};ui.UpdateInput(scaledInput);
        scaledInput.PointerPressed=scaledInput.PointerReleased=true;scaledInput.PressPosition=scaledInput.ReleasePosition=scaledInput.Mouse;scaledInput.HasReleasePosition=true;
        ui.UpdateInput(scaledInput);Check(!ui.Consume(0,action),"Uncovered fractional edge accepted a hit");
        scaledInput.Mouse=scaledInput.PressPosition=scaledInput.ReleasePosition={20.8f,64};
        ui.UpdateInput(scaledInput);Check(ui.Consume(button.GetUUID(),action),"Framebuffer-covered edge did not hit in viewport units");
        auto detached=Scene::ExtractSubtree(button);detached->OnViewportResize(320,240);detached->PrepareSprites(true);
        RuntimeUI preview(*detached);preview.ResolveSurface({{320,240},{400,300}},true);
        RenderCommand::Clear();preview.Draw();GLint oldRead=0;glGetIntegerv(GL_READ_BUFFER,&oldRead);glReadBuffer(GL_COLOR_ATTACHMENT0);
        uint8_t previewColor[4]{};glReadPixels(5,300-12,1,1,GL_RGBA,GL_UNSIGNED_BYTE,previewColor);glReadBuffer(oldRead);
        Check(previewColor[0]>240&&previewColor[1]==0,"Detached authoring preview acquired disabled appearance from hit ineligibility");
        InputFrame previewInput;previewInput.Enabled=true;preview.UpdateInput(previewInput);previewInput.AcceptDown=true;preview.UpdateInput(previewInput);
        Check(!preview.Consume(0,action)&&!preview.Diagnostics().Focus,"Detached authoring preview gained runtime eligibility");
        scaledTarget->Unbind();scaledTarget.reset();
        target->Unbind();
        target.reset();
        std::cout << "PASS: camera-free UI render/hit/clip agreement, raster "
                     "restoration, actions, modal gating, retirement, resize, "
                     "authored copy/roundtrip/prefab and text metrics\n";
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << "FAIL: " << e.what() << '\n';
        return 1;
    }
}
