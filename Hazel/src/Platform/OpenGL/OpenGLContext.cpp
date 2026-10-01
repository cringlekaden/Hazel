#include "hzpch.h"

#include "Platform/OpenGL/OpenGLContext.h"
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
        HZ_CORE_INFO("OpenGL Info:");
        HZ_CORE_INFO("  Vendor: {0}", (const char*)glGetString(GL_VENDOR));
        HZ_CORE_INFO("  Renderer: {0}", (const char*)glGetString(GL_RENDERER));
        HZ_CORE_INFO("  Version: {0}", (const char*)glGetString(GL_VERSION));
        GLint versionMajor = 0;
        GLint versionMinor = 0;
        glGetIntegerv(GL_MAJOR_VERSION, &versionMajor);
        glGetIntegerv(GL_MINOR_VERSION, &versionMinor);
        const bool supportsOpenGL41 = versionMajor > 4 || (versionMajor == 4 && versionMinor >= 1);
        HZ_CORE_ASSERT(supportsOpenGL41, "Hazel requires OpenGL 4.1 or newer...");
        if (!supportsOpenGL41)
        {
            HZ_CORE_ERROR("Hazel requires OpenGL 4.1 or newer; this context reports {}.{}", versionMajor, versionMinor);
            std::abort();
        }
    }

    void OpenGLContext::SwapBuffers()
    {
        HZ_PROFILE_FUNCTION();
        glfwSwapBuffers(m_WindowHandle);
    }
}