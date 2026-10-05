#include "AuthoringPanel.h"
#include "EditorLayer.h"
#include "Hazel/Project/ProjectSerializer.h"
#include "Hazel/Project/ScriptSource.h"
#include "Hazel/Renderer/Renderer.h"
#include "Hazel/Scene/Prefab.h"
#include "Hazel/Scene/SceneSerializer.h"
#include "Hazel/Scripting/ScriptEngine.h"
#include "Hazel/Utils/PlatformUtils.h"
#include "Hazel/Utils/Process.h"
#include "UI/PropertyUI.h"
#include <ImGuizmo.h>
#include <algorithm>
#include <cctype>
#include <glm/gtc/type_ptr.hpp>
#include <imgui.h>
#include <imgui_internal.h>
#include <misc/cpp/imgui_stdlib.h>
namespace Hazel
{
static std::filesystem::path Path(const std::string &value) { return std::filesystem::u8path(value); }
static void Browse(std::string &value, bool folder)
{
    PropertyUI::WrapButton("Browse");
    if (ImGui::Button("Browse"))
    {
        auto chosen = folder ? FileDialogs::SelectFolder() : FileDialogs::OpenFile("All files\0*\0");
        if (!chosen.empty())
            value = chosen;
    }
}
static void PathInput(const char *label, std::string &value, bool folder)
{
    PropertyUI::Row row(label, label);
    ImGui::SetNextItemWidth(std::max(1.f, ImGui::GetContentRegionAvail().x - ImGui::GetFontSize() * 5));
    ImGui::InputText("##path", &value);
    Browse(value, folder);
}
static bool Portable(const std::string &value)
{
    auto path = Project::NormalizeAssetPath(Path(value)).lexically_normal();
    return !value.empty() && !path.is_absolute() && value.find(':') == std::string::npos &&
           *path.begin() != "..";
}
static bool Contained(const std::filesystem::path &root, const std::filesystem::path &path)
{
    std::error_code error;
    auto absolute = std::filesystem::weakly_canonical(path, error);
    if (error)
        return false;
    auto base = std::filesystem::weakly_canonical(root, error);
    if (error)
        return false;
    auto relative = absolute.lexically_relative(base);
    return !relative.empty() && *relative.begin() != "..";
}
AuthoringPanel::AuthoringPanel(EditorLayer &editor) : m_Editor(editor)
{
    m_Preferences = EditorPreferences::Load(m_Output);
    m_PreferenceRecovery = m_Output;
    m_Draft = m_Preferences;
    ImGui::GetIO().FontGlobalScale = m_Preferences.UIScale;
    m_Editor.m_ShowPhysicsColliders = m_Preferences.ShowColliders;
    auto &style = ImGui::GetStyle();
    style.FramePadding = {6, 4};
    style.ItemSpacing = {6, 6};
    style.CellPadding = {4, 3};
    style.FrameRounding = 2;
    style.GrabRounding = 2;
    style.Colors[ImGuiCol_HeaderActive] = {.20f, .30f, .38f, 1};
    if (!m_Output.empty())
        m_ShowOutput = true;
}
ActionAvailability AuthoringPanel::Availability(EditorAction action) const
{
    return EditorActionAvailability({static_cast<EditorMode>(m_Editor.m_SceneState), m_Tools.Busy(),
                                     bool(Project::GetActive()), bool(m_Editor.m_EditorScene),
                                     m_Documents.Pending() && !m_ResolvingDocumentAction},
                                    action);
}
bool AuthoringPanel::Require(EditorAction action)
{
    auto availability = Availability(action);
    return availability ? true : m_Editor.ActionFailed(availability.Reason);
}
void AuthoringPanel::MarkSceneSaved() { m_SavedScene = SceneText(); }
std::string AuthoringPanel::SceneText() const
{
    return SceneSerializer(m_Editor.m_EditorScene).SerializeAuthoredSnapshot();
}
void AuthoringPanel::RememberProject()
{
    m_Preferences.Remember(m_Editor.m_ProjectPath);
    if (!m_PreferenceRecovery.empty())
        return; // Keep malformed/future preferences until an explicit Apply and
                // Save.
    try
    {
        m_Preferences.Save();
    }
    catch (const std::exception &error)
    {
        m_Editor.ActionFailed(error.what());
    }
}
void AuthoringPanel::BindProject()
{
    auto availability = [this](EditorAction action) { return Availability(action); };
    m_Editor.m_SceneHierarchyPanel.Availability = availability;
    m_PrefabInspector.Availability = availability;
    m_PrefabInspector.EditOperation = EditorAction::EditAsset;
    if (m_Editor.m_ContentBrowserPanel)
        m_Editor.m_ContentBrowserPanel->Availability = availability;
    m_Editor.m_SceneHierarchyPanel.OpenAsset = [this](const auto &path) { SelectAsset(path); };
    m_PrefabInspector.OpenAsset = m_Editor.m_SceneHierarchyPanel.OpenAsset;
    if (m_Editor.m_ContentBrowserPanel)
        m_Editor.m_ContentBrowserPanel->SelectAsset = [this](auto &path) { SelectAsset(path); };
    if (Project::GetActive())
        m_Sprites.Bind(Project::GetActive()->GetAssets());
    m_Sprites.ReportError = [this](const std::string &error) { m_Editor.ActionFailed(error); };
    m_Sprites.Availability = availability;
    m_Sprites.AssetsChanged = [this]
    {
        if (m_Editor.m_ContentBrowserPanel)
            m_Editor.m_ContentBrowserPanel->Refresh();
    };
    m_Sprites.CaptureTarget = [this] { return AssignmentTarget(); };
    m_Sprites.TargetName = [this]
    {
        const auto target = AssignmentTarget();
        auto entity = target.Entity ? m_Editor.m_EditorScene->GetEntityByUUID(target.Entity) : Entity{};
        return entity ? entity.GetName() : std::string{};
    };
    m_Sprites.RequestClose = [this]
    {
        Guard(OperationIntent::CloseSheet,
              [this]
              {
                  m_Sprites.Close();
                  if (m_ActiveDocument == EditorDocument::Sheet)
                      m_ActiveDocument = EditorDocument::Scene;
                  m_ProjectChoicesLoaded = false;
                  return true;
              });
    };
    if (m_Editor.m_ContentBrowserPanel)
        m_Editor.m_ContentBrowserPanel->CreateSpriteSheet = [this](const auto &path)
        {
            if (Require(EditorAction::EditAsset))
                m_Sprites.BeginCreate(path);
        };
    if (m_Editor.m_ContentBrowserPanel)
        m_Editor.m_ContentBrowserPanel->ImportTexture = [this]
        {
            if (Require(EditorAction::EditAsset))
                m_Sprites.BeginImport();
        };
    m_Sprites.ReplaceDocument = [this](std::function<bool()> action)
    { Guard(OperationIntent::OpenSheet, std::move(action)); };
    m_Sprites.GuardPending = [this] { return m_Documents.Pending(); };
    m_Sprites.AssignSprite = [this](SpriteReference reference, SceneTarget target)
    {
        try
        {
            auto entity = ResolveTarget(target);
            if (!entity)
                return false;
            Project::GetActive()->GetAssets()->Resolve(reference);
            entity = ResolveTarget(target);
            if (!entity)
                return false;
            if (!entity.HasComponent<SpriteRendererComponent>())
                entity.AddComponent<SpriteRendererComponent>();
            entity.GetComponent<SpriteRendererComponent>().Source = reference;
            m_Editor.m_ActionError.clear();
            return true;
        }
        catch (const std::exception &error)
        {
            return m_Editor.ActionFailed(error.what());
        }
    };
    m_Sprites.AssignClip = [this](AnimationReference reference, SceneTarget target)
    {
        try
        {
            auto entity = ResolveTarget(target);
            if (!entity)
                return false;
            Project::GetActive()->GetAssets()->Clip(reference);
            entity = ResolveTarget(target);
            if (!entity)
                return false;
            if (!entity.HasComponent<SpriteRendererComponent>())
                entity.AddComponent<SpriteRendererComponent>();
            if (!entity.HasComponent<SpriteAnimationComponent>())
                entity.AddComponent<SpriteAnimationComponent>();
            entity.GetComponent<SpriteAnimationComponent>().DefaultClip = reference;
            entity.GetComponent<SpriteAnimationComponent>().ResetRuntime();
            m_Editor.m_ActionError.clear();
            return true;
        }
        catch (const std::exception &error)
        {
            return m_Editor.ActionFailed(error.what());
        }
    };
    m_Editor.m_SceneHierarchyPanel.ReportError = [this](const std::string &error)
    { m_Editor.ActionFailed(error); };
    m_Editor.m_SceneHierarchyPanel.CreatePrefab = [this](auto entity) { CreatePrefab(entity); };
    m_Editor.m_SceneHierarchyPanel.EditScript = [this](auto &name)
    {
        try
        {
            OpenScript(ScriptSource::Find(Project::GetAssetDirectory(), name));
        }
        catch (const std::exception &error)
        {
            m_Editor.ActionFailed(error.what());
        }
    };
    m_PrefabInspector.SetContext(nullptr);
    m_PrefabScene.reset();
    m_ShowPrefab = false;
    m_CreatePrefab = false;
    m_PrefabSourceID = m_PrefabSourceScene = 0;
    m_ActiveDocument = EditorDocument::Scene;
    if (m_ShowProject)
    {
        const auto &config = Project::GetActive()->GetConfig();
        m_ProjectName = config.Name;
        m_ScriptProject = config.ScriptProject;
        m_Startup = config.StartScene.generic_u8string();
        m_AssetDirectory = config.AssetDirectory.generic_u8string();
        m_Module = config.ScriptModulePath.generic_u8string();
    }
    MarkSceneSaved();
    RememberProject();
}
void AuthoringPanel::ValidatePython()
{
    if (m_Tools.Busy())
        return;
    Start("Validate Python", {});
}
void AuthoringPanel::Preflight(bool exporting)
{
    if (!m_Tools.Busy())
        Start("Toolchain readiness",
              {"authoring-preflight", "--operation", exporting ? "export" : "scripts"});
}
void AuthoringPanel::Start(std::string label, std::vector<std::string> args,
                           std::function<void()> completion)
{
    if (!args.empty() && (args[0] == "script-build" || args[0] == "editor-export") &&
        !Require(EditorAction::StartTool))
        return;
    if (!args.empty() && args[0] == "new-project" && !Require(EditorAction::CreateProject))
        return;
    ToolRequest request{Path(m_Preferences.Python), Path(m_Preferences.SDK), m_Editor.m_ProjectPath,
                        std::move(args), label};
    if (!m_Tools.Start(std::move(request)))
    {
        m_Editor.ActionFailed("A tool operation is already running");
        return;
    }
    m_Completion = std::move(completion);
    m_Output = label + " in progress. Output streams below.\nCancellation is "
                       "unavailable; closing the editor "
                       "waits for completion.";
    m_ShowOutput = true;
}
void AuthoringPanel::Tick(double timestep)
{
    m_Sprites.Tick(timestep);
    std::string progress;
    if (m_Tools.ReadProgress(progress))
        m_Output = m_Tools.Request().Label + " in progress.\n" + progress;
    if (!m_Tools.Poll(m_Report))
        return;
    m_Output =
        m_Report.Output + "\n" +
        (m_Report.Success ? "Completed successfully." : "Failed. Resolve the diagnostic and retry.");
    auto completion = std::move(m_Completion);
    m_Completion = {};
    if (m_Report.Success && completion)
    {
        try
        {
            completion();
        }
        catch (const std::exception &error)
        {
            m_Editor.ActionFailed(error.what());
            m_Output += "\n" + std::string(error.what());
        }
    }
    if (m_ExitAfterJob)
    {
        m_ExitAfterJob = false;
        RequestClose();
    }
}
bool AuthoringPanel::Ready(bool exporting) const
{
    return !m_Tools.Busy() && m_Report.Success && !m_Tools.Request().Arguments.empty() &&
           m_Tools.Request().Arguments[0] == "authoring-preflight" &&
           (!exporting || m_Tools.Request().Arguments.back() == "export") &&
           m_Tools.Request().Python == Path(m_Preferences.Python) &&
           m_Tools.Request().SDK == Path(m_Preferences.SDK) && m_Report.Python &&
           Path(m_Preferences.SDK).is_absolute();
}
void AuthoringPanel::ToolStatus(bool exporting)
{
    ImGui::TextWrapped("Python: %s", m_Report.Python.Executable.empty()
                                         ? "Not validated"
                                         : m_Report.Python.Executable.generic_u8string().c_str());
    if (m_Report.Python)
        ImGui::TextWrapped("%s | %s", m_Report.Python.Version.c_str(), m_Report.Python.Source.c_str());
    else
        ImGui::TextWrapped("%s", m_Report.Python.Error.empty()
                                     ? "Python 3.9+ is required for tooling. Editing and "
                                       "precompiled Play remain available."
                                     : m_Report.Python.Error.c_str());
    bool sdk = Path(m_Preferences.SDK).is_absolute();
    ImGui::TextWrapped("SDK: %s", sdk ? m_Preferences.SDK.c_str()
                                      : "Missing. Configure a Hazel source SDK; "
                                        "this is separate from Python.");
    ImGui::TextWrapped("Script authoring also needs the SDK's built ScriptCore "
                       "and Mono/.NET targeting pack. "
                       "Export builds Release with the host C++ compiler. "
                       "Failures appear in Output.");
    if (ImGui::Button("Configure Python / SDK"))
    {
        m_Draft = m_Preferences;
        m_ShowPreferences = true;
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(m_Tools.Busy());
    if (ImGui::Button("Check readiness"))
        Preflight(exporting);
    ImGui::EndDisabled();
}
void AuthoringPanel::RequestClose()
{
    if (m_Documents.Pending())
        return;
    if (m_Tools.Busy())
    {
        m_ExitAfterJob = true;
        m_ShowOutput = true;
        return;
    }
    Guard(OperationIntent::CloseEditor,
          []
          {
              Application::Get().Close();
              return true;
          });
}
void AuthoringPanel::Shortcuts()
{
    const auto &io = ImGui::GetIO();
    if (io.WantTextInput || ImGui::IsAnyItemActive() || m_Documents.Pending() ||
        ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId))
        return;
    if (io.KeyCtrl)
    {
        if (ImGui::IsKeyPressed(ImGuiKey_O, false))
            Guard(OperationIntent::OpenProject, [this] { return m_Editor.OpenProject(); });
        if (ImGui::IsKeyPressed(ImGuiKey_N, false))
            Guard(OperationIntent::NewScene,
                  [this]
                  {
                      m_Editor.NewScene();
                      MarkSceneSaved();
                      return true;
                  });
        if (ImGui::IsKeyPressed(ImGuiKey_S, false))
        {
            if (io.KeyAlt)
                SaveAll();
            else
                SaveActive(io.KeyShift);
        }
        if (ImGui::IsKeyPressed(ImGuiKey_D, false) &&
            (m_Editor.m_ViewportFocused || m_Editor.m_SceneHierarchyPanel.Focused()))
            m_Editor.OnDuplicateEntity();
        if (ImGui::IsKeyPressed(ImGuiKey_R, false))
            m_Editor.ReloadScripts();
        return;
    }
    if (m_Editor.m_ViewportFocused && Availability(EditorAction::EditScene) && !ImGuizmo::IsUsing())
    {
        if (ImGui::IsKeyPressed(ImGuiKey_Q, false))
            m_Editor.m_GizmoType = -1;
        if (ImGui::IsKeyPressed(ImGuiKey_W, false))
            m_Editor.m_GizmoType = ImGuizmo::TRANSLATE;
        if (ImGui::IsKeyPressed(ImGuiKey_E, false))
            m_Editor.m_GizmoType = ImGuizmo::ROTATE;
        if (ImGui::IsKeyPressed(ImGuiKey_R, false))
            m_Editor.m_GizmoType = ImGuizmo::SCALE;
    }
    if ((m_Editor.m_ViewportFocused || m_Editor.m_SceneHierarchyPanel.Focused()) &&
        ImGui::IsKeyPressed(ImGuiKey_Delete, false))
        m_Editor.m_SceneHierarchyPanel.DeleteSelected();
}
void AuthoringPanel::FileMenu()
{
    if (ImGui::MenuItem("New Project...", nullptr, false,
                        bool(Availability(EditorAction::CreateProject))))
    {
        m_ShowNew = true;
        Preflight();
    }
    if (ImGui::MenuItem("Open Project...", "Ctrl+O", false,
                        bool(Availability(EditorAction::ReplaceProject))))
        Guard(OperationIntent::OpenProject, [this] { return m_Editor.OpenProject(); });
    if (ImGui::MenuItem("Open Project for Repair...", nullptr, false,
                        bool(Availability(EditorAction::ReplaceProject))))
        Guard(OperationIntent::OpenProject,
              [this]
              {
                  auto path = FileDialogs::OpenFile("Hazel Project\0*.hproj\0");
                  return !path.empty() && m_Editor.OpenProject(Path(path), true);
              });
    if (ImGui::MenuItem("Open Scene for Repair...", nullptr, false,
                        bool(Availability(EditorAction::ReplaceScene))))
        Guard(OperationIntent::OpenScene,
              [this]
              {
                  auto path = FileDialogs::OpenFile("Hazel Scene\0*.hazel\0");
                  return !path.empty() && m_Editor.OpenScene(Path(path), true);
              });
    if (ImGui::BeginMenu("Recent Projects"))
    {
        if (ImGui::IsWindowAppearing())
        {
            m_RecentAvailable.clear();
            for (const auto &path : m_Preferences.RecentProjects)
            {
                std::error_code error;
                m_RecentAvailable[path] = std::filesystem::is_regular_file(Path(path), error);
            }
        }
        std::string remove;
        for (auto &path : m_Preferences.RecentProjects)
        {
            bool exists = m_RecentAvailable[path];
            if (ImGui::MenuItem(path.c_str(), exists ? nullptr : "Missing", false,
                                exists && bool(Availability(EditorAction::ReplaceProject))))
                Guard(OperationIntent::OpenProject,
                      [this, path] { return m_Editor.OpenProject(Path(path)); });
            if (!exists)
            {
                ImGui::PushID(path.c_str());
                if (ImGui::SmallButton("Remove missing entry"))
                    remove = path;
                ImGui::PopID();
            }
        }
        if (!remove.empty())
        {
            m_Preferences.RecentProjects.erase(std::remove(m_Preferences.RecentProjects.begin(),
                                                           m_Preferences.RecentProjects.end(), remove),
                                               m_Preferences.RecentProjects.end());
            try
            {
                if (m_PreferenceRecovery.empty())
                    m_Preferences.Save();
            }
            catch (const std::exception &error)
            {
                m_Editor.ActionFailed(error.what());
            }
        }
        if (m_Preferences.RecentProjects.empty())
            ImGui::TextDisabled("No recent projects");
        ImGui::EndMenu();
    }
    if (ImGui::MenuItem("Save Project", nullptr, false, bool(Availability(EditorAction::EditAsset))))
        m_Editor.SaveProject();
    ImGui::Separator();
    if (ImGui::MenuItem("New Scene", "Ctrl+N", false, bool(Availability(EditorAction::ReplaceScene))))
        Guard(OperationIntent::NewScene,
              [this]
              {
                  m_Editor.NewScene();
                  MarkSceneSaved();
                  return true;
              });
    if (ImGui::MenuItem("Open Scene...", nullptr, false, bool(Availability(EditorAction::ReplaceScene))))
        Guard(OperationIntent::OpenScene, [this] { return m_Editor.OpenScene(); });
    if (ImGui::MenuItem(
            ("Save " + ActiveName()).c_str(), "Ctrl+S", false,
            bool(Availability(m_ActiveDocument == EditorDocument::Scene ? EditorAction::SaveScene
                                                                        : EditorAction::SaveAsset))))
        SaveActive();
    if (ImGui::MenuItem("Save As...", "Ctrl+Shift+S", false,
                        m_ActiveDocument == EditorDocument::Scene &&
                            bool(Availability(EditorAction::SaveScene))))
        SaveActive(true);
    if (m_ActiveDocument != EditorDocument::Scene)
        ImGui::TextDisabled("Prefab/sheet: Save in place; Save As needs new identity");
    if (ImGui::MenuItem("Save All...", "Ctrl+Alt+S"))
        SaveAll();
    ImGui::Separator();
    if (ImGui::MenuItem("Exit"))
        RequestClose();
}
void AuthoringPanel::Menus()
{
    if (ImGui::BeginMenu("Project"))
    {
        bool project = bool(Project::GetActive());
        if (ImGui::MenuItem("Project Settings...", nullptr, false, project))
        {
            auto &config = Project::GetActive()->GetConfig();
            m_ProjectName = config.Name;
            m_ScriptProject = config.ScriptProject;
            m_Startup = config.StartScene.generic_u8string();
            m_AssetDirectory = config.AssetDirectory.generic_u8string();
            m_Module = config.ScriptModulePath.generic_u8string();
            m_ShowProject = true;
            m_ProjectChoicesLoaded = false;
        }
        if (ImGui::MenuItem("Create Script...", nullptr, false,
                            bool(Availability(EditorAction::EditAsset))))
            m_ShowScripts = true;
        if (ImGui::MenuItem("Build Scripts...", nullptr, false,
                            bool(Availability(EditorAction::StartTool))))
        {
            m_ShowBuild = true;
            Preflight();
        }
        if (ImGui::MenuItem("Reload Scripts", "Ctrl+R", false,
                            ScriptEngine::IsInitialized() &&
                                bool(Availability(EditorAction::ReloadScripts))))
            m_Editor.ReloadScripts();
        if (ImGui::MenuItem("Export Game...", nullptr, false,
                            bool(Availability(EditorAction::StartTool))))
        {
            m_ShowExport = true;
            Preflight(true);
        }
        if (ImGui::MenuItem("Open Project Folder", nullptr, false, project))
            if (!FileDialogs::OpenPath(m_Editor.m_ProjectPath.parent_path().generic_u8string()))
                m_Editor.ActionFailed("No application could open the project folder");
        if (!project)
            ImGui::TextDisabled("Open or create a project first");
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Scene"))
    {
        if (ImGui::MenuItem("Add Entity", nullptr, false, bool(Availability(EditorAction::EditScene))))
            m_Editor.m_SceneHierarchyPanel.AddEntity();
        if (ImGui::MenuItem("Duplicate Selected", "Ctrl+D", false,
                            bool(Availability(EditorAction::EditScene)) &&
                                bool(m_Editor.m_SceneHierarchyPanel.GetSelectedEntity())))
            m_Editor.OnDuplicateEntity();
        if (ImGui::MenuItem("Delete Selected", "Delete", false,
                            bool(Availability(EditorAction::EditScene)) &&
                                bool(m_Editor.m_SceneHierarchyPanel.GetSelectedEntity())))
            m_Editor.m_SceneHierarchyPanel.DeleteSelected();
        ImGui::Separator();
        if (ImGui::MenuItem("Play", nullptr, false, bool(Availability(EditorAction::Play))))
            m_Editor.OnScenePlay();
        if (ImGui::MenuItem("Simulate", nullptr, false, bool(Availability(EditorAction::Simulate))))
            m_Editor.OnSceneSimulate();
        const auto runtime = Availability(EditorAction::RuntimeControl);
        if (ImGui::MenuItem("Stop", nullptr, false, bool(runtime)))
            m_Editor.OnSceneStop();
        if (ImGui::MenuItem("Pause / Resume", nullptr, false, bool(runtime)))
            m_Editor.m_ActiveScene->SetPaused(!m_Editor.m_ActiveScene->IsPaused());
        if (ImGui::MenuItem("Step", nullptr, false, bool(runtime) && m_Editor.m_ActiveScene->IsPaused()))
            m_Editor.m_ActiveScene->Step();
        if (auto reason = Availability(EditorAction::EditScene).Reason)
            ImGui::TextWrapped("%s", reason);
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Edit"))
    {
        if (ImGui::MenuItem("Editor Preferences..."))
        {
            m_Draft = m_Preferences;
            m_ShowPreferences = true;
            ValidatePython();
        }
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Window"))
    {
        if (ImGui::MenuItem("Sprite Sheet", nullptr, m_Sprites.Visible(), m_Sprites.HasDocument()))
            m_Sprites.Show();
        if (ImGui::MenuItem("Prefab Inspector", nullptr, m_ShowPrefab, bool(m_PrefabScene)))
            m_ShowPrefab = !m_ShowPrefab;
        if (ImGui::MenuItem("Output", nullptr, m_ShowOutput))
            m_ShowOutput = !m_ShowOutput;
        ImGui::EndMenu();
    }
}
void AuthoringPanel::Preferences()
{
    if (!m_ShowPreferences)
        return;
    ImGui::SetNextWindowPos(
        ImVec2(ImGui::GetMainViewport()->WorkPos.x + 40, ImGui::GetMainViewport()->WorkPos.y + 40),
        ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize({580, 500}, ImGuiCond_FirstUseEver);
    ImGui::Begin("Editor Preferences", &m_ShowPreferences);
    if (!m_PreferenceRecovery.empty())
    {
        ImGui::TextWrapped("%s", m_PreferenceRecovery.c_str());
        ImGui::TextWrapped("Recent projects stay in memory until recovery. Apply "
                           "and Save explicitly "
                           "replaces the preserved file with these preferences.");
        ImGui::Separator();
    }
    ImGui::TextDisabled("User preferences — Apply and Save persists this draft");
    if (ImGui::CollapsingHeader("Storage"))
        PropertyUI::ReadOnly("location", "Settings file",
                             EditorPreferences::Location().generic_u8string().c_str());
    PathInput("Python executable", m_Draft.Python, false);
    PropertyUI::Help("Blank selects deterministic discovery. An explicit invalid "
                     "path is an error.");
    ImGui::BeginDisabled(m_Tools.Busy());
    if (ImGui::Button("Auto-detect"))
    {
        m_Draft.Python.clear();
        ToolRequest r{{}, Path(m_Draft.SDK), {}, {}, "Detect Python"};
        m_Tools.Start(std::move(r));
        m_ShowOutput = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Validate"))
    {
        ToolRequest r{Path(m_Draft.Python), Path(m_Draft.SDK), {}, {}, "Validate Python"};
        m_Tools.Start(std::move(r));
        m_ShowOutput = true;
    }
    ImGui::EndDisabled();
    ImGui::TextWrapped("Selected: %s\n%s %s\n%s", m_Report.Python.Executable.generic_u8string().c_str(),
                       m_Report.Python.Source.c_str(), m_Report.Python.Version.c_str(),
                       m_Report.Python.Error.c_str());
    PathInput("Hazel SDK folder", m_Draft.SDK, true);
    PathInput("Script editor executable", m_Draft.ScriptEditor, false);
    PropertyUI::Help("Blank uses the OS default for .cs files; a configured "
                     "editor receives one source-file argument.");
    PropertyUI::SliderFloat("ui-scale", "UI scale", m_Draft.UIScale, .8f, 2, "%.2f");
    PropertyUI::Checkbox("colliders", "Collider overlay", m_Draft.ShowColliders);
    bool paths = (m_Draft.Python.empty() || Path(m_Draft.Python).is_absolute()) &&
                 (m_Draft.SDK.empty() || Path(m_Draft.SDK).is_absolute()) &&
                 (m_Draft.ScriptEditor.empty() || Path(m_Draft.ScriptEditor).is_absolute());
    if (!paths)
        ImGui::TextWrapped("Tool locations must be absolute paths.");
    ImGui::BeginDisabled(!paths || m_Tools.Busy());
    if (ImGui::Button("Apply and Save"))
        try
        {
            m_Draft.RecentProjects = m_Preferences.RecentProjects;
            m_Draft.Save();
            m_PreferenceRecovery.clear();
            m_Preferences = m_Draft;
            ImGui::GetIO().FontGlobalScale = m_Preferences.UIScale;
            m_Editor.m_ShowPhysicsColliders = m_Preferences.ShowColliders;
            m_Output = "Preferences saved.";
            m_ShowOutput = true;
            ValidatePython();
        }
        catch (const std::exception &error)
        {
            m_Editor.ActionFailed(error.what());
        }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("Reset to defaults"))
    {
        auto recent = m_Draft.RecentProjects;
        m_Draft = {};
        m_Draft.RecentProjects = recent;
    }
    auto &caps = Renderer::GetCapabilities();
    ImGui::Separator();
    if (ImGui::CollapsingHeader("Detected renderer (read-only)"))
    {
        PropertyUI::ReadOnly("vendor", "Vendor", caps.Vendor.c_str());
        PropertyUI::ReadOnly("device", "Device", caps.Device.c_str());
        PropertyUI::ReadOnly("slots", "Texture slots", std::to_string(caps.MaxTextureSlots).c_str());
        PropertyUI::ReadOnly("texture-size", "Maximum texture size",
                             std::to_string(caps.MaxTextureSize).c_str());
        ImGui::TextWrapped("Detected facts are not saved as project settings.");
    }
    ImGui::End();
}
void AuthoringPanel::RefreshProjectChoices()
{
    m_SceneChoices.clear();
    m_ModuleChoices.clear();
    m_ProjectChoicesLoaded = true;
    m_ProjectChoiceRoot = m_Editor.m_ProjectPath.parent_path() / Path(m_AssetDirectory);
    if (!Portable(m_AssetDirectory) ||
        !Contained(m_Editor.m_ProjectPath.parent_path(), m_ProjectChoiceRoot))
    {
        m_Editor.ActionFailed("Assets choices require a contained project-relative folder");
        return;
    }
    std::error_code error;
    for (auto it = std::filesystem::recursive_directory_iterator(
             m_ProjectChoiceRoot, std::filesystem::directory_options::skip_permission_denied, error);
         !error && it != std::filesystem::recursive_directory_iterator(); it.increment(error))
    {
        if (!it->is_regular_file(error))
            continue;
        auto path = it->path().lexically_relative(m_ProjectChoiceRoot).generic_u8string();
        if (it->path().extension() == ".hazel")
            m_SceneChoices.push_back(path);
        else if (it->path().extension() == ".dll")
            m_ModuleChoices.push_back(path);
    }
    std::sort(m_SceneChoices.begin(), m_SceneChoices.end());
    std::sort(m_ModuleChoices.begin(), m_ModuleChoices.end());
    if (error)
        m_Editor.ActionFailed("Cannot list project choices: " + error.message());
}
void AuthoringPanel::ProjectSettings()
{
    if (!m_ShowProject || !Project::GetActive())
        return;
    ImGui::SetNextWindowPos(
        ImVec2(ImGui::GetMainViewport()->WorkPos.x + 40, ImGui::GetMainViewport()->WorkPos.y + 40),
        ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize({580, 500}, ImGuiCond_FirstUseEver);
    ImGui::Begin("Project Settings", &m_ShowProject);
    ImGui::PushID(m_Editor.m_ProjectPath.generic_u8string().c_str());
    ImGui::TextWrapped("Project scope. Saved in the portable .hproj. Changes apply on the next "
                       "open/Play. "
                       "Machine SDK and Python paths belong in Editor Preferences.");
    PropertyUI::Text("Display name", "Display name", m_ProjectName);
    PropertyUI::ReadOnly("script-build", "Script identifier", m_ScriptProject.c_str());
    PropertyUI::Help("Set at creation to match the Premake workspace and "
                     "assembly. Renaming requires SDK changes.");
    PropertyUI::Text("Assets folder", "Assets folder", m_AssetDirectory);
    auto root = m_Editor.m_ProjectPath.parent_path() / Path(m_AssetDirectory);
    if (!m_ProjectChoicesLoaded || m_ProjectChoiceRoot != root)
    {
        m_ProjectChoiceRoot = root;
        m_ProjectChoicesLoaded = false;
        // Typing a path invalidates choices, but never scans on every keystroke.
        if (ImGui::Button("Load scene / assembly choices"))
            RefreshProjectChoices();
    }
    else if (ImGui::Button("Refresh scene / assembly choices"))
        RefreshProjectChoices();
    {
        PropertyUI::Row row("startup", "Startup scene");
        if (ImGui::BeginCombo("##value", m_Startup.c_str()))
        {
            if (!m_ProjectChoicesLoaded)
                RefreshProjectChoices();
            for (const auto &choice : m_SceneChoices)
                if (ImGui::Selectable(choice.c_str(), choice == m_Startup))
                    m_Startup = choice;
            ImGui::EndCombo();
        }
    }
    {
        PropertyUI::Row row("module", "Script assembly");
        if (ImGui::BeginCombo("##value", m_Module.c_str()))
        {
            if (!m_ProjectChoicesLoaded)
                RefreshProjectChoices();
            for (const auto &choice : m_ModuleChoices)
                if (ImGui::Selectable(choice.c_str(), choice == m_Module))
                    m_Module = choice;
            ImGui::EndCombo();
        }
    }
    bool valid = !m_ProjectName.empty() && Portable(m_AssetDirectory) && Portable(m_Startup) &&
                 Portable(m_Module);
    if (!valid)
        ImGui::TextWrapped("Choose existing project-relative assets, startup scene "
                           "and compiled assembly, "
                           "and a valid script build identifier.");
    ImGui::BeginDisabled(!valid || !Availability(EditorAction::EditAsset));
    if (ImGui::Button("Save Project Settings"))
    {
        auto config = Project::GetActive()->GetConfig();
        config.Name = m_ProjectName;
        config.ScriptProject = m_ScriptProject;
        config.StartScene = Path(m_Startup);
        config.AssetDirectory = Path(m_AssetDirectory);
        config.ScriptModulePath = Path(m_Module);
        Guard(OperationIntent::OpenProject,
              [this, config]
              {
                  try
                  {
                      auto root = m_Editor.m_ProjectPath.parent_path() / config.AssetDirectory;
                      if (!Contained(m_Editor.m_ProjectPath.parent_path(), root) ||
                          !Contained(root, root / config.StartScene) ||
                          !Contained(root, root / config.ScriptModulePath))
                          throw std::runtime_error(
                              "Project paths must remain within the project asset root");
                      auto previous = Project::GetActive();
                      auto candidate = Project::LoadCandidate(m_Editor.m_ProjectPath);
                      candidate->GetConfig() = config;
                      candidate->LoadScene(config.StartScene);
                      if (!ProjectSerializer(candidate).Serialize(m_Editor.m_ProjectPath))
                          throw std::runtime_error("Cannot save project settings");
                      if (!m_Editor.OpenProject(m_Editor.m_ProjectPath))
                      {
                          if (!ProjectSerializer(previous).Serialize(m_Editor.m_ProjectPath))
                              throw std::runtime_error(
                                  "Open failed and descriptor rollback failed; restore "
                                  "the previous descriptor from version control");
                          throw std::runtime_error(
                              "Settings could not be applied; previous descriptor and "
                              "editor session were restored");
                      }
                      m_Output = "Project settings saved and applied.";
                      m_ShowOutput = true;
                      return true;
                  }
                  catch (const std::exception &error)
                  {
                      return m_Editor.ActionFailed(error.what());
                  }
              });
    }
    ImGui::EndDisabled();
    ImGui::PopID();
    ImGui::End();
}
void AuthoringPanel::NewProject()
{
    if (!m_ShowNew)
        return;
    ImGui::SetNextWindowPos(
        ImVec2(ImGui::GetMainViewport()->WorkPos.x + 40, ImGui::GetMainViewport()->WorkPos.y + 40),
        ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize({570, 440}, ImGuiCond_FirstUseEver);
    ImGui::Begin("New Project", &m_ShowNew);
    PropertyUI::Text("Display name", "Display name", m_Name);
    PropertyUI::Text("Technical identifier", "Technical identifier", m_Identifier);
    ImGui::TextWrapped("Letters, digits and underscore; start with a letter or "
                       "underscore. Used for the descriptor, "
                       "assembly and C# namespace.");
    PropertyUI::Text("Destination project folder", "Destination project folder", m_Destination);
    ImGui::PushID("dest");
    if (ImGui::Button("Browse parent folder"))
    {
        auto dir = FileDialogs::SelectFolder();
        if (!dir.empty())
            m_Destination = (Path(dir) / m_Identifier).generic_u8string();
    }
    ImGui::PopID();
    ImGui::TextWrapped("Creates %s.hproj, Assets/Scenes/Start.hazel (primary "
                       "camera), Scripts/Source/Example.cs, canonical "
                       "Premake build, Textures and Prefabs. Builds the initial "
                       "assembly before opening.",
                       m_Identifier.c_str());
    ToolStatus();
    if (!Ready())
        ImGui::TextWrapped("Run Check readiness after configuring tools. "
                           "Compiler/preflight diagnostics appear in Output.");
    bool valid = !m_Name.empty() && m_Name.size() <= 120 &&
                 ScriptSource::ValidIdentifier(m_Identifier) && Path(m_Destination).is_absolute();
    if (!valid)
        ImGui::TextWrapped("Enter a valid name/identifier and a new folder beneath "
                           "an existing parent. "
                           "Existing destinations are never overwritten.");
    ImGui::BeginDisabled(!valid || !Ready() || !Availability(EditorAction::CreateProject));
    if (ImGui::Button("Create and Open"))
    {
        auto target = Path(m_Destination) / (m_Identifier + ".hproj");
        Guard(OperationIntent::OpenProject,
              [this, target]
              {
                  Start("Create project",
                        {"new-project", "--name", m_Name, "--identifier", m_Identifier, "--destination",
                         m_Destination},
                        [this, target]
                        {
                            m_ShowNew = false;
                            m_Editor.OpenProject(target);
                        });
                  return m_Tools.Busy();
              });
    }
    ImGui::EndDisabled();
    ImGui::End();
}
void AuthoringPanel::Scripts()
{
    if (m_ShowScripts)
    {
        ImGui::SetNextWindowPos(
            ImVec2(ImGui::GetMainViewport()->WorkPos.x + 40, ImGui::GetMainViewport()->WorkPos.y + 40),
            ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize({570, 330}, ImGuiCond_FirstUseEver);
        ImGui::Begin("Create Script", &m_ShowScripts);
        PropertyUI::Text("Class name", "Class name", m_ScriptName);
        PropertyUI::Text("Namespace", "Namespace", m_Namespace);
        ImGui::TextWrapped("Creates Assets/Scripts/Source/<class>.cs with startup/update/cleanup "
                           "and a "
                           "prefab Instantiate/Destroy sample. Write gameplay C# in your external "
                           "editor, "
                           "then Build Scripts and select the compiled class in Properties.");
        ImGui::TextWrapped("Instantiate returns a valid entity with physics ready. Its script "
                           "starts at "
                           "the next safe boundary; use As<T>() after startup. Destroy "
                           "invalidates the "
                           "handle immediately and runs cleanup outside callbacks.");
        bool valid =
            ScriptSource::ValidIdentifier(m_ScriptName) && ScriptSource::ValidNamespace(m_Namespace);
        if (!valid)
            ImGui::TextWrapped("Use valid C# identifiers and a class filename that does not exist.");
        ImGui::BeginDisabled(!valid || !Availability(EditorAction::EditAsset));
        if (ImGui::Button("Create and Open Script") && Require(EditorAction::EditAsset))
            try
            {
                auto path =
                    ScriptSource::Create(Project::GetAssetDirectory(), m_ScriptName, m_Namespace);
                m_Output = "Created " + path.generic_u8string() +
                           ". Build Scripts, then assign its class in Properties.";
                m_ShowOutput = true;
                OpenScript(path);
                m_ShowScripts = false;
                if (m_Editor.m_ContentBrowserPanel)
                    m_Editor.m_ContentBrowserPanel->Refresh();
            }
            catch (const std::exception &error)
            {
                m_Editor.ActionFailed(error.what());
            }
        ImGui::EndDisabled();
        ImGui::End();
    }
    if (m_ShowBuild)
    {
        ImGui::SetNextWindowPos(
            ImVec2(ImGui::GetMainViewport()->WorkPos.x + 40, ImGui::GetMainViewport()->WorkPos.y + 40),
            ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize({570, 360}, ImGuiCond_FirstUseEver);
        ImGui::Begin("Build Scripts", &m_ShowBuild);
        ToolStatus();
        ImGui::TextWrapped("Builds the active project's Debug assembly through the "
                           "configured SDK. Stop Play first. "
                           "Successful compilation reloads the assembly; authored "
                           "fields remain in the scene.");
        ImGui::BeginDisabled(!Ready() || !Availability(EditorAction::StartTool));
        if (ImGui::Button("Build Scripts"))
        {
            auto project = m_Editor.m_ProjectPath;
            Start("Build Scripts", {"script-build", project.generic_u8string(), "--config", "Debug"},
                  [this, project]
                  {
                      if (project == m_Editor.m_ProjectPath)
                      {
                          ScriptEngine::Init(Project::GetAssetFileSystemPath(
                              Project::GetActive()->GetConfig().ScriptModulePath));
                      }
                  });
        }
        ImGui::EndDisabled();
        ImGui::End();
    }
}
void AuthoringPanel::Export()
{
    if (!m_ShowExport)
        return;
    ImGui::SetNextWindowPos(
        ImVec2(ImGui::GetMainViewport()->WorkPos.x + 40, ImGui::GetMainViewport()->WorkPos.y + 40),
        ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize({580, 520}, ImGuiCond_FirstUseEver);
    ImGui::Begin("Export Game", &m_ShowExport);
    ImGui::TextWrapped("Active project: %s", m_Editor.m_ProjectPath.generic_u8string().c_str());
#ifdef HZ_PLATFORM_WINDOWS
    ImGui::TextUnformatted("Target: Windows x64 | Release");
#else
    ImGui::TextUnformatted("Target: Linux x86_64 | Release");
#endif
    ImGui::TextWrapped("Other targets require building on that platform; "
                       "cross-compilation is unavailable. "
                       "Export builds Release engine/runtime and scripts before "
                       "packaging, using the "
                       "canonical SDK service and two compiler jobs.");
    PathInput("Output folder", m_ExportPath, true);
    PropertyUI::Text("Package name", "Package name", m_PackageName);
    ToolStatus(true);
    bool name = !m_PackageName.empty() &&
                std::all_of(m_PackageName.begin(), m_PackageName.end(),
                            [](char c)
                            {
                                return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                                       (c >= '0' && c <= '9') || c == '_' || c == '-';
                            }) &&
                std::isalnum(static_cast<unsigned char>(m_PackageName[0]));
    bool valid = Path(m_ExportPath).is_absolute() && name;
    if (!valid)
        ImGui::TextWrapped("Select an absolute output folder and a portable "
                           "package name (letters, digits, "
                           "underscore or hyphen; start with a letter/digit).");
    ImGui::BeginDisabled(!valid || !Ready(true) || !Availability(EditorAction::StartTool));
    if (ImGui::Button("Export Release Game"))
    {
        auto project = m_Editor.m_ProjectPath;
        Guard(OperationIntent::Export,
              [this, project]
              {
                  Start("Export Game", {"editor-export", project.generic_u8string(), "--name",
                                        m_PackageName, "--output", m_ExportPath});
                  return m_Tools.Busy();
              });
    }
    ImGui::EndDisabled();
    if (ImGui::Button("Open Output Folder"))
        FileDialogs::OpenPath(m_ExportPath);
    ImGui::End();
}
void AuthoringPanel::OpenScript(const std::filesystem::path &path)
{
    if (!Require(EditorAction::EditAsset))
        return;
    bool launched = m_Preferences.ScriptEditor.empty()
                        ? FileDialogs::OpenPath(path.generic_u8string())
                        : Process::Launch(Path(m_Preferences.ScriptEditor), {path.generic_u8string()});
    if (!launched)
        m_Editor.ActionFailed("Cannot open script. Configure an external editor in Edit > Editor "
                              "Preferences; source was preserved.");
}
void AuthoringPanel::SelectAsset(const std::filesystem::path &path)
{
    if (!Require(EditorAction::OpenAsset))
        return;
    if (path.extension() == ".cs")
    {
        OpenScript(path);
        return;
    }
    if (path.extension() == ".hsprites")
    {
        Guard(OperationIntent::OpenSheet,
              [this, path]
              {
                  if (!m_Sprites.Open(path))
                      return false;
                  m_ActiveDocument = EditorDocument::Sheet;
                  return true;
              });
        return;
    }
    if (path.extension() == ".hazel")
    {
        Guard(OperationIntent::OpenScene, [this, path] { return m_Editor.OpenScene(path); });
        return;
    }
    if (path.extension() != ".hprefab")
        return;
    Guard(OperationIntent::OpenPrefab,
          [this, path]
          {
              try
              {
                  auto relative = Project::MakeAssetReference(Project::GetAssetDirectory(), path);
                  auto scene = Prefab::Load(Project::GetAssetDirectory(), relative, true);
                  auto saved = SceneSerializer(scene).SerializeAuthoredSnapshot();
                  auto transform = Prefab::GetEntity(scene).GetComponent<TransformComponent>();
                  transform.Translation.x = transform.Translation.y = 0;
                  m_PrefabScene = scene;
                  m_PrefabReference = relative.generic_u8string();
                  m_InitialTransform = transform;
                  m_SavedPrefab = std::move(saved);
                  m_PrefabInspector.SetContext(scene);
                  m_PrefabInspector.EditScript = m_Editor.m_SceneHierarchyPanel.EditScript;
                  m_PrefabInspector.ReportError = [this](const auto &error)
                  { m_Editor.ActionFailed(error); };
                  m_ShowPrefab = true;
                  m_ActiveDocument = EditorDocument::Prefab;
                  return true;
              }
              catch (const std::exception &error)
              {
                  return m_Editor.ActionFailed(error.what());
              }
          });
}
void AuthoringPanel::CreatePrefab(Entity entity)
{
    if (!Require(EditorAction::EditAsset) || !entity || !entity.BelongsTo(m_Editor.m_EditorScene.get()))
        return;
    m_PrefabSourceID = entity.GetUUID();
    m_PrefabSourceScene = m_Editor.m_EditorScene->GetIdentity();
    m_PrefabName = "Prefabs/" + entity.GetName() + ".hprefab";
    m_CreatePrefab = true;
}
void AuthoringPanel::InstantiatePrefab(const std::filesystem::path &path, bool useInspectorTransform)
{
    if (!Require(EditorAction::EditScene))
        return;
    try
    {
        auto relative = Project::MakeAssetReference(Project::GetAssetDirectory(), path);
        auto entity =
            useInspectorTransform
                ? Prefab::Instantiate(Project::GetAssetDirectory(), relative, *m_Editor.m_EditorScene,
                                      m_InitialTransform)
                : Prefab::Instantiate(Project::GetAssetDirectory(), relative, *m_Editor.m_EditorScene);
        m_Editor.m_SceneHierarchyPanel.SetSelectedEntity(entity);
    }
    catch (const std::exception &error)
    {
        m_Editor.ActionFailed(error.what());
    }
}
void AuthoringPanel::Prefabs()
{
    if (m_CreatePrefab)
    {
        ImGui::SetNextWindowPos(
            ImVec2(ImGui::GetMainViewport()->WorkPos.x + 40, ImGui::GetMainViewport()->WorkPos.y + 40),
            ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize({560, 340}, ImGuiCond_FirstUseEver);
        ImGui::Begin("Create Prefab", &m_CreatePrefab);
        ImGui::TextWrapped("Single detached entity; self references remap. "
                           "External entity references and native scripts "
                           "are rejected. Future instances inherit edits; existing "
                           "instances remain independent.");
        PropertyUI::Text("Asset path inside Assets", "Asset path inside Assets", m_PrefabName);
        if (ImGui::Button("Browse asset folder"))
        {
            auto folder = FileDialogs::SelectFolder();
            if (!folder.empty())
            {
                m_PrefabName = Project::MakeAssetReference(Project::GetAssetDirectory(),
                                                           Path(folder) / Path(m_PrefabName).filename())
                                   .generic_u8string();
            }
        }
        auto source = m_Editor.m_EditorScene->GetIdentity() == m_PrefabSourceScene
                          ? m_Editor.m_EditorScene->GetEntityByUUID(m_PrefabSourceID)
                          : Entity{};
        bool valid = false;
        try
        {
            auto path = Path(m_PrefabName);
            valid = source && Portable(m_PrefabName) && path.extension() == ".hprefab";
        }
        catch (const std::exception &error)
        {
            ImGui::TextWrapped("%s", error.what());
        }
        if (!valid)
            ImGui::TextWrapped("Choose a new .hprefab path inside this project's "
                               "Assets and a valid authored entity.");
        ImGui::BeginDisabled(!valid || !Availability(EditorAction::EditAsset));
        if (ImGui::Button("Create Prefab Asset") && Require(EditorAction::EditAsset))
            try
            {
                if (std::filesystem::exists(
                        Prefab::Resolve(Project::GetAssetDirectory(), Path(m_PrefabName))))
                    throw std::runtime_error(
                        "Prefab destination already exists; choose a new asset path");
                Prefab::Save(Project::GetAssetDirectory(), Path(m_PrefabName), m_Editor.m_EditorScene,
                             source);
                m_CreatePrefab = false;
                if (m_Editor.m_ContentBrowserPanel)
                    m_Editor.m_ContentBrowserPanel->Refresh();
                m_Output = "Prefab created: " + m_PrefabName;
                m_ShowOutput = true;
                SelectAsset(Project::GetAssetDirectory() / Path(m_PrefabName));
            }
            catch (const std::exception &error)
            {
                m_Editor.ActionFailed(error.what());
            }
        ImGui::EndDisabled();
        ImGui::End();
    }
    if (!m_ShowPrefab || !m_PrefabScene)
        return;
    ImGui::SetNextWindowPos(
        ImVec2(ImGui::GetMainViewport()->WorkPos.x + 40, ImGui::GetMainViewport()->WorkPos.y + 40),
        ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize({570, 590}, ImGuiCond_FirstUseEver);
    ImGui::Begin("Prefab Inspector");
    m_PrefabFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
    if (m_PrefabFocused)
        m_ActiveDocument = EditorDocument::Prefab;
    bool dirty = SceneSerializer(m_PrefabScene).SerializeAuthoredSnapshot() != m_SavedPrefab;
    ImGui::TextWrapped("%s%s", m_PrefabReference.c_str(), dirty ? " * Unsaved" : "");
    ImGui::TextWrapped("Detached instances. Saving affects future instances "
                       "only; no overrides or automatic propagation.");
    const auto saveAvailable = Availability(EditorAction::SaveAsset);
    ImGui::BeginDisabled(!saveAvailable);
    if (ImGui::Button("Save Prefab"))
        SaveDocument(
            {EditorDocument::Prefab, m_PrefabScene->GetIdentity(), m_PrefabReference, dirty, {}});
    ImGui::EndDisabled();
    PropertyUI::Help(saveAvailable.Reason);
    ImGui::SameLine();
    if (ImGui::Button("Close Document"))
        Guard(OperationIntent::ClosePrefab,
              [this]
              {
                  ClosePrefab();
                  return true;
              });
    if (!m_PrefabScene)
    {
        ImGui::End();
        return;
    }
    m_PrefabInspector.DrawAssetProperties(Prefab::GetEntity(m_PrefabScene));
    ImGui::Separator();
    ImGui::TextWrapped("Viewport drops use the asset's authored transform. Set "
                       "placement below for Instantiate and Select.");
    ImGui::TextUnformatted("Initial instance transform");
    const TransformComponent defaults;
    PropertyUI::Vector("position", "Position", glm::value_ptr(m_InitialTransform.Translation), 3, .1f,
                       glm::value_ptr(defaults.Translation));
    auto rotation = glm::degrees(m_InitialTransform.Rotation);
    if (PropertyUI::Vector("rotation", "Rotation (deg)", glm::value_ptr(rotation), 3, .5f,
                           glm::value_ptr(defaults.Rotation)))
        m_InitialTransform.Rotation = glm::radians(rotation);
    PropertyUI::Vector("scale", "Scale", glm::value_ptr(m_InitialTransform.Scale), 3, .1f,
                       glm::value_ptr(defaults.Scale), "%.2f", {}, .001f, 1000);
    ImGui::BeginDisabled(dirty || !Availability(EditorAction::EditScene));
    if (ImGui::Button("Instantiate and Select"))
        InstantiatePrefab(Project::GetAssetDirectory() / Path(m_PrefabReference), true);
    ImGui::EndDisabled();
    if (dirty)
        ImGui::TextWrapped("Save the prefab before instantiating its edits.");
    ImGui::End();
}
void AuthoringPanel::Render()
{
    Preferences();
    ProjectSettings();
    NewProject();
    Scripts();
    Export();
    Prefabs();
    m_Sprites.Render(bool(Availability(EditorAction::EditAsset)));
    if (m_Sprites.Focused())
        m_ActiveDocument = EditorDocument::Sheet;
    DocumentGuard();
    if (m_ShowOutput)
    {
        if (auto *stats = ImGui::FindWindowByName("Stats"); stats && stats->DockId)
            ImGui::SetNextWindowDockID(stats->DockId, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize({600, 260}, ImGuiCond_FirstUseEver);
        ImGui::Begin("Output", &m_ShowOutput);
        if (m_ExitAfterJob)
        {
            ImGui::TextWrapped("Exit requested. The editor will close after this job completes.");
            if (ImGui::Button("Cancel Exit Request"))
                m_ExitAfterJob = false;
        }
        if (ImGui::Button("Copy Output"))
            ImGui::SetClipboardText((m_Editor.m_ActionError + "\n" + m_Output).c_str());
        if (m_Tools.Busy())
            ImGui::TextUnformatted("Tool job running. Other tool jobs/project "
                                   "replacement are disabled.");
        else if (!m_Tools.Request().Label.empty())
        {
            ImGui::TextWrapped("%s: %s", m_Tools.Request().Label.c_str(),
                               m_Report.Success ? "Completed successfully"
                                                : "Failed; resolve the diagnostic and retry");
            const auto &arguments = m_Tools.Request().Arguments;
            if (m_Report.Success && !arguments.empty() && arguments[0] == "editor-export")
            {
                ImGui::TextWrapped("Artifacts: %s", arguments.back().c_str());
                if (ImGui::Button("Open Output Folder") && !FileDialogs::OpenPath(arguments.back()))
                    m_Editor.ActionFailed("Cannot open the output folder. Copy its "
                                          "artifact path from Output.");
            }
        }
        if (!m_Editor.m_ActionError.empty())
            ImGui::TextWrapped("%s", m_Editor.m_ActionError.c_str());
        ImGui::BeginChild("Tool output", {0, 0}, false, ImGuiWindowFlags_HorizontalScrollbar);
        bool follow = ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 1;
        ImGui::TextUnformatted(m_Output.c_str());
        if (follow)
            ImGui::SetScrollHereY(1);
        ImGui::EndChild();
        ImGui::End();
    }
}
} // namespace Hazel
