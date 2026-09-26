#include "hzpch.h"

#include "Hazel/Window.h"
#include "Hazel/Application.h"
#include "Hazel/Log.h"
#include <memory>

#include <glad/glad.h>
namespace Hazel
{
    Application* Application::s_Instance = nullptr;

    Application::Application()
    {
        s_Instance = this;
        m_Window = std::unique_ptr<Window>(Window::Create());
        m_Window->SetEventCallback(HZ_BIND_EVENT_FN(Application::OnEvent));
        m_ImGuiLayer = new ImGuiLayer();
        PushOverlay(m_ImGuiLayer);

        glGenVertexArrays(1, &m_VertexArray);
        glBindVertexArray(m_VertexArray);
        glGenBuffers(1, &m_VertexBuffer);
        glBindBuffer(GL_ARRAY_BUFFER, m_VertexBuffer);
        const float vertices[] = {
            -0.5f, -0.5f, 0.0f,
            0.5f, -0.5f, 0.0f,
            0.0f, 0.5f, 0.0f
        };
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
        const unsigned int indices[] = { 0, 1, 2 };
        glGenBuffers(1, &m_IndexBuffer);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_IndexBuffer);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
    }

    Application::~Application()
    {
        glDeleteBuffers(1, &m_IndexBuffer);
        glDeleteBuffers(1, &m_VertexBuffer);
        glDeleteVertexArrays(1, &m_VertexArray);
    }

    void Application::PushLayer(Layer* layer)
    {
        m_LayerStack.PushLayer(layer);
    }

    void Application::PushOverlay(Layer* overlay)
    {
        m_LayerStack.PushOverlay(overlay);
    }

    void Application::Run()
    {
        while (m_Running)
        {
            glClearColor(0.1f,0.1f, 0.1f, 1);
            glClear(GL_COLOR_BUFFER_BIT);

            glBindVertexArray(m_VertexArray);
            glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_INT, nullptr);

            for(Layer* layer : m_LayerStack)
                layer->OnUpdate();
            m_ImGuiLayer->Begin();
            for(Layer* layer : m_LayerStack)
                layer->OnImGuiRender();
            m_ImGuiLayer->End();
            m_Window->OnUpdate();
        }
    }

    void Application::OnEvent(Event& e)
    {
        EventDispatcher dispatcher(e);
        dispatcher.Dispatch<WindowCloseEvent>(HZ_BIND_EVENT_FN(Application::OnWindowClose));
        for(auto it = m_LayerStack.end(); it != m_LayerStack.begin(); )
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
}