#include "hzpch.h"
#include "ProjectAssets.h"
#include "Hazel/Project/Project.h"
namespace Hazel
{
namespace
{
template <class T, class Budget>
Ref<const T> Admit(T value, size_t bytes, const Ref<Budget> &budget)
{
    if (bytes > 128 * 1024 * 1024 || budget->Bytes.load() + bytes > 256 * 1024 * 1024 ||
        budget->Versions.load() >= 128)
        throw std::runtime_error("Mesh/material CPU metadata exceeds 256 MiB / 128 live versions; "
                                 "release inactive documents and Refresh");
    auto owned = CreateScope<T>(std::move(value));
    budget->Bytes.fetch_add(bytes);
    budget->Versions.fetch_add(1);
    return Ref<const T>(owned.release(), [budget, bytes](const T *p) {
        delete p;
        budget->Bytes.fetch_sub(bytes);
        budget->Versions.fetch_sub(1);
    });
}
} // namespace
void ProjectAssets::ReloadMesh(const std::string &key, MeshEntry &entry)
{
    try
    {
        auto candidate = ReadMeshAsset(m_Root, std::filesystem::u8path(key));
        ValidateMeshAssignment(m_Root, candidate, {});
        size_t bytes = sizeof(MeshAsset) + candidate.Geometry.Vertices.size() * sizeof(MeshVertex) +
                       candidate.Geometry.Indices.size() * 4 +
                       candidate.Geometry.Primitives.size() * sizeof(MeshPrimitive) +
                       WriteMeshText(candidate.Definition).size() * 2;
        if (m_MeshBytes - entry.Bytes + bytes > 128 * 1024 * 1024)
            throw std::runtime_error(
                "Accepted mesh metadata exceeds 128 MiB; release inactive documents and Refresh");
        if (entry.Accepted && entry.Accepted->Generation == candidate.Generation &&
            WriteMeshText(entry.Accepted->Definition) == WriteMeshText(candidate.Definition))
        {
            entry.Error.clear();
            return;
        }
        auto accepted = Admit(std::move(candidate), bytes, m_AssetBudget);
        m_MeshBytes = m_MeshBytes - entry.Bytes + bytes;
        entry.Accepted = std::move(accepted);
        entry.Bytes = bytes;
        entry.Error.clear();
    }
    catch (const std::exception &e)
    {
        entry.Error = std::string(e.what()).substr(0, 1024);
    }
}
void ProjectAssets::ReloadMaterial(const std::string &key, MaterialEntry &entry)
{
    try
    {
        auto candidate =
            ReadMaterial(Project::ResolveOwnedAsset(m_Root, std::filesystem::u8path(key)));
        AuditMaterial(m_Root, candidate);
        size_t bytes = sizeof(MaterialDefinition) + WriteMaterialText(candidate).size() * 2;
        if (m_MaterialBytes - entry.Bytes + bytes > 4 * 1024 * 1024)
            throw std::runtime_error(
                "Accepted material metadata exceeds 4 MiB; release inactive documents and Refresh");
        if (entry.Accepted && *entry.Accepted == candidate)
        {
            entry.Error.clear();
            return;
        }
        auto accepted = Admit(std::move(candidate), bytes, m_AssetBudget);
        m_MaterialBytes = m_MaterialBytes - entry.Bytes + bytes;
        entry.Accepted = std::move(accepted);
        entry.Bytes = bytes;
        entry.Error.clear();
    }
    catch (const std::exception &e)
    {
        entry.Error = std::string(e.what()).substr(0, 1024);
    }
}
ProjectAssets::MeshResolution ProjectAssets::Mesh(const std::filesystem::path &reference)
{
    AssetPath(reference, ".hmesh");
    auto key = reference.generic_u8string();
    auto it = m_Meshes.find(key);
    if (it == m_Meshes.end())
    {
        if (m_Meshes.size() >= 32)
            return {{}, "Mesh cache entry limit (32); release inactive documents and Refresh"};
        it = m_Meshes.emplace(key, MeshEntry{}).first;
        ReloadMesh(key, it->second);
    }
    return {it->second.Accepted, it->second.Error};
}
ProjectAssets::MaterialResolution ProjectAssets::Material(const std::filesystem::path &reference)
{
    AssetPath(reference, ".hmat");
    auto key = reference.generic_u8string();
    auto it = m_Materials.find(key);
    if (it == m_Materials.end())
    {
        if (m_Materials.size() >= 128)
            return {{}, "Material cache entry limit (128); release inactive documents and Refresh"};
        it = m_Materials.emplace(key, MaterialEntry{}).first;
        ReloadMaterial(key, it->second);
    }
    return {it->second.Accepted, it->second.Error};
}
void ProjectAssets::RefreshMeshes()
{
    for (auto it = m_Meshes.begin(); it != m_Meshes.end();)
        if (!it->second.Accepted || it->second.Accepted.use_count() == 1)
        {
            m_MeshBytes -= it->second.Bytes;
            it = m_Meshes.erase(it);
        }
        else
        {
            ReloadMesh(it->first, it->second);
            ++it;
        }
    for (auto it = m_Materials.begin(); it != m_Materials.end();)
        if (!it->second.Accepted || it->second.Accepted.use_count() == 1)
        {
            m_MaterialBytes -= it->second.Bytes;
            it = m_Materials.erase(it);
        }
        else
        {
            ReloadMaterial(it->first, it->second);
            ++it;
        }
}
} // namespace Hazel
