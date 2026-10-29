#include "hzpch.h"
#include "OwnedOverlay.h"
#include <cmath>
namespace Hazel::GameUI
{
OverlayPlacement PlaceOwnedOverlay(Rect surface, Rect trigger, float width, float height, float gap)
{
    surface.Inset({});
    trigger.Inset({});
    if (!std::isfinite(width) || !std::isfinite(height) || !std::isfinite(gap) || width < 0 || height < 0 ||
        gap < 0)
        throw std::invalid_argument("Invalid owned-overlay placement dimensions");
    const float bottom = surface.Position.y + surface.Size.y;
    float below = std::max(0.f, bottom - (trigger.Position.y + trigger.Size.y + gap));
    float above = std::max(0.f, trigger.Position.y - gap - surface.Position.y);
    OverlayPlacement p;
    p.Above = below < height && above > below;
    p.Bounds.Size = {std::min(width, surface.Size.x), std::min(height, p.Above ? above : below)};
    p.Bounds.Position.x = std::clamp(trigger.Position.x, surface.Position.x,
                                     surface.Position.x + surface.Size.x - p.Bounds.Size.x);
    p.Bounds.Position.y =
        p.Above ? trigger.Position.y - gap - p.Bounds.Size.y : trigger.Position.y + trigger.Size.y + gap;
    p.Bounds.Position.y = std::clamp(p.Bounds.Position.y, surface.Position.y, bottom - p.Bounds.Size.y);
    return p;
}
} // namespace Hazel::GameUI
