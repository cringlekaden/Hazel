#include "EditorDocuments.h"
#include <algorithm>
#include <exception>

namespace Hazel
{
bool EditorDocuments::Affects(OperationIntent intent, EditorDocument kind)
{
    switch (intent)
    {
    case OperationIntent::OpenScene:
    case OperationIntent::NewScene:
        return kind == EditorDocument::Scene;
    case OperationIntent::OpenPrefab:
    case OperationIntent::ClosePrefab:
        return kind == EditorDocument::Prefab;
    case OperationIntent::OpenSheet:
    case OperationIntent::CloseSheet:
        return kind == EditorDocument::Sheet;
    case OperationIntent::Play:
    case OperationIntent::Simulate:
        return kind != EditorDocument::Scene;
    default:
        return true;
    }
}
bool EditorDocuments::AllowsSaved(OperationIntent intent)
{
    return intent == OperationIntent::Play || intent == OperationIntent::Simulate ||
           intent == OperationIntent::Export;
}
bool EditorDocuments::AllowsDiscard(OperationIntent intent)
{
    return intent != OperationIntent::SaveAll && !AllowsSaved(intent);
}
const char *EditorDocuments::Description(OperationIntent intent)
{
    switch (intent)
    {
    case OperationIntent::SaveAll:
        return "Save All";
    case OperationIntent::OpenProject:
        return "Open / replace project";
    case OperationIntent::OpenScene:
        return "Open / replace scene";
    case OperationIntent::NewScene:
        return "Create new scene";
    case OperationIntent::OpenPrefab:
        return "Open / replace prefab draft";
    case OperationIntent::OpenSheet:
        return "Open / replace sheet draft";
    case OperationIntent::ClosePrefab:
        return "Close prefab document";
    case OperationIntent::CloseSheet:
        return "Close sheet document";
    case OperationIntent::CloseEditor:
        return "Close editor";
    case OperationIntent::Play:
        return "Play current scene draft using saved assets";
    case OperationIntent::Simulate:
        return "Simulate current scene draft using saved assets";
    case OperationIntent::Export:
        return "Export saved project files";
    }
    return "";
}
bool EditorDocuments::Request(OperationIntent intent, const std::vector<DocumentInfo> &documents,
                              Action action)
{
    if (Pending())
        return false;
    m_Intent = intent;
    m_Results.clear();
    m_Error.clear();
    m_Affected.clear();
    for (const auto &doc : documents)
        if (doc.Dirty && Affects(intent, doc.Kind))
            m_Affected.push_back(doc);
    if (m_Affected.empty())
        return action();
    m_Action = std::move(action);
    return false;
}
bool EditorDocuments::Resolve(GuardChoice choice, const std::vector<DocumentInfo> &current,
                              const Save &save)
{
    if (!Pending())
        return false;
    if (choice == GuardChoice::Cancel)
    {
        m_Action = {};
        m_Affected.clear();
        m_Results.clear();
        m_Error.clear();
        return false;
    }
    if (choice == GuardChoice::UseSaved && !AllowsSaved(m_Intent))
        return false;
    if (choice == GuardChoice::Discard && !AllowsDiscard(m_Intent))
        return false;
    for (const auto &doc : m_Affected)
    {
        auto found = std::find_if(current.begin(), current.end(), [&](const auto &now)
                                  { return now.Kind == doc.Kind && now.Identity == doc.Identity; });
        if (found == current.end())
        {
            m_Error = "A document changed identity. Cancel and retry the operation.";
            return false;
        }
    }
    if (choice == GuardChoice::SaveAndContinue)
    {
        m_Results.clear();
        bool success = true;
        for (const auto &doc : m_Affected)
        {
            auto found = std::find_if(current.begin(), current.end(), [&](const auto &now)
                                      { return now.Kind == doc.Kind && now.Identity == doc.Identity; });
            // Retry saves only remaining dirty documents after partial success.
            DocumentSaveResult result{*found, SaveOutcome::Saved, "Already saved"};
            if (found->Dirty)
                try
                {
                    result = save(*found);
                }
                catch (const std::exception &error)
                {
                    result = {*found, SaveOutcome::Failed, error.what()};
                }
            success &= result.Outcome == SaveOutcome::Saved;
            m_Results.push_back(std::move(result));
        }
        if (!success)
        {
            m_Error = "Some documents were not saved. Successful saves remain saved; "
                      "drafts for "
                      "failed/cancelled saves are retained.";
            return false;
        }
    }
    try
    {
        if (!m_Action())
        {
            m_Error = "Operation did not complete. The previous valid session and "
                      "remaining drafts are retained.";
            return false;
        }
    }
    catch (const std::exception &error)
    {
        m_Error = error.what();
        return false;
    }
    m_Action = {};
    m_Affected.clear();
    m_Error.clear();
    return true;
}
} // namespace Hazel
