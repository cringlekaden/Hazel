#pragma once
#include "Camera.h"
#include "PerspectiveCameraController.h"
#include "PlanarCameraController.h"
#include "Hazel/Scene/SceneDefaults.h"
namespace Hazel
{
// A transient view camera; hosts own gestures/workspace and authored SceneCamera
// remains independent. Planar and perspective poses survive projection switches.
class NavigationCamera : public Camera
{
  public:
    NavigationCamera(float fov = 45, float aspect = 1.778f, float nearClip = .1f,
                     float farClip = 1000);
    EditorNavigation Navigation() const { return m_Navigation; }
    void SetNavigation(EditorNavigation mode);
    void SetViewportSize(float width, float height);
    void RestorePlanar(glm::vec2 focus, float size);
    glm::vec2 PlanarFocus() const { return m_Planar.Focus; }
    float PlanarSize() const { return m_Planar.Size; }
    void RestoreOrbit(const glm::vec3 &focus, float pitch, float yaw, float distance);
    void RestoreWorldPose(glm::vec3 position, glm::quat orientation);
    void FrameBounds(const glm::vec3 &minimum, const glm::vec3 &maximum);
    void Reset();
    void Pan(glm::vec2 points);
    void Orbit(glm::vec2 points);
    void Scroll(float steps);
    void Fly(glm::vec2 look, glm::vec3 movement, float seconds, float speed);
    const glm::mat4 &GetViewMatrix() const { return m_View; }
    glm::mat4 GetViewProjection() const { return m_Projection * m_View; }
    const glm::vec3 &GetPosition() const { return m_Position; }
    glm::vec3 GetUpDirection() const { return GetOrientation() * glm::vec3(0, 1, 0); }
    glm::vec3 GetRightDirection() const { return GetOrientation() * glm::vec3(1, 0, 0); }
    glm::vec3 GetForwardDirection() const { return GetOrientation() * glm::vec3(0, 0, -1); }
    glm::quat GetOrientation() const
    {
        return m_Navigation == EditorNavigation::Planar ? glm::quat(1, 0, 0, 0)
                                                        : m_Perspective.Orientation();
    }
    float GetPitch() const { return m_Perspective.Pitch; }
    float GetYaw() const { return m_Perspective.Yaw; }
    float GetDistance() const { return m_Perspective.Distance; }
    void SetDistance(float distance)
    {
        RestoreOrbit(GetFocalPoint(), GetPitch(), GetYaw(), distance);
    }
    const glm::vec3 &GetFocalPoint() const { return m_Perspective.Focus; }

  private:
    void Update();
    EditorNavigation m_Navigation = EditorNavigation::Orbit;
    PerspectiveCameraController m_Perspective;
    PlanarCameraController m_Planar;
    float m_FOV, m_Aspect, m_Near, m_Far, m_Height = 720;
    glm::mat4 m_View{1};
    glm::vec3 m_Position{0};
};
} // namespace Hazel
