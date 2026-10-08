#pragma once

#include "Hazel/Renderer/RendererAPI.h"

namespace Hazel {

    class RenderCommand
    {
    public:
        static void Init(const RendererSettings& settings = {})
        {
            s_RendererAPI->Init(settings);
        }

        static void Shutdown() { s_RendererAPI->Shutdown(); }
        static const RendererCapabilities& GetCapabilities() { return s_RendererAPI->GetCapabilities(); }
        static const RendererResolution& GetResolution() { return s_RendererAPI->GetResolution(); }
        static const RendererSettings& GetSettings() { return s_RendererAPI->GetSettings(); }

        static void SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height)
        {
            s_RendererAPI->SetViewport(x, y, width, height);
        }

        static void SetClearColor(const glm::vec4& color)
        {
            s_RendererAPI->SetClearColor(color);
        }

        static void Clear()
        {
            s_RendererAPI->Clear();
        }

        static void DrawIndexed(const Ref<VertexArray>& vertexArray, std::uint32_t indexCount = 0)
        {
            s_RendererAPI->DrawIndexed(vertexArray, indexCount);
        }
        static void DrawLines(const Ref<VertexArray>& vertexArray, std::uint32_t vertexCount)
        {
            s_RendererAPI->DrawLines(vertexArray, vertexCount);
        }
        static void SetLineWidth(float width) { s_RendererAPI->SetLineWidth(width); }
        static std::uint32_t GetMaxTextureSlots() { return s_RendererAPI->GetMaxTextureSlots(); }
    private:
        static Scope<RendererAPI> s_RendererAPI;
    };
}
