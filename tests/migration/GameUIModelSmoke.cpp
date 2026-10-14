#include "Hazel/UI/RuntimeUIModel.h"
#include "Hazel/Utils/UTF8.h"
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
static void HardeningChecks()
{
    // Audit D2: nonempty bounds/clip separately, but no effective intersection.
    std::vector<Button> clipped{{1,{{100,100},{50,30}},{{0,0},{50,30}},"offscreen"}};
    InputRouter router;InputFrame input;input.Enabled=true;Action action;
    router.Update(1,clipped,input);input.AcceptDown=true;router.Update(1,clipped,input);
    Check(router.Focused()==0&&!router.Consume(1,action),"D2: completely clipped button focused/activated");
    clipped[0].Bounds.Size={0,30};router.Update(1,clipped,input);
    Check(!router.SetFocus(1,1,clipped),"Zero-size button accepted focus");
    auto tiny=FramebufferCoverage({{.1f,.1f},{.4f,.4f}},{1.25f,1.25f},{100,100});
    Check(tiny.Size==glm::vec2(0),"Fractional clip gained framebuffer coverage");
    auto covered=FramebufferCoverage({{.1f,.1f},{2,2}},{1.25f,1.25f},{100,100});
    Check(covered.Position==glm::vec2(1)&&covered.Size==glm::vec2(1),"Framebuffer clip did not snap inward");
    // Audit D5: gesture owns the release frame even when capture is cancelled.
    for(bool retire:{false,true})
    {
        InputRouter r;std::vector<Button> b{{1,{{0,0},{100,40}},{{0,0},{100,40}},"activate"}};
        InputFrame f;f.Enabled=true;f.Mouse={10,10};r.Update(2,b,f);
        f.LeftDown=true;r.Update(2,b,f);if(retire)r.Retire(2,1);
        f.LeftDown=false;f.Mouse={120,10};f.AcceptDown=true;f.AcceptPressed=true;r.Update(2,b,f);
        Check(!r.Consume(2,action),"D5: cancelled pointer release became keyboard activation");
        f.AcceptDown=f.AcceptPressed=false;r.Update(2,b,f);f.AcceptDown=true;r.Update(2,b,f);
        Check(r.Consume(2,action),"Independent Accept after cancellation was suppressed");
    }
    {
        InputRouter r;std::vector<Button> b{{1,{{0,0},{100,40}},{{0,0},{100,40}},"lower"},
            {2,{{0,0},{100,40}},{{0,0},{100,40}},"disabled",1,true,false}};
        InputFrame f;f.Enabled=true;f.Mouse={10,10};r.Update(3,b,f);
        f.PointerPressed=f.PointerReleased=true;f.PressPosition=f.Mouse;r.Update(3,b,f);
        Check(!r.Consume(3,action),"Disabled control let a click activate a lower control");
        b.back().Hit=HitPolicy::PassThrough;f.PointerPressed=f.PointerReleased=false;r.Update(3,b,f);
        f.PointerPressed=f.PointerReleased=true;r.Update(3,b,f);
        Check(r.Consume(3,action)&&action.Entity==1,"Explicit pass-through blocked lower control");
    }
    {
        InputRouter r;std::vector<Button> b{{1,{{0,0},{100,40}},{{0,0},{100,40}},"activate"}};
        InputFrame f;f.Enabled=true;r.Update(4,b,f,1);
        for(size_t i=0;i<InputRouter::MaxActions+5;++i)
        {f.AcceptDown=false;r.Update(4,b,f,1);f.AcceptDown=true;r.Update(4,b,f,1);}
        Check(r.DroppedActions()==5,"Model queue overflow did not count drop-newest");
        size_t delivered=0;while(r.Consume(4,action))++delivered;
        Check(delivered==InputRouter::MaxActions,"Bounded queue changed retained activation count");
    }
    // Audit D3: exact scalar decoding, byte offsets and format policy.
    std::vector<uint32_t> chars;std::vector<size_t> bytes;
    Hazel::UTF8::Decode(std::string("\xef\xbb\xbf")+u8"Aé😀",chars,&bytes);
    Check(chars==std::vector<uint32_t>{'A',0xe9,0x1f600}&&bytes==std::vector<size_t>{3,4,6,10},"Leading BOM/scalar offsets changed");
    Hazel::UTF8::Decode("\xef\xbb\xbf",chars,&bytes);
    Check(chars.empty()&&bytes==std::vector<size_t>{3},"BOM-only text did not become empty");
    Hazel::UTF8::Decode(std::string("A\xef\xbb\xbf"),chars);
    Check(chars.size()==2&&chars[1]==0xfeff,"Nonleading BOM scalar was not retained");
    Hazel::UTF8::Decode("\xf4\x8f\xbf\xbf",chars);Check(chars[0]==0x10ffff,"Last Unicode scalar rejected");
    for(auto invalid:std::vector<std::string>{std::string("A\0B",3),"\xc0\xaf","\xe0\x80\xaf","\xed\xa0\x80","\xf4\x90\x80\x80","\xf5\x80\x80\x80","\x80","\xc2","\xe2\x28\xa1","\xff"})
        Reject([&]{Hazel::UTF8::Decode(invalid,chars);});
    // D6: invert resolved edges; negative deltas and minimum-clamped authored sizes remain valid.
    Rect parent{{20,30},{800,600}};
    for(auto stretch:{false,true})for(auto pivot:std::vector<glm::vec2>{{0,0},{.5f,.5f},{1,1}})
    {
        RectLayout r;r.AnchorMin={.1f,.1f};r.AnchorMax=stretch?glm::vec2(.9f):r.AnchorMin;
        r.SizeDelta=stretch?glm::vec2(-100,-80):glm::vec2(140,80);r.Pivot=pivot;
        r.Margin={3,4,5,6};r.MinimumSize={20,20};auto start=ResolveRect(parent,r);
        for(unsigned edges:{0u,1u,2u,4u,8u,3u,12u})
        {
            auto edited=EditRect(parent,r,{15,12},edges);auto actual=ResolveRect(parent,edited);
            if(!edges){Check(glm::length(actual.Position-start.Position-glm::vec2(15,12))<.001f&&actual.Size==start.Size,"Move changed resolved size");Check(edited.SizeDelta==r.SizeDelta,"Move rewrote size delta");}
            if(edges&1)Check(Same(actual.Position.x+actual.Size.x,start.Position.x+start.Size.x),"Left resize moved opposite edge");
            if(edges&2)Check(Same(actual.Position.y+actual.Size.y,start.Position.y+start.Size.y),"Top resize moved opposite edge");
            if(edges&4)Check(Same(actual.Position.x,start.Position.x),"Right resize moved opposite edge");
            if(edges&8)Check(Same(actual.Position.y,start.Position.y),"Bottom resize moved opposite edge");
            if(stretch)Check(edited.SizeDelta.x<0&&edited.SizeDelta.y<0,"Resize destroyed valid negative stretch delta");
        }
    }
    RectLayout minimum;minimum.SizeDelta={-50,-50};minimum.MinimumSize={40,30};minimum.Pivot={1,1};
    auto moved=EditRect(parent,minimum,{10,10},0);Check(moved.SizeDelta==minimum.SizeDelta,"Minimum-constrained move changed authored size");
    auto clamped=EditRect(parent,minimum,{100,100},3);Check(ResolveRect(parent,clamped).Size==minimum.MinimumSize,"Resize escaped minimum");
    Reject([&]{EditRect(parent,minimum,{NAN,0},4);});
    std::cout<<"PASS: D2/D5, clipped/fractional hits, disabled blockers, queue limits, strict scalars and legacy resolved-edge edits\n";
}
int main()
{
    try
    {
        HardeningChecks();
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
        Check(!router.SetFocus(41,2,{}),"Stale indexed focus accessed a replaced button list");
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
        {
            InputRouter route;std::vector<Button> controls{{1,{{0,0},{100,40}},{{0,0},{100,40}},"menu"}};InputFrame input;input.Enabled=true;input.KeyboardEnabled=false;
            route.Update(100,controls,input);input.AcceptDown=true;route.Update(100,controls,input);Action action;
            Check(!route.Consume(100,action),"Gameplay Space activated nonmodal UI");
            input.KeyboardEnabled=true;route.Update(100,controls,input);Check(!route.Consume(100,action),"Held gameplay key activated newly modal UI");
            input.AcceptDown=false;route.Update(100,controls,input);input.AcceptDown=true;route.Update(100,controls,input);Check(route.Consume(100,action),"UI keyboard ownership did not activate");
        }
        {
            InputRouter route;std::vector<Button> controls{{1,{{0,0},{100,40}},{{0,0},{100,40}},"play"}};InputFrame input;input.Enabled=true;input.Mouse={10,10};route.Update(111,controls,input);
            input.PointerPressed=true;input.PointerReleased=true;input.PressPosition={10,10};input.HasReleasePosition=true;input.ReleasePosition={120,10};route.Update(111,controls,input);Action action;Check(!route.Consume(111,action),"Release outside became a click after pointer moved inside");
            input.PointerPressed=input.PointerReleased=false;route.Update(111,controls,input);input.PointerPressed=input.PointerReleased=true;input.ReleasePosition={10,10};input.Mouse={120,10};route.Update(111,controls,input);Check(route.Consume(111,action),"Valid release inside was lost after pointer moved outside");
        }
        std::cout << "PASS: backend-independent canvas/anchors/flow/clips, capture/release, "
                     "focus/disabled, once-only actions, viewport gating and scene retirement\n";
    }
    catch (const std::exception &e)
    {
        std::cerr << "FAIL: " << e.what() << '\n';
        return 1;
    }
}
