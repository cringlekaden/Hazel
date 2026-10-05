#include "ConsolePanel.h"
#include "UI/PropertyUI.h"
#include "Hazel/Utils/PlatformUtils.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <misc/cpp/imgui_stdlib.h>
#include <algorithm>
namespace Hazel
{
void ConsolePanel::RefreshRows(bool reset)
{
    const auto &entries = m_Model->Entries();
    if (reset)
    {
        m_Visible.clear();
        m_LastSequence = 0;
    }
    const auto first = entries.empty() ? UINT64_MAX : entries.front().Sequence;
    m_Visible.erase(m_Visible.begin(), std::lower_bound(m_Visible.begin(), m_Visible.end(), first));
    m_Selected.erase(
        std::remove_if(m_Selected.begin(), m_Selected.end(), [&](auto id) { return id < first; }),
        m_Selected.end());
    auto begin = std::upper_bound(entries.begin(), entries.end(), m_LastSequence,
                                  [](auto id, const auto &entry) { return id < entry.Sequence; });
    for (auto iterator = begin; iterator != entries.end(); ++iterator)
        if (m_Filter.Matches(*iterator))
            m_Visible.push_back(iterator->Sequence);
    if (!entries.empty())
        m_LastSequence = entries.back().Sequence;
}
void ConsolePanel::Render()
{
    if (!Visible)
        return;
    // Renaming a window changes its ImGui hash (even with ###). Migrate only
    // this panel's saved settings, once, without rebuilding docks or writing ini.
    const auto id = ImHashStr("Console");
    if (!ImGui::FindWindowByID(id) && !ImGui::FindWindowSettingsByID(id))
    {
        if (const auto *old = ImGui::FindWindowSettingsByID(ImHashStr("Output")))
        {
            const auto previous = *old;
            auto *settings = ImGui::CreateNewWindowSettings("Console");
            *settings = previous;
            settings->ID = id;
            settings->WantDelete = false;
            if (auto *dock = ImGui::DockBuilderGetNode(previous.DockId))
                if (dock->SelectedTabId == previous.ID)
                    dock->SelectedTabId = id;
        }
        else if (auto *stats = ImGui::FindWindowByName("Stats"); stats && stats->DockId)
        {
            // Same first-use placement as Output; existing Console layouts win.
            ImGui::SetNextWindowDockID(stats->DockId, ImGuiCond_FirstUseEver);
        }
    }
    if (m_RevealRequested)
    {
        if (!ImGui::GetTopMostPopupModal())
            ImGui::SetNextWindowFocus();
        m_RevealRequested = false;
    }
    ImGui::SetNextWindowSize({620, 300}, ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Console", &Visible, ImGuiWindowFlags_NoFocusOnAppearing))
    {
        ImGui::End();
        return;
    }
    bool reset = false;
    const auto options = [&](const char *name, unsigned &mask, int count, bool severity) {
        if (ImGui::Button(name))
            ImGui::OpenPopup(name);
        if (ImGui::BeginPopup(name))
        {
            for (int index = 0; index < count; ++index)
            {
                bool enabled = mask & (1u << index);
                const char *label =
                    severity
                        ? spdlog::level::to_string_view(static_cast<spdlog::level::level_enum>(index)).data()
                        : ConsoleModel::SourceName(static_cast<ConsoleSource>(index));
                if (ImGui::Checkbox(label, &enabled))
                {
                    mask ^= 1u << index;
                    reset = true;
                }
            }
            if (ImGui::Button("All"))
            {
                mask = (1u << count) - 1;
                reset = true;
            }
            ImGui::SameLine();
            if (ImGui::Button("None"))
            {
                mask = 0;
                reset = true;
            }
            ImGui::EndPopup();
        }
    };
    options("Severity", m_Filter.Severities, 6, true);
    ImGui::SameLine();
    options("Source", m_Filter.Sources, 7, false);
    PropertyUI::WrapButton("Settings");
    if (ImGui::Button("Settings"))
        ImGui::OpenPopup("Console settings");
    if (ImGui::BeginPopup("Console settings"))
    {
        ImGui::TextWrapped("Native capture admits new engine/application records; display filters only hide "
                           "retained records. Tool output is always collected.");
        PropertyUI::Combo("capture", "Native capture", CaptureLevel,
                          "trace\0debug\0info\0warning\0error\0critical\0");
        ImGui::BeginDisabled(!CapturePreferenceAvailable);
        if (ImGui::Button("Save capture preference"))
            SaveCapture = true;
        ImGui::EndDisabled();
        if (!CapturePreferenceAvailable)
            PropertyUI::Help(
                "Preferences were recovered from an unreadable/unsupported file. Review them in Edit > "
                "Preferences and explicitly Apply and Save before saving this preference.");
        ImGui::TextWrapped("Clear removes retained and queued messages and resets log-error/drop counts. "
                           "Operation results and the pinned failure remain.");
        ImGui::EndPopup();
    }
    const float resetWidth = ImGui::CalcTextSize("Reset filters").x + ImGui::GetStyle().FramePadding.x * 2;
    ImGui::SetNextItemWidth(
        std::max(80.f, ImGui::GetContentRegionAvail().x - resetWidth - ImGui::GetStyle().ItemSpacing.x));
    reset |= ImGui::InputTextWithHint("##search", "Search messages", &m_Filter.Search);
    PropertyUI::WrapButton("Reset filters");
    if (ImGui::Button("Reset filters"))
    {
        m_Filter = {};
        reset = true;
    }
    PropertyUI::Help("Resets severity, source, search and operation filters so every retained record is eligible.");
    RefreshRows(reset);
    if (ImGui::Button("Copy"))
        ImGui::OpenPopup("Copy output");
    if (ImGui::BeginPopup("Copy output"))
    {
        if (ImGui::MenuItem("Copy selected", "Ctrl+C", false, !m_Selected.empty()))
            ImGui::SetClipboardText(m_Model->Copy(m_Filter, m_Selected).c_str());
        if (m_Selected.empty())
            PropertyUI::Help("Select retained messages first; Ctrl-click adds or removes individual rows.");
        if (ImGui::MenuItem("Copy visible"))
            ImGui::SetClipboardText(m_Model->Copy(m_Filter).c_str());
        ImGui::EndPopup();
    }
    PropertyUI::Help("Copies filtered records with full metadata and multiline text. Ctrl+C copies the "
                     "current selection when Console owns keyboard focus.");
    PropertyUI::WrapButton("Clear");
    if (ImGui::Button("Clear"))
    {
        m_Model->Clear();
        m_Selected.clear();
        m_Detail = 0;
        RefreshRows(true);
    }
    PropertyUI::Help("Removes queued and retained messages and resets counters; operation results and the "
                     "pinned failure remain.");
    const float followWidth =
        ImGui::GetFrameHeight() + ImGui::GetStyle().ItemInnerSpacing.x + ImGui::CalcTextSize("Auto-scroll").x;
    if (ImGui::GetItemRectMax().x + ImGui::GetStyle().ItemSpacing.x + followWidth <=
        ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x)
        ImGui::SameLine();
    ImGui::Checkbox("Auto-scroll", &m_Follow);
    PropertyUI::Help("Follows new messages only while at the bottom. Scrolling upward pauses following; "
                     "Latest resumes it.");
    PropertyUI::WrapButton("Latest");
    if (ImGui::Button("Latest"))
    {
        m_Follow = true;
        m_Jump = true;
    }
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    ImGui::TextWrapped("%zu visible / %zu kept | %llu dropped, %llu truncated", m_Visible.size(),
                       m_Model->Entries().size(), static_cast<unsigned long long>(m_Model->Dropped()),
                       static_cast<unsigned long long>(m_Model->Truncated()));
    ImGui::PopStyleColor();
    if(m_Filter.Active()) {
        const auto operation=m_Filter.Operation?" | operation #"+std::to_string(m_Filter.Operation):std::string{};
        ImGui::PushStyleColor(ImGuiCol_Text,{1,.7f,.3f,1});
        ImGui::TextWrapped("Filters hide %zu messages%s",m_Model->Entries().size()-m_Visible.size(),operation.c_str());
        ImGui::PopStyleColor();
    }
    if (m_Revision != m_Model->Revision())
    {
        m_Revision = m_Model->Revision();
        m_OperationCache = m_Model->Operations();
        m_FailureCache = m_Model->LatestFailure();
        m_CheckedArtifact.clear();
    }
    const auto &failure = m_FailureCache;
    if (failure)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, {1, .5f, .4f, 1});
        const auto label =
            std::string(ConsoleModel::OutcomeName(failure->Outcome)) + ": " + failure->Label + "###failure";
        if (ImGui::SmallButton(label.c_str()))
        {
            m_Operations = true;
            m_Operation = failure->ID;
        }
        ImGui::PopStyleColor();
        PropertyUI::Help("Inspect the latest failed operation, independent of message filters, Clear and "
                         "later successes.");
        PropertyUI::WrapButton("Dismiss failure");
        if (ImGui::SmallButton("Dismiss failure"))
            m_Model->DismissFailure();
    }
    if (ExitRequested)
    {
        ImGui::TextWrapped("Exit requested: compiler/export jobs finish before closing.");
        if (ImGui::Button("Keep editor open"))
            CancelExit = true;
    }
    const auto &operations = m_OperationCache;
    if (!operations.empty())
    {
        const auto &current = operations.back();
        const auto duration = current.Outcome == ToolOutcome::Running
                                  ? std::chrono::duration_cast<std::chrono::milliseconds>(
                                        std::chrono::steady_clock::now() - current.ClockStart)
                                  : current.Elapsed;
        if (ImGui::Checkbox("Operations", &m_Operations) && m_Operations)
            m_Operation = current.ID;
        ImGui::SameLine();
        const auto status =
            current.Label + ": " + current.Stage + " | " + std::to_string(duration.count() / 1000) + " s";
        ImGui::TextUnformatted(status.c_str());
        if (ImGui::GetItemRectMax().x > ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x)
            PropertyUI::Help(status.c_str());
        if (m_Operations)
        {
            ImGui::BeginChild("Operations", {0, ImGui::GetFontSize() * 12}, true);
            for (auto it = operations.rbegin(); it != operations.rend(); ++it)
            {
                ImGui::PushID(std::to_string(it->ID).c_str());
                const auto label = it->Label + " — " + ConsoleModel::OutcomeName(it->Outcome);
                if (ImGui::Selectable(label.c_str(), m_Operation == it->ID))
                    m_Operation = it->ID;
                ImGui::PopID();
            }
            auto chosen = std::find_if(operations.begin(), operations.end(),
                                       [&](const auto &op) { return op.ID == m_Operation; });
            const ConsoleOperation *detail =
                chosen != operations.end() ? &*chosen
                                           : (failure && failure->ID == m_Operation ? &*failure : nullptr);
            if (detail)
            {
                const auto elapsed = detail->Outcome == ToolOutcome::Running
                                         ? std::chrono::duration_cast<std::chrono::milliseconds>(
                                               std::chrono::steady_clock::now() - detail->ClockStart)
                                         : detail->Elapsed;
                ImGui::TextWrapped("Operation %llu | %s | %.1f seconds",
                                   static_cast<unsigned long long>(detail->ID), detail->Stage.c_str(),
                                   elapsed.count() / 1000.0);
                ImGui::Separator();
                ImGui::TextWrapped("Project: %s\nSDK: %s\nPython: %s\nTool arguments: %s\nStarted: %s",
                                   detail->Project.c_str(), detail->SDK.c_str(),
                                   detail->EffectivePython.c_str(), detail->Command.c_str(),
                                   ConsoleModel::Timestamp(detail->Start, true).c_str());
                ImGui::TextWrapped("Python selection: %s | override: %s", detail->PythonSource.c_str(),
                                   detail->Python.empty() ? "Automatic" : detail->Python.c_str());
                if (detail->Outcome != ToolOutcome::Running)
                    ImGui::TextWrapped("Ended: %s | exit %d",
                                       ConsoleModel::Timestamp(detail->End, true).c_str(), detail->ExitCode);
                if (ImGui::Button("Show operation messages"))
                {
                    m_Filter.Operation = detail->ID;
                    reset = true;
                }
                PropertyUI::WrapButton("Copy result");
                if (ImGui::Button("Copy result"))
                    ImGui::SetClipboardText((detail->Label + " (#" + std::to_string(detail->ID) + ") — " +
                                             ConsoleModel::OutcomeName(detail->Outcome) + "\nProject: " +
                                             detail->Project + "\nTool arguments: " + detail->Command +
                                             "\nExit: " + std::to_string(detail->ExitCode) +
                                             "\nStarted: " + ConsoleModel::Timestamp(detail->Start, true) +
                                             "\n" + detail->Diagnostic + "\nArtifacts: " + detail->Artifact)
                                                .c_str());
                if (!detail->Artifact.empty())
                {
                    ImGui::TextWrapped("Artifacts: %s", detail->Artifact.c_str());
                    std::error_code error;
                    const auto folder = std::filesystem::u8path(detail->Artifact);
                    if (m_CheckedArtifact != detail->Artifact)
                    {
                        m_CheckedArtifact = detail->Artifact;
                        m_ArtifactFolder = folder;
                        if (folder.is_absolute() && std::filesystem::is_regular_file(folder, error))
                            m_ArtifactFolder = folder.parent_path();
                        m_FolderExists = m_ArtifactFolder.is_absolute() &&
                                         std::filesystem::is_directory(m_ArtifactFolder, error);
                    }
                    const bool exists = m_FolderExists;
                    ImGui::BeginDisabled(!exists);
                    if (ImGui::Button("Open output folder"))
                    {
                        m_FolderExists = std::filesystem::is_directory(m_ArtifactFolder, error);
                        if (!m_FolderExists || !FileDialogs::OpenPath(m_ArtifactFolder.generic_u8string()))
                            Log::GetClientLogger()->log(spdlog::source_loc{"Authoring", 0, ""},
                                                        spdlog::level::err, "Cannot open output folder: {}",
                                                        detail->Artifact);
                    }
                    ImGui::EndDisabled();
                    if (!exists)
                        PropertyUI::Help("The artifact folder is missing or unavailable; copy its recorded "
                                         "location instead.");
                }
                ImGui::TextWrapped("%s", detail->Diagnostic.c_str());
            }
            ImGui::EndChild();
        }
    }
    if (reset)
        RefreshRows(true);
    const auto &entries = m_Model->Entries();
    const auto find = [&](uint64_t id) -> const ConsoleEntry * {
        auto it = std::lower_bound(entries.begin(), entries.end(), id,
                                   [](const auto &entry, auto key) { return entry.Sequence < key; });
        return it != entries.end() && it->Sequence == id ? &*it : nullptr;
    };
    if (m_Detail && !find(m_Detail))
        m_Detail = 0;
    const float available = ImGui::GetContentRegionAvail().y;
    ImGui::BeginChild("Messages", {0, m_Detail ? std::max(ImGui::GetFontSize() * 3, available * .55f) : 0},
                      true);
    const bool atBottom = ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 1;
    if (ImGui::IsWindowHovered() && ImGui::GetIO().MouseWheel > 0)
        m_Follow = false;
    const bool compact = ImGui::GetContentRegionAvail().x < ImGui::GetFontSize() * 36;
    if (ImGui::BeginTable("Records", compact ? 1 : 4,
                          ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp))
    {
        if (!compact)
        {
            ImGui::TableSetupColumn("Time", ImGuiTableColumnFlags_WidthFixed, ImGui::GetFontSize() * 6);
            ImGui::TableSetupColumn("Source", ImGuiTableColumnFlags_WidthFixed, ImGui::GetFontSize() * 4);
            ImGui::TableSetupColumn("Level", ImGuiTableColumnFlags_WidthFixed, ImGui::GetFontSize() * 4);
        }
        ImGui::TableSetupColumn("Message");
        ImGui::TableHeadersRow();
        ImGuiListClipper clip;
        clip.Begin(static_cast<int>(m_Visible.size()));
        while (clip.Step())
            for (int index = clip.DisplayStart; index < clip.DisplayEnd; ++index)
                if (const auto *entry = find(m_Visible[index]))
                {
                    ImGui::TableNextRow();
                    ImGui::PushID(std::to_string(entry->Sequence).c_str());
                    if (!compact)
                    {
                        ImGui::TableSetColumnIndex(0);
                        ImGui::TextUnformatted(ConsoleModel::Timestamp(entry->Time).c_str());
                        ImGui::TableSetColumnIndex(1);
                        ImGui::TextUnformatted(ConsoleModel::SourceName(entry->Source));
                        ImGui::TableSetColumnIndex(2);
                        ImGui::TextUnformatted(spdlog::level::to_string_view(entry->Level).data());
                    }
                    ImGui::TableSetColumnIndex(compact ? 0 : 3);
                    auto summary = entry->Message.substr(0, std::min<size_t>(entry->Message.find('\n'), 512));
                    if (compact)
                        summary = std::string(ConsoleModel::SourceName(entry->Source)) + "/" +
                                  spdlog::level::to_string_view(entry->Level).data() + " | " + summary;
                    if (summary.empty())
                        summary = "(empty message)";
                    const bool selected =
                        std::find(m_Selected.begin(), m_Selected.end(), entry->Sequence) != m_Selected.end();
                    if (entry->Level >= spdlog::level::err)
                        ImGui::PushStyleColor(ImGuiCol_Text, {1, .5f, .4f, 1});
                    else if (entry->Level == spdlog::level::warn)
                        ImGui::PushStyleColor(ImGuiCol_Text, {1, .75f, .35f, 1});
                    const auto position = ImGui::GetCursorPos();
                    if (ImGui::Selectable("##record", selected, ImGuiSelectableFlags_SpanAllColumns))
                    {
                        if (!ImGui::GetIO().KeyCtrl)
                            m_Selected.clear();
                        if (selected && ImGui::GetIO().KeyCtrl)
                            m_Selected.erase(
                                std::remove(m_Selected.begin(), m_Selected.end(), entry->Sequence),
                                m_Selected.end());
                        else
                            m_Selected.push_back(entry->Sequence);
                        m_Detail = entry->Sequence;
                    }

                    if (ImGui::BeginPopupContextItem("Record actions"))
                    {
                        if (ImGui::MenuItem("Copy message"))
                            ImGui::SetClipboardText(entry->Message.c_str());
                        ImGui::EndPopup();
                    }
                    const auto after = ImGui::GetCursorPos();
                    ImGui::SetCursorPos(position);
                    ImGui::TextUnformatted(summary.c_str());
                    ImGui::SetCursorPos(after);
                    if (entry->Level >= spdlog::level::warn)
                        ImGui::PopStyleColor();
                    ImGui::PopID();
                }
        ImGui::EndTable();
    }
    if (m_Jump || (m_Follow && atBottom))
    {
        ImGui::SetScrollHereY(1);
        m_Jump = false;
    }
    else if (!atBottom)
        m_Follow = false;
    ImGui::EndChild();
    if (m_Detail)
    {
        if (ImGui::SmallButton("Hide message detail"))
            m_Detail = 0;
        if (const auto *detail = find(m_Detail))
        {
            ImGui::BeginChild("Message detail", {0, 0}, true);
            ImGui::TextWrapped("%s | %s | sequence %llu | operation %llu",
                               ConsoleModel::Timestamp(detail->Time, true).c_str(),
                               ConsoleModel::SourceName(detail->Source),
                               static_cast<unsigned long long>(detail->Sequence),
                               static_cast<unsigned long long>(detail->Operation));
            ImGui::TextDisabled("Session elapsed: %.3f seconds", detail->Elapsed.count() / 1000.0);
            ImGui::TextWrapped("%s", detail->Message.c_str());
            ImGui::EndChild();
        }
    }
    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && !ImGui::GetIO().WantTextInput &&
        !m_Selected.empty() && ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_C, false))
        ImGui::SetClipboardText(m_Model->Copy(m_Filter, m_Selected).c_str());
    ImGui::End();
}
} // namespace Hazel
