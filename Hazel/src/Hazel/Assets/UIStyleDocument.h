#pragma once
#include "Hazel/Core/FileDocument.h"
#include "ProjectAssets.h"
namespace Hazel
{
class UIStyleDocument
{
  public:
    explicit UIStyleDocument(Ref<ProjectAssets> assets) : m_Assets(std::move(assets))
    {
    }
    void Open(const std::filesystem::path &reference);
    void Create(const std::filesystem::path &reference, const UIStyleDefinition &definition);
    void Save(const std::filesystem::path &recoveryRoot = {});
    UIStyleDefinition &Draft()
    {
        return m_Draft;
    }
    const UIStyleDefinition &Draft() const
    {
        return m_Draft;
    }
    void Changed()
    {
        m_Dirty = true;
    }
    bool Dirty() const
    {
        return m_Dirty;
    }
    const std::filesystem::path &Reference() const
    {
        return m_Reference;
    }
    const FileDocument &File() const
    {
        return m_File;
    }

  private:
    Ref<ProjectAssets> m_Assets;
    UIStyleDefinition m_Draft;
    std::filesystem::path m_Reference;
    FileDocument m_File;
    bool m_Dirty = false;
};
} // namespace Hazel
