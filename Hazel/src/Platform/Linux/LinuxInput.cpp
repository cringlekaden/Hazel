// Adapted upstream Input API; native Linux GLFW implementation.
#include "hzpch.h"
#include "Hazel/Core/Input.h"

#include "Hazel/Core/Application.h"
#include <GLFW/glfw3.h>

namespace Hazel {

    namespace { uint64_t s_GamepadRevision=1;
        void GamepadChanged(int,int){++s_GamepadRevision;}
    }
    GamepadSample Input::GetGamepad() {
        // This callback owns only connection revision, never scene/action state.
        glfwSetJoystickCallback(GamepadChanged);
        GamepadSample result;result.Revision=s_GamepadRevision;
        for(int id=GLFW_JOYSTICK_1;id<=GLFW_JOYSTICK_LAST;++id) {
            GLFWgamepadstate state{};if(!glfwGetGamepadState(id,&state))continue;
            result.Device=id;
            const int buttons[]={GLFW_GAMEPAD_BUTTON_A,GLFW_GAMEPAD_BUTTON_B,GLFW_GAMEPAD_BUTTON_X,GLFW_GAMEPAD_BUTTON_Y,
                GLFW_GAMEPAD_BUTTON_LEFT_BUMPER,GLFW_GAMEPAD_BUTTON_RIGHT_BUMPER,GLFW_GAMEPAD_BUTTON_BACK,GLFW_GAMEPAD_BUTTON_START,
                GLFW_GAMEPAD_BUTTON_GUIDE,GLFW_GAMEPAD_BUTTON_LEFT_THUMB,GLFW_GAMEPAD_BUTTON_RIGHT_THUMB,
                GLFW_GAMEPAD_BUTTON_DPAD_UP,GLFW_GAMEPAD_BUTTON_DPAD_RIGHT,GLFW_GAMEPAD_BUTTON_DPAD_DOWN,GLFW_GAMEPAD_BUTTON_DPAD_LEFT};
            for(size_t i=0;i<result.Buttons.size();++i)result.Buttons[i]=state.buttons[buttons[i]]==GLFW_PRESS;
            const int axes[]={GLFW_GAMEPAD_AXIS_LEFT_X,GLFW_GAMEPAD_AXIS_LEFT_Y,GLFW_GAMEPAD_AXIS_RIGHT_X,GLFW_GAMEPAD_AXIS_RIGHT_Y};
            for(size_t i=0;i<result.Sticks.size();++i)result.Sticks[i]=state.axes[axes[i]];
            break;
        }
        return result;
    }


	bool Input::IsKeyPressed(const KeyCode key)
	{
		auto* window = static_cast<GLFWwindow*>(Application::Get().GetWindow().GetNativeWindow());
		auto state = glfwGetKey(window, static_cast<int32_t>(key));
		return state == GLFW_PRESS;
	}

	bool Input::IsMouseButtonPressed(const MouseCode button)
	{
		auto* window = static_cast<GLFWwindow*>(Application::Get().GetWindow().GetNativeWindow());
		auto state = glfwGetMouseButton(window, static_cast<int32_t>(button));
		return state == GLFW_PRESS;
	}

	glm::vec2 Input::GetMousePosition()
	{
		auto* window = static_cast<GLFWwindow*>(Application::Get().GetWindow().GetNativeWindow());
		double xpos, ypos;
		glfwGetCursorPos(window, &xpos, &ypos);

		return { (float)xpos, (float)ypos };
	}

    glm::vec2 Input::GetMouseScreenPosition() {
        auto* window = static_cast<GLFWwindow*>(Application::Get().GetWindow().GetNativeWindow());
        int x = 0, y = 0; glfwGetWindowPos(window, &x, &y);
        return GetMousePosition() + glm::vec2(x, y);
    }

	float Input::GetMouseX()
	{
		return GetMousePosition().x;
	}

	float Input::GetMouseY()
	{
		return GetMousePosition().y;
	}

}
