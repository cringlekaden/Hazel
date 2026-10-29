#include "hzpch.h"

#include "ScrollView.h"
#include <cmath>
namespace Hazel::GameUI
{
void Validate(const ScrollConfig &v)
{
    if ((!v.Horizontal && !v.Vertical) || !std::isfinite(v.PixelsPerUnit) || v.PixelsPerUnit < 1 ||
        v.PixelsPerUnit > 1024 || !std::isfinite(v.MinimumThumb) || v.MinimumThumb < 1 ||
        v.MinimumThumb > 256 || !std::isfinite(v.RevealMargin) || v.RevealMargin < 0 ||
        v.RevealMargin > 64)
        throw std::invalid_argument(
            "ScrollView needs enabled axes, speed 1..1024, thumb 1..256 and reveal margin 0..64");
}
Rect ScrollThumb(Rect track, float view, float extent, float offset, float minimum, int axis)
{
    Rect thumb = track;
    float range = std::max(0.f, extent - view), size = track.Size[axis];
    thumb.Size[axis] =
        std::min(size, std::max(minimum, extent > 0 ? size * std::min(1.f, view / extent) : size));
    thumb.Position[axis] +=
        (size - thumb.Size[axis]) * (range > 0 ? std::clamp(offset / range, 0.f, 1.f) : 0.f);
    return thumb;
}
float RevealDelta(Rect view, Rect target, float margin, int axis)
{
    float inset = std::min(margin, view.Size[axis] * .25f), start = view.Position[axis] + inset,
          end = view.Position[axis] + view.Size[axis] - inset;
    if (target.Size[axis] > end - start)
        return target.Position[axis] - start; // deterministic leading edge
    if (target.Position[axis] < start)
        return target.Position[axis] - start;
    return std::max(0.f, target.Position[axis] + target.Size[axis] - end);
}
} // namespace Hazel::GameUI
