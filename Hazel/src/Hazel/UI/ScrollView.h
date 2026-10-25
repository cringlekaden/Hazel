#pragma once
#include "RuntimeUIModel.h"
namespace Hazel::GameUI
{
struct ScrollConfig
{
    uint64_t Content = 0, HorizontalTrack = 0, HorizontalThumb = 0, VerticalTrack = 0,
             VerticalThumb = 0;
    bool Horizontal = false, Vertical = true;
    float PixelsPerUnit = 48, MinimumThumb = 16, RevealMargin = 4;
};
void Validate(const ScrollConfig &config);
Rect ScrollThumb(Rect track, float viewport, float extent, float offset, float minimum, int axis);
float RevealDelta(Rect viewport, Rect target, float margin, int axis);
} // namespace Hazel::GameUI
