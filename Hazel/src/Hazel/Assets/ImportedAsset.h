#pragma once
#include "Geometry.h"
#include "Material.h"
#include <map>
namespace YAML
{
class Node;
class Emitter;
} // namespace YAML
namespace Hazel
{
struct MeshSlot
{
    uint64_t ID = 0;
    std::string Name;
    std::filesystem::path Material;
    bool operator==(const MeshSlot &b) const
    {
        return ID == b.ID && Name == b.Name && Material == b.Material;
    }
};
struct MeshDefinition
{
    uint64_t ID = 0;
    std::string Name;
    std::filesystem::path Import;
    std::vector<MeshSlot> Slots;
    void Validate() const;
};
// Source keys are scoped provenance only. IDs survive accepted reimports.
struct ImportSlot
{
    uint64_t ID = 0;
    std::string Source;
    std::filesystem::path Material;
    std::string SourceKey;
};
struct ImportMesh
{
    uint64_t ID = 0;
    std::string Source;
    std::filesystem::path Output, Geometry;
    std::string GeometryHash;
    std::vector<ImportSlot> Slots;
    std::string SourceKey;
};
struct ImportMaterial
{
    uint64_t ID = 0;
    std::string Source;
    std::filesystem::path Output;
    std::string AcceptedHash;
    std::string SourceKey;
};
struct ImportNode
{
    uint64_t ID = 0;
    std::string Source, SourceKey;
};
struct ImportManifest
{
    uint64_t ID = 0, Generation = 0, PrefabRoot = 0;
    std::filesystem::path Source, Prefab;
    std::string SourceHash, PrefabSource, PrefabAcceptedHash, Producer;
    std::string SourceNotice; // Original attribution travels without source/parser access.
    int Scene = -1;
    std::vector<ImportMesh> Meshes;
    std::vector<ImportMaterial> Materials;
    std::vector<ImportNode> Nodes;
    void Validate() const;
};
MeshDefinition ReadMeshText(const std::string &bytes);
MeshDefinition ReadMesh(const std::filesystem::path &file);
std::string WriteMeshText(const MeshDefinition &data);
ImportManifest ReadImportText(const std::string &bytes);
ImportManifest ReadImport(const std::filesystem::path &file);
std::string WriteImportText(const ImportManifest &data);
struct MeshBinding
{
    std::filesystem::path Mesh;
    std::map<uint64_t, std::filesystem::path>
        Materials; // whole-material replacements by stable slot ID
    void Validate() const;
    bool operator==(const MeshBinding &b) const
    {
        return Mesh == b.Mesh && Materials == b.Materials;
    }
};
MeshBinding ReadMeshBinding(const YAML::Node &node);
void WriteMeshBinding(YAML::Emitter &out, const MeshBinding &binding);
std::vector<std::filesystem::path> AuditMeshBinding(const std::filesystem::path &root,
                                                    const MeshBinding &binding);
struct MeshAsset
{
    MeshDefinition Definition;
    GeometryData Geometry;
    uint64_t Generation = 0;
};
MeshAsset ReadMeshAsset(const std::filesystem::path &root, const std::filesystem::path &reference);
std::vector<std::filesystem::path> AuditImport(const std::filesystem::path &root,
                                               const std::filesystem::path &reference);
std::vector<std::filesystem::path> AuditMesh(const std::filesystem::path &root,
                                             const std::filesystem::path &reference);
void ValidateMeshAssignment(const std::filesystem::path &root, const MeshAsset &mesh,
                            const std::map<uint64_t, std::filesystem::path> &overrides);
} // namespace Hazel
