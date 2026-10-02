#include "hzpch.h"

#include "Platform/OpenGL/OpenGLContext.h"
#include "Platform/OpenGL/OpenGLCapabilities.h"
#include "Hazel/Core/Core.h"
#include "Hazel/Core/Log.h"

#include "GLFW/glfw3.h"
#include "glad/glad.h"
#include <cstdlib>

namespace Hazel {
    
    OpenGLContext::OpenGLContext(GLFWwindow* windowHandle) : m_WindowHandle(windowHandle)
    {
        HZ_CORE_ASSERT(windowHandle, "Failed to initialize OpenGLContext: Window handle is null...");
    }

    // Minimum backend version, verified with GLSL 410 and bind-based operations.
    void OpenGLContext::ConfigureWindowHints()
    {
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    }

    void OpenGLContext::Init()
    {
        HZ_PROFILE_FUNCTION();
        glfwMakeContextCurrent(m_WindowHandle);
        const int status = gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
        if(status == 0)
        {
            HZ_CORE_ERROR("Failed to initialize Glad...");
            std::abort();
        }
        OpenGLCapabilities::Initialize();
        const auto& caps = OpenGLCapabilities::Get();
        HZ_CORE_INFO("OpenGL Info: {} / {} / {}", caps.Vendor, caps.Device, caps.Driver);

    }

    void OpenGLContext::SwapBuffers()
    {
        HZ_PROFILE_FUNCTION();
        glfwSwapBuffers(m_WindowHandle);
    }
}