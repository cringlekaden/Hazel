#include "hzpch.h"

#include "Hazel/Renderer/Renderer2D.h"
#include "Hazel/Renderer/RenderCommand.h"
#include "Hazel/Renderer/Shader.h"
#include "Hazel/Renderer/VertexArray.h"

#include <glm/gtc/matrix_transform.hpp>

#include <cstdint>

namespace Hazel {

    struct Renderer2DStorage
    {
        Ref<VertexArray> QuadVertexArray;
        Ref<Shader> FlatColorShader;
    };

    static Scope<Renderer2DStorage> s_Data;

    void Renderer2D::Init()
    {
        s_Data = CreateScope<Renderer2DStorage>();
        s_Data->QuadVertexArray = VertexArray::Create();
        float squareVertices[] = {
            -0.5f, -0.5f, 0.0f,
             0.5f, -0.5f, 0.0f,
             0.5f,  0.5f, 0.0f,
            -0.5f,  0.5f, 0.0f
        };
        Ref<VertexBuffer> squareVB = VertexBuffer::Create(squareVertices, sizeof(squareVertices));
        squareVB->SetLayout({{ ShaderDataType::Float3, "a_Position" }});
        s_Data->QuadVertexArray->AddVertexBuffer(squareVB);
        std::uint32_t squareIndices[] = { 0, 1, 2, 2, 3, 0 };
        Ref<IndexBuffer> squareIB = IndexBuffer::Create(squareIndices, 6);
        s_Data->QuadVertexArray->SetIndexBuffer(squareIB);
        s_Data->FlatColorShader = Shader::Create("assets/shaders/FlatColor.glsl");
    }

    void Renderer2D::Shutdown()
    {
        s_Data.reset();
    }

    void Renderer2D::BeginScene(const OrthographicCamera& camera)
    {
        s_Data->FlatColorShader->Bind();
        s_Data->FlatColorShader->SetMat4("u_ViewProjection", camera.GetViewProjectionMatrix());
    }

    void Renderer2D::EndScene()
    {
    }

    void Renderer2D::DrawQuad(const glm::vec2& position, const glm::vec2& size, const glm::vec4& color)
    {
        DrawQuad({ position.x, position.y, 0.0f }, size, color);
    }

    void Renderer2D::DrawQuad(const glm::vec3& position, const glm::vec2& size, const glm::vec4& color)
    {
        s_Data->FlatColorShader->Bind();
        s_Data->FlatColorShader->SetFloat4("u_Color", color);
        const glm::mat4 transform =
            glm::translate(glm::mat4(1.0f), position) *
            glm::scale(glm::mat4(1.0f), { size.x, size.y, 1.0f });
        s_Data->FlatColorShader->SetMat4("u_Transform", transform);
        s_Data->QuadVertexArray->Bind();
        RenderCommand::DrawIndexed(s_Data->QuadVertexArray);
    }
}