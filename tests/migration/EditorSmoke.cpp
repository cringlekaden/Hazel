// Real pinned EditorLayer/panels in Application; files and layout are isolated.
#include "EditorLayer.h"
#include "ContentBrowserPayload.h"
#include "Hazel/Scene/SceneSerializer.h"
#include "Hazel/Scripting/ScriptEngine.h"
#include "Hazel/Project/Project.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <ImGuizmo.h>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <filesystem>
#include <iostream>
#include <random>
#include <stdexcept>
#ifdef HZ_PLATFORM_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
namespace Hazel {
static void Check(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
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
            e.NewScene(); Check(e.m_EditorScene == e.m_ActiveScene, "NewScene did not replace editor scene");
            auto camera = e.m_EditorScene->CreateEntityWithUUID(900, "Camera");
            camera.AddComponent<CameraComponent>().Camera.SetOrthographic(4, -1, 1);
            camera.AddComponent<ScriptComponent>().ClassName = "Sandbox.Camera";
            auto player = e.m_EditorScene->CreateEntityWithUUID(901, u8"Player %n é");
            player.AddComponent<SpriteRendererComponent>();
            player.AddComponent<Rigidbody2DComponent>().Type = Rigidbody2DComponent::BodyType::Dynamic;
            player.AddComponent<BoxCollider2DComponent>();
            player.AddComponent<ScriptComponent>().ClassName = "Sandbox.Player";
            auto texture = m_Directory / "SandboxProject/Assets/Textures" / std::filesystem::u8path(u8"texture-é-🚀.png");
            std::filesystem::copy_file(m_Directory / "assets/textures/Checkerboard.png", texture);
            const auto payload = texture.generic_u8string();
            const auto decoded = ContentBrowserPath(payload.c_str(), static_cast<int>(payload.size() + 1));
            Check(decoded == texture, "Portable content-browser payload lost Unicode path");
            player.GetComponent<SpriteRendererComponent>().Texture = Texture2D::Create(decoded.generic_u8string());
            e.m_SceneHierarchyPanel.SetSelectedEntity(player);
            e.OnDuplicateEntity();
            auto duplicate = e.m_SceneHierarchyPanel.GetSelectedEntity();
            Check(duplicate && duplicate.GetUUID() != player.GetUUID(), "Editor duplication failed");
            e.m_EditorScene->DestroyEntity(duplicate);
            e.m_SceneHierarchyPanel.SetSelectedEntity(player);
            e.m_EditorScenePath = m_Directory / "SandboxProject/Assets/Scenes" / std::filesystem::u8path(u8"workflow-é-🚀.hazel");
            e.SaveScene(); e.SaveProject();
            break;
        }
        case 4: {
            auto file = e.m_EditorScenePath; e.NewScene(); e.OpenScene(file);
            auto player = e.m_EditorScene->GetEntityByUUID(901);
            Check(player && player.GetComponent<SpriteRendererComponent>().Texture->IsLoaded(), "Editor save/reopen/texture failed");
            e.m_SceneHierarchyPanel.SetSelectedEntity(player);
            KeyPressedEvent event(Key::W); e.OnKeyPressed(event);
            Check(e.m_GizmoType == ImGuizmo::TRANSLATE, "Translate shortcut failed");
            e.m_ShowPhysicsColliders = true;
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
            e.OnScenePlay(); Check(e.m_ActiveScene != e.m_EditorScene && e.m_ActiveScene->IsRunning(), "Editor play failed");
            e.OnScenePause();
            break;
        }
        case 7:
            Check(ScriptEngine::GetEntityScriptInstance(901)->GetFieldValue<float>("Time") == 0, "Paused editor advanced scripts");
            e.m_ActiveScene->Step(); break;
        case 8:
            Check(ScriptEngine::GetEntityScriptInstance(901)->GetFieldValue<float>("Time") > 0, "Editor single-step failed");
            e.SaveScene(); e.OnSceneStop();
            Check(e.m_ActiveScene == e.m_EditorScene && !e.m_ActiveScene->IsRunning(), "Editor stop did not restore editor scene");
            e.OnSceneSimulate(); break;
        case 9:
            Check(e.m_ActiveScene != e.m_EditorScene && e.m_ActiveScene->GetEntityByUUID(901).GetComponent<Rigidbody2DComponent>().RuntimeBody, "Editor simulation failed");
            e.OnSceneStop(); e.OnScenePlay(); e.OnSceneStop();
            e.OpenProject(e.m_ProjectPath);
            Check(e.m_ContentBrowserPanel && ScriptEngine::IsInitialized(), "Editor project reopen failed");
            break;
        case 10:
            Check(glGetError() == GL_NO_ERROR, "Editor/gizmo/panels OpenGL error");
            m_Done = true; Application::Get().Close(); break;
        }
    }
private:
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
        std::filesystem::create_directories(directory / "Resources");
        for (const char* name : { "assets", "Resources/Icons", "SandboxProject" })
            std::filesystem::copy(source / name, directory / name, std::filesystem::copy_options::recursive);
        std::filesystem::copy_file(source / "imgui.ini", directory / "imgui.ini");
        std::filesystem::create_directories(directory / "Resources/Scripts");
        std::filesystem::create_directories(directory / "SandboxProject/Assets/Scripts/Binaries");
        std::filesystem::copy_file(core, directory / "Resources/Scripts/Hazel-ScriptCore.dll");
        std::filesystem::copy_file(scripts, directory / "SandboxProject/Assets/Scripts/Binaries/Sandbox.dll", std::filesystem::copy_options::overwrite_existing);
        auto project = directory / "SandboxProject" / std::filesystem::u8path(u8"project-é-🚀.hproj");
        std::filesystem::copy_file(directory / "SandboxProject/Sandbox.hproj", project);
        auto projectArgument = project.generic_u8string();
        char executable[] = "EditorSmoke"; char* arguments[] = { executable, projectArgument.data() };
        bool done = false;
        {
            ApplicationSpecification spec; spec.Name = "Migration Editor";
            spec.WorkingDirectory = directory.generic_u8string(); spec.CommandLineArgs = { 2, arguments };
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
