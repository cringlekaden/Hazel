// Private GLB adapter checks. No symbols/types escape to engine assets or cooking.
#pragma once
#include "tinygltf_json_c.h"
#include "Hazel/Utils/UTF8.h"
#include <set>
#include <cmath>
namespace Hazel::Import::GLBPrivate
{
using JSON = tg3json_value;
inline void Require(bool yes, const std::string &why)
{
    if (!yes)
        throw std::runtime_error("GLB: " + why);
}
inline const JSON *Get(const JSON &n, const char *key)
{
    return tg3json_object_get(&n, key);
}
inline const JSON &Need(const JSON &n, const char *key)
{
    auto p = Get(n, key);
    Require(p, std::string("missing required field ") + key);
    return *p;
}
inline std::string Text(const JSON &n)
{
    Require(n.type == TG3JSON_STRING, "expected a string");
    std::string s(n.u.string.ptr, n.u.string.len);
    UTF8::Validate(s);
    return s;
}
inline uint64_t Integer(const JSON &n, uint64_t max = UINT32_MAX)
{
    if (n.type == TG3JSON_INT)
    {
        Require(n.u.integer >= 0 && uint64_t(n.u.integer) <= max,
                "expected a bounded nonnegative integer");
        return uint64_t(n.u.integer);
    }
    Require(n.type == TG3JSON_REAL && std::isfinite(n.u.real) && n.u.real >= 0 &&
                n.u.real <= double(max) && std::floor(n.u.real) == n.u.real,
            "expected a bounded nonnegative integer");
    return uint64_t(n.u.real);
}
inline double Number(const JSON &n)
{
    Require(n.type == TG3JSON_INT || n.type == TG3JSON_REAL, "expected a finite number");
    double v = n.type == TG3JSON_INT ? double(n.u.integer) : n.u.real;
    Require(std::isfinite(v) && std::abs(v) <= 1000000, "nonfinite/excessive number");
    return v;
}
inline void Boolean(const JSON &n)
{
    Require(n.type == TG3JSON_BOOL, "expected a boolean");
}
inline void Array(const JSON &n, size_t max)
{
    Require(n.type == TG3JSON_ARRAY && n.u.array.count <= max, "invalid/excessive array");
}
inline void Object(const JSON &n, std::initializer_list<const char *> keys)
{
    Require(n.type == TG3JSON_OBJECT, "expected an object");
    for (size_t i = 0; i < n.u.object.count; ++i)
    {
        auto &e = n.u.object.items[i];
        std::string key(e.key, e.key_len);
        bool found = key == "extras";
        for (auto allowed : keys)
            found |= key == allowed;
        Require(found,
                "unsupported field '" + key + "'; remove unsupported content before importing");
    }
    if (auto name = Get(n, "name"))
    {
        auto value = Text(*name);
        if (!value.empty())
            AssetName(value);
    }
}
inline void Vector(const JSON &n, size_t count)
{
    Array(n, count);
    Require(n.u.array.count == count, "wrong fixed-vector shape");
    for (size_t i = 0; i < count; ++i)
        Number(n.u.array.items[i]);
}
inline void IntField(const JSON &n, const char *key, uint64_t max = UINT32_MAX)
{
    if (auto v = Get(n, key))
        Integer(*v, max);
}
inline void NumField(const JSON &n, const char *key)
{
    if (auto v = Get(n, key))
        Number(*v);
}
inline void VecField(const JSON &n, const char *key, size_t size)
{
    if (auto v = Get(n, key))
        Vector(*v, size);
}
inline void Walk(const JSON &n, unsigned depth, size_t &remaining)
{
    Require(depth <= 64 && remaining, "JSON nesting/semantic traversal limit exceeded");
    --remaining;
    if (n.type == TG3JSON_STRING)
    {
        Text(n);
        return;
    }
    if (n.type == TG3JSON_OBJECT)
    {
        std::set<std::string> keys;
        for (size_t i = 0; i < n.u.object.count; ++i)
        {
            auto &e = n.u.object.items[i];
            std::string key(e.key, e.key_len);
            UTF8::Validate(key);
            Require(keys.insert(key).second, "duplicate JSON field '" + key + "'");
            Walk(*e.value, depth + 1, remaining);
        }
    }
    else if (n.type == TG3JSON_ARRAY)
        for (size_t i = 0; i < n.u.array.count; ++i)
            Walk(n.u.array.items[i], depth + 1, remaining);
}
template <class F> void Each(const JSON &root, const char *name, size_t max, F visit)
{
    if (auto a = Get(root, name))
    {
        Array(*a, max);
        for (size_t i = 0; i < a->u.array.count; ++i)
            try
            {
                visit(a->u.array.items[i]);
            }
            catch (const std::exception &e)
            {
                throw std::runtime_error(std::string(name) + "[" + std::to_string(i) +
                                         "]: " + e.what());
            }
    }
}
inline void TextureInfo(const JSON &n, bool normal = false, bool occlusion = false)
{
    Object(n, {"index", "texCoord", normal ? "scale" : "", occlusion ? "strength" : ""});
    Integer(Need(n, "index"));
    if (auto uv = Get(n, "texCoord"))
        Require(Integer(*uv) == 0, "only TEXCOORD_0 is supported");
    if (normal)
        NumField(n, "scale");
    if (occlusion)
        NumField(n, "strength");
}
inline void Preflight(const JSON &root)
{
    size_t remaining = 1000000;
    Walk(root, 0, remaining);
    Object(root, {"asset", "scene", "scenes", "nodes", "meshes", "materials", "textures", "images",
                  "samplers", "accessors", "bufferViews", "buffers", "extensionsUsed",
                  "extensionsRequired"});
    auto &asset = Need(root, "asset");
    Object(asset, {"version", "generator", "copyright", "minVersion"});
    Require(Text(Need(asset, "version")) == "2.0", "only glTF 2.0 is supported");
    if (auto v = Get(asset, "minVersion"))
        Require(Text(*v) == "2.0", "unsupported minimum glTF version");
    for (auto key : {"generator", "copyright"})
        if (auto v = Get(asset, key))
            Text(*v);
    for (auto key : {"extensionsUsed", "extensionsRequired"})
        Each(root, key, 16, [](auto &e) {
            Require(Text(e) == "KHR_materials_unlit",
                    "unsupported optional/required extension '" + Text(e) +
                        "'; material meaning cannot be approximated");
        });
    IntField(root, "scene", 9999);
    Each(root, "buffers", 1, [](auto &n) {
        Object(n, {"name", "byteLength"});
        Require(Integer(Need(n, "byteLength"), 67108864) > 0, "empty buffer is unsupported");
    });
    Each(root, "bufferViews", 65536, [](auto &n) {
        Object(n, {"name", "buffer", "byteOffset", "byteLength", "byteStride", "target"});
        Integer(Need(n, "buffer"), 0);
        Require(Integer(Need(n, "byteLength"), 67108864) > 0, "empty buffer view");
        IntField(n, "byteOffset", 67108864);
        if (auto s = Get(n, "byteStride"))
        {
            auto stride = Integer(*s, 252);
            Require(stride >= 4 && stride % 4 == 0,
                    "bufferView byteStride requires 4..252, multiple of 4");
        }
        if (auto t = Get(n, "target"))
        {
            auto target = Integer(*t);
            Require(target == 34962 || target == 34963, "unsupported bufferView target");
        }
    });
    Each(root, "accessors", 65536, [](auto &n) {
        Object(n, {"name", "bufferView", "byteOffset", "componentType", "normalized", "count",
                   "type", "min", "max"});
        Integer(Need(n, "bufferView"), 65535);
        IntField(n, "byteOffset", 67108864);
        auto count = Integer(Need(n, "count"), 3000000);
        Require(count > 0, "empty accessor");
        auto type = Text(Need(n, "type"));
        size_t shape = type == "SCALAR" ? 1
                       : type == "VEC2" ? 2
                       : type == "VEC3" ? 3
                       : type == "VEC4" ? 4
                                        : 0;
        Require(shape, "matrix accessors are outside the static mesh subset");
        Require(shape == 1 || count <= 1000000, "accessor vector count exceeds 1 million vertices");
        auto component = Integer(Need(n, "componentType"));
        Require(component == 5120 || component == 5121 || component == 5122 || component == 5123 ||
                    component == 5125 || component == 5126,
                "unsupported accessor component type");
        if (auto v = Get(n, "normalized"))
        {
            Boolean(*v);
            Require(!v->u.boolean || component == 5120 || component == 5121 || component == 5122 ||
                        component == 5123,
                    "invalid normalized component type");
        }
        VecField(n, "min", shape);
        VecField(n, "max", shape);
    });
    Each(root, "samplers", 4096, [](auto &n) {
        Object(n, {"name", "minFilter", "magFilter", "wrapS", "wrapT"});
        for (auto key : {"minFilter", "magFilter", "wrapS", "wrapT"})
            IntField(n, key);
    });
    Each(root, "textures", 4096, [](auto &n) {
        Object(n, {"name", "sampler", "source"});
        Integer(Need(n, "source"), 1023);
        IntField(n, "sampler", 4095);
    });
    Each(root, "images", 1024, [](auto &n) {
        Object(n, {"name", "bufferView", "mimeType"});
        Integer(Need(n, "bufferView"), 65535);
        auto mime = Text(Need(n, "mimeType"));
        Require(mime == "image/png" || mime == "image/jpeg",
                "embedded image requires PNG/JPEG; URIs and compressed textures are unsupported");
    });
    Each(root, "materials", 4096, [](auto &n) {
        Object(n, {"name", "pbrMetallicRoughness", "normalTexture", "occlusionTexture",
                   "emissiveTexture", "emissiveFactor", "alphaMode", "alphaCutoff", "doubleSided",
                   "extensions"});
        if (auto p = Get(n, "pbrMetallicRoughness"))
        {
            Object(*p, {"baseColorFactor", "baseColorTexture", "metallicFactor", "roughnessFactor",
                        "metallicRoughnessTexture"});
            VecField(*p, "baseColorFactor", 4);
            NumField(*p, "metallicFactor");
            NumField(*p, "roughnessFactor");
            for (auto key : {"baseColorTexture", "metallicRoughnessTexture"})
                if (auto t = Get(*p, key))
                    TextureInfo(*t);
        }
        if (auto t = Get(n, "normalTexture"))
            TextureInfo(*t, true);
        if (auto t = Get(n, "occlusionTexture"))
            TextureInfo(*t, false, true);
        if (auto t = Get(n, "emissiveTexture"))
            TextureInfo(*t);
        VecField(n, "emissiveFactor", 3);
        NumField(n, "alphaCutoff");
        if (auto v = Get(n, "doubleSided"))
            Boolean(*v);
        if (auto v = Get(n, "alphaMode"))
        {
            auto mode = Text(*v);
            Require(mode == "OPAQUE" || mode == "MASK",
                    "alpha BLEND is not implemented; export OPAQUE or MASK");
        }
        if (auto ex = Get(n, "extensions"))
        {
            Object(*ex, {"KHR_materials_unlit"});
            Require(ex->u.object.count == 1, "invalid material extension block");
            auto &unlit = Need(*ex, "KHR_materials_unlit");
            Require(unlit.type == TG3JSON_OBJECT && unlit.u.object.count == 0,
                    "KHR_materials_unlit must be an empty object");
        }
    });
    Each(root, "meshes", 1024, [](auto &n) {
        Object(n, {"name", "primitives"});
        auto &primitives = Need(n, "primitives");
        Array(primitives, 4096);
        Require(primitives.u.array.count > 0, "mesh has no primitives");
        for (size_t i = 0; i < primitives.u.array.count; ++i)
        {
            auto &p = primitives.u.array.items[i];
            Object(p, {"attributes", "indices", "material", "mode"});
            if (auto mode = Get(p, "mode"))
                Require(Integer(*mode) == 4, "only TRIANGLES primitives are supported");
            IntField(p, "indices", 65535);
            IntField(p, "material", 4095);
            auto &a = Need(p, "attributes");
            Object(a, {"POSITION", "NORMAL", "TANGENT", "TEXCOORD_0", "COLOR_0"});
            Integer(Need(a, "POSITION"), 65535);
            for (auto key : {"NORMAL", "TANGENT", "TEXCOORD_0", "COLOR_0"})
                IntField(a, key, 65535);
        }
    });
    Each(root, "nodes", 10000, [](auto &n) {
        Object(n, {"name", "mesh", "children", "translation", "rotation", "scale", "matrix"});
        IntField(n, "mesh", 1023);
        if (auto children = Get(n, "children"))
        {
            Array(*children, 10000);
            for (size_t i = 0; i < children->u.array.count; ++i)
                Integer(children->u.array.items[i], 9999);
        }
        VecField(n, "translation", 3);
        VecField(n, "rotation", 4);
        VecField(n, "scale", 3);
        VecField(n, "matrix", 16);
        Require(!Get(n, "matrix") ||
                    (!Get(n, "translation") && !Get(n, "rotation") && !Get(n, "scale")),
                "node cannot combine matrix with TRS");
    });
    Each(root, "scenes", 10000, [](auto &n) {
        Object(n, {"name", "nodes"});
        auto &nodes = Need(n, "nodes");
        Array(nodes, 10000);
        for (size_t i = 0; i < nodes.u.array.count; ++i)
            Integer(nodes.u.array.items[i], 9999);
    });
}
} // namespace Hazel::Import::GLBPrivate
