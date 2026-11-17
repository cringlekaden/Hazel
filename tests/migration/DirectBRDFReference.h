#pragma once
#include <glm/glm.hpp>
#include <algorithm>
#include <cmath>

// Independent FP64 reference shared by the existing direct and native parity tests.
inline glm::dvec3 OracleBRDF(glm::dvec3 base, double metal, double rough, glm::dvec3 n,
                             glm::dvec3 v, glm::dvec3 l)
{
    const double pi = std::acos(-1.0);
    double nl = glm::dot(n, l), nv = glm::dot(n, v);
    if (nl <= 0 || nv <= 0)
        return {};
    auto h = glm::normalize(v + l);
    double nh = glm::dot(n, h), vh = glm::dot(v, h), alpha = std::max(.045, rough);
    alpha *= alpha;
    double alpha2 = alpha * alpha;
    double d = alpha2 / (pi * std::pow(1 + (alpha2 - 1) * nh * nh, 2));
    double lambdaV = std::sqrt(1 + alpha2 * (1 / (nv * nv) - 1)),
           lambdaL = std::sqrt(1 + alpha2 * (1 / (nl * nl) - 1));
    double visibility = 1 / (2 * nl * nv * (lambdaV + lambdaL));
    auto f0 = glm::mix(glm::dvec3(.04), base, metal);
    auto f = f0 + (1.0 - f0) * std::pow(1 - vh, 5);
    return ((1 - metal) * (1.0 - f) * base / pi + d * visibility * f) * nl;
}
inline glm::dvec3 OracleNeutral(glm::dvec3 value)
{
    double minimum = std::min({value.r, value.g, value.b});
    double offset = minimum < .08 ? minimum - 6.25 * minimum * minimum : .04;
    value -= offset;
    double peak = std::max({value.r, value.g, value.b});
    if (peak < .76)
        return value;
    double compressed = 1 - .24 * .24 / (peak + .24 - .76);
    auto scaled = value * (compressed / peak);
    double amount = 1 - 1 / (.15 * (peak - compressed) + 1);
    return glm::mix(scaled, glm::dvec3(compressed), amount);
}
