#include "hzpch.h"
#include <set>
#include "Geometry.h"
#include <cmath>
#include <cstring>
#include <algorithm>
#include <limits>
namespace Hazel
{
namespace
{
void Require(bool yes, const char *why)
{
    if (!yes)
        throw std::runtime_error(why);
}
bool Near(float x, float y)
{
    return std::abs(x - y) <= .0001f;
}
MeshBounds Bounds(const GeometryData &d, const MeshPrimitive &p)
{
    Require(p.IndexCount && p.FirstIndex <= d.Indices.size() &&
                p.IndexCount <= d.Indices.size() - p.FirstIndex,
            "Primitive index range is invalid");
    MeshBounds b;
    b.Min = glm::vec3(std::numeric_limits<float>::max());
    b.Max = -b.Min;
    for (size_t i = p.FirstIndex; i < size_t(p.FirstIndex) + p.IndexCount; ++i)
    {
        auto index = d.Indices[i];
        Require(index < d.Vertices.size(), "Geometry vertex index is invalid");
        auto v = d.Vertices[index].Position;
        b.Min = glm::min(b.Min, v);
        b.Max = glm::max(b.Max, v);
    }
    return b;
}
void U32(std::string &b, uint32_t v)
{
    for (int i = 0; i < 4; ++i)
        b += char(v >> (i * 8));
}
void U64(std::string &b, uint64_t v)
{
    for (int i = 0; i < 8; ++i)
        b += char(v >> (i * 8));
}
void F32(std::string &b, float f)
{
    static_assert(sizeof(float) == 4 && std::numeric_limits<float>::is_iec559,
                  "IEEE754 f32 required");
    uint32_t v;
    std::memcpy(&v, &f, 4);
    U32(b, v);
}
template <int N> void Vec(std::string &b, const glm::vec<N, float> &v)
{
    for (int i = 0; i < N; ++i)
        F32(b, v[i]);
}
struct Reader
{
    std::string_view Bytes;
    size_t At = 0;
    uint64_t Integer(size_t n)
    {
        Require(At <= Bytes.size() && n <= Bytes.size() - At, "Truncated geometry field");
        uint64_t v = 0;
        for (size_t i = 0; i < n; ++i)
            v |= uint64_t(uint8_t(Bytes[At++])) << (8 * i);
        return v;
    }
    uint32_t U32()
    {
        return uint32_t(Integer(4));
    }
    uint64_t U64()
    {
        return Integer(8);
    }
    float F32()
    {
        auto v = U32();
        float f;
        std::memcpy(&f, &v, 4);
        return f;
    }
    template <int N> glm::vec<N, float> Vec()
    {
        glm::vec<N, float> v;
        for (int i = 0; i < N; ++i)
            v[i] = F32();
        return v;
    }
};
} // namespace
void GeometryData::UpdateBounds()
{
    Require(!Primitives.empty(), "Geometry needs primitives");
    for (size_t i = 0; i < Primitives.size(); ++i)
    {
        auto &p = Primitives[i];
        p.Bounds = ::Hazel::Bounds(*this, p);
        if (!i)
            Bounds = p.Bounds;
        else
        {
            Bounds.Min = glm::min(Bounds.Min, p.Bounds.Min);
            Bounds.Max = glm::max(Bounds.Max, p.Bounds.Max);
        }
    }
}
void GeometryData::Validate() const
{
    Require(Mesh && !Vertices.empty() && Vertices.size() <= 1000000 && !Indices.empty() &&
                Indices.size() <= 3000000 && !Primitives.empty() && Primitives.size() <= 4096,
            "Geometry exceeds supported mesh/count limits");
    for (const auto &v : Vertices)
    {
        auto finite = [](auto vec) {
            for (int i = 0; i < vec.length(); ++i)
                Require(std::isfinite(vec[i]) && std::abs(vec[i]) <= 1000000,
                        "Nonfinite/excessive geometry attribute");
        };
        finite(v.Position);
        finite(v.Normal);
        finite(v.Tangent);
        finite(v.UV);
        finite(v.Color);
        Require(Near(glm::dot(v.Normal, v.Normal), 1) &&
                    Near(glm::dot(glm::vec3(v.Tangent), glm::vec3(v.Tangent)), 1) &&
                    Near(glm::dot(v.Normal, glm::vec3(v.Tangent)), 0) &&
                    (v.Tangent.w == 1 || v.Tangent.w == -1),
                "Geometry normal/tangent basis is invalid");
        for (int i = 0; i < 4; ++i)
            Require(v.Color[i] >= 0 && v.Color[i] <= 1,
                    "Geometry vertex colors require linear 0..1 values");
    }
    std::set<uint64_t> slots;
    size_t next = 0;
    MeshBounds total;
    for (size_t i = 0; i < Primitives.size(); ++i)
    {
        const auto &p = Primitives[i];
        Require(p.Slot && slots.insert(p.Slot).second && p.FirstIndex == next && p.IndexCount &&
                    p.IndexCount % 3 == 0 && !(p.Attributes & ~3u),
                "Invalid geometry primitive/slot contract");
        auto actual = ::Hazel::Bounds(*this, p);
        Require(actual == p.Bounds, "Geometry primitive bounds disagree with vertices");
        next += p.IndexCount;
        if (!i)
            total = actual;
        else
        {
            total.Min = glm::min(total.Min, actual.Min);
            total.Max = glm::max(total.Max, actual.Max);
        }
        for (size_t j = p.FirstIndex; j < next; j += 3)
        {
            auto a = Vertices[Indices[j]].Position, b = Vertices[Indices[j + 1]].Position,
                 c = Vertices[Indices[j + 2]].Position;
            auto cross = glm::cross(b - a, c - a);
            Require(glm::dot(cross, cross) > 1e-20f, "Degenerate geometry triangle is unsupported");
        }
    }
    Require(next == Indices.size() && total == Bounds, "Geometry coverage/bounds are invalid");
}
std::string EncodeGeometry(const GeometryData &d)
{
    d.Validate();
    std::string payload;
    payload.reserve(d.Vertices.size() * 64 + d.Indices.size() * 4 + d.Primitives.size() * 48);
    for (auto &v : d.Vertices)
    {
        Vec(payload, v.Position);
        Vec(payload, v.Normal);
        Vec(payload, v.Tangent);
        Vec(payload, v.UV);
        Vec(payload, v.Color);
    }
    for (auto i : d.Indices)
        U32(payload, i);
    for (auto &p : d.Primitives)
    {
        U64(payload, p.Slot);
        U32(payload, p.FirstIndex);
        U32(payload, p.IndexCount);
        U32(payload, p.Attributes);
        Vec(payload, p.Bounds.Min);
        Vec(payload, p.Bounds.Max);
        U32(payload, 0);
    }
    const uint64_t index = 128 + d.Vertices.size() * 64, primitive = index + d.Indices.size() * 4;
    std::string b = "HZGEOM01";
    U32(b, 1);
    U32(b, 128);
    U64(b, d.Mesh);
    U64(b, 128);
    U64(b, index);
    U64(b, primitive);
    U32(b, uint32_t(d.Vertices.size()));
    U32(b, uint32_t(d.Indices.size()));
    U32(b, uint32_t(d.Primitives.size()));
    U32(b, 64);
    U64(b, 128 + payload.size());
    auto digest = AssetSHA256(payload.data(), payload.size());
    b.append(reinterpret_cast<const char *>(digest.data()), digest.size());
    Vec(b, d.Bounds.Min);
    Vec(b, d.Bounds.Max);
    Require(b.size() == 128, "Internal geometry header size");
    b += payload;
    Require(b.size() <= GeometryByteLimit, "Geometry artifact exceeds byte limit");
    return b;
}
GeometryData DecodeGeometry(std::string_view bytes)
{
    Require(bytes.size() >= 128 && bytes.size() <= GeometryByteLimit &&
                bytes.substr(0, 8) == "HZGEOM01",
            "Invalid/oversized geometry artifact");
    Reader r{bytes, 8};
    Require(r.U32() == 1 && r.U32() == 128, "Unsupported geometry version/header");
    GeometryData d;
    d.Mesh = r.U64();
    auto vertices = r.U64(), indices = r.U64(), primitives = r.U64();
    auto vc = r.U32(), ic = r.U32(), pc = r.U32();
    Require(r.U32() == 64 && r.U64() == bytes.size(), "Invalid geometry stride/length");
    Require(vc && vc <= 1000000 && ic && ic <= 3000000 && pc && pc <= 4096 && vertices == 128 &&
                indices == 128 + uint64_t(vc) * 64 && primitives == indices + uint64_t(ic) * 4 &&
                bytes.size() == primitives + uint64_t(pc) * 48,
            "Invalid geometry counts/offsets");
    auto digest = AssetSHA256(bytes.data() + 128, bytes.size() - 128);
    Require(std::memcmp(bytes.data() + 72, digest.data(), 32) == 0,
            "Geometry payload checksum mismatch");
    r.At = 104;
    d.Bounds.Min = r.Vec<3>();
    d.Bounds.Max = r.Vec<3>();
    d.Vertices.resize(vc);
    for (auto &v : d.Vertices)
    {
        v.Position = r.Vec<3>();
        v.Normal = r.Vec<3>();
        v.Tangent = r.Vec<4>();
        v.UV = r.Vec<2>();
        v.Color = r.Vec<4>();
    }
    d.Indices.resize(ic);
    for (auto &i : d.Indices)
        i = r.U32();
    d.Primitives.resize(pc);
    for (auto &p : d.Primitives)
    {
        p.Slot = r.U64();
        p.FirstIndex = r.U32();
        p.IndexCount = r.U32();
        p.Attributes = r.U32();
        p.Bounds.Min = r.Vec<3>();
        p.Bounds.Max = r.Vec<3>();
        Require(!r.U32(), "Unknown geometry primitive fields");
    }
    d.Validate();
    return d;
}
GeometryData ReadGeometry(const std::filesystem::path &file)
{
    return DecodeGeometry(ReadAssetBytes(file, GeometryByteLimit));
}
} // namespace Hazel
