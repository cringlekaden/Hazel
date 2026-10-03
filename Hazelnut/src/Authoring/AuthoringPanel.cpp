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
#include <algorithm>
#include <cctype>
#include <glm/gtc/type_ptr.hpp>
#include <imgui.h>
#include <imgui_internal.h>
#include <misc/cpp/imgui_stdlib.h>
namespace Hazel
{
static std::filesystem::path Path(const std::string &value)
{
	return std::filesystem::u8path(value);
}
static void Browse(std::string &value, bool folder)
{
	ImGui::SameLine();
	if (ImGui::Button("Browse"))
	{
		auto chosen = folder ? FileDialogs::SelectFolder() : FileDialogs::OpenFile("All files\0*\0");
		if (!chosen.empty())
			value = chosen;
	}
}
static void PathInput(const char *label, std::string &value, bool folder)
{
	ImGui::PushID(label);
	ImGui::TextUnformatted(label);
	ImGui::SetNextItemWidth(std::max(80.0f, ImGui::GetContentRegionAvail().x - 90.0f));
	ImGui::InputText("##path", &value);
	Browse(value, folder);
	ImGui::PopID();
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
	m_Draft = m_Preferences;
	ImGui::GetIO().FontGlobalScale = m_Preferences.UIScale;
	m_Editor.m_ShowPhysicsColliders = m_Preferences.ShowColliders;
	if (!m_Output.empty())
		m_ShowOutput = true;
}
void AuthoringPanel::MarkSceneSaved()
{
	m_SavedScene = SceneText();
}
std::string AuthoringPanel::SceneText() const
{
	return SceneSerializer(m_Editor.m_EditorScene).SerializeText();
}
void AuthoringPanel::Guard(std::function<void()> action, bool includeScene)
{
	if (m_Tools.Busy())
	{
		m_Editor.ActionFailed("Wait for the current tool job to finish. Jobs cannot be cancelled; closing "
							  "waits for completion.");
		m_ShowOutput = true;
		return;
	}
	try
	{
		bool dirty = includeScene && SceneText() != m_SavedScene;
		if (m_PrefabScene)
			dirty |= SceneSerializer(m_PrefabScene).SerializeText() != m_SavedPrefab;
		if (dirty)
		{
			m_PendingIncludesScene = includeScene;
			m_Pending = std::move(action);
			return;
		}
		action();
	}
	catch (const std::exception &error)
	{
		m_Editor.ActionFailed(error.what());
	}
}
void AuthoringPanel::RememberProject()
{
	m_Preferences.Remember(m_Editor.m_ProjectPath);
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
	if (m_Editor.m_ContentBrowserPanel)
		m_Editor.m_ContentBrowserPanel->SelectAsset = [this](auto &path) { SelectAsset(path); };
	m_Editor.m_SceneHierarchyPanel.ReportError = [this](const std::string &error) {
		m_Editor.ActionFailed(error);
	};
	m_Editor.m_SceneHierarchyPanel.CreatePrefab = [this](auto entity) { CreatePrefab(entity); };
	m_Editor.m_SceneHierarchyPanel.EditScript = [this](auto &name) {
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
void AuthoringPanel::Start(std::string label, std::vector<std::string> args, std::function<void()> completion)
{
	ToolRequest request{Path(m_Preferences.Python), Path(m_Preferences.SDK), m_Editor.m_ProjectPath,
						std::move(args), label};
	if (!m_Tools.Start(std::move(request)))
	{
		m_Editor.ActionFailed("A tool operation is already running");
		return;
	}
	m_Completion = std::move(completion);
	m_Output = label + " in progress. Output streams below.\nCancellation is unavailable; closing the editor "
					   "waits for completion.";
	m_ShowOutput = true;
}
void AuthoringPanel::Tick()
{
	std::string progress;
	if (m_Tools.ReadProgress(progress))
		m_Output = m_Tools.Request().Label + " in progress.\n" + progress;
	if (!m_Tools.Poll(m_Report))
		return;
	m_Output = m_Report.Output + "\n" +
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
		   Path(m_Preferences.SDK).is_absolute() &&
		   std::filesystem::is_regular_file(Path(m_Preferences.SDK) / "scripts/hazel.py");
}
void AuthoringPanel::ToolStatus(bool exporting)
{
	ImGui::TextWrapped("Python: %s", m_Report.Python.Executable.empty()
										 ? "Not validated"
										 : m_Report.Python.Executable.generic_u8string().c_str());
	if (m_Report.Python)
		ImGui::TextWrapped("%s | %s", m_Report.Python.Version.c_str(), m_Report.Python.Source.c_str());
	else
		ImGui::TextWrapped(
			"%s", m_Report.Python.Error.empty()
					  ? "Python 3.9+ is required for tooling. Editing and precompiled Play remain available."
					  : m_Report.Python.Error.c_str());
	bool sdk = Path(m_Preferences.SDK).is_absolute() &&
			   std::filesystem::is_regular_file(Path(m_Preferences.SDK) / "scripts/hazel.py");
	ImGui::TextWrapped("SDK: %s",
					   sdk ? m_Preferences.SDK.c_str()
						   : "Missing. Configure a Hazel source SDK; this is separate from Python.");
	ImGui::TextWrapped("Script authoring also needs the SDK's built ScriptCore and Mono/.NET targeting pack. "
					   "Export builds Release with the host C++ compiler. Failures appear in Output.");
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
	if (m_Pending)
		return;
	if (m_Tools.Busy())
	{
		m_ExitAfterJob = true;
		m_ShowOutput = true;
		return;
	}
	Guard([] { Application::Get().Close(); });
}
void AuthoringPanel::Shortcuts()
{
	const auto &io = ImGui::GetIO();
	if (!io.KeyCtrl || io.WantTextInput || m_Pending)
		return;
	if (ImGui::IsKeyPressed(ImGuiKey_O, false))
		Guard([this] { m_Editor.OpenProject(); });
	if (ImGui::IsKeyPressed(ImGuiKey_N, false))
		Guard([this] {
			m_Editor.NewScene();
			MarkSceneSaved();
		});
	if (ImGui::IsKeyPressed(ImGuiKey_S, false))
	{
		if (io.KeyShift)
			m_Editor.SaveSceneAs();
		else
			m_Editor.SaveScene();
	}
	if (ImGui::IsKeyPressed(ImGuiKey_D, false))
		m_Editor.OnDuplicateEntity();
	if (ImGui::IsKeyPressed(ImGuiKey_R, false))
		m_Editor.ReloadScripts();
}
void AuthoringPanel::FileMenu()
{
	if (ImGui::MenuItem("New Project..."))
	{
		m_ShowNew = true;
		Preflight();
	}
	if (ImGui::MenuItem("Open Project...", "Ctrl+O"))
		Guard([this] { m_Editor.OpenProject(); });
	if (ImGui::BeginMenu("Recent Projects"))
	{
		std::string remove;
		for (auto &path : m_Preferences.RecentProjects)
		{
			bool exists = std::filesystem::is_regular_file(Path(path));
			if (ImGui::MenuItem(path.c_str(), exists ? nullptr : "Missing", false, exists))
				Guard([this, path] { m_Editor.OpenProject(Path(path)); });
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
			m_Preferences.RecentProjects.erase(
				std::remove(m_Preferences.RecentProjects.begin(), m_Preferences.RecentProjects.end(), remove),
				m_Preferences.RecentProjects.end());
			try
			{
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
	if (ImGui::MenuItem("Save Project", nullptr, false, Project::GetActive() != nullptr))
		m_Editor.SaveProject();
	ImGui::Separator();
	if (ImGui::MenuItem("New Scene", "Ctrl+N"))
		Guard([this] {
			m_Editor.NewScene();
			MarkSceneSaved();
		});
	if (ImGui::MenuItem("Open Scene..."))
		Guard([this] { m_Editor.OpenScene(); });
	if (ImGui::MenuItem("Save Scene", "Ctrl+S"))
		m_Editor.SaveScene();
	if (ImGui::MenuItem("Save Scene As...", "Ctrl+Shift+S"))
		m_Editor.SaveSceneAs();
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
		}
		if (ImGui::MenuItem("Create Script...", nullptr, false, project))
			m_ShowScripts = true;
		if (ImGui::MenuItem("Build Scripts...", nullptr, false, project))
		{
			m_ShowBuild = true;
			Preflight();
		}
		if (ImGui::MenuItem("Reload Scripts", "Ctrl+R", false,
							ScriptEngine::IsInitialized() && !m_Tools.Busy()))
			m_Editor.ReloadScripts();
		if (ImGui::MenuItem("Export Game...", nullptr, false, project))
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
	ImGui::TextWrapped("User scope. Saved in %s. Project descriptors stay portable; existing dock layouts "
					   "remain in imgui.ini.",
					   EditorPreferences::Location().generic_u8string().c_str());
	PathInput("Python executable", m_Draft.Python, false);
	ImGui::TextWrapped("Blank selects deterministic platform discovery. An invalid explicit path is an "
					   "error; no fallback is concealed.");
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
	ImGui::TextWrapped("Blank uses the OS default for .cs files. A configured editor receives the source "
					   "file as one argument.");
	ImGui::SliderFloat("UI scale", &m_Draft.UIScale, .8f, 2, "%.2f");
	ImGui::Checkbox("Show physics colliders", &m_Draft.ShowColliders);
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
	ImGui::TextWrapped("Detected renderer (read-only): %s / %s. Texture slots %u; maximum texture size %u. "
					   "These facts are not saved as project settings.",
					   caps.Vendor.c_str(), caps.Device.c_str(), caps.MaxTextureSlots, caps.MaxTextureSize);
	ImGui::End();
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
	ImGui::TextWrapped("Project scope. Saved in the portable .hproj. Changes apply on the next open/Play. "
					   "Machine SDK and Python paths belong in Editor Preferences.");
	ImGui::InputText("Display name", &m_ProjectName);
	ImGui::InputText("Script build identifier", &m_ScriptProject, ImGuiInputTextFlags_ReadOnly);
	ImGui::TextWrapped(
		"Set at creation to match the canonical Premake workspace/assembly. Renaming a build "
		"workspace is an SDK development operation. Create Script uses this project's source folder.");
	ImGui::InputText("Assets folder", &m_AssetDirectory);
	auto root = m_Editor.m_ProjectPath.parent_path() / Path(m_AssetDirectory);
	if (ImGui::BeginCombo("Startup scene", m_Startup.c_str()))
	{
		std::error_code error;
		for (auto it = std::filesystem::recursive_directory_iterator(root, error);
			 !error && it != std::filesystem::recursive_directory_iterator(); it.increment(error))
			if (it->path().extension() == ".hazel" &&
				ImGui::Selectable(it->path().lexically_relative(root).generic_u8string().c_str()))
				m_Startup = it->path().lexically_relative(root).generic_u8string();
		ImGui::EndCombo();
	}
	if (ImGui::BeginCombo("Script assembly", m_Module.c_str()))
	{
		std::error_code error;
		for (auto it = std::filesystem::recursive_directory_iterator(root, error);
			 !error && it != std::filesystem::recursive_directory_iterator(); it.increment(error))
			if (it->path().extension() == ".dll" &&
				ImGui::Selectable(it->path().lexically_relative(root).generic_u8string().c_str()))
				m_Module = it->path().lexically_relative(root).generic_u8string();
		ImGui::EndCombo();
	}
	bool valid = !m_ProjectName.empty() && Portable(m_AssetDirectory) && Portable(m_Startup) &&
				 Portable(m_Module) && Contained(m_Editor.m_ProjectPath.parent_path(), root) &&
				 Contained(root, root / Path(m_Startup)) && Contained(root, root / Path(m_Module)) &&
				 std::filesystem::is_regular_file(root / Path(m_Startup)) &&
				 std::filesystem::is_regular_file(root / Path(m_Module));
	if (!valid)
		ImGui::TextWrapped("Choose existing project-relative assets, startup scene and compiled assembly, "
						   "and a valid script build identifier.");
	ImGui::BeginDisabled(!valid || m_Tools.Busy() || m_Editor.m_SceneState != EditorLayer::SceneState::Edit);
	if (ImGui::Button("Save Project Settings"))
	{
		auto config = Project::GetActive()->GetConfig();
		config.Name = m_ProjectName;
		config.ScriptProject = m_ScriptProject;
		config.StartScene = Path(m_Startup);
		config.AssetDirectory = Path(m_AssetDirectory);
		config.ScriptModulePath = Path(m_Module);
		Guard([this, config] {
			try
			{
				auto previous = Project::GetActive();
				auto candidate = Project::LoadCandidate(m_Editor.m_ProjectPath);
				candidate->GetConfig() = config;
				candidate->LoadScene(config.StartScene);
				if (!ProjectSerializer(candidate).Serialize(m_Editor.m_ProjectPath))
					throw std::runtime_error("Cannot save project settings");
				if (!m_Editor.OpenProject(m_Editor.m_ProjectPath))
				{
					if (!ProjectSerializer(previous).Serialize(m_Editor.m_ProjectPath))
						throw std::runtime_error("Open failed and descriptor rollback failed; restore "
												 "the previous descriptor from version control");
					throw std::runtime_error("Settings could not be applied; previous descriptor and "
											 "editor session were restored");
				}
				m_Output = "Project settings saved and applied.";
				m_ShowOutput = true;
			}
			catch (const std::exception &error)
			{
				m_Editor.ActionFailed(error.what());
			}
		});
	}
	ImGui::EndDisabled();
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
	ImGui::InputText("Display name", &m_Name);
	ImGui::InputText("Technical identifier", &m_Identifier);
	ImGui::TextWrapped("Letters, digits and underscore; start with a letter. Used for the descriptor, "
					   "assembly and C# namespace.");
	ImGui::InputText("Destination project folder", &m_Destination);
	ImGui::PushID("dest");
	if (ImGui::Button("Browse parent folder"))
	{
		auto dir = FileDialogs::SelectFolder();
		if (!dir.empty())
			m_Destination = (Path(dir) / m_Identifier).generic_u8string();
	}
	ImGui::PopID();
	ImGui::TextWrapped(
		"Creates %s.hproj, Assets/Scenes/Start.hazel (primary camera), Scripts/Source/Example.cs, canonical "
		"Premake build, Textures and Prefabs. Builds the initial assembly before opening.",
		m_Identifier.c_str());
	ToolStatus();
	if (!Ready())
		ImGui::TextWrapped(
			"Run Check readiness after configuring tools. Compiler/preflight diagnostics appear in Output.");
	bool valid = !m_Name.empty() && m_Name.size() <= 120 && ScriptSource::ValidIdentifier(m_Identifier) &&
				 Path(m_Destination).is_absolute() && !std::filesystem::exists(Path(m_Destination)) &&
				 std::filesystem::is_directory(Path(m_Destination).parent_path());
	if (!valid)
		ImGui::TextWrapped("Enter a valid name/identifier and a new folder beneath an existing parent. "
						   "Existing destinations are never overwritten.");
	ImGui::BeginDisabled(!valid || !Ready());
	if (ImGui::Button("Create and Open"))
	{
		auto target = Path(m_Destination) / (m_Identifier + ".hproj");
		Guard([this, target] {
			Start("Create project",
				  {"new-project", "--name", m_Name, "--identifier", m_Identifier, "--destination",
				   m_Destination},
				  [this, target] {
					  m_ShowNew = false;
					  m_Editor.OpenProject(target);
				  });
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
		ImGui::InputText("Class name", &m_ScriptName);
		ImGui::InputText("Namespace", &m_Namespace);
		ImGui::TextWrapped("Creates Assets/Scripts/Source/<class>.cs with startup/update/cleanup and a "
						   "prefab Instantiate/Destroy sample. Write gameplay C# in your external editor, "
						   "then Build Scripts and select the compiled class in Properties.");
		ImGui::TextWrapped("Instantiate returns a valid entity with physics ready. Its script starts at "
						   "the next safe boundary; use As<T>() after startup. Destroy invalidates the "
						   "handle immediately and runs cleanup outside callbacks.");
		bool valid = ScriptSource::ValidIdentifier(m_ScriptName) &&
					 ScriptSource::ValidNamespace(m_Namespace) &&
					 !std::filesystem::exists(Project::GetAssetDirectory() / "Scripts/Source" /
											  (m_ScriptName + ".cs"));
		if (!valid)
			ImGui::TextWrapped("Use valid C# identifiers and a class filename that does not exist.");
		ImGui::BeginDisabled(!valid);
		if (ImGui::Button("Create and Open Script"))
			try
			{
				auto path = ScriptSource::Create(Project::GetAssetDirectory(), m_ScriptName, m_Namespace);
				m_Output = "Created " + path.generic_u8string() +
						   ". Build Scripts, then assign its class in Properties.";
				m_ShowOutput = true;
				OpenScript(path);
				m_ShowScripts = false;
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
		ImGui::TextWrapped(
			"Builds the active project's Debug assembly through the configured SDK. Stop Play first. "
			"Successful compilation reloads the assembly; authored fields remain in the scene.");
		ImGui::BeginDisabled(!Ready() || m_Editor.m_SceneState != EditorLayer::SceneState::Edit);
		if (ImGui::Button("Build Scripts"))
		{
			auto project = m_Editor.m_ProjectPath;
			Start("Build Scripts", {"script-build", project.generic_u8string(), "--config", "Debug"},
				  [this, project] {
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
	ImGui::TextWrapped("Other targets require building on that platform; cross-compilation is unavailable. "
					   "Export builds Release engine/runtime and scripts before packaging, using the "
					   "canonical SDK service and two compiler jobs.");
	ImGui::InputText("Output folder", &m_ExportPath);
	ImGui::PushID("output");
	Browse(m_ExportPath, true);
	ImGui::PopID();
	ImGui::InputText("Package name", &m_PackageName);
	ToolStatus(true);
	bool name = !m_PackageName.empty() && std::all_of(m_PackageName.begin(), m_PackageName.end(), [](char c) {
		return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' ||
			   c == '-';
	}) && std::isalnum(static_cast<unsigned char>(m_PackageName[0]));
	bool valid = Path(m_ExportPath).is_absolute() && name;
	if (!valid)
		ImGui::TextWrapped("Select an absolute output folder and a portable package name (letters, digits, "
						   "underscore or hyphen; start with a letter/digit).");
	ImGui::BeginDisabled(!valid || !Ready(true) || m_Editor.m_SceneState != EditorLayer::SceneState::Edit);
	if (ImGui::Button("Export Release Game"))
	{
		auto project = m_Editor.m_ProjectPath;
		Guard([this, project] {
			Start("Export Game", {"editor-export", project.generic_u8string(), "--name", m_PackageName,
								  "--output", m_ExportPath});
		});
	}
	ImGui::EndDisabled();
	if (std::filesystem::is_directory(Path(m_ExportPath)) && ImGui::Button("Open Output Folder"))
		FileDialogs::OpenPath(m_ExportPath);
	ImGui::End();
}
void AuthoringPanel::OpenScript(const std::filesystem::path &path)
{
	bool launched = m_Preferences.ScriptEditor.empty()
						? FileDialogs::OpenPath(path.generic_u8string())
						: Process::Launch(Path(m_Preferences.ScriptEditor), {path.generic_u8string()});
	if (!launched)
		m_Editor.ActionFailed("Cannot open script. Configure an external editor in Edit > Editor "
							  "Preferences; source was preserved.");
}
void AuthoringPanel::SelectAsset(const std::filesystem::path &path)
{
	if (path.extension() == ".cs")
	{
		OpenScript(path);
		return;
	}
	if (path.extension() != ".hprefab")
		return;
	Guard(
		[this, path] {
			try
			{
				auto relative = Project::MakeAssetReference(Project::GetAssetDirectory(), path);
				auto scene = Prefab::Load(Project::GetAssetDirectory(), relative);
				m_InitialTransform = Prefab::GetEntity(scene).GetComponent<TransformComponent>();
				m_InitialTransform.Translation.x = m_InitialTransform.Translation.y = 0.0f;
				m_PrefabScene = scene;
				m_PrefabReference = relative.generic_u8string();
				m_PrefabInspector.SetContext(scene);
				m_PrefabInspector.EditScript = m_Editor.m_SceneHierarchyPanel.EditScript;
				m_PrefabInspector.ReportError = [this](const std::string &error) {
					m_Editor.ActionFailed(error);
				};
				m_SavedPrefab = SceneSerializer(scene).SerializeText();
				m_ShowPrefab = true;
			}
			catch (const std::exception &error)
			{
				m_Editor.ActionFailed(error.what());
			}
		},
		false);
}
void AuthoringPanel::CreatePrefab(Entity entity)
{
	if (m_Editor.m_SceneState != EditorLayer::SceneState::Edit)
		return;
	m_PrefabSourceID = entity.GetUUID();
	m_PrefabSourceScene = m_Editor.m_EditorScene->GetIdentity();
	m_PrefabName = "Prefabs/" + entity.GetName() + ".hprefab";
	m_CreatePrefab = true;
}
void AuthoringPanel::InstantiatePrefab(const std::filesystem::path &path)
{
	if (m_Editor.m_SceneState != EditorLayer::SceneState::Edit)
	{
		m_Editor.ActionFailed("Stop Play before authoring prefab instances");
		return;
	}
	try
	{
		auto relative = Project::MakeAssetReference(Project::GetAssetDirectory(), path);
		auto entity = Prefab::Instantiate(Project::GetAssetDirectory(), relative, *m_Editor.m_EditorScene,
										  m_InitialTransform);
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
		ImGui::TextWrapped(
			"Single detached entity; self references remap. External entity references and native scripts "
			"are rejected. Future instances inherit edits; existing instances remain independent.");
		ImGui::InputText("Asset path inside Assets", &m_PrefabName);
		if (ImGui::Button("Browse asset folder"))
		{
			auto folder = FileDialogs::SelectFolder();
			if (!folder.empty())
			{
				m_PrefabName = Project::MakeAssetReference(
					Project::GetAssetDirectory(), Path(folder) / Path(m_PrefabName).filename()).generic_u8string();
			}
		}
		auto source = m_Editor.m_EditorScene->GetIdentity() == m_PrefabSourceScene
						  ? m_Editor.m_EditorScene->GetEntityByUUID(m_PrefabSourceID)
						  : Entity{};
		bool valid = false;
		try
		{
			auto path = Prefab::Resolve(Project::GetAssetDirectory(), Path(m_PrefabName));
			valid = source && !std::filesystem::exists(path);
		}
		catch (const std::exception &error)
		{
			ImGui::TextWrapped("%s", error.what());
		}
		if (!valid)
			ImGui::TextWrapped(
				"Choose a new .hprefab path inside this project's Assets and a valid authored entity.");
		ImGui::BeginDisabled(!valid);
		if (ImGui::Button("Create Prefab Asset"))
			try
			{
				Prefab::Save(Project::GetAssetDirectory(), Path(m_PrefabName), m_Editor.m_EditorScene,
							 source);
				m_CreatePrefab = false;
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
	bool dirty = SceneSerializer(m_PrefabScene).SerializeText() != m_SavedPrefab;
	ImGui::TextWrapped("%s%s", m_PrefabReference.c_str(), dirty ? " * Unsaved" : "");
	ImGui::TextWrapped(
		"Detached instances. Saving affects future instances only; no overrides or automatic propagation.");
	if (ImGui::Button("Save Prefab"))
		try
		{
			Prefab::Save(Project::GetAssetDirectory(), Path(m_PrefabReference), m_PrefabScene,
						 Prefab::GetEntity(m_PrefabScene));
			m_SavedPrefab = SceneSerializer(m_PrefabScene).SerializeText();
			m_Output = "Prefab saved.";
			m_ShowOutput = true;
		}
		catch (const std::exception &error)
		{
			m_Editor.ActionFailed(error.what());
		}
	ImGui::SameLine();
	if (ImGui::Button("Close"))
		Guard(
			[this] {
				m_ShowPrefab = false;
				m_PrefabInspector.SetContext(nullptr);
				m_PrefabScene.reset();
			},
			false);
	if (!m_PrefabScene)
	{
		ImGui::End();
		return;
	}
	m_PrefabInspector.DrawAssetProperties(Prefab::GetEntity(m_PrefabScene));
	ImGui::Separator();
	ImGui::TextUnformatted("Initial instance transform");
	ImGui::DragFloat3("Position", glm::value_ptr(m_InitialTransform.Translation), .1f);
	ImGui::DragFloat3("Rotation (radians)", glm::value_ptr(m_InitialTransform.Rotation), .05f);
	ImGui::DragFloat3("Scale", glm::value_ptr(m_InitialTransform.Scale), .1f, .001f, 1000);
	ImGui::BeginDisabled(dirty || m_Editor.m_SceneState != EditorLayer::SceneState::Edit);
	if (ImGui::Button("Instantiate and Select"))
		InstantiatePrefab(Project::GetAssetDirectory() / Path(m_PrefabReference));
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
	if (m_Pending)
	{
		ImGui::OpenPopup("Unsaved changes");
	}
	auto *mainViewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowViewport(mainViewport->ID);
	ImGui::SetNextWindowPos(mainViewport->GetCenter(), ImGuiCond_Appearing, {0.5f, 0.5f});
	if (ImGui::BeginPopupModal("Unsaved changes", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::TextWrapped("Save authored scene and prefab changes before continuing?");
		if (ImGui::Button("Save and Continue"))
		{
			bool saved = !m_PendingIncludesScene || m_Editor.SaveScene();
			if (saved && m_PrefabScene)
				try
				{
					Prefab::Save(Project::GetAssetDirectory(), Path(m_PrefabReference), m_PrefabScene,
								 Prefab::GetEntity(m_PrefabScene));
					m_SavedPrefab = SceneSerializer(m_PrefabScene).SerializeText();
				}
				catch (const std::exception &error)
				{
					m_Editor.ActionFailed(error.what());
					saved = false;
				}
			if (saved)
			{
				auto next = std::move(m_Pending);
				m_Pending = {};
				ImGui::CloseCurrentPopup();
				next();
			}
		}
		ImGui::SameLine();
		if (ImGui::Button("Discard and Continue"))
		{
			auto next = std::move(m_Pending);
			m_Pending = {};
			ImGui::CloseCurrentPopup();
			next();
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel"))
		{
			m_Pending = {};
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}
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
			ImGui::TextUnformatted("Tool job running. Other tool jobs/project replacement are disabled.");
		else if (!m_Tools.Request().Label.empty())
		{
			ImGui::TextWrapped("%s: %s", m_Tools.Request().Label.c_str(),
							   m_Report.Success ? "Completed successfully" : "Failed; resolve the diagnostic and retry");
			const auto &arguments = m_Tools.Request().Arguments;
			if (m_Report.Success && !arguments.empty() && arguments[0] == "editor-export")
			{
				ImGui::TextWrapped("Artifacts: %s", arguments.back().c_str());
				if (ImGui::Button("Open Output Folder") && !FileDialogs::OpenPath(arguments.back()))
					m_Editor.ActionFailed("Cannot open the output folder. Copy its artifact path from Output.");
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
