#include "hzpch.h"
#include "MaterialDocument.h"
#include "Hazel/Project/Project.h"
#include "Hazel/Core/FileSystem.h"
namespace Hazel
{
void MaterialDocument::Validate(const MaterialDefinition &value) const
{
    if (!m_Assets)
        throw std::runtime_error("Open a project before authoring materials");
    value.Validate();
    AuditMaterial(m_Assets->Root(), value);
}
void MaterialDocument::Open(const std::filesystem::path &reference)
{
    AssetPath(reference, ".hmat");
    FileDocument file;
    file.Open(Project::ResolveOwnedAsset(m_Assets->Root(), reference));
    auto value = ReadMaterialText(file.Original());
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
void MaterialDocument::Create(const std::filesystem::path &reference,
                              const MaterialDefinition &value)
{
    AssetPath(reference, ".hmat");
    Validate(value);
    auto path = Project::ResolveOwnedAsset(m_Assets->Root(), reference);
    auto bytes = WriteMaterialText(value);
    std::filesystem::create_directories(path.parent_path());
    FileSystem::WriteFileAtomically(path, [&](auto &out) { out << bytes; }, WriteMode::CreateNew);
    Open(reference);
    m_Assets->Reload(reference);
}
bool MaterialDocument::Dirty() const
{
    return !(m_Draft == m_Saved);
}
void MaterialDocument::Save()
{
    if (Active())
        Finish();
    Validate(m_Draft);
    auto saved = m_Draft;
    auto bytes = WriteMaterialText(m_Draft);
    m_File.Save(bytes);
    m_Saved = std::move(saved);
    m_Assets->Reload(m_Reference);
}
void MaterialDocument::Begin()
{
    if (!m_Before)
        m_Before = m_Draft;
}
void MaterialDocument::Preview(MaterialDefinition value)
{
    Validate(value);
    if (!m_Before)
        Begin();
    m_Draft = std::move(value);
}
void MaterialDocument::Finish(bool cancel)
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
    auto bytes = WriteMaterialText(*m_Before).size() * 2 + WriteMaterialText(m_Draft).size() * 2 +
                 sizeof(Entry);
    if (bytes > MaxBytes)
        throw std::runtime_error(
            "Material edit exceeds 4 MiB history budget; cancel or shorten it");
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
void MaterialDocument::Edit(MaterialDefinition value)
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
void MaterialDocument::Undo()
{
    if (!CanUndo())
        throw std::runtime_error("No material undo available");
    auto value = m_History[m_Cursor - 1].Before;
    Validate(value);
    m_Draft = std::move(value);
    --m_Cursor;
}
void MaterialDocument::Redo()
{
    if (!CanRedo())
        throw std::runtime_error("No material redo available");
    auto value = m_History[m_Cursor].After;
    Validate(value);
    m_Draft = std::move(value);
    ++m_Cursor;
}
} // namespace Hazel
