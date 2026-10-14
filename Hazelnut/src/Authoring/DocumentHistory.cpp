#include "DocumentHistory.h"
#include "Hazel/Scene/Scene.h"
#include <stdexcept>
namespace Hazel
{
void DocumentHistory::Bind(uint64_t document)
{
    if (m_Document == document)
        return;
    if (Active())
        throw std::logic_error("Cancel the transaction before replacing the document history");
    Clear();
    m_Document = document;
    m_SavedAuthored.clear();
    m_Save = m_Base;
}
void DocumentHistory::Check(Scene &scene) const
{
    scene.CheckThread();
    if (!m_Document || scene.GetIdentity() != m_Document)
        throw std::runtime_error("History belongs to a different document; reopen its Inspector");
    if (scene.IsRunning() || scene.IsSimulating() || scene.IsStopping())
        throw std::runtime_error("Stop Play / Simulate before applying authoring history");
}
uint64_t DocumentHistory::Revision() const
{
    return m_Cursor ? m_Entries[m_Cursor - 1].After : m_Base;
}
bool DocumentHistory::Begin(Scene &scene, Scope<DocumentCommand> command)
{
    try
    {
        Check(scene);
        if (Active())
            throw std::runtime_error("Finish or cancel the current document transaction first");
        if (!command || command->Bytes() > m_MaxBytes)
            throw std::runtime_error("Edit exceeds the document history memory limit");
        command->Check(scene);
        command->SetMemoryLimit(m_MaxBytes);
        m_Transaction = std::move(command);
        m_Diagnostic.clear();
        return true;
    }
    catch (const std::exception &error)
    {
        m_Diagnostic = error.what();
        return false;
    }
}
bool DocumentHistory::Commit(Scene &scene)
{
    try
    {
        Check(scene);
        if (!Active())
            return true;
        m_Transaction->Check(scene);
        if (!m_Transaction->Changed())
        {
            m_Transaction.reset();
            return true;
        }
        const auto bytes = m_Transaction->Bytes();
        if (!m_MaxOperations || bytes > m_MaxBytes)
            throw std::runtime_error("Edit exceeds history limits; cancel or reduce its size");
        // Prepare feedback before changing the branch/cursor: even allocation
        // failure while formatting an eviction notice leaves the edit cancellable.
        auto projectedBytes = m_Bytes + bytes;
        for (size_t i = m_Cursor; i < m_Entries.size(); ++i)
            projectedBytes -= m_Entries[i].Bytes;
        size_t projectedCount = m_Cursor + 1, evicted = 0;
        while (projectedCount > m_MaxOperations || projectedBytes > m_MaxBytes)
        {
            projectedBytes -= m_Entries[evicted++].Bytes;
            --projectedCount;
        }
        std::string diagnostic;
        if (evicted)
            diagnostic = "History limit: evicted " + std::to_string(evicted) +
                         " oldest edit(s). Saved authored content remains the dirty-state baseline.";
        // Allocate before modifying the redo branch or moving the transaction.
        const auto before = Revision();
        m_Entries.push_back({before, m_Next + 1, nullptr, bytes});
        auto command = std::move(m_Transaction);
        // The just-allocated slot survives discarding every previous redo slot.
        while (m_Cursor + 1 < m_Entries.size())
        {
            m_Bytes -= m_Entries[m_Cursor].Bytes;
            m_Entries.erase(m_Entries.begin() + m_Cursor);
        }
        m_Entries.back().Command = std::move(command);
        ++m_Next;
        ++m_Cursor;
        m_Bytes += bytes;
        while (m_Entries.size() > m_MaxOperations || m_Bytes > m_MaxBytes)
        {
            m_Base = m_Entries.front().After;
            m_Bytes -= m_Entries.front().Bytes;
            m_Entries.pop_front();
            --m_Cursor;
        }
        m_Diagnostic = std::move(diagnostic);
        return true;
    }
    catch (const std::exception &error)
    {
        m_Diagnostic = error.what();
        return false;
    }
}
bool DocumentHistory::Cancel(Scene &scene)
{
    if (!Active())
        return true;
    try
    {
        Check(scene);
        m_Transaction->Cancel(scene);
        m_Transaction.reset();
        return true;
    }
    catch (const std::exception &error)
    {
        // Never carry an interrupted transaction into another entity/document.
        m_Diagnostic = std::string("Edit cancelled; ") + error.what();
        m_Transaction.reset();
        return false;
    }
}
bool DocumentHistory::Undo(Scene &scene)
{
    try
    {
        Check(scene);
        if (!CanUndo())
            throw std::runtime_error("No available Undo; finish the current edit first");
        m_Entries[m_Cursor - 1].Command->Apply(scene, true);
        --m_Cursor;
        m_Diagnostic.clear();
        return true;
    }
    catch (const std::exception &error)
    {
        m_Diagnostic = error.what();
        return false;
    }
}
bool DocumentHistory::Redo(Scene &scene)
{
    try
    {
        Check(scene);
        if (!CanRedo())
            throw std::runtime_error("No available Redo; finish the current edit first");
        m_Entries[m_Cursor].Command->Apply(scene, false);
        ++m_Cursor;
        m_Diagnostic.clear();
        return true;
    }
    catch (const std::exception &error)
    {
        m_Diagnostic = error.what();
        return false;
    }
}
void DocumentHistory::Clear()
{
    if (Active())
        throw std::logic_error("Cancel the transaction before clearing history");
    m_Entries.clear();
    m_Cursor = m_Bytes = 0;
    m_Base = ++m_Next;
    m_Diagnostic.clear();
}
void DocumentHistory::MarkSaved(std::string authored)
{
    if (Active())
        throw std::logic_error("Finish the edit before establishing the save point");
    m_SavedAuthored = std::move(authored);
    m_Save = Revision();
}
bool DocumentHistory::SavePointRetained() const
{
    if (m_Save == m_Base)
        return true;
    for (const auto &entry : m_Entries)
        if (entry.After == m_Save)
            return true;
    return false;
}
const std::string &DocumentHistory::UndoName() const
{
    static const std::string empty;
    return CanUndo() ? m_Entries[m_Cursor - 1].Command->Name() : empty;
}
const std::string &DocumentHistory::RedoName() const
{
    static const std::string empty;
    return CanRedo() ? m_Entries[m_Cursor].Command->Name() : empty;
}
std::string DocumentHistory::TakeDiagnostic()
{
    auto result = std::move(m_Diagnostic);
    m_Diagnostic.clear();
    return result;
}
} // namespace Hazel
