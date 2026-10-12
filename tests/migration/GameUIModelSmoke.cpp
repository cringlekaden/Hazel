#include "Hazel/UI/RuntimeUIModel.h"
#include <iostream>
#include <limits>
#include <stdexcept>
#include <cmath>
using namespace Hazel::GameUI;
static void Check(bool value, const char *why)
{
    if (!value)
        throw std::runtime_error(why);
}
static bool Same(float a, float b)
{
    return std::abs(a - b) < .001f;
}
template <class F> void Reject(F f)
{
    bool bad = false;
    try
    {
        f();
    }
    catch (const std::invalid_argument &)
    {
        bad = true;
    }
    Check(bad, "Malformed UI configuration was accepted");
}
int main()
{
    try
    {
        for (auto viewport : std::vector<glm::vec2>{{1024, 600}, {1280, 720}, {960, 720}})
        {
            auto fit = FitCanvas(viewport, {});
            Check(Same(fit.Bounds.Size.x, 1280), "Canvas reference width changed");
            RectLayout anchored;
            anchored.AnchorMin = anchored.AnchorMax = {1, 1};
            anchored.Pivot = {1, 1};
            anchored.Offset = {-24, -24};
            anchored.SizeDelta = {240, 48};
            auto button = ResolveRect(fit.Content, anchored);
            Check(Same(button.Position.x + button.Size.x,
                       fit.Content.Position.x + fit.Content.Size.x - 24),
                  "Right margin changed after resize");
            Check(Same(button.Position.y + button.Size.y,
                       fit.Content.Position.y + fit.Content.Size.y - 24),
                  "Bottom margin changed after resize");
            Check(glm::length(fit.ToLogical((button.Position + button.Size * .5f) * fit.Scale) -
                              (button.Position + button.Size * .5f)) < .001f,
                  "Pointer mapping differs from drawn rectangle");
            RectLayout stretch;
            stretch.AnchorMax = {1, 1};
            stretch.SizeDelta = {-40, -60};
            stretch.Offset = {20, 30};
            auto panel = ResolveRect(fit.Content, stretch);
            Check(Same(panel.Size.x, fit.Content.Size.x - 40) &&
                      Same(panel.Position.y, fit.Content.Position.y + 30),
                  "Stretched anchors lost offsets");
        }
        Check(FitCanvas({0, 600}, {}).Scale == 0, "Minimized canvas did not suspend dimensions");
        Reject(
            []
            {
                CanvasLayout c;
                c.ReferenceSize.x = 0;
                FitCanvas({100, 100}, c);
            });
        Reject(
            []
            {
                RectLayout c;
                c.AnchorMin = {1, 0};
                ResolveRect({{0, 0}, {100, 100}}, c);
            });
        Reject(
            []
            {
                RectLayout c;
                c.Margin.x = -1;
                Validate(c);
            });
        Reject(
            []
            {
                RectLayout c;
                c.Offset.y = std::numeric_limits<float>::quiet_NaN();
                Validate(c);
            });
        std::vector<FlowItem> items(3);
        items[0].PreferredSize = {20, 40};
        items[1].Visible = false;
        items[2].PreferredSize = {30, 50};
        items[2].Flex = 1;
        FlowLayout flow;
        flow.Gap = 10;
        std::vector<Rect> layout;
        ArrangeFlow({{10, 20}, {200, 200}}, flow, items, layout);
        Check(layout[1].Size == glm::vec2(0) && Same(layout[0].Position.y, 20) &&
                  Same(layout[2].Position.y, 70) && Same(layout[2].Size.y, 150),
              "Hidden row consumed a gap or flex space lost");
        flow.Direction = Axis::Horizontal;
        flow.Main = Align::End;
        flow.Cross = Align::Center;
        items[2].Flex = 0;
        ArrangeFlow({{0, 0}, {200, 100}}, flow, items, layout);
        Check(Same(layout[0].Position.x, 140) && Same(layout[2].Position.x, 170) &&
                  Same(layout[0].Position.y, 30),
              "Horizontal alignment differs from declared padding/gaps");
        Check(!Rect{{0, 0}, {10, 10}}.Contains({10, 5}) && Rect{{0, 0}, {10, 10}}.Contains({0, 0}),
              "Adjacent button edges overlap");
        Check(Rect{{0, 0}, {10, 10}}.Intersect({{20, 20}, {10, 10}}).Size == glm::vec2(0),
              "Disjoint clip has a hit region");
        std::vector<Button> buttons = {
            {1, {{0, 0}, {100, 40}}, {{0, 0}, {200, 100}}, "begin", 0, true, true},
            {2, {{0, 0}, {100, 40}}, {{0, 0}, {50, 40}}, "continue", 1, true, true},
            {3, {{110, 0}, {90, 40}}, {{0, 0}, {200, 100}}, "locked", 2, true, false}};
        InputRouter router;
        InputFrame input;
        input.Enabled = true;
        input.Mouse = {25, 20};
        Action action;
        router.Update(41, buttons, input);
        Check(router.Hovered() == 2 && router.Focused() == 1,
              "Topmost mouse hit or tab order wrong");
        input.PointerPressed = input.PointerReleased = true;
        input.PressPosition = input.Mouse;
        router.Update(41, buttons, input);
        Check(router.Consume(41, action) && action.Entity == 2 && !router.Consume(41, action),
              "Quick click between updates was lost or doubled");
        input.PointerPressed = input.PointerReleased = false;
        input.LeftDown = true;
        router.Update(41, buttons, input);
        Check(!router.Consume(41, action) && router.State(buttons[1]) == ButtonState::Pressed,
              "Mouse activated on press");
        input.AcceptDown = true;
        router.Update(41, buttons, input);
        Check(!router.Consume(41, action), "Keyboard duplicated an active pointer gesture");
        input.AcceptDown = false;
        router.Update(41, buttons, input);
        input.Mouse = {75, 20};
        input.LeftDown = false;
        router.Update(41, buttons, input);
        Check(!router.Consume(41, action), "Release outside clipped captured button activated");
        input.Mouse = {25, 20};
        input.LeftDown = true;
        router.Update(41, buttons, input);
        input.LeftDown = false;
        input.AcceptDown = true;
        router.Update(41, buttons, input);
        Check(router.Consume(41, action) && action.Entity == 2 && action.Name == "continue" &&
                  !router.Consume(41, action),
              "Mouse/keyboard duplicated activation");
        router.Update(41, buttons, input);
        Check(!router.Consume(41, action), "Held activation repeated");
        input.AcceptDown = false;
        input.NextDown = true;
        router.Update(41, buttons, input);
        Check(router.Focused() == 1, "Tab did not wrap past disabled button");
        input.NextDown = false;
        input.PreviousDown = true;
        router.Update(41, buttons, input);
        Check(router.Focused() == 2, "Reverse tab order wrong");
        Check(!router.SetFocus(41, 3, buttons) && !router.SetFocus(40, 1, buttons),
              "Disabled or retired focus accepted");
        input.PreviousDown = false;
        input.LeftDown = true;
        router.Update(41, buttons, input);
        buttons[1].Interactable = false;
        input.LeftDown = false;
        router.Update(41, buttons, input);
        Check(!router.Consume(41, action) && router.State(buttons[1]) == ButtonState::Disabled,
              "Disabled capture still activated");
        input.AcceptDown = true;
        router.Update(41, buttons, input);
        Check(!router.Consume(40, action), "Retired scene consumed current action");
        router.Update(42, buttons, input);
        Check(!router.Consume(41, action) && !router.Consume(42, action),
              "Transition retained work or held key activated new scene");
        Check(router.State(buttons[0]) != ButtonState::Pressed,
              "Retired held key left pressed feedback in new scene");
        input.AcceptDown = false;
        router.Update(42, buttons, input);
        input.AcceptDown = true;
        router.Update(42, buttons, input);
        input.Enabled = false;
        router.Update(42, buttons, input);
        Check(!router.Consume(42, action) && router.Focused() == 0,
              "Inactive viewport retained actions/focus");
        input.Enabled = true;
        router.Update(42, buttons, input);
        Check(!router.Consume(42, action), "Reactivating viewport replayed held input");
        input.AcceptDown = false;
        router.Update(42, buttons, input);
        input.AcceptDown = true;
        router.Update(42, buttons, input);
        buttons[0].Visible = false;
        router.Update(42, buttons, input);
        Check(!router.Consume(42, action), "Hidden button left delayed work");
        Reject(
            [&]
            {
                buttons[2].ID = 2;
                router.Update(42, buttons, input);
            });
        buttons[2].ID = 3;
        buttons[0].Visible = true;
        input.AcceptDown = false;
        router.Update(42, buttons, input);
        input.AcceptDown = true;
        router.Update(42, buttons, input);
        router.Retire(42, 1);
        Check(!router.Consume(42, action) && router.Focused() == 0,
              "Entity retirement between dispatch and consumption retained work");
        router.Reset();
        Check(router.Focused() == 0 && !router.Consume(42, action),
              "Stop retained UI observations");
        std::cout << "PASS: backend-independent canvas/anchors/flow/clips, capture/release, "
                     "focus/disabled, once-only actions, viewport gating and scene retirement\n";
    }
    catch (const std::exception &e)
    {
        std::cerr << "FAIL: " << e.what() << '\n';
        return 1;
    }
}
