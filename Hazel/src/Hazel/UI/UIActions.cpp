#include "hzpch.h"
#include "UIActions.h"
#include "Hazel/Core/DocumentSchema.h"
#include <yaml-cpp/yaml.h>
#include <cmath>

namespace Hazel::GameUI {
namespace {
constexpr size_t N=size_t(UIAction::Count);
bool Navigation(size_t a){return a>=size_t(UIAction::Up)&&a<=size_t(UIAction::Previous);}
bool ShiftOK(ShiftMatch match,bool shift){return match==ShiftMatch::Any||(match==ShiftMatch::With)==shift;}
bool ValidKey(int k){if(k==Key::LeftControl||k==Key::RightControl||k==Key::LeftAlt||k==Key::RightAlt||k==Key::LeftSuper||k==Key::RightSuper)return false;return k==-1||(k==32||k==39||(k>=44&&k<=57)||k==59||k==61||(k>=65&&k<=93)||k==96)||(k>=161&&k<=162)||(k>=256&&k<=269)||(k>=280&&k<=284)||(k>=290&&k<=314)||(k>=320&&k<=336)||(k>=340&&k<=348);}
}
const char* ActionName(UIAction a){static const char* names[]={"Press","Up","Down","Left","Right","Next","Previous","Accept","Cancel"};return size_t(a)<N?names[size_t(a)]:"None";}
std::string KeyName(int k){if(k==-1)return "None";if(k>=Key::F1&&k<=Key::F25)return "F"+std::to_string(k-Key::F1+1);if(k>=Key::KP0&&k<=Key::KP9)return "Keypad "+std::to_string(k-Key::KP0);if(k>=33&&k<=96)return std::string(1,char(k));switch(k){case Key::LeftShift:return "Left Shift";case Key::RightShift:return "Right Shift";case Key::Backspace:return "Backspace";case Key::Delete:return "Delete";case Key::Home:return "Home";case Key::End:return "End";case Key::PageUp:return "Page Up";case Key::PageDown:return "Page Down";case Key::Space:return "Space";case Key::Enter:return "Enter";case Key::Escape:return "Escape";case Key::Tab:return "Tab";case Key::Up:return "Up";case Key::Down:return "Down";case Key::Left:return "Left";case Key::Right:return "Right";default:return "Key "+std::to_string(k);}}
UIBindings UIBindings::Defaults(){
    UIBindings b;
    auto set=[&](UIAction a,int key,int alt,int pad,ShiftMatch shift=ShiftMatch::Any){b.Actions[size_t(a)]={key,alt,pad,shift};};
    set(UIAction::Up,Key::Up,-1,int(GamepadButton::Up));set(UIAction::Down,Key::Down,-1,int(GamepadButton::Down));
    set(UIAction::Left,Key::Left,-1,int(GamepadButton::Left));set(UIAction::Right,Key::Right,-1,int(GamepadButton::Right));
    set(UIAction::Next,Key::Tab,-1,-1,ShiftMatch::Without);set(UIAction::Previous,Key::Tab,-1,-1,ShiftMatch::With);
    set(UIAction::Accept,Key::Enter,Key::Space,int(GamepadButton::A));set(UIAction::Cancel,Key::Escape,-1,int(GamepadButton::B));
    return b;
}
void Validate(const UIBindings& b){
    if(b.PointerButton<0||b.PointerButton>7||b.Stick< -1||b.Stick>1||!std::isfinite(b.DeadZone)||!std::isfinite(b.NeutralZone)||!std::isfinite(b.RepeatDelay)||!std::isfinite(b.RepeatInterval)||b.NeutralZone<0||b.NeutralZone>=b.DeadZone||b.DeadZone>1||b.RepeatDelay<.1f||b.RepeatDelay>2||b.RepeatInterval<.03f||b.RepeatInterval>1)throw std::invalid_argument("UI bindings: invalid pointer, stick, dead zone or repeat settings");
    for(size_t a=0;a<N;++a){const auto& x=b.Actions[a];
        if(!ValidKey(x.Key)||!ValidKey(x.Alternate)||x.Pad< -1||x.Pad>=int(GamepadButton::Count)||int(x.Shift)<0||int(x.Shift)>2)throw std::invalid_argument(std::string("UI binding invalid: ")+ActionName(UIAction(a)));
        if(a==0&&(x.Key!=-1||x.Alternate!=-1||x.Pad!=-1))throw std::invalid_argument("Press uses the pointer button; keyboard/gamepad use Accept");
        for(size_t c=1;c<a;++c){auto& y=b.Actions[c];bool shiftOverlap=x.Shift==ShiftMatch::Any||y.Shift==ShiftMatch::Any||x.Shift==y.Shift;
            bool keyConflict=false;for(int k:{x.Key,x.Alternate})if(k!=-1&&(k==y.Key||k==y.Alternate))keyConflict=true;
            if((shiftOverlap&&keyConflict)||(x.Pad!=-1&&x.Pad==y.Pad))throw std::invalid_argument(std::string("UI binding conflict: ")+ActionName(UIAction(c))+" / "+ActionName(UIAction(a)));
        }
    }
    for(auto a:{UIAction::Next,UIAction::Previous,UIAction::Accept,UIAction::Cancel})if(b.Actions[size_t(a)].Key==-1&&b.Actions[size_t(a)].Alternate==-1)throw std::invalid_argument(std::string("UI keyboard binding required: ")+ActionName(a));
}
std::string SerializeBindings(const UIBindings& b){Validate(b);YAML::Emitter out;
    out<<YAML::BeginMap<<YAML::Key<<"Version"<<YAML::Value<<1<<YAML::Key<<"PointerButton"<<YAML::Value<<b.PointerButton<<YAML::Key<<"Stick"<<YAML::Value<<b.Stick<<YAML::Key<<"DeadZone"<<YAML::Value<<b.DeadZone<<YAML::Key<<"NeutralZone"<<YAML::Value<<b.NeutralZone<<YAML::Key<<"RepeatDelay"<<YAML::Value<<b.RepeatDelay<<YAML::Key<<"RepeatInterval"<<YAML::Value<<b.RepeatInterval<<YAML::Key<<"Actions"<<YAML::Value<<YAML::BeginMap;
    for(size_t a=1;a<N;++a){auto& x=b.Actions[a];out<<YAML::Key<<ActionName(UIAction(a))<<YAML::Value<<YAML::Flow<<YAML::BeginSeq<<x.Key<<x.Alternate<<x.Pad<<int(x.Shift)<<YAML::EndSeq;}
    out<<YAML::EndMap<<YAML::EndMap;if(!out.good())throw std::runtime_error(out.GetLastError());return out.c_str();
}
UIBindings ParseBindings(const std::string& text){
    if(text.size()>8192)throw std::invalid_argument("UI bindings exceed 8192 bytes");auto n=YAML::Load(text);DocumentSchema::Structure(n);
    DocumentSchema::Keys(n,{"Version","PointerButton","Stick","DeadZone","NeutralZone","RepeatDelay","RepeatInterval","Actions"},"UIBindings");
    if(n["Version"].as<int>()!=1)throw std::invalid_argument("Unsupported UI binding Version");
    auto b=UIBindings::Defaults();b.PointerButton=n["PointerButton"].as<int>();b.Stick=n["Stick"].as<int>();b.DeadZone=n["DeadZone"].as<float>();b.NeutralZone=n["NeutralZone"].as<float>();b.RepeatDelay=n["RepeatDelay"].as<float>();b.RepeatInterval=n["RepeatInterval"].as<float>();
    DocumentSchema::Keys(n["Actions"],{"Up","Down","Left","Right","Next","Previous","Accept","Cancel"},"UIBindings.Actions");
    for(size_t a=1;a<N;++a){auto x=n["Actions"][ActionName(UIAction(a))];if(!x.IsSequence()||x.size()!=4)throw std::invalid_argument(std::string("Missing/invalid UI binding ")+ActionName(UIAction(a)));int shift=x[3].as<int>();if(shift<0||shift>2)throw std::invalid_argument("Invalid UI shift match");b.Actions[a]={x[0].as<int>(),x[1].as<int>(),x[2].as<int>(),ShiftMatch(shift)};}
    Validate(b);return b;
}
void UIInputAdapter::Configure(const UIBindings& b){Validate(b);m_Bindings=b;Cancel();}
void UIInputAdapter::Cancel(){
    m_LastAction=UIAction::Count;m_Device=InputDevice::Pointer;
    m_Ready=m_PadReady=false;m_Pad=-1;m_StickDirection=-1;m_Edges.clear();
    m_Held.fill(false);m_Armed.fill(false);m_Repeating.fill(false);m_Sources.fill(0);m_Repeat.fill(0);m_Overflow=false;
}
void UIInputAdapter::Queue(ActionEdge e,uint8_t source){
    if(m_Edges.size()<MaxEdges)m_Edges.push_back({e,source});else {++m_Dropped;m_Overflow=true;}
}
void UIInputAdapter::KeyEdge(int key,bool down,bool repeat,bool shift,bool command){
    if(repeat||command)return;
    for(size_t a=1;a<N;++a){auto& b=m_Bindings.Actions[a];
        if((key==b.Key||key==b.Alternate)&&ShiftOK(b.Shift,shift))
            Queue({UIAction(a),down,{},InputDevice::Keyboard},uint8_t((key==b.Key?1:0)|(key==b.Alternate?2:0)));
    }
}
void UIInputAdapter::PointerEdge(int button,bool down,glm::vec2 point){
    if(button==m_Bindings.PointerButton)Queue({UIAction::Press,down,point,InputDevice::Pointer});
}
InputFrame UIInputAdapter::Sample(const DeviceSample& s,float seconds,bool enabled){
    InputFrame f;f.Mouse=s.Point;f.Enabled=enabled;
    if(!std::isfinite(seconds)||seconds<0)throw std::invalid_argument("UI input timestep must be finite and nonnegative");
    if(!enabled||m_Overflow){if(m_Overflow)++m_Dropped;Cancel();f.Enabled=false;return f;}
    const bool shift=s.Keys[Key::LeftShift]||s.Keys[Key::RightShift];
    const bool command=s.Keys[Key::LeftControl]||s.Keys[Key::RightControl]||s.Keys[Key::LeftAlt]||s.Keys[Key::RightAlt]||s.Keys[Key::LeftSuper]||s.Keys[Key::RightSuper];
    if(command){Cancel();f.Enabled=false;return f;}
    std::array<bool,N> held{}, keyboard{},pad{},physical{};
    std::array<uint8_t,N> sources{};
    held[0]=physical[0]=s.Mouse[size_t(m_Bindings.PointerButton)];sources[0]=held[0]?1:0;
    for(size_t a=1;a<N;++a){auto& b=m_Bindings.Actions[a];
        const bool first=b.Key>=0&&s.Keys[size_t(b.Key)],second=b.Alternate>=0&&s.Keys[size_t(b.Alternate)];
        physical[a]=first||second;
        if(ShiftOK(b.Shift,shift)){sources[a]=uint8_t((first?1:0)|(second?2:0));keyboard[a]=physical[a];}
        else if(physical[a])m_Armed[a]=false; // Changing Shift alone is not a fresh Tab gesture.
    }
    const bool changedPad=s.Pad.Device!=m_Pad||s.Pad.Revision!=m_PadRevision;
    const bool retiredPad=changedPad&&m_Pad>=0;
    if(changedPad){m_Pad=s.Pad.Device;m_PadRevision=s.Pad.Revision;m_PadReady=false;m_StickDirection=-1;}
    bool neutral=true;
    for(bool b:s.Pad.Buttons)neutral&=!b;
    for(float v:s.Pad.Sticks)neutral&=std::isfinite(v)&&std::abs(v)<=m_Bindings.NeutralZone;
    if(retiredPad){
        m_Ready=false;m_Held.fill(false);m_Armed.fill(false);m_Repeating.fill(false);m_Sources.fill(0);m_Edges.clear();f.Enabled=false;return f;
    }
    const bool padArmed=m_PadReady;
    if(s.Pad.Device>=0&&neutral)m_PadReady=true;
    if(s.Pad.Device>=0&&padArmed){
        for(size_t a=1;a<N;++a){int p=m_Bindings.Actions[a].Pad;pad[a]=p>=0&&s.Pad.Buttons[size_t(p)];}
        if(m_Bindings.Stick>=0){
            float x=s.Pad.Sticks[size_t(m_Bindings.Stick*2)],y=s.Pad.Sticks[size_t(m_Bindings.Stick*2+1)];
            if(!std::isfinite(x)||!std::isfinite(y)){x=y=0;m_PadReady=false;}
            if(std::max(std::abs(x),std::abs(y))<=m_Bindings.NeutralZone)m_StickDirection=-1;
            else if(m_StickDirection==-1&&std::max(std::abs(x),std::abs(y))>=m_Bindings.DeadZone)
                m_StickDirection=int(std::abs(x)>=std::abs(y)?(x<0?UIAction::Left:UIAction::Right):(y<0?UIAction::Up:UIAction::Down));
            if(m_StickDirection>=0)pad[size_t(m_StickDirection)]=true;
        }
    }
    for(size_t a=1;a<N;++a){if(pad[a])sources[a]|=4;held[a]=sources[a]!=0;}
    for(auto pair:{std::pair<size_t,size_t>{1,2},{3,4},{5,6}})
        if(held[pair.first]&&held[pair.second])held[pair.first]=held[pair.second]=false;
    // A diagonal chooses horizontal, then must neutral-rearm the stick before changing axis.
    if(held[3]||held[4])held[1]=held[2]=false;
    f.LeftDown=held[0];f.AcceptDown=held[size_t(UIAction::Accept)];f.CancelDown=held[size_t(UIAction::Cancel)];
    if(!m_Ready){
        m_Ready=true;m_Held=held;m_Sources=sources;
        for(size_t a=0;a<N;++a)m_Armed[a]=!held[a]&&!physical[a];
        m_Edges.clear();return f;
    }
    std::array<bool,N> touched{};
    auto emit=[&](ActionEdge e){size_t a=size_t(e.Action);touched[a]=true;
        if(e.Down){
            if(!m_Held[a]&&m_Armed[a]){
                f.Events.push_back(e);m_Device=e.Device;m_LastAction=e.Action;
                m_Repeat[a]=m_Bindings.RepeatDelay;m_Repeating[a]=true;
                if(e.Action==UIAction::Press)for(size_t nav=1;nav<=size_t(UIAction::Previous);++nav){m_Repeating[nav]=false;if(m_Held[nav]||m_Sources[nav])m_Armed[nav]=false;}
            }
            m_Held[a]=true;m_Armed[a]=false;
        }else {
            if(m_Held[a])f.Events.push_back(e);
            m_Held[a]=false;m_Armed[a]=true;m_Repeating[a]=false;
        }
    };
    // Merge binding sources before generating logical edges. Releasing Enter while
    // Space or gamepad South is still held cannot release/re-press Accept.
    for(auto queued:m_Edges){auto e=queued.Edge;size_t a=size_t(e.Action);
        if(e.Down)m_Sources[a]|=queued.Source;else m_Sources[a]&=uint8_t(~queued.Source);
        e.Down=m_Sources[a]!=0;
        if(Navigation(a)&&e.Down){
            const size_t opposite=a==1?2:a==2?1:a==3?4:a==4?3:a==5?6:5;
            if(keyboard[opposite]||pad[opposite]||((a==1||a==2)&&(held[3]||held[4])))continue;
        }
        emit(e);
    }
    m_Edges.clear();m_Sources=sources;
    for(size_t a=0;a<N;++a){
        if(held[a]!=m_Held[a])emit({UIAction(a),held[a],s.Point,pad[a]?InputDevice::Gamepad:(a==0?InputDevice::Pointer:InputDevice::Keyboard)});
        if(!held[a]){m_Armed[a]=!physical[a];m_Repeating[a]=false;}
        if(Navigation(a)&&held[a]&&!touched[a]&&m_Repeating[a]){
            m_Repeat[a]-=std::min(seconds,.1f);
            if(m_Repeat[a]<=0){
                f.Events.push_back({UIAction(a),true,{},pad[a]?InputDevice::Gamepad:InputDevice::Keyboard});
                m_Device=pad[a]?InputDevice::Gamepad:InputDevice::Keyboard;m_LastAction=UIAction(a);
                m_Repeat[a]=m_Bindings.RepeatInterval;
            }
        }
    }
    if(f.Events.size()>MaxEdges){++m_Dropped;Cancel();f={};return f;}
    return f;
}
}
