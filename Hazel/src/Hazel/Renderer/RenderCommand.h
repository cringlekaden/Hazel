#pragma once

#include "Hazel/Renderer/RendererAPI.h"

namespace Hazel {

    class RenderCommand
    {
    public:
        static void Init(const RendererSettings& settings = {})
        {
            s_DrawSubmissionCount = 0;
            s_RendererAPI->Init(settings);
        }

        static void Shutdown() { s_RendererAPI->Shutdown(); }
        static const RendererCapabilities& GetCapabilities() { return s_RendererAPI->GetCapabilities(); }
        static const RendererResolution& GetResolution() { return s_RendererAPI->GetResolution(); }
        static const RendererSettings& GetSettings() { return s_RendererAPI->GetSettings(); }

        class StateScope {public:StateScope(){s_RendererAPI->PushState();}~StateScope(){s_RendererAPI->PopState();}StateScope(const StateScope&)=delete;};
        static void SetPickingMask(glm::bvec4 mask){s_RendererAPI->SetPickingMask(mask);}
        static RasterState GetRasterState() {return s_RendererAPI->GetRasterState();}
        static void SetRasterState(const RasterState& state) {s_RendererAPI->SetRasterState(state);}

        static void BeginTextCompositing(){s_RendererAPI->BeginTextCompositing();}
        static void SetTextCompositing(bool text){s_RendererAPI->SetTextCompositing(text);}
        static void EndTextCompositing(){s_RendererAPI->EndTextCompositing();}
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

        static void DrawIndexed(const Ref<VertexArray>& vertexArray, std::uint32_t indexCount = 0, std::uint32_t firstIndex = 0)
        {
            s_RendererAPI->DrawIndexed(vertexArray, indexCount, firstIndex);
            ++s_DrawSubmissionCount;
        }
        static void DrawLines(const Ref<VertexArray>& vertexArray, std::uint32_t vertexCount, std::uint32_t firstVertex = 0)
        {
            s_RendererAPI->DrawLines(vertexArray, vertexCount, firstVertex);
            ++s_DrawSubmissionCount;
        }
        static void SetLineWidth(float width) { s_RendererAPI->SetLineWidth(width); }
        static std::uint32_t GetMaxTextureSlots() { return s_RendererAPI->GetMaxTextureSlots(); }
        // Monotonic within the current renderer lifetime; counts engine API
        // submissions, independently of Renderer2D's primitive/batch statistics.
        static std::uint64_t GetDrawSubmissionCount() { return s_DrawSubmissionCount; }
    private:
        inline static std::uint64_t s_DrawSubmissionCount = 0;
        static Scope<RendererAPI> s_RendererAPI;
    };
}
