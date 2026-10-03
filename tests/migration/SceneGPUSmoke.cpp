// Full target scene consumers on a real desktop GL context; overrides are test-only.
#include "Hazel.h"
#include "Hazel/Scene/Scene.h"
#include "Hazel/Scene/Entity.h"
#include "Hazel/Scene/SceneSerializer.h"
#include "Hazel/Scripting/ScriptEngine.h"
#include "Hazel/Project/Project.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <random>
#include <stdexcept>
#include <vector>
using namespace Hazel;
static void Check(bool value,const char* message) { if(!value) throw std::runtime_error(message); }
struct Fixture {
    std::filesystem::path Previous=std::filesystem::current_path();
    std::filesystem::path Directory=std::filesystem::temp_directory_path()/std::filesystem::u8path("hazel-scene-gpu-é-"+std::to_string(std::random_device{}()));
    Fixture() {
        Check(std::filesystem::create_directory(Directory),"Scene GPU isolation failed");
        std::filesystem::copy("assets",Directory/"assets",std::filesystem::copy_options::recursive);
    }
    ~Fixture() { std::error_code e; std::filesystem::current_path(Previous,e); std::filesystem::remove_all(Directory,e); }
};
static void Begin(const Ref<Framebuffer>& framebuffer) {
    framebuffer->Bind(); glDisable(GL_DEPTH_TEST);
    RenderCommand::SetClearColor({0,0,0,0}); RenderCommand::Clear(); framebuffer->ClearAttachment(1,-1);
}
static void CheckScenePixels(const Ref<Scene>& scene) {
    std::vector<int> ids(256*256); glReadBuffer(GL_COLOR_ATTACHMENT1);
    glReadPixels(0,0,256,256,GL_RED_INTEGER,GL_INT,ids.data());
    for(uint64_t uuid:{11,12,13}) {
        const int entity=static_cast<uint32_t>(scene->GetEntityByUUID(uuid));
        Check(std::count(ids.begin(),ids.end(),entity)>20,"Scene sprite/circle/text picking output missing");
    }
    glReadBuffer(GL_COLOR_ATTACHMENT0);
    std::vector<unsigned char> colors(256*256*4); glReadPixels(0,0,256,256,GL_RGBA,GL_UNSIGNED_BYTE,colors.data());
    for(uint64_t uuid:{11,12,13}) {
        const int entity=static_cast<uint32_t>(scene->GetEntityByUUID(uuid)); size_t colored=0;
        const size_t channel=uuid==11 ? 0 : uuid==12 ? 2 : 1;
        for(size_t i=0;i<ids.size();++i) if(ids[i]==entity && colors[4*i+channel]>20 && colors[4*i+channel]>colors[4*i+(channel+1)%3]) ++colored;
        Check(colored>20,"Scene sprite/circle/text color output missing");
    }
    Check(glGetError()==GL_NO_ERROR,"Scene rendering OpenGL error");
}
static Ref<Scene> MakeScene(const std::filesystem::path& directory) {
    auto scene=CreateRef<Scene>(); scene->OnViewportResize(256,256);
    auto camera=scene->CreateEntityWithUUID(10,"Camera"); camera.AddComponent<CameraComponent>().Camera.SetOrthographic(4,-1,1);
    auto sprite=scene->CreateEntityWithUUID(11,u8"carré-é"); sprite.GetComponent<TransformComponent>().Translation={-1,0,0};
    auto& src=sprite.AddComponent<SpriteRendererComponent>(); src.Color={1,0,0,1}; src.TilingFactor=2;
    const auto texture=directory/"assets/textures"/std::filesystem::u8path("damier-é.png");
    std::filesystem::copy_file(directory/"assets/textures/Checkerboard.png",texture);
    const auto previous=std::filesystem::current_path();
    std::filesystem::current_path(directory/"assets");
    try { src.Texture=Texture2D::Create(std::filesystem::relative(texture,directory/"assets").generic_u8string()); }
    catch(...) { std::filesystem::current_path(previous); throw; }
    std::filesystem::current_path(previous);
    auto disk=scene->CreateEntityWithUUID(12,"Circle"); disk.GetComponent<TransformComponent>().Translation={1,0,0};
    disk.AddComponent<CircleRendererComponent>().Color={0,0,1,1};
    disk.AddComponent<Rigidbody2DComponent>().Type=Rigidbody2DComponent::BodyType::Dynamic;
    disk.AddComponent<CircleCollider2DComponent>();
    auto text=scene->CreateEntityWithUUID(13,"Text"); text.GetComponent<TransformComponent>().Translation={-0.6f,1,0};
    text.GetComponent<TransformComponent>().Scale={0.6f,0.6f,1};
    auto& tc=text.AddComponent<TextComponent>(); tc.TextString=u8"Hazel é"; tc.Kerning=0.1f; tc.LineSpacing=0.2f; tc.Color={0,1,0,1};
    return scene;
}
class ReloadLayer : public Layer {
public:
    ReloadLayer(const std::filesystem::path& app,const std::filesystem::path& target,bool& detached,Ref<ScriptClass>& external)
        : m_App(app),m_Target(target),m_Detached(detached),m_External(external) {}
    void OnAttach() override {
        m_Scene=CreateRef<Scene>(); auto entity=m_Scene->CreateEntityWithUUID(300,"Reload");
        entity.AddComponent<ScriptComponent>().ClassName="Migration.SceneProbe";
        m_Scene->OnRuntimeStart(); m_Previous=ScriptEngine::GetEntityScriptInstance(300);
        m_Previous->SetFieldValue<double>("Precise",17.5);
        // Atomic replacement exercises the real watcher -> Application queue flow.
        auto replacement=m_Target; replacement += ".tmp";
        std::filesystem::copy_file(m_App,replacement);
        std::filesystem::rename(replacement,m_Target);
        m_Started=std::chrono::steady_clock::now();
    }
    void OnUpdate(Timestep) override {
        m_Scene->OnUpdateRuntime(0);
        if(!m_Previous->GetManagedObject()) {
            auto current=ScriptEngine::GetEntityScriptInstance(300);
            Check(current && current->GetFieldValue<double>("Precise")==17.5,"Queued assembly reload lost live fields");
            m_External=current->GetScriptClass();
            Application::Get().Close();
        } else Check(std::chrono::steady_clock::now()-m_Started<std::chrono::seconds(5),"Automatic queued assembly reload timed out");
    }
    void OnDetach() override {
        Check(ScriptEngine::IsInitialized(),"Application destroyed Mono before scene-owning layers");
        m_Scene->OnRuntimeStop(); m_Scene.reset(); m_Previous.reset(); m_Detached=true;
    }
private:
    std::filesystem::path m_App,m_Target;
    bool& m_Detached;
    Ref<ScriptClass>& m_External;
    Ref<Scene> m_Scene;
    Ref<ScriptInstance> m_Previous;
    std::chrono::steady_clock::time_point m_Started;
};
int main(int argc,char** argv) {
    try {
        Log::Init(); Check(argc==3,"Usage: SceneGPUSmoke Core.dll Fixture.dll");
        const auto core=std::filesystem::absolute(std::filesystem::u8path(argv[1]));
        const auto app=std::filesystem::absolute(std::filesystem::u8path(argv[2])); Fixture fixture;
        std::filesystem::create_directories(fixture.Directory/"assets/Scripts");
        std::filesystem::copy_file(core,fixture.Directory/"assets/Scripts/Hazel-ScriptCore.dll");
        const auto target=fixture.Directory/std::filesystem::u8path("Fixture-é.dll"); std::filesystem::copy_file(app,target);
        bool detached=false; Ref<ScriptClass> externalClass;
        {
            ApplicationSpecification spec; spec.Name="Migration Scene"; spec.Resources.Root=fixture.Directory/"assets"; std::filesystem::current_path(fixture.Directory);
            Application application(spec); glfwHideWindow(static_cast<GLFWwindow*>(application.GetWindow().GetNativeWindow()));
            std::cout<<"Renderer: "<<glGetString(GL_RENDERER)<<"; Version: "<<glGetString(GL_VERSION)<<'\n';
            auto project=Project::New(); project->GetConfig().AssetDirectory="assets";
            project->GetConfig().ScriptModulePath=std::filesystem::relative(target,fixture.Directory/"assets");
            Check(Project::SaveActive(fixture.Directory/"Scene.hproj"),"GPU scene project setup failed");
            ScriptEngine::Init();
            FramebufferSpecification fs; fs.Width=256; fs.Height=256;
            fs.Attachments={FramebufferTextureFormat::RGBA8,FramebufferTextureFormat::RED_INTEGER,FramebufferTextureFormat::Depth};
            auto framebuffer=Framebuffer::Create(fs);
            auto source=MakeScene(fixture.Directory);
            const auto file=fixture.Directory/std::filesystem::u8path("scène-é.hazel"); SceneSerializer(source).Serialize(file.generic_u8string());
            auto loaded=CreateRef<Scene>(); loaded->OnViewportResize(256,256);
            Check(SceneSerializer(loaded).Deserialize(file.generic_u8string()),"GPU scene/texture/text YAML round trip failed");
            Check(loaded->GetEntityByUUID(11).GetComponent<SpriteRendererComponent>().Texture->IsLoaded(),"Serialized UTF-8 texture not loaded");
            const auto& text=loaded->GetEntityByUUID(13).GetComponent<TextComponent>();
            Check(text.TextString==u8"Hazel é" && text.Kerning==0.1f && text.LineSpacing==0.2f && text.Color==glm::vec4(0,1,0,1),"Text component serialization failed");
            EditorCamera editor(45,1,0.1f,100); editor.SetViewportSize(256,256);
            Begin(framebuffer); loaded->OnUpdateEditor(0,editor); CheckScenePixels(loaded);
            auto runtime=Scene::Copy(loaded); runtime->OnRuntimeStart(); Begin(framebuffer); runtime->OnUpdateRuntime(1.0f/60); CheckScenePixels(runtime);
            const auto camera=runtime->GetEntityByUUID(10).GetComponent<CameraComponent>().Camera.GetProjection();
            runtime->OnViewportResize(0,0); Check(runtime->GetEntityByUUID(10).GetComponent<CameraComponent>().Camera.GetProjection()==camera,"Minimized scene damaged camera projection");
            Check(framebuffer->ReadPixel(1,192,128)==static_cast<int>(static_cast<uint32_t>(runtime->GetEntityByUUID(12))),"Runtime primary-camera circle picking failed");
            runtime->OnRuntimeStop(); runtime->OnSimulationStart(); Begin(framebuffer); runtime->OnUpdateSimulation(1.0f/60,editor); CheckScenePixels(runtime); runtime->OnSimulationStop();
            framebuffer->Unbind(); framebuffer.reset(); source.reset(); loaded.reset(); runtime.reset(); glEnable(GL_DEPTH_TEST);
            // Exercise the intentional Player/Camera fixtures and every published
            // managed component call in a real Application/input/physics context.
            auto scripts=CreateRef<Scene>();
            auto player=scripts->CreateEntityWithUUID(501,"Player");
            player.AddComponent<Rigidbody2DComponent>().Type=Rigidbody2DComponent::BodyType::Dynamic;
            player.AddComponent<BoxCollider2DComponent>();
            player.AddComponent<ScriptComponent>().ClassName="Regression.Player";
            auto cameraEntity=scripts->CreateEntityWithUUID(500,"Camera");
            cameraEntity.AddComponent<ScriptComponent>().ClassName="Regression.Camera";
            auto probe=scripts->CreateEntityWithUUID(502,"Component probe");
            probe.AddComponent<Rigidbody2DComponent>().Type=Rigidbody2DComponent::BodyType::Dynamic;
            probe.AddComponent<BoxCollider2DComponent>(); probe.AddComponent<TextComponent>();
            probe.AddComponent<ScriptComponent>().ClassName="Migration.ComponentProbe";
            scripts->OnRuntimeStart();
            Check(ScriptEngine::GetEntityScriptInstance(502)->GetFieldValue<bool>("Passed"),"Managed component internal calls failed");
            scripts->OnUpdateRuntime(0.125f);
            Check(ScriptEngine::GetEntityScriptInstance(501)->GetFieldValue<float>("Time")==0.125f,"Regression Player.OnUpdate failed");
            Check(cameraEntity.GetComponent<TransformComponent>().Translation.z==5.0f,"Regression Camera/entity script cast failed");
            Check(probe.GetComponent<TextComponent>().TextString==u8"Hazel é λ", "Managed UTF-8 text setter failed");
            scripts->OnRuntimeStop(); scripts->OnRuntimeStart();
            Check(ScriptEngine::GetEntityScriptInstance(502)->GetFieldValue<bool>("Passed"),"Managed component restart failed");
            scripts->OnRuntimeStop(); scripts.reset();
            externalClass=ScriptEngine::GetEntityClass("Migration.SceneProbe");
            application.PushLayer(CreateScope<ReloadLayer>(app,target,detached,externalClass)); application.Run();
        }
        Check(detached && !Application::TryGet() && !ScriptEngine::IsInitialized() && !externalClass->GetMethod("OnCreate",0),"Application scripting/layer shutdown left stale resources");
        std::cout<<"PASS: full scene editor/runtime/simulation rendering and picking, UTF-8 texture/text serialization, minimized camera, automatic atomic assembly reload and layer/Mono/context shutdown\n";
        return 0;
    } catch(const std::exception& error) { std::cerr<<"FAIL: "<<error.what()<<'\n'; return 1; }
}
