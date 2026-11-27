#pragma once

#include "PhysicalCamera.h"
#include <cmath>
#include <cstdint>
#include <glm/glm.hpp>
#include <stdexcept>

namespace Hazel
{
enum class DepthOfFieldQuality { Balanced, Cinematic };

struct DepthOfFieldSettings
{
    bool Enabled = false;
    DepthOfFieldQuality Quality = DepthOfFieldQuality::Balanced;
    unsigned ApertureBlades = 0; // Zero models a circular pupil.
    float ApertureRotation = 0; // Radians.
    float FocusHalfLife = 0; // Seconds for half a manual focus step, in inverse world units.
    uint64_t TrackedSubject = 0; // Scene entity ID. Zero selects manual focus.

    void Validate() const
    {
        if (unsigned(Quality) > unsigned(DepthOfFieldQuality::Cinematic) ||
            (ApertureBlades != 0 && (ApertureBlades < 3 || ApertureBlades > 11)) ||
            !(std::abs(ApertureRotation) <= 6.2831855f) ||
            !(FocusHalfLife >= 0 && FocusHalfLife <= 5))
            throw std::runtime_error("Depth of field requires a circular or 3–11 blade pupil, rotation in [-2pi,2pi] and focus half-life in [0,5] seconds");
    }

    bool operator==(const DepthOfFieldSettings& other) const
    {
        return Enabled == other.Enabled && Quality == other.Quality &&
            ApertureBlades == other.ApertureBlades && ApertureRotation == other.ApertureRotation &&
            FocusHalfLife == other.FocusHalfLife && TrackedSubject == other.TrackedSubject;
    }
};

// A centered OpenGL-style perspective projection. Hazel uses the same window
// depth convention for its Metal world buffer after clip-space conversion.
struct CameraDefocus
{
    double FocusWindowDepth;
    double RadiusPixelsPerDepth;

    static CameraDefocus FromPerspective(const PhysicalCamera& lens, const glm::mat4& projection,
                                        uint32_t height)
    {
        lens.Validate();
        if (!height || !std::isfinite(projection[1][1]) || projection[1][1] <= 0 ||
            !std::isfinite(projection[2][2]) || !std::isfinite(projection[3][2]) ||
            projection[2][3] != -1 || projection[3][3] != 0 || projection[3][2] >= 0)
            throw std::runtime_error("Camera defocus requires a finite perspective projection and a nonzero image height");

        const double focalLength = lens.FocalLengthMM / 1000.0;
        const double focus = lens.FocusDistance;
        const double sensorHeight = 2 * focalLength / projection[1][1];
        const double radiusAtInfinity = focalLength * focalLength * height /
            (2 * lens.Aperture * (focus - focalLength) * sensorHeight);
        const double depthOffset = projection[3][2];
        return {.5 * (1.0 - projection[2][2] + depthOffset / focus),
                -2 * radiusAtInfinity * focus / depthOffset};
    }

    double RadiusPixels(double windowDepth) const
    {
        return (windowDepth - FocusWindowDepth) * RadiusPixelsPerDepth;
    }
};
}
