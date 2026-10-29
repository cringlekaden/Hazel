#pragma once
#include "Hazel/Core/Base.h"
#include <cstdint>
#include <deque>
#include <limits>
#include <string>
#include <vector>
#include <map>
#include <typeindex>
#include <optional>

namespace Hazel
{
class Scene;
struct HistoryStamp
{
    uint64_t Entity = 0, Lifetime = 0, Structure = 0;
    std::map<std::type_index, uint64_t> Components;
};
struct HistoryRestore
{
    HistoryStamp Before, After;
};
HistoryStamp CaptureHistoryStamp(Scene &, uint64_t);
// Commands own authored values and identity guards only. Context is supplied by
// the document owner at application, never retained in a history entry.
class DocumentCommand
{
  public:
    virtual ~DocumentCommand() = default;
    virtual const std::string &Name() const = 0;
    virtual size_t Bytes() const = 0;
    virtual bool Changed() const = 0;
    virtual void Check(Scene &scene) const = 0;
    virtual void Apply(Scene &scene, bool undo) = 0; // atomic, including validation
    virtual void Cancel(Scene &scene) = 0;
    virtual void Reconcile(const std::vector<HistoryRestore> &) noexcept
    {
    }
    virtual const std::vector<HistoryRestore> &Restorations() const
    {
        static const std::vector<HistoryRestore> empty;
        return empty;
    }
    virtual std::optional<uint64_t> Selection(bool) const
    {
        return {};
    }
    void SetMemoryLimit(size_t bytes)
    {
        m_MemoryLimit = bytes;
    }

  protected:
    size_t m_MemoryLimit = std::numeric_limits<size_t>::max();
};
class DocumentHistory
{
  public:
    static constexpr size_t DefaultOperations = 100, DefaultBytes = 64 * 1024 * 1024;
    explicit DocumentHistory(size_t operations = DefaultOperations, size_t bytes = DefaultBytes)
        : m_MaxOperations(operations), m_MaxBytes(bytes)
    {
    }
    void Bind(uint64_t document); // cancel first; unchanged identity retains history
    bool Begin(Scene &scene, Scope<DocumentCommand> command);
    DocumentCommand *Transaction()
    {
        return m_Transaction.get();
    }
    bool Commit(Scene &scene);
    bool Execute(Scene &scene, Scope<DocumentCommand> command);
    std::optional<uint64_t> Selection() const
    {
        return m_Selection;
    }
    bool Cancel(Scene &scene);
    bool Undo(Scene &scene);
    bool Redo(Scene &scene);
    void Clear();
    void MarkSaved(std::string authored); // only after a successful write/open
    bool Dirty(const std::string &authored) const
    {
        return authored != m_SavedAuthored;
    }
    const std::string &SavedAuthored() const
    {
        return m_SavedAuthored;
    }
    bool Active() const
    {
        return bool(m_Transaction);
    }
    bool CanUndo() const
    {
        return !Active() && m_Cursor > 0;
    }
    bool CanRedo() const
    {
        return !Active() && m_Cursor < m_Entries.size();
    }
    const std::string &UndoName() const;
    const std::string &RedoName() const;
    uint64_t Identity() const
    {
        return m_Document;
    }
    size_t Operations() const
    {
        return m_Entries.size();
    }
    size_t UsedBytes() const
    {
        return m_Bytes;
    }
    bool SavePointRetained() const;
    const std::string &Diagnostic() const
    {
        return m_Diagnostic;
    }
    std::string TakeDiagnostic();

  private:
    struct Entry
    {
        uint64_t Before, After;
        Scope<DocumentCommand> Command;
        size_t Bytes;
    };
    void Reconcile(DocumentCommand &command) noexcept;
    void Check(Scene &scene) const;
    uint64_t Revision() const;
    std::deque<Entry> m_Entries;
    Scope<DocumentCommand> m_Transaction;
    uint64_t m_Document = 0, m_Base = 1, m_Next = 1, m_Save = 1;
    size_t m_Cursor = 0, m_Bytes = 0, m_MaxOperations, m_MaxBytes;
    std::optional<uint64_t> m_Selection;
    std::string m_SavedAuthored, m_Diagnostic;
};
} // namespace Hazel
