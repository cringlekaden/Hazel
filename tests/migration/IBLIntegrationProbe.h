#pragma once
#include "Hazel/Core/FileDocument.h"
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <unordered_set>
// Maintainer-only bounded experiment, never linked into a shipping renderer.
// It measures a full-BRDF estimator without changing prepared or authored assets.
// Source HDR access here is deliberately NOT a source-free runtime solution.
static int IBLIntegrationProbe(int argc, char **argv)
{
    Check(argc == 8, "Usage: --ibl-integration-probe source.hdr capture-directory output-directory "
                     "environment-samples brdf-samples shader.glsl");
    unsigned ne = unsigned(std::stoul(argv[5])), nb = unsigned(std::stoul(argv[6]));
    Check(ne >= 32 && ne <= 8192 && nb >= 16 && nb <= 1024, "Probe sample budget out of bounds");
    const auto source =
        DecodeRadianceHDR(ReadAssetBytes(std::filesystem::u8path(argv[2]), 64 * 1024 * 1024));
    auto input = std::filesystem::u8path(argv[3]), output = std::filesystem::u8path(argv[4]);
    Check(!std::filesystem::exists(output), "Retain previous candidate evidence");
    std::filesystem::create_directories(output);
    int step = 0;
    if (const auto *value = std::getenv("HAZEL_IBL_PROBE_STEP"))
        step = std::stoi(value);
    Check(step >= -6 && step <= 6 && step % 3 == 0, "Probe step must be -6,-3,0,3,6");
    const std::string prefix = "dark-" + std::to_string(step) + "-";
    constexpr unsigned w = 1280, h = 720;
    constexpr double pi = 3.14159265358979323846;
    auto floats = [&](const std::string &name)
    {
        auto bytes = ReadAssetBytes(input / name, size_t(w) * h * 16);
        Check(bytes.size() == size_t(w) * h * 16, "Expected native 1280x720 FP32 capture");
        std::vector<glm::vec4> a(size_t(w) * h);
        std::memcpy(a.data(), bytes.data(), bytes.size());
        return a;
    };
    auto normal = floats(prefix + "2.rgba32f"), positions = floats(prefix + "12.rgba32f"),
         geo = floats(prefix + "13.rgba32f"), base = floats(prefix + "1.rgba32f"),
         surface = floats(prefix + "11.rgba32f"), properties = floats(prefix + "4.rgba32f");
    auto idBytes = ReadAssetBytes(input / (prefix + "8.ids32"), size_t(w) * h * 4);
    Check(idBytes.size() == size_t(w) * h * 4, "Expected native entity IDs");
    std::vector<int> ids(size_t(w) * h);
    std::memcpy(ids.data(), idBytes.data(), idBytes.size());
    auto meta = YAML::Load(FileDocument::Read(input / "capture.yaml"));
    auto xyz = meta["Views"][std::to_string(step)]["Eye"];
    glm::vec3 eye(xyz[0].as<float>(), xyz[1].as<float>(), xyz[2].as<float>());
    double yaw = meta["Yaw"].as<double>();
    auto env = [&](glm::vec3 v)
    {
        return glm::vec3(std::cos(yaw) * v.x - std::sin(yaw) * v.z, v.y,
                         std::sin(yaw) * v.x + std::cos(yaw) * v.z);
    };
    std::unordered_set<int> supported;
    for (auto uuid : {110, 111, 112, 113, 114, 120, 121})
        supported.insert(meta["Entities"][std::to_string(uuid)].as<int>());
    size_t coverage = 0;
    for (size_t i = 0; i < normal.size(); ++i)
    {
        bool covered = supported.count(ids[i]) != 0;
        auto n = covered ? glm::normalize(env(glm::vec3(normal[i]) * 2.f - 1.f)) : glm::vec3(0);
        auto v = covered ? glm::normalize(env(eye - glm::vec3(positions[i]))) : glm::vec3(0);
        normal[i] = glm::vec4(n, covered ? surface[i].y : -1.f);
        positions[i] = glm::vec4(v, surface[i].z);
        geo[i] = glm::vec4(covered ? env(glm::vec3(geo[i])) : glm::vec3(0), properties[i].z);
        coverage += covered;
    }
    // Positive neighboring-cell proposal covers original bilinear radiance.
    const auto proposalStart = std::chrono::steady_clock::now();
    std::vector<double> cdf;
    std::vector<glm::vec4> pdf(size_t(source.Width) * source.Height);
    double total = 0;
    for (unsigned y = 0; y < source.Height; ++y)
        for (unsigned x = 0; x < source.Width; ++x)
        {
            double weight = 0;
            for (int dy = -1; dy <= 1; ++dy)
                for (int dx = -1; dx <= 1; ++dx)
                {
                    auto xx = (int(x) + dx + int(source.Width)) % int(source.Width);
                    auto yy = std::clamp(int(y) + dy, 0, int(source.Height) - 1);
                    auto k = (size_t(yy) * source.Width + xx) * 3;
                    weight += .2126 * source.RGB[k] + .7152 * source.RGB[k + 1] +
                              .0722 * source.RGB[k + 2];
                }
            double area =
                2 * pi / source.Width *
                (std::cos(pi * y / source.Height) - std::cos(pi * (y + 1) / source.Height));
            total += weight * area;
            cdf.push_back(total);
            pdf[size_t(y) * source.Width + x] = glm::vec4(float(weight), 0, 0, 0);
        }
    Check(total > 0, "Probe needs nonzero source");
    for (auto &p : pdf)
        p.x = float(p.x / total);
    auto radical = [](unsigned i, unsigned b)
    {
        double x = 0, d = 1;
        while (i)
        {
            d /= b;
            x += d * (i % b);
            i /= b;
        }
        return x;
    };
    auto evaluate = [&](glm::dvec3 d)
    {
        double xx = (.5 + std::atan2(d.z, d.x) / (2 * pi)) * source.Width - .5,
               yy = std::acos(std::clamp(d.y, -1., 1.)) / pi * source.Height - .5;
        int ix = int(std::floor(xx)), iy = int(std::floor(yy));
        auto at = [&](int x, int y)
        {
            x = (x % int(source.Width) + int(source.Width)) % int(source.Width);
            y = std::clamp(y, 0, int(source.Height) - 1);
            auto k = (size_t(y) * source.Width + x) * 3;
            return glm::dvec3(source.RGB[k], source.RGB[k + 1], source.RGB[k + 2]);
        };
        return glm::mix(glm::mix(at(ix, iy), at(ix + 1, iy), xx - ix),
                        glm::mix(at(ix, iy + 1), at(ix + 1, iy + 1), xx - ix), yy - iy);
    };
    std::vector<glm::vec4> bank(ne * 2);
    for (unsigned i = 0; i < ne; ++i)
    {
        size_t k = std::min(
            cdf.size() - 1,
            size_t(std::upper_bound(cdf.begin(), cdf.end(), total * (i + .5) / ne) - cdf.begin()));
        unsigned x = unsigned(k % source.Width), y = unsigned(k / source.Width);
        double phi = 2 * pi * ((x + radical(i + 1, 2)) / source.Width - .5),
               ct = glm::mix(std::cos(pi * y / source.Height),
                             std::cos(pi * (y + 1) / source.Height), radical(i + 1, 3)),
               st = std::sqrt(std::max(0., 1 - ct * ct));
        bank[i] = glm::vec4(st * std::cos(phi), ct, st * std::sin(phi), pdf[k].x);
        bank[ne + i] = glm::vec4(evaluate(glm::dvec3(bank[i])), 1);
    }
    std::vector<glm::vec4> radiance(pdf.size());
    for (size_t i = 0; i < radiance.size(); ++i)
        radiance[i] = glm::vec4(source.RGB[3 * i], source.RGB[3 * i + 1], source.RGB[3 * i + 2], 1);
    const double proposalSeconds =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - proposalStart).count();
    auto texture =
        [](unsigned width, unsigned height, const std::vector<glm::vec4> &data, bool linear)
    {
        TextureSpecification s;
        s.Width = width;
        s.Height = height;
        s.Format = ImageFormat::RGBA32F;
        s.GenerateMips = false;
        s.MinFilter = s.MagFilter = linear ? TextureFilter::Linear : TextureFilter::Nearest;
        s.WrapS = TextureWrap::Repeat;
        s.WrapT = TextureWrap::ClampToEdge;
        auto t = Texture2D::Create(s);
        t->SetData(data.data(), uint32_t(data.size() * sizeof(glm::vec4)));
        return t;
    };
    std::vector<Ref<Texture2D>> maps{texture(w, h, normal, false),
                                     texture(w, h, positions, false),
                                     texture(w, h, geo, false),
                                     texture(w, h, base, false),
                                     texture(source.Width, source.Height, radiance, true),
                                     texture(source.Width, source.Height, pdf, false),
                                     texture(ne, 2, bank, false),
                                     texture(w, h, floats(prefix + "10.rgba32f"), false)};
    auto fragment = FileDocument::Read(std::filesystem::u8path(argv[7]));
    const std::string vertex = R"(#version 410 core
