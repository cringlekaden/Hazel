#pragma once
#include "EditorPreferences.h"
#include "Panels/SceneHierarchyPanel.h"
#include "ProjectTools.h"
#include <functional>
namespace Hazel
{
class EditorLayer;
class AuthoringPanel
{
  public:
	explicit AuthoringPanel(EditorLayer &editor);
	void Menus();
	void Shortcuts();
	void FileMenu();
	void Render();
	void Tick();
	void ShowOutput()
	{
		m_ShowOutput = true;
	}
	void BindProject();
	void RememberProject();
	void SelectAsset(const std::filesystem::path &path);
	void CreatePrefab(Entity entity);
	void InstantiatePrefab(const std::filesystem::path &path);
	void OpenScript(const std::filesystem::path &path);
	void Guard(std::function<void()> operation, bool includeScene = true);
	void MarkSceneSaved();

  private:
	void Preferences();
	void ProjectSettings();
	void NewProject();
	void Scripts();
	void Export();
	void Prefabs();
	void ToolStatus(bool exporting = false);
	void ValidatePython();
	void Preflight(bool exporting = false);
	void Start(std::string label, std::vector<std::string> args, std::function<void()> completion = {});
	bool Ready(bool exporting = false) const;
	std::string SceneText() const;
	EditorLayer &m_Editor;
	EditorPreferences m_Preferences, m_Draft;
	ProjectTools m_Tools;
	ToolReport m_Report;
	std::function<void()> m_Completion, m_Pending;
	bool m_PendingIncludesScene = true;
	std::string m_Output, m_SavedScene, m_SavedPrefab, m_PrefabReference, m_ProjectName, m_ScriptProject;
	std::string m_Name = "My Game", m_Identifier = "MyGame", m_Destination, m_Startup, m_AssetDirectory,
				m_Module;
	std::string m_ScriptName = "NewScript", m_Namespace = "Game", m_PrefabName = "Prefabs/NewPrefab.hprefab",
				m_ExportPath, m_PackageName = "Game";
	Ref<Scene> m_PrefabScene;
	Entity m_PrefabSource;
	SceneHierarchyPanel m_PrefabInspector;
	TransformComponent m_InitialTransform;
	bool m_ShowPreferences = false, m_ShowProject = false, m_ShowNew = false, m_ShowScripts = false,
		 m_ShowBuild = false, m_ShowExport = false, m_ShowPrefab = false, m_CreatePrefab = false,
		 m_ShowOutput = false;
};
} // namespace Hazel
