#pragma once

#include "Hazel/Renderer/RenderCommand.h"
#include "Hazel/Renderer/OrthographicCamera.h"
#include "Hazel/Renderer/Shader.h"

namespace Hazel {

    class Renderer
    {
    public:
        static void Init(const RendererSettings& settings = {});
        static const RendererCapabilities& GetCapabilities() { return RenderCommand::GetCapabilities(); }
        static const RendererResolution& GetResolution() { return RenderCommand::GetResolution(); }
        static const RendererSettings& GetSettings() { return RenderCommand::GetSettings(); }
        static void Shutdown();
        static void BeginScene(OrthographicCamera& camera);
        static void EndScene();

        static void Submit(
            const Ref<Shader>& shader,
            const Ref<VertexArray>& vertexArray,
            const glm::mat4& transform = glm::mat4(1.0f));

        static void OnWindowResize(uint32_t width, uint32_t height);

        static RendererAPI::API GetAPI()
        {
            return RendererAPI::GetAPI();
        }

    private:
        struct SceneData
        {
            glm::mat4 ViewProjectionMatrix;
        };

        static Scope<SceneData> s_SceneData;
    };
}