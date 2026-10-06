#include "AuthoringPanel.h"
#include "AuthoringReadiness.h"
#include "EditorLayer.h"
#include "Hazel/Core/Resources.h"
#include "Hazel/Project/ProjectCreation.h"
#include "Hazel/Scripting/ScriptEngine.h"
#include "UI/PropertyUI.h"
#include <imgui.h>
namespace Hazel {
    bool AuthoringPanel::CreateProject(const std::string &name, const std::string &identifier,
                                       const std::filesystem::path &destination) {
        if (!Require(EditorAction::CreateProject))
            return false;
        try {
            const auto result = ProjectCreation::Create(
                {name, identifier, destination, Resources::Resolve("Templates")});
            m_CreatedProject = result.Descriptor;
            Notify("Created native project: " + result.Descriptor.generic_u8string() +
                   ". Editing ready; scripts intentionally uncompiled.");
            if (!m_Editor.OpenProject(result.Descriptor)) {
                Notify("Project creation succeeded, but Open failed. Created files and "
                       "previous "
                       "session are retained; cancel the pending operation and use Open "
                       "created "
                       "project.",
                       spdlog::level::warn);
                return false;
            }
            m_ShowNew = false;
            m_CreatedProject.clear();
            return true;
        } catch (const std::exception &e) {
            return m_Editor.ActionFailed(e.what());
        }
    }
    void AuthoringPanel::Readiness() {
        AuthoringReadinessInput input;
        input.Project = bool(Project::GetActive());
        input.Saved = !m_Editor.m_ProjectPath.empty();
        input.ToolSDK = bool(m_SDK);
        input.ToolsChecked = Ready();
        input.AssemblyLoaded = ScriptEngine::IsInitialized();
        input.ClassesAvailable = true;
        if (m_Editor.m_EditorScene)
            for (auto handle : m_Editor.m_EditorScene->GetAllEntitiesWith<ScriptComponent>()) {
                const auto &name = Entity(handle, m_Editor.m_EditorScene.get())
                                       .GetComponent<ScriptComponent>()
                                       .ClassName;
                if (!name.empty()) {
                    input.AssignedScripts = true;
                    input.ClassesAvailable &= ScriptEngine::EntityClassExists(name);
                }
            }
        auto r = AuthoringReadiness::Evaluate(input);
        PropertyUI::ReadOnly("editing-ready", "Editing / saving", r.EditReason.c_str());
        PropertyUI::ReadOnly("scripts-ready", "Build managed scripts", r.ScriptReason.c_str());
        PropertyUI::ReadOnly("reload-ready", "Reload managed scripts",
                             input.AssemblyLoaded
                                 ? "Accepted assembly loaded; reload validates intended module (no SDK)"
                                 : "No accepted assembly; Reload validates intended module (no SDK)");
        PropertyUI::ReadOnly("play-ready", "Play", r.PlayReason.c_str());
        input.ToolsChecked = Ready(true);
        r = AuthoringReadiness::Evaluate(input);
        PropertyUI::ReadOnly("export-ready", "Standalone export", r.ExportReason.c_str());
        ImGui::TextWrapped("Opening/editing needs no source SDK. Reload publishes only a validated "
                           "assembly. Play validates current scene content; export validates saved "
                           "files and their dependencies.");
        if (ImGui::SmallButton("Open Console"))
            m_Console.Show();
    }
} // namespace Hazel
