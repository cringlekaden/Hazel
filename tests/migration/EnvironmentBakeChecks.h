#pragma once
#include "EnvironmentBake.h"
#include "Hazel/Assets/Environment.h"
#include "Hazel/Assets/EnvironmentDocument.h"
#include "Hazel/Core/FileSystem.h"
#include "Hazel/Project/Project.h"
#include "IBLReferenceMath.h"
#include <glm/gtc/constants.hpp>
// Independent checks use analytic integrals and uniform FP64 spherical
// quadrature, never the producer's low-discrepancy sequence or GGX helpers.
static void EnvironmentBakeChecks()
{
    using namespace Hazel;
    const double pi = std::acos(-1.);
    auto rejected = [](auto work) {
        bool bad = false;
        try
        {
            work();
        }
        catch (const std::exception &)
        {
            bad = true;
        }
        Check(bad, "Invalid environment candidate accepted");
    };
    RadianceImage constant;
    constant.Width = 16;
    constant.Height = 8;
    for (int i = 0; i < 128; ++i)
        constant.RGB.insert(constant.RGB.end(), {.125f, .5f, 2.f});
    EnvironmentProfile profile{16, 16, 8, 1024};
    auto set = EnvironmentBake::Prepare(constant, profile);
    for (auto image : {&set.Background, &set.Irradiance, &set.Specular})
    {
        for (auto &m : image->Mips)
            for (size_t i = 0; i < m.size(); ++i)
            {
                double expected =
                    constant.RGB[i % 3] * (image->Kind == LightingImageKind::Irradiance ? pi : 1.);
                Check(std::abs(m[i] - expected) < 1e-5,
                      "Constant irradiance piC / normalized specular conservation");
            }
        auto bytes = EncodeLightingImage(*image);
        Check(EncodeLightingImage(DecodeLightingImage(bytes)) == bytes,
              "Portable lighting round trip");
        bytes.back() ^= 1;
        rejected([&] { DecodeLightingImage(bytes); });
    }
    bool sawCancel = false;
    rejected([&] {
        EnvironmentBake::Prepare(constant, profile, [&] {
            sawCancel = true;
            return true;
        });
    });
    Check(sawCancel, "Preparation did not observe cancellation");
    auto invalid = constant;
    invalid.RGB[0] = std::numeric_limits<float>::infinity();
    rejected([&] { EnvironmentBake::Prepare(invalid, profile); });
    // Axis-linear radiance has exact irradiance pi*(C + 2/3*n). This exercises
    // every cube face/row and the equirectangular mapping without a photo oracle.
    RadianceImage axes;
    axes.Width = 256;
    axes.Height = 128;
    for (unsigned y = 0; y < axes.Height; ++y)
        for (unsigned x = 0; x < axes.Width; ++x)
        {
            double theta = pi * (y + .5) / axes.Height, phi = 2 * pi * ((x + .5) / axes.Width - .5);
            glm::dvec3 d(std::cos(phi) * std::sin(theta), std::cos(theta),
                         std::sin(phi) * std::sin(theta));
            for (int c = 0; c < 3; ++c)
                axes.RGB.push_back(float(1 + d[c]));
        }
    auto directional = EnvironmentBake::Prepare(axes, profile);
    for (unsigned face = 0; face < 6; ++face)
        for (unsigned y = 0; y < 8; ++y)
            for (unsigned x = 0; x < 8; ++x)
            {
                auto d = CubeDirection(face, (x + .5) / 8, (y + .5) / 8);
                for (int c = 0; c < 3; ++c)
                {
                    size_t at = ((face * 8 + y) * 8 + x) * 3 + c;
                    Check(std::abs(directional.Irradiance.Mips[0][at] - pi * (1 + 2 * d[c] / 3)) <
                              .012,
                          "Axis irradiance orientation/normalization");
                }
            }
    double previousCoefficient = 2;
    for (unsigned mip = 0; mip < directional.Specular.Mips.size(); ++mip)
    {
        unsigned side = directional.Specular.Side >> mip;
        unsigned a = (side - 1) / 2, b = side / 2;
        double radiance = 0, directionX = 0;
        for (unsigned y : {a, b})
            for (unsigned x : {a, b})
            {
                radiance += directional.Specular.Mips[mip][(y * side + x) * 3];
                directionX += CubeDirection(0, (x + .5) / side, (y + .5) / side).x;
            }
        double coefficient = (radiance - 4) / directionX;
        Check(coefficient <= previousCoefficient + .01, "GGX mip lobe failed to broaden");
        previousCoefficient = coefficient;
    }
    Check(std::abs(previousCoefficient - 2. / 3) < .015,
          "Roughest specular lobe normalization endpoint");
    auto lut = EnvironmentBake::IntegrateBRDF();
    // Uniform solid-angle hemisphere quadrature converges independently for
    // broad lobes; sharp floor endpoints also have the analytic mirror limit.
    auto integrate = [&](double nv, double r, unsigned n) {
        glm::dvec3 V(std::sqrt(1 - nv * nv), 0, nv);
        glm::dvec2 sum(0);
        double a2 = std::pow(std::max(.045, r), 4);
        for (unsigned z = 0; z < n; ++z)
        {
            double nl = (z + .5) / n;
            for (unsigned j = 0; j < 2 * n; ++j)
            {
                double phi = 2 * pi * (j + .5) / (2 * n), radius = std::sqrt(1 - nl * nl);
                glm::dvec3 L(radius * std::cos(phi), radius * std::sin(phi), nl);
                auto H = glm::normalize(L + V);
                double nh = H.z, vh = glm::dot(V, H), den = 1 + (a2 - 1) * nh * nh;
                double D = a2 / (pi * den * den);
                double vis = .5 / (nl * std::sqrt(nv * nv * (1 - a2) + a2) +
                                   nv * std::sqrt(nl * nl * (1 - a2) + a2));
                double fc = std::pow(1 - vh, 5);
                sum += glm::dvec2(1 - fc, fc) * (D * vis * nl * 2 * pi / (2 * n * n));
            }
        }
        return sum;
    };
    double maximumError = 0, maximumDelta = 0;
    for (unsigned y : {0u, 31u, 63u, 127u})
        for (unsigned x : {6u, 31u, 63u, 127u})
        {
            double nv = (x + .5) / 128, r = (y + .5) / 128;
            auto coarse = IBLReferenceIntegral(nv, r, 128), fine = IBLReferenceIntegral(nv, r, 256);
            maximumDelta = std::max(maximumDelta, glm::length(fine - coarse));
            Check(glm::length(fine - coarse) < .002, "Independent LUT quadrature did not converge");
            size_t p = (y * 128 + x) * 2;
            glm::dvec2 actual(lut.Mips[0][p], lut.Mips[0][p + 1]);
            maximumError = std::max(maximumError, glm::length(actual - fine));
            Check(glm::length(actual - fine) < .01,
                  "LUT differs from independent FP64 correlated Smith integration");
            Check(actual.x + actual.y < 1.02, "Single-scattering white furnace created energy");
        }
    // Cross-check the quadrature/PDF change against direct uniform-hemisphere
    // integration where the lobe is broad enough for that discretization.
    for (double r : {.5, 1.})
        Check(glm::length(integrate(.5, r, 512) - IBLReferenceIntegral(.5, r, 256)) < .002,
              "Independent hemisphere and half-vector integrals disagree");
    std::cout << "IBL LUT FP64 maximum error " << maximumError << "; quadrature convergence "
              << maximumDelta << "\n";
    Check(std::abs(lut.Mips[0][(127) * 2] - 1) < .001 && lut.Mips[0][127 * 2 + 1] < .00001,
          "Mirror endpoint/channel order");
    EnvironmentDefinition definition;
    definition.Image = "Source.hdr";
    auto text = WriteEnvironmentText(definition);
    Check(ReadEnvironmentText(text) == definition, "Environment intent round trip");
    rejected([&] { ReadEnvironmentText(text + "Unknown: 1\n"); });
    // Exact RGBE values decode as linear float data, retaining HDR range, rows
    // and channels; no sRGB transfer or ordinary 8-bit texture path is involved.
    std::string hdr = "#?RADIANCE\nFORMAT=32-bit_rle_rgbe\n\n-Y 1 +X 2\n";
    for (unsigned char b : {128, 64, 32, 130, 32, 64, 128, 129})
        hdr += char(b);
    auto decoded = DecodeRadianceHDR(hdr);
    Check(decoded.RGB == std::vector<float>({2, 1, .5f, .25f, .5f, 1}),
          "HDR float/row interpretation");
    rejected([&] { DecodeRadianceHDR(hdr.substr(0, hdr.size() - 1)); });
    std::string rle = "#?RADIANCE\nFORMAT=32-bit_rle_rgbe\n\n-Y 1 +X 8\n";
    auto dataAt = rle.size();
    for (unsigned char b : {2, 2, 0, 8, 136, 128, 136, 64, 136, 32, 136, 130})
        rle += char(b);
    auto scanline = DecodeRadianceHDR(rle);
    Check(scanline.Width == 8 && scanline.RGB[0] == 2 && scanline.RGB[1] == 1 &&
              scanline.RGB[2] == .5f,
          "Radiance scanline RLE decode");
    for (auto bad : {rle.substr(0, rle.size() - 1), rle + "trailing",
                     std::string("#?RADIANCE\nFORMAT=32-bit_rle_rgbe\n\n-Y 99999999 +X 8192\n")})
        rejected([&] { DecodeRadianceHDR(bad); });
    for (auto offset : {dataAt + 3, dataAt + 4})
    {
        auto bad = rle;
        bad[offset] = char(offset == dataAt + 3 ? 9 : 137);
        rejected([&] { DecodeRadianceHDR(bad); });
    }
    auto colorIntent = rle;
    colorIntent.insert(colorIntent.find("FORMAT"), "PRIMARIES=1 2 3 4\n");
    rejected([&] { DecodeRadianceHDR(colorIntent); });
    Check(std::abs(IBLReferenceIntegral(1, 1).x + IBLReferenceIntegral(1, 1).y -
                   (1 - std::log(2.))) < .00002,
          "IBL rough-conductor single-scattering analytic loss");
    double maxDiffuseApproximation = 0;
    for (double nv : {.05, .25, .5, 1.})
    {
        glm::dvec3 V(std::sqrt(1 - nv * nv), 0, nv);
        double exact = 0;
        for (unsigned y = 0; y < 128; ++y)
            for (unsigned x = 0; x < 256; ++x)
            {
                double nl = (y + .5) / 128, phi = 2 * pi * (x + .5) / 256;
                glm::dvec3 L(std::sqrt(1 - nl * nl) * std::cos(phi),
                             std::sqrt(1 - nl * nl) * std::sin(phi), nl);
                double vh = glm::dot(V, glm::normalize(V + L));
                exact += (1 - (.04 + .96 * std::pow(1 - vh, 5))) * nl * 2 / (128 * 256);
            }
        maxDiffuseApproximation =
            std::max(maxDiffuseApproximation, std::abs(exact - .96 * (1 - std::pow(1 - nv, 5))));
    }
    std::cout << "IBL diffuse Fview approximation maximum furnace deficit "
              << maxDiffuseApproximation
              << " (unit white dielectric; expected model approximation, not added energy)\n";
    // Publication uses the normal authoring codec, source adapter, lease and
    // accepted descriptor. Reuse this admitted LUT instead of baking it twice.
    auto root = std::filesystem::temp_directory_path() / ("hazel-environment-" + AssetKey(UUID()));
    std::filesystem::create_directories(root);
    struct Cleanup
    {
        std::filesystem::path Root;
        ~Cleanup()
        {
            std::error_code ec;
            std::filesystem::remove_all(Root, ec);
        }
    } cleanup{root};
    auto put = [&](auto p, const std::string &b) {
        std::filesystem::create_directories((root / p).parent_path());
        FileSystem::WriteFileAtomically(
            root / p, [&](auto &o) { o.write(b.data(), std::streamsize(b.size())); });
    };
    auto shared = std::filesystem::path("Prepared/BRDF") / EnvironmentBake::LUTKey() /
                  "0000000000000001.hlight";
    put(shared, EncodeLightingImage(lut));
    definition.Profile = {4, 4, 4, 64};
    definition.Image = "Source.hdr";
    put(definition.Image, hdr);
    auto assets = CreateRef<ProjectAssets>(root);
    EnvironmentDocument document(assets);
    document.Create("Test.henv", definition);
    auto edited = definition;
    edited.Profile.Samples = 128;
    document.Edit(edited);
    Check(document.Dirty() && document.CanUndo(), "Environment authored history omitted settings");
    document.Undo();
    Check(!document.Dirty(), "Environment exact undo");
    document.Redo();
    document.Save();
    Check(!document.Dirty() && ReadEnvironment(root / "Test.henv") == edited,
          "Environment save/redo");
    EnvironmentBake::PrepareImage(root, "Test.henv");
    auto acceptance = ReadAssetBytes(root / "Test.henv.accepted", 65536);
    auto accepted = ReadPreparedEnvironment(root, "Test.henv");
    assets->Reload("Test.henv");
    auto pinned = assets->Environment("Test.henv");
    Check(bool(pinned.Accepted), "Accepted lighting resolution");
    std::filesystem::rename(root / "Source.hdr", root / "source-unavailable");
    Check(AuditEnvironment(root, "Test.henv").size() == 6,
          "Runtime lighting closure required original HDR");
    std::filesystem::rename(root / "source-unavailable", root / "Source.hdr");
    rejected([&] { EnvironmentBake::PrepareImage(root, "Test.henv", [] { return true; }); });
    Check(ReadAssetBytes(root / "Test.henv.accepted", 65536) == acceptance,
          "Cancelled preparation replaced acceptance");
    bool changed = false;
    {
        OperationProgress::Scope progress([&](auto label, size_t, size_t) {
            if (!changed && label == "Diffuse irradiance")
            {
                changed = true;
                put("Source.hdr", hdr + "changed");
            }
        });
        rejected([&] { EnvironmentBake::PrepareImage(root, "Test.henv"); });
    }
    Check(changed && ReadAssetBytes(root / "Test.henv.accepted", 65536) == acceptance,
          "Changed HDR source accepted stale work");
    put("Source.hdr", hdr);
    auto draft = edited;
    draft.Profile.Samples = 256;
    document.Edit(draft);
    document.Save();
    assets->Reload("Test.henv");
    auto stale = assets->Environment("Test.henv");
    Check(stale.Accepted == pinned.Accepted && !stale.Error.empty(),
          "Stale intent retired accepted lighting or hid status");
    rejected([&] { ReadPreparedEnvironment(root, "Test.henv"); });
    document.Undo();
    document.Save();
    auto corrupt = accepted.Dependencies[2];
    auto bytes = ReadAssetBytes(root / corrupt, LightingArtifactByteLimit);
    put(corrupt, "corrupt");
    assets->Reload("Test.henv");
    Check(assets->Environment("Test.henv").Accepted == pinned.Accepted &&
              !assets->Environment("Test.henv").Error.empty(),
          "Corrupt artifact replaced accepted resources");
    put(corrupt, bytes);
    assets->Reload("Test.henv");
    bool changedIntent = false;
    {
        OperationProgress::Scope progress([&](auto label, size_t, size_t) {
            if (!changedIntent && label == "Diffuse irradiance")
            {
                changedIntent = true;
                put("Test.henv", WriteEnvironmentText(draft));
            }
        });
        rejected([&] { EnvironmentBake::PrepareImage(root, "Test.henv"); });
    }
    Check(changedIntent && ReadAssetBytes(root / "Test.henv.accepted", 65536) == acceptance,
          "Changed preparation settings accepted stale work");
    put("Test.henv", WriteEnvironmentText(edited));
    auto next = hdr;
    next.back() = char(130);
    put("Source.hdr", next);
    bool cancelledAfterGeneration = false;
    auto prepared = root / "Test.henv.prepared";
    auto generations = [&] {
        size_t n = 0;
        for (auto &e : std::filesystem::directory_iterator(prepared))
            if (e.path().filename().generic_u8string().front() != '.')
                ++n;
        return n;
    };
    auto count = generations();
    rejected([&] {
        EnvironmentBake::PrepareImage(root, "Test.henv", [&] {
            if (generations() > count)
            {
                cancelledAfterGeneration = true;
                return true;
            }
            return false;
        });
    });
    Check(cancelledAfterGeneration &&
              ReadAssetBytes(root / "Test.henv.accepted", 65536) == acceptance,
          "Cancellation after generation publication accepted partial replacement");
    EnvironmentBake::PrepareImage(root, "Test.henv");
    Check(ReadPreparedEnvironment(root, "Test.henv").Identity != accepted.Identity,
          "Retry failed to publish accepted complete generation");
    Check(assets->Environment("Test.henv").Accepted == pinned.Accepted,
          "Per-frame environment access scanned disk");
    assets->Reload("Test.henv");
    Check(assets->Environment("Test.henv").Accepted->Identity != pinned.Accepted->Identity,
          "Explicit environment refresh failed");
    std::cout << "PASS: environment document history/save/reopen, source-free closure, "
                 "stale/conflicting/cancelled/corrupt preparation retention and coherent retry\n";
    std::cout << "PASS: bounded HDR decode, portable lighting artifacts, cancellation, constant "
                 "piC, axis irradiance and independent FP64 LUT/furnace checks\n";
}
