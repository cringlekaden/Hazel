#include "hzpch.h"
#include "CameraColorGrade.h"
#include "Hazel/Assets/AssetData.h"
#include <stdexcept>

namespace Hazel
{
namespace
{
glm::dvec2 DaylightXY(double temperature)
{
    const double t = 1000 / temperature;
    const double x = temperature <= 7000
        ? ((-4.6070 * t + 2.9678) * t + .09911) * t + .244063
        : ((-2.0064 * t + 1.9018) * t + .24748) * t + .237040;
    return {x, (-3 * x + 2.87) * x - .275};
}

glm::dvec2 ToUV(glm::dvec2 xy)
{
    return glm::dvec2(4 * xy.x, 6 * xy.y) / (12 * xy.y - 2 * xy.x + 3);
}

glm::dvec2 WhiteUV(double temperature)
{
    const double t = temperature / 1000;
    const glm::dvec2 planckian(
        ((.128641212 * t + .154118254) * t + .860117757) / ((.708145163 * t + .842420235) * t + 1),
        ((.0420481691 * t + .0422806245) * t + .317398726) / ((.161456053 * t - .0289741816) * t + 1));
    if (temperature <= 4000) return planckian;
    // Match the standard daylight white at the neutral setting.
    static const auto daylightOffset = ToUV({.3127, .3290}) - ToUV(DaylightXY(ColorGradeSettings::NeutralKelvin));
    const auto daylight = ToUV(DaylightXY(temperature)) + daylightOffset;
    return glm::mix(planckian, daylight, glm::smoothstep(4000., 5000., temperature));
}

glm::dvec3 ToXYZ(glm::dvec2 uv)
{
    return {1.5 * uv.x / uv.y, 1, (4 - uv.x - 10 * uv.y) / (2 * uv.y)};
}
}

void ColorGradeSettings::Validate() const
{
    const auto within = [](float x, float lo, float hi) { return x >= lo && x <= hi; };
    if (!within(WhiteKelvin, 2000, 12000) || !within(TintUV, -.02f, .02f) ||
        !within(Saturation, 0, 2) || !within(Contrast, -1, 1) ||
        !within(Shadows, -1, 1) || !within(Highlights, -1, 1) || !within(LUTStrength, 0, 1))
        throw std::runtime_error("Invalid camera white balance or color grade range");
    AssetPath(LUT, ".cube", true);
}

glm::dmat3 ColorGradeSettings::WhiteBalance() const
{
    if (!HasWhiteBalance()) return glm::dmat3(1);
    auto reference = WhiteUV(WhiteKelvin);
    if (TintUV != 0) {
        const auto tangent = WhiteUV(WhiteKelvin + 1.) - WhiteUV(WhiteKelvin - 1.);
        reference += double(TintUV) * glm::normalize(glm::dvec2(tangent.y, -tangent.x));
    }
    // Matrices are supplied as columns. Bradford scales cone responses at Y=1.
    static const glm::dmat3 rgbToXYZ(
        .4123907993, .2126390059, .0193308187,
        .3575843394, .7151686788, .1191947798,
        .1804807884, .0721923154, .9505321522);
    static const glm::dmat3 bradford(
        .8951, -.7502, .0389,
        .2664, 1.7135, -.0685,
        -.1614, .0367, 1.0296);
    static const auto toCone = bradford * rgbToXYZ;
    static const auto fromCone = glm::inverse(toCone);
    static const auto d65 = bradford * ToXYZ(ToUV({.3127, .3290}));
    const auto gain = d65 / (bradford * ToXYZ(reference));
    return fromCone * glm::dmat3(gain.x, 0, 0, 0, gain.y, 0, 0, 0, gain.z) * toCone;
}
}
