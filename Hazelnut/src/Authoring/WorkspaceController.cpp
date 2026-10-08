#include "AuthoringPanel.h"
#include "EditorLayer.h"
#include "Hazel/Core/Application.h"
#include "Hazel/Core/FileSystem.h"
#include "Hazel/Core/Resources.h"
#include "Hazel/Utils/PlatformUtils.h"
#include "UI/PropertyUI.h"
#include <imgui.h>
namespace Hazel {
    namespace {
        std::filesystem::path ScenePath(const std::string &value) {
            auto p = std::filesystem::u8path(value);
            return p.is_absolute() ? p : Project::GetAssetDirectory() / p;
        }
        std::string SceneReference(const std::filesystem::path &path) {
            auto relative = path.lexically_relative(Project::GetAssetDirectory());
            return !relative.empty() && *relative.begin() != ".." ? relative.generic_u8string()
                                                                  : path.generic_u8string();
        }
    } // namespace
    void AuthoringPanel::Startup(const std::vector<std::string> &arguments) {
        const auto launch = EditorLaunch::Parse(arguments);
        m_StartupRestore = launch.Restore && m_Preferences.RestoreSession;
        if (launch.Restore)
            Application::Get().GetWindow().RestorePlacement(m_State->Session.Window);
        else
            Application::Get().GetWindow().RestorePlacement({});
        if (!launch.Error.empty()) {
            m_Editor.ActionFailed(launch.Error);
            return;
        }
        const auto project =
            launch.ProjectToOpen(m_Preferences.RestoreSession, m_State->Session.LastProject);
        if (!project.empty()) {
            if (!m_Editor.OpenProject(project)) {
                if (!launch.Explicit)
                    m_MissingProject = project.generic_u8string();
                return; // Explicit failure must never fall back to remembered/bundled content.
            }
        } else if (!launch.Explicit && launch.Restore && m_State->Session.LastProject.empty()) {
            const auto bundled =
                Project::Discover(FileSystem::GetExecutablePath().parent_path() / "Example");
            if (bundled.size() == 1 && !m_Editor.OpenProject(bundled.front()))
                return;
        }
        if (!launch.Scene.empty()) {
            m_Editor.OpenScene(launch.Scene);
            RestoreWorkspace(false);
        } else if (Project::GetActive() && m_StartupRestore)
            RestoreWorkspace(true);
    }
    void AuthoringPanel::SceneOpened() {
        if (m_WorkspaceProject != m_Editor.m_ProjectPath)
            return;
        if (!m_Editor.m_EditorScenePath.empty()) {
            const auto scene=SceneReference(m_Editor.m_EditorScenePath);
            if(scene!=m_Workspace.Scene){m_Workspace.Entity=0;m_Workspace.Sections.clear();}
            m_Workspace.Scene=scene;
        }
        m_MissingScene.clear();
        try{m_State->SaveWorkspace(m_WorkspaceProject,m_Workspace);}catch(const std::exception& e){m_PersistenceError=e.what();Notify(m_PersistenceError,spdlog::level::warn);}
    }
    void AuthoringPanel::RestoreWorkspace(bool restoreScene) {
        if (!Project::GetActive() || !m_State)
            return;
        const auto saved = m_Workspace; // Successful Open updates the current workspace record.
        if (restoreScene && !saved.Scene.empty() &&
            ScenePath(saved.Scene) != m_Editor.m_EditorScenePath) {
            if (!m_Editor.OpenScene(ScenePath(saved.Scene))) {
                m_MissingScene = saved.Scene;
                m_Workspace.Scene = saved.Scene;
                Notify("Remembered scene could not open. Validated startup scene retained; Locate "
                       "or Forget in Editor Preferences.",
                       spdlog::level::warn);
            }
        }
        if (saved.Scene.empty() || ScenePath(saved.Scene) == m_Editor.m_EditorScenePath) {
            m_Editor.m_EditorCamera.RestoreOrbit(saved.Focus, saved.Pitch, saved.Yaw,
                                                 saved.Distance);
            m_Editor.m_SceneHierarchyPanel.SetSelectedEntity(
                m_Editor.m_EditorScene->GetEntityByUUID(saved.Entity));
            m_Editor.m_SceneHierarchyPanel.RestoreSections(saved.Sections);
        }
        if (m_Editor.m_ContentBrowserPanel &&
            !m_Editor.m_ContentBrowserPanel->Restore(std::filesystem::u8path(saved.BrowserFolder),
                                                     saved.BrowserSearch, saved.BrowserType,
                                                     saved.Thumbnail))
            Notify("Remembered browser folder is unavailable; Assets root retained",
                   spdlog::level::warn);
        m_ExportPath = saved.ExportFolder;
        m_PackageName = saved.ExportName;
        if (m_Documents.Pending()) {
            m_RestoreAssetsPending = true;
            return;
        }
        m_RestoreAssetsPending = false;
        if (!saved.Prefab.empty() && !m_PrefabScene) {
            try {
                auto path = Project::ResolveOwnedAsset(Project::GetAssetDirectory(),
                                                       std::filesystem::u8path(saved.Prefab));
                SelectAsset(path);
                m_ShowPrefab = saved.PrefabVisible;
            } catch (const std::exception &e) {
                Notify(std::string("Remembered prefab not restored: ") + e.what(),
                       spdlog::level::warn);
            }
        }
        if (!saved.Sheet.empty() && !m_Sprites.HasDocument()) {
            try {
                auto path = Project::ResolveOwnedAsset(Project::GetAssetDirectory(),
                                                       std::filesystem::u8path(saved.Sheet));
                if (m_Sprites.Open(path))
                    m_Sprites.RestoreSelection(saved.Region, saved.Clip, saved.SheetVisible);
            } catch (const std::exception &e) {
                Notify(std::string("Remembered sheet not restored: ") + e.what(),
                       spdlog::level::warn);
            }
        }
    }
    void AuthoringPanel::CaptureWorkspace() {
        if (!m_State)
            return;
        auto &session = m_State->Session;
        session.Window = Application::Get().GetWindow().GetPlacement();
        session.Panels =
            (m_Editor.m_SceneHierarchyPanel.HierarchyVisible ? 1 : 0) |
            (m_Editor.m_SceneHierarchyPanel.PropertiesVisible ? 2 : 0) |
            (!m_Editor.m_ContentBrowserPanel || m_Editor.m_ContentBrowserPanel->Visible ? 4 : 0) |
            (m_Editor.m_ShowStats ? 8 : 0) | (m_Console.Visible ? 16 : 0);
        auto filter = m_Console.Filter();
        session.Severities = filter.Severities;
        session.Sources = filter.Sources;
        session.ConsoleSearch = filter.Search;
        session.Follow = m_Console.Follow();
        if (m_WorkspaceProject.empty() || m_WorkspaceProject != m_Editor.m_ProjectPath)
            return;
        session.LastProject = m_Editor.m_ProjectPath.generic_u8string();
        const bool sameScene =
            m_Workspace.Scene.empty() || ScenePath(m_Workspace.Scene) == m_Editor.m_EditorScenePath;
        if (sameScene && m_Editor.m_SceneState == EditorLayer::SceneState::Edit) {
            auto selected = m_Editor.m_SceneHierarchyPanel.GetSelectedEntity();
            m_Workspace.Entity = selected ? static_cast<uint64_t>(selected.GetUUID()) : 0;
            m_Workspace.Sections = m_Editor.m_SceneHierarchyPanel.Sections();
        }
        if (sameScene) {
            const auto &camera = m_Editor.m_EditorCamera;
            m_Workspace.Focus = camera.GetFocalPoint();
            m_Workspace.Pitch = camera.GetPitch();
            m_Workspace.Yaw = camera.GetYaw();
            m_Workspace.Distance = camera.GetDistance();
        }
        if (auto &browser = m_Editor.m_ContentBrowserPanel; browser) {
            m_Workspace.BrowserFolder = browser->Folder().generic_u8string();
            m_Workspace.BrowserSearch = browser->Search();
            m_Workspace.BrowserType = browser->TypeFilter();
            m_Workspace.Thumbnail = browser->Thumbnail();
        }
        m_Workspace.Prefab = m_PrefabScene ? m_PrefabReference : "";
        m_Workspace.PrefabVisible = m_ShowPrefab;
        m_Workspace.Sheet = m_Sprites.HasDocument() ? m_Sprites.Name() : "";
        m_Workspace.SheetVisible = m_Sprites.Visible();
        m_Workspace.Region = m_Sprites.SelectedRegion();
        m_Workspace.Clip = m_Sprites.SelectedClip();
        m_Workspace.ExportFolder = m_ExportPath;
        m_Workspace.ExportName = m_PackageName;
    }
    void AuthoringPanel::FlushWorkspace() {
        if (!m_State)
            return;
        try {
            CaptureWorkspace();
            m_State->SaveSession();
            if (!m_WorkspaceProject.empty())
                m_State->SaveWorkspace(m_WorkspaceProject, m_Workspace);
            m_PersistenceError.clear();
        } catch (const std::exception &e) {
            m_PersistenceError = e.what();
            Notify("Editor state not saved: " + m_PersistenceError, spdlog::level::warn);
        }
    }
    void AuthoringPanel::WorkspaceControls() {
        if (!m_State)
            return;
        if (!m_State->Diagnostic.empty())
            ImGui::TextWrapped("%s", m_State->Diagnostic.c_str());
        if (!m_PersistenceError.empty())
            ImGui::TextWrapped("%s", m_PersistenceError.c_str());
        if (!Application::Get().GetImGuiLayer()->OwnsWorkspace())
            ImGui::TextWrapped("Another editor owns shared session/layout writes. This instance "
                               "saves separate snapshots under UserData/instances.");
        if (ImGui::Button("Open editor data folder"))
            FileDialogs::OpenPath(Resources::Get().UserData.generic_u8string());
        PropertyUI::Help("Contains machine-local settings, workspaces, instance snapshots and "
                         "original recovery copies.");
        if (ImGui::Button("Reload session baseline")) {
            auto *gui = Application::Get().GetImGuiLayer();
            m_State = std::make_unique<EditorState>(gui->OwnsWorkspace(), gui->InstanceToken());
            if (!m_WorkspaceProject.empty())
                m_State->LoadWorkspace(m_WorkspaceProject);
            m_WrittenState.clear();
            m_PersistenceError.clear();
        }
        if (ImGui::Button("Reload saved preferences")) {
            m_Preferences = EditorPreferences::Load(m_PreferenceRecovery);
            m_Draft = m_Preferences;
            RefreshSDK(true);
        }
        PropertyUI::WrapButton("Reset session settings");
        if (ImGui::Button("Reset session settings"))
            try {
                m_State->Session = {};
                CaptureWorkspace();
                m_State->SaveSession(true);
                m_State->Diagnostic.clear();
                m_PersistenceError.clear();
            } catch (const std::exception &e) {
                m_Editor.ActionFailed(e.what());
            }
        if (!m_MissingProject.empty()) {
            ImGui::TextWrapped("Remembered project unavailable: %s. File > Recent Projects / New "
                               "Project remain available.",
                               m_MissingProject.c_str());
            const auto can = Availability(EditorAction::ReplaceProject);
            ImGui::BeginDisabled(!can);
            if (ImGui::Button("Locate remembered project")) {
                const auto chosen = FileDialogs::OpenFile("Hazel project\0*.hproj\0");
                if (!chosen.empty()) {
                    const auto previous = m_MissingProject;
                    const auto path = std::filesystem::u8path(chosen);
                    Guard(OperationIntent::OpenProject, [this, previous, path] {
                        if (!m_Editor.OpenProject(path))
                            return false;
                        m_PreviousWorkspace = previous;
                        m_MissingProject.clear();
                        return true;
                    });
                }
            }
            ImGui::EndDisabled();
            PropertyUI::Help(can.Reason);
            PropertyUI::WrapButton("Forget remembered project");
            if (ImGui::Button("Forget remembered project")) {
                m_State->Session.LastProject.clear();
                m_MissingProject.clear();
                FlushWorkspace();
            }
        }
        if (!m_WorkspaceProject.empty()) {
            const auto can = Availability(EditorAction::ReplaceScene);
            ImGui::BeginDisabled(!can);
            if (!m_Workspace.Scene.empty()) {
                ImGui::TextWrapped("Remembered scene: %s", m_Workspace.Scene.c_str());
                if (ImGui::Button("Open remembered scene")) {
                    const auto path = ScenePath(m_Workspace.Scene);
                    Guard(OperationIntent::OpenScene,
                          [this, path] { return m_Editor.OpenScene(path); });
                }
            }
            if (!m_MissingScene.empty()) {
                PropertyUI::WrapButton("Locate scene");
                if (ImGui::Button("Locate scene")) {
                    auto path = FileDialogs::OpenFile("Hazel scene\0*.hazel\0");
                    if (!path.empty()) {
                        const auto file = std::filesystem::u8path(path);
                        Guard(OperationIntent::OpenScene,
                              [this, file] { return m_Editor.OpenScene(file); });
                    }
                }
                PropertyUI::WrapButton("Forget missing scene");
                if (ImGui::Button("Forget missing scene")) {
                    m_MissingScene.clear();
                    m_Workspace.Scene = SceneReference(m_Editor.m_EditorScenePath);
                    FlushWorkspace();
                }
            }
            ImGui::EndDisabled();
            PropertyUI::Help(can.Reason);
            const auto editable = Availability(EditorAction::EditAsset);
            ImGui::BeginDisabled(!editable);
            if (ImGui::Button("Forget workspace for this project") &&
                Require(EditorAction::EditAsset))
                try {
                    m_Workspace = {};
                    m_MissingScene.clear();
                    m_State->SaveWorkspace(m_WorkspaceProject, m_Workspace, true);
                    m_State->Diagnostic.clear();
                    Notify("Workspace reset; authored documents and dock layout retained");
                } catch (const std::exception &e) {
                    m_Editor.ActionFailed(e.what());
                }
            if (!m_PreviousWorkspace.empty() && ImGui::Button("Associate previous workspace") &&
                Require(EditorAction::EditAsset))
                try {
                    m_State->Associate(std::filesystem::u8path(m_PreviousWorkspace),
                                       m_WorkspaceProject);
                    m_Workspace = m_State->LoadWorkspace(m_WorkspaceProject);
                    RestoreWorkspace(false);
                    m_PreviousWorkspace.clear();
                } catch (const std::exception &e) {
                    m_Editor.ActionFailed(e.what());
                }
            ImGui::EndDisabled();
            PropertyUI::Help(editable.Reason);
        }
    }
} // namespace Hazel
