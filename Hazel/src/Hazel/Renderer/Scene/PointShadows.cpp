#include "hzpch.h"
#include "PointShadows.h"
#include "Lighting.h"
#include "SceneSubmission.h"
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>

namespace Hazel
{
PointShadowLayout FitPointShadows(const SceneSubmission& frame)
{
    PointShadowLayout layout;
    for (size_t i = 0; i < frame.LocalLights.size(); ++i)
    {
        const auto& light = frame.LocalLights[i];
        if (light.OuterCos != -1 || !light.CastShadows ||
            !glm::any(glm::greaterThan(light.Incident, glm::vec3(0))))
            continue;
        ++layout.Requested;
        if (layout.Side)
            continue;
        PointLight validation;
        validation.Range = light.Range;
        validation.ShadowResolution = light.ShadowResolution;
        validation.Validate();
        for (int axis = 0; axis < 3; ++axis)
            if (!std::isfinite(light.Position[axis]))
                throw std::runtime_error("Point shadows require a finite position");

        layout.LightIndex = unsigned(i);
        layout.Side = light.ShadowResolution;
        const double nearDepth = double(light.Range) / 1024;
        const double inverseTangent = 1 - 8.0 / layout.Side;
        layout.Data.Parameters = {float(i + 1), float(layout.Side), float(nearDepth), float(inverseTangent)};
        const auto projection = glm::perspectiveRH_NO(2 * std::atan(1 / inverseTangent), 1.0,
            nearDepth, double(light.Range));
        for (unsigned face = 0; face < PointShadowFaces; ++face)
        {
            const unsigned axis = face / 2;
            glm::dvec3 forward(0);
            forward[axis] = face % 2 == 0 ? 1 : -1;
            const glm::dvec3 up = axis == 1 ? glm::dvec3(0, 0, 1) : glm::dvec3(0, 1, 0);
            layout.VP[face] = glm::mat4(projection * glm::lookAtRH(glm::dvec3(0), forward, up));
        }
    }
    return layout;
}
} // namespace Hazel
