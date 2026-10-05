#include "AuthoringPanel.h"
#include "EditorLayer.h"
#include "Hazel/Scripting/ScriptEngine.h"
#include "UI/PropertyUI.h"
#include <algorithm>
#include <imgui.h>

namespace Hazel
{
ActionAvailability AuthoringPanel::RuntimeAvailability(RuntimeAction action) const
{
    if (action == RuntimeAction::Play) return Availability(EditorAction::Play);
    if (action == RuntimeAction::Simulate) return Availability(EditorAction::Simulate);
    const auto available = Availability(EditorAction::RuntimeControl);
    if (!available) return available;
    if (!m_Editor.m_ActiveScene) return {"No runtime scene"};
    if (action == RuntimeAction::Step && !m_Editor.m_ActiveScene->IsPaused())
        return {"Pause first to advance one update"};
    return {};
}
bool AuthoringPanel::InvokeRuntime(RuntimeAction action)
{
    const auto available = RuntimeAvailability(action);
    if (!available) return m_Editor.ActionFailed(available.Reason);
    switch (action)
    {
    case RuntimeAction::Play: return m_Editor.OnScenePlay();
    case RuntimeAction::Simulate: return m_Editor.OnSceneSimulate();
    case RuntimeAction::Stop:
        m_Editor.OnSceneStop();
        return m_Editor.m_SceneState == EditorLayer::SceneState::Edit;
    case RuntimeAction::TogglePause:
        m_Editor.m_ActiveScene->SetPaused(!m_Editor.m_ActiveScene->IsPaused());
        return true;
    case RuntimeAction::Step:
        m_Editor.m_ActiveScene->Step();
        return true;
    }
    return false;
}
void AuthoringPanel::Toolbar()
{
    const bool editing = m_Editor.m_SceneState == EditorLayer::SceneState::Edit;
    const bool paused = !editing && m_Editor.m_ActiveScene && m_Editor.m_ActiveScene->IsPaused();
    const char *mode = editing ? "Edit"
                       : m_Editor.m_SceneState == EditorLayer::SceneState::Play
                             ? (paused ? "Play (paused)" : "Play")
                             : (paused ? "Simulate (paused)" : "Simulate");
    const float size = std::clamp(ImGui::GetContentRegionAvail().y - 4.f,
                                  ImGui::GetFontSize() * .85f, ImGui::GetFontSize() * 1.25f);
    const float spacing = ImGui::GetStyle().ItemSpacing.x;
    const int count = editing ? 2 : (paused ? 3 : 2);
    const float controls = count * (size + 4) + (count - 1) * spacing;
    const float availableWidth = ImGui::GetContentRegionAvail().x;
    const bool showMode = availableWidth >= controls + spacing + ImGui::CalcTextSize(mode).x;
    const float group = controls + (showMode ? spacing + ImGui::CalcTextSize(mode).x : 0);
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::max(0.f, (availableWidth - group) * .5f));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {2, 2});
    ImGui::PushStyleColor(ImGuiCol_Button, {0, 0, 0, 0});
    const auto image = [&](const char *id, const Ref<Texture2D> &icon, RuntimeAction action, const char *help)
    {
        const auto available = RuntimeAvailability(action);
        ImGui::BeginDisabled(!available);
        const bool clicked = ImGui::ImageButton(id, (ImTextureID)(uintptr_t)icon->GetRendererID(),
                                                {size, size});
        ImGui::EndDisabled();
        PropertyUI::Help(available.Reason ? available.Reason : help);
        if (clicked) InvokeRuntime(action); // Revalidate, then use the shared guarded production action.
    };
    if (editing)
    {
        image("play", m_Editor.m_IconPlay, RuntimeAction::Play,
              "Run the current scene draft; referenced assets use saved files. Unsaved assets prompt first.");
        ImGui::SameLine();
        image("simulate", m_Editor.m_IconSimulate, RuntimeAction::Simulate,
              "Preview physics without scripts; uses the scene draft and saved assets. Unsaved assets prompt first.");
    }
    else
    {
        image("stop", m_Editor.m_IconStop, RuntimeAction::Stop,
              "Stop runtime and return to the retained authored scene and selection.");
        ImGui::SameLine();
        image("pause-resume", paused ? m_Editor.m_IconPlay : m_Editor.m_IconPause,
              RuntimeAction::TogglePause, paused ? "Resume runtime updates." : "Pause runtime updates; enables single-step.");
        if (paused)
        {
            ImGui::SameLine();
            image("step", m_Editor.m_IconStep, RuntimeAction::Step, "Advance one update while remaining paused.");
        }
    }
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
    if (showMode)
    {
        ImGui::SameLine();
        ImGui::AlignTextToFramePadding();
        ImGui::TextColored(editing ? ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled)
                                  : ImVec4(.5f, .8f, .6f, 1), "%s", mode);
    }
    // Status/document UI retains identity, dirty state and tool progress outside this toolbar.
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
    const std::string operation = m_Editor.m_Console->Status();
    const std::string identity = project + " | Scene: " + scene + " | Active: " + ActiveName() + " | " +
                                 mode + " | " + std::to_string(dirty) + " unsaved" +
                                 (operation.empty() ? "" : " | " + operation);
    ImGui::AlignTextToFramePadding();
    if (!operation.empty() || m_Editor.m_Console->Errors() || m_Editor.m_Console->HasFailure())
    {
        const auto count=m_Editor.m_Console->Errors();
        const bool failure=m_Editor.m_Console->HasFailure();
        const auto button=count?"Console ("+std::to_string(count)+" errors)":failure?std::string("Console — failed job"):std::string("Console");
        if(count||failure)ImGui::PushStyleColor(ImGuiCol_Text,{1,.55f,.4f,1});
        if (ImGui::SmallButton(button.c_str()))
            m_Console.Show();
        if(count||failure)ImGui::PopStyleColor();
        PropertyUI::Help("Open Console without interrupting the active document. Error count includes captured errors since Clear, even if filtered or dropped; a failed operation stays pinned until dismissed.");
        ImGui::SameLine();
    }
    ImGui::TextUnformatted(identity.c_str());
    if (ImGui::GetItemRectMax().x > ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x)
        PropertyUI::Help(identity.c_str());
    ImGui::EndChild();
}
} // namespace Hazel
