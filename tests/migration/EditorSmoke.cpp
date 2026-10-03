// Real pinned EditorLayer/panels in Application; files and layout are isolated.
#include "EditorLayer.h"
#include "ContentBrowserPayload.h"
#include "Hazel/Scene/SceneSerializer.h"
#include "Hazel/Scripting/ScriptEngine.h"
#include "Hazel/Project/Project.h"
#include "Hazel/Project/ProjectSerializer.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <ImGuizmo.h>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <stdexcept>
#ifdef HZ_PLATFORM_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
namespace Hazel {
static void Check(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
static std::string Read(const std::filesystem::path& file) {
    std::ifstream input(file,std::ios::binary); return {std::istreambuf_iterator<char>(input),{}};
}
class EditorWorkflowSmoke : public Layer {
public:
    EditorWorkflowSmoke(EditorLayer& editor, std::filesystem::path directory, bool& done)
        : m_Editor(editor), m_Directory(std::move(directory)), m_Done(done) {}
    void OnUpdate(Timestep) override {
        auto& e = m_Editor;
        switch (++m_Frame) {
        case 3: {
            Check(e.m_ContentBrowserPanel && ScriptEngine::IsInitialized(), "Editor project/assembly startup failed");
            auto* viewport = ImGui::FindWindowByName("Viewport");
            Check(viewport && viewport->DockId && e.m_ViewportSize.x > 0 && e.m_ViewportSize.y > 0, "Docked editor viewport missing");
            auto oldScene=e.m_ActiveScene;
            auto stale=oldScene->CreateEntity("Retired selection"); e.m_HoveredEntity=stale;
            e.NewScene(); Check(e.m_EditorScene == e.m_ActiveScene, "NewScene did not replace editor scene");
            Check(!e.m_HoveredEntity && !e.m_SceneHierarchyPanel.SetSelectedEntity(stale),"NewScene retained borrowed observations");
            oldScene.reset(); // The next selection arrives before another picking update, with a destroyed Scene pointer.
            Check(!e.m_SceneHierarchyPanel.SetSelectedEntity(stale),"Selection dereferenced a destroyed foreign scene");
            auto camera = e.m_EditorScene->CreateEntityWithUUID(900, "Camera");
            camera.AddComponent<CameraComponent>().Camera.SetOrthographic(4, -1, 1);
            camera.AddComponent<ScriptComponent>().ClassName = "Sandbox.Camera";
            auto player = e.m_EditorScene->CreateEntityWithUUID(901, u8"Player %n é");
            player.AddComponent<SpriteRendererComponent>();
            player.AddComponent<Rigidbody2DComponent>().Type = Rigidbody2DComponent::BodyType::Dynamic;
            player.AddComponent<BoxCollider2DComponent>();
            player.AddComponent<ScriptComponent>().ClassName = "Sandbox.Player";
            auto texture = m_Directory / "SandboxProject/Assets/Textures" / std::filesystem::u8path(u8"texture é 🚀.png");
            std::filesystem::copy_file(m_Directory / "assets/textures/Checkerboard.png", texture);
            const auto payload = texture.lexically_relative(m_Directory).generic_u8string();
            const auto decoded = ContentBrowserPath(payload.c_str(), static_cast<int>(payload.size() + 1));
            Check(decoded == texture.lexically_relative(m_Directory), "Portable content-browser payload lost Unicode path");
            Check(SceneHierarchyPanel::AssignSpriteTexture(player.GetComponent<SpriteRendererComponent>(),decoded),"Drag/drop assignment code failed");
            auto& authored=ScriptEngine::GetScriptFieldMap(player)["Speed"];
            authored.Field={ScriptFieldType::Float,"Speed",nullptr}; authored.SetValue<float>(4.5f);
            auto external=e.m_EditorScene->CreateEntityWithUUID(902,"External texture");
            auto& externalSprite=external.AddComponent<SpriteRendererComponent>();
            Check(SceneHierarchyPanel::AssignSpriteTexture(externalSprite,m_Directory/"assets/textures/Checkerboard.png"),"External assignment failed");
            e.m_SceneHierarchyPanel.SetSelectedEntity(player);
            e.OnDuplicateEntity();
            auto duplicate = e.m_SceneHierarchyPanel.GetSelectedEntity();
            Check(duplicate && duplicate.GetUUID() != player.GetUUID(), "Editor duplication failed");
            e.m_EditorScene->DestroyEntity(duplicate);
            Check(!e.m_SceneHierarchyPanel.GetSelectedEntity(),"Destroyed selection remained valid");
            e.m_HoveredEntity=duplicate;
            Check(!e.m_HoveredEntity,"Destroyed hovered entity remained valid");
            e.m_SceneHierarchyPanel.SetSelectedEntity(player);
            e.m_EditorScenePath = m_Directory / "SandboxProject/Assets/Scenes" / std::filesystem::u8path(u8"workflow-é-🚀.hazel");
            Check(e.SaveScene(),"Editor scene save reported failure");
            Project::GetActive()->GetConfig().StartScene=e.m_EditorScenePath.lexically_relative(std::filesystem::absolute(Project::GetAssetDirectory()));
            Check(e.SaveProject(),"Editor project save reported failure");
            const auto saved=Read(e.m_EditorScenePath);
            Check(saved.find(std::string(u8"Textures/texture é 🚀.png"))!=std::string::npos &&
                  saved.find("SandboxProject/Assets/Textures")==std::string::npos,"Project texture was not asset-root relative");
            Check(saved.find((m_Directory/"assets/textures/Checkerboard.png").generic_u8string())!=std::string::npos,
                  "External absolute reference was discarded");
            break;
        }
        case 4: {
            auto file = e.m_EditorScenePath; e.NewScene(); Check(e.OpenScene(file),"OpenScene reported failure");
            auto player = e.m_EditorScene->GetEntityByUUID(901);
            Check(player && player.GetComponent<SpriteRendererComponent>().Texture->IsLoaded(), "Editor save/reopen/texture failed");
            e.m_SceneHierarchyPanel.SetSelectedEntity(player);
            KeyPressedEvent event(Key::W); e.OnKeyPressed(event);
            Check(e.m_GizmoType == ImGuizmo::TRANSLATE, "Translate shortcut failed");
            e.m_ShowPhysicsColliders = true;
            // Original upstream Windows separators remain readable on Linux too.
            e.m_HoveredEntity=player;
            Check(e.OpenScene(m_Directory/"SandboxProject/Assets/Scenes/Example.hazel"),"Legacy Windows-separated texture reference failed");
            Check(!e.m_HoveredEntity && !e.m_SceneHierarchyPanel.SetSelectedEntity(player),"OpenScene retained retired observations");
            Check(e.OpenScene(file),"Authored scene reopen failed");
            FailureChecks();
            break;
        }
        case 5: {
            KeyPressedEvent event(Key::E); e.OnKeyPressed(event);
            Check(e.m_GizmoType == ImGuizmo::ROTATE, "Rotate shortcut failed");
            break;
        }
        case 6: {
            KeyPressedEvent event(Key::R); e.OnKeyPressed(event);
            Check(e.m_GizmoType == ImGuizmo::SCALE, "Scale shortcut failed");
            auto authored=e.m_EditorScene->GetEntityByUUID(901); e.m_HoveredEntity=authored;
            e.OnScenePlay(); Check(e.m_ActiveScene != e.m_EditorScene && e.m_ActiveScene->IsRunning(), "Editor play failed");
            Check(!e.m_HoveredEntity && !e.m_SceneHierarchyPanel.SetSelectedEntity(authored),"Play retained editor observations");
            e.OnScenePause();
            break;
        }
        case 7:
            Check(ScriptEngine::GetEntityScriptInstance(901)->GetFieldValue<float>("Time") == 0, "Paused editor advanced scripts");
            e.m_ActiveScene->Step(); break;
        case 8:
            Check(ScriptEngine::GetEntityScriptInstance(901)->GetFieldValue<float>("Time") > 0, "Editor single-step failed");
            {
                auto instance=ScriptEngine::GetEntityScriptInstance(901); instance->SetFieldValue<float>("Speed",88);
                Check(e.ReloadScripts(),"Editor valid reload failed");
                Check(ScriptEngine::GetEntityScriptInstance(901)->GetFieldValue<float>("Speed")==88,"Editor reload lost live fields");
                Check(ScriptEngine::GetScriptFieldMap(e.m_EditorScene->GetEntityByUUID(901)).at("Speed").GetValue<float>()==4.5f,"Reload mutated editor defaults");
                FailureChecks();
                e.m_HoveredEntity=e.m_ActiveScene->GetEntityByUUID(901);
                Check(e.SaveScene(),"Save while playing failed"); e.OnSceneStop();
                Check(!e.m_HoveredEntity,"Stop retained runtime hover");
                e.OnScenePlay();
                Check(ScriptEngine::GetEntityScriptInstance(901)->GetFieldValue<float>("Speed")==4.5f,"New Play inherited prior live fields");
                e.OnSceneStop();
            }
            Check(e.m_ActiveScene == e.m_EditorScene && !e.m_ActiveScene->IsRunning(), "Editor stop did not restore editor scene");
            e.m_HoveredEntity=e.m_EditorScene->GetEntityByUUID(901);
            e.OnSceneSimulate();
            Check(!e.m_HoveredEntity,"Simulate retained editor hover"); break;
        case 9:
            Check(e.m_ActiveScene != e.m_EditorScene && e.m_ActiveScene->GetEntityByUUID(901).GetComponent<Rigidbody2DComponent>().RuntimeBody, "Editor simulation failed");
            e.OnSceneStop(); e.OnScenePlay(); e.OnSceneStop();
            e.OnScenePlay();
            {
                auto retired=e.m_ActiveScene; auto instance=ScriptEngine::GetEntityScriptInstance(901);
                e.m_HoveredEntity=retired->GetEntityByUUID(901);
                Check(e.OpenProject(e.m_ProjectPath),"Editor project reopen reported failure");
                Check(!retired->IsRunning() && !instance->GetManagedObject() && !e.m_HoveredEntity,
                      "Project replacement retained runtime/borrowed owners");
            }
            Check(e.m_ContentBrowserPanel && ScriptEngine::IsInitialized(), "Editor project reopen failed");
            {
                const auto relocated=m_Directory/std::filesystem::u8path(u8"Relocated Project é 🚀");
                std::filesystem::copy(m_Directory/"SandboxProject",relocated,std::filesystem::copy_options::recursive);
                const auto original=m_Directory/"SandboxProject";
                const auto parked=m_Directory/"Parked original";
                // Windows cannot rename the watched project's ancestor directory while its watcher is active.
                // Switch to the copy (joining the old watcher), then make the original unavailable and reopen.
                Check(e.OpenProject(relocated/"Sandbox.hproj"),"Copied project initial open failed");
                std::filesystem::rename(original,parked); // Prevent silently resolving against the original project.
                Check(e.OpenProject(relocated/"Sandbox.hproj"),"Relocated project open failed");
                auto texture=e.m_EditorScene->GetEntityByUUID(901).GetComponent<SpriteRendererComponent>().Texture;
                Check(texture && texture->IsLoaded() && std::filesystem::u8path(texture->GetPath())==
                      relocated/"Assets/Textures"/std::filesystem::u8path(u8"texture é 🚀.png"),"Relocation used original asset root");
                Check(e.m_EditorScene->GetEntityByUUID(902).GetComponent<SpriteRendererComponent>().Texture->IsLoaded(),"Relocation discarded external reference");
                std::filesystem::rename(parked,original);
            }
            break;
        case 10:
            Check(glGetError() == GL_NO_ERROR, "Editor/gizmo/panels OpenGL error");
            m_Done = true; Application::Get().Close(); break;
        }
    }
private:
    void FailureChecks() {
        auto& e=m_Editor;
        const auto project=Project::GetActive(); const auto scene=e.m_ActiveScene; const auto authored=e.m_EditorScene;
        const auto state=e.m_SceneState; const auto projectPath=e.m_ProjectPath; const auto scenePath=e.m_EditorScenePath;
        const auto speed=ScriptEngine::GetScriptFieldMap(authored->GetEntityByUUID(901)).at("Speed").GetValue<float>();
        const auto instance=ScriptEngine::GetEntityScriptInstance(901);
        auto preserved=[&]() {
            Check(Project::GetActive()==project && e.m_ActiveScene==scene && e.m_EditorScene==authored &&
                  e.m_SceneState==state && e.m_ProjectPath==projectPath && e.m_EditorScenePath==scenePath,
                  "Failed action replaced valid editor session");
            Check(ScriptEngine::GetScriptFieldMap(authored->GetEntityByUUID(901)).at("Speed").GetValue<float>()==speed,
                  "Failed action changed authored fields");
            Check(!e.m_ActionError.empty(),"Failed action did not report actionable error");
            if(instance) Check(instance->GetManagedObject() && ScriptEngine::GetEntityScriptInstance(901)==instance,
                               "Failed action destroyed live managed domain");
        };
        const auto badProject=projectPath.parent_path()/"bad.hproj";
        const auto badScene=projectPath.parent_path()/"broken.hazel";
        std::ofstream(badProject)<<"Project: [unterminated";
        std::ofstream(badScene)<<"Scene: [unterminated";
        for(const auto& path:{badProject,projectPath.parent_path()/"missing.hproj"}) {
            Check(!e.OpenProject(path),"Invalid project reported success"); preserved();
        }
        for(const auto& path:{badScene,projectPath.parent_path()/"missing.hazel",badProject}) {
            Check(!e.OpenScene(path),"Invalid scene reported success"); preserved();
        }
        const auto missingTexture=projectPath.parent_path()/"missing-texture.hazel";
        std::ofstream(missingTexture)<<"Scene: Missing texture\nEntities:\n  - Entity: 901\n    SpriteRendererComponent:\n      Color: [1, 1, 1, 1]\n      TexturePath: Textures/missing.png\n      TilingFactor: 1\n";
        Check(!e.OpenScene(missingTexture),"Missing texture scene reported success"); preserved();
        auto candidate=CreateRef<Project>(); candidate->GetConfig()=project->GetConfig();
        for(bool corrupt:{false,true}) {
            candidate->GetConfig().StartScene=corrupt ? std::filesystem::absolute(badScene) : std::filesystem::path("Scenes/missing.hazel");
            Check(ProjectSerializer(candidate).Serialize(badProject),"Failure project setup failed");
            Check(!e.OpenProject(badProject),"Invalid start scene reported success"); preserved();
        }
        candidate->GetConfig()=project->GetConfig(); candidate->GetConfig().AssetDirectory="missing-assets";
        Check(ProjectSerializer(candidate).Serialize(badProject),"Missing asset setup failed");
        Check(!e.OpenProject(badProject),"Missing assets reported success"); preserved();
        candidate->GetConfig()=project->GetConfig(); candidate->GetConfig().ScriptModulePath="Scripts/missing.dll";
        Check(ProjectSerializer(candidate).Serialize(badProject),"Missing assembly setup failed");
        Check(!e.OpenProject(badProject),"Missing assembly reported success"); preserved();
        candidate->GetConfig()=project->GetConfig();
        Check(ProjectSerializer(candidate).Serialize(badProject),"Corrupt assembly setup failed");
        const auto assembly=Project::GetAssetFileSystemPath(project->GetConfig().ScriptModulePath);
        const auto backup=m_Directory/"valid-scripts.dll";
        std::filesystem::copy_file(assembly,backup,std::filesystem::copy_options::overwrite_existing);
        std::ofstream(assembly,std::ios::binary|std::ios::trunc)<<"corrupt assembly";
        Check(!e.OpenProject(badProject),"Corrupt assembly project reported success"); preserved();
        Check(!e.ReloadScripts(),"Corrupt reload reported success"); preserved();
        std::filesystem::copy_file(backup,assembly,std::filesystem::copy_options::overwrite_existing);
        const auto core=m_Directory/"assets/Scripts/Hazel-ScriptCore.dll";
        const auto coreBackup=m_Directory/"valid-core.dll";
        std::filesystem::copy_file(core,coreBackup,std::filesystem::copy_options::overwrite_existing);
        std::filesystem::remove(core);
        Check(!e.OpenProject(badProject),"Missing ScriptCore reported success"); preserved();
        std::ofstream(core,std::ios::binary|std::ios::trunc)<<"corrupt ScriptCore";
        Check(!e.OpenProject(badProject),"Corrupt ScriptCore reported success"); preserved();
        // A valid managed image with the wrong ScriptCore API must also fail before commitment.
        std::filesystem::copy_file(backup,core,std::filesystem::copy_options::overwrite_existing);
        Check(!e.OpenProject(badProject),"Incompatible ScriptCore reported success"); preserved();
        std::filesystem::copy_file(coreBackup,core,std::filesystem::copy_options::overwrite_existing);
        const auto priorScene=Read(scenePath), priorProject=Read(projectPath);
        Check(!e.SerializeScene(authored,m_Directory/"missing-dir/save.hazel"),"Failed scene save reported success"); preserved();
        const auto blocked=m_Directory/"blocked.hazel";
        std::filesystem::create_directories(blocked); std::ofstream(blocked/"sentinel")<<"intact";
        Check(!e.SerializeScene(authored,blocked) && Read(blocked/"sentinel")=="intact","Failed scene replacement damaged destination"); preserved();
        e.m_ProjectPath=m_Directory/"missing-dir/save.hproj";
        Check(!e.SaveProject(),"Failed project save reported success"); e.m_ProjectPath=projectPath; preserved();
        Check(Read(scenePath)==priorScene && Read(projectPath)==priorProject,"Failed save changed prior scene/project bytes");
    }
    EditorLayer& m_Editor;
    std::filesystem::path m_Directory;
    bool& m_Done;
    int m_Frame = 0;
};
}
int main(int argc, char** argv) {
    using namespace Hazel;
    const auto previous = std::filesystem::current_path();
    const auto directory = std::filesystem::temp_directory_path() / std::filesystem::u8path("hazel-editor-é-" + std::to_string(std::random_device{}()));
    try {
        Log::Init(); Check(argc == 4, "Usage: EditorSmoke Core.dll Sandbox.dll Hazelnut-source-dir");
        auto core = std::filesystem::absolute(std::filesystem::u8path(argv[1]));
        auto scripts = std::filesystem::absolute(std::filesystem::u8path(argv[2]));
        auto source = std::filesystem::absolute(std::filesystem::u8path(argv[3]));
        std::filesystem::create_directory(directory);
        std::filesystem::create_directories(directory / "assets/Icons");
        for (const char* name : { "assets", "Resources/Icons", "SandboxProject" })
            std::filesystem::copy(source / name, directory / (std::string(name)=="Resources/Icons" ? "assets/Icons" : name), std::filesystem::copy_options::recursive);
        std::filesystem::copy_file(source / "imgui.ini", directory / "assets/imgui.ini");
        std::filesystem::create_directories(directory / "assets/Scripts");
        std::filesystem::create_directories(directory / "SandboxProject/Assets/Scripts/Binaries");
        std::filesystem::copy_file(core, directory / "assets/Scripts/Hazel-ScriptCore.dll");
        std::filesystem::copy_file(scripts, directory / "SandboxProject/Assets/Scripts/Binaries/Sandbox.dll", std::filesystem::copy_options::overwrite_existing);
        auto project = directory / "SandboxProject/Sandbox.hproj";
        auto projectArgument = project.lexically_relative(directory).generic_u8string();
        char executable[] = "EditorSmoke"; char* arguments[] = { executable, projectArgument.data() };
        bool done = false;
        {
            ApplicationSpecification spec; spec.Name = "Migration Editor";
            spec.Resources.Root = directory / "assets"; std::filesystem::current_path(directory); spec.CommandLineArgs = { 2, arguments };
            Application application(spec);
            glfwHideWindow(static_cast<GLFWwindow*>(application.GetWindow().GetNativeWindow()));
            std::cout << "Renderer: " << glGetString(GL_RENDERER) << "; Version: " << glGetString(GL_VERSION) << '\n';
            auto editor = CreateScope<EditorLayer>(); auto* observer = editor.get();
            application.PushLayer(std::move(editor));
            application.PushLayer(CreateScope<EditorWorkflowSmoke>(*observer, directory, done));
            application.Run();
        }
        Check(done && !ScriptEngine::IsInitialized() && !Application::TryGet(), "Editor workflow/shutdown failed");
        std::filesystem::current_path(previous);
#ifdef HZ_PLATFORM_WINDOWS
        // Diagnose the reproduced Windows-only cleanup lock separately from the
        // actual editor/shutdown assertions; keep deletion strict for this probe.
        std::error_code cleanupError; std::filesystem::remove_all(directory, cleanupError);
        if (cleanupError) {
            wchar_t nativeDirectory[32768]{};
            GetCurrentDirectoryW(32768, nativeDirectory);
            std::cerr << "Cleanup diagnostic: error=" << cleanupError.value()
                << "; cwd=" << std::filesystem::current_path().generic_u8string()
                << "; native-cwd=" << std::filesystem::path(nativeDirectory).generic_u8string() << '\n';
            std::error_code scanError;
            for (auto it = std::filesystem::recursive_directory_iterator(directory, scanError);
                 !scanError && it != std::filesystem::recursive_directory_iterator(); it.increment(scanError))
                std::cerr << "Remaining fixture: " << it->path().generic_u8string() << '\n';
            throw std::filesystem::filesystem_error("Editor fixture cleanup", directory, cleanupError);
        }
#else
        std::filesystem::remove_all(directory);
#endif
        std::cout << "PASS: actual docked editor/panels, Unicode content paths, save/reopen/project, duplicate, gizmo shortcuts/drawing, play/pause/step/simulate/stop and shutdown\n";
        return 0;
    } catch (const std::exception& error) {
        std::error_code ignored; std::filesystem::current_path(previous, ignored);
        std::filesystem::remove_all(directory, ignored);
        std::cerr << "FAIL: " << error.what() << '\n'; return 1;
    }
}
