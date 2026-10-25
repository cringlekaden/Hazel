#include "Dropdown.h"
#include "Hazel/Utils/UTF8.h"
#include "hzpch.h"
#include <cmath>
namespace Hazel::GameUI
{
void Validate(const DropdownConfig &c)
{
    if (!std::isfinite(c.MinimumWidth) || c.MinimumWidth < 1 || c.MinimumWidth > 4096 ||
        !std::isfinite(c.MaximumHeight) || c.MaximumHeight < 1 || c.MaximumHeight > 4096 ||
        !std::isfinite(c.Gap) || c.Gap < 0 || c.Gap > 64 || c.Placeholder.size() > 1024)
        throw std::invalid_argument("Dropdown needs width/height 1..4096, gap "
                                    "0..64 and placeholder <=1024 UTF-8 bytes");
    UTF8::Validate(c.Placeholder);
}
DropdownPlacement PlaceDropdown(Rect surface, Rect trigger, float width, float height, float gap)
{
    surface.Inset({});
    trigger.Inset({});
    if (!std::isfinite(width) || !std::isfinite(height) || !std::isfinite(gap) || width < 0 ||
        height < 0 || gap < 0)
        throw std::invalid_argument("Invalid dropdown placement dimensions");
    const float bottom = surface.Position.y + surface.Size.y;
    float below = std::max(0.f, bottom - (trigger.Position.y + trigger.Size.y + gap));
    float above = std::max(0.f, trigger.Position.y - gap - surface.Position.y);
    DropdownPlacement p;
    p.Above = below < height && above > below;
    p.Bounds.Size = {std::min(width, surface.Size.x), std::min(height, p.Above ? above : below)};
    p.Bounds.Position.x = std::clamp(trigger.Position.x, surface.Position.x,
                                     surface.Position.x + surface.Size.x - p.Bounds.Size.x);
    p.Bounds.Position.y = p.Above ? trigger.Position.y - gap - p.Bounds.Size.y
                                  : trigger.Position.y + trigger.Size.y + gap;
    p.Bounds.Position.y =
        std::clamp(p.Bounds.Position.y, surface.Position.y, bottom - p.Bounds.Size.y);
    return p;
}
} // namespace Hazel::GameUI
