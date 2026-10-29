#include "Hazel/UI/UIActions.h"
static void UIActionChecks()
{
    using namespace Hazel;
    auto bindings=UIBindings::Defaults();
    Check(SerializeBindings(ParseBindings(SerializeBindings(bindings)))==SerializeBindings(bindings),"Binding round trip changed defaults");
    Reject([&]{auto b=bindings;b.Actions[size_t(UIAction::Cancel)].Key=Key::Enter;Validate(b);});
    Reject([&]{auto b=bindings;b.NeutralZone=b.DeadZone;Validate(b);});
    Reject([&]{auto b=bindings;b.RepeatInterval=NAN;Validate(b);});
    bool malformed=false;try{ParseBindings("Version: 9");}catch(const std::exception&){malformed=true;}Check(malformed,"Invalid override accepted");
    UIInputAdapter adapter;DeviceSample sample;InputRouter route;Action action;
    std::vector<Button> b{{1,{{0,0},{20,20}},{{0,0},{200,200}},"one",0},
                          {2,{{0,80},{20,20}},{{0,0},{200,200}},"two",1},
                          {3,{{80,0},{20,20}},{{0,0},{200,200}},"three",2},
                          {4,{{80,80},{20,20}},{{0,0},{200,200}},"four",3}};
    auto update=[&](float dt=.016f,bool enabled=true){auto f=adapter.Sample(sample,dt,enabled);route.Update(1,b,f);return f;};
    update();Check(route.Focused()==1,"Initial focus not first tab target");
    sample.Keys[Key::Right]=true;update();Check(route.Focused()==3,"Right used sequential navigation instead of geometry");
    sample.Keys[Key::Right]=false;update();sample.Keys[Key::Down]=true;update();Check(route.Focused()==4,"Down did not use geometric neighbor");
    sample.Keys[Key::Down]=false;update();sample.Keys[Key::Down]=true;update();Check(route.Focused()==4&&route.NavigationMisses()==1,"No-candidate navigation wrapped");
    sample.Keys.fill(false);update();sample.Keys[Key::Tab]=true;update();Check(route.Focused()==1,"Tab did not wrap in order");
    sample.Keys.fill(false);update();sample.Keys[Key::Tab]=sample.Keys[Key::LeftShift]=true;update();Check(route.Focused()==4,"Shift Tab order incorrect");
    // Ordered quick interactions, not only final polling state.
    sample.Keys.fill(false);update();adapter.KeyEdge(Key::Enter,true,false,false);adapter.KeyEdge(Key::Enter,false,false,false);adapter.KeyEdge(Key::Enter,true,false,false);adapter.KeyEdge(Key::Enter,false,false,false);update();
    Check(route.Consume(1,action)&&action.Entity==4&&route.Consume(1,action)&&!route.Consume(1,action),"Ordered quick Accept edges lost/duplicated");
    adapter.PointerEdge(0,true,{10,10});adapter.PointerEdge(0,false,{40,40});adapter.KeyEdge(Key::Enter,true,false,false);adapter.KeyEdge(Key::Enter,false,false,false);update();
    Check(!route.Consume(1,action),"Cancelled pointer release fell through to Accept");
    adapter.PointerEdge(0,true,{10,10});adapter.PointerEdge(0,false,{10,10});adapter.PointerEdge(0,true,{90,10});adapter.PointerEdge(0,false,{90,10});update();
    Check(route.Consume(1,action)&&action.Entity==1&&route.Consume(1,action)&&action.Entity==3&&!route.Consume(1,action),"Multiple ordered quick clicks were collapsed");
    sample.Keys[Key::Escape]=sample.Keys[Key::Enter]=true;update();
    Check(!route.Consume(1,action)&&route.ConsumeCancel(1,0)&&!route.ConsumeCancel(1,0),"Cancel did not suppress Accept / consume once");
    sample.Keys.fill(false);update();
    sample.Keys[Key::Enter]=true;update(.016f,false);update();Check(!route.Consume(1,action),"Held Accept on ownership restoration activated");
    for(int i=0;i<50;++i)update(.1f);Check(!route.Consume(1,action),"Held Accept repeated");
    sample.Keys.fill(false);update();sample.Pad.Device=0;sample.Pad.Buttons[size_t(GamepadButton::A)]=true;update();Check(!route.Consume(1,action),"Held reconnect A activated");
    sample.Pad.Buttons.fill(false);update();sample.Pad.Buttons[size_t(GamepadButton::A)]=true;update();Check(route.Consume(1,action),"Rearmed pad Accept failed");for(int i=0;i<12;++i)update(.1f);Check(!route.Consume(1,action),"Pad Accept repeated");
    sample.Pad.Buttons.fill(false);update();route.SetFocus(1,1,b);sample.Pad.Sticks[0]=.3f;update();Check(route.Focused()==1,"Stick jitter stole focus");
    sample.Pad.Sticks[0]=.7f;update();Check(route.Focused()==3,"Stick direction missing");route.SetFocus(1,1,b);
    update(.1f);update(.1f);update(.1f);Check(route.Focused()==1,"Navigation repeated before delay");update(.1f);Check(route.Focused()==3,"Held navigation did not repeat");
    sample.Pad.Sticks[0]=-.9f;route.SetFocus(1,4,b);update(.01f);Check(route.Focused()==4,"Stick changed direction without neutral rearm");
    sample.Pad.Sticks[0]=0;update();sample.Pad.Sticks[0]=-.9f;update();Check(route.Focused()==2,"Neutral-rearmed opposite direction failed");
    sample.Pad.Device=-1;sample.Pad.Sticks.fill(0);update();sample.Pad.Device=0;sample.Pad.Revision=2;sample.Pad.Sticks[0]=.9f;update();Check(route.Focused()==1,"Reconnect did not reset ownership / suppress held stick");
    // Effective clipped region and deterministic tab-rank ties.
    route.Reset();InputFrame f;f.Enabled=true;route.Update(2,b,f);b[2].Bounds={{80,-40},{20,20}};b[2].Clip={{0,0},{200,200}};
    f.Navigate={1,0};route.Update(2,b,f);Check(route.Focused()==4,"Fully clipped directional candidate admitted");
    b[2].Bounds={{80,0},{20,20}};b[3].Bounds=b[2].Bounds;route.SetFocus(2,1,b);route.Update(2,b,f);Check(route.Focused()==3,"Directional tie not broken by tab rank");
    route.SetScope(2,99);std::vector<Button> modal{{10,{{0,0},{20,20}},{{0,0},{200,200}},"modal"}};f.Navigate={};route.Update(2,modal,f);Check(route.Focused()==10,"Modal initial focus missing");
    route.SetScope(2,0);route.Update(2,b,f);Check(route.Focused()==1,"Router did not choose deterministic default; scene owns return-focus restoration");
    b[2].Interactable=false;route.Reconcile(2,b);Check(route.Focused()==1,"Retired focus did not choose first eligible target");
    route.SetScope(3,0);route.Update(3,b,f);Check(!route.ConsumeCancel(2,0)&&!route.Consume(2,action),"Scope/actions crossed scene identity");
    adapter.Cancel();sample={};route.Reset();update();
    sample.Keys[Key::Right]=sample.Keys[Key::Left]=true;update();Check(route.Focused()==1,"Opposing directions navigated");
    sample.Keys.fill(false);update();sample.Keys[Key::Right]=true;update();
    adapter.PointerEdge(0,true,{10,10});adapter.PointerEdge(0,false,{10,10});update();
    while(route.Consume(1,action)){}for(int i=0;i<10;++i)update(.1f);
    Check(route.Focused()==1,"Held navigation stole focus back after pointer ownership");
    adapter.Cancel();sample={};route.Reset();update();
    adapter.PointerEdge(0,true,{10,10});adapter.PointerEdge(0,false,{10,10});
    adapter.KeyEdge(Key::Right,true,false,false);adapter.KeyEdge(Key::Right,false,false,false);update();
    Check(route.Focused()==4,"Fresh ordered navigation after a quick click was suppressed");
    while(route.Consume(1,action)){}
    // Aggregate cancellation suppresses this sample, then synchronizes final held state.
    {InputRouter quick;InputFrame frame;frame.Enabled=true;quick.Update(77,b,frame);
        frame.Events={{UIAction::Cancel,true},{UIAction::Cancel,false}};
        quick.Update(77,b,frame);Check(quick.ConsumeCancel(77,0)&&!quick.ConsumeCancel(77,0),"Quick Cancel duplicated/lost");
        quick.Update(77,b,frame);Check(quick.ConsumeCancel(77,0),"Fresh quick Cancel in next sample stayed latched");}
    // Multiple physical bindings are one logical gesture until all are released.
    adapter.Cancel();sample={};route.Reset();update();
    adapter.KeyEdge(Key::Enter,true,false,false);sample.Keys[Key::Enter]=true;update();Check(route.Consume(1,action),"Primary Accept missing");
    adapter.KeyEdge(Key::Space,true,false,false);sample.Keys[Key::Space]=true;update();Check(!route.Consume(1,action),"Alternate held beside primary duplicated Accept");
    adapter.KeyEdge(Key::Enter,false,false,false);sample.Keys[Key::Enter]=false;update();Check(!route.Consume(1,action),"Releasing primary while alternate held reactivated");
    adapter.KeyEdge(Key::Space,false,false,false);sample.Keys[Key::Space]=false;update();
    sample.Keys[Key::LeftControl]=sample.Keys[Key::Enter]=true;update();sample.Keys[Key::LeftControl]=false;update();Check(!route.Consume(1,action),"Releasing Ctrl while Accept held activated UI");
    sample.Keys.fill(false);update();sample.Keys[Key::Tab]=true;update();auto tabFocus=route.Focused();sample.Keys[Key::LeftShift]=true;update();Check(route.Focused()==tabFocus,"Changing Shift with held Tab synthesized a new gesture");
    adapter.Configure(bindings);sample={};adapter.Sample(sample,0,true);
    for(int i=0;i<66;++i)adapter.KeyEdge(Key::Enter,i%2==0,false,false);
    auto overflow=adapter.Sample(sample,0,true);Check(!overflow.Enabled&&overflow.Events.empty()&&adapter.Dropped()>0,"Edge overflow partially routed gestures");
    std::cout<<"PASS: device-neutral ordered edges, spatial/tab focus, scope reconciliation, pad neutral/repeat/disconnect, once activation, binding validation and cancellation\n";
}
