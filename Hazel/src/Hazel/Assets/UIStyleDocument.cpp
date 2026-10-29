#include "hzpch.h"
#include "UIStyleDocument.h"
#include "Hazel/Project/Project.h"
namespace Hazel
{
void UIStyleDocument::Open(const std::filesystem::path &reference)
{
    UIStyleSelection{reference}.Validate();
    if (reference.empty())
        throw std::runtime_error("Engine style is read-only; create a project style");
    auto path = Project::ResolveOwnedAsset(m_Assets->Root(), reference);
    auto candidate = ReadUIStyle(path);
    FileDocument file;
    file.Open(path);
    if (WriteUIStyleText(ReadUIStyleText(file.Original())) != WriteUIStyleText(candidate))
        throw std::runtime_error("Style changed while opening; retry");
    file.Check();
    m_Reference = reference;
    m_Draft = std::move(candidate);
    m_File = std::move(file);
    m_Dirty = false;
}
void UIStyleDocument::Create(const std::filesystem::path &reference,
                             const UIStyleDefinition &definition)
{
    UIStyleSelection{reference}.Validate();
    if (reference.empty())
        throw std::runtime_error("Select a project .hstyle destination");
    auto text = WriteUIStyleText(definition);
    (void)AuditUIStyle(m_Assets->Root(), definition);
    auto path = Project::ResolveOwnedAsset(m_Assets->Root(), reference);
    std::filesystem::create_directories(path.parent_path());
    FileSystem::WriteFileAtomically(path, [&](auto &out) { out << text; }, WriteMode::CreateNew);
    FileDocument file;
    file.Open(path);
    if (file.Original() != text)
        throw std::runtime_error("Created style changed externally; open it explicitly");
    m_File = std::move(file);
    m_Reference = reference;
    m_Draft = definition;
    m_Dirty = false;
    m_Assets->Reload(reference);
}
void UIStyleDocument::Save(const std::filesystem::path &recoveryRoot)
{
    m_File.Check();
    auto text = WriteUIStyleText(m_Draft);
    (void)AuditUIStyle(m_Assets->Root(), m_Draft);
    m_File.Save(text, recoveryRoot);
    m_Dirty = false;
    m_Assets->Reload(m_Reference);
}
} // namespace Hazel
