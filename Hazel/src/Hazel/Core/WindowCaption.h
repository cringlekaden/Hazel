#pragma once
#include <cmath>
#include <string>
namespace Hazel {
// Client/window coordinates, never framebuffer pixels or global desktop
// coordinates.
struct CaptionRect {
    float X = 0, Y = 0, Width = 0, Height = 0;
    bool Contains(float x, float y) const {
        return std::isfinite(x) && std::isfinite(y) && std::isfinite(X) &&
               std::isfinite(Y) && std::isfinite(Width) &&
               std::isfinite(Height) && Width > 0 && Height > 0 && x >= X &&
               y >= Y && x < X + Width && y < Y + Height;
    }
};
enum class CaptionHit { Client, Drag, Minimize, Maximize, Close };
struct CaptionLayout {
    CaptionRect Drag, Minimize, Maximize, Close;
    CaptionHit Hit(float x, float y) const {
        if (Close.Contains(x, y))
            return CaptionHit::Close;
        if (Maximize.Contains(x, y))
            return CaptionHit::Maximize;
        if (Minimize.Contains(x, y))
            return CaptionHit::Minimize;
        return Drag.Contains(x, y) ? CaptionHit::Drag : CaptionHit::Client;
    }
};
struct CaptionState {
    bool Requested = false, Custom = false;
    std::string Reason = "Native decorations";
};
} // namespace Hazel
