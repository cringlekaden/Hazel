#pragma once

#include "PhysicalCamera.h"
#include <cmath>
#include <stdexcept>

namespace Hazel
{
enum class MotionShutterMode { Duration, Angle };
enum class MotionBlurQuality { Balanced, Cinematic };

struct MotionBlurSettings
{
    bool Enabled = false;
    MotionShutterMode Shutter = MotionShutterMode::Duration;
    float ShutterAngleDegrees = 180;
    MotionBlurQuality Quality = MotionBlurQuality::Balanced;

    void Validate() const
    {
        if (unsigned(Shutter) > unsigned(MotionShutterMode::Angle) ||
            unsigned(Quality) > unsigned(MotionBlurQuality::Cinematic) ||
            !(ShutterAngleDegrees >= 0 && ShutterAngleDegrees <= 360))
            throw std::runtime_error("Motion blur requires a valid shutter mode, quality and angle in [0,360] degrees");
    }

    double Fraction(const PhysicalCamera& lens, double frameSeconds) const
    {
        if (!Enabled || !(frameSeconds > 0) || !std::isfinite(frameSeconds))
            return 0;
        return Shutter == MotionShutterMode::Duration ? lens.ShutterSeconds / frameSeconds :
            ShutterAngleDegrees / 360.0;
    }

    bool operator==(const MotionBlurSettings& other) const
    {
        return Enabled == other.Enabled && Shutter == other.Shutter &&
            ShutterAngleDegrees == other.ShutterAngleDegrees && Quality == other.Quality;
    }
};
}
