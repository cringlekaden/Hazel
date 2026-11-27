#pragma once

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace Hazel
{
struct PhysicalCamera
{
    static constexpr double ReferenceEV100 = 10;

    bool LensProjection = false;
    float FocalLengthMM = 50, SensorWidthMM = 36, SensorHeightMM = 24;
    float Aperture = 4, ShutterSeconds = 1.f / 64, ISO = 100;
    float FocusDistance = 5;

    bool operator==(const PhysicalCamera& other) const
    {
        return LensProjection == other.LensProjection && FocalLengthMM == other.FocalLengthMM &&
            SensorWidthMM == other.SensorWidthMM && SensorHeightMM == other.SensorHeightMM &&
            Aperture == other.Aperture && ShutterSeconds == other.ShutterSeconds &&
            ISO == other.ISO && FocusDistance == other.FocusDistance;
    }

    void Validate() const
    {
        const auto inRange = [](float value, float lower, float upper) {
            return value >= lower && value <= upper;
        };
        if (!inRange(FocalLengthMM, 1, 2000) || !inRange(SensorWidthMM, 1, 200) ||
            !inRange(SensorHeightMM, 1, 200))
            throw std::runtime_error("Physical camera requires a 1–2000 mm focal length and 1–200 mm sensor dimensions");
        if (!inRange(Aperture, .5f, 128) || !inRange(ShutterSeconds, 1.f / 32000, 60) ||
            !inRange(ISO, 1, 204800))
            throw std::runtime_error("Physical camera requires f/0.5–128, 1/32000–60 seconds and ISO 1–204800");
        if (!std::isfinite(FocusDistance) || double(FocusDistance) <= double(FocalLengthMM) / 1000)
            throw std::runtime_error("Physical camera focus distance must be finite and greater than focal length");
    }

    double EV100() const
    {
        return std::log2(double(Aperture) * Aperture * 100 / (double(ShutterSeconds) * ISO));
    }

    float Log2ExposureGain(float compensationStops) const
    {
        if (!(compensationStops >= -16 && compensationStops <= 16))
            throw std::runtime_error("Camera exposure compensation requires [-16,16] stops");
        return float(std::clamp(ReferenceEV100 - EV100() + compensationStops, -16.0, 16.0));
    }

    double ActiveSensorHeightMM(double aspect) const
    {
        return std::min(double(SensorHeightMM), SensorWidthMM / aspect);
    }

    double VerticalFOV(double aspect) const
    {
        return 2 * std::atan(ActiveSensorHeightMM(aspect) / (2 * FocalLengthMM));
    }

    double HorizontalFOV(double aspect) const
    {
        return 2 * std::atan(ActiveSensorHeightMM(aspect) * aspect / (2 * FocalLengthMM));
    }
};
}
