#include "hzpch.h"
// Adapted from TheCherno/Hazel 1feb705 for local ownership and native Linux/Windows portability.
#include <cmath>
#include "EditorCamera.h"

#include "Hazel/Core/Input.h"
#include "Hazel/Core/KeyCodes.h"
#include "Hazel/Core/MouseCodes.h"

#include <GLFW/glfw3.h>

#ifndef GLM_ENABLE_EXPERIMENTAL
#define GLM_ENABLE_EXPERIMENTAL
#endif
#include <glm/gtx/quaternion.hpp>

namespace Hazel {

    void EditorCamera::SetNavigation(EditorNavigation mode) {
        if(mode!=EditorNavigation::Planar && mode!=EditorNavigation::Orbit)throw std::invalid_argument("Unsupported editor navigation");
        m_Navigation=mode;m_InitialMousePosition=Input::GetMousePosition();UpdateProjection();UpdateView();
    }
    void EditorCamera::RestorePlanar(glm::vec2 focus,float size) {
        if(!std::isfinite(focus.x)||!std::isfinite(focus.y)||!std::isfinite(size)||size<.01f||size>100000)return;
        m_PlanarFocus=focus;m_PlanarSize=size;UpdateProjection();UpdateView();
    }
    void EditorCamera::FrameBounds(const glm::vec3 &minimum, const glm::vec3 &maximum)
    {
        for (int i = 0; i < 3; ++i)
            if (!std::isfinite(minimum[i]) || !std::isfinite(maximum[i]) || minimum[i] > maximum[i])
                throw std::invalid_argument("Invalid framing bounds");
        auto center = (minimum + maximum) * .5f;
        auto span = maximum - minimum;
        if (m_Navigation == EditorNavigation::Planar)
            RestorePlanar(
                glm::vec2(center),
                std::clamp(std::max(span.y, span.x / m_AspectRatio) * 1.2f, .01f, 100000.f));
        else
        {
            float radius = std::max(.05f, glm::length(span) * .5f);
            float half =
                std::atan(std::tan(glm::radians(m_FOV) * .5f) * std::min(1.f, m_AspectRatio));
            RestoreOrbit(center, m_Pitch, m_Yaw,
                         std::clamp(radius / std::sin(half) * 1.15f, .1f, 100000.f));
        }
    }
    void EditorCamera::RestoreOrbit(const glm::vec3& focus,float pitch,float yaw,float distance) {
        if(!std::isfinite(pitch)||!std::isfinite(yaw)||!std::isfinite(distance)||distance<.1f||distance>100000)return;
        for(int axis=0;axis<3;++axis)if(!std::isfinite(focus[axis]))return;
        m_FocalPoint=focus;m_Pitch=pitch;m_Yaw=yaw;m_Distance=distance;UpdateView();
    }

	EditorCamera::EditorCamera(float fov, float aspectRatio, float nearClip, float farClip)
		: m_FOV(fov), m_AspectRatio(aspectRatio), m_NearClip(nearClip), m_FarClip(farClip)
	{
        m_ViewportWidth = aspectRatio * m_ViewportHeight;
        UpdateProjection();
		UpdateView();
	}

	void EditorCamera::UpdateProjection()
	{
		m_AspectRatio = m_ViewportWidth / m_ViewportHeight;
		if(m_Navigation==EditorNavigation::Planar) {
            const float half=m_PlanarSize*.5f;m_Projection=glm::ortho(-half*m_AspectRatio,half*m_AspectRatio,-half,half,.1f,1000.f);return;
        }
		m_Projection = glm::perspective(glm::radians(m_FOV), m_AspectRatio, m_NearClip, m_FarClip);
	}

	void EditorCamera::UpdateView()
	{
		if(m_Navigation==EditorNavigation::Planar) {
            m_Position={m_PlanarFocus,10};m_ViewMatrix=glm::translate(glm::mat4(1),-m_Position);return;
        }
		m_Position = CalculatePosition();

		glm::quat orientation = GetOrientation();
		m_ViewMatrix = glm::translate(glm::mat4(1.0f), m_Position) * glm::toMat4(orientation);
		m_ViewMatrix = glm::inverse(m_ViewMatrix);
	}

