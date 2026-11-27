#include "hzpch.h"
#include "CameraLensEffects.h"
#include "Hazel/Assets/AssetData.h"

namespace Hazel
{
void LensEffectsSettings::Validate() const
{
    const auto within = [](float value, float low, float high) { return value >= low && value <= high; };
    if (!within(FlareStrength, 0, 1) || GhostCount > 4 || !within(GhostSpacing, .12f, .8f) ||
        !within(HaloStrength, 0, 1) || !within(HaloRadius, .05f, .5f) ||
        !within(Starburst, 0, 1) || !within(AnamorphicStreak, 0, 1) ||
        !within(DirtStrength, 0, 4) || !within(DirtScale, .25f, 4) ||
        !within(Vignette, 0, 1) || !within(ChromaticPixels, 0, 3) ||
        !within(Distortion, -.25f, .25f) || !within(Grain, 0, .08f))
        throw std::runtime_error("Invalid lens effects range, pupil reflection count or sensor settings");
    AssetPath(DirtTexture, nullptr, true);
}
}
