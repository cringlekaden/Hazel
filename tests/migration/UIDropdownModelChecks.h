#include "Hazel/UI/Dropdown.h"
static void UIDropdownModelChecks()
{
    DropdownConfig c;
    Validate(c);
    Reject(
        [&]
        {
            auto bad = c;
            bad.MaximumHeight = NAN;
            Validate(bad);
        });
    Reject(
        [&]
        {
            auto bad = c;
            bad.Placeholder = "\xed\xa0\x80";
            Validate(bad);
        });
    auto p = PlaceDropdown({{0, 0}, {320, 240}}, {{280, 200}, {40, 30}}, 160, 120, 4);
    Check(p.Above && p.Bounds.Position == glm::vec2(160, 76) &&
              p.Bounds.Size == glm::vec2(160, 120),
          "Dropdown edge flip/clamp independent geometry incorrect");
    p = PlaceDropdown({{0, 0}, {320, 240}}, {{20, 100}, {80, 30}}, 160, 200, 4);
    Check(!p.Above && p.Bounds.Position == glm::vec2(20, 134) && p.Bounds.Size.y == 106,
          "Dropdown constrained side policy incorrect");
    p = PlaceDropdown({{0, 0}, {100, 100}}, {{10, 10}, {40, 20}}, 200, 30, 4);
    Check(p.Bounds.Position == glm::vec2(0, 34) && p.Bounds.Size == glm::vec2(100, 30),
          "Dropdown width must fit owning surface");
    std::cout << "PASS: bounded dropdown configuration and independent popup "
                 "flip/clamp geometry\n";
}
