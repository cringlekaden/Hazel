#include "hzpch.h"
#include "SpotShadows.h"
#include "SceneSubmission.h"
#include "Lighting.h"
#include <algorithm>
#include <cmath>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>

namespace Hazel
{
SpotShadowLayout FitSpotShadows(const SceneSubmission& frame)
{
    SpotShadowLayout layout;
    for (size_t i = 0; i < frame.LocalLights.size(); ++i)
    {
        const auto& light = frame.LocalLights[i];
        if (light.OuterCos == -1 || !light.CastShadows || !glm::any(glm::greaterThan(light.Incident, glm::vec3(0))))
            continue;
        ++layout.Requested;
        if (layout.Count == MaxSpotShadows)
            continue;
        LocalLight validation;
        validation.Range = light.Range;
        validation.Validate();
        SpotLight::ValidateShadowResolution(light.ShadowResolution);
        const glm::dvec3 position(light.Position), forward(light.Direction);
        if (!std::isfinite(glm::length(position)) || !std::isfinite(glm::length(forward)) ||
            std::abs(glm::length(forward) - 1) > 1e-4 ||
            !(light.OuterCos > 0 && light.OuterCos < 1))
            throw std::runtime_error("Spot shadows require a finite position, unit direction and perspective cone");

        const unsigned side = light.ShadowResolution;
        const double nearDepth = double(light.Range) / 1024.0;
        const double tangent = std::tan(std::acos(double(light.OuterCos))) / (1.0 - 4.0 / side);
        glm::dvec3 up(light.ShadowUp);
        const double upLength = glm::length(up);
        if (!std::isfinite(upLength) || upLength == 0 || glm::length(glm::cross(forward, up / upLength)) < 1e-5)
            up = std::abs(forward.y) > .99 ? glm::dvec3(0, 0, 1) : glm::dvec3(0, 1, 0);
        const auto view = glm::lookAtRH(glm::dvec3(0), forward, up);
        const auto projection = glm::perspectiveRH_NO(2.0 * std::atan(tangent), 1.0, nearDepth, double(light.Range));
        const auto slot = layout.Count++;
        layout.Data.VP[slot] = glm::mat4(projection * view);
        layout.Data.Tiles[slot] = {float(layout.Extent.x), float(side), float(nearDepth), float(tangent)};
        layout.Data.LightIndices[slot] = int(i + 1);
        layout.Extent.x += side;
        layout.Extent.y = std::max(layout.Extent.y, side);
    }
    return layout;
}
} // namespace Hazel
