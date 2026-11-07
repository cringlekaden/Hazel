#pragma once
#include "EnvironmentBake.h"
#include "Hazel/Core/FileSystem.h"
#include "Hazel/Project/ProjectSerializer.h"
#include "Hazel/Scene/Entity.h"
#include "Hazel/Scene/SceneSerializer.h"
#include <cmath>
// Maintainer-only source recipes; never invoked during native project creation.
static std::string DiagnosticHDR(unsigned width, unsigned height, bool axes)
{
    std::string out =
        "#?RADIANCE\n# Hazel diagnostic IBL_v1; linear Rec.709 D65\nFORMAT=32-bit_rle_rgbe\n\n-Y " +
        std::to_string(height) + " +X " + std::to_string(width) + "\n";
    const double pi = std::acos(-1.);
    for (unsigned y = 0; y < height; ++y)
        for (unsigned x = 0; x < width; ++x)
        {
            double theta = pi * (y + .5) / height, phi = 2 * pi * ((x + .5) / width - .5);
            glm::dvec3 rgb(.25);
            if (axes)
                rgb = glm::dvec3(1) + glm::dvec3(std::cos(phi) * std::sin(theta), std::cos(theta),
                                                 std::sin(phi) * std::sin(theta));
            int exponent = 0;
            double maximum = std::max(rgb.r, std::max(rgb.g, rgb.b));
            if (maximum < 1e-32)
                out.append(4, '\0');
            else
            {
                double scale = std::frexp(maximum, &exponent) * 256 / maximum;
                for (int c = 0; c < 3; ++c)
                    out += char(uint8_t(rgb[c] * scale));
                out += char(exponent + 128);
            }
        }
    return out;
}
static void GenerateEnvironmentStarter(const std::filesystem::path &root)
{
    using namespace Hazel;
    const std::filesystem::path folder = "Native/Environment_v1";
    if (std::filesystem::exists(root / folder))
        throw std::runtime_error("Native environment starter already exists");
    std::filesystem::create_directories(root / folder);
    auto source = DiagnosticHDR(16, 8, false);
    FileSystem::WriteNewFile(root / folder / "Neutral.hdr", source);
    EnvironmentDefinition d;
    d.Name = "Neutral constant radiance";
    d.Image = folder / "Neutral.hdr";
    d.Profile = {16, 16, 8, 1024};
    d.Notice = "Original Hazel diagnostic source; Apache-2.0 (Hazel LICENSE). Exact RGB .25; no "
               "external image.";
    auto ref = folder / "Neutral.henv";
    FileSystem::WriteNewFile(root / ref, WriteEnvironmentText(d));
    EnvironmentBake::PrepareImage(root, ref);
    FileSystem::WriteNewFile(root / folder / "NOTICE.txt",
                             d.Notice + "\nNative creation copies accepted portable data. No "
                                        "baker, parser, GPU or SDK needed.\n");
}
static void AddEnvironmentRenderingReference(const std::filesystem::path &projectFolder,
                                             const std::filesystem::path &photo)
{
    using namespace Hazel;
    auto project = Project::LoadCandidate(projectFolder / "RenderingReference.hproj");
    if (!project)
        throw std::runtime_error("Missing reference project");
    auto root = project->GetAssetRoot();
    const std::filesystem::path folder = "Reference/IBL_v1";
    if (std::filesystem::exists(root / folder))
        throw std::runtime_error("IBL_v1 already exists; do not rewrite frozen conditions");
    auto photoBytes = ReadAssetBytes(photo, 64 * 1024 * 1024);
    if (AssetHash(photoBytes) != "e7cfda5f4e98e623db12b8bfd0184e048488e4855d9c83e2751fb44a32e80c45")
        throw std::runtime_error(
            "Photographic HDR bytes do not match admitted Studio Small 09 1k source");
    (void)DecodeRadianceHDR(photoBytes);
    std::filesystem::create_directories(root / folder / "Sources");
    for (auto name : {"Constant", "AsymmetricAxes", "Photo"})
    {
        EnvironmentDefinition d;
        d.Name = name;
        d.Image = folder / "Sources" / (std::string(name) + ".hdr");
        d.Notice =
            std::string(name) == "Photo"
                ? "Studio Small 09, Sergej Majboroda / Poly Haven, CC0-1.0. Original unchanged 1k "
                  "HDR. https://polyhaven.com/a/studio_small_09 ; https://polyhaven.com/license ; "
                  "https://dl.polyhaven.org/file/ph-assets/HDRIs/hdr/1k/studio_small_09_1k.hdr ; "
                  "SHA256 e7cfda5f4e98e623db12b8bfd0184e048488e4855d9c83e2751fb44a32e80c45"
                : "Original Hazel IBL_v1 diagnostic source, Apache-2.0 (Hazel LICENSE). Constant "
                  "RGB .25; axes RGB=1+(x,y,z), right handed +Y up. RGBE quantization is recorded "
                  "by the actual source hash.";
        FileSystem::WriteNewFile(
            root / d.Image, std::string(name) == "Photo"
                                ? photoBytes
                                : DiagnosticHDR(256, 128, std::string(name) == "AsymmetricAxes"));
        auto ref = folder / (std::string(name) + ".henv");
        FileSystem::WriteNewFile(root / ref, WriteEnvironmentText(d));
        EnvironmentBake::PrepareImage(root, ref);
    }
    YAML::Node conditions;
    conditions["Version"] = 1;
    conditions["Recipe"] = IBLRecipe;
    conditions["LogicalExtent"] = std::vector<int>{1280, 720};
    conditions["PixelExtent"] = std::vector<int>{1280, 720};
    conditions["Exposure"] = 0;
    conditions["ToneMapping"] = "Neutral";
    conditions["Clock"] = 0;
    conditions["Seed"] = 0;
    conditions["Camera"] = 1;
    conditions["Scope"] = "IBL_v1; additive to frozen Core_v1, Import_v1, Direct_v1";
    for (auto name : {"DirectOnly", "IBLOnly", "Combined", "Photo", "AxesYaw90"})
    {
        auto scene = CreateRef<Scene>();
        scene->SetAssets(project->GetAssets());
        SceneSerializer codec(scene, root, true, project->GetAssets());
        if (!codec.DeserializeText(
                FileDocument::Read(root / "Scenes/Direct_v1-PerspectivePBR.hazel"), false,
                ResourceLoading::MetadataOnly))
            throw std::runtime_error(codec.Report().Error);
        scene->SetName(std::string("IBL_v1 ") + name);
        for (auto handle : scene->GetAllEntitiesWith<TextComponent>())
        {
            Entity text(handle, scene.get());
            auto &label = text.GetComponent<TextComponent>().TextString;
            if (label.find("no ambient or IBL") != std::string::npos)
                label = std::string("IBL_v1 / ") + name + " / fixed scene exposure 0";
            else if (label.rfind("Direct_v1", 0) == 0)
                label.replace(0, 9, "IBL_v1");
        }
        auto &instance = scene->Presentation().Environment;
        std::string which = std::string(name) == "Photo" ? "Photo"
                            : std::string(name) == "Combined" || std::string(name) == "AxesYaw90"
                                ? "AsymmetricAxes"
                                : "Constant";
        instance.Asset = folder / (which + ".henv");
        instance.Gain = 1;
        instance.Yaw = std::string(name) == "AxesYaw90" ? glm::radians(90.f) : 0;
        instance.Lighting = std::string(name) != "DirectOnly";
        instance.Background = instance.Lighting;
        for (auto h : scene->GetAllEntitiesWith<DirectionalLightComponent>())
        {
            Entity e(h, scene.get());
            e.GetComponent<DirectionalLightComponent>().Enabled =
                std::string(name) == "DirectOnly" || std::string(name) == "Combined";
        }
        auto ref = std::filesystem::path("Scenes") / (std::string("IBL_v1-") + name + ".hazel");
        FileSystem::WriteNewFile(root / ref, SceneSerializer(scene).SerializeText());
        auto config = project->GetConfig();
        config.StartScene = ref;
        auto selector = CreateRef<Project>();
        selector->GetConfig() = config;
        FileSystem::WriteNewFile(projectFolder /
                                     (std::string("RenderingReference-IBL-") + name + ".hproj"),
                                 ProjectSerializer(selector).SerializeText());
        auto c = conditions["Cases"][name];
        c["Scene"] = ref.generic_u8string();
        c["SceneHash"] = AssetHash(FileDocument::Read(root / ref));
        c["Environment"] = instance.Asset.generic_u8string();
        c["Yaw"] = instance.Yaw;
        c["Gain"] = instance.Gain;
        c["Lighting"] = instance.Lighting;
        c["Background"] = instance.Background;
        c["SunEnabled"] = std::string(name) == "DirectOnly" || std::string(name) == "Combined";
    }
    FileSystem::WriteNewFile(
        root / folder / "NOTICE.txt",
        "IBL_v1 is numerical/reference content, not a showcase. Diagnostic HDRs are original Hazel "
        "Apache-2.0.\nPhoto: Studio Small 09 by Sergej Majboroda, Poly Haven; CC0-1.0.\nSource: "
        "https://polyhaven.com/a/studio_small_09 ; license: "
        "https://polyhaven.com/license\nOriginal 1k HDR: "
        "https://dl.polyhaven.org/file/ph-assets/HDRIs/hdr/1k/studio_small_09_1k.hdr\nOriginal "
        "SHA256: e7cfda5f4e98e623db12b8bfd0184e048488e4855d9c83e2751fb44a32e80c45\nNo source "
        "pixels were edited. Prepared cubes/LUT use Hazel's recorded IBL_v1 recipe.\n");
    for (auto &entry : std::filesystem::recursive_directory_iterator(root / folder))
        if (entry.is_regular_file() && entry.path().extension() != ".lock")
            conditions["Files"][entry.path().lexically_relative(root).generic_u8string()] =
                AssetHash(ReadAssetBytes(entry.path(), LightingArtifactByteLimit));
    for (auto &entry : std::filesystem::recursive_directory_iterator(root / "Prepared/BRDF"))
        if (entry.is_regular_file())
            conditions["Files"][entry.path().lexically_relative(root).generic_u8string()] =
                AssetHash(ReadAssetBytes(entry.path(), LightingArtifactByteLimit));
    YAML::Emitter out;
    out.SetFloatPrecision(9);
    out << conditions;
    FileSystem::WriteNewFile(root / folder / "conditions.yaml", std::string(out.c_str()) + "\n");
}
