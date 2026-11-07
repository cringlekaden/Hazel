#include "hzpch.h"
#include "RuntimeSession.h"
#include "Scene.h"
#include "Entity.h"
#include "Components.h"
#include "Hazel/Core/Application.h"
#include "Hazel/Core/Input.h"
#include "Hazel/Math/Math.h"
#include <glm/gtx/quaternion.hpp>
namespace Hazel
{
void RuntimeSession::SetCameraInspection(bool enabled)
{
    CheckThread();
    CancelCameraInspectionGesture();
    if (!enabled)
    {
        m_Inspection.reset();
        return;
    }
    if (!m_Scene)
        throw std::logic_error("Start Play before enabling camera inspection");
    auto entity = m_Scene->GetPrimaryCameraEntity();
    if (!entity || entity.GetComponent<CameraComponent>().Camera.GetProjectionType() !=
                       SceneCamera::ProjectionType::Perspective)
        throw std::runtime_error("Camera inspection requires a primary perspective camera");
    auto &projection = entity.GetComponent<CameraComponent>().Camera;
    NavigationCamera camera(glm::degrees(projection.GetPerspectiveVerticalFOV()),
                            projection.GetAspectRatio(), projection.GetPerspectiveNearClip(),
                            projection.GetPerspectiveFarClip());
    glm::vec3 position, rotation, scale;
    if (!Math::DecomposeTransform(m_Scene->GetWorldTransform(entity), position, rotation, scale))
        throw std::runtime_error("Camera inspection cannot resolve the camera world pose");
    camera.RestoreWorldPose(position, glm::quat(rotation));
    m_InspectionSeed = camera;
    m_Inspection = camera;
    HZ_CORE_INFO("Camera inspection enabled: transient view; Home restores authored pose, Escape "
                 "releases pointer");
}
void RuntimeSession::ResetCameraInspection()
{
    CheckThread();
    CancelCameraInspectionGesture();
    if (m_Inspection)
        m_Inspection = m_InspectionSeed;
}
void RuntimeSession::CancelCameraInspectionGesture()
{
    if (m_InspectionGesture == InspectionGesture::Fly)
        Application::Get().GetWindow().SetPointerCaptured(false);
    m_InspectionGesture = InspectionGesture::Idle;
    m_InspectionLatch = true;
    m_InspectionSeedPointer = true;
}
void RuntimeSession::UpdateCameraInspection(Timestep ts, bool focused, bool overImage)
{
    CheckThread();
    if (!m_Inspection)
        return;
    auto &window = Application::Get().GetWindow();
    bool left = Input::IsMouseButtonPressed(Mouse::ButtonLeft),
         right = Input::IsMouseButtonPressed(Mouse::ButtonRight),
         middle = Input::IsMouseButtonPressed(Mouse::ButtonMiddle);
    bool held = left || right || middle,
         alt = Input::IsKeyPressed(Key::LeftAlt) || Input::IsKeyPressed(Key::RightAlt),
         shift = Input::IsKeyPressed(Key::LeftShift) || Input::IsKeyPressed(Key::RightShift),
         space = Input::IsKeyPressed(Key::Space);
    if (!focused || !window.IsFocused() || Input::IsKeyPressed(Key::Escape))
    {
        CancelCameraInspectionGesture();
        return;
    }
    if (Input::IsKeyPressed(Key::Home))
    {
        ResetCameraInspection();
        return;
    }
    if (!held)
    {
        m_InspectionLatch = false;
        if (CameraInspectionGestureActive())
            CancelCameraInspectionGesture();
        m_InspectionLatch = false;
    }
    if (CameraInspectionGestureActive() && !(m_InspectionButton == 0   ? left
                                             : m_InspectionButton == 1 ? right
                                                                       : middle))
        CancelCameraInspectionGesture();
    if (!CameraInspectionGestureActive() && !m_InspectionLatch && held)
    {
        m_InspectionLatch = true;
        if (overImage && (right || middle || (left && (alt || shift || space))))
        {
            m_InspectionButton = middle ? 2 : right ? 1 : 0;
            if (middle)
                m_InspectionGesture = InspectionGesture::Pan;
            else if (right)
                m_InspectionGesture = alt ? InspectionGesture::Dolly : InspectionGesture::Fly;
            else
                m_InspectionGesture = shift ? InspectionGesture::Pan : InspectionGesture::Orbit;
            m_InspectionSeedPointer = true;
            if (m_InspectionGesture == InspectionGesture::Fly)
                window.SetPointerCaptured(true);
        }
    }
    auto pointer = Input::GetMouseScreenPosition();
    auto delta = m_InspectionSeedPointer ? glm::vec2(0) : pointer - m_InspectionPointer;
    m_InspectionPointer = pointer;
    m_InspectionSeedPointer = false;
    if (m_InspectionGesture == InspectionGesture::Orbit)
        m_Inspection->Orbit(delta);
    if (m_InspectionGesture == InspectionGesture::Pan)
        m_Inspection->Pan(delta);
    if (m_InspectionGesture == InspectionGesture::Dolly)
        m_Inspection->Scroll(-delta.y * .04f);
    if (m_InspectionGesture == InspectionGesture::Fly)
    {
        glm::vec3 move{float(Input::IsKeyPressed(Key::D)) - float(Input::IsKeyPressed(Key::A)),
                       float(Input::IsKeyPressed(Key::E)) - float(Input::IsKeyPressed(Key::Q)),
                       float(Input::IsKeyPressed(Key::W)) - float(Input::IsKeyPressed(Key::S))};
        float speed =
            3 * (shift ? 5.f : 1.f) *
            ((Input::IsKeyPressed(Key::LeftControl) || Input::IsKeyPressed(Key::RightControl))
                 ? .2f
                 : 1.f);
        m_Inspection->Fly(delta, move, ts, speed);
    }
}
} // namespace Hazel
