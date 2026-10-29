// Adapted from TheCherno/Hazel 1feb705 for local ownership and native Linux/Windows portability.
#pragma once

#include "Hazel/Core/KeyCodes.h"
#include "Hazel/Core/MouseCodes.h"
#include "Hazel/Core/Gamepad.h"

#include <glm/glm.hpp>

namespace Hazel {

	class Input
	{
	public:
		static bool IsKeyPressed(KeyCode key);
        static GamepadSample GetGamepad(); // Main thread; first standardized connected device.

		static bool IsMouseButtonPressed(MouseCode button);
		static glm::vec2 GetMousePosition();
        static glm::vec2 GetMouseScreenPosition();
		static float GetMouseX();
		static float GetMouseY();
	};
}
