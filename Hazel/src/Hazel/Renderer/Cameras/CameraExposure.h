#pragma once

#include <cmath>
#include <glm/glm.hpp>
#include <stdexcept>

namespace Hazel {
enum class ExposureMode { ManualPhysical, Automatic };
enum class ExposureMetering { Average, CenterWeighted, Spot };

struct CameraExposure {
    ExposureMode Mode = ExposureMode::ManualPhysical;
    float CompensationStops = 0;
    ExposureMetering Metering = ExposureMetering::CenterWeighted;
    float LowPercentile = .1f, HighPercentile = .9f;
    glm::vec2 SpotCenter{.5f};
    float SpotRadius = .12f;
    bool IncludeSky = true, Locked = false;
    float MinGainStops = -8, MaxGainStops = 8;
    float BrighteningRate = .75f, DarkeningRate = 3.5f;

    void Validate() const
    {
        const auto between = [](float v, float low, float high) {
            return v >= low && v <= high;
        };
        if ((Mode != ExposureMode::ManualPhysical && Mode != ExposureMode::Automatic) ||
            !between(CompensationStops, -16, 16))
            throw std::runtime_error("Invalid camera exposure mode or compensation");
        if (unsigned(Metering) > unsigned(ExposureMetering::Spot) ||
            !between(LowPercentile, 0, 1) || !between(HighPercentile, 0, 1) ||
            HighPercentile - LowPercentile < .01f ||
            !between(SpotCenter.x, 0, 1) || !between(SpotCenter.y, 0, 1) ||
            !between(SpotRadius, .01f, 1) ||
            !between(MinGainStops, -16, 16) || !between(MaxGainStops, MinGainStops, 16) ||
            !between(BrighteningRate, .01f, 30) || !between(DarkeningRate, .01f, 30))
            throw std::runtime_error("Invalid automatic exposure metering or adaptation settings");
    }

    bool operator==(const CameraExposure& other) const
    {
        return Mode == other.Mode && CompensationStops == other.CompensationStops &&
            Metering == other.Metering && LowPercentile == other.LowPercentile &&
            HighPercentile == other.HighPercentile && SpotCenter == other.SpotCenter &&
            SpotRadius == other.SpotRadius && IncludeSky == other.IncludeSky &&
            Locked == other.Locked && MinGainStops == other.MinGainStops &&
            MaxGainStops == other.MaxGainStops && BrighteningRate == other.BrighteningRate &&
            DarkeningRate == other.DarkeningRate;
    }
};
}
