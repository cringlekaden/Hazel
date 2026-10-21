#pragma once

#include "Hazel/Renderer/VertexArray.h"
#include "Hazel/Renderer/RendererCapabilities.h"
#include "Hazel/Renderer/RendererPolicy.h"

#include <glm/glm.hpp>
#include <memory>

namespace Hazel {

    struct RasterState { bool DepthTest=true, ClipEnabled=false; glm::ivec4 ClipRect{}, Viewport{}; float LineWidth=1; };

    class RendererAPI
    {
    public:
        enum class API
        {
            None = 0,
            OpenGL = 1
        };

        virtual ~RendererAPI() = default;
        
        virtual void Init(const RendererSettings& settings = {}) = 0;
        virtual void Shutdown() {}
        virtual const RendererCapabilities& GetCapabilities() const = 0;
        virtual const RendererSettings& GetSettings() const = 0;
        virtual const RendererResolution& GetResolution() const = 0;
        virtual RasterState GetRasterState() const = 0;
        virtual void SetRasterState(const RasterState& state) = 0;
        // Submission-local text coverage scope. Backends preserve target-0 blend
        // state, enable source-over alpha only for text, then restore it exactly.
        virtual void BeginTextCompositing() = 0;
        virtual void SetTextCompositing(bool text) = 0;
        virtual void EndTextCompositing() = 0;
        virtual void SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height) = 0;
        virtual void SetClearColor(const glm::vec4& color) = 0;
        virtual void Clear() = 0;
        virtual void DrawIndexed(const Ref<VertexArray>& vertexArray, std::uint32_t indexCount = 0, std::uint32_t firstIndex = 0) = 0;
        virtual void DrawLines(const Ref<VertexArray>& vertexArray, std::uint32_t vertexCount, std::uint32_t firstVertex = 0) = 0;
        virtual void SetLineWidth(float width) = 0;
        virtual std::uint32_t GetMaxTextureSlots() const = 0;

        static API GetAPI() { return s_API; }
        static Scope<RendererAPI> Create();
    private:
        static API s_API;
    };
}