	std::pair<float, float> EditorCamera::PanSpeed() const
	{
		float x = std::min(m_ViewportWidth / 1000.0f, 2.4f); // max = 2.4f
		float xFactor = 0.0366f * (x * x) - 0.1778f * x + 0.3021f;

		float y = std::min(m_ViewportHeight / 1000.0f, 2.4f); // max = 2.4f
		float yFactor = 0.0366f * (y * y) - 0.1778f * y + 0.3021f;

		return { xFactor, yFactor };
	}

	float EditorCamera::RotationSpeed() const
	{
		return 0.8f;
	}

	float EditorCamera::ZoomSpeed() const
	{
		float distance = m_Distance * 0.2f;
		distance = std::max(distance, 0.0f);
		float speed = distance * distance;
		speed = std::min(speed, 100.0f); // max speed = 100
		return speed;
	}

	void EditorCamera::OnUpdate(Timestep)
	{
        const glm::vec2 mouse{Input::GetMouseX(),Input::GetMouseY()};
        const auto pixels=mouse-m_InitialMousePosition;m_InitialMousePosition=mouse;
        if(m_Navigation==EditorNavigation::Planar) {
            if(Input::IsMouseButtonPressed(Mouse::ButtonMiddle) || (Input::IsKeyPressed(Key::LeftAlt)&&Input::IsMouseButtonPressed(Mouse::ButtonLeft)))
                m_PlanarFocus+=glm::vec2(-pixels.x,pixels.y)*(m_PlanarSize/m_ViewportHeight);
            UpdateView();return;
        }
		if (Input::IsKeyPressed(Key::LeftAlt))
		{
            glm::vec2 delta=pixels*.003f;

			if (Input::IsMouseButtonPressed(Mouse::ButtonMiddle))
				MousePan(delta);
			else if (Input::IsMouseButtonPressed(Mouse::ButtonLeft))
				MouseRotate(delta);
			else if (Input::IsMouseButtonPressed(Mouse::ButtonRight))
				MouseZoom(delta.y);
		}

		UpdateView();
	}

	void EditorCamera::OnEvent(Event& e)
	{
		EventDispatcher dispatcher(e);
		dispatcher.Dispatch<MouseScrolledEvent>(HZ_BIND_EVENT_FN(EditorCamera::OnMouseScroll));
	}

	bool EditorCamera::OnMouseScroll(MouseScrolledEvent& e)
	{
		if(m_Navigation==EditorNavigation::Planar) {m_PlanarSize=std::clamp(m_PlanarSize*std::exp(-e.GetYOffset()*.15f),.01f,100000.f);UpdateProjection();return false;}
        float delta = e.GetYOffset() * 0.1f;
		MouseZoom(delta);
		UpdateView();
		return false;
	}

	void EditorCamera::MousePan(const glm::vec2& delta)
	{
		auto [xSpeed, ySpeed] = PanSpeed();
		m_FocalPoint += -GetRightDirection() * delta.x * xSpeed * m_Distance;
		m_FocalPoint += GetUpDirection() * delta.y * ySpeed * m_Distance;
	}

	void EditorCamera::MouseRotate(const glm::vec2& delta)
	{
		float yawSign = GetUpDirection().y < 0 ? -1.0f : 1.0f;
		m_Yaw += yawSign * delta.x * RotationSpeed();
		m_Pitch += delta.y * RotationSpeed();
	}

	void EditorCamera::MouseZoom(float delta)
	{
		m_Distance -= delta * ZoomSpeed();
		if (m_Distance < 1.0f)
		{
			m_FocalPoint += GetForwardDirection();
			m_Distance = 1.0f;
		}
	}

	glm::vec3 EditorCamera::GetUpDirection() const
	{
		return glm::rotate(GetOrientation(), glm::vec3(0.0f, 1.0f, 0.0f));
	}

	glm::vec3 EditorCamera::GetRightDirection() const
	{
		return glm::rotate(GetOrientation(), glm::vec3(1.0f, 0.0f, 0.0f));
	}

	glm::vec3 EditorCamera::GetForwardDirection() const
	{
		return glm::rotate(GetOrientation(), glm::vec3(0.0f, 0.0f, -1.0f));
	}

	glm::vec3 EditorCamera::CalculatePosition() const
	{
		return m_FocalPoint - GetForwardDirection() * m_Distance;
	}

	glm::quat EditorCamera::GetOrientation() const
	{
		return m_Navigation==EditorNavigation::Planar?glm::quat(1.f,0.f,0.f,0.f):glm::quat(glm::vec3(-m_Pitch, -m_Yaw, 0.0f));
	}

}
