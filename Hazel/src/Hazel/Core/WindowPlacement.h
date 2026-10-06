#pragma once
#include <vector>
namespace Hazel {
    struct DisplayArea {
        int X = 0, Y = 0, Width = 1280, Height = 720;
        float Scale = 1;
    };
    struct WindowPlacement {
        int X = 64, Y = 64, Width = 1280, Height = 720;
        float Scale = 1;
        bool Maximized = false;
    };
    // Pure screen-coordinate policy, shared by restore and logical regressions.
    WindowPlacement FitWindow(WindowPlacement value, const std::vector<DisplayArea> &displays);
} // namespace Hazel
