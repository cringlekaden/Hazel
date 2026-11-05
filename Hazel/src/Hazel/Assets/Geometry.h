#pragma once
#include "AssetData.h"
#include <glm/glm.hpp>
#include <vector>
namespace Hazel
{
// CPU-only mesh-local, right handed, +Y up, metres. UV origin is top left.
struct MeshVertex
{
    glm::vec3 Position{}, Normal{0, 0, 1};
    glm::vec4 Tangent{1, 0, 0, 1};
    glm::vec2 UV{};
    glm::vec4 Color{1}; // linear RGB, straight alpha
};
struct MeshBounds
{
    glm::vec3 Min{}, Max{};
    bool operator==(const MeshBounds &b) const
    {
        return Min == b.Min && Max == b.Max;
    }
};
struct MeshPrimitive
{
    uint64_t Slot = 0;
    uint32_t FirstIndex = 0, IndexCount = 0, Attributes = 0; // bit 0 UV0, bit 1 COLOR0
    MeshBounds Bounds;
};
struct GeometryData
{
    uint64_t Mesh = 0;
    std::vector<MeshVertex> Vertices;
    std::vector<uint32_t> Indices;
    std::vector<MeshPrimitive> Primitives;
    MeshBounds Bounds;
    void UpdateBounds();
    void Validate() const;
};
constexpr size_t GeometryByteLimit = 80 * 1024 * 1024;
// Version 1, checked little-endian scalar fields; never dumps C++/GLM structs.
std::string EncodeGeometry(const GeometryData &data);
GeometryData DecodeGeometry(std::string_view bytes);
GeometryData ReadGeometry(const std::filesystem::path &file);
} // namespace Hazel
