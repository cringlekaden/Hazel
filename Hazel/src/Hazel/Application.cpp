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
        float vertices[] = {
            -0.5f, -0.5f, 0.0f,
            0.5f, -0.5f, 0.0f,
            0.0f,  0.5f, 0.0f
        };
        m_VertexBuffer.reset(VertexBuffer::Create(vertices, sizeof(vertices)));
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
        std::uint32_t indices[] = { 0, 1, 2 };
        m_IndexBuffer.reset(IndexBuffer::Create(indices, sizeof(indices) / sizeof(std::uint32_t)));
        const std::string vertexSrc = R"glsl(#version 330 core
        layout(location = 0) in vec3 a_Position;
        out vec3 v_Position;
        void main()
        {
            v_Position = a_Position;
            gl_Position = vec4(a_Position, 1.0);
        }
        )glsl";
        const std::string fragmentSrc = R"glsl(#version 330 core
        layout(location = 0) out vec4 color;
        in vec3 v_Position;
        void main()
        {
            color = vec4(v_Position * 0.5 + 0.5, 1.0);
        }
        )glsl";
        m_Shader = std::make_unique<Shader>(vertexSrc, fragmentSrc);
    }

    Application::~Application()
    {
        if(m_Shader)
        {
            m_Shader->Unbind();
            m_Shader.reset();
        }
        m_IndexBuffer.reset();
        m_VertexBuffer.reset();
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

            m_Shader->Bind();
            glBindVertexArray(m_VertexArray);
            glDrawElements(GL_TRIANGLES, m_IndexBuffer->GetCount(), GL_UNSIGNED_INT, nullptr);

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