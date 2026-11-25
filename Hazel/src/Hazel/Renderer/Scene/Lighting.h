#pragma once
#include <cmath>
#include <glm/glm.hpp>
#include <stdexcept>
namespace Hazel
{
// Linear Rec.709 incident RGB, renderer-relative intensity; no photometric units.
struct LightParameters
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
            throw std::runtime_error("Light intensity requires [0,1000]");
    }
};
struct DirectionalLight : LightParameters {};
// Shared portable storage bound: 16 + 320 * 48 fits the GL 4.1 16 KiB UBO guarantee.
inline constexpr unsigned MaxLocalLights = 320;
struct PointLight : LightParameters
{
    float Range = 10;
    void Validate() const
    {
        LightParameters::Validate();
        if (!std::isfinite(Range) || Range < .01f || Range > 10000)
            throw std::runtime_error("Local light range requires [.01,10000] world units");
    }
};
struct SpotLight : PointLight
{
    // Half angles in radians. A strictly positive penumbra avoids undefined smoothstep.
    float InnerCone = .34906585f, OuterCone = .52359878f;
    void Validate() const
    {
        PointLight::Validate();
        if (!std::isfinite(InnerCone) || !std::isfinite(OuterCone) ||
            InnerCone < 0 || OuterCone > 1.553343f || OuterCone - InnerCone < .001f)
            throw std::runtime_error("Spot half angles require 0 <= inner < outer <= 89 degrees and .001 rad penumbra");
    }
};
inline glm::vec3 LightPosition(const glm::mat4 &world)
{
    glm::vec3 position(world[3]);
    for (int i=0;i<3;++i)
        if (!std::isfinite(position[i])) throw std::runtime_error("Nonfinite local light position");
    return position;
}
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
