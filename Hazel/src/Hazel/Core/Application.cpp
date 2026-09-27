#include "Hazel/Events/ApplicationEvent.h"
#include "hzpch.h"

#include "Hazel/Core/Application.h"
#include "Hazel/Core/Window.h"
#include "Hazel/Core/Log.h"
#include "Hazel/Renderer/Renderer.h"

#include <GLFW/glfw3.h>

namespace Hazel {
    
    Application* Application::s_Instance = nullptr;

    Application::Application()
    {
        s_Instance = this;
        m_Window = Window::Create();
        m_Window->SetEventCallback(HZ_BIND_EVENT_FN(Application::OnEvent));
        Renderer::Init();
        Scope<ImGuiLayer> overlay = CreateScope<ImGuiLayer>();
        m_ImGuiLayer.reset(overlay.get());
        PushOverlay(std::move(overlay));
    }

    Application::~Application() = default;

    void Application::PushLayer(Scope<Layer> layer)
    {
        m_LayerStack.PushLayer(std::move(layer));
    }

    void Application::PushOverlay(Scope<Layer> overlay)
    {
        m_LayerStack.PushOverlay(std::move(overlay));
    }

    void Application::Run()
    {
        while (m_Running)
        {
            const float time = static_cast<float>(glfwGetTime());
            Timestep timestep = time - m_LastFrameTime;
            m_LastFrameTime = time;
            if(!m_Minimized)
            {
                for (auto& layer : m_LayerStack)
                    layer->OnUpdate(timestep);
            }
            m_ImGuiLayer->Begin();
            for (auto& layer : m_LayerStack)
                layer->OnImGuiRender();
            m_ImGuiLayer->End();
            m_Window->OnUpdate();
        }
    }

    void Application::OnEvent(Event& e)
    {
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