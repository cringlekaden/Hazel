#include "hzpch.h"
#include "Hazel/Core/Window.h"
#include <GLFW/glfw3.h>
namespace Hazel {
    std::vector<DisplayArea> Window::GetDisplayAreas() const {
        std::vector<DisplayArea> areas;
        int count = 0;
        auto monitors = glfwGetMonitors(&count);
        auto primary = glfwGetPrimaryMonitor();
        auto append = [&](GLFWmonitor *m) {
            DisplayArea d;
            float y;
            glfwGetMonitorWorkarea(m, &d.X, &d.Y, &d.Width, &d.Height);
            glfwGetMonitorContentScale(m, &d.Scale, &y);
            areas.push_back(d);
        };
        if (primary)
            append(primary);
        for (int i = 0; i < count; ++i)
            if (monitors[i] != primary)
                append(monitors[i]);
        return areas;
    }
    float Window::GetContentScale() const {
        float x = 1, y = 1;
        glfwGetWindowContentScale(static_cast<GLFWwindow *>(GetNativeWindow()), &x, &y);
        return x;
    }
    WindowPlacement Window::GetPlacement() {
        auto w = static_cast<GLFWwindow *>(GetNativeWindow());
        const bool maximized = glfwGetWindowAttrib(w, GLFW_MAXIMIZED) != 0;
        if (!maximized && !glfwGetWindowAttrib(w, GLFW_ICONIFIED)) {
            glfwGetWindowPos(w, &m_NormalPlacement.X, &m_NormalPlacement.Y);
            glfwGetWindowSize(w, &m_NormalPlacement.Width, &m_NormalPlacement.Height);
            m_NormalPlacement.Scale = GetContentScale();
        }
        auto result = m_NormalPlacement;
        result.Maximized = maximized;
        return result;
    }
    void Window::RestorePlacement(const WindowPlacement &requested) {
        auto w = static_cast<GLFWwindow *>(GetNativeWindow());
        auto displays = GetDisplayAreas();
        int left = 0, top = 0, right = 0, bottom = 0;
        glfwGetWindowFrameSize(w, &left, &top, &right, &bottom);
        // GLFW positions describe the client area. Keep native title/resize chrome reachable.
        for (auto &area : displays) {
            area.X += left;
            area.Y += std::max(top, 24);
            area.Width = std::max(1, area.Width - left - right);
            area.Height = std::max(1, area.Height - std::max(top, 24) - bottom);
        }
        auto p = FitWindow(requested, displays);
        glfwRestoreWindow(w);
        glfwSetWindowSize(w, p.Width, p.Height);
        glfwSetWindowPos(w, p.X, p.Y);
        m_NormalPlacement = p;
        if (p.Maximized)
            glfwMaximizeWindow(w);
    }
} // namespace Hazel
