#pragma once
#include "RuntimeUIModel.h"
namespace Hazel::GameUI
{
struct OverlayPlacement
{
    Rect Bounds;
    bool Above = false;
};
// Both dropdown panels and passive tips use this surface-local placement policy.
OverlayPlacement PlaceOwnedOverlay(Rect surface, Rect trigger, float width, float height, float gap);
} // namespace Hazel::GameUI
