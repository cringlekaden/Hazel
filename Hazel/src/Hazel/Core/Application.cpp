#include "hzpch.h"

#include "Hazel/Core/Application.h"
#include "Hazel/Events/ApplicationEvent.h"
#include "Hazel/Core/Window.h"
#include "Hazel/Core/Log.h"
#include "Hazel/Renderer/Renderer.h"

#include <GLFW/glfw3.h>

namespace Hazel {
    
    Application* Application::s_Instance = nullptr;

    Application::Application()
    {
        HZ_PROFILE_FUNCTION();
        HZ_CORE_ASSERT(!s_Instance, "Application already exists...");
        s_Instance = this;
        m_Window = Window::Create();
        m_Window->SetEventCallback(HZ_BIND_EVENT_FN(Application::OnEvent));
        Renderer::Init();
        Scope<ImGuiLayer> overlay = CreateScope<ImGuiLayer>();
        m_ImGuiLayer = overlay.get();
        PushOverlay(std::move(overlay));
    }

    Application::~Application()
    {
        HZ_PROFILE_FUNCTION();
        Renderer::Shutdown();
    }

    void Application::PushLayer(Scope<Layer> layer)
    {
        HZ_PROFILE_FUNCTION();
        m_LayerStack.PushLayer(std::move(layer));
    }

    void Application::PushOverlay(Scope<Layer> overlay)
    {
        HZ_PROFILE_FUNCTION();
        m_LayerStack.PushOverlay(std::move(overlay));
    }

    void Application::Run()
    {
        HZ_PROFILE_FUNCTION();
        while (m_Running)
        {
            HZ_PROFILE_SCOPE("RunLoop");
            const float time = static_cast<float>(glfwGetTime());
            Timestep timestep = time - m_LastFrameTime;
            m_LastFrameTime = time;
            if (!m_Minimized)
            {
                {
                    HZ_PROFILE_SCOPE("LayerStack OnUpdate");
                    for (auto& layer : m_LayerStack)
                        layer->OnUpdate(timestep);
                }
                m_ImGuiLayer->Begin();
                {
                    HZ_PROFILE_SCOPE("LayerStack OnImGuiRender");
                    for (auto& layer : m_LayerStack)
                        layer->OnImGuiRender();
                }
                m_ImGuiLayer->End();
            }
            m_Window->OnUpdate();
        }
    }

    void Application::OnEvent(Event& e)
    {
        HZ_PROFILE_FUNCTION();
        EventDispatcher dispatcher(e);
        dispatcher.Dispatch<WindowCloseEvent>(HZ_BIND_EVENT_FN(Application::OnWindowClose));
        dispatcher.Dispatch<WindowResizeEvent>(HZ_BIND_EVENT_FN(Application::OnWindowResize));
        for (auto it = m_LayerStack.end(); it != m_LayerStack.begin(); )
        {
            (*--it)->OnEvent(e);
            if (e.Handled)
                break;
        }
    }

    bool Application::OnWindowClose(WindowCloseEvent&)
    {
        m_Running = false;
        return true;
    }

    bool Application::OnWindowResize(WindowResizeEvent& e)
    {
        HZ_PROFILE_FUNCTION();
        if (e.GetWidth() == 0 || e.GetHeight() == 0)
        {
            m_Minimized = true;
            return false;
        }
        m_Minimized = false;
        Renderer::OnWindowResize(e.GetWidth(), e.GetHeight());
        return false;
    }
}
