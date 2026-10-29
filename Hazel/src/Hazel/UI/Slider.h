#pragma once
#include "RuntimeUIModel.h"
namespace Hazel::GameUI
{
// Values are reconstructed from integer lattice positions, never accumulated.
struct SteppedRange
{
    float Minimum = 0, Maximum = 1, Step = .05f;
    static constexpr uint32_t MaxSteps = 65536;
    void Validate() const;
    uint32_t LastIndex() const;
    float At(uint32_t index) const;
    uint32_t Index(float value) const;
    float Quantize(float value) const
    {
        return At(Index(value));
    }
};
struct SliderGeometry
{
    Rect Track{}, Thumb{}, Fill{};
    float Start = 0, Travel = 0;
    int Axis = 0;
    bool Reverse = false;
    float Position(glm::vec2 point, float grab = 0) const;
    bool operator==(const SliderGeometry &b) const;
};
SliderGeometry ArrangeSlider(Rect content, Rect track, glm::vec2 thumbSize, float normalized,
                             FillDirection direction);
} // namespace Hazel::GameUI
