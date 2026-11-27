#pragma once

#include "Hazel/Renderer/Cameras/CameraExposure.h"
#include "UI/PropertyUI.h"
#include <imgui.h>

namespace Hazel {
inline void DrawCameraExposure(CameraExposure& exposure)
{
    int mode = int(exposure.Mode);
    if (PropertyUI::Combo("exposure-mode", "Exposure mode", mode, "Manual physical\0Automatic\0"))
        exposure.Mode = ExposureMode(mode);
    PropertyUI::SliderFloat("compensation", "Exposure compensation (stops)", exposure.CompensationStops, -16, 16);
    if (exposure.Mode != ExposureMode::Automatic) return;
    PropertyUI::Checkbox("exposure-lock", "Lock adaptation", exposure.Locked);
    if (!ImGui::TreeNode("Advanced metering")) return;
    int meter = int(exposure.Metering);
    if (PropertyUI::Combo("metering", "Metering", meter, "Average\0Center-weighted\0Spot\0"))
        exposure.Metering = ExposureMetering(meter);
    if (exposure.Metering == ExposureMetering::Spot) {
        PropertyUI::SliderFloat("spot-x", "Spot horizontal", exposure.SpotCenter.x, 0, 1);
        PropertyUI::SliderFloat("spot-y", "Spot vertical (from top)", exposure.SpotCenter.y, 0, 1);
        PropertyUI::SliderFloat("spot-radius", "Spot radius (short edge)", exposure.SpotRadius, .01f, 1);
    }
    PropertyUI::Checkbox("meter-sky", "Include sky and clear background", exposure.IncludeSky);
    PropertyUI::SliderFloat("low-percentile", "Low percentile", exposure.LowPercentile, 0, exposure.HighPercentile - .01f);
    PropertyUI::SliderFloat("high-percentile", "High percentile", exposure.HighPercentile, exposure.LowPercentile + .01f, 1);
    PropertyUI::SliderFloat("min-gain", "Minimum gain (stops)", exposure.MinGainStops, -16, exposure.MaxGainStops);
    PropertyUI::SliderFloat("max-gain", "Maximum gain (stops)", exposure.MaxGainStops, exposure.MinGainStops, 16);
    PropertyUI::SliderFloat("brighten-rate", "Dark transition rate (1/s)", exposure.BrighteningRate, .01f, 30);
    PropertyUI::SliderFloat("darken-rate", "Bright transition rate (1/s)", exposure.DarkeningRate, .01f, 30);
    ImGui::TreePop();
}
}
