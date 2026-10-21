#pragma once
#include "ProjectAssets.h"
#include "Hazel/Core/FileDocument.h"
namespace Hazel {
// Uses the same accepted-file/conflict protocol as sprite-sheet authoring. Draft
// metadata only; text components use the independent scene/prefab command history.
class FontFamilyDocument {
public:
    explicit FontFamilyDocument(Ref<ProjectAssets> assets):m_Assets(std::move(assets)){}
    void Open(const std::filesystem::path& reference);
    void Create(const std::filesystem::path& reference,const FontFamilyDefinition& definition);
    void Save(const std::filesystem::path& recoveryRoot={});
    void Discard(){Open(m_Reference);}
    FontFamilyDefinition& Draft(){return m_Draft;}
    const FontFamilyDefinition& Draft() const{return m_Draft;}
    void Changed(){m_Dirty=true;}
    bool Dirty() const{return m_Dirty;}
    const std::filesystem::path& Reference() const{return m_Reference;}
    const FileDocument& File() const{return m_File;}
private:
    Ref<ProjectAssets> m_Assets;
    FontFamilyDefinition m_Draft;
    std::filesystem::path m_Reference;
    FileDocument m_File;
    bool m_Dirty=false;
};
}
