#pragma once
#include <algorithm>
#include <cmath>
#include <glm/glm.hpp>
#include <vector>
// Independent FP64 tensor quadrature. No producer/shader helpers or source
// cache data. GGX CDF coordinates resolve floor-roughness/grazing lobes.
static glm::dvec2 IBLReferenceIntegral(double nv, double rough, unsigned count = 256)
{
    const double pi = std::acos(-1.);
    std::vector<glm::dvec2> points;
    for (unsigned i = 0; i < count; ++i)
    {
        double z = std::cos(pi * (i + .75) / (count + .5)), old, derivative = 0;
        do
        {
            double p = 1, p0 = 0;
            for (unsigned k = 1; k <= count; ++k)
            {
                double next = ((2 * k - 1) * z * p - (k - 1) * p0) / k;
                p0 = p;
                p = next;
            }
            derivative = count * (z * p - p0) / (z * z - 1);
            old = z;
            z -= p / derivative;
        } while (std::abs(z - old) > 1e-14);
        points.emplace_back((z + 1) / 2, 1 / ((1 - z * z) * derivative * derivative));
    }
    double a2 = std::pow(std::max(.045, rough), 4);
    glm::dvec3 V(std::sqrt(1 - nv * nv), 0, nv);
    glm::dvec2 sum(0);
    for (auto q : points)
    {
        double nh = std::sqrt((1 - q.x) / (1 + (a2 - 1) * q.x)), radius = std::sqrt(1 - nh * nh);
        for (unsigned i = 0; i < 4 * count; ++i)
        {
            double phi = 2 * pi * (i + .5) / (4 * count);
            glm::dvec3 H(radius * std::cos(phi), radius * std::sin(phi), nh);
            double vh = glm::dot(V, H), nl = 2 * vh * nh - nv;
            if (vh <= 0 || nl <= 0)
                continue;
            double lambdaV = (std::sqrt(1 + a2 * (1 - nv * nv) / (nv * nv)) - 1) / 2,
                   lambdaL = (std::sqrt(1 + a2 * (1 - nl * nl) / (nl * nl)) - 1) / 2;
            double G = 1 / (1 + lambdaV + lambdaL), fc = std::pow(1 - vh, 5);
            sum += glm::dvec2(1 - fc, fc) * (G * vh / (nh * nv) * q.y / (4 * count));
        }
    }
    return sum;
}
