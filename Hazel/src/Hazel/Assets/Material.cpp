#include "hzpch.h"
#include <set>
#include "Material.h"
#include "Hazel/Core/DocumentSchema.h"
#include "Hazel/Project/Project.h"
#include <yaml-cpp/yaml.h>
#include <cmath>
#include <cstring>
namespace Hazel
{
const char *MaterialTextureName(MaterialTexture s)
{
    static const char *names[]{"BaseColor", "MetallicRoughness", "Normal", "Occlusion", "Emissive"};
    if (unsigned(s) >= 5)
        throw std::runtime_error("Unknown material texture slot");
    return names[unsigned(s)];
}
TextureEncoding MaterialTextureEncoding(MaterialTexture s)
{
    MaterialTextureName(s);
    return s == MaterialTexture::BaseColor || s == MaterialTexture::Emissive
               ? TextureEncoding::SRGB
               : TextureEncoding::Linear;
}
TextureSpecification MaterialTextureSpecification(MaterialTexture slot, const MaterialImage &image)
{
    auto spec = TextureSpecification::FileDefaults();
    spec.Format = ImageFormat::RGBA8;
    spec.Encoding = MaterialTextureEncoding(slot);
    spec.MinFilter = image.Min;
    spec.MagFilter = image.Mag;
    spec.WrapS = image.S;
    spec.WrapT = image.T;
    spec.GenerateMips = unsigned(image.Min) >= 2;
    spec.Validate(true);
    return spec;
}
void MaterialDefinition::Validate() const
{
    AssetName(Name);
    if (unsigned(Model) > 1 || unsigned(Alpha) > 1)
        throw std::runtime_error("Unsupported material model/alpha mode");
    auto range = [](float v, float lo, float hi) {
        if (!std::isfinite(v) || v < lo || v > hi)
            throw std::runtime_error("Material value is nonfinite/out of range");
    };
    for (int i = 0; i < 4; ++i)
        range(BaseColor[i], 0, 1);
    for (int i = 0; i < 3; ++i)
        range(Emissive[i], 0, 1);
    range(Metallic, 0, 1);
    range(Roughness, 0, 1);
    range(NormalScale, -16, 16);
    range(OcclusionStrength, 0, 1);
    range(AlphaCutoff, 0, 1);
    for (auto &t : Textures)
    {
        AssetPath(t.Image, nullptr, true);
        TextureSpecification s = TextureSpecification::FileDefaults();
        s.MinFilter = t.Min;
        s.MagFilter = t.Mag;
        s.WrapS = t.S;
        s.WrapT = t.T;
        s.Validate(true);
    }
    if (Model == MaterialModel::Unlit &&
        (!Textures[1].Image.empty() || !Textures[2].Image.empty() || !Textures[3].Image.empty() ||
         !Textures[4].Image.empty() || Emissive != glm::vec3(0)))
        throw std::runtime_error("Unlit materials support base color and alpha only");
}
bool MaterialDefinition::operator==(const MaterialDefinition &b) const
{
    auto same = [](float a, float b) { return std::memcmp(&a, &b, sizeof(float)) == 0; };
    bool values = true;
    for (int i = 0; i < 4; ++i)
        values &= same(BaseColor[i], b.BaseColor[i]);
    for (int i = 0; i < 3; ++i)
        values &= same(Emissive[i], b.Emissive[i]);
    return values && Name == b.Name && Model == b.Model && same(Metallic, b.Metallic) &&
           same(Roughness, b.Roughness) && same(NormalScale, b.NormalScale) &&
           same(OcclusionStrength, b.OcclusionStrength) && same(AlphaCutoff, b.AlphaCutoff) &&
           Alpha == b.Alpha && DoubleSided == b.DoubleSided && Textures == b.Textures;
}
namespace
{
template <int N> void ReadVector(const YAML::Node &n, glm::vec<N, float> &v)
{
    if (!n.IsSequence() || n.size() != N)
        throw std::runtime_error("Invalid material vector");
    for (int i = 0; i < N; ++i)
        v[i] = n[i].as<float>();
}
template <int N> void WriteVector(YAML::Emitter &o, const glm::vec<N, float> &v)
{
    o << YAML::Flow << YAML::BeginSeq;
    for (int i = 0; i < N; ++i)
        o << v[i];
    o << YAML::EndSeq;
}
} // namespace
MaterialDefinition ReadMaterialText(const std::string &bytes)
{
    if (bytes.size() > 65536)
        throw std::runtime_error("Material exceeds 64 KiB");
    auto root = YAML::Load(bytes);
    DocumentSchema::Structure(root);
    DocumentSchema::Keys(root, {"Material"}, "root");
    auto n = root["Material"];
    DocumentSchema::Keys(n,
                         {"Version", "Name", "Model", "BaseColor", "Emissive", "Metallic",
                          "Roughness", "NormalScale", "OcclusionStrength", "Alpha", "AlphaCutoff",
                          "DoubleSided", "Textures"},
                         "Material");
    if (n["Version"].as<unsigned>() != 1)
        throw std::runtime_error("Unsupported Material.Version");
    MaterialDefinition d;
    d.Name = n["Name"].as<std::string>();
    auto model = n["Model"].as<std::string>();
    if (model == "MetallicRoughness")
        d.Model = MaterialModel::MetallicRoughness;
    else if (model == "Unlit")
        d.Model = MaterialModel::Unlit;
    else
        throw std::runtime_error("Unknown material model");
    ReadVector(n["BaseColor"], d.BaseColor);
    ReadVector(n["Emissive"], d.Emissive);
    d.Metallic = n["Metallic"].as<float>();
    d.Roughness = n["Roughness"].as<float>();
    d.NormalScale = n["NormalScale"].as<float>();
    d.OcclusionStrength = n["OcclusionStrength"].as<float>();
    d.AlphaCutoff = n["AlphaCutoff"].as<float>();
    d.DoubleSided = n["DoubleSided"].as<bool>();
    auto alpha = n["Alpha"].as<std::string>();
    if (alpha == "Opaque")
        d.Alpha = MaterialAlpha::Opaque;
    else if (alpha == "Mask")
        d.Alpha = MaterialAlpha::Mask;
    else
        throw std::runtime_error("Alpha Blend is unsupported; choose Opaque or Mask");
    auto textures = n["Textures"];
    DocumentSchema::Keys(textures,
                         {"BaseColor", "MetallicRoughness", "Normal", "Occlusion", "Emissive"},
                         "material textures");
    for (unsigned i = 0; i < 5; ++i)
    {
        auto t = textures[MaterialTextureName(MaterialTexture(i))];
        DocumentSchema::Keys(t, {"Image", "Min", "Mag", "S", "T"}, "material texture");
        auto &v = d.Textures[i];
        v.Image = std::filesystem::u8path(t["Image"].as<std::string>());
        v.Min = ParseTextureFilter(t["Min"].as<std::string>());
        v.Mag = ParseTextureFilter(t["Mag"].as<std::string>());
        v.S = ParseTextureWrap(t["S"].as<std::string>());
        v.T = ParseTextureWrap(t["T"].as<std::string>());
    }
    d.Validate();
    return d;
}
MaterialDefinition ReadMaterial(const std::filesystem::path &file)
{
    return ReadMaterialText(ReadAssetBytes(file, 65536));
}
std::string WriteMaterialText(const MaterialDefinition &d)
{
    d.Validate();
    YAML::Emitter o;
    o.SetFloatPrecision(9);
    o << YAML::BeginMap << YAML::Key << "Material" << YAML::Value << YAML::BeginMap << YAML::Key
      << "Version" << YAML::Value << 1 << YAML::Key << "Name" << YAML::Value << d.Name << YAML::Key
      << "Model" << YAML::Value << (d.Model == MaterialModel::Unlit ? "Unlit" : "MetallicRoughness")
      << YAML::Key << "BaseColor" << YAML::Value;
    WriteVector(o, d.BaseColor);
    o << YAML::Key << "Emissive" << YAML::Value;
    WriteVector(o, d.Emissive);
    o << YAML::Key << "Metallic" << YAML::Value << d.Metallic << YAML::Key << "Roughness"
      << YAML::Value << d.Roughness << YAML::Key << "NormalScale" << YAML::Value << d.NormalScale
      << YAML::Key << "OcclusionStrength" << YAML::Value << d.OcclusionStrength << YAML::Key
      << "Alpha" << YAML::Value << (d.Alpha == MaterialAlpha::Opaque ? "Opaque" : "Mask")
      << YAML::Key << "AlphaCutoff" << YAML::Value << d.AlphaCutoff << YAML::Key << "DoubleSided"
      << YAML::Value << d.DoubleSided << YAML::Key << "Textures" << YAML::Value << YAML::BeginMap;
    for (unsigned i = 0; i < 5; ++i)
    {
        auto &t = d.Textures[i];
        o << YAML::Key << MaterialTextureName(MaterialTexture(i)) << YAML::Value << YAML::BeginMap
          << YAML::Key << "Image" << YAML::Value << t.Image.generic_u8string() << YAML::Key << "Min"
          << YAML::Value << TextureFilterName(t.Min) << YAML::Key << "Mag" << YAML::Value
          << TextureFilterName(t.Mag) << YAML::Key << "S" << YAML::Value << TextureWrapName(t.S)
          << YAML::Key << "T" << YAML::Value << TextureWrapName(t.T) << YAML::EndMap;
    }
    o << YAML::EndMap << YAML::EndMap << YAML::EndMap;
    return o.c_str();
}
std::vector<std::filesystem::path> AuditMaterial(const std::filesystem::path &root,
                                                 const MaterialDefinition &d)
{
    d.Validate();
    std::set<std::filesystem::path> deps;
    for (unsigned i = 0; i < d.Textures.size(); ++i)
        if (const auto &t = d.Textures[i]; !t.Image.empty())
        {
            auto spec = MaterialTextureSpecification(MaterialTexture(i), t);
            Texture2D::ReadImage(Project::ResolveOwnedAsset(root, t.Image), spec.Format);
            deps.insert(t.Image);
        }
    return {deps.begin(), deps.end()};
}
} // namespace Hazel
