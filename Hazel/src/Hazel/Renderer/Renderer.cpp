#include "hzpch.h"

#include "Hazel/Renderer/Renderer.h"
#include "Hazel/Renderer/Drawing/Renderer2D.h"
#include "Hazel/Renderer/Drawing/Renderer3D.h"

namespace Hazel {

    Scope<Renderer::SceneData> Renderer::s_SceneData = CreateScope<Renderer::SceneData>();

    namespace { bool s_DrawingStarted = false; }

    void Renderer::Init(const RendererSettings& settings, RendererInitialization initialization)
    {
        HZ_PROFILE_FUNCTION();
        RenderCommand::Init(settings);
        if (initialization == RendererInitialization::PresentationOnly) return;
        if (GetAPI() != RendererAPI::API::OpenGL)
            throw std::runtime_error("Metal drawing services are unavailable in M1; use PresentationOnly without ImGui");
        s_DrawingStarted = true; // also clean up a partially initialized Renderer2D
        Renderer2D::Init();
    }

    void Renderer::Shutdown()
    {
        if (s_DrawingStarted) {
            Renderer3D::Shutdown();
            Renderer2D::Shutdown();
            s_DrawingStarted = false;
        }
        RenderCommand::Shutdown();
    }

    void Renderer::BeginScene(OrthographicCamera& camera)
    {
        s_SceneData->ViewProjectionMatrix = camera.GetViewProjectionMatrix();
    }

    void Renderer::EndScene()
    {
    }

    void Renderer::Submit(
        const Ref<Shader>& shader,
        const Ref<VertexArray>& vertexArray,
        const glm::mat4& transform)
    {
        shader->Bind();
        shader->SetMat4("u_ViewProjection", s_SceneData->ViewProjectionMatrix);
        shader->SetMat4("u_Transform", transform);
        vertexArray->Bind();
        RenderCommand::DrawIndexed(vertexArray);
    }

    void Renderer::OnWindowResize(uint32_t width, uint32_t height)
    {
        RenderCommand::SetViewport(0, 0, width, height);
    }
}
