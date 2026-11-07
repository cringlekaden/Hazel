#include "Hazel.h"
#include "Hazel/Events/KeyEvent.h"
#include <imgui.h>
#include <cmath>
#include "Hazel/Core/Buffer.h"
#include "Hazel/Core/FileSystem.h"
#include "Hazel/Core/InstallationLayout.h"
#include "Hazel/Core/ApplicationLaunch.h"
#include "Hazel/Core/Timer.h"
#include "Hazel/Core/UUID.h"
#include "Hazel/Math/Math.h"
#include <GLFW/glfw3.h>
#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>
#include <filesystem>
#include <fstream>
#include <thread>
#include <unordered_set>
#include <stdexcept>
#include <iostream>
#include <optional>

static void Check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
#include "InstrumentationChecks.h"
#include "UIStyleAssetChecks.h"
struct Counts { int attached=0, detached=0, destroyed=0; };
class CountLayer : public Hazel::Layer {
public:
    explicit CountLayer(Counts& counts): m_Counts(counts) {}
    ~CountLayer() override { ++m_Counts.destroyed; }
    void OnAttach() override { ++m_Counts.attached; }
    void OnDetach() override { ++m_Counts.detached; }
private: Counts& m_Counts;
};
static void CheckCore()
{
    using namespace Hazel;
    {
        std::vector<std::string> values{"app","--project",u8"project space é/Game.hproj","--smoke-test"};
        std::vector<char*> pointers;for(auto& value:values)pointers.push_back(value.data());
        auto launch=ApplicationLaunch::Parse(static_cast<int>(pointers.size()),pointers.data());
        Check(launch.SmokeTest && launch.Count()==3 && launch.Arguments.back()==nullptr &&
              launch.Arguments[0]==pointers[0] && launch.Arguments[1]==pointers[1] && launch.Arguments[2]==pointers[2],
              "Diagnostic parsing lost application arguments or Unicode paths");
        values.push_back("--smoke-test");pointers.clear();for(auto& value:values)pointers.push_back(value.data());
        bool rejected=false;try{ApplicationLaunch::Parse(static_cast<int>(pointers.size()),pointers.data());}catch(const std::invalid_argument&){rejected=true;}
        Check(rejected,"Duplicate diagnostic launch accepted");
    }
    const auto bundle=InstallationLayout::FromExecutable(std::filesystem::u8path("/portable space é/Hazelnut.app/Contents/MacOS/Hazelnut"));
    Check(bundle.ResourceRoot==std::filesystem::u8path("/portable space é/Hazelnut.app/Contents/Resources") &&
          bundle.MonoRoot==bundle.ResourceRoot/"Mono" &&
          bundle.RuntimeProject==bundle.EditorProject && bundle.Metadata==bundle.ResourceRoot/"build.json",
          "Bundle installation roots disagree");
    const auto loose=InstallationLayout::FromExecutable(std::filesystem::path("/portable/Nutella"));
    Check(loose.ResourceRoot=="/portable/Resources" && loose.MonoRoot=="/portable/mono" &&
          loose.RuntimeProject=="/portable" && loose.EditorProject=="/portable/Example", "Loose installation contract changed");
    ApplicationResourceSpecification partialResources;
    partialResources.UserData = std::filesystem::temp_directory_path()/"hazel explicit data";
#ifdef HZ_PLATFORM_LINUX
    // A host's explicit writable root must not require an unused user-data lookup.
    std::vector<std::pair<std::string,std::optional<std::string>>> environment;
    for (const char* name : {"HOME", "XDG_DATA_HOME", "HAZEL_DATA"}) {
        const char* value = std::getenv(name);
        environment.emplace_back(name, value ? std::optional<std::string>(value) : std::nullopt);
        unsetenv(name);
    }
    auto restoreEnvironment = [&] {
        for (const auto& [name,value] : environment)
            if (value) setenv(name.c_str(),value->c_str(),1); else unsetenv(name.c_str());
    };
    try {
#endif
        const auto resolved = Resources::Defaults("CoreSmoke",partialResources);
        Check(resolved.UserData==partialResources.UserData && !resolved.Root.empty() && !resolved.MonoRoot.empty(),
              "Partial resource specification lost explicit data or required an unused default");
#ifdef HZ_PLATFORM_LINUX
    } catch (...) { restoreEnvironment(); throw; }
    restoreEnvironment();
#endif
    HZ_CORE_ASSERT(true);
    HZ_CORE_ASSERT(true, "variadic {} {}", 1, 2);
    Counts first, second, overlay;
    LayerStack stack;
    auto a=CreateScope<CountLayer>(first); auto* pa=a.get();
    auto b=CreateScope<CountLayer>(second); auto* pb=b.get();
    auto o=CreateScope<CountLayer>(overlay); auto* po=o.get();
    stack.PushLayer(std::move(a)); stack.PushOverlay(std::move(o)); stack.PushLayer(std::move(b));
    auto it=stack.begin(); Check(it->get()==pa,"First layer order");
    Check((++it)->get()==pb,"Insertion boundary moved past overlay");
    Check((++it)->get()==po,"Overlay order");
    stack.PopOverlay(pa); stack.PopLayer(po);
    Check(first.destroyed==0 && overlay.destroyed==0,"Cross-partition pop removed a layer");
    stack.PopLayer(pa); stack.PopOverlay(po);
    Check(first.detached==1 && first.destroyed==1 && overlay.detached==1 && overlay.destroyed==1,"Pop lifetime differs");
    stack.Clear(); stack.Clear();
    Check(second.attached==1 && second.detached==1 && second.destroyed==1,"Clear lifetime differs");
    Counts reused; stack.PushLayer(CreateScope<CountLayer>(reused)); stack.Clear();
    Check(reused.destroyed==1,"Clear did not reset insertion boundary");

    KeyPressedEvent event(Key::A,true); event.Handled=true;
    EventDispatcher dispatcher(event); dispatcher.Dispatch<KeyPressedEvent>([](auto&){return false;});
    Check(event.Handled && event.IsRepeat(),"Dispatcher lost handled/repeat state");
    Check(KeyTypedEvent(0x1f680).GetKeyCode()==0x1f680,"Unicode codepoint truncated");
    Buffer original(4); original.Data[0]=42;
    Buffer copied=Buffer::Copy(original); Buffer moved=std::move(original);
    Check(!original && original.Size==0 && moved.Data[0]==42 && copied.Data!=moved.Data,"Buffer move/copy ownership");
    moved.Release(); Check(!moved && copied.Data[0]==42,"Buffer release damaged copy");
    const auto path=std::filesystem::temp_directory_path()/std::filesystem::path("hazel-core-"+std::to_string(static_cast<std::uint64_t>(UUID()))+".bin");
    {std::ofstream file(path,std::ios::binary); file.write("abcd",4);}
    auto file=FileSystem::ReadFileBinary(path); std::filesystem::remove(path);
    Check(file.Size==4 && file.Data[3]=='d',"Binary filesystem read");
    auto traceRoot=path;traceRoot+=".trace";std::filesystem::create_directory(traceRoot);
    UIStyleAssetChecks();
    InstrumentationChecks(traceRoot);std::filesystem::remove_all(traceRoot);
    std::unordered_set<UUID> ids{UUID(1),UUID(1),UUID(2)};
    Check(ids.size()==2,"UUID hash/value identity");
    Timer timer; Check(timer.Elapsed()>=0.0f,"Monotonic timer");
    auto transform=glm::translate(glm::mat4(1),glm::vec3(2,3,4))*glm::scale(glm::mat4(1),glm::vec3(2,3,4));
    glm::vec3 t,r,s; Check(Math::DecomposeTransform(transform,t,r,s),"Transform decomposition failed");
    Check(glm::length(t-glm::vec3(2,3,4))<0.001f && glm::length(s-glm::vec3(2,3,4))<0.001f,"Transform decomposition values");
    NavigationCamera camera(45,16.0f/9.0f,0.1f,1000);
    SceneCamera authoredProjection;authoredProjection.SetViewportSize(1600,900);authoredProjection.SetPerspective(glm::radians(45.f),.1f,1000.f);
    Check(camera.GetProjection()==authoredProjection.GetProjection(),"Authored/transient perspective math diverged");
    camera.Scroll(1);
    Check(camera.GetDistance()<10 && std::isfinite(camera.GetViewProjection()[0][0]),"Editor camera zoom/projection");
}
struct PendingCapture {
    bool& contextWasAlive;
    explicit PendingCapture(bool& value) : contextWasAlive(value) {}
    ~PendingCapture() { contextWasAlive = glfwGetCurrentContext() != nullptr; }
};
class LifecycleLayer : public Hazel::Layer {
public:
    explicit LifecycleLayer(Counts& counts): m_Counts(counts) {}
    ~LifecycleLayer() override { ++m_Counts.destroyed; }
    void OnAttach() override { ++m_Counts.attached; }
    void OnDetach() override {
        Check(glfwGetCurrentContext()!=nullptr,"Context gone before layer detach");
        Check((ImGui::GetCurrentContext()!=nullptr)==Hazel::Application::Get().GetSpecification().EnableImGui,"Optional ImGui lifetime differs from specification");
        Hazel::Renderer2D::ResetStats(); // proves renderer still exists during OnDetach.
        ++m_Counts.detached;
    }
    void OnUpdate(Hazel::Timestep) override {
        auto position=Hazel::Input::GetMousePosition();
        Check(std::isfinite(position.x) && std::isfinite(position.y),"Native mouse query");
        (void)Hazel::Input::IsKeyPressed(Hazel::Key::A);
        (void)Hazel::Input::IsMouseButtonPressed(Hazel::Mouse::ButtonLeft);
        if (++frames>10) throw std::runtime_error("Main thread queue did not close application");
    }
private: Counts& m_Counts; int frames=0;
};
class SmokeDrawLayer : public Hazel::Layer {
public:
    explicit SmokeDrawLayer(unsigned& frames):m_Frames(frames){}
    void OnUpdate(Hazel::Timestep) override {
        using namespace Hazel;
        const auto size=Application::Get().GetWindow().GetFramebufferSize();
        RenderCommand::SetViewport(0,0,size.first,size.second);
        RenderCommand::SetClearColor({0,0,0,1});RenderCommand::Clear();
        OrthographicCamera camera(-1,1,-1,1);Renderer2D::BeginScene(camera);
        Renderer2D::DrawQuad(glm::vec2(0),glm::vec2(1),glm::vec4(0,1,0,1));Renderer2D::EndScene();
        if(++m_Frames==1) {
            unsigned char pixel[4]{};glReadBuffer(GL_BACK);
            glReadPixels(size.first/2,size.second/2,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel);
            Check(pixel[1]>200 && pixel[0]<30,"Diagnostic frame did not actually render its quad");
        }
    }
private: unsigned& m_Frames;
};
static void CheckSmokeLifecycle() {
    using namespace Hazel;
    for(int mode=0;mode<3;++mode) {
        ApplicationSpecification spec;spec.Name="Bounded application qualification";spec.EnableImGui=false;
        auto app=CreateScope<Application>(spec);unsigned frames=0,closes=0;
        if(mode!=1)app->PushLayer(CreateScope<SmokeDrawLayer>(frames));
        app->SetCloseRequest([&]{++closes;if(mode!=2)app->Close();});
        bool rejected=false;
        try{app->RunForSmokeTest();}catch(const std::runtime_error& error){
            const std::string text=error.what();
            rejected=(mode==1 && text.find("no engine draw")!=std::string::npos) ||
                     (mode==2 && text.find("deferred or blocked")!=std::string::npos);
            if(!rejected)throw;
        }
        Check(mode==0 ? (!rejected && frames==60 && closes==1) : rejected,
              "Bounded run bypassed draw/close validation or did not complete 60 frames");
        if(mode==1)Check(closes==0,"No-draw diagnostic incorrectly requested a successful close");
        if(mode==2)Check(closes==1,"Blocked diagnostic retried or bypassed the close handler");
        app.reset();
    }
    std::cout<<"PASS: bounded real rendered frames, ordinary close dispatch, no-draw rejection and blocked-close preservation\n";
}
int main(int argc,char** argv)
{
    try {
        Hazel::Log::Init(); CheckCore();CheckSmokeLifecycle();
        for(int repeat=0;repeat<2;++repeat) {
            Counts counts; int callbacks=0;
            Hazel::ApplicationSpecification specification;
            specification.Name="Migration core lifecycle";
            specification.EnableImGui=repeat==0;
            specification.CommandLineArgs={argc,argv};

            auto application=Hazel::CreateScope<Hazel::Application>(specification);
            int width=0,height=0;
            glfwGetWindowSize(static_cast<GLFWwindow*>(application->GetWindow().GetNativeWindow()),&width,&height);
            Check(application->GetWindow().GetWidth()==static_cast<unsigned int>(width) &&
                  application->GetWindow().GetHeight()==static_cast<unsigned int>(height),
                  "Initial window dimensions differ from the native client area");
            auto* native=static_cast<GLFWwindow*>(application->GetWindow().GetNativeWindow());
            int pixelsX=0,pixelsY=0;glfwGetFramebufferSize(native,&pixelsX,&pixelsY);
            const auto pixels=application->GetWindow().GetFramebufferSize();
            Check(pixels.first==static_cast<unsigned>(pixelsX)&&pixels.second==static_cast<unsigned>(pixelsY),"Framebuffer service lost native pixel dimensions");
            float scaleX=0,scaleY=0;glfwGetWindowContentScale(native,&scaleX,&scaleY);
            Check(scaleX>0&&scaleY>0&&pixelsX>0&&pixelsY>0,"Invalid native framebuffer/content scale");
            std::cout<<"Window: logical "<<width<<'x'<<height<<", framebuffer "<<pixelsX<<'x'<<pixelsY<<", scale "<<scaleX<<'x'<<scaleY<<'\n';
            Check(application->GetSpecification().CommandLineArgs[0]==argv[0],"Application args/specification");
            Check((application->GetImGuiLayer()!=nullptr)==specification.EnableImGui,"Optional ImGui access differs from specification");
            application->PushLayer(Hazel::CreateScope<LifecycleLayer>(counts));
            std::thread producer([&]{application->SubmitToMainThread([&]{
                ++callbacks;
                application->SubmitToMainThread([&]{++callbacks; application->Close();});
            });}); producer.join();
            application->Run();
            bool contextWasAlive=false;
            auto captured=Hazel::CreateRef<PendingCapture>(contextWasAlive);
            application->SubmitToMainThread([captured] {});
            captured.reset();
            application.reset();
            Check(contextWasAlive,"Queued resource captures outlived graphics context");
            Check(callbacks==2,"Main-thread queue/reentrant submission");
            Check(counts.attached==1 && counts.detached==1 && counts.destroyed==1,"Application layer lifetime");

        }
        std::cout<<"PASS: Scope layers/partitions/clear, Unicode events, buffers/filesystem/UUID/timer/math/camera, args/input/reentrant queue, detach-renderer-ImGui-context lifetime and repeated application shutdown\n";
        return 0;
    } catch(const std::exception& error) { std::cerr<<"FAIL: "<<error.what()<<'\n'; return 1; }
}
