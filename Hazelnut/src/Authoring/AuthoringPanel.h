#pragma once
#include "EditorDocuments.h"
#include "EditorPreferences.h"
#include "HazelSDK.h"
#include "Panels/SceneHierarchyPanel.h"
#include "Panels/SpriteSheetPanel.h"
#include "ProjectTools.h"
#include "Panels/ConsolePanel.h"
#include "Hazel/Core/FileDocument.h"
#include "Hazel/Core/DocumentLoadReport.h"
#include "EditorState.h"
#include <functional>
#include <unordered_map>
namespace Hazel
{
class EditorLayer;
class AuthoringPanel
{
    friend class EditorWorkflowSmoke;

  public:
    explicit AuthoringPanel(EditorLayer &editor);
    void Menus();
    void Shortcuts();
    void RequestClose();
    void FileMenu();
    void Render();
    void Tick(double timestep = 0);
    void PollTools(); // Main thread, including minimized frames.
    void ShowConsole() { m_Console.Show(); }
    void BindProject();
    void RememberProject();
    void SelectAsset(const std::filesystem::path &path);
    void CreatePrefab(Entity entity);
    void InstantiatePrefab(const std::filesystem::path &path, bool useInspectorTransform = false);
    void OpenScript(const std::filesystem::path &path);
    void Guard(OperationIntent intent, std::function<bool()> operation);
    bool SaveActive(bool saveAs = false);
    void SaveAll();
    void ObserveSceneFocus();
    void Toolbar();
    void Status();
    enum class RuntimeAction { Play, Simulate, Stop, TogglePause, Step };
    ActionAvailability RuntimeAvailability(RuntimeAction action) const;
    bool InvokeRuntime(RuntimeAction action);
    ActionAvailability Availability(EditorAction action) const;
    bool Require(EditorAction action);
    void MarkSceneSaved();
    void ReportOpen(const std::filesystem::path& path,const DocumentLoadReport& report);
    bool SpriteDirty() const { return m_Sprites.Dirty(); }
    void GuardPlay(bool simulate);
    void Startup(const std::vector<std::string>& arguments);
    void FlushWorkspace();
    void RestoreWorkspace(bool scene);
    void SceneOpened();
    const std::string& RenderingRestartReason() const;
    void ApplyRuntimeVSync();
    void RestoreEditorVSync();

  private:
    void Preferences();
    void RecoveryControls();
    void SaveConflictControls();
    void WorkspaceControls();
    void CaptureWorkspace();
    void ProjectSettings();
    void ProjectRendering();
    void EditorRendering();
    void CopyDeviceReport();
    bool SaveRenderingRequests(const RuntimeRendererRequests& requests);
    void NewProject();
    void Scripts();
    void Readiness();
    bool CreateProject(const std::string& name,const std::string& identifier,const std::filesystem::path& destination);
    void Export();
    void Prefabs();
    void ToolStatus(bool exporting = false);
    void RefreshProjectChoices();
    void ValidatePython();
    void RefreshSDK(bool draft = false);
    void UseAutomaticSDK();
    void Preflight(bool exporting = false);
    void Start(std::string label, std::vector<std::string> args, ToolCompletion completion = ToolCompletion::None, std::filesystem::path target = {});
    void Notify(const std::string& message,spdlog::level::level_enum level = spdlog::level::info);
    bool Ready(bool exporting = false) const;
    std::string SceneText() const;
    std::vector<DocumentInfo> Documents() const;
    DocumentSaveResult SaveDocument(const DocumentInfo &document);
    void ClosePrefab();
    void DocumentGuard();
    std::string ActiveName() const;
    SceneTarget AssignmentTarget() const;
    Entity ResolveTarget(SceneTarget target);
    EditorLayer &m_Editor;
    EditorPreferences m_Preferences, m_Draft;
    std::unique_ptr<EditorState> m_State;
    ProjectWorkspace m_Workspace;
    std::filesystem::path m_WorkspaceProject;
    std::filesystem::path m_CreatedProject;
    RuntimeRendererRequests m_RenderingDraft;
    bool m_RenderingAuthored=false;
    mutable std::string m_RenderingRestart;
    std::string m_MissingProject, m_MissingScene, m_PreviousWorkspace;
    std::string m_PendingState, m_WrittenState, m_PersistenceError;
    double m_StateClock=0, m_StateChanged=0, m_StatePolled=0;
    bool m_StartupRestore=true, m_RestoreAssetsPending=false;
    int m_PreferenceCategory=0;
    std::string m_PreferenceSearch;
    SDKSelection m_SDK, m_DraftSDK;
    std::string m_CheckedDraftSDK;
    ProjectTools m_Tools;
    ToolReport m_Report;
    ConsolePanel m_Console;
    uint64_t m_ProjectGeneration=0;
    EditorDocuments m_Documents;
    EditorDocument m_ActiveDocument = EditorDocument::Scene;
    bool m_PrefabFocused = false;
    bool m_ResolvingDocumentAction = false;
    std::string m_LastTitle;
    std::filesystem::path m_ProjectChoiceRoot;
    std::vector<std::string> m_SceneChoices, m_ModuleChoices;
    bool m_ProjectChoicesLoaded = false;
    std::unordered_map<std::string, bool> m_RecentAvailable;
    bool m_ExitAfterJob = false;
    std::string m_PreferenceRecovery, m_SavedScene, m_SavedPrefab, m_PrefabReference,
        m_ProjectName, m_ScriptProject;
    std::string m_Name = "My Game", m_Identifier = "MyGame", m_Destination, m_Startup, m_AssetDirectory,
                m_Module;
    std::string m_ScriptName = "NewScript", m_Namespace = "Game",
                m_PrefabName = "Prefabs/NewPrefab.hprefab", m_ExportPath, m_PackageName = "Game";
    Ref<Scene> m_PrefabScene;
    FileDocument m_PrefabFile;
    bool m_ShowSaveConflict=false;
    EditorDocument m_ConflictDocument=EditorDocument::Scene;
    uint64_t m_ConflictIdentity=0;
    uint64_t m_PrefabSourceID = 0, m_PrefabSourceScene = 0;
    SceneHierarchyPanel m_PrefabInspector;
    SpriteSheetPanel m_Sprites;
    TransformComponent m_InitialTransform;
    bool m_ShowPreferences = false, m_ShowProject = false, m_ShowNew = false, m_ShowScripts = false,
         m_ShowBuild = false, m_ShowExport = false, m_ShowPrefab = false, m_CreatePrefab = false;
};
} // namespace Hazel
