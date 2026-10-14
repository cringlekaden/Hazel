#include "AuthoringPanel.h"
#include "EditorLayer.h"
#include "Hazel/Scene/SceneSerializer.h"
#include <imgui.h>
namespace Hazel
{
void AuthoringPanel::CancelEdits()
{
    m_Editor.m_UIEdit.Finish(true);
    m_Editor.m_SceneHierarchyPanel.FinishUIEdit();
    m_PrefabInspector.FinishUIEdit();
    if (m_Editor.m_EditorScene && m_SceneHistory.Active())
        m_SceneHistory.Cancel(*m_Editor.m_EditorScene);
    if (m_PrefabScene && m_PrefabHistory.Active())
        m_PrefabHistory.Cancel(*m_PrefabScene);
}
void AuthoringPanel::PrepareSave()
{
    // Save accepts completed Inspector values; a pointer gesture must be
    // released explicitly and is cancelled before a save/transition boundary.
    m_Editor.m_UIEdit.Finish(true);
    m_Editor.m_SceneHierarchyPanel.FinishUIEdit(false);
    m_PrefabInspector.FinishUIEdit(false);
    if (m_Editor.m_EditorScene && m_SceneHistory.Active())
        if (!m_SceneHistory.Commit(*m_Editor.m_EditorScene))
            m_SceneHistory.Cancel(*m_Editor.m_EditorScene);
    if (m_PrefabScene && m_PrefabHistory.Active())
        if (!m_PrefabHistory.Commit(*m_PrefabScene))
            m_PrefabHistory.Cancel(*m_PrefabScene);
}
DocumentHistory *AuthoringPanel::ActiveHistory()
{
    return m_ActiveDocument == EditorDocument::Scene                     ? &m_SceneHistory
           : m_ActiveDocument == EditorDocument::Prefab && m_PrefabScene ? &m_PrefabHistory
                                                                         : nullptr;
}
Ref<Scene> AuthoringPanel::ActiveHistoryScene() const
{
    return m_ActiveDocument == EditorDocument::Scene    ? m_Editor.m_EditorScene
           : m_ActiveDocument == EditorDocument::Prefab ? m_PrefabScene
                                                        : nullptr;
}
void AuthoringPanel::HistoryFeedback(DocumentHistory &history)
{
    auto message = history.TakeDiagnostic();
    if (!message.empty())
        Notify(message, spdlog::level::warn);
}
bool AuthoringPanel::UndoRedo(bool redo)
{
    auto history = ActiveHistory();
    auto scene = ActiveHistoryScene();
    if (!Require(m_ActiveDocument == EditorDocument::Scene ? EditorAction::EditScene
                                                           : EditorAction::EditAsset))
        return false;
    if (!history || !scene)
        return m_Editor.ActionFailed("Select a scene or prefab document for authoring history");
    if (history->Active() || m_Editor.m_UIEdit.Active())
        return m_Editor.ActionFailed("Finish or cancel the active edit before Undo / Redo");
    const bool applied = redo ? history->Redo(*scene) : history->Undo(*scene);
    if (!applied)
        return m_Editor.ActionFailed(history->Diagnostic());
    m_Editor.m_ActionError.clear();
    return true;
}
void AuthoringPanel::HistoryMenu()
{
    auto history = ActiveHistory();
    const bool editable =
        bool(Availability(m_ActiveDocument == EditorDocument::Scene ? EditorAction::EditScene
                                                                    : EditorAction::EditAsset));
    const bool allowed = editable && history && !history->Active() && !m_Editor.m_UIEdit.Active() &&
                         !ImGui::GetIO().WantTextInput;
    const auto undo = "Undo" + (history && !history->UndoName().empty() ? " " + history->UndoName()
                                                                        : std::string{});
    const auto redo = "Redo" + (history && !history->RedoName().empty() ? " " + history->RedoName()
                                                                        : std::string{});
    if (ImGui::MenuItem(undo.c_str(), "Ctrl+Z", false, allowed && history->CanUndo()))
        UndoRedo(false);
    if (ImGui::MenuItem(redo.c_str(), "Ctrl+Shift+Z / Ctrl+Y", false,
                        allowed && history->CanRedo()))
        UndoRedo(true);
    if (history)
    {
        ImGui::TextDisabled("%zu edits | %.1f KiB retained", history->Operations(),
                            history->UsedBytes() / 1024.0);
        if (!history->SavePointRetained())
            ImGui::TextDisabled("Save point evicted; exact content still determines dirty state");
        if (ImGui::MenuItem("Clear this document's history", nullptr, false,
                            allowed && history->Operations()))
            history->Clear();
    }
    ImGui::Separator();
}
} // namespace Hazel
