#pragma once
#include "Hazel/Assets/Environment.h"
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
