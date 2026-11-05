#pragma once
#include "AssetData.h"
#include "Hazel/Renderer/Texture.h"
#include <glm/glm.hpp>
#include <array>
namespace Hazel
{
enum class MaterialModel : uint8_t
{
    MetallicRoughness,
    Unlit
};
enum class MaterialAlpha : uint8_t
{
    Opaque,
    Mask
};
enum class MaterialTexture : uint8_t
{
    BaseColor,
    MetallicRoughness,
    Normal,
    Occlusion,
    Emissive,
    Count
};
const char *MaterialTextureName(MaterialTexture slot);
TextureEncoding MaterialTextureEncoding(MaterialTexture slot);
struct MaterialImage
{
    std::filesystem::path Image;
    TextureFilter Min = TextureFilter::LinearMipmapLinear, Mag = TextureFilter::Linear;
    TextureWrap S = TextureWrap::Repeat, T = TextureWrap::Repeat;
    bool operator==(const MaterialImage &b) const
    {
        return Image == b.Image && Min == b.Min && Mag == b.Mag && S == b.S && T == b.T;
    }
};
struct MaterialDefinition
{
    std::string Name = "New material";
    MaterialModel Model = MaterialModel::MetallicRoughness;
    glm::vec4 BaseColor{1}; // Linear RGB / straight alpha. Pickers display sRGB.
    glm::vec3 Emissive{};   // Linear nonnegative, no transfer on gains.
    float Metallic = 0, Roughness = .5f, NormalScale = 1, OcclusionStrength = 1, AlphaCutoff = .5f;
    MaterialAlpha Alpha = MaterialAlpha::Opaque;
    bool DoubleSided = false;
    std::array<MaterialImage, 5> Textures;
    void Validate() const;
    bool operator==(const MaterialDefinition &b) const;
};
MaterialDefinition ReadMaterialText(const std::string &bytes);
MaterialDefinition ReadMaterial(const std::filesystem::path &file);
std::string WriteMaterialText(const MaterialDefinition &data);
std::vector<std::filesystem::path> AuditMaterial(const std::filesystem::path &root,
                                                 const MaterialDefinition &data);
} // namespace Hazel
