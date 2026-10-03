#include "hzpch.h"
#include "Hazel/Utils/PlatformUtils.h"
#include <GLFW/glfw3.h>
namespace Hazel { float Time::GetTime() { return static_cast<float>(glfwGetTime()); } }
