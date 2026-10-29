#include "hzpch.h"
#include "FontFamilyDocument.h"
#include "Hazel/Project/Project.h"
namespace Hazel {
void FontFamilyDocument::Open(const std::filesystem::path& reference) {
    FontSelection{reference}.Validate();if(reference.empty())throw std::runtime_error("Engine family is read-only; create a project family");
    auto path=Project::ResolveOwnedAsset(m_Assets->Root(),reference);
    auto candidate=ReadFontFamily(path);FileDocument file;file.Open(path);
    if(WriteFontFamilyText(ReadFontFamilyText(file.Original()))!=WriteFontFamilyText(candidate))throw std::runtime_error("Family changed while opening; retry");
    file.Check();m_Reference=reference;m_Draft=std::move(candidate);m_File=std::move(file);m_Dirty=false;
}
void FontFamilyDocument::Create(const std::filesystem::path& reference,const FontFamilyDefinition& definition) {
    FontSelection{reference}.Validate();if(reference.empty())throw std::runtime_error("Select a project .hfont destination");
    auto text=WriteFontFamilyText(definition);(void)InspectFontFamily(m_Assets->Root(),definition);
    auto path=Project::ResolveOwnedAsset(m_Assets->Root(),reference);std::filesystem::create_directories(path.parent_path());
    FileSystem::WriteFileAtomically(path,[&](auto& out){out<<text;},WriteMode::CreateNew);
    FileDocument file;file.Open(path);if(file.Original()!=text)throw std::runtime_error("Created family changed externally; open it explicitly");
    m_File=std::move(file);m_Reference=reference;m_Draft=definition;m_Dirty=false;m_Assets->Reload(reference);
}
void FontFamilyDocument::Save(const std::filesystem::path& recoveryRoot) {
    m_File.Check();auto text=WriteFontFamilyText(m_Draft);
    (void)InspectFontFamily(m_Assets->Root(),m_Draft); // requirements are export gates, not a ban on saving an editable declaration
    m_File.Save(text,recoveryRoot);m_Dirty=false;m_Assets->Reload(m_Reference);
}
}
