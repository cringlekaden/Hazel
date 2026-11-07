#include "hzpch.h"
#include "Environment.h"
#include "Hazel/Core/DocumentSchema.h"
#include "Hazel/Project/Project.h"
#include <cmath>
#include <cstring>
#include <limits>
#include <set>
#include <yaml-cpp/yaml.h>
namespace Hazel
{
namespace
{
void Need(bool yes, const char *why)
{
    if (!yes)
        throw std::runtime_error(why);
}
bool Power(uint32_t n)
{
    return n && !(n & (n - 1));
}
void U32(std::string &b, uint32_t n)
{
    for (unsigned i = 0; i < 4; ++i)
        b += char(n >> (8 * i));
}
uint32_t Get(std::string_view b, size_t &at)
{
    Need(at <= b.size() && b.size() - at >= 4, "Truncated lighting field");
    uint32_t n = 0;
    for (unsigned i = 0; i < 4; ++i)
        n |= uint32_t(uint8_t(b[at++])) << (8 * i);
    return n;
}
YAML::Node Parse(const std::string &bytes)
{
    Need(bytes.size() <= 65536, "Environment metadata exceeds 64 KiB");
    auto n = YAML::Load(bytes);
    DocumentSchema::Structure(n);
    return n;
}
YAML::Node ParseAcceptance(const std::string &bytes)
{
    auto n=Parse(bytes);
    DocumentSchema::Keys(n,
                         {"Version", "Recipe", "Generation", "Identity", "IntentHash", "SourceHash",
                          "Producer", "LUTIdentity", "Seconds", "PeakBytes", "Evaluation",
                          "Outputs", "PreparationRecipe"},
                         "accepted environment");
    auto version = n["Version"].as<unsigned>();
    Need((version == 1 || version == 2) && n["Recipe"].as<std::string>() == IBLRecipe,
         "Incompatible accepted lighting recipe; Prepare with matching producer");
    if (version == 2)
        Need(n["PreparationRecipe"].as<std::string>() == EnvironmentPreparationRecipe,
             "Unsupported environment preparation revision; use a matching producer");
    else
        Need(!n["PreparationRecipe"], "Version 1 acceptance cannot contain future preparation data");
    Need(n["Evaluation"].as<std::string>()==RadianceEvaluation,"Unsupported HDR source evaluation identity");
    auto outputs=n["Outputs"];
    DocumentSchema::Keys(outputs,{"Background","Irradiance","Specular","BRDF"},"lighting outputs");
    for(auto name:{"Background","Irradiance","Specular","BRDF"})DocumentSchema::Keys(outputs[name],{"Path","Hash"},"lighting output");
    return n;
}
} // namespace
void ValidateEnvironmentAcceptanceSchema(const std::string &bytes){(void)ParseAcceptance(bytes);}
void RadianceImage::Validate() const
{
    Need(Width >= 2 && Height >= 1 && Width <= 8192 && Height <= 4096 &&
             uint64_t(Width) * Height <= 8388608 && RGB.size() == size_t(Width) * Height * 3,
         "HDR dimensions/data exceed 8M pixels / 8192x4096");
    for (float f : RGB)
        Need(std::isfinite(f) && f >= 0 && f <= 1e10f,
             "HDR radiance must be finite, nonnegative and <=1e10 in linear Rec.709");
}
void EnvironmentProfile::Validate() const
{
    Need(Power(Background) && Background >= 4 && Background <= 512 && Power(Specular) &&
             Specular >= 4 && Specular <= 512 && Power(Irradiance) && Irradiance >= 4 &&
             Irradiance <= 64 && Power(Samples) && Samples >= 64 && Samples <= 65536,
         "Preparation requires power-of-two background 4..512, specular 4..512, irradiance 4..64 "
         "and samples 64..65536");
}
void EnvironmentDefinition::Validate() const
{
    AssetName(Name);
    AssetNotice(Notice);
    AssetPath(Image, ".hdr");
    Profile.Validate();
}
bool EnvironmentDefinition::operator==(const EnvironmentDefinition &b) const
{
    return Name == b.Name && Notice == b.Notice && Image == b.Image &&
           Profile == b.Profile;
}
EnvironmentDefinition ReadEnvironmentText(const std::string &bytes)
{
    auto root = Parse(bytes);
    DocumentSchema::Keys(root, {"Environment"}, "root");
    auto n = root["Environment"];
    DocumentSchema::Keys(n, {"Version", "Name", "Source", "Preparation", "Notice"}, "Environment");
    Need(n["Version"].as<unsigned>() == 1, "Unsupported Environment.Version");
    EnvironmentDefinition d;
    d.Name = n["Name"].as<std::string>();
    d.Notice = n["Notice"].as<std::string>();
    auto s = n["Source"];
    DocumentSchema::Keys(s, {"Kind", "Image", "Interpretation"}, "environment source");
    Need(s["Kind"].as<std::string>() == "RadianceHDR" &&
             s["Interpretation"].as<std::string>() == "LinearRec709D65TopDown",
         "Only linear Rec.709/D65 top-down Radiance HDR source interpretation is supported");
    d.Image = std::filesystem::u8path(s["Image"].as<std::string>());
    auto p = n["Preparation"];
    DocumentSchema::Keys(p, {"Recipe", "Background", "Specular", "Irradiance", "Samples"},
                         "Preparation");
    Need(p["Recipe"].as<std::string>() == IBLRecipe,
         "Unsupported IBL preparation recipe; use a matching producer");
    d.Profile = {p["Background"].as<uint32_t>(), p["Specular"].as<uint32_t>(),
                 p["Irradiance"].as<uint32_t>(), p["Samples"].as<uint32_t>()};
    d.Validate();
    return d;
}
std::string WriteEnvironmentText(const EnvironmentDefinition &d)
{
    d.Validate();
    YAML::Node n;
    n["Version"] = 1;
    n["Name"] = d.Name;
    n["Notice"] = d.Notice;
    n["Source"]["Kind"] = "RadianceHDR";
    n["Source"]["Image"] = d.Image.generic_u8string();
    n["Source"]["Interpretation"] = "LinearRec709D65TopDown";
    auto p = n["Preparation"];
    p["Recipe"] = IBLRecipe;
    p["Background"] = d.Profile.Background;
    p["Specular"] = d.Profile.Specular;
    p["Irradiance"] = d.Profile.Irradiance;
    p["Samples"] = d.Profile.Samples;
    YAML::Node root;
    root["Environment"] = n;
    YAML::Emitter out;
    out.SetFloatPrecision(9);
    out << root;
    auto bytes = std::string(out.c_str()) + "\n";
    Need(bytes.size() <= 65536, "Environment document exceeds 64 KiB; shorten attribution/name");
    return bytes;
}
std::string EnvironmentIntent(const EnvironmentDefinition &value)
{
    auto semantic = value;
    semantic.Name = "Environment source and preparation";
    semantic.Notice.clear();
    return AssetHash(WriteEnvironmentText(semantic));
}
std::string EnvironmentCacheIdentity(const std::string &source, const std::string &intent,
    const std::string &producer, const std::string &lut, const std::string &preparation)
{
    // Decode the frozen v1 identity; the old producer/renderer is not retained.
    const auto algorithm = preparation == EnvironmentPreparationRecipe
        ? std::string("/RGB32F/") + preparation + "/"
        : std::string("/RGB32F-Hammersley-normalized-v1/");
    Need(preparation == EnvironmentPreparationRecipe || preparation == "Hazel.EnvPrepare_v1.PointHammersley.FullTail1",
         "Unsupported preparation identity");
    return AssetHash(source + intent + producer + RadianceEvaluation + algorithm + lut);
}
EnvironmentDefinition ReadEnvironment(const std::filesystem::path &path)
{
    return ReadEnvironmentText(ReadAssetBytes(path, 65536));
}
size_t LightingImage::Bytes() const
{
    size_t n = 0;
    for (auto &m : Mips)
        n += m.size() * 4;
    return n;
}
void LightingImage::Validate() const
{
    Need(unsigned(Kind) >= 1 && unsigned(Kind) <= 4 && Power(Side) && Side <= 512,
         "Invalid prepared lighting kind/size");
    uint32_t count = 1;
    Need(EncodingVersion == 1 || EncodingVersion == 2, "Unsupported lighting encoding version");
    if (EncodingVersion == 2)
        Need(Kind == LightingImageKind::Specular && Side >= 4, "Version 2 requires specular tail-four data");
    if (Kind == LightingImageKind::Specular)
        for (auto n = Side; n > (EncodingVersion == 2 ? 4u : 1u); n /= 2)
            ++count;
    Need(Mips.size() == count, "Lighting artifact requires its complete explicit mip chain");
    Need(Bytes() <= 32 * 1024 * 1024, "Prepared lighting exceeds 32 MiB per artifact");
    if (Kind == LightingImageKind::BRDF)
        for (size_t i = 0; i < Mips[0].size(); i += 2)
            Need(Mips[0][i] + Mips[0][i + 1] <= 1.02f,
                 "BRDF LUT exceeds the single-scattering furnace range");
    uint32_t side = Side;
    for (auto &m : Mips)
    {
        Need(m.size() == size_t(side) * side * Faces() * Channels(),
             "Lighting mip/face/row length mismatch");
        for (float f : m)
            Need(std::isfinite(f) && f >= 0 && f <= (Kind == LightingImageKind::BRDF ? 4.f : 4e10f),
                 "Prepared lighting contains nonfinite, negative or unrepresentable radiance/data");
        side = std::max(1u, side / 2);
    }
}
std::string EncodeLightingImage(const LightingImage &d)
{
    d.Validate();
    std::string payload;
    payload.reserve(d.Bytes());
    for (auto &m : d.Mips)
        for (float f : m)
        {
            uint32_t n;
            std::memcpy(&n, &f, 4);
            U32(payload, n);
        }
    std::string b = "HZLIGHT1";
    U32(b, d.EncodingVersion);
    U32(b, unsigned(d.Kind));
    U32(b, d.Side);
    U32(b, uint32_t(d.Mips.size()));
    U32(b, d.Channels());
    U32(b, d.Faces());
    U32(b, d.Kind == LightingImageKind::BRDF
               ? 2
               : 1); // NoV/roughness rows or canonical cube faces/rows
    U32(b, uint32_t(payload.size()));
    auto hash = AssetSHA256(payload.data(), payload.size());
    b.append(reinterpret_cast<const char *>(hash.data()), hash.size());
    b += payload;
    return b;
}
LightingImage DecodeLightingImage(const std::string &bytes)
{
    Need(bytes.size() >= 72 && bytes.size() <= LightingArtifactByteLimit &&
             bytes.substr(0, 8) == "HZLIGHT1",
         "Invalid/oversized lighting artifact");
    size_t at = 8;
    auto version = Get(bytes, at);
    Need(version == 1 || version == 2, "Unsupported lighting artifact version");
    LightingImage d;
    d.EncodingVersion = version;
    d.Kind = LightingImageKind(Get(bytes, at));
    d.Side = Get(bytes, at);
    auto levels = Get(bytes, at), channels = Get(bytes, at), faces = Get(bytes, at),
         orientation = Get(bytes, at), length = Get(bytes, at);
    Need(unsigned(d.Kind) >= 1 && unsigned(d.Kind) <= 4 && Power(d.Side) && d.Side <= 512 &&
             channels == d.Channels() && faces == d.Faces() &&
             orientation == (d.Kind == LightingImageKind::BRDF ? 2u : 1u),
         "Invalid lighting layout");
    unsigned expected = 1;
    if (d.Kind == LightingImageKind::Specular)
        for (auto s = d.Side; s > (version == 2 ? 4u : 1u); s /= 2)
            ++expected;
    Need(levels == expected, "Incomplete/extra lighting mips");
    Need(length == bytes.size() - 72, "Lighting payload length mismatch");
    auto digest = AssetSHA256(bytes.data() + 72, length);
    Need(std::memcmp(digest.data(), bytes.data() + 40, 32) == 0,
         "Lighting payload checksum mismatch");
    at = 72;
    auto side = d.Side;
    for (unsigned mip = 0; mip < levels; ++mip)
    {
        size_t count = size_t(side) * side * channels * faces;
        Need(count <= (bytes.size() - at) / 4, "Truncated lighting mip");
        auto &out = d.Mips.emplace_back();
        out.reserve(count);
        for (size_t i = 0; i < count; ++i)
        {
            auto n = Get(bytes, at);
            float f;
            std::memcpy(&f, &n, 4);
            out.push_back(f);
        }
        side = std::max(1u, side / 2);
    }
    Need(at == bytes.size(), "Trailing lighting bytes");
    d.Validate();
    return d;
}
std::filesystem::path EnvironmentAcceptance(const std::filesystem::path &r)
{
    AssetPath(r, ".henv");
    auto p = r;
    p += ".accepted";
    return p;
}
PreparedEnvironment ReadPreparedEnvironmentCandidate(const std::filesystem::path &root,
                                                     const std::filesystem::path &reference,
                                                     const std::string &acceptedBytes)
{
    auto d = ReadEnvironment(Project::ResolveOwnedAsset(root, reference));
    auto accepted = EnvironmentAcceptance(reference);
    auto n=ParseAcceptance(acceptedBytes);
    PreparedEnvironment e;
    e.Generation = ReadAssetKey(n["Generation"].as<std::string>());
    e.Identity = n["Identity"].as<std::string>();
    e.IntentHash = n["IntentHash"].as<std::string>();
    e.SourceHash = n["SourceHash"].as<std::string>();
    e.LUTIdentity = n["LUTIdentity"].as<std::string>();
    e.Producer = n["Producer"].as<std::string>();
    e.PreparationRecipe = n["Version"].as<unsigned>() == 2 ? n["PreparationRecipe"].as<std::string>()
        : "Hazel.EnvPrepare_v1.PointHammersley.FullTail1";

    ReadDigest(e.Identity);
    ReadDigest(e.IntentHash);
    ReadDigest(e.SourceHash);
    ReadDigest(e.LUTIdentity);
    AssetName(e.Producer);
    Need(e.IntentHash == EnvironmentIntent(d),
         "Environment preparation is stale: source/settings intent changed. Prepare before Play; "
         "previous accepted viewing resources retained");
    e.Seconds = n["Seconds"].as<double>();
    e.PeakBytes = n["PeakBytes"].as<uint64_t>();
    Need(std::isfinite(e.Seconds) && e.Seconds >= 0 && e.PeakBytes <= 512ull * 1024 * 1024,
         "Invalid environment cost metadata");
    Need(e.LUTIdentity ==
             AssetHash(std::string(IBLRecipe) + "/LUT128-RG32F-Hammersley4096-v1/" + e.Producer),
         "LUT identity does not match the BRDF recipe/producer");
    Need(e.Identity == EnvironmentCacheIdentity(e.SourceHash,e.IntentHash,e.Producer,e.LUTIdentity,e.PreparationRecipe),
         "Environment identity does not match source, preparation and producer");
    e.Dependencies = {reference, accepted};
    auto outputs = n["Outputs"];

    unsigned i = 0;
    for (auto target : {&e.Background, &e.Irradiance, &e.Specular, &e.BRDF})
    {
        const char *names[] = {"Background", "Irradiance", "Specular", "BRDF"};
        auto item = outputs[names[i]];

        auto p = std::filesystem::u8path(item["Path"].as<std::string>());
        AssetPath(p, ".hlight");
        auto expected = i == 3
                            ? p
                            : std::filesystem::u8path(reference.generic_u8string() + ".prepared") /
                                  AssetKey(e.Generation) / (std::string(names[i]) + ".hlight");
        if (i == 3)
        {
            Need(p.parent_path() == std::filesystem::path("Prepared/BRDF") / e.LUTIdentity,
                 "LUT escapes its versioned recipe cache");
            ReadAssetKey(p.stem().generic_u8string());
        }
        Need(p == expected,
             "Lighting output escapes its accepted immutable generation/recipe path");
        auto h = item["Hash"].as<std::string>();
        ReadDigest(h);
        e.Artifacts += h;
        auto bytes = ReadAssetBytes(Project::ResolveOwnedAsset(root, p), LightingArtifactByteLimit);
        Need(AssetHash(bytes) == h, "Accepted lighting output hash mismatch; restore or Prepare");
        *target = DecodeLightingImage(bytes);
        Need(unsigned(target->Kind) == ++i, "Incorrect accepted lighting image kind");
        e.Dependencies.push_back(p);
    }
    e.Artifacts = AssetHash(e.Artifacts);
    Need(e.Background.Side == d.Profile.Background && e.Irradiance.Side == d.Profile.Irradiance &&
             e.Specular.Side == d.Profile.Specular && e.BRDF.Side == 128,
         "Prepared quality does not match authored preparation");
    return e;
}
PreparedEnvironment ReadPreparedEnvironment(const std::filesystem::path &root,
                                            const std::filesystem::path &reference)
{
    return ReadPreparedEnvironmentCandidate(
        root, reference,
        ReadAssetBytes(Project::ResolveOwnedAsset(root, EnvironmentAcceptance(reference)), 65536));
}
std::vector<std::filesystem::path> AuditEnvironment(const std::filesystem::path &r,
                                                    const std::filesystem::path &p)
{
    return ReadPreparedEnvironment(r, p).Dependencies;
}
glm::dvec3 CubeDirection(unsigned f, double s, double t)
{
    double a = 2 * s - 1, b = 2 * t - 1;
    glm::dvec3 d;
    switch (f)
    {
    case 0:
        d = {1, -b, -a};
        break;
    case 1:
        d = {-1, -b, a};
        break;
    case 2:
        d = {a, 1, b};
        break;
    case 3:
        d = {a, -1, -b};
        break;
    case 4:
        d = {a, -b, 1};
        break;
    case 5:
        d = {-a, -b, -1};
        break;
    default:
        throw std::runtime_error("Invalid cube face");
    }
    return glm::normalize(d);
}
} // namespace Hazel
