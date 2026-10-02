#pragma once

#include "Hazel/Renderer/RendererAPI.h"

namespace Hazel {

    class OpenGLRendererAPI : public RendererAPI
    {
    public:
        void Init(const RendererSettings& settings = {}) override;
        void Shutdown() override;
        const RendererCapabilities& GetCapabilities() const override;
        const RendererSettings& GetSettings() const override;
        void SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height) override;
        void SetClearColor(const glm::vec4& color) override;
        void Clear() override;
        void DrawIndexed(const Ref<VertexArray>& vertexArray, std::uint32_t indexCount = 0) override;
        void DrawLines(const Ref<VertexArray>& vertexArray, std::uint32_t vertexCount) override;
        void SetLineWidth(float width) override;
        std::uint32_t GetMaxTextureSlots() const override;
    };
}
