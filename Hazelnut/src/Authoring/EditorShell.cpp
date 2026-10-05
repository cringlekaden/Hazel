#include "AuthoringPanel.h"
#include "EditorLayer.h"
#include "Hazel/Scripting/ScriptEngine.h"
#include "UI/PropertyUI.h"
#include <algorithm>
#include <imgui.h>

namespace Hazel
{
void AuthoringPanel::Toolbar()
{
    auto button = [&](const char *label, EditorAction action, const char *hint,
                      const std::function<void()> &invoke)
    {
        const auto available = Availability(action);
        ImGui::BeginDisabled(!available);
        const bool clicked = ImGui::Button(label);
        ImGui::EndDisabled();
        PropertyUI::Help(available.Reason ? available.Reason : hint);
        if (clicked && Require(action))
            invoke();
    };
    const float right = ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x;
    const auto width = [](const char *text)
    { return ImGui::CalcTextSize(text).x + ImGui::GetStyle().FramePadding.x * 2; };
    const float overflow = width("More...") + ImGui::GetStyle().ItemSpacing.x;
    const auto fits = [&](const char *text)
    {
        return ImGui::GetItemRectMax().x + ImGui::GetStyle().ItemSpacing.x + width(text) + overflow <=
               right;
    };
    const auto secondary = [&](const char *label, EditorAction action, const char *hint,
                               const std::function<void()> &invoke)
    {
        if (fits(label))
        {
            ImGui::SameLine();
            button(label, action, hint, invoke);
        }
    };
    const auto saveName = "Save " + ActiveName();
    const bool named = width(saveName.c_str()) + width("Play") + overflow + ImGui::GetFontSize() * 2 <
                       ImGui::GetContentRegionAvail().x;
    button(named ? saveName.c_str() : "Save",
           m_ActiveDocument == EditorDocument::Scene ? EditorAction::SaveScene : EditorAction::SaveAsset,
           ("Ctrl+S: " + ActiveName()).c_str(), [this] { SaveActive(); });
    if (fits("Play"))
        ImGui::SameLine();
    if (m_Editor.m_SceneState == EditorLayer::SceneState::Edit)
        button("Play", EditorAction::Play, "Runs current scene draft; referenced assets use saved files",
               [this] { m_Editor.OnScenePlay(); });
    else
        button("Stop", EditorAction::RuntimeControl, "Return to authored editor scene",
               [this] { m_Editor.OnSceneStop(); });
    if (m_Editor.m_SceneState != EditorLayer::SceneState::Edit)
    {
        secondary(m_Editor.m_ActiveScene->IsPaused() ? "Resume" : "Pause", EditorAction::RuntimeControl,
                  "Pause / resume",
                  [this] { m_Editor.m_ActiveScene->SetPaused(!m_Editor.m_ActiveScene->IsPaused()); });
        if (m_Editor.m_ActiveScene->IsPaused())
            secondary("Step", EditorAction::RuntimeControl, "Advance one update while paused",
                      [this] { m_Editor.m_ActiveScene->Step(); });
    }
    secondary("Save All...", EditorAction::Browse, "Ctrl+Alt+S: review dirty documents",
              [this] { SaveAll(); });
    secondary("Add Entity", EditorAction::EditScene, "Create and select an empty entity",
              [this] { m_Editor.m_SceneHierarchyPanel.AddEntity(); });
    secondary("Simulate", EditorAction::Simulate,
              "Preview physics using current scene draft and saved assets",
              [this] { m_Editor.OnSceneSimulate(); });
    secondary("Build Scripts...", EditorAction::StartTool,
              "Check tools, then build the project's Debug scripts",
              [this]
              {
                  m_ShowBuild = true;
                  Preflight();
              });
    secondary("Export...", EditorAction::StartTool, "Export saved files with explicit draft handling",
              [this]
              {
                  m_ShowExport = true;
                  Preflight(true);
              });
    if (ImGui::GetItemRectMax().x + ImGui::GetStyle().ItemSpacing.x + width("More...") <= right)
        ImGui::SameLine();
    if (ImGui::Button("More..."))
        ImGui::OpenPopup("Toolbar actions");
    if (ImGui::BeginPopup("Toolbar actions"))
    {
        button("Save All...", EditorAction::Browse, "Review the eligible dirty documents",
               [this]
               {
                   SaveAll();
                   ImGui::CloseCurrentPopup();
               });
        button("Add Entity", EditorAction::EditScene, "Create and select an entity",
               [this]
               {
                   m_Editor.m_SceneHierarchyPanel.AddEntity();
                   ImGui::CloseCurrentPopup();
               });
        button("Simulate", EditorAction::Simulate, "Preview physics",
               [this]
               {
                   m_Editor.OnSceneSimulate();
                   ImGui::CloseCurrentPopup();
               });
        button("Build Scripts...", EditorAction::StartTool, "Check build prerequisites",
               [this]
               {
                   m_ShowBuild = true;
                   Preflight();
                   ImGui::CloseCurrentPopup();
               });
        button("Export...", EditorAction::StartTool, "Check export prerequisites",
               [this]
               {
                   m_ShowExport = true;
                   Preflight(true);
                   ImGui::CloseCurrentPopup();
               });
        if (m_Editor.m_SceneState != EditorLayer::SceneState::Edit)
        {
            button(m_Editor.m_ActiveScene->IsPaused() ? "Resume" : "Pause", EditorAction::RuntimeControl,
                   "Pause / resume",
                   [this] { m_Editor.m_ActiveScene->SetPaused(!m_Editor.m_ActiveScene->IsPaused()); });
            if (m_Editor.m_ActiveScene->IsPaused())
                button("Step", EditorAction::RuntimeControl, "Advance one update",
                       [this] { m_Editor.m_ActiveScene->Step(); });
        }
        ImGui::EndPopup();
    }
}
void AuthoringPanel::Status()
{
    const auto docs = Documents();
    const auto dirty =
        std::count_if(docs.begin(), docs.end(), [](const auto &doc) { return doc.Dirty; });
    const auto project = Project::GetActive() ? Project::GetActive()->GetConfig().Name : "No project";
    const auto scene = m_Editor.m_EditorScenePath.empty()
                           ? std::string("Untitled scene")
                           : m_Editor.m_EditorScenePath.filename().u8string();
    const auto sceneDoc = std::find_if(docs.begin(), docs.end(), [](const auto &doc)
                                       { return doc.Kind == EditorDocument::Scene; });
    const std::string title = project + " — " + scene +
                              (sceneDoc != docs.end() && sceneDoc->Dirty ? " *" : "") + " — Hazelnut";
    if (title != m_LastTitle)
    {
        Application::Get().GetWindow().SetTitle(title);
        m_LastTitle = title;
    }
    ImGui::BeginChild("Editor status",
                      {0, ImGui::GetTextLineHeightWithSpacing() + ImGui::GetStyle().FramePadding.y * 2},
                      false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    const char *mode = m_Editor.m_SceneState == EditorLayer::SceneState::Edit   ? "Edit"
                       : m_Editor.m_SceneState == EditorLayer::SceneState::Play ? "Play"
                                                                                : "Simulate";
    const std::string operation =
        m_Tools.Busy() ? m_Tools.Request().Label + " in progress"
        : m_Tools.Request().Label.empty()
            ? ""
            : m_Tools.Request().Label + (m_Report.Success ? " completed" : " failed");
    const std::string identity = project + " | Scene: " + scene + " | Active: " + ActiveName() + " | " +
                                 mode + " | " + std::to_string(dirty) + " unsaved" +
                                 (operation.empty() ? "" : " | " + operation);
    ImGui::AlignTextToFramePadding();
    if (!operation.empty() || !m_Editor.m_ActionError.empty())
    {
        if (ImGui::SmallButton("View Output"))
            m_ShowOutput = true;
        ImGui::SameLine();
    }
    ImGui::TextUnformatted(identity.c_str());
    PropertyUI::Help(identity.c_str());
    ImGui::EndChild();
}
} // namespace Hazel