void main(){vec2 p=vec2((gl_VertexID<<1)&2,gl_VertexID&2);gl_Position=vec4(p*2-1,0,1);})";
    auto shader = Shader::Create("IBLIntegrationProbe", vertex, fragment);
    GLuint vao = 0;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    FramebufferSpecification spec;
    spec.Width = w;
    spec.Height = h;
    spec.Attachments = {FramebufferTextureFormat::RGBA32F};
    auto target = Framebuffer::Create(spec);
    target->Bind();
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    glDisable(GL_FRAMEBUFFER_SRGB);
    shader->Bind();
    const char *names[]{"u_N", "u_V", "u_Ng", "u_Base", "u_Source", "u_PDF", "u_Bank", "u_Moments"};
    for (unsigned i = 0; i < maps.size(); ++i)
    {
        maps[i]->Bind(i);
        shader->SetInt(names[i], i);
    }
    shader->SetInt("u_NE", ne);
    shader->SetInt("u_NB", nb);
    GLuint timer = 0;
    glGenQueries(1, &timer);
    std::ofstream costs(output / "timings.txt");
    costs << "covered " << coverage << " native " << w << 'x' << h << " env " << ne << " brdf "
          << nb << '\n';
    costs << "proposal_seconds " << proposalSeconds << " source_and_proposal_gpu_bytes "
          << (radiance.size() + pdf.size() + bank.size()) * sizeof(glm::vec4) << '\n';
    for (int model : {0, 1, 2, 3})
    {
        int calibrated = model & 1;
        shader->SetInt("u_Calibrated", calibrated);
        shader->SetInt("u_GeometricLightHorizon", model >> 1);
        for (unsigned i = 0; i < 4; ++i)
            glDrawArrays(GL_TRIANGLES, 0, 3);
        glFinish();
        std::vector<double> times;
        for (unsigned i = 0; i < (model < 2 ? 24u : 1u); ++i)
        {
            glBeginQuery(GL_TIME_ELAPSED, timer);
            glDrawArrays(GL_TRIANGLES, 0, 3);
            glEndQuery(GL_TIME_ELAPSED);
            GLuint64 ns = 0;
            glGetQueryObjectui64v(timer, GL_QUERY_RESULT, &ns);
            times.push_back(double(ns) / 1e6);
        }
        std::sort(times.begin(), times.end());
        costs << (model >= 2 ? "single-control-horizon-" : "") << (calibrated ? "ratio" : "plain")
              << " median_ms " << times[times.size() / 2] << " p95_ms "
              << times[std::min(times.size() - 1, size_t(times.size() * .95))] << '\n';
        costs.flush();
        std::vector<glm::vec4> pixels(size_t(w) * h);
        glReadBuffer(GL_COLOR_ATTACHMENT0);
        glReadPixels(0, 0, w, h, GL_RGBA, GL_FLOAT, pixels.data());
        std::ofstream out(output / (std::string(model >= 2 ? "horizon-" : "") +
                                    (calibrated ? "ratio.rgba32f" : "plain.rgba32f")),
                          std::ios::binary);
        out.write(reinterpret_cast<const char *>(pixels.data()), pixels.size() * sizeof(glm::vec4));
    }
    // Held-out continuous domain sweep, using exactly the same geometric and
    // effective shading frames as the baseline probe. Not part of GPU timing.
    auto sweepText = ReadAssetBytes(input / "sweep-inputs.txt", 1024 * 1024);
    auto momentsText = ReadAssetBytes(input / "sweep-results.txt", 1024 * 1024);
    std::istringstream sweep(sweepText), momentRows(momentsText);
    std::fill(normal.begin(), normal.end(), glm::vec4(0, 0, 0, -1));
    std::vector<glm::vec4> moments(normal.size());
    std::string line, ml;
    size_t count = 0;
    while (std::getline(sweep, line))
    {
        Check(count < 512 && std::getline(momentRows, ml), "Bounded paired sweep required");
        std::istringstream row(line), mr(ml);
        SurfaceReferenceInput s;
        for (auto v : {&s.N, &s.V, &s.Ng, &s.Base})
            for (int c = 0; c < 3; ++c)
                row >> (*v)[c];
        row >> s.Rough >> s.Metal >> s.AO;
        Check(bool(row), "Invalid sweep input");
        normal[count] = glm::vec4(glm::normalize(s.N), s.Rough);
        positions[count] = glm::vec4(glm::normalize(s.V), s.Metal);
        geo[count] = glm::vec4(glm::normalize(s.Ng), s.AO);
        base[count] = glm::vec4(s.Base, 1);
        double discarded;
        for (int c = 0; c < 6; ++c)
            mr >> discarded;
        mr >> moments[count].x >> moments[count].y;
        Check(bool(mr), "Invalid paired sweep moments");
        ++count;
    }
    Check(count > 0, "Empty sweep");
    for (unsigned i = 0; i < 4; ++i)
    {
        const auto &data = i == 0 ? normal : i == 1 ? positions : i == 2 ? geo : base;
        maps[i]->SetData(data.data(), uint32_t(data.size() * sizeof(glm::vec4)));
    }
    maps[7]->SetData(moments.data(), uint32_t(moments.size() * sizeof(glm::vec4)));
    for (unsigned i = 0; i < maps.size(); ++i)
        maps[i]->Bind(i);
    for (int calibrated : {0, 1})
    {
        shader->SetInt("u_GeometricLightHorizon", 0);
        shader->SetInt("u_Calibrated", calibrated);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        std::vector<glm::vec4> values(count);
        glReadPixels(0, 0, int(count), 1, GL_RGBA, GL_FLOAT, values.data());
        std::ofstream out(output / (calibrated ? "sweep-ratio.txt" : "sweep-plain.txt"));
        out << std::setprecision(12);
        for (auto v : values)
            out << v.x << ' ' << v.y << ' ' << v.z << '\n';
    }
    glDeleteQueries(1, &timer);
    glDeleteVertexArrays(1, &vao);
    Check(glGetError() == GL_NO_ERROR, "Integration probe OpenGL error");
    std::cout << "MEASURED: bounded full-BRDF candidates; no production lighting changed; other "
                 "regressions not executed\n";
    return 0;
}
