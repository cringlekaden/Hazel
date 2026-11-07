#include "hzpch.h"
#include "NavigationCamera.h"
#include "PerspectiveProjection.h"
#include "OrthographicProjection.h"
#include <glm/gtx/quaternion.hpp>
#include <algorithm>
#include <cmath>
namespace Hazel {
NavigationCamera::NavigationCamera(float fov,float aspect,float nearClip,float farClip):m_FOV(glm::radians(fov)),m_Aspect(aspect),m_Near(nearClip),m_Far(farClip){Update();}
void NavigationCamera::Update() {
    if(m_Navigation==EditorNavigation::Planar){float half=m_PlanarSize*.5f;m_Projection=OrthographicProjection::Matrix(-half*m_Aspect,half*m_Aspect,-half,half,m_Near,m_Far);m_Position={m_PlanarFocus,10};m_View=glm::translate(glm::mat4(1),-m_Position);}
    else {m_Projection=PerspectiveProjection::Matrix(m_FOV,m_Aspect,m_Near,m_Far);m_Position=m_Perspective.Position();m_View=glm::inverse(glm::translate(glm::mat4(1),m_Position)*glm::toMat4(m_Perspective.Orientation()));}
}
void NavigationCamera::SetNavigation(EditorNavigation mode){if(mode!=EditorNavigation::Planar&&mode!=EditorNavigation::Orbit)throw std::invalid_argument("Unknown navigation mode");m_Navigation=mode;Update();}
void NavigationCamera::SetViewportSize(float width,float height){if(width>0&&height>0){m_Aspect=width/height;m_Height=height;Update();}}
void NavigationCamera::RestorePlanar(glm::vec2 focus,float size){if(!std::isfinite(focus.x)||!std::isfinite(focus.y)||!std::isfinite(size)||size<.01f||size>100000)return;m_PlanarFocus=focus;m_PlanarSize=size;Update();}
void NavigationCamera::RestoreOrbit(const glm::vec3& focus,float pitch,float yaw,float distance){for(int i=0;i<3;++i)if(!std::isfinite(focus[i]))return;if(!std::isfinite(pitch)||!std::isfinite(yaw)||!std::isfinite(distance)||distance<.05f||distance>100000)return;m_Perspective.Focus=focus;m_Perspective.Pitch=pitch;m_Perspective.Yaw=yaw;m_Perspective.Distance=distance;Update();}
void NavigationCamera::RestoreWorldPose(glm::vec3 position,glm::quat orientation){m_Perspective.SetPose(position,orientation);Update();}
void NavigationCamera::Reset(){if(m_Navigation==EditorNavigation::Planar)RestorePlanar({0,0},10);else RestoreOrbit({0,0,0},0,0,10);}
void NavigationCamera::Pan(glm::vec2 points){if(m_Navigation==EditorNavigation::Planar)m_PlanarFocus+=glm::vec2(-points.x,points.y)*(m_PlanarSize/m_Height);else m_Perspective.Pan(points,m_FOV,m_Height);Update();}
void NavigationCamera::Orbit(glm::vec2 points){if(m_Navigation==EditorNavigation::Orbit)m_Perspective.Orbit(points);Update();}
void NavigationCamera::Scroll(float steps){if(m_Navigation==EditorNavigation::Planar)m_PlanarSize=std::clamp(m_PlanarSize*std::exp(-steps*.15f),.01f,100000.f);else m_Perspective.Dolly(steps);Update();}
void NavigationCamera::Fly(glm::vec2 look,glm::vec3 movement,float seconds,float speed){if(m_Navigation==EditorNavigation::Orbit)m_Perspective.Fly(look,movement,seconds,speed);Update();}
void NavigationCamera::FrameBounds(const glm::vec3& minimum,const glm::vec3& maximum){
    for(int i=0;i<3;++i)if(!std::isfinite(minimum[i])||!std::isfinite(maximum[i])||minimum[i]>maximum[i])throw std::invalid_argument("Invalid framing bounds");
    auto center=(minimum+maximum)*.5f,span=maximum-minimum;
    if(m_Navigation==EditorNavigation::Planar)RestorePlanar(glm::vec2(center),std::clamp(std::max(span.y,span.x/m_Aspect)*1.2f,.01f,100000.f));
    else {float radius=std::max(.05f,glm::length(span)*.5f);float half=std::atan(std::tan(m_FOV*.5f)*std::min(1.f,m_Aspect));RestoreOrbit(center,GetPitch(),GetYaw(),std::clamp(radius/std::sin(half)*1.15f,.1f,100000.f));}
}
}
