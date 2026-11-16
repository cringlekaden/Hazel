#include "hzpch.h"
#include "Hazel/Renderer/RenderCommand.h"
#include <stdexcept>
namespace Hazel {
    Scope<RendererAPI> RenderCommand::s_RendererAPI;
    void RenderCommand::Configure(RendererAPI::API api) {
        if (s_RendererAPI) throw std::logic_error("Graphics backend is immutable until shutdown completes");
        RendererAPI::Select(api);
        try { s_RendererAPI = RendererAPI::Create(); }
        catch (...) { RendererAPI::Select(RendererAPI::API::OpenGL); throw; }
    }
    RendererAPI& RenderCommand::Backend() {
        if (!s_RendererAPI) throw std::logic_error("Graphics backend is not configured");
        return *s_RendererAPI;
    }
    void RenderCommand::Init(const RendererSettings& settings) {
        // Existing low-level OpenGL verification hosts initialize their own contexts.
        if (!s_RendererAPI) Configure(RendererAPI::API::OpenGL);
        s_DrawSubmissionCount = 0;
        Backend().Init(settings);
    }
    void RenderCommand::Shutdown() { if (s_RendererAPI) s_RendererAPI->Shutdown(); }
    void RenderCommand::Release() {
        s_RendererAPI.reset();
        RendererAPI::Select(RendererAPI::API::OpenGL);
    }
}
