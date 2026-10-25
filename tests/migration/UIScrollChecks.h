#include "Hazel/UI/ScrollView.h"
static void UIScrollChecks()
{
    using namespace Hazel;
    auto thumb = ScrollThumb({{10, 20}, {8, 100}}, 100, 400, 150, 16, 1);
    Check(thumb.Position == glm::vec2(10, 57.5f) && thumb.Size == glm::vec2(8, 25),
          "Proportional scrollbar midpoint incorrect");
    Check(ScrollThumb({{0, 0}, {100, 10}}, 5, 1000, 995, 16, 0).Position.x == 84,
          "Minimum thumb/end mapping incorrect");
    Check(ScrollThumb({{0, 0}, {0, 10}}, 0, 0, 0, 16, 0).Size.x == 0,
          "Zero travel emitted invalid thumb");
    Check(RevealDelta({{0, 0}, {100, 100}}, {{0, 110}, {20, 20}}, 4, 1) == 34,
          "Minimal reveal did not include margin");
    Check(RevealDelta({{0, 0}, {100, 100}}, {{0, -10}, {20, 200}}, 4, 1) == -14,
          "Oversized reveal must align leading edge");
    Check(RevealDelta({{0, 0}, {100, 100}}, {{0, 4}, {20, 200}}, 4, 1) == 0,
          "Oversized reveal oscillates");
    ScrollConfig config;
    Validate(config);
    Reject([&] {
        auto bad = config;
        bad.PixelsPerUnit = NAN;
        Validate(bad);
    });
    UIInputAdapter adapter;
    DeviceSample sample;
    adapter.Sample(sample, 0, true);
    adapter.ScrollEdge({.25f, -.5f}, {10, 20});
    adapter.ScrollEdge({-.125f, .25f}, {30, 40});
    auto f = adapter.Sample(sample, .016f, true);
    Check(f.Events.size() == 2 && f.Events[0].Action == UIAction::Scroll &&
              f.Events[0].Delta == glm::vec2(.25f, .5f) &&
              f.Events[1].Delta == glm::vec2(-.125f, -.25f) &&
              f.Events[1].Point == glm::vec2(30, 40),
          "Ordered fractional/two-axis wheel data lost");
    adapter.ScrollEdge({0, -1}, {});
    f = adapter.Sample(sample, .016f, false);
    Check(f.Events.empty(), "Ownership loss delivered buffered wheel");
    f = adapter.Sample(sample, .016f, true);
    Check(f.Events.empty(), "Ownership restore replayed wheel");
    auto bindings = UIBindings::Defaults();
    bindings.Actions[size_t(UIAction::ScrollDown)].Key = Key::K;
    adapter.Configure(bindings);
    adapter.Sample(sample, 0, true);
    sample.Keys[Key::K] = true;
    f = adapter.Sample(sample, .016f, true);
    Check(f.Events.size() == 1 && f.Events[0].Action == UIAction::ScrollDown,
          "Remapped Scroll action did not route");
    std::cout << "PASS: proportional thumbs, reveal margins/oversize, ordered fractional wheel and "
                 "remappable Scroll\n";
}
