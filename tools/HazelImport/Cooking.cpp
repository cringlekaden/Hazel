#include "ImportedSceneData.h"
#include "Allocation.h"
#include "mikktspace.h"
#include "Hazel/Core/OperationProgress.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <numeric>
#include <set>
#include <stdexcept>
namespace Hazel::Import
{
namespace
{
void Require(bool yes, const char *why)
{
    if (!yes)
        throw std::runtime_error(why);
}
template <class V> void Finite(const V &v)
{
    for (int i = 0; i < v.length(); ++i)
        Require(std::isfinite(v[i]) && std::abs(v[i]) <= 1000000,
                "Nonfinite/excessive imported attribute");
}
struct CornerMesh
{
    std::vector<MeshVertex> *Corners;
};
MeshVertex &Corner(const SMikkTSpaceContext *c, int face, int vertex)
{
    return (*static_cast<CornerMesh *>(c->m_pUserData)->Corners)[size_t(face) * 3 + vertex];
}
int Faces(const SMikkTSpaceContext *c)
{
    return int(static_cast<CornerMesh *>(c->m_pUserData)->Corners->size() / 3);
}
int Three(const SMikkTSpaceContext *, int)
{
    return 3;
}
void Position(const SMikkTSpaceContext *c, float *v, int f, int i)
{
    auto p = Corner(c, f, i).Position;
    for (int j = 0; j < 3; ++j)
        v[j] = p[j];
}
void Normal(const SMikkTSpaceContext *c, float *v, int f, int i)
{
    auto p = Corner(c, f, i).Normal;
    for (int j = 0; j < 3; ++j)
        v[j] = p[j];
}
void UV(const SMikkTSpaceContext *c, float *v, int f, int i)
{
    auto p = Corner(c, f, i).UV;
    v[0] = p.x;
    v[1] = p.y;
}
void Tangent(const SMikkTSpaceContext *c, const float *v, float sign, int f, int i)
{
    Corner(c, f, i).Tangent = {v[0], v[1], v[2], sign};
}
glm::vec4 Fallback(glm::vec3 normal)
{
    auto axis = std::abs(normal.z) < .9f ? glm::vec3(0, 0, 1) : glm::vec3(0, 1, 0);
    return {glm::normalize(glm::cross(axis, normal)), 1};
}
// Defined field order, excluding any compiler padding, for deterministic reindexing.
std::array<float, 16> Fields(const MeshVertex &v)
{
    return {{v.Position.x, v.Position.y, v.Position.z, v.Normal.x, v.Normal.y, v.Normal.z,
             v.Tangent.x, v.Tangent.y, v.Tangent.z, v.Tangent.w, v.UV.x, v.UV.y, v.Color.r,
             v.Color.g, v.Color.b, v.Color.a}};
}
} // namespace
void CheckCancelled(const Cancel &cancel)
{
    if (cancel && cancel())
        throw std::runtime_error("Import cancelled; accepted generation retained");
}
void ValidateImported(const ImportedSceneData &d)
{
    AssetName(d.Adapter);
    Require(!d.Meshes.empty() && d.Meshes.size() <= 1024 && d.Materials.size() <= 4096 &&
                d.Images.size() <= 1024 && d.Nodes.size() <= 9999 && !d.Roots.empty(),
            "Imported scene exceeds supported counts or has no geometry/roots");
    size_t owned = 0;
    uint64_t vertexCount = 0, indexCount = 0;
    std::set<std::string> meshKeys, materialKeys, nodeKeys;
    for (auto &image : d.Images)
    {
        AssetName(image.Name);
        Require(image.Pixels.Width && image.Pixels.Height && image.Pixels.Width <= 65535 &&
                    image.Pixels.Height <= 65535 && image.Pixels.Format == ImageFormat::RGBA8 &&
                    image.Pixels.Pixels.size() ==
                        uint64_t(image.Pixels.Width) * image.Pixels.Height * 4,
                "Invalid owned imported RGBA8 image");
        owned += image.Pixels.Pixels.size();
    }
    for (auto &material : d.Materials)
    {
        Require(materialKeys.insert(material.SourceKey).second,
                "Duplicate adapter material source key");
        material.Values.Validate();
        for (auto i : material.Images)
            Require(i >= -1 && (i < 0 || size_t(i) < d.Images.size()),
                    "Imported image reference is invalid");
    }
    for (auto &mesh : d.Meshes)
    {
        AssetName(mesh.Name);
        Require(meshKeys.insert(mesh.SourceKey).second && !mesh.Primitives.empty() &&
                    mesh.Primitives.size() <= 4096,
                "Invalid imported mesh/source key");
        std::set<std::string> keys;
        for (auto &p : mesh.Primitives)
        {
            Require(keys.insert(p.SourceKey).second && p.Material >= -1 &&
                        (p.Material < 0 || size_t(p.Material) < d.Materials.size()),
                    "Invalid imported primitive/source/material");
            Require(!p.Vertices.empty() && p.Vertices.size() <= 1000000 && !p.Indices.empty() &&
                        p.Indices.size() <= 3000000 && p.Indices.size() % 3 == 0,
                    "Invalid/excessive imported triangle counts");
            vertexCount += p.Vertices.size();
            indexCount += p.Indices.size();
            Require(vertexCount <= 1000000 && indexCount <= 3000000,
                    "Import exceeds 1 million source vertices / 3 million indices");
            owned += p.Vertices.size() * sizeof(MeshVertex) + p.Indices.size() * 4;
            for (auto i : p.Indices)
                Require(i < p.Vertices.size(), "Imported triangle index is invalid");
            for (auto &v : p.Vertices)
            {
                Finite(v.Position);
                Finite(v.Normal);
                Finite(v.Tangent);
                Finite(v.UV);
                Finite(v.Color);
                for (int i = 0; i < 4; ++i)
                    Require(v.Color[i] >= 0 && v.Color[i] <= 1,
                            "Imported COLOR_0 requires linear 0..1");
            }
            bool textured = false;
            if (p.Material >= 0)
                for (auto image : d.Materials[p.Material].Images)
                    textured |= image >= 0;
            Require(!textured || p.UV0, "Textured primitive has no TEXCOORD_0; export UV0");
        }
    }
    Require(owned <= 128 * 1024 * 1024, "Import owned data exceeds 128 MiB working budget");
    std::vector<unsigned char> state(d.Nodes.size(), 0);
    std::vector<unsigned> parents(d.Nodes.size(), 0);
    std::function<void(int, unsigned)> visit = [&](int i, unsigned depth) {
        Require(i >= 0 && size_t(i) < d.Nodes.size() && depth <= 256,
                "Imported node index/depth limit");
        Require(!state[i], "Imported hierarchy cycles or repeats a node");
        state[i] = 1;
        auto &n = d.Nodes[i];
        AssetName(n.Name);
        Require(nodeKeys.insert(n.SourceKey).second && n.Mesh >= -1 &&
                    (n.Mesh < 0 || size_t(n.Mesh) < d.Meshes.size()),
                "Invalid imported node/source/mesh");
        Finite(n.Translation);
        Finite(n.Rotation);
        Finite(n.Scale);
        for (int j = 0; j < 3; ++j)
            Require(std::abs(n.Scale[j]) >= 1e-6f, "Singular imported node scale is unsupported");
        for (auto child : n.Children)
        {
            Require(child >= 0 && size_t(child) < d.Nodes.size() && ++parents[child] == 1,
                    "Imported node has multiple parents");
            visit(child, depth + 1);
        }
        state[i] = 2;
    };
    for (auto root : d.Roots)
        visit(root, 0);
    Require(std::any_of(d.Nodes.begin(), d.Nodes.end(), [](auto &n) { return n.Mesh >= 0; }),
            "Selected scene has no mesh instances");
    Require(std::all_of(state.begin(), state.end(), [](auto s) { return s == 2; }),
            "Adapter retained unreachable nodes; select one scene explicitly");
}
GeometryData CookMesh(const ImportedMeshData &mesh,
                      const std::vector<ImportedMaterialData> &materials, uint64_t id,
                      const std::vector<uint64_t> &slots, const Cancel &cancel)
{
    Require(mesh.Primitives.size() == slots.size(), "Cooked slot mapping mismatch");
    GeometryData output;
    output.Mesh = id;
    for (size_t pi = 0; pi < mesh.Primitives.size(); ++pi)
    {
        CheckCancelled(cancel);
        OperationProgress::Report("Cooking mesh " + mesh.Name, pi, mesh.Primitives.size());
        const auto &p = mesh.Primitives[pi];
        Require(p.Indices.size() * (sizeof(MeshVertex) + 8) +
                        output.Vertices.size() * sizeof(MeshVertex) + output.Indices.size() * 4 <=
                    192 * 1024 * 1024,
                "Mesh corner/reindex storage exceeds 192 MiB cooking budget");
        std::vector<MeshVertex> corners;
        corners.reserve(p.Indices.size());
        for (auto i : p.Indices)
        {
            Require(i < p.Vertices.size(), "Invalid triangle index during cook");
            corners.push_back(p.Vertices[i]);
        }
        const bool normalMap = p.Material >= 0 && materials.at(p.Material).Images[2] >= 0;
        for (size_t i = 0; i < corners.size(); i += 3)
        {
            auto cross = glm::cross(corners[i + 1].Position - corners[i].Position,
                                    corners[i + 2].Position - corners[i].Position);
            Require(glm::dot(cross, cross) > 1e-20f,
                    "Degenerate triangle rejected; remove zero-area geometry in the source");
            auto flat = glm::normalize(cross);
            if (normalMap)
            {
                Require(p.UV0, "Normal mapping requires UV0");
                auto a = corners[i + 1].UV - corners[i].UV, b = corners[i + 2].UV - corners[i].UV;
                Require(std::abs(a.x * b.y - a.y * b.x) > 1e-12f,
                        "Normal-mapped triangle has degenerate UV0; repair source UVs");
            }
            for (size_t j = i; j < i + 3; ++j)
            {
                auto &v = corners[j];
                if (!p.Normals)
                    v.Normal = flat;
                else
                {
                    Require(glm::dot(v.Normal, v.Normal) > 1e-12f, "Zero-length source normal");
                    v.Normal = glm::normalize(v.Normal);
                }
                if (!p.Normals || !p.Tangents)
                    v.Tangent = Fallback(v.Normal);
                else
                {
                    auto t = glm::vec3(v.Tangent);
                    Require(glm::dot(t, t) > 1e-12f && (v.Tangent.w == 1 || v.Tangent.w == -1),
                            "Invalid source tangent/sign");
                    t = glm::normalize(t);
                    Require(std::abs(glm::dot(t, v.Normal)) < .0001f,
                            "Source tangent is not orthogonal to its normal");
                    v.Tangent = {t, v.Tangent.w};
                }
            }
        }
        if (p.UV0 && (!p.Tangents || !p.Normals))
        {
            CheckCancelled(cancel);
            hz_import_memory_begin(64 * 1024 * 1024);
            CornerMesh contextData{&corners};
            SMikkTSpaceInterface api{};
            api.m_getNumFaces = Faces;
            api.m_getNumVerticesOfFace = Three;
            api.m_getPosition = Position;
            api.m_getNormal = Normal;
            api.m_getTexCoord = UV;
            api.m_setTSpaceBasic = Tangent;
            SMikkTSpaceContext context{&api, &contextData};
            auto ok = genTangSpaceDefault(&context);
            Require(ok && !hz_import_memory_failed() && !hz_import_memory_live(),
                    "Mikk tangent generation exceeded its 64 MiB allocation limit or failed");
        }
        CheckCancelled(cancel);
        std::vector<uint32_t> order(corners.size()), remap(corners.size());
        std::iota(order.begin(), order.end(), 0);
        std::sort(order.begin(), order.end(),
                  [&](auto a, auto b) { return Fields(corners[a]) < Fields(corners[b]); });
        bool first = true;
        std::array<float, 16> previous{};
        uint32_t index = 0;
        for (auto corner : order)
        {
            auto fields = Fields(corners[corner]);
            if (first || fields != previous)
            {
                Require(output.Vertices.size() < 1000000,
                        "Per-corner splits exceed 1 million cooked vertices");
                output.Vertices.push_back(corners[corner]);
                index = uint32_t(output.Vertices.size() - 1);
                previous = fields;
                first = false;
            }
            remap[corner] = index;
        }
        MeshPrimitive primitive{slots[pi],
                                uint32_t(output.Indices.size()),
                                uint32_t(corners.size()),
                                uint32_t(p.UV0) | (uint32_t(p.Color0) << 1),
                                {}};
        Require(output.Indices.size() + remap.size() <= 3000000,
                "Cooked index count exceeds 3 million");
        output.Indices.insert(output.Indices.end(), remap.begin(), remap.end());
        output.Primitives.push_back(primitive);
    }
    output.UpdateBounds();
    output.Validate();
    return output;
}
std::string CookImage(const ImportedImageData &image)
{
    auto &pixels = image.Pixels;
    Require(pixels.Width && pixels.Width <= 65535 && pixels.Height && pixels.Height <= 65535 &&
                pixels.Format == ImageFormat::RGBA8 &&
                pixels.Pixels.size() == uint64_t(pixels.Width) * pixels.Height * 4,
            "Invalid imported RGBA8 image for TGA");
    std::string bytes(18, '\0');
    bytes[2] = 2;
    bytes[12] = char(pixels.Width);
    bytes[13] = char(pixels.Width >> 8);
    bytes[14] = char(pixels.Height);
    bytes[15] = char(pixels.Height >> 8);
    bytes[16] = 32;
    bytes[17] = 0x28;
    bytes.reserve(18 + pixels.Pixels.size());
    for (size_t i = 0; i < pixels.Pixels.size(); i += 4)
    {
        bytes += char(pixels.Pixels[i + 2]);
        bytes += char(pixels.Pixels[i + 1]);
        bytes += char(pixels.Pixels[i]);
        bytes += char(pixels.Pixels[i + 3]);
    }
    return bytes;
}
} // namespace Hazel::Import
