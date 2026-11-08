#pragma once
#include "Hazel/Assets/Environment.h"
#include "IBLReferenceMath.h"
#include <algorithm>
#include <cmath>
// Streaming FP64 source quadrature, independent of the producer's QMC, CDF,
// normalization and GPU texture sampling. Linear source radiance is never
// clipped.
struct SurfaceReferenceInput
{
    glm::dvec3 N, V, Ng, Base;
    double Rough, Metal, AO;
};
struct SurfaceReferenceOutput
{
    glm::dvec3 Spec{}, HorizonSpec{}, Prefilter{}, Diffuse{}, NormalCosine{};
};
static std::vector<SurfaceReferenceOutput>
IntegrateSurfaceReference(const Hazel::RadianceImage &source,
                          const std::vector<SurfaceReferenceInput> &samples, unsigned subdivision)
{
    const double pi = std::acos(-1.);
    unsigned width = source.Width * subdivision, height = source.Height * subdivision;
    std::vector<SurfaceReferenceOutput> out(samples.size());
    std::vector<glm::dvec3> reflect;
    std::vector<double> nv, alpha2, normalization;
    for (auto &s : samples)
    {
        reflect.push_back(glm::normalize(2 * glm::dot(s.N, s.V) * s.N - s.V));
        nv.push_back(glm::dot(s.N, s.V));
        alpha2.push_back(std::pow(std::max(.045, s.Rough), 4));
        double value = 0;
        for (unsigned k = 0; k < 65536; ++k)
        {
            double h2 = (1 - (k + .5) / 65536) / (1 + (alpha2.back() - 1) * (k + .5) / 65536);
            value += std::max(0., 2 * h2 - 1) / 65536;
        }
        normalization.push_back(value);
    }
    auto at = [&](int x, int y)
    {
        x = (x % int(source.Width) + int(source.Width)) % int(source.Width);
        y = std::clamp(y, 0, int(source.Height) - 1);
        size_t p = (size_t(y) * source.Width + x) * 3;
        return glm::dvec3(source.RGB[p], source.RGB[p + 1], source.RGB[p + 2]);
    };
    std::vector<double> sine(width), cosine(width);
    for (unsigned x = 0; x < width; ++x)
    {
        double phi = 2 * pi * ((x + .5) / width - .5);
        sine[x] = std::sin(phi);
        cosine[x] = std::cos(phi);
    }
    for (unsigned y = 0; y < height; ++y)
    {
        double theta = pi * (y + .5) / height, sy = (y + .5) / height * source.Height - .5;
        int iy = int(std::floor(sy));
        double st = std::sin(theta), ct = std::cos(theta);
        double area =
            2 * pi / width * (std::cos(pi * y / height) - std::cos(pi * (y + 1) / height));
        for (unsigned x = 0; x < width; ++x)
        {
            double sx = (x + .5) / width * source.Width - .5;
            int ix = int(std::floor(sx));
            auto radiance =
                glm::mix(glm::mix(at(ix, iy), at(ix + 1, iy), sx - ix),
                         glm::mix(at(ix, iy + 1), at(ix + 1, iy + 1), sx - ix), sy - iy) *
                area;
            glm::dvec3 L(st * cosine[x], ct, st * sine[x]);
            for (size_t i = 0; i < samples.size(); ++i)
            {
                auto &s = samples[i];
                auto &r = out[i];
                auto weight = [&](double cosine)
                {
                    if (cosine <= 0)
                        return 0.;
                    double nh2 = (1 + cosine) * .5, den = (1 - nh2) + alpha2[i] * nh2;
                    return alpha2[i] * cosine / (4 * pi * den * den * normalization[i]);
                };
                r.Prefilter += radiance * weight(glm::dot(reflect[i], L));
                double nl = glm::dot(s.N, L);
                if (nl > 0)
                    r.NormalCosine += radiance * (nl / std::acos(-1.));
                if (nl <= 0 || nv[i] <= 0)
                    continue;
                double vl = glm::dot(s.V, L), hs = std::sqrt(std::max(1e-30, 2 * (1 + vl))),
                       nh = (nv[i] + nl) / hs, vh = (1 + vl) / hs;
                double den = (1 - nh * nh) + alpha2[i] * nh * nh;
                double D = alpha2[i] / (pi * den * den),
                       visibility =
                           .5 / (nl * std::sqrt(nv[i] * nv[i] * (1 - alpha2[i]) + alpha2[i]) +
                                 nv[i] * std::sqrt(nl * nl * (1 - alpha2[i]) + alpha2[i]));
                double f = 1 - std::clamp(vh, 0., 1.);
                f = f * f * f * f * f;
                auto f0 = glm::mix(glm::dvec3(.04), s.Base, s.Metal);
                auto F = f0 + (1. - f0) * f;
                auto contribution = radiance * F * (D * visibility * nl * s.AO);
                r.Spec += contribution;
                if (glm::dot(s.Ng, L) > 0)
                    r.HorizonSpec += contribution;
                double fv = std::pow(1 - std::clamp(nv[i], 0., 1.), 5);
                r.Diffuse += radiance * s.Base * (1 - s.Metal) * (1. - (f0 + (1. - f0) * fv)) *
                             (nl * s.AO / pi);
            }
        }
    }
    return out;
}

