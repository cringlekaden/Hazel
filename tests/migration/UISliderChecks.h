#include "Hazel/UI/Slider.h"
static void UISliderChecks()
{
    SteppedRange r{0, 10, 3};
    Check(r.LastIndex() == 4 && r.At(3) == 9 && r.At(4) == 10,
          "Nondivisible range lost exact endpoint");
    Check(r.Quantize(1.5f) == 3 && r.Quantize(9.5f) == 10 && r.Quantize(-1) == 0 &&
              r.Quantize(99) == 10,
          "Slider tie/clamp policy changed");
    r = {-1, 1, .25f};
    Check(r.Index(0) == 4 && r.At(4) == 0, "Signed slider lattice incorrect");
    for (auto bad : {SteppedRange{0, 0, 1}, SteppedRange{0, 1, 0}, SteppedRange{0, 1, -1},
                     SteppedRange{0, 1, 1e-7f}, SteppedRange{0, 1, 1e-6f},
                     SteppedRange{1e8f, 1e8f + 64, 1}, SteppedRange{-1e10f, 1e10f, 1},
                     SteppedRange{0, 1, std::numeric_limits<float>::infinity()}})
        Reject([&] { bad.Validate(); });
    Reject([&] { r.Quantize(std::numeric_limits<float>::quiet_NaN()); });
    for (unsigned d = 0; d < 4; ++d)
    {
        auto g = ArrangeSlider({{0, 0}, {200, 100}}, {{10, 10}, {180, 80}}, {20, 20}, .5f,
                               FillDirection(d));
        Check(g.Thumb.Position == glm::vec2(90, 40), "Centered slider thumb not centered");
        auto near = g.Thumb.Position;
        near[g.Axis] = g.Start - 1000;
        auto far = near;
        far[g.Axis] = g.Start + g.Travel + 1000;
        Check(g.Position(near) == (g.Reverse ? 1 : 0) && g.Position(far) == (g.Reverse ? 0 : 1),
              "Outside drag does not clamp/reverse");
        auto p = g.Thumb.Position + g.Thumb.Size * .5f;
        p[g.Axis] += 3;
        Check(Same(g.Position(p, 3), .5f), "Thumb press offset jumped");
    }
    auto zero =
        ArrangeSlider({{0, 0}, {10, 10}}, {{0, 0}, {10, 10}}, {30, 30}, .2f, FillDirection::Right);
    Check(zero.Travel == 0 && zero.Thumb.Size == glm::vec2(10),
          "Zero travel created invalid bounds");
    using Hazel::GameUI::DeviceSample;
    using Hazel::GameUI::UIInputAdapter;
    UIInputAdapter adapter;
    DeviceSample sample;
    adapter.Sample(sample, 0, true);
    sample.Keys[Hazel::Key::Right] = true;
    auto f = adapter.Sample(sample, 0, true, true);
    Check(f.Events.empty(), "Context switch treated held navigation as Adjust");
    sample.Keys.fill(false);
    adapter.Sample(sample, 0, true, true);
    sample.Keys[Hazel::Key::Right] = true;
    f = adapter.Sample(sample, 0, true, true);
    Check(f.Events.size() == 1 && f.Events[0].Action == UIAction::AdjustIncrease,
          "Adjust default missing or also navigated");
    f = adapter.Sample(sample, 10, true, true);
    Check(f.Events.size() <= 1, "Large timestep caused adjustment burst");
    auto bindings = UIBindings::Defaults();
    bindings.Actions[size_t(UIAction::AdjustIncrease)].Key = Hazel::Key::K;
    Validate(bindings);
    bindings.Actions[size_t(UIAction::Accept)].Key = Hazel::Key::K;
    Reject([&] { Validate(bindings); });
    std::cout << "PASS: slider quantization, geometry and contextual remappable "
                 "Adjust\n";
}
