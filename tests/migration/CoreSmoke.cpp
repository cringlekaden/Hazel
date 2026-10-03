#include "Hazel.h"
#include "Hazel/Events/KeyEvent.h"
#include <imgui.h>
#include <cmath>
#include "Hazel/Core/Buffer.h"
#include "Hazel/Core/FileSystem.h"
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

static void Check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
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
    std::unordered_set<UUID> ids{UUID(1),UUID(1),UUID(2)};
    Check(ids.size()==2,"UUID hash/value identity");
    Timer timer; Check(timer.Elapsed()>=0.0f,"Monotonic timer");
    auto transform=glm::translate(glm::mat4(1),glm::vec3(2,3,4))*glm::scale(glm::mat4(1),glm::vec3(2,3,4));
    glm::vec3 t,r,s; Check(Math::DecomposeTransform(transform,t,r,s),"Transform decomposition failed");
    Check(glm::length(t-glm::vec3(2,3,4))<0.001f && glm::length(s-glm::vec3(2,3,4))<0.001f,"Transform decomposition values");
    EditorCamera camera(45,16.0f/9.0f,0.1f,1000);
    MouseScrolledEvent scroll(0,1); camera.OnEvent(scroll);
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
int main(int argc,char** argv)
{
    try {
        Hazel::Log::Init(); CheckCore();
        for(int repeat=0;repeat<2;++repeat) {
            Counts counts; int callbacks=0;
            Hazel::ApplicationSpecification specification;
            specification.Name="Migration core lifecycle";
            specification.EnableImGui=repeat==0;
            specification.CommandLineArgs={argc,argv};

            auto application=Hazel::CreateScope<Hazel::Application>(specification);
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
