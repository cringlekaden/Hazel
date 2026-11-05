#pragma once
#include "Hazel/Core/FileDocument.h"
#include "ProjectAssets.h"
#include <optional>
namespace Hazel
{
// Independent authored file owner. History contains bounded CPU values only;
// accepted ProjectAssets versions are refreshed only by explicit Save/Reload.
class MaterialDocument
{
  public:
    explicit MaterialDocument(Ref<ProjectAssets> assets) : m_Assets(std::move(assets))
    {
    }
    void Open(const std::filesystem::path &reference);
    void Create(const std::filesystem::path &reference, const MaterialDefinition &value);
    void Save();
    bool Dirty() const;
    const MaterialDefinition &Draft() const
    {
        return m_Draft;
    }
    const std::filesystem::path &Reference() const
    {
        return m_Reference;
    }
    const FileDocument &File() const
    {
        return m_File;
    }
    void Begin();
    void Preview(MaterialDefinition value);
    void Finish(bool cancel = false);
    void Edit(MaterialDefinition value);
    void Undo();
    void Redo();
    bool CanUndo() const
    {
        return !m_Before && m_Cursor > 0;
    }
    bool CanRedo() const
    {
        return !m_Before && m_Cursor < m_History.size();
    }
    bool Active() const
    {
        return bool(m_Before);
    }
    static constexpr size_t MaxEntries = 100, MaxBytes = 4 * 1024 * 1024;

  private:
    Ref<ProjectAssets> m_Assets;
    std::filesystem::path m_Reference;
    FileDocument m_File;
    MaterialDefinition m_Draft, m_Saved;
    std::optional<MaterialDefinition> m_Before;
    struct Entry
    {
        MaterialDefinition Before, After;
        size_t Bytes = 0;
    };
    std::vector<Entry> m_History;
    size_t m_Cursor = 0, m_Bytes = 0;
    void Validate(const MaterialDefinition &value) const;
};
} // namespace Hazel