// Independent microfacet-CDF tensor quadrature for sharp lobes that source-cell
// quadrature cannot resolve. Gauss-Legendre in the GGX NDF CDF and midpoint
// azimuth; no production visible-normal sampler, LUT or radiance proposal.
static std::vector<SurfaceReferenceOutput>
IntegrateSurfaceNDFReference(const Hazel::RadianceImage &source,
                             const std::vector<SurfaceReferenceInput> &samples, unsigned order)
{
    const double pi = std::acos(-1.);
    const auto nodes = IBLReferenceNodes(order);
    auto radiance = [&](glm::dvec3 l)
    {
        double x = (.5 + std::atan2(l.z, l.x) / (2 * pi)) * source.Width - .5;
        double y = std::acos(std::clamp(l.y, -1., 1.)) / pi * source.Height - .5;
        int ix = int(std::floor(x)), iy = int(std::floor(y));
        auto at = [&](int xx, int yy)
        {
            xx = (xx % int(source.Width) + int(source.Width)) % int(source.Width);
            yy = std::clamp(yy, 0, int(source.Height) - 1);
            auto p = (size_t(yy) * source.Width + xx) * 3;
            return glm::dvec3(source.RGB[p], source.RGB[p + 1], source.RGB[p + 2]);
        };
        return glm::mix(glm::mix(at(ix, iy), at(ix + 1, iy), x - ix),
                        glm::mix(at(ix, iy + 1), at(ix + 1, iy + 1), x - ix), y - iy);
    };
    std::vector<SurfaceReferenceOutput> out(samples.size());
    for (size_t i = 0; i < samples.size(); ++i)
    {
        const auto &s = samples[i];
        double nv = glm::dot(s.N, s.V), a2 = std::pow(std::max(.045, s.Rough), 4);
        if (nv <= 0)
            continue;
        auto X = glm::normalize(
                 glm::cross(std::abs(s.N.z) < .9 ? glm::dvec3(0, 0, 1) : glm::dvec3(1, 0, 0), s.N)),
             Y = glm::cross(s.N, X);
        auto f0 = glm::mix(glm::dvec3(.04), s.Base, s.Metal);
        for (auto q : nodes)
        {
            double u = q.x, w = q.y;
            double nh = std::sqrt((1 - u) / (1 + (a2 - 1) * u)),
                   sh = std::sqrt(std::max(0., 1 - nh * nh));
            for (unsigned j = 0; j < 4 * order; ++j)
            {
                double phi = 2 * pi * (j + .5) / (4 * order);
                auto H = X * (sh * std::cos(phi)) + Y * (sh * std::sin(phi)) + s.N * nh;
                double vh = glm::dot(s.V, H);
                if (vh <= 0)
                    continue;
                auto L = 2 * vh * H - s.V;
                double nl = glm::dot(s.N, L);
                if (nl <= 0)
                    continue;
                double vis = .5 / (nl * std::sqrt(nv * nv * (1 - a2) + a2) +
                                   nv * std::sqrt(nl * nl * (1 - a2) + a2));
                double fc = std::pow(1 - std::clamp(vh, 0., 1.), 5);
                auto value = radiance(L) * (f0 + (1. - f0) * fc) *
                             (4 * nl * vis * vh / nh * s.AO * w / (4 * order));
                out[i].Spec += value;
                if (glm::dot(s.Ng, L) > 0)
                    out[i].HorizonSpec += value;
            }
        }
    }
    return out;
}

// Guard the reference's Jacobian, visibility algebra, color and AO independently
// of any candidate. A constant field has known BRDF moments; a geometric
// incoming horizon can only remove positive radiance.
static void SurfaceReferenceChecks()
{
    Hazel::RadianceImage constant;
    constant.Width = 256;
    constant.Height = 128;
    const glm::dvec3 color(.25, .5, 1.);
    for (unsigned i = 0; i < constant.Width * constant.Height; ++i)
        for (int c = 0; c < 3; ++c)
            constant.RGB.push_back(float(color[c]));
    std::vector<SurfaceReferenceInput> inputs;
    for (double nv : {.05, .5})
        for (double r : {.05, .5, 1.})
            for (double m : {0., 1.})
                inputs.push_back({{0, 0, 1},
                                  {std::sqrt(1 - nv * nv), 0, nv},
                                  {0, 0, 1},
                                  {.955, .638, .538},
                                  r,
                                  m,
                                  .6});
    auto results = IntegrateSurfaceNDFReference(constant, inputs, 256);
    for (size_t i = 0; i < inputs.size(); ++i)
    {
        const auto &s = inputs[i];
        auto ab = IBLReferenceIntegral(s.V.z, s.Rough);
        auto f0 = glm::mix(glm::dvec3(.04), s.Base, s.Metal);
        auto expected = color * (f0 * ab.x + ab.y) * s.AO;
        for (int c = 0; c < 3; ++c)
        {
            if (std::abs(results[i].Spec[c] - expected[c]) > 1e-10 ||
                std::abs(results[i].HorizonSpec[c] - expected[c]) > 1e-10)
                throw std::runtime_error(
                    "FP64 NDF reference disagrees with independent furnace moments");
        }
    }
    auto input = inputs.back();
    input.Ng = glm::normalize(glm::dvec3(.8, 0, .2));
    auto horizon = IntegrateSurfaceNDFReference(constant, {input}, 256).front();
    if (!(horizon.HorizonSpec.x > 0 && horizon.HorizonSpec.x < horizon.Spec.x - .001))
        throw std::runtime_error("Reference did not distinguish incoming geometric horizon");
    auto broad = IntegrateSurfaceReference(constant, {inputs.back()}, 4).front();
    for (int c = 0; c < 3; ++c)
        if (std::abs(broad.Spec[c] - results.back().Spec[c]) > 2e-5)
            throw std::runtime_error("Independent source/NDF furnace quadratures disagree");
}
