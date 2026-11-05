#include "ImportedSceneData.h"
#include "Allocation.h"
#include "tiny_gltf_v3.h"
#include "GLBSemantics.h"
#include "Hazel/Core/OperationProgress.h"
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>
#include <cstring>
#include <algorithm>
#include <map>
namespace Hazel::Import
{
namespace
{
using namespace GLBPrivate;
uint32_t U32(std::string_view b, size_t at)
{
    Require(at <= b.size() && b.size() - at >= 4, "truncated field");
    return uint32_t(uint8_t(b[at])) | (uint32_t(uint8_t(b[at + 1])) << 8) |
           (uint32_t(uint8_t(b[at + 2])) << 16) | (uint32_t(uint8_t(b[at + 3])) << 24);
}
const char* ParserReason(tg3_error_code code)
{
    switch(code){
    case TG3_ERR_INVALID_INDEX:return "out-of-range source index; inspect node/accessor/material references";
    case TG3_ERR_INVALID_ACCESSOR:return "invalid accessor shape/count/offset/stride or byte range";
    case TG3_ERR_INVALID_BUFFER:case TG3_ERR_BUFFER_SIZE_MISMATCH:return "embedded buffer byte length/range mismatch";
    case TG3_ERR_INVALID_BUFFER_VIEW:return "invalid bufferView offset/length/stride";
    case TG3_ERR_INVALID_IMAGE:return "invalid embedded image bufferView/MIME";
    case TG3_ERR_OUT_OF_MEMORY:return "parser allocation budget exceeded";
    default:return "invalid source schema; check required fields, types and declared ranges";
    }
}
struct Framing
{
    std::string_view JSON, Bin;
};
Framing Frame(std::string_view b)
{
    Require(b.size() >= 20 && b.size() <= 67108864 && b.substr(0, 4) == "glTF" && U32(b, 4) == 2 &&
                U32(b, 8) == b.size(),
            "requires a complete GLB 2 file, at most 64 MiB");
    Framing f;
    size_t at = 12;
    unsigned chunks = 0;
    while (at < b.size())
    {
        Require(at % 4 == 0 && b.size() - at >= 8, "invalid chunk header/alignment");
        auto length = U32(b, at), type = U32(b, at + 4);
        at += 8;
        Require(length % 4 == 0 && length <= b.size() - at, "invalid chunk length/alignment");
        if (chunks == 0)
        {
            Require(type == 0x4e4f534a && length, "first chunk must be JSON");
            f.JSON = b.substr(at, length);
        }
        else
        {
            Require(chunks == 1 && type == 0x004e4942,
                    "only one JSON followed by one BIN chunk is supported");
            f.Bin = b.substr(at, length);
        }
        at += length;
        ++chunks;
    }
    return f;
}
std::string String(tg3_str s)
{
    return s.len ? std::string(s.data, s.len) : std::string();
}
std::string Name(tg3_str s, const char *fallback, size_t index)
{
    auto value = String(s);
    if (value.empty())
        value = std::string(fallback) + " " + std::to_string(index + 1);
    AssetName(value);
    return value;
}
template <class T> const T &At(const T *data, uint32_t count, int index, const char *label)
{
    Require(index >= 0 && uint32_t(index) < count, std::string("invalid ") + label + " index");
    return data[index];
}
struct Span
{
    const uint8_t *Data;
    uint64_t Size;
};
Span View(const tg3_model &m, int index)
{
    auto &v = At(m.buffer_views, m.buffer_views_count, index, "bufferView");
    auto &b = At(m.buffers, m.buffers_count, v.buffer, "buffer");
    Require(v.byte_offset <= b.byte_length && v.byte_length <= b.byte_length - v.byte_offset &&
                b.byte_length <= b.data.count,
            "bufferView escapes its declared buffer");
    return {b.data.data + v.byte_offset, v.byte_length};
}
struct Accessor
{
    const uint8_t *Data;
    uint64_t Count, Stride;
    int Component, Components;
    bool Normalized;
    double Read(uint64_t index, int component) const
    {
        Require(index < Count && component < Components, "accessor element out of range");
        auto p = Data + index * Stride +
                 component * (Component == 5126 || Component == 5125   ? 4
                              : Component == 5122 || Component == 5123 ? 2
                                                                       : 1);
        uint32_t value = uint32_t(p[0]);
        if (Component == 5122 || Component == 5123 || Component == 5125 || Component == 5126)
            value |= uint32_t(p[1]) << 8;
        if (Component == 5125 || Component == 5126)
            value |= (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
        if (Component == 5126)
        {
            float f;
            std::memcpy(&f, &value, 4);
            Require(std::isfinite(f) && std::abs(f) <= 1000000,
                    "nonfinite/excessive floating accessor value");
            return f;
        }
        if (Component == 5120)
        {
            auto v = int8_t(value);
            return Normalized ? std::max(-1., double(v) / 127) : v;
        }
        if (Component == 5122)
        {
            auto v = int16_t(value);
            return Normalized ? std::max(-1., double(v) / 32767) : v;
        }
        return Normalized ? double(value) / (Component == 5121 ? 255. : 65535.) : value;
    }
};
Accessor ReadAccessor(const tg3_model &m, int index, int components, bool floating,
                      bool indices = false)
{
    auto &a = At(m.accessors, m.accessors_count, index, "accessor");
    auto &v = At(m.buffer_views, m.buffer_views_count, a.buffer_view, "accessor bufferView");
    Require(!a.sparse.is_sparse, "sparse accessors are unsupported");
    auto type = components == 1 ? TG3_TYPE_SCALAR : components;
    Require(a.type == type, "accessor has the wrong vector shape");
    if (floating)
        Require(a.component_type == 5126 && !a.normalized,
                "positions/normals/tangents require non-normalized FLOAT accessors");
    else if (indices)
        Require(
            (a.component_type == 5121 || a.component_type == 5123 || a.component_type == 5125) &&
                !a.normalized && !v.byte_stride,
            "indices require tightly packed unsigned BYTE/SHORT/INT");
    else
        Require((a.component_type == 5126 && !a.normalized) ||
                    ((a.component_type == 5121 || a.component_type == 5123) && a.normalized),
                "UV/color require FLOAT or normalized unsigned BYTE/SHORT");
    uint64_t size = a.component_type == 5126 || a.component_type == 5125   ? 4
                    : a.component_type == 5122 || a.component_type == 5123 ? 2
                                                                           : 1;
    auto element = size * components;
    auto stride = v.byte_stride ? v.byte_stride : element;
    auto span = View(m, a.buffer_view);
    Require(a.count && a.count <= (indices ? 3000000 : 1000000) && stride >= element &&
                stride <= 252 && stride % size == 0 && a.byte_offset % size == 0 &&
                v.byte_offset % size == 0 && a.byte_offset <= span.Size,
            "invalid accessor count/stride/alignment/offset");
    Require(element <= span.Size - a.byte_offset &&
                (a.count - 1) <= (span.Size - a.byte_offset - element) / stride,
            "accessor range escapes its bufferView");
    return {span.Data + a.byte_offset, a.count,    stride,
            a.component_type,          components, a.normalized != 0};
}
int Attribute(const tg3_primitive &p, const char *key)
{
    for (uint32_t i = 0; i < p.attributes_count; ++i)
        if (String(p.attributes[i].key) == key)
            return p.attributes[i].value;
    return -1;
}
TextureFilter Filter(int value, bool mag)
{
    switch (value)
    {
    case -1:
        return mag ? TextureFilter::Linear : TextureFilter::LinearMipmapLinear;
    case 9728:
        return TextureFilter::Nearest;
    case 9729:
        return TextureFilter::Linear;
    case 9984:
        Require(!mag, "mipmap magnification");
        return TextureFilter::NearestMipmapNearest;
    case 9985:
        Require(!mag, "mipmap magnification");
        return TextureFilter::LinearMipmapNearest;
    case 9986:
        Require(!mag, "mipmap magnification");
        return TextureFilter::NearestMipmapLinear;
    case 9987:
        Require(!mag, "mipmap magnification");
        return TextureFilter::LinearMipmapLinear;
    default:
        throw std::runtime_error("GLB: unknown sampler filter");
    }
}
TextureWrap Wrap(int value)
{
    switch (value)
    {
    case 10497:
        return TextureWrap::Repeat;
    case 33071:
        return TextureWrap::ClampToEdge;
    case 33648:
        return TextureWrap::MirroredRepeat;
    default:
        throw std::runtime_error("GLB: unknown sampler wrap");
    }
}
void Texture(const tg3_model &m, int texture, int uv, ImportedMaterialData &material, unsigned slot)
{
    Require(uv == 0, "only texture UV0 is supported");
    if (texture < 0)
        return;
    auto &t = At(m.textures, m.textures_count, texture, "texture");
    At(m.images, m.images_count, t.source, "texture image");
    material.Images[slot] = t.source;
    auto &value = material.Values.Textures[slot];
    if (t.sampler >= 0)
    {
        auto &s = At(m.samplers, m.samplers_count, t.sampler, "sampler");
        value.Min = Filter(s.min_filter, false);
        value.Mag = Filter(s.mag_filter, true);
        value.S = Wrap(s.wrap_s);
        value.T = Wrap(s.wrap_t);
    }
}
void Transform(const tg3_node &n, ImportedNodeData &out)
{
    glm::mat4 original(1);
    glm::quat rotation;
    if (n.has_matrix)
    {
        for (int c = 0; c < 4; ++c)
            for (int r = 0; r < 4; ++r)
                original[c][r] = float(n.matrix[c * 4 + r]);
        Require(original[0][3] == 0 && original[1][3] == 0 && original[2][3] == 0 &&
                    original[3][3] == 1,
                "node matrix must be affine");
        out.Translation = glm::vec3(original[3]);
        glm::mat3 axes(original);
        for (int i = 0; i < 3; ++i)
        {
            out.Scale[i] = glm::length(axes[i]);
            Require(out.Scale[i] >= 1e-6f, "singular node matrix");
            axes[i] /= out.Scale[i];
        }
        Require(std::abs(glm::dot(axes[0], axes[1])) < 1e-5f &&
                    std::abs(glm::dot(axes[0], axes[2])) < 1e-5f &&
                    std::abs(glm::dot(axes[1], axes[2])) < 1e-5f,
                "sheared node matrices cannot be represented as native TRS");
        if (glm::determinant(axes) < 0)
        {
            out.Scale.x = -out.Scale.x;
            axes[0] = -axes[0];
        }
        rotation = glm::quat_cast(axes);
    }
    else
    {
        for (int i = 0; i < 3; ++i)
        {
            out.Translation[i] = float(n.translation[i]);
            out.Scale[i] = float(n.scale[i]);
        }
        rotation = {float(n.rotation[3]), float(n.rotation[0]), float(n.rotation[1]),
                    float(n.rotation[2])};
        Require(std::abs(glm::dot(rotation, rotation) - 1) < 1e-4f,
                "node rotation must be a unit quaternion");
        original = glm::translate(glm::mat4(1), out.Translation) * glm::toMat4(rotation) *
                   glm::scale(glm::mat4(1), out.Scale);
    }
    out.Rotation = glm::eulerAngles(rotation);
    auto rebuilt = glm::translate(glm::mat4(1), out.Translation) *
                   glm::toMat4(glm::quat(out.Rotation)) * glm::scale(glm::mat4(1), out.Scale);
    for (int c = 0; c < 4; ++c)
        for (int r = 0; r < 4; ++r)
            Require(std::isfinite(original[c][r]) &&
                        std::abs(original[c][r] - rebuilt[c][r]) <=
                            1e-4f * std::max(1.f, std::abs(original[c][r])),
                    "node matrix is not exactly representable by native signed TRS");
}
} // namespace
ImportedSceneData ReadGLB(std::string_view bytes, int scene, const Cancel &cancel)
{
    CheckCancelled(cancel);
    OperationProgress::Report("Validating GLB framing and semantics");
    auto frame = Frame(bytes);
    hz_import_memory_begin(128 * 1024 * 1024);
    {
        struct JSONOwner
        {
            JSON Value{};
            ~JSONOwner()
            {
                tg3json_value_free(&Value);
            }
        } root;
        tg3json_parse_options options{};
        options.depth_limit = 64;
        options.memory_budget = 64 * 1024 * 1024;
        options.max_single_alloc = 32 * 1024 * 1024;
        options.max_string_length = 1024 * 1024;
        const char *error = nullptr;
        Require(tg3json_parse_n_opts(frame.JSON.data(), frame.JSON.size(), &options, &root.Value,
                                     &error) &&
                    !hz_import_memory_failed(),
                "malformed JSON or 64 MiB JSON allocation limit exceeded");
        Preflight(root.Value);
    }
    Require(!hz_import_memory_live(), "JSON preflight did not release memory");
    CheckCancelled(cancel);
    struct ModelOwner
    {
        tg3_model Model{};
        tg3_error_stack Errors{};
        ModelOwner()
        {
            tg3_error_stack_init(&Errors);
        }
        ~ModelOwner()
        {
            tg3_model_free(&Model);
            tg3_error_stack_free(&Errors);
        }
    } owner;
    tg3_parse_options options;
    tg3_parse_options_init(&options);
    options.strictness = TG3_STRICT;
    options.images_as_is = 1;
    options.borrow_input_buffers = 1;
    options.skip_extras_values = 1;
    options.memory.memory_budget = 64 * 1024 * 1024;
    options.memory.max_single_alloc = 32 * 1024 * 1024;
    auto code =
        tg3_parse_glb(&owner.Model, &owner.Errors, reinterpret_cast<const uint8_t *>(bytes.data()),
                      bytes.size(), nullptr, 0, &options);
    // Never read upstream message/json_path pointers (including under arena exhaustion).
    Require(code == TG3_OK && !owner.Errors.has_error && !hz_import_memory_failed(),
            std::string(hz_import_memory_failed()?"parser allocation budget exceeded":ParserReason(code))+" (code "+std::to_string(unsigned(code))+")");
    const auto &m = owner.Model;
    Require(m.buffers_count == 1 && m.buffers[0].byte_length <= frame.Bin.size() &&
                frame.Bin.size() - m.buffers[0].byte_length <= 3,
            "BIN length does not match its single embedded buffer");
    for (size_t i = size_t(m.buffers[0].byte_length); i < frame.Bin.size(); ++i)
        Require(frame.Bin[i] == 0, "BIN padding must be zero");
    Require(m.meshes_count && m.meshes_count <= 1024 && m.materials_count <= 4096 &&
                m.images_count <= 1024 && m.nodes_count <= 10000 && m.scenes_count &&
                m.scenes_count <= 10000,
            "unsupported model counts/no scene geometry");
    if (scene < 0)
    {
        scene = m.default_scene;
        if (scene < 0)
        {
            Require(m.scenes_count == 1, "multiple scenes without a default; choose a scene index");
            scene = 0;
        }
    }
    auto &selected = At(m.scenes, m.scenes_count, scene, "selected scene");
    ImportedSceneData d;
    d.Adapter = "GLB2/TinyGLTF-" HZ_IMPORT_TINYGLTF_COMMIT;
    size_t imageBytes = 0, vertexCount = 0, indexCount = 0;
    for (uint32_t i = 0; i < m.images_count; ++i)
    {
        CheckCancelled(cancel);
        OperationProgress::Report("Decoding embedded image", i, m.images_count);
        auto &image = m.images[i];
        auto encoded = View(m, image.buffer_view);
        auto mime = String(image.mime_type);
        Require((mime == "image/png" && encoded.Size >= 8 &&
                 std::memcmp(encoded.Data, "\x89PNG\r\n\x1a\n", 8) == 0) ||
                    (mime == "image/jpeg" && encoded.Size >= 3 && encoded.Data[0] == 255 &&
                     encoded.Data[1] == 216 && encoded.Data[2] == 255),
                "image MIME and encoded bytes disagree");
        auto pixels =
            Texture2D::ReadImageBytes(encoded.Data, size_t(encoded.Size), ImageFormat::RGBA8,
                                      (64 * 1024 * 1024 - imageBytes) / 4, true);
        imageBytes += pixels.Pixels.size();
        d.Images.push_back(
            {"image:" + std::to_string(i), Name(image.name, "Image", i), std::move(pixels)});
    }
    for (uint32_t i = 0; i < m.materials_count; ++i)
    {
        auto &in = m.materials[i];
        ImportedMaterialData out;
        out.SourceKey = "material:" + std::to_string(i);
        auto &v = out.Values;
        v.Name = Name(in.name, "Material", i);
        auto &p = in.pbr_metallic_roughness;
        for (int j = 0; j < 4; ++j)
            v.BaseColor[j] = float(p.base_color_factor[j]);
        for (int j = 0; j < 3; ++j)
            v.Emissive[j] = float(in.emissive_factor[j]);
        v.Metallic = float(p.metallic_factor);
        v.Roughness = float(p.roughness_factor);
        v.NormalScale = float(in.normal_texture.scale);
        v.OcclusionStrength = float(in.occlusion_texture.strength);
        v.AlphaCutoff = float(in.alpha_cutoff);
        v.DoubleSided = in.double_sided != 0;
        auto alpha = String(in.alpha_mode);
        Require(alpha == "OPAQUE" || alpha == "MASK", "unsupported material alpha mode");
        v.Alpha = alpha == "MASK" ? MaterialAlpha::Mask : MaterialAlpha::Opaque;
        for (uint32_t j = 0; j < in.ext.extensions_count; ++j)
        {
            Require(String(in.ext.extensions[j].name) == "KHR_materials_unlit",
                    "unsupported material extension");
            v.Model = MaterialModel::Unlit;
        }
        Texture(m, p.base_color_texture.index, p.base_color_texture.tex_coord, out, 0);
        Texture(m, p.metallic_roughness_texture.index, p.metallic_roughness_texture.tex_coord, out,
                1);
        Texture(m, in.normal_texture.index, in.normal_texture.tex_coord, out, 2);
        Texture(m, in.occlusion_texture.index, in.occlusion_texture.tex_coord, out, 3);
        Texture(m, in.emissive_texture.index, in.emissive_texture.tex_coord, out, 4);
        if (v.Model == MaterialModel::Unlit)
            Require(out.Images[1] < 0 && out.Images[2] < 0 && out.Images[3] < 0 &&
                        out.Images[4] < 0 && v.Emissive == glm::vec3(0),
                    "Unlit source contains ignored lighting inputs; remove them explicitly before "
                    "importing");
        v.Validate();
        d.Materials.push_back(std::move(out));
    }
    for (uint32_t i = 0; i < m.meshes_count; ++i)
    {
        CheckCancelled(cancel);
        auto &mesh = m.meshes[i];
        ImportedMeshData out;
        out.Name = Name(mesh.name, "Mesh", i);
        out.SourceKey = "mesh:" + std::to_string(i);
        Require(mesh.primitives_count && mesh.primitives_count <= 4096, "invalid primitive count");
        for (uint32_t j = 0; j < mesh.primitives_count; ++j)
        {
            auto &primitive = mesh.primitives[j];
            ImportedPrimitiveData p;
            p.SourceKey = out.SourceKey + "/primitive:" + std::to_string(j);
            p.Material = primitive.material;
            auto position = ReadAccessor(m, Attribute(primitive, "POSITION"), 3, true);
            vertexCount += position.Count;
            Require(vertexCount <= 1000000, "source exceeds 1 million vertices");
            p.Vertices.resize(size_t(position.Count));
            for (size_t k = 0; k < p.Vertices.size(); ++k)
                for (int c = 0; c < 3; ++c)
                    p.Vertices[k].Position[c] = float(position.Read(k, c));
            for (auto key : {"NORMAL", "TANGENT", "TEXCOORD_0", "COLOR_0"})
            {
                int index = Attribute(primitive, key);
                if (index < 0)
                    continue;
                bool normal = std::strcmp(key, "NORMAL") == 0,
                     tangent = std::strcmp(key, "TANGENT") == 0,
                     uv = std::strcmp(key, "TEXCOORD_0") == 0;
                int shape = normal    ? 3
                            : tangent ? 4
                            : uv      ? 2
                            : At(m.accessors, m.accessors_count, index, "color accessor").type ==
                                    TG3_TYPE_VEC3
                                ? 3
                                : 4;
                auto values = ReadAccessor(m, index, shape, normal || tangent);
                Require(values.Count == position.Count, "attribute counts disagree");
                p.Normals |= normal;
                p.Tangents |= tangent;
                p.UV0 |= uv;
                p.Color0 |= !normal && !tangent && !uv;
                for (size_t k = 0; k < p.Vertices.size(); ++k)
                    for (int c = 0; c < shape; ++c)
                    {
                        float value = float(values.Read(k, c));
                        auto &v = p.Vertices[k];
                        if (normal)
                            v.Normal[c] = value;
                        else if (tangent)
                            v.Tangent[c] = value;
                        else if (uv)
                            v.UV[c] = value;
                        else
                            v.Color[c] = value;
                    }
            }
            if (primitive.indices >= 0)
            {
                auto indices = ReadAccessor(m, primitive.indices, 1, false, true);
                indexCount += indices.Count;
                Require(indexCount <= 3000000, "source exceeds 3 million indices");
                p.Indices.resize(size_t(indices.Count));
                for (size_t k = 0; k < p.Indices.size(); ++k)
                {
                    auto value = indices.Read(k, 0);
                    Require(value < p.Vertices.size(),
                            "triangle index escapes its position accessor");
                    p.Indices[k] = uint32_t(value);
                }
            }
            else
            {
                indexCount += position.Count;
                Require(indexCount <= 3000000, "source exceeds 3 million indices");
                p.Indices.resize(size_t(position.Count));
                for (uint32_t k = 0; k < p.Indices.size(); ++k)
                    p.Indices[k] = k;
            }
            out.Primitives.push_back(std::move(p));
        }
        d.Meshes.push_back(std::move(out));
    }
    // Only the selected hierarchy is published. Detect cycles/multiple parents before
    // allocating a native hierarchy; source array indices remain private provenance.
    std::vector<int> remap(m.nodes_count, -1);
    std::function<int(int, unsigned)> copy = [&](int index, unsigned depth) {
        auto &node = At(m.nodes, m.nodes_count, index, "node");
        Require(depth <= 256 && remap[index] < 0,
                "node hierarchy cycle, repeated root or multiple parents");
        int target = int(d.Nodes.size());
        remap[index] = target;
        ImportedNodeData out;
        out.Name = Name(node.name, "Node", size_t(index));
        out.SourceKey = "node:" + std::to_string(index);
        out.Mesh = node.mesh;
        Transform(node, out);
        d.Nodes.push_back(std::move(out));
        for (uint32_t j = 0; j < node.children_count; ++j)
        {
            int child = copy(node.children[j], depth + 1);
            d.Nodes[target].Children.push_back(child);
        }
        return target;
    };
    for (uint32_t i = 0; i < selected.nodes_count; ++i)
        d.Roots.push_back(copy(selected.nodes[i], 0));
    CheckCancelled(cancel);
    ValidateImported(d);
    return d;
}
} // namespace Hazel::Import
