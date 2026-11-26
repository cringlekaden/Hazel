#include "hzpch.h"
#include "Hazel/Renderer/Resources/EnvironmentResources.h"
#include "ProjectAssets.h"
namespace Hazel
{
namespace
{
size_t Bytes(const PreparedEnvironment &e)
{
    return e.Background.Bytes() + e.Irradiance.Bytes() + e.Specular.Bytes() + e.BRDF.Bytes();
}
} // namespace
void ProjectAssets::ReloadEnvironment(const std::string &key, EnvironmentEntry &entry)
{
    try
    {
        auto candidate = ReadPreparedEnvironment(m_Root, std::filesystem::u8path(key));
        if (entry.Accepted && entry.Accepted->Identity == candidate.Identity &&
            entry.Accepted->Generation == candidate.Generation)
        {
            if (entry.Accepted->Artifacts != candidate.Artifacts)
                throw std::runtime_error("Immutable lighting generation changed; restore accepted "
                                         "artifacts or Prepare a fresh generation");
            entry.Error.clear();
            return;
        }
        const auto bytes = Bytes(candidate);
        auto budget = m_AssetBudget;
        if (bytes > 64 * 1024 * 1024 || budget->Bytes.load() + bytes > 256 * 1024 * 1024 ||
            budget->Versions.load() >= 128)
            throw std::runtime_error("Environment CPU cache exceeds 256 MiB / 128 versions; "
                                     "release inactive views and Refresh");
        auto value = CreateScope<PreparedEnvironment>(std::move(candidate));
        budget->Bytes.fetch_add(bytes);
        budget->Versions.fetch_add(1);
        entry.Accepted = Ref<const PreparedEnvironment>(value.release(), [budget, bytes](auto *p) {
            delete p;
            budget->Bytes.fetch_sub(bytes);
            budget->Versions.fetch_sub(1);
        });
        entry.Error.clear();
    }
    catch (const std::exception &e)
    {
        entry.Error = std::string(e.what()).substr(0, 1024);
    }
}
ProjectAssets::EnvironmentResolution ProjectAssets::Environment(const std::filesystem::path &r)
{
    AssetPath(r, ".henv");
    auto key = r.generic_u8string();
    auto it = m_Environments.find(key);
    if (it == m_Environments.end())
    {
        if (m_Environments.size() >= 8)
            return {{}, "Environment entry limit (8); release inactive views and Refresh"};
        it = m_Environments.emplace(key, EnvironmentEntry{}).first;
        ReloadEnvironment(key, it->second);
    }
    return {it->second.Accepted, it->second.Error};
}
Ref<EnvironmentResources> ProjectAssets::RealizeEnvironment(const Ref<const PreparedEnvironment> &e)
{
    if (!e)
        throw std::runtime_error("No accepted environment to realize");
    auto key = e->Identity + AssetKey(e->Generation);
    auto it = m_EnvironmentGPU.find(key);
    if (it != m_EnvironmentGPU.end())
        return it->second;
    while (m_EnvironmentGPU.size() >= 4)
    {
        auto victim = std::find_if(m_EnvironmentGPU.begin(), m_EnvironmentGPU.end(),
                                   [](auto &p) { return p.second.use_count() == 1; });
        if (victim == m_EnvironmentGPU.end())
            throw std::runtime_error("Environment GPU cache exceeds four pinned generations");
        m_EnvironmentGPU.erase(victim);
    }
    auto resource = CreateRef<EnvironmentResources>(*e);
    m_EnvironmentGPU.emplace(key, resource);
    return resource;
}
Ref<ReflectionProbeResources> ProjectAssets::RealizeReflectionProbe(const Ref<const PreparedEnvironment> &environment)
{
    if (!environment)
        throw std::runtime_error("Reflection probe has no accepted environment");
    const auto key = environment->Identity + AssetKey(environment->Generation);
    if (const auto it = m_ReflectionProbeGPU.find(key); it != m_ReflectionProbeGPU.end())
        return it->second;
    // Each entry has a bounded face size. Views and queued Metal frames pin
    // entries until retirement, so eviction includes in-flight generations.
    constexpr size_t maxGenerations = 8;
    if (m_ReflectionProbeGPU.size() == maxGenerations)
    {
        const auto victim = std::find_if(m_ReflectionProbeGPU.begin(), m_ReflectionProbeGPU.end(),
                                         [](const auto &entry) { return entry.second.use_count() == 1; });
        if (victim == m_ReflectionProbeGPU.end())
            throw std::runtime_error("Reflection probe GPU cache exceeds eight pinned generations");
        m_ReflectionProbeGPU.erase(victim);
    }
    auto resource = CreateRef<ReflectionProbeResources>(*environment);
    m_ReflectionProbeGPU.emplace(key, resource);
    return resource;
}
void ProjectAssets::RefreshEnvironments()
{
    // Retain accepted versions through failed/stale refresh, including unpinned
    // entries. Explicit refresh is the only filesystem freshness boundary.
    for (auto &e : m_Environments)
        ReloadEnvironment(e.first, e.second);
}
} // namespace Hazel
