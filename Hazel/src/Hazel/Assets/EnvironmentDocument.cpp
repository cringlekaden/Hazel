#include "EnvironmentDocument.h"
#include "Hazel/Core/FileSystem.h"
#include "Hazel/Project/Project.h"
#include "hzpch.h"
namespace Hazel
{
void EnvironmentDocument::Validate(const EnvironmentDefinition &value) const
{
    if (!m_Assets)
        throw std::runtime_error("Open a project before authoring environments");
    value.Validate();
    Project::ResolveOwnedAsset(
        m_Assets->Root(), value.Image); // source may be unavailable for accepted runtime viewing
}
void EnvironmentDocument::Open(const std::filesystem::path &reference)
{
    AssetPath(reference, ".henv");
    FileDocument file;
    file.Open(Project::ResolveOwnedAsset(m_Assets->Root(), reference));
    auto value = ReadEnvironmentText(file.Original());
    Validate(value);
    auto saved = value;
    auto path = reference;
    m_File = std::move(file);
    m_Draft = std::move(value);
    m_Saved = std::move(saved);
    m_Reference = std::move(path);
    m_History.clear();
    m_Before.reset();
    m_Cursor = m_Bytes = 0;
}
void EnvironmentDocument::Create(const std::filesystem::path &reference,
                                 const EnvironmentDefinition &value)
{
    AssetPath(reference, ".henv");
    Validate(value);
    auto path = Project::ResolveOwnedAsset(m_Assets->Root(), reference);
    auto bytes = WriteEnvironmentText(value);
    std::filesystem::create_directories(path.parent_path());
    FileSystem::WriteFileAtomically(path, [&](auto &out) { out << bytes; }, WriteMode::CreateNew);
    Open(reference);
    m_Assets->Reload(reference);
}
bool EnvironmentDocument::Dirty() const
{
    return !(m_Draft == m_Saved);
}
void EnvironmentDocument::Save()
{
    if (Active())
        Finish();
    Validate(m_Draft);
    auto saved = m_Draft;
    auto bytes = WriteEnvironmentText(m_Draft);
    m_File.Save(bytes);
    m_Saved = std::move(saved);
    m_Assets->Reload(m_Reference);
}
void EnvironmentDocument::Begin()
{
    if (!m_Before)
        m_Before = m_Draft;
}
void EnvironmentDocument::Preview(EnvironmentDefinition value)
{
    Validate(value);
    if (!m_Before)
        Begin();
    m_Draft = std::move(value);
}
void EnvironmentDocument::Finish(bool cancel)
{
    if (!m_Before)
        return;
    if (cancel)
    {
        m_Draft = std::move(*m_Before);
        m_Before.reset();
        return;
    }
    if (*m_Before == m_Draft)
    {
        m_Before.reset();
        return;
    }
    auto bytes = WriteEnvironmentText(*m_Before).size() * 2 +
                 WriteEnvironmentText(m_Draft).size() * 2 + sizeof(Entry);
    if (bytes > MaxBytes)
        throw std::runtime_error(
            "Environment edit exceeds 4 MiB history budget; cancel or shorten it");
    Entry entry{*m_Before, m_Draft, bytes};
    auto history = m_History;
    history.resize(m_Cursor);
    history.push_back(std::move(entry));
    size_t total = 0;
    for (auto &item : history)
        total += item.Bytes;
    while (history.size() > MaxEntries || total > MaxBytes)
    {
        total -= history.front().Bytes;
        history.erase(history.begin());
    }
    m_History = std::move(history);
    m_Cursor = m_History.size();
    m_Bytes = total;
    m_Before.reset();
}
void EnvironmentDocument::Edit(EnvironmentDefinition value)
{
    if (Active())
        Finish();
    Begin();
    try
    {
        Preview(std::move(value));
        Finish();
    }
    catch (...)
    {
        Finish(true);
        throw;
    }
}
void EnvironmentDocument::Undo()
{
    if (!CanUndo())
        throw std::runtime_error("No environment undo available");
    auto value = m_History[m_Cursor - 1].Before;
    Validate(value);
    m_Draft = std::move(value);
    --m_Cursor;
}
void EnvironmentDocument::Redo()
{
    if (!CanRedo())
        throw std::runtime_error("No environment redo available");
    auto value = m_History[m_Cursor].After;
    Validate(value);
    m_Draft = std::move(value);
    ++m_Cursor;
}
} // namespace Hazel
