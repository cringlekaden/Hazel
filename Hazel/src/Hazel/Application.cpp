#include "hzpch.h"

#include "Hazel/Window.h"
#include "Hazel/Application.h"
#include "Hazel/Log.h"
#include <memory>

#include <glad/glad.h>
namespace Hazel
{
    Application* Application::s_Instance = nullptr;

    static GLenum ShaderDataTypeToOpenGLBaseType(ShaderDataType type)
    {
        switch (type)
        {
            case ShaderDataType::None:   break;
            case ShaderDataType::Float:  return GL_FLOAT;
            case ShaderDataType::Float2: return GL_FLOAT;
            case ShaderDataType::Float3: return GL_FLOAT;
            case ShaderDataType::Float4: return GL_FLOAT;
            case ShaderDataType::Mat3:   return GL_FLOAT;
            case ShaderDataType::Mat4:   return GL_FLOAT;
            case ShaderDataType::Int:    return GL_INT;
            case ShaderDataType::Int2:   return GL_INT;
            case ShaderDataType::Int3:   return GL_INT;
            case ShaderDataType::Int4:   return GL_INT;
            case ShaderDataType::Bool:   return GL_BOOL;
        }
        HZ_CORE_ASSERT(false, "Unknown ShaderDataType...");
        return 0;
    }

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
            // position                            // color
            -0.5f, -0.5f, 0.0f,       0.8f, 0.2f, 0.8f, 1.0f,
            0.5f, -0.5f, 0.0f,       0.2f, 0.3f, 0.8f, 1.0f,
            0.0f,  0.5f, 0.0f,       0.8f, 0.8f, 0.2f, 1.0f
        };
        m_VertexBuffer.reset(VertexBuffer::Create(vertices, sizeof(vertices)));
        BufferLayout layout = {{ ShaderDataType::Float3, "a_Position" }, { ShaderDataType::Float4, "a_Color" }};
        m_VertexBuffer->SetLayout(layout);
        std::uint32_t index = 0;
        for (const auto& element : m_VertexBuffer->GetLayout())
        {
            glEnableVertexAttribArray(index);
            glVertexAttribPointer(
                index,
                element.GetComponentCount(),
                ShaderDataTypeToOpenGLBaseType(element.Type),
                element.Normalized ? GL_TRUE : GL_FALSE,
                m_VertexBuffer->GetLayout().GetStride(),
                reinterpret_cast<const void*>(static_cast<std::uintptr_t>(element.Offset)));
            ++index;
        }
        std::uint32_t indices[] = { 0, 1, 2 };
        m_IndexBuffer.reset(IndexBuffer::Create(indices, sizeof(indices) / sizeof(std::uint32_t)));
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
                color = v_Color;
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