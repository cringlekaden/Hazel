#include "hzpch.h"

#include "Platform/OpenGL/OpenGLRendererAPI.h"
#include "Platform/OpenGL/OpenGLCapabilities.h"

#include <glad/glad.h>
#include <algorithm>

namespace Hazel {
    RasterState OpenGLRendererAPI::GetRasterState() const {
        RasterState state;state.DepthTest=glIsEnabled(GL_DEPTH_TEST);state.ClipEnabled=glIsEnabled(GL_SCISSOR_TEST);
        glGetFloatv(GL_LINE_WIDTH,&state.LineWidth);
        glGetIntegerv(GL_SCISSOR_BOX,&state.ClipRect.x);glGetIntegerv(GL_VIEWPORT,&state.Viewport.x);return state;
    }
    void OpenGLRendererAPI::SetRasterState(const RasterState& state) {
        if(state.DepthTest)glEnable(GL_DEPTH_TEST);else glDisable(GL_DEPTH_TEST);
        if(state.ClipEnabled)glEnable(GL_SCISSOR_TEST);else glDisable(GL_SCISSOR_TEST);
        SetLineWidth(state.LineWidth);
        glScissor(state.ClipRect.x,state.ClipRect.y,state.ClipRect.z,state.ClipRect.w);
    }


    // Adapted target diagnostics; debug output became core in 4.3.
    static void APIENTRY OpenGLMessageCallback(GLenum, GLenum, GLuint, GLenum severity,
                                               GLsizei, const GLchar* message, const void*)
    {
        switch (severity) {
            case GL_DEBUG_SEVERITY_HIGH: HZ_CORE_CRITICAL("OpenGL: {}", message); break;
            case GL_DEBUG_SEVERITY_MEDIUM: HZ_CORE_ERROR("OpenGL: {}", message); break;
            case GL_DEBUG_SEVERITY_LOW: HZ_CORE_WARN("OpenGL: {}", message); break;
            default: HZ_CORE_TRACE("OpenGL: {}", message); break;
        }
    }


    void OpenGLRendererAPI::Init(const RendererSettings& requested)
    {
        HZ_PROFILE_FUNCTION();
        OpenGLCapabilities::Configure(requested);
        HZ_CORE_INFO("Renderer requests/effective:\n{}", RendererPolicy::Describe(OpenGLCapabilities::GetResolution()));
        const auto& caps = OpenGLCapabilities::Get();
        const auto& settings = OpenGLCapabilities::GetSettings();
        HZ_CORE_INFO("Renderer paths: bind-based OpenGL 4.1 resources; {}; {} texture slots (device {}); debug output {}",
            OpenGLCapabilities::UseShaderBinaries() ? "SPIR-V specialization" : "shaderc/Cross to GLSL 410",
            settings.TextureSlots, caps.MaxTextureSlots, settings.EnableDebugOutput);
        HZ_CORE_INFO("Framebuffer limits: size {}; color attachments {}; draw buffers {}; samples color/integer/depth {}/{}/{}",
            caps.MaxTextureSize, caps.MaxColorAttachments, caps.MaxDrawBuffers, caps.MaxColorSamples, caps.MaxIntegerSamples, caps.MaxDepthSamples);
        if (caps.DebugOutput) {
            glDisable(GL_DEBUG_OUTPUT);
            glDisable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
            glDebugMessageCallback(nullptr, nullptr);
        }
        if (settings.EnableDebugOutput) {
            glEnable(GL_DEBUG_OUTPUT);
            glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
            glDebugMessageCallback(OpenGLMessageCallback, nullptr);
            glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DEBUG_SEVERITY_NOTIFICATION,
                                  0, nullptr, GL_FALSE);
        }
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_LINE_SMOOTH);
    }

    void OpenGLRendererAPI::SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height)
    {
        glViewport(static_cast<GLint>(x), static_cast<GLint>(y), static_cast<GLsizei>(width), static_cast<GLsizei>(height));
    }

    void OpenGLRendererAPI::SetClearColor(const glm::vec4& color)
    {
        glClearColor(color.r, color.g, color.b, color.a);
    }

    void OpenGLRendererAPI::Clear()
    {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    void OpenGLRendererAPI::DrawIndexed(const Ref<VertexArray>& vertexArray, std::uint32_t indexCount, std::uint32_t firstIndex)
    {
        vertexArray->Bind();
        const std::uint32_t count = indexCount != 0 ? indexCount : vertexArray->GetIndexBuffer()->GetCount();
        glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(count), GL_UNSIGNED_INT, reinterpret_cast<const void*>(uintptr_t(firstIndex)*sizeof(uint32_t)));
    }
    void OpenGLRendererAPI::DrawLines(const Ref<VertexArray>& vertexArray, std::uint32_t count, std::uint32_t firstVertex)
    {
        vertexArray->Bind();
        glDrawArrays(GL_LINES,static_cast<GLint>(firstVertex),static_cast<GLsizei>(count));
    }
    void OpenGLRendererAPI::SetLineWidth(float width) {
        const auto& caps = OpenGLCapabilities::Get();
        glLineWidth(std::clamp(width, caps.MinLineWidth, caps.MaxLineWidth));
    }
    void OpenGLRendererAPI::Shutdown() { OpenGLCapabilities::Reset(); }
    const RendererCapabilities& OpenGLRendererAPI::GetCapabilities() const { return OpenGLCapabilities::Get(); }
    const RendererResolution& OpenGLRendererAPI::GetResolution() const { return OpenGLCapabilities::GetResolution(); }
    const RendererSettings& OpenGLRendererAPI::GetSettings() const { return OpenGLCapabilities::GetSettings(); }
    std::uint32_t OpenGLRendererAPI::GetMaxTextureSlots() const
    {
        return OpenGLCapabilities::GetSettings().TextureSlots;
    }
}
