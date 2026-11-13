#pragma once
#include <fstream>
#include <iomanip>
#include <sstream>
// Maintainer-only entry on an existing regression executable. This oracle is
// absent from Hazel/Nutella; no producer, GL sampling or bake estimator is used.
static int IBLSurfaceReferenceCLI(int argc, char **argv)
{
    Check(argc == 6, "Usage: --ibl-reference source.hdr inputs.txt subdivision output.txt, or "
                     "--ibl-reference-ndf source.hdr inputs.txt order output.txt, or "
                     "--ibl-reference-sparse source.hdr inputs.txt order output.txt");
    const bool ndf = std::string(argv[1]) == "--ibl-reference-ndf";
    const bool sparse = std::string(argv[1]) == "--ibl-reference-sparse";
    size_t consumed = 0;
    auto requested = std::stoul(argv[4], &consumed);
    Check(consumed == std::string(argv[4]).size() && requested >= (sparse ? 8u : ndf ? 32u : 1u) &&
              requested <= (sparse ? 128u : ndf ? 4096u : 16u),
          "Reference subdivisions 1..16, NDF order 32..4096, sparse order 8..128");
    unsigned subdivision = unsigned(requested);
    auto source =
        DecodeRadianceHDR(ReadAssetBytes(std::filesystem::u8path(argv[2]), 64 * 1024 * 1024));
    std::istringstream file(ReadAssetBytes(std::filesystem::u8path(argv[3]), 1024 * 1024));
    std::vector<SurfaceReferenceInput> samples;
    std::string line;
    while (std::getline(file, line))
    {
        Check(line.size() < 2048 && samples.size() < 512,
              "Surface input limit is 512 bounded rows");
        std::istringstream row(line);
        SurfaceReferenceInput s;
        for (auto v : {&s.N, &s.V, &s.Ng, &s.Base})
            for (int c = 0; c < 3; ++c)
                row >> (*v)[c];
        row >> s.Rough >> s.Metal >> s.AO;
        Check(bool(row), "Expected N V geometricN baseRGB roughness metallic AO");
        std::string extra;
        Check(!(row >> extra), "Unexpected surface input field");
        for (auto v : {s.N, s.V, s.Ng, s.Base})
            for (int c = 0; c < 3; ++c)
                Check(std::isfinite(v[c]), "Nonfinite surface input");
        Check(glm::length(s.N) > .001 && glm::length(s.V) > .001 && glm::length(s.Ng) > .001,
              "Surface direction is degenerate");
        Check(s.Rough >= 0 && s.Rough <= 1 && s.Metal >= 0 && s.Metal <= 1 && s.AO >= 0 &&
                  s.AO <= 1,
              "Surface parameter outside 0..1");
        Check(s.Base.x >= 0 && s.Base.y >= 0 && s.Base.z >= 0 &&
                  glm::length(s.Base) <= std::sqrt(3.),
              "Invalid base reflectance");
        s.N = glm::normalize(s.N);
        s.V = glm::normalize(s.V);
        s.Ng = glm::normalize(s.Ng);
        samples.push_back(s);
    }
    Check(!samples.empty() && (sparse ? 260ull : ndf ? 4ull : uint64_t(source.Width) * source.Height) * subdivision *
                                      subdivision * samples.size() <=
                                  5000000000ull,
          "Source quadrature exceeds five billion sample evaluations; split the input rows");
    auto output = std::filesystem::absolute(std::filesystem::u8path(argv[5]));
    Check(!std::filesystem::exists(output),
          "Reference output already exists; retain earlier evidence");
    auto results = sparse ? IntegrateSurfaceSparseReference(source, samples, subdivision)
                       : ndf ? IntegrateSurfaceNDFReference(source, samples, subdivision)
                       : IntegrateSurfaceReference(source, samples, subdivision);
    std::ostringstream text;
    text << std::setprecision(12);
    for (const auto &r : results)
    {
        for (auto v : {r.Spec,r.HorizonSpec,r.Prefilter,r.Diffuse,r.NormalCosine})
            for (int c=0;c<3;++c)
                Check(std::isfinite(v[c]),"Nonfinite reference result; no evidence output written");
        for (auto v : {r.Spec, r.HorizonSpec})
        {
            text << v.x << ' ' << v.y << ' ' << v.z << ' ';
            if (sparse) break;
        }
        if (!ndf && !sparse)
            for (auto v : {r.Prefilter, r.Diffuse, r.NormalCosine})
                text << v.x << ' ' << v.y << ' ' << v.z << ' ';
        text << '\n';
    }
    FileSystem::WriteNewFile(output, text.str());
    std::cout << "MEASURED: independent FP64 " << (sparse ? "sparse-source (primary RGB only)" : ndf ? "NDF" : "source") << " quadrature for "
              << samples.size()
              << " surfaces; graphics regressions not executed in reference-only mode\n";
    return 0;
}
