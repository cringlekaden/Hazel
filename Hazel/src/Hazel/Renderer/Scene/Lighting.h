#pragma once
#include <cmath>
#include <glm/glm.hpp>
#include <stdexcept>
namespace Hazel
{
// Linear Rec.709 incident RGB, renderer-relative intensity; no photometric units.
struct DirectionalLight
{
    glm::vec3 Color{1};
    float Intensity = 1;
    bool Enabled = true;
    void Validate() const
    {
        for (int i = 0; i < 3; ++i)
            if (!std::isfinite(Color[i]) || Color[i] < 0 || Color[i] > 1)
                throw std::runtime_error("Light color requires linear RGB in [0,1]");
        if (!std::isfinite(Intensity) || Intensity < 0 || Intensity > 1000)
            throw std::runtime_error("Directional intensity requires [0,1000]");
    }
};
inline glm::vec3 DirectionalTravel(const glm::mat4 &world)
{
    auto travel = glm::vec3(world * glm::vec4(0, 0, -1, 0));
    auto length = glm::length(travel);
    if (!std::isfinite(length) || length <= 0)
        throw std::runtime_error("Directional light transform has no finite direction");
    return travel / length;
}
inline constexpr float DirectBRDFRoughnessFloor =
    .045f; // Direct_v1: single scattering GGX / correlated Smith.
} // namespace Hazel
