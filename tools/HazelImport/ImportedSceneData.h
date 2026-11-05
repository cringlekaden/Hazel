#pragma once
#include "Hazel/Assets/Geometry.h"
#include "Hazel/Assets/Material.h"
#include <functional>
namespace Hazel::Import
{
// Format-independent producer boundary: owned CPU values, no parser handles/GL.
struct ImportedImageData
{
    std::string SourceKey, Name;
    TextureImage Pixels;
};
struct ImportedMaterialData
{
    std::string SourceKey;
    MaterialDefinition Values;
    std::array<int, 5> Images{{-1, -1, -1, -1, -1}};
};
struct ImportedPrimitiveData
{
    std::string SourceKey;
    std::vector<MeshVertex> Vertices;
    std::vector<uint32_t> Indices;
    int Material = -1;
    bool Normals = false, Tangents = false, UV0 = false, Color0 = false;
};
struct ImportedMeshData
{
    std::string SourceKey, Name;
    std::vector<ImportedPrimitiveData> Primitives;
};
struct ImportedNodeData
{
    std::string SourceKey, Name;
    glm::vec3 Translation{}, Rotation{}, Scale{1}; // exact representable signed TRS
    int Mesh = -1;
    std::vector<int> Children;
};
struct ImportedSceneData
{
    std::string Adapter; // actual parser/adapter revision, producer provenance only
    std::vector<ImportedImageData> Images;
    std::vector<ImportedMaterialData> Materials;
    std::vector<ImportedMeshData> Meshes;
    std::vector<ImportedNodeData> Nodes;
    std::vector<int> Roots;
};
using Cancel = std::function<bool()>;
void CheckCancelled(const Cancel &cancel);
// Each adapter owns byte acquisition/format parsing and releases all parser memory
// before returning. The same common validator/cooker accepts other adapters later.
ImportedSceneData ReadGLB(std::string_view bytes, int scene, const Cancel &cancel = {});
void ValidateImported(const ImportedSceneData &data);
GeometryData CookMesh(const ImportedMeshData &mesh,
                      const std::vector<ImportedMaterialData> &materials, uint64_t id,
                      const std::vector<uint64_t> &slots, const Cancel &cancel = {});
std::string CookImage(const ImportedImageData &image); // canonical unchanged RGBA8 -> top-left TGA
} // namespace Hazel::Import
