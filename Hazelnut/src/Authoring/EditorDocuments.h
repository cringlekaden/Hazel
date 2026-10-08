#pragma once
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace Hazel
{
enum class EditorDocument
{
    Scene,
    Prefab,
    Sheet
};
enum class OperationIntent
{
    SaveAll,
    OpenProject,
    OpenScene,
    NewScene,
    OpenPrefab,
    OpenSheet,
    ClosePrefab,
    CloseSheet,
    CloseEditor,
    Play,
    Simulate,
    Export
};
enum class GuardChoice
{
    SaveAndContinue,
    UseSaved,
    Discard,
    Cancel
};
enum class SaveOutcome
{
    Saved,
    Failed,
    Cancelled,
    Unavailable
};
struct DocumentInfo
{
    EditorDocument Kind;
    uint64_t Identity = 0;
    std::string Name;
    bool Dirty = false;
    std::string SaveUnavailable;
};
struct DocumentSaveResult
{
    DocumentInfo Document;
    SaveOutcome Outcome;
    std::string Message;
};
// Exactly the three existing authored document types. No ImGui, serializers,
// assets or session ownership. Destructive callbacks stage replacement themselves:
// failed/cancelled Open must never discard the previous drafts beforehand.
class EditorDocuments
{
  public:
    using Action = std::function<bool()>;
    using Save = std::function<DocumentSaveResult(const DocumentInfo &)>;
    bool Request(OperationIntent intent, const std::vector<DocumentInfo> &documents, Action action);
    bool Resolve(GuardChoice choice, const std::vector<DocumentInfo> &current, const Save &save);
    bool Pending() const
    {
        return bool(m_Action);
    }
    OperationIntent Intent() const
    {
        return m_Intent;
    }
    const std::vector<DocumentInfo> &Affected() const
    {
        return m_Affected;
    }
    const std::vector<DocumentSaveResult> &Results() const
    {
        return m_Results;
    }
    const std::string &Error() const
    {
        return m_Error;
    }
    static bool Affects(OperationIntent intent, EditorDocument kind);
    static bool AllowsSaved(OperationIntent intent);
    static bool AllowsDiscard(OperationIntent intent);
    static const char *Description(OperationIntent intent);

  private:
    OperationIntent m_Intent = OperationIntent::SaveAll;
    std::vector<DocumentInfo> m_Affected;
    std::vector<DocumentSaveResult> m_Results;
    std::string m_Error;
    Action m_Action;
};
} // namespace Hazel
