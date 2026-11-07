#pragma once
#include <algorithm>
#include <cmath>
#include <glm/glm.hpp>
// Independent FP64 search of the geometric reflection constraint. This does
// not use the shader's analytic half-angle solution or an energy multiplier.
static glm::dvec3 GeometricNormalReference(glm::dvec3 n, glm::dvec3 g, glm::dvec3 v)
{
    n = glm::normalize(n);
    g = glm::normalize(g);
    v = glm::normalize(v);
    double a = glm::dot(g, v), c = glm::dot(n, g);
    if (a <= 0 || (c > 0 && glm::dot(glm::reflect(-v, n), g) >= 0))
        return n;
    auto t = n - g * c;
    if (glm::length(t) < 1e-6)
        return g;
    t = glm::normalize(t);
    double lo = 0, hi = std::min(std::acos(std::clamp(c, -1., 1.)), std::acos(-1.) / 2);
    for (int i = 0; i < 64; ++i)
    {
        double mid = (lo + hi) * .5;
        auto candidate = g * std::cos(mid) + t * std::sin(mid);
        if (glm::dot(glm::reflect(-v, candidate), g) >= 0)
            lo = mid;
        else
            hi = mid;
    }
    return glm::normalize(g * std::cos(lo) + t * std::sin(lo));
}
