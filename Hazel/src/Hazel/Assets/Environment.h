#pragma once
#include "AssetData.h"
#include "Hazel/Core/Base.h"
#include <glm/glm.hpp>
#include <vector>
namespace Hazel
{
// Normalized source boundary: linear Rec.709/D65 RGB, top-down equirectangular
// rows. No source format, parser, transfer function or graphics handle survives.
struct RadianceImage
{
    uint32_t Width = 0, Height = 0;
    std::vector<float> RGB;
    void Validate() const;
};
RadianceImage DecodeRadianceHDR(const std::string &bytes);
inline constexpr const char *RadianceEvaluation =
    "stb_image-2.23-29b5d519a3eae19a0cf30a79e35e456a067b2f0279d27a5a9b3015c1d25b74d8/"
    "HDR-framing1-linearRec709D65-top";
inline constexpr const char *IBLRecipe =
    "Hazel.IBL_v1.GGX-correlated-single.r045.E-pi.AB-NoV-r.cube-top";
inline constexpr unsigned EnvironmentProducerContract = 2;
inline constexpr const char *EnvironmentPreparationRecipe =
    "Hazel.EnvPrepare_v2.SolidAngleBalancedMIS.BilinearSource.Tail4";
struct EnvironmentProfile
{
    uint32_t Background = 256, Specular = 512, Irradiance = 64, Samples = 4096;
    void Validate() const;
    bool operator==(const EnvironmentProfile &other) const {
        return Background==other.Background && Specular==other.Specular && Irradiance==other.Irradiance && Samples==other.Samples;
    }
    static EnvironmentProfile Preview() { return {128,128,32,1024}; }
    static EnvironmentProfile Production() { return {}; }
    static EnvironmentProfile High() { auto p=Production();p.Background=512;p.Samples=16384;return p; }
};
struct EnvironmentDefinition
{
    std::string Name = "Image environment", Notice;
    std::filesystem::path Image;
    EnvironmentProfile Profile;
    void Validate() const;
    bool operator==(const EnvironmentDefinition &other) const;
};
std::string EnvironmentIntent(const EnvironmentDefinition &value);
std::string EnvironmentCacheIdentity(const std::string &source, const std::string &intent,
    const std::string &producer, const std::string &lut, const std::string &preparation);
EnvironmentDefinition ReadEnvironmentText(const std::string &bytes);
std::string WriteEnvironmentText(const EnvironmentDefinition &value);
EnvironmentDefinition ReadEnvironment(const std::filesystem::path &path);
// Portable IEEE754 little-endian artifacts. Each mip stores +X,-X,+Y,-Y,+Z,-Z,
// then rows in the documented cube t direction. LUT rows increase roughness.
inline constexpr size_t LightingArtifactByteLimit = 32 * 1024 * 1024 + 72;
enum class LightingImageKind : uint32_t
{
    Background = 1,
    Irradiance = 2,
    Specular = 3,
    BRDF = 4
};
struct LightingImage
{
    uint32_t EncodingVersion = 1; // v2 specular chains end at a 4x4 angular floor.
    LightingImageKind Kind = LightingImageKind::Background;
    uint32_t Side = 0;
    std::vector<std::vector<float>> Mips;
    uint32_t Channels() const
    {
        return Kind == LightingImageKind::BRDF ? 2 : 3;
    }
    uint32_t Faces() const
    {
        return Kind == LightingImageKind::BRDF ? 1 : 6;
    }
    void Validate() const;
    size_t Bytes() const;
};
std::string EncodeLightingImage(const LightingImage &image);
LightingImage DecodeLightingImage(const std::string &bytes);
struct PreparedEnvironment
{
    std::string Identity, IntentHash, SourceHash, Producer, LUTIdentity, Artifacts, PreparationRecipe;
    LightingImage Background, Irradiance, Specular, BRDF;
    std::vector<std::filesystem::path> Dependencies;
    uint64_t Generation = 0;
    double Seconds = 0;
    uint64_t PeakBytes = 0;
};
// Does not open producer inputs. Stale intent or invalid artifacts reject a new
// candidate; ProjectAssets retains its previous accepted immutable version.
void ValidateEnvironmentAcceptanceSchema(const std::string &acceptedBytes);
PreparedEnvironment ReadPreparedEnvironmentCandidate(const std::filesystem::path &root,
                                                     const std::filesystem::path &reference,
                                                     const std::string &acceptedBytes);
PreparedEnvironment ReadPreparedEnvironment(const std::filesystem::path &root,
                                            const std::filesystem::path &reference);
std::vector<std::filesystem::path> AuditEnvironment(const std::filesystem::path &root,
                                                    const std::filesystem::path &reference);
std::filesystem::path EnvironmentAcceptance(const std::filesystem::path &reference);
glm::dvec3 CubeDirection(unsigned face, double s, double t);
} // namespace Hazel
