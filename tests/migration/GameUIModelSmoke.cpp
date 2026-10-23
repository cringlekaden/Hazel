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
#include "UIActionChecks.h"
#include "MeasuredLayoutChecks.h"
#include "GridLayoutChecks.h"
#include "WrapLayoutChecks.h"
#include "ImageFitChecks.h"
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
    std::cout<<"PASS: D2/D5 clipped/fractional hits, queue limits and strict Unicode scalars\n";
}
int main()
{
    try
    {
        UIActionChecks();
        HardeningChecks();
        MeasuredLayoutChecks();
        GridLayoutChecks();
        WrapLayoutChecks();
        ImageFitChecks();
        Check(FitCanvas({0,600},{}).Scale==0,"Minimized canvas did not suspend layout");
        Reject([]{RectLayout r;r.Margin.x=-1;Validate(r);});
        Reject([]{RectLayout r;r.Offset.y=NAN;Validate(r);});
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
