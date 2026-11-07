#pragma once
#include "Hazel/Renderer/Cameras/NavigationCamera.h"
#include <cmath>
namespace Hazel {
struct NavigationInput {
    glm::vec2 Pointer{};glm::vec3 Movement{};
    bool Left=false,Right=false,Middle=false,Alt=false,Shift=false,Space=false;
    bool Focused=true,OverImage=false,Blocked=false,Escape=false;
    float Speed=3;
};
// Editor gesture ownership, independent of sampling API. One uninterrupted press
// keeps ownership outside the image; projection/controller code knows no bindings.
class ViewportNavigation {
public:
    enum class Gesture { Idle,Orbit,Pan,Dolly,Fly };
    static bool Intent(const NavigationInput& s){return s.Right||s.Middle||(s.Left&&(s.Alt||s.Space||s.Shift));}
    void Cancel(){m_Gesture=Gesture::Idle;m_Latched=true;m_Seed=true;}
    void SeedPointer(){m_Seed=true;}
    bool Active()const{return m_Gesture!=Gesture::Idle;}
    Gesture Current()const{return m_Gesture;}
    void Update(NavigationCamera& camera,const NavigationInput& s,float seconds) {
        if(!std::isfinite(s.Pointer.x)||!std::isfinite(s.Pointer.y)||std::abs(s.Pointer.x)>1e7f||std::abs(s.Pointer.y)>1e7f){m_Seed=true;return;}
        const bool held=s.Left||s.Right||s.Middle;
        if(!held)m_Latched=false;
        if(!s.Focused||s.Blocked||s.Escape||camera.Navigation()!=m_Mode){Cancel();m_Mode=camera.Navigation();}
        if(!s.Focused||s.Blocked||s.Escape){m_Previous=s.Pointer;return;}
        if(Active()&&!ButtonHeld(s)){m_Gesture=Gesture::Idle;m_Seed=true;}
        if(!Active()&&!m_Latched&&s.OverImage&&Intent(s)) {
            m_Button=s.Middle?2:s.Right?1:0;
            m_Gesture=camera.Navigation()==EditorNavigation::Planar?Gesture::Pan:
                s.Middle||s.Shift?Gesture::Pan:s.Right?(s.Alt?Gesture::Dolly:Gesture::Fly):Gesture::Orbit;
            m_Latched=true;m_Seed=true;
        }
        const auto delta=m_Seed?glm::vec2(0):s.Pointer-m_Previous;m_Previous=s.Pointer;m_Seed=false;
        if(held&&!Active())m_Latched=true;
        switch(m_Gesture){
        case Gesture::Pan:camera.Pan(delta);break;
        case Gesture::Orbit:camera.Orbit(delta);break;
        case Gesture::Dolly:camera.Scroll(-delta.y*.04f);break;
        case Gesture::Fly:camera.Fly(delta,s.Movement,seconds,s.Speed*(s.Shift?5.f:1.f));break;
        default:break;
        }
    }
private:
    bool ButtonHeld(const NavigationInput&s)const{return m_Button==0?s.Left:m_Button==1?s.Right:s.Middle;}
    Gesture m_Gesture=Gesture::Idle;EditorNavigation m_Mode=EditorNavigation::Orbit;
    glm::vec2 m_Previous{};int m_Button=0;bool m_Latched=false,m_Seed=true;
};
}
