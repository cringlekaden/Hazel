#include "Hazel.h"
#include "Hazel/Scene/RuntimeSession.h"
#include "Hazel/Scene/SceneSerializer.h"
#include "Hazel/Scripting/ScriptEngine.h"
#include <GLFW/glfw3.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <stdexcept>
using namespace Hazel;
static void Check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
class DestroyRequest : public ScriptableEntity {
public:
    static RuntimeSession* Session;
    static bool Rejected;
    void OnDestroy() override { Rejected = !Session->RequestSceneLoad("Scenes/Target.hazel"); }
};
RuntimeSession* DestroyRequest::Session = nullptr;
bool DestroyRequest::Rejected = false;
static Ref<Scene> Make(const char* name, const char* script = nullptr) {
    auto scene = CreateRef<Scene>();
    auto camera = scene->CreateEntityWithUUID(1, "Camera");
    camera.AddComponent<CameraComponent>().Camera.SetOrthographic(10, -10, 10);
    auto entity = scene->CreateEntityWithUUID(2, name);
    entity.AddComponent<Rigidbody2DComponent>().Type=Rigidbody2DComponent::BodyType::Dynamic;
    entity.AddComponent<BoxCollider2DComponent>();
    if (script) entity.AddComponent<ScriptComponent>().ClassName=script;
    return scene;
}
int main(int argc, char** argv) {
    const auto directory = std::filesystem::temp_directory_path() / std::filesystem::u8path("hazel-runtime space-é-"+std::to_string(std::random_device{}()));
    try {
        Log::Init(); Check(argc==3, "Usage: RuntimeSessionSmoke Core.dll Fixture.dll");
        std::filesystem::create_directories(directory/"Assets/Scenes");
        ApplicationSpecification spec; spec.Name="Runtime regression"; spec.EnableImGui=false;
        Application app(spec);
        glfwHideWindow(static_cast<GLFWwindow*>(app.GetWindow().GetNativeWindow()));
        auto project=Project::New(); project->GetConfig().AssetDirectory="Assets";
        project->GetConfig().ScriptModulePath="Fixture.dll";
        Check(Project::SaveActive(directory/"Runtime.hproj"),"Cannot save runtime fixture");
        auto resources=Resources::Get();
        resources.Root=directory/"Resources";
        std::filesystem::create_directories(resources.Root/"Scripts");
        std::filesystem::copy_file(std::filesystem::u8path(argv[1]),resources.Root/"Scripts/Hazel-ScriptCore.dll");
        Resources::Configure(resources);
        ScriptEngine::Init(std::filesystem::u8path(argv[2]));
        auto source=Make("Source", "Migration.TransitionOnCreate"), target=Make("Target", "Migration.SceneProbe");
        SceneSerializer(target,project->GetAssetRoot()).Serialize((directory/"Assets/Scenes/Target.hazel").generic_u8string());
        RuntimeSession session; session.Resize(800,400);
        session.Start(project,source);
        const auto identity=session.GetIdentity();
        auto retired=session.GetScene(); auto instance=ScriptEngine::GetEntityScriptInstance(2);
        auto scriptClass=instance->GetScriptClass();
        Check(retired!=source && retired->FindEntityByName("Source"),"Runtime did not copy authored scene");
        Check(instance->GetFieldValue<int>("Creates")==1,"OnCreate request did not return to callback");
        session.Update(0.016f);
        Check(session.GetScene()!=retired && session.GetScene()->FindEntityByName("Target"),"OnCreate transition failed");
        Check(!retired->IsRunning() && !retired->GetEntityByUUID(2).GetComponent<Rigidbody2DComponent>().RuntimeBody && !instance->GetManagedObject(),"Transition retained old scripts/physics");
        Check(ScriptEngine::GetEntityClass("Migration.TransitionOnCreate")==scriptClass,"Ordinary scene transition replaced script domain");
        Check(session.GetIdentity()==identity && ScriptEngine::GetSceneContext()==session.GetScene().get(),"Session identity/script scene context mismatch");
        auto current=session.GetScene(); auto currentInstance=ScriptEngine::GetEntityScriptInstance(2);
        const auto policy=project->GetConfig().Rendering;
        RuntimeRendererRequests incompatible;incompatible.TextureSlots=2;project->GetConfig().Rendering=incompatible;
        bool rejectedPolicy=false;try{session.Start(project,source);}catch(const std::exception&){rejectedPolicy=true;}
        Check(rejectedPolicy && session.GetScene()==current && current->IsRunning() &&
              ScriptEngine::GetEntityScriptInstance(2)==currentInstance && session.GetIdentity()==identity,
              "Unapplied project policy replaced a valid runtime session");
        project->GetConfig().Rendering=policy;
        void* body=current->GetEntityByUUID(2).GetComponent<Rigidbody2DComponent>().RuntimeBody;
        Check(session.RequestSceneLoad("Scenes/missing.hazel"),"Missing scene request was not deferred");
        session.Update(0.016f);
        Check(session.GetScene()==current && current->IsRunning() && ScriptEngine::GetEntityScriptInstance(2)==currentInstance && body==current->GetEntityByUUID(2).GetComponent<Rigidbody2DComponent>().RuntimeBody && !session.GetError().empty(),"Failed transition damaged current session");
        std::ofstream(directory/"Assets/Scenes/Bad.hazel")<<"Scene: Bad\nEntities: [invalid]\n";
        session.RequestSceneLoad("Scenes/Bad.hazel"); session.Update(0.016f);
        Check(session.GetScene()==current,"Corrupt scene replaced current scene");
        auto badPhysics=Make("Invalid shape");
        badPhysics->GetEntityByUUID(2).GetComponent<BoxCollider2DComponent>().Size={-1,1};
        SceneSerializer(badPhysics,project->GetAssetRoot()).Serialize((directory/"Assets/Scenes/InvalidPhysics.hazel").generic_u8string());
        session.RequestSceneLoad("Scenes/InvalidPhysics.hazel");session.Update(0.016f);
        Check(session.GetScene()==current && current->IsRunning(),"Invalid physics replaced/aborted current session");
        Check(!session.RequestSceneLoad("../outside.hazel") && !session.RequestSceneLoad("/tmp/outside.hazel"),"Transition accepted non-project path");
        session.SetInput({400,200},true); glm::vec2 mouse;
        Check(session.GetMouseWorldPosition(mouse) && glm::length(mouse)<1e-4f,"Viewport center mapping failed");
        session.SetInput({600,100},true);
        Check(session.GetMouseWorldPosition(mouse) && std::abs(mouse.x-5)<1e-4f && std::abs(mouse.y-2.5f)<1e-4f,"Viewport scale mapping failed");
        session.Resize(400,400); session.SetInput({300,100},true);
        Check(session.GetMouseWorldPosition(mouse) && std::abs(mouse.x-2.5f)<1e-4f,"Resized viewport mapping failed");
        session.SetInput({300,100},false); Check(!session.GetMouseWorldPosition(mouse),"Inactive viewport accepts clicks");
        session.RequestSceneLoad("Scenes/Target.hazel"); session.Stop();
        Check(!session.RequestSceneLoad("Scenes/Target.hazel") && !ScriptEngine::GetRuntimeSession(),"Stopped session retained request binding");
        auto update=Make("Update", "Migration.TransitionOnUpdate");
        session.Start(project,update);
        auto updating=session.GetScene(); session.Update(0.016f);
        Check(session.GetScene()==updating && ScriptEngine::GetEntityScriptInstance(2)->GetFieldValue<int>("Updates")==1,"Update request destroyed executing scene");
        session.Update(0.016f); Check(session.GetScene()->FindEntityByName("Target"),"First conflicting request did not win");
        auto reloading = session.GetScene();
        auto oldReloadInstance = ScriptEngine::GetEntityScriptInstance(2);
        oldReloadInstance->SetFieldValue<float>("Speed", 19.0f);
        session.RequestSceneLoad("Scenes/Target.hazel");
        ScriptEngine::ReloadAssembly();
        Check(session.GetScene()==reloading && ScriptEngine::GetRuntimeSession()==&session &&
              ScriptEngine::GetEntityScriptInstance(2)->GetFieldValue<float>("Speed")==19.0f && !oldReloadInstance->GetManagedObject(),
              "Assembly reload lost session binding/live fields or retained old handles");
        session.Update(0.016f);
        Check(session.GetScene()!=reloading && ScriptEngine::GetEntityScriptInstance(2)->GetFieldValue<float>("Speed")==2.5f,
              "Pending transition was lost on reload or inherited retired live fields");
        for (int repeat=0;repeat<12;++repeat) {
            retired=session.GetScene(); instance=ScriptEngine::GetEntityScriptInstance(2);
            session.RequestSceneLoad("Scenes/Target.hazel"); session.Update(0.016f);
            Check(!retired->IsRunning() && !retired->GetEntityByUUID(2).GetComponent<Rigidbody2DComponent>().RuntimeBody && !instance->GetManagedObject(),"Repeated transition retained runtime owners");
        }
        session.Stop();
        auto native=Make("Native");native->GetEntityByUUID(2).AddComponent<NativeScriptComponent>().Bind<DestroyRequest>();
        DestroyRequest::Session=&session;session.Start(project,native);session.Update(0.016f);session.Stop();
        Check(DestroyRequest::Rejected,"Shutdown callback scheduled retired work");
        session.Start(project,source); session.Stop(); session.Start(project,target); session.Update(0.016f);
        Check(session.GetScene()->FindEntityByName("Target"),"Retired OnCreate request survived restart");
        session.Stop();
        auto motion=Make("Motion", "Migration.MotionCameraProbe");
        motion->GetEntityByUUID(2).GetComponent<Rigidbody2DComponent>().GravityScale=0;
        SceneSerializer(motion,project->GetAssetRoot()).Serialize((directory/"Assets/Scenes/Motion.hazel").generic_u8string());
        motion=project->LoadScene("Scenes/Motion.hazel");
        Check(motion->GetEntityByUUID(2).GetComponent<Rigidbody2DComponent>().GravityScale==0,"Authored gravity did not serialize");
        session.Resize(800,400);session.Start(project,motion);
        Check(ScriptEngine::GetEntityScriptInstance(2)->GetFieldValue<bool>("Passed"),"Managed motion/camera API failed");
        session.Update(0.016f);
        const auto position=session.GetScene()->GetEntityByUUID(2).GetComponent<TransformComponent>().Translation;
        Check(std::abs(position.x-3.032f)<0.001f && std::abs(position.y-4)<0.001f,"Physics ignored teleport, velocity or zero gravity");
        session.Resize(400,800);
        Check(session.GetScene()->GetPrimaryCameraEntity().GetComponent<CameraComponent>().Camera.GetAspectRatio()==0.5f,"Camera aspect did not follow viewport resize");
        session.Stop();
        Check(motion->GetEntityByUUID(2).GetComponent<TransformComponent>().Translation.x==0 &&
              motion->GetPrimaryCameraEntity().GetComponent<CameraComponent>().Camera.GetOrthographicSize()==10,
              "Runtime motion/camera edits leaked into authored scene");
        ScriptEngine::Shutdown(); Check(!session.GetScene() && !ScriptEngine::GetRuntimeSession(),"Domain shutdown retained runtime session");
        std::cout<<"PASS: shared runtime OnCreate/OnUpdate transitions, failure recovery, first request wins, cancellation, authored copy, retained domain, retired observations/physics, repeated transitions and viewport input\n";
    } catch (const std::exception& error) { std::cerr<<"FAIL: "<<error.what()<<'\n'; std::error_code ignored; std::filesystem::remove_all(directory,ignored);return 1; }
    std::error_code ignored;std::filesystem::remove_all(directory,ignored);return 0;
}
