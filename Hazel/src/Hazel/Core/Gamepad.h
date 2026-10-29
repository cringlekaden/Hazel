#pragma once
#include <array>
#include <cstdint>
namespace Hazel {
// Standard gamepad controls, independent of the window backend's numeric constants.
enum class GamepadButton : int { A,B,X,Y,LeftBumper,RightBumper,Back,Start,Guide,LeftThumb,RightThumb,Up,Right,Down,Left,Count };
struct GamepadSample {
    int Device=-1;
    uint64_t Revision=0;
    std::array<bool,15> Buttons{};
    // left X/Y, right X/Y; positive Y points down. Triggers are not UI axes.
    std::array<float,4> Sticks{};
};
}
