#include "hzpch.h"
#include "Hazel/Project/Project.h"
#include "ProjectAssets.h"
namespace Hazel
{
void ProjectAssets::ReloadStyle(const std::string &key, StyleEntry &entry)
{
    try
    {
        auto definition =
            ReadUIStyle(Project::ResolveOwnedAsset(m_Root, std::filesystem::u8path(key)));
        (void)AuditUIStyle(m_Root, definition); // CPU dependency validation, only at load/reload
        size_t bytes = WriteUIStyleText(definition).size() * 2 + sizeof(UIStyleDefinition) +
                       definition.Variants.size() * sizeof(UIStyleVariant);
        if (m_StyleBytes - entry.Bytes + bytes > MaxStyleBytes)
            throw std::runtime_error("UI style retained metadata exceeds 4 MiB; release inactive "
                                     "users and Refresh assets");
        if (entry.Accepted && WriteUIStyleText(*entry.Accepted) == WriteUIStyleText(definition))
        {
            entry.Error.clear();
            return;
        }
        auto budget = m_StyleBudget;
        if (budget->Bytes.load() + bytes > MaxStyleLiveBytes ||
            budget->Versions.load() >= MaxStyleVersions)
            throw std::runtime_error("UI style live metadata admission limit (8 MiB estimate / 128 "
                                     "versions); release pinned old users and Reload");
        auto allocation = CreateScope<UIStyleDefinition>(std::move(definition));
        budget->Bytes.fetch_add(bytes);
        budget->Versions.fetch_add(1);
        Ref<const UIStyleDefinition> accepted(allocation.release(),
                                              [budget, bytes](const UIStyleDefinition *p) {
                                                  delete p;
                                                  budget->Bytes.fetch_sub(bytes);
                                                  budget->Versions.fetch_sub(1);
                                              });
        m_StyleBytes = m_StyleBytes - entry.Bytes + bytes;
        entry.Accepted = std::move(accepted);
        entry.Bytes = bytes;
        entry.Error.clear();
    }
    catch (const std::exception &error)
    {
        entry.Error = std::string(error.what()).substr(0, 1024);
        HZ_CORE_WARN("UI style '{}': {}. Accepted presentation retained; Reload/Refresh to retry.",
                     key, entry.Error);
    }
}
ProjectAssets::StyleResolution ProjectAssets::Style(const std::filesystem::path &reference)
{
    UIStyleSelection{reference}.Validate();
    if (reference.empty())
    {
        static const Ref<const UIStyleDefinition> engine(&DefaultUIStyle(), [](const UIStyleDefinition*) {});
        return {engine, {}};
    }
    auto key = reference.generic_u8string();
    auto it = m_Styles.find(key);
    if (it == m_Styles.end())
    {
        if (m_Styles.size() >= MaxStyleEntries)
            return {
                {},
                "UI style metadata entry limit (64); release inactive users and Refresh assets"};
        it = m_Styles.emplace(key, StyleEntry{}).first;
        ReloadStyle(key, it->second);
    }
    return {it->second.Accepted, it->second.Error};
}
} // namespace Hazel
