#include "AuthoringPanel.h"
#include "EditorLayer.h"
#include "Hazel/Scene/Prefab.h"
#include "Hazel/Scene/SceneSerializer.h"
#include "Hazel/Core/Resources.h"
#include <algorithm>
namespace Hazel
{
static std::filesystem::path Path(const std::string &value) { return std::filesystem::u8path(value); }
std::vector<DocumentInfo> AuthoringPanel::Documents() const
{
    std::vector<DocumentInfo> docs;
    if (m_Editor.m_EditorScene)
    {
        const auto canSave = Availability(EditorAction::SaveScene);
        docs.push_back({EditorDocument::Scene, m_Editor.m_EditorScene->GetIdentity(),
                        m_Editor.m_EditorScenePath.empty()
                            ? "Untitled scene"
                            : m_Editor.m_EditorScenePath.filename().u8string(),
                        SceneText() != m_SavedScene, canSave ? "" : canSave.Reason});
    }
    const auto canSaveAsset = Availability(EditorAction::SaveAsset);
    if (m_PrefabScene)
        docs.push_back({EditorDocument::Prefab, m_PrefabScene->GetIdentity(), m_PrefabReference,
                        SceneSerializer(m_PrefabScene).SerializeAuthoredSnapshot() != m_SavedPrefab,
                        canSaveAsset ? "" : canSaveAsset.Reason});
    if (m_Sprites.HasDocument())
        docs.push_back({EditorDocument::Sheet, m_Sprites.Identity(), m_Sprites.Name(), m_Sprites.Dirty(),
                        canSaveAsset ? "" : canSaveAsset.Reason});
    return docs;
}
DocumentSaveResult AuthoringPanel::SaveDocument(const DocumentInfo &document)
{
    auto docs = Documents();
    auto found =
        std::find_if(docs.begin(), docs.end(), [&](const auto &doc)
                     { return doc.Kind == document.Kind && doc.Identity == document.Identity; });
    if (found == docs.end())
        return {document, SaveOutcome::Failed, "Document changed identity; retry from its panel"};
    if (!found->SaveUnavailable.empty())
        return {document, SaveOutcome::Unavailable, found->SaveUnavailable};
    try
    {
        bool saved = false;
        switch (document.Kind)
        {
        case EditorDocument::Scene:
            if (!Require(EditorAction::SaveScene))
                return {document, SaveOutcome::Unavailable, m_Editor.m_ActionError};
            saved = m_Editor.SaveScene();
            break;
        case EditorDocument::Sheet:
            saved = m_Sprites.Save();
            break;
        case EditorDocument::Prefab:
            if (!Require(EditorAction::SaveAsset))
                return {document, SaveOutcome::Unavailable, m_Editor.m_ActionError};
            m_PrefabFile.Save(Prefab::Serialize(Project::GetAssetDirectory(), m_PrefabScene,
                         Prefab::GetEntity(m_PrefabScene), true),Resources::Get().UserData/"recovery");
            m_SavedPrefab = SceneSerializer(m_PrefabScene).SerializeAuthoredSnapshot();
            saved = true;
            break;
        }
        if (saved)
        {
            m_Editor.m_ActionError.clear();
            Notify("Saved " + document.Name);
            return {document, SaveOutcome::Saved, "Saved"};
        }
        if (document.Kind == EditorDocument::Scene && m_Editor.m_ActionError.empty())
            return {document, SaveOutcome::Cancelled, "Save As was cancelled; draft retained"};
        if(m_Editor.m_ActionError.find("changed on disk")!=std::string::npos)
        {m_ShowSaveConflict=true;m_ConflictDocument=document.Kind;m_ConflictIdentity=document.Identity;}
        return {document, SaveOutcome::Failed,
                m_Editor.m_ActionError.empty() ? "Save failed; draft retained" : m_Editor.m_ActionError};
    }
    catch (const std::exception &error)
    {
        m_Editor.ActionFailed(error.what());
        if(std::string(error.what()).find("changed on disk")!=std::string::npos)
        {m_ShowSaveConflict=true;m_ConflictDocument=document.Kind;m_ConflictIdentity=document.Identity;}
        return {document, SaveOutcome::Failed, error.what()};
    }
}
void AuthoringPanel::Guard(OperationIntent intent, std::function<bool()> action)
{
    EditorAction needed = EditorAction::OpenAsset;
    switch (intent)
    {
    case OperationIntent::OpenProject:
        needed = EditorAction::ReplaceProject;
        break;
    case OperationIntent::OpenScene:
    case OperationIntent::NewScene:
        needed = EditorAction::ReplaceScene;
        break;
    case OperationIntent::Play:
        needed = EditorAction::Play;
        break;
    case OperationIntent::Simulate:
        needed = EditorAction::Simulate;
        break;
    case OperationIntent::Export:
        needed = EditorAction::StartTool;
        break;
    case OperationIntent::CloseEditor:
        needed = EditorAction::ReplaceProject;
        break;
    case OperationIntent::SaveAll:
        needed = EditorAction::Browse;
        break;
    default:
        break;
    }
    if (!Require(needed))
        return;
    try
    {
        m_Documents.Request(intent, Documents(), [this, needed, action = std::move(action)]
                            { return Require(needed) && action(); });
    }
    catch (const std::exception &error)
    {
        m_Editor.ActionFailed(error.what());
    }
}
void AuthoringPanel::GuardPlay(bool simulate)
{
    Guard(simulate ? OperationIntent::Simulate : OperationIntent::Play, [this, simulate]
          { return simulate ? m_Editor.OnSceneSimulate(true) : m_Editor.OnScenePlay(true); });
}
void AuthoringPanel::SaveAll()
{
    Guard(OperationIntent::SaveAll,
          [this]
          {
              Notify("Save All completed; all listed eligible dirty documents saved.");
              return true;
          });
}
std::string AuthoringPanel::ActiveName() const
{
    for (const auto &doc : Documents())
        if (doc.Kind == m_ActiveDocument)
            return doc.Name + (doc.Dirty ? " *" : "");
    return "No active document";
}
bool AuthoringPanel::SaveActive(bool saveAs)
{
    auto docs = Documents();
    auto found = std::find_if(docs.begin(), docs.end(),
                              [&](const auto &doc) { return doc.Kind == m_ActiveDocument; });
    if (found == docs.end())
        return m_Editor.ActionFailed("Focus a scene, prefab or sprite sheet document first");
    if (saveAs)
    {
        if (m_ActiveDocument != EditorDocument::Scene)
            return m_Editor.ActionFailed("Save As is supported for scenes. Prefab/sheet copies need an "
                                         "explicit new asset identity; save this document in place.");
        return Require(EditorAction::SaveScene) && m_Editor.SaveSceneAs();
    }
    return SaveDocument(*found).Outcome == SaveOutcome::Saved;
}
void AuthoringPanel::ObserveSceneFocus()
{
    if (m_Editor.m_ViewportFocused || m_Editor.m_SceneHierarchyPanel.Focused())
        m_ActiveDocument = EditorDocument::Scene;
}
SceneTarget AuthoringPanel::AssignmentTarget() const
{
    auto entity = m_Editor.m_SceneHierarchyPanel.GetSelectedEntity();
    if (!entity || !entity.BelongsTo(m_Editor.m_EditorScene.get()))
        return {};
    return {m_Editor.m_EditorScene->GetIdentity(), entity.GetUUID()};
}
Entity AuthoringPanel::ResolveTarget(SceneTarget target)
{
    if (!Require(EditorAction::EditScene))
        return {};
    if (!m_Editor.m_EditorScene || target.Scene != m_Editor.m_EditorScene->GetIdentity())
    {
        m_Editor.ActionFailed("The assignment scene changed; select an entity and retry");
        return {};
    }
    auto entity = m_Editor.m_EditorScene->GetEntityByUUID(target.Entity);
    if (!entity)
    {
        m_Editor.ActionFailed("The assignment entity no longer exists; select an entity and retry");
        return {};
    }
    return entity;
}
void AuthoringPanel::ClosePrefab()
{
    m_ShowPrefab = false;
    m_PrefabFocused = false;
    m_PrefabInspector.SetContext(nullptr);
    m_PrefabScene.reset();
    if (m_ActiveDocument == EditorDocument::Prefab)
        m_ActiveDocument = EditorDocument::Scene;
}
} // namespace Hazel
