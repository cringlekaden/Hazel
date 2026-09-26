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

        // Colored triangle.
        m_VertexArray.reset(VertexArray::Create());
        float vertices[] = {
            // position                // color
            -0.5f, -0.5f, 0.0f,       0.8f, 0.2f, 0.8f, 1.0f,
            0.5f, -0.5f, 0.0f,       0.2f, 0.3f, 0.8f, 1.0f,
            0.0f,  0.5f, 0.0f,       0.8f, 0.8f, 0.2f, 1.0f
        };
        std::shared_ptr<VertexBuffer> vertexBuffer;
        vertexBuffer.reset(VertexBuffer::Create(vertices, sizeof(vertices)));
        vertexBuffer->SetLayout({{ ShaderDataType::Float3, "a_Position" }, { ShaderDataType::Float4, "a_Color" }});
        m_VertexArray->AddVertexBuffer(vertexBuffer);
        std::uint32_t indices[] = { 0, 1, 2 };
        std::shared_ptr<IndexBuffer> indexBuffer;
        indexBuffer.reset(IndexBuffer::Create(indices, sizeof(indices) / sizeof(std::uint32_t)));
        m_VertexArray->SetIndexBuffer(indexBuffer);
        // Blue square, drawn behind the triangle.
        m_SquareVA.reset(VertexArray::Create());
        float squareVertices[] = {
            -0.75f, -0.75f, 0.0f,
            0.75f, -0.75f, 0.0f,
            0.75f,  0.75f, 0.0f,
            -0.75f,  0.75f, 0.0f
        };
        std::shared_ptr<VertexBuffer> squareVB;
        squareVB.reset(VertexBuffer::Create(squareVertices, sizeof(squareVertices)));
        squareVB->SetLayout({{ ShaderDataType::Float3, "a_Position" }});
        m_SquareVA->AddVertexBuffer(squareVB);
        std::uint32_t squareIndices[] = { 0, 1, 2, 2, 3, 0 };
        std::shared_ptr<IndexBuffer> squareIB;
        squareIB.reset(IndexBuffer::Create(squareIndices, sizeof(squareIndices) / sizeof(std::uint32_t)));
        m_SquareVA->SetIndexBuffer(squareIB);
        const std::string vertexSrc = R"glsl(#version 330 core
            layout(location = 0) in vec3 a_Position;
            layout(location = 1) in vec4 a_Color;

            out vec3 v_Position;
            out vec4 v_Color;

            void main()
            {
                v_Position = a_Position;
                v_Color = a_Color;
                gl_Position = vec4(a_Position, 1.0);
            }
            )glsl";
        const std::string fragmentSrc = R"glsl(#version 330 core
            layout(location = 0) out vec4 color;

            in vec3 v_Position;
            in vec4 v_Color;

            void main()
            {
                color = vec4(v_Position * 0.5 + 0.5, 1.0);
                color = v_Color;
            }
            )glsl";
        m_Shader.reset(new Shader(vertexSrc, fragmentSrc));
        const std::string blueShaderVertexSrc = R"glsl(#version 330 core
            layout(location = 0) in vec3 a_Position;

            out vec3 v_Position;

            void main()
            {
                v_Position = a_Position;
                gl_Position = vec4(a_Position, 1.0);
            }
            )glsl";
        const std::string blueShaderFragmentSrc = R"glsl(#version 330 core
            layout(location = 0) out vec4 color;

            in vec3 v_Position;

            void main()
            {
                color = vec4(0.2, 0.3, 0.8, 1.0);
            }
            )glsl";
        m_BlueShader.reset(new Shader(blueShaderVertexSrc, blueShaderFragmentSrc));
    }

    Application::~Application() = default;

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
            glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);

            m_BlueShader->Bind();
            m_SquareVA->Bind();
            glDrawElements(GL_TRIANGLES, m_SquareVA->GetIndexBuffer()->GetCount(), GL_UNSIGNED_INT, nullptr);

            m_Shader->Bind();
            m_VertexArray->Bind();
            glDrawElements(GL_TRIANGLES, m_VertexArray->GetIndexBuffer()->GetCount(), GL_UNSIGNED_INT, nullptr);

            for (Layer* layer : m_LayerStack)
                layer->OnUpdate();
            m_ImGuiLayer->Begin();
            for (Layer* layer : m_LayerStack)
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