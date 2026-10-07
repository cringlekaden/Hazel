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
void AuthoringPanel::Caption() {
    auto &window = Application::Get().GetWindow();
    if (window.GetCaptionState().Requested != m_Preferences.CustomCaption)
        window.SetCustomCaption(m_Preferences.CustomCaption);
    const bool custom = window.GetCaptionState().Custom;
    const auto *viewport = ImGui::GetMainViewport();
    const float remaining = ImGui::GetContentRegionAvail().x;
    const float button = ImGui::GetFrameHeight() * 1.5f;
    const float controls = custom ? button * 3 + ImGui::GetStyle().ItemSpacing.x * 3 : 0;
    if (custom && remaining < controls + ImGui::GetFontSize() * 5) {
        window.UseNativeCaption("Native fallback: menus and caption controls need "
                                "more width; enable again in Preferences to retry");
        return;
    }
    const auto start = ImGui::GetCursorScreenPos();
    CaptionLayout layout;
    const auto project =
        Project::GetActive() ? Project::GetActive()->GetConfig().Name : std::string("No project");
    const std::string identity = project + " | " + ActiveName();
    // Clip identity on small windows; the full value remains in status/native
    // title.
    const float width = std::max(0.f, remaining - controls);
    if (width > ImGui::GetFontSize() * 3) {
        const auto end = ImVec2(start.x + width, start.y + ImGui::GetFrameHeight());
        ImGui::GetWindowDrawList()->PushClipRect(start, end, true);
        ImGui::GetWindowDrawList()->AddText(
            {start.x + ImGui::GetStyle().FramePadding.x,
             start.y + ImGui::GetStyle().FramePadding.y},
            ImGui::GetColorU32(window.IsFocused() ? ImGuiCol_Text : ImGuiCol_TextDisabled),
            identity.c_str());
        ImGui::GetWindowDrawList()->PopClipRect();
        ImGui::Dummy({width, ImGui::GetFrameHeight()});
        if (ImGui::CalcTextSize(identity.c_str()).x > width)
            PropertyUI::Help(identity.c_str());
        layout.Drag = {start.x - viewport->Pos.x, start.y - viewport->Pos.y, width,
                       ImGui::GetFrameHeight()};
    }
    if (custom) {
        const float right = ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x;
        ImGui::SetCursorScreenPos({right - controls, start.y});
        bool first = true;
        auto control = [&](const char *id, CaptionRect &rect, CaptionHit hit, auto action,
                           const char *help) {
            if (!first)
                ImGui::SameLine();
            first = false;
            const auto position = ImGui::GetCursorScreenPos();
            const bool hover = window.GetCaptionPointerHit() == hit;
            if (hover)
                ImGui::PushStyleColor(ImGuiCol_Button,
                                      ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered));
            if (ImGui::Button(id, {button, ImGui::GetFrameHeight()}))
                action();
            if (hover)
                ImGui::PopStyleColor();
            rect = {position.x - viewport->Pos.x, position.y - viewport->Pos.y, button,
                    ImGui::GetFrameHeight()};
            auto *draw = ImGui::GetWindowDrawList();
            const auto color = ImGui::GetColorU32(ImGuiCol_Text);
            const float size = ImGui::GetFontSize() * .6f;
            const ImVec2 p{position.x + (button - size) * .5f,
                           position.y + (ImGui::GetFrameHeight() - size) * .5f};
            if (hit == CaptionHit::Minimize)
                draw->AddLine({p.x, p.y + size * .75f}, {p.x + size, p.y + size * .75f}, color);
            else if (hit == CaptionHit::Close) {
                draw->AddLine(p, {p.x + size, p.y + size}, color);
                draw->AddLine({p.x + size, p.y}, {p.x, p.y + size}, color);
            } else if (window.GetPlacement().Maximized) {
                draw->AddRect({p.x + size * .25f, p.y}, {p.x + size, p.y + size * .75f}, color);
                draw->AddRect({p.x, p.y + size * .25f}, {p.x + size * .75f, p.y + size}, color);
            } else
                draw->AddRect(p, {p.x + size, p.y + size}, color);
            if (hover && hit == CaptionHit::Maximize)
                ImGui::SetTooltip("%s", help);
            else
                PropertyUI::Help(help);
        };
        control(
            "##minimize", layout.Minimize, CaptionHit::Minimize, [&] { window.Minimize(); },
            "Minimize; editor documents remain open");
        control(
            "##maximize", layout.Maximize, CaptionHit::Maximize, [&] { window.ToggleMaximize(); },
            "Maximize or restore; Windows 11 offers Snap layouts on hover or Win+Z");
        control(
            "##close", layout.Close, CaptionHit::Close, [&] { window.RequestClose(); },
            "Close through document/job decisions (Alt+F4)");
    }
    window.SetCaptionLayout(custom ? layout : CaptionLayout{});
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
                                 (m_Editor.m_SceneFile.NeedsBackup() || m_Editor.m_ProjectFile.NeedsBackup() || m_Editor.m_SceneLoad.State==DocumentLoadState::EditableWithProblems || m_Editor.m_ProjectLoad.State==DocumentLoadState::EditableWithProblems?" | Recovery/migration — Console":"") +
                                 (operation.empty() ? "" : " | " + operation);
    ImGui::AlignTextToFramePadding();
    if (!operation.empty() || m_Editor.m_Console->Errors() || m_Editor.m_Console->HasFailure() || m_Editor.m_SceneFile.NeedsBackup() || m_Editor.m_ProjectFile.NeedsBackup() || m_Editor.m_SceneLoad.State==DocumentLoadState::EditableWithProblems || m_Editor.m_ProjectLoad.State==DocumentLoadState::EditableWithProblems)
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
    if(!RenderingRestartReason().empty()) {
        if(ImGui::SmallButton("Renderer restart pending"))m_ShowProject=true;
        PropertyUI::Help(RenderingRestartReason().c_str());ImGui::SameLine();
    }
    ImGui::TextUnformatted(identity.c_str());
    if (ImGui::GetItemRectMax().x > ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x)
        PropertyUI::Help(identity.c_str());
    ImGui::EndChild();
}
} // namespace Hazel
