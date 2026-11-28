#pragma once

#include <filesystem>
#include <glm/glm.hpp>

namespace Hazel
{
struct ColorGradeSettings
{
    static constexpr float NeutralKelvin = 6504;

    float WhiteKelvin = NeutralKelvin;
    float TintUV = 0;
    float Saturation = 1, Contrast = 0, Shadows = 0, Highlights = 0;
    std::filesystem::path LUT;
    float LUTStrength = 1;

    bool HasWhiteBalance() const { return WhiteKelvin != NeutralKelvin || TintUV != 0; }
    bool HasToneControls() const { return Saturation != 1 || Contrast != 0 || Shadows != 0 || Highlights != 0; }
    glm::dmat3 WhiteBalance() const;
    void Validate() const;

    bool operator==(const ColorGradeSettings& rhs) const
    {
        return WhiteKelvin == rhs.WhiteKelvin && TintUV == rhs.TintUV &&
            Saturation == rhs.Saturation && Contrast == rhs.Contrast && Shadows == rhs.Shadows &&
            Highlights == rhs.Highlights && LUT == rhs.LUT && LUTStrength == rhs.LUTStrength;
    }
};
}
