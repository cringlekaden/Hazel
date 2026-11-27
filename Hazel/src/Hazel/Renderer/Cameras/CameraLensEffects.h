#pragma once

#include <filesystem>

namespace Hazel
{
struct LensEffectsSettings
{
    float FlareStrength = 0;
    unsigned GhostCount = 3;
    float GhostSpacing = .42f;
    float HaloStrength = 0, HaloRadius = .24f;
    float Starburst = 0, AnamorphicStreak = 0;
    std::filesystem::path DirtTexture;
    float DirtStrength = 0, DirtScale = 1;
    float Vignette = 0, ChromaticPixels = 0, Distortion = 0, Grain = 0;

    void Validate() const;

    bool operator==(const LensEffectsSettings& rhs) const
    {
        return FlareStrength == rhs.FlareStrength && GhostCount == rhs.GhostCount && GhostSpacing == rhs.GhostSpacing &&
            HaloStrength == rhs.HaloStrength && HaloRadius == rhs.HaloRadius && Starburst == rhs.Starburst &&
            AnamorphicStreak == rhs.AnamorphicStreak && DirtTexture == rhs.DirtTexture &&
            DirtStrength == rhs.DirtStrength && DirtScale == rhs.DirtScale && Vignette == rhs.Vignette &&
            ChromaticPixels == rhs.ChromaticPixels && Distortion == rhs.Distortion && Grain == rhs.Grain;
    }
};
}
