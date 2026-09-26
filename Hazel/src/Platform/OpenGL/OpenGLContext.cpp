#include "hzpch.h"

#include "Platform/OpenGL/OpenGLContext.h"
#include "Hazel/Core.h"
#include "Hazel/Log.h"

#include "GLFW/glfw3.h"
#include "glad/glad.h"
#include <cstdlib>

namespace Hazel {
    
    OpenGLContext::OpenGLContext(GLFWwindow* windowHandle) : m_WindowHandle(windowHandle)
    {
        HZ_CORE_ASSERT(windowHandle, "Failed to initialize OpenGLContext: Window handle is null...");
    }

    void OpenGLContext::Init()
    {
        glfwMakeContextCurrent(m_WindowHandle);
        const int status = gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
        if(status == 0)
        {
            HZ_CORE_ERROR("Failed to initialize Glad...");
            std::abort();
        }
        HZ_CORE_INFO("OpenGL Info:");
        HZ_CORE_INFO("  Vendor: {0}", (const char*)glGetString(GL_VENDOR));
        HZ_CORE_INFO("  Renderer: {0}", (const char*)glGetString(GL_RENDERER));
        HZ_CORE_INFO("  Version: {0}", (const char*)glGetString(GL_VERSION));
    }

    void OpenGLContext::SwapBuffers()
    {
        glfwSwapBuffers(m_WindowHandle);
    }
}