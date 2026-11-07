#include "EnvironmentBake.h"
#include "Hazel/Core/BuildIdentity.h"
#include "Hazel/Core/FileDocument.h"
#include "Hazel/Core/FileLease.h"
#include "Hazel/Core/FileSystem.h"
#include "Hazel/Core/OperationProgress.h"
#include "Hazel/Core/UUID.h"
#include "Hazel/Project/Project.h"
#include "ProducerMetrics.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
namespace Hazel::EnvironmentBake
{
namespace
{
constexpr double Pi = 3.1415926535897932384626433832795;
constexpr unsigned LUTSide = 128, LUTSamples = 4096;
void Check(const Cancel &c)
{
    if (c && c())
        throw std::runtime_error(
            "Environment preparation cancelled; last accepted generation retained");
}
double Radical(uint32_t b)
{
    b = (b << 16) | (b >> 16);
    b = ((b & 0x55555555) << 1) | ((b & 0xaaaaaaaa) >> 1);
    b = ((b & 0x33333333) << 2) | ((b & 0xcccccccc) >> 2);
    b = ((b & 0x0f0f0f0f) << 4) | ((b & 0xf0f0f0f0) >> 4);
    b = ((b & 0x00ff00ff) << 8) | ((b & 0xff00ff00) >> 8);
    return double(b) * 2.3283064365386962890625e-10;
}
glm::dvec3 Sample(const RadianceImage &in, glm::dvec3 d)
{
    double u = .5 + std::atan2(d.z, d.x) / (2 * Pi), v = std::acos(std::clamp(d.y, -1., 1.)) / Pi;
    double x = u * in.Width - .5, y = v * in.Height - .5;
    int ix = int(std::floor(x)), iy = int(std::floor(y));
    double fx = x - ix, fy = y - iy;
    auto at = [&](int xx, int yy) {
        xx = (xx % int(in.Width) + int(in.Width)) % int(in.Width);
        yy = std::clamp(yy, 0, int(in.Height) - 1);
        size_t p = (size_t(yy) * in.Width + xx) * 3;
        return glm::dvec3(in.RGB[p], in.RGB[p + 1], in.RGB[p + 2]);
    };
    return glm::mix(glm::mix(at(ix, iy), at(ix + 1, iy), fx),
                    glm::mix(at(ix, iy + 1), at(ix + 1, iy + 1), fx), fy);
}
glm::dvec3 ToWorld(glm::dvec3 local, glm::dvec3 N)
{
    auto up = std::abs(N.z) < .999 ? glm::dvec3(0, 0, 1) : glm::dvec3(1, 0, 0);
    auto T = glm::normalize(glm::cross(up, N));
    return T * local.x + glm::cross(N, T) * local.y + N * local.z;
}
std::vector<glm::dvec3> Samples(unsigned count, double rough, bool diffuse)
{
    std::vector<glm::dvec3> out;
    out.reserve(count);
    double a = std::pow(std::max(rough, .045), 2), a2 = a * a;
    for (unsigned i = 0; i < count; ++i)
    {
        double phi = 2 * Pi * double(i) / count, u = Radical(i);
        double z = diffuse ? std::sqrt(1 - u) : std::sqrt((1 - u) / (1 + (a2 - 1) * u));
        double radius = std::sqrt(std::max(0., 1 - z * z));
        out.emplace_back(radius * std::cos(phi), radius * std::sin(phi), z);
    }
    return out;
}
void Store(std::vector<float> &out, glm::dvec3 v)
{
    for (int c = 0; c < 3; ++c)
        out.push_back(float(v[c]));
}
LightingImage Cube(const RadianceImage &in, LightingImageKind kind, unsigned side, unsigned count,
                   const Cancel &cancel)
{
    LightingImage result;
    result.Kind = kind;
    result.Side = side;
    unsigned levels = 1;
    if (kind == LightingImageKind::Specular)
        for (auto s = side; s > 1; s /= 2)
            ++levels;
    for (unsigned mip = 0; mip < levels; ++mip)
    {
        auto &out = result.Mips.emplace_back();
        out.reserve(size_t(side) * side * 18);
        double rough = levels > 1 ? double(mip) / (levels - 1) : 0;
        auto sequence = Samples(count, rough, kind == LightingImageKind::Irradiance);
        for (unsigned f = 0; f < 6; ++f)
            for (unsigned y = 0; y < side; ++y)
            {
                Check(cancel);
                OperationProgress::Report(kind == LightingImageKind::Irradiance
                                              ? "Diffuse irradiance"
                                          : kind == LightingImageKind::Specular
                                              ? "GGX specular mip " + std::to_string(mip)
                                              : "Background cube",
                                          f * side + y, 6 * side);
                for (unsigned x = 0; x < side; ++x)
                {
                    auto N = CubeDirection(f, (x + .5) / side, (y + .5) / side);
                    glm::dvec3 sum(0);
                    double weight = 0;
                    if (kind == LightingImageKind::Background ||
                        (kind == LightingImageKind::Specular && mip == 0))
                        sum = Sample(in, N);
                    else
                        for (auto local : sequence)
                        {
                            auto d = ToWorld(local, N);
                            if (kind == LightingImageKind::Irradiance)
                                sum += Sample(in, d) * (Pi / count);
                            else
                            {
                                auto L = 2 * glm::dot(N, d) * d - N;
                                double nl = glm::dot(N, L);
                                if (nl > 0)
                                {
                                    sum += Sample(in, L) * nl;
                                    weight += nl;
                                }
                            }
                        }
                    if (kind == LightingImageKind::Specular && mip > 0)
                    {
                        if (weight <= 0)
                            throw std::runtime_error("GGX prefilter has no valid samples");
                        sum /= weight;
                    }
                    Store(out, sum);
                }
            }
        side = std::max(1u, side / 2);
    }
    result.Validate();
    return result;
}
void Put(const std::filesystem::path &p, const std::string &bytes)
{
    std::filesystem::create_directories(p.parent_path());
    FileSystem::WriteNewFile(p, bytes);
}
struct Stage
{
    std::filesystem::path Path;
    ~Stage()
    {
        std::error_code ec;
        std::filesystem::remove_all(Path, ec);
    }
};
} // namespace
std::string Producer()
{
    auto &b = BuildIdentity::Current();
#ifdef _MSC_VER
    auto compiler = std::string("MSVC-") + std::to_string(_MSC_VER);
#else
    auto compiler = std::string(__VERSION__);
#endif
    return b.System + "/" + b.Architecture + "/" + b.Configuration + "/" + compiler +
           "/IEEE754-precise-FP64-v1";
}
std::string LUTKey()
{
    return AssetHash(std::string(IBLRecipe) + "/LUT128-RG32F-Hammersley4096-v1/" + Producer());
}
LightingSet Prepare(const RadianceImage &source, const EnvironmentProfile &p, const Cancel &cancel)
{
    source.Validate();
    p.Validate();
    Check(cancel);
    LightingSet set;
    set.Background = Cube(source, LightingImageKind::Background, p.Background, p.Samples, cancel);
    set.Irradiance = Cube(source, LightingImageKind::Irradiance, p.Irradiance, p.Samples, cancel);
    set.Specular = Cube(source, LightingImageKind::Specular, p.Specular, p.Samples, cancel);
    return set;
}
LightingImage IntegrateBRDF(const Cancel &cancel)
{
    LightingImage out;
    out.Kind = LightingImageKind::BRDF;
    out.Side = LUTSide;
    auto &pixels = out.Mips.emplace_back();
    pixels.reserve(LUTSide * LUTSide * 2);
    for (unsigned y = 0; y < LUTSide; ++y)
    {
        Check(cancel);
        OperationProgress::Report("Integrating shared BRDF LUT", y, LUTSide);
        double rough = std::max((y + .5) / LUTSide, .045), a2 = std::pow(rough, 4);
        auto sequence = Samples(LUTSamples, rough, false);
        for (unsigned x = 0; x < LUTSide; ++x)
        {
            double nv = (x + .5) / LUTSide;
            double vx = std::sqrt(1 - nv * nv);
            double A = 0, B = 0;
            for (auto H : sequence)
            {
                double vh = std::max(vx * H.x + nv * H.z, 0.);
                double nl = 2 * vh * H.z - nv;
                if (nl <= 0 || vh <= 0)
                    continue;
                double visibility = .5 / (nl * std::sqrt(nv * nv * (1 - a2) + a2) +
                                          nv * std::sqrt(nl * nl * (1 - a2) + a2));
                double factor = 4 * nl * visibility * vh / H.z;
                double f = std::pow(1 - vh, 5);
                A += (1 - f) * factor;
                B += f * factor;
            }
            pixels.push_back(float(A / LUTSamples));
            pixels.push_back(float(B / LUTSamples));
        }
    }
    out.Validate();
    return out;
}
void PrepareImage(const std::filesystem::path &assets, const std::filesystem::path &reference,
                  const Cancel &cancel)
{
    const auto root = std::filesystem::weakly_canonical(assets);
    AssetPath(reference, ".henv");
    auto path = Project::ResolveOwnedAsset(root, reference);
    auto lock = path;
    lock += ".prepare.lock";
    auto lease = FileLease::Try(lock);
    if (!lease)
        throw std::runtime_error("Environment preparation is already owned by another operation");
    FileDocument authored;
    authored.Open(path);
    auto d = ReadEnvironmentText(authored.Original());
    auto source = Project::ResolveOwnedAsset(root, d.Image);
    auto sourceBytes = ReadAssetBytes(source, 64 * 1024 * 1024);
    auto sourceHash = AssetHash(sourceBytes), intent = EnvironmentIntent(d), lutKey = LUTKey();
    auto identity = AssetHash(sourceHash + intent + Producer() + RadianceEvaluation +
                              "/RGB32F-Hammersley-normalized-v1/" + lutKey);
    auto acceptedRef = EnvironmentAcceptance(reference);
    auto acceptedPath = Project::ResolveOwnedAsset(root, acceptedRef);
    FileDocument accepted;
    bool replacing = std::filesystem::exists(acceptedPath);
    if (replacing){
        accepted.Open(acceptedPath);
        ValidateEnvironmentAcceptanceSchema(accepted.Original());
    }
    auto prepared = reference;
    prepared += ".prepared";
    auto parent = Project::ResolveOwnedAsset(root, prepared);
    std::filesystem::create_directories(parent);
    // A killed native job cannot run destructors. The lease proves no producer
    // is using these stages; remove only our flat, marker-owned stale paths.
    for (auto &item : std::filesystem::directory_iterator(parent))
    {
        if (item.is_symlink() || !item.is_directory() ||
            item.path().filename().generic_u8string().rfind(".stage-", 0) != 0)
            continue;
        auto marker = item.path() / "owner";
        if (!std::filesystem::is_regular_file(marker) || std::filesystem::is_symlink(marker))
            continue;
        if (ReadAssetBytes(marker, 128) != "Hazel.EnvironmentBake.v1\n")
            continue;
        bool owned = true;
        for (auto &file : std::filesystem::directory_iterator(item.path()))
        {
            auto name = file.path().filename();
            owned &= !file.is_symlink() && file.is_regular_file() &&
                     (name == "owner" || name == "Background.hlight" ||
                      name == "Irradiance.hlight" || name == "Specular.hlight");
        }
        if (owned)
            std::filesystem::remove_all(item.path());
    }
    Stage stage{parent / (".stage-" + AssetKey(UUID()))};
    if (!std::filesystem::create_directory(stage.Path))
        throw std::runtime_error("Cannot reserve environment staging directory");
    Put(stage.Path / "owner", "Hazel.EnvironmentBake.v1\n");
    auto start = std::chrono::steady_clock::now();
    Check(cancel);
    auto sourceImage = DecodeRadianceHDR(sourceBytes);
    auto set = Prepare(sourceImage, d.Profile, cancel);
    auto lutFolder = std::filesystem::path("Prepared/BRDF") / lutKey;
    auto lutRoot = Project::ResolveOwnedAsset(root, lutFolder);
    std::filesystem::create_directories(lutRoot);
    std::filesystem::path lutRef, lutPath;
    std::string lutBytes;
    std::vector<std::filesystem::path> cached;
    size_t scanned = 0;
    for (auto &file : std::filesystem::directory_iterator(lutRoot))
    {
        if (++scanned > 64 || cached.size() >= 16)
            throw std::runtime_error("Shared LUT recipe cache exceeds 16 artifacts; inspect and "
                                     "retain required accepted generations before preparing");
        if (!file.is_symlink() && file.is_regular_file() && file.path().extension() == ".hlight")
            cached.push_back(file.path());
    }
    std::sort(cached.begin(), cached.end());
    for (auto &file : cached)
        try
        {
            ReadAssetKey(file.stem().generic_u8string());
            auto bytes = ReadAssetBytes(file, LightingArtifactByteLimit);
            auto lut = DecodeLightingImage(bytes);
            if (lut.Kind != LightingImageKind::BRDF || lut.Side != LUTSide)
                throw std::runtime_error("Invalid shared LUT layout");
            lutBytes = std::move(bytes);
            lutPath = file;
            lutRef = file.lexically_relative(root);
            break;
        }
        catch (const std::exception &)
        {
            OperationProgress::Report(
                "Unusable shared LUT candidate retained; preparing a fresh immutable artifact");
        }
    if (lutBytes.empty())
    {
        lutBytes = EncodeLightingImage(IntegrateBRDF(cancel));
        lutRef = lutFolder / (AssetKey(UUID()) + ".hlight");
        lutPath = Project::ResolveOwnedAsset(root, lutRef);
    }
    const uint64_t generationID = UUID();
    auto generation = prepared / AssetKey(generationID);
    YAML::Node n;
    n["Version"] = 1;
    n["Recipe"] = IBLRecipe;
    n["Identity"] = identity;
    n["Generation"] = AssetKey(generationID);
    n["IntentHash"] = intent;
    n["SourceHash"] = sourceHash;
    n["Evaluation"] = RadianceEvaluation;
    n["Producer"] = Producer();
    n["LUTIdentity"] = lutKey;
    unsigned i = 0;
    for (auto image : {&set.Background, &set.Irradiance, &set.Specular})
    {
        const char *names[] = {"Background", "Irradiance", "Specular"};
        auto bytes = EncodeLightingImage(*image);
        auto name = std::string(names[i++]) + ".hlight";
        Put(stage.Path / name, bytes);
        auto output = n["Outputs"][names[i - 1]];
        output["Path"] = (generation / name).generic_u8string();
        output["Hash"] = AssetHash(bytes);
        (void)DecodeLightingImage(ReadAssetBytes(stage.Path / name, LightingArtifactByteLimit));
    }
    n["Outputs"]["BRDF"]["Path"] = lutRef.generic_u8string();
    n["Outputs"]["BRDF"]["Hash"] = AssetHash(lutBytes);
    auto seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    auto peak = PeakResidentBytes();
    if (peak > 512ull * 1024 * 1024)
        throw std::runtime_error(
            "Preparation exceeded 512 MiB measured process peak; accepted generation retained");
    n["Seconds"] = seconds;
    n["PeakBytes"] = peak;
    auto current = [&] {
        Check(cancel);
        authored.Check();
        if (ReadAssetBytes(source, 64 * 1024 * 1024) != sourceBytes)
            throw std::runtime_error(
                "HDR source changed during preparation; retry. Accepted generation retained");
        if (replacing)
            accepted.Check();
        else if (std::filesystem::exists(acceptedPath))
            throw std::runtime_error("Environment acceptance changed during preparation");
    };
    current();
    OperationProgress::Report("Publishing validated environment generation");
    if (!std::filesystem::exists(lutPath))
    {
        std::filesystem::create_directories(lutPath.parent_path());
        try
        {
            FileSystem::WriteFileAtomically(
                lutPath,
                [&](auto &o) { o.write(lutBytes.data(), std::streamsize(lutBytes.size())); },
                WriteMode::CreateNew);
        }
        catch (...)
        {
            if (!std::filesystem::exists(lutPath) ||
                ReadAssetBytes(lutPath, LightingArtifactByteLimit) != lutBytes)
                throw;
        }
    }
    else if (ReadAssetBytes(lutPath, LightingArtifactByteLimit) != lutBytes)
        throw std::runtime_error("Shared LUT changed during preparation");
    auto final = Project::ResolveOwnedAsset(root, generation);
    if (!std::filesystem::exists(final))
        FileSystem::PublishDirectoryNew(stage.Path, final);
    else
        for (auto item : n["Outputs"])
        {
            auto p = std::filesystem::u8path(item.second["Path"].as<std::string>());
            if (AssetHash(ReadAssetBytes(Project::ResolveOwnedAsset(root, p),
                                         LightingArtifactByteLimit)) !=
                item.second["Hash"].as<std::string>())
                throw std::runtime_error("Existing immutable lighting generation is corrupt; "
                                         "accepted descriptor retained");
        }
    current();
    auto serialize = [&] {
        YAML::Emitter out;
        out.SetDoublePrecision(17);
        out << n;
        return std::string(out.c_str()) + "\n";
    };
    // The immutable files are published but still unaccepted. Run the exact
    // native runtime validator before replacing the single acceptance point.
    (void)ReadPreparedEnvironmentCandidate(root, reference, serialize());
    current();
    seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    peak = PeakResidentBytes();
    if (peak > 512ull * 1024 * 1024)
        throw std::runtime_error(
            "Preparation validation exceeded 512 MiB process peak; accepted generation retained");
    n["Seconds"] = seconds;
    n["PeakBytes"] = peak;
    auto bytes = serialize();
    if (replacing)
        accepted.Save(bytes);
    else
        FileSystem::WriteFileAtomically(
            acceptedPath, [&](auto &o) { o << bytes; }, WriteMode::CreateNew);
    std::cout << "PREPARED " << reference.generic_u8string() << "\nIdentity " << identity
              << "\nLUT " << lutKey << "\nBake " << seconds << " s; peak resident " << peak
              << " bytes; RGB32F cubes, RG32F LUT; " << d.Profile.Samples << " convolution / "
              << LUTSamples << " LUT samples\n";
}
} // namespace Hazel::EnvironmentBake
