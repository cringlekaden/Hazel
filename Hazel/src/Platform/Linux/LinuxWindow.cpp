#include "hzpch.h"

#include "Platform/Linux/LinuxWindow.h"
#include "Hazel/Core/Log.h"
#include "Hazel/Events/ApplicationEvent.h"
#include "Hazel/Events/MouseEvent.h"
#include "Hazel/Events/KeyEvent.h"
#include "Platform/OpenGL/OpenGLContext.h"

#include <cstdlib>

namespace Hazel {

    static unsigned int s_GLFWWindowCount = 0;

    static void GLFWErrorCallback(int error, const char* description)
    {
        HZ_CORE_ERROR("GLFW Error ({0}): {1}", error, description);
    }

    Scope<Window> Window::Create(const WindowProps& props)
    {
        return CreateScope<LinuxWindow>(props);
    }

    LinuxWindow::LinuxWindow(const WindowProps& props)
    {
        HZ_PROFILE_FUNCTION();
        Init(props);
    }

    LinuxWindow::~LinuxWindow()
    {
        HZ_PROFILE_FUNCTION();
        Shutdown();
    }

    void LinuxWindow::Init(const WindowProps& props)
    {
        HZ_PROFILE_FUNCTION();
        m_Data.Title = props.Title;
        m_Data.Width = props.Width;
        m_Data.Height = props.Height;
        HZ_CORE_INFO("Creating window {0} ({1}, {2})", props.Title, props.Width, props.Height);
        if (s_GLFWWindowCount == 0)
        {
            HZ_CORE_INFO("Initializing GLFW");
            int success = 0;
            {
                HZ_PROFILE_SCOPE("glfwInit");
                success = glfwInit();
            }
            HZ_CORE_ASSERT(success, "Could not initialize GLFW...");
            if (!success)
            {
                HZ_CORE_ERROR("Could not initialize GLFW...");
                std::abort();
            }
            glfwSetErrorCallback(GLFWErrorCallback);
        }
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        {
            HZ_PROFILE_SCOPE("glfwCreateWindow");
            m_Window = glfwCreateWindow(
                (int)props.Width,
                (int)props.Height,
                m_Data.Title.c_str(),
                nullptr,
                nullptr);
        }
        HZ_CORE_ASSERT(m_Window, "Failed to create an OpenGL 4.2 core window...");
        if (!m_Window)
        {
            HZ_CORE_ERROR("Failed to create an OpenGL 4.2 core window...");
            std::abort();
        }
        ++s_GLFWWindowCount;
        m_Context = CreateScope<OpenGLContext>(m_Window);
        m_Context->Init();
        glfwSetWindowUserPointer(m_Window, &m_Data);
        SetVSync(true);
        glfwSetWindowSizeCallback(m_Window, [](GLFWwindow* window, int width, int height)
        {
            WindowData& data = *(WindowData*)glfwGetWindowUserPointer(window);
            data.Width = width;
            data.Height = height;
            WindowResizeEvent event(width, height);
            data.EventCallback(event);
        });
        glfwSetWindowCloseCallback(m_Window, [](GLFWwindow* window)
        {
            WindowData& data = *(WindowData*)glfwGetWindowUserPointer(window);
            WindowCloseEvent event;
            data.EventCallback(event);
        });
        glfwSetKeyCallback(m_Window, [](GLFWwindow* window, int key, int /*scancode*/, int action, int /*mods*/)
        {
            WindowData& data = *(WindowData*)glfwGetWindowUserPointer(window);
            switch (action)
            {
                case GLFW_PRESS:
                {
                    KeyPressedEvent event(key, 0);
                    data.EventCallback(event);
                    break;
                }
                case GLFW_RELEASE:
                {
                    KeyReleasedEvent event(key);
                    data.EventCallback(event);
                    break;
                }
                case GLFW_REPEAT:
                {
                    KeyPressedEvent event(key, 1);
                    data.EventCallback(event);
                    break;
                }
            }
        });
        glfwSetCharCallback(m_Window, [](GLFWwindow* window, unsigned int keycode)
        {
            WindowData& data = *(WindowData*)glfwGetWindowUserPointer(window);
            KeyTypedEvent event(keycode);
            data.EventCallback(event);
        });
        glfwSetMouseButtonCallback(m_Window, [](GLFWwindow* window, int button, int action, int /*mods*/)
        {
            WindowData& data = *(WindowData*)glfwGetWindowUserPointer(window);
            switch (action)
            {
                case GLFW_PRESS:
                {
                    MouseButtonPressedEvent event(button);
                    data.EventCallback(event);
                    break;
                }
                case GLFW_RELEASE:
                {
                    MouseButtonReleasedEvent event(button);
                    data.EventCallback(event);
                    break;
                }
            }
        });
        glfwSetScrollCallback(m_Window, [](GLFWwindow* window, double xOffset, double yOffset)
        {
            WindowData& data = *(WindowData*)glfwGetWindowUserPointer(window);
            MouseScrolledEvent event((float)xOffset, (float)yOffset);
            data.EventCallback(event);
        });
        glfwSetCursorPosCallback(m_Window, [](GLFWwindow* window, double xPos, double yPos)
        {
            WindowData& data = *(WindowData*)glfwGetWindowUserPointer(window);
            MouseMovedEvent event((float)xPos, (float)yPos);
            data.EventCallback(event);
        });
    }

    void LinuxWindow::Shutdown()
    {
        HZ_PROFILE_FUNCTION();
        m_Context.reset();
        glfwDestroyWindow(m_Window);
        if (--s_GLFWWindowCount == 0)
        {
            HZ_CORE_INFO("Terminating GLFW");
            glfwTerminate();
        }
    }

    void LinuxWindow::OnUpdate()
    {
        HZ_PROFILE_FUNCTION();
        glfwPollEvents();
        m_Context->SwapBuffers();
    }

    void LinuxWindow::SetVSync(bool enabled)
    {
        HZ_PROFILE_FUNCTION();
        if (enabled)
            glfwSwapInterval(1);
        else
            glfwSwapInterval(0);

        m_Data.VSync = enabled;
    }

    bool LinuxWindow::IsVSync() const
    {
        return m_Data.VSync;
    }
}
