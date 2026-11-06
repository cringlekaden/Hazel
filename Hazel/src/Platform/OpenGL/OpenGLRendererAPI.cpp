#include "hzpch.h"

#include "Platform/OpenGL/OpenGLRendererAPI.h"
#include "Platform/OpenGL/OpenGLCapabilities.h"

#include <glad/glad.h>
#include <algorithm>

namespace Hazel {
    void OpenGLRendererAPI::SetPickingMask(glm::bvec4 mask){glColorMaski(1,mask.x,mask.y,mask.z,mask.w);}
    RasterState OpenGLRendererAPI::GetRasterState() const {
        RasterState state;
        GLboolean depthWrite;
        glGetBooleanv(GL_DEPTH_WRITEMASK, &depthWrite);
        state.DepthWrite = depthWrite;
        state.Blend = glIsEnabledi(GL_BLEND, 0);
        state.SRGB = glIsEnabled(GL_FRAMEBUFFER_SRGB);
        GLint compare, cull, face;
        glGetIntegerv(GL_DEPTH_FUNC, &compare);
        glGetIntegerv(GL_CULL_FACE_MODE, &cull);
        glGetIntegerv(GL_FRONT_FACE, &face);
        state.Compare = compare == GL_LEQUAL   ? DepthCompare::LessEqual
                        : compare == GL_ALWAYS ? DepthCompare::Always
                                               : DepthCompare::Less;
        state.Cull = !glIsEnabled(GL_CULL_FACE) ? CullFace::None
                     : cull == GL_FRONT         ? CullFace::Front
                                                : CullFace::Back;
        state.Clockwise = face == GL_CW;
        GLboolean color[4];
        glGetBooleani_v(GL_COLOR_WRITEMASK, 0, color);
        state.ColorMask = {color[0] != 0, color[1] != 0, color[2] != 0, color[3] != 0};
        state.DepthTest = glIsEnabled(GL_DEPTH_TEST);
        state.ClipEnabled = glIsEnabled(GL_SCISSOR_TEST);
        GLboolean mask[4];glGetBooleani_v(GL_COLOR_WRITEMASK,1,mask);state.PickingMask={mask[0]!=0,mask[1]!=0,mask[2]!=0,mask[3]!=0};
        glGetFloatv(GL_LINE_WIDTH,&state.LineWidth);
        glGetIntegerv(GL_SCISSOR_BOX,&state.ClipRect.x);glGetIntegerv(GL_VIEWPORT,&state.Viewport.x);return state;
    }
    void OpenGLRendererAPI::SetRasterState(const RasterState& state) {
        glDepthMask(state.DepthWrite);
        glDepthFunc(state.Compare == DepthCompare::Less        ? GL_LESS
                    : state.Compare == DepthCompare::LessEqual ? GL_LEQUAL
                                                               : GL_ALWAYS);
        if (state.Cull == CullFace::None)
            glDisable(GL_CULL_FACE);
        else
        {
            glEnable(GL_CULL_FACE);
            glCullFace(state.Cull == CullFace::Back ? GL_BACK : GL_FRONT);
        }
        glFrontFace(state.Clockwise ? GL_CW : GL_CCW);
        if (state.Blend)
        {
            glEnablei(GL_BLEND, 0);
            glBlendEquationSeparatei(0, GL_FUNC_ADD, GL_FUNC_ADD);
            glBlendFuncSeparatei(0, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE,
                                 GL_ONE_MINUS_SRC_ALPHA);
        }
        else
            glDisablei(GL_BLEND, 0);
        if (state.SRGB)
            glEnable(GL_FRAMEBUFFER_SRGB);
        else
            glDisable(GL_FRAMEBUFFER_SRGB);
        glColorMaski(0, state.ColorMask.x, state.ColorMask.y, state.ColorMask.z, state.ColorMask.w);
        glViewport(state.Viewport.x, state.Viewport.y, state.Viewport.z, state.Viewport.w);
        if(state.DepthTest)glEnable(GL_DEPTH_TEST);else glDisable(GL_DEPTH_TEST);
        if(state.ClipEnabled)glEnable(GL_SCISSOR_TEST);else glDisable(GL_SCISSOR_TEST);
        SetLineWidth(state.LineWidth);SetPickingMask(state.PickingMask);
        glScissor(state.ClipRect.x,state.ClipRect.y,state.ClipRect.z,state.ClipRect.w);
    }

    void OpenGLRendererAPI::PushState()
    {
        if (m_SavedStates.size() >= 16)
            throw std::logic_error("Render state nesting exceeded");
        const bool integerBlend=glIsEnabledi(GL_BLEND,1);
        GLfloat clear[4];
        glGetFloatv(GL_COLOR_CLEAR_VALUE, clear);
        GLint polygon[2];
        glGetIntegerv(GL_POLYGON_MODE, polygon);
        GLint readBuffer;
        glGetIntegerv(GL_READ_BUFFER, &readBuffer);
        std::vector<GLenum> drawBuffers(GetCapabilities().MaxDrawBuffers);
        for (unsigned i = 0; i < drawBuffers.size(); ++i)
        {
            GLint b;
            glGetIntegerv(GL_DRAW_BUFFER0 + i, &b);
            drawBuffers[i] = b;
        }
        auto raster = GetRasterState();
        GLint program, vao, active, array, uniform, read, draw, depthFunc, cull, front;
        glGetIntegerv(GL_CURRENT_PROGRAM, &program);
        glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vao);
        glGetIntegerv(GL_ACTIVE_TEXTURE, &active);
        glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &array);
        glGetIntegerv(GL_UNIFORM_BUFFER_BINDING, &uniform);
        glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &read);
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &draw);
        glGetIntegerv(GL_DEPTH_FUNC, &depthFunc);
        glGetIntegerv(GL_CULL_FACE_MODE, &cull);
        glGetIntegerv(GL_FRONT_FACE, &front);
        std::array<GLint, 32> textures{}, samplers{};
        const auto textureCount = std::min(32u, GetCapabilities().MaxTextureSlots);
        std::array<GLint, 6> ubos{}, blend{};
        for (unsigned i = 0; i < textureCount; ++i)
        {
            glActiveTexture(GL_TEXTURE0 + i);
            glGetIntegerv(GL_TEXTURE_BINDING_2D, &textures[i]);
            glGetIntegerv(GL_SAMPLER_BINDING, &samplers[i]);
        }
        glActiveTexture(active);
        for (unsigned i = 0; i < 6; ++i)
            glGetIntegeri_v(GL_UNIFORM_BUFFER_BINDING, i, &ubos[i]);
        const GLenum fields[]{GL_BLEND_SRC_RGB,   GL_BLEND_DST_RGB,      GL_BLEND_SRC_ALPHA,
                              GL_BLEND_DST_ALPHA, GL_BLEND_EQUATION_RGB, GL_BLEND_EQUATION_ALPHA};
        for (int i = 0; i < 6; ++i)
            glGetIntegeri_v(fields[i], 0, &blend[i]);
        m_SavedStates.emplace_back([=] {
            SetRasterState(raster);
            if(integerBlend)glEnablei(GL_BLEND,1);else glDisablei(GL_BLEND,1);
            glClearColor(clear[0], clear[1], clear[2], clear[3]);
            glPolygonMode(GL_FRONT_AND_BACK, polygon[0]);
            glDepthFunc(depthFunc);
            glCullFace(cull);
            glFrontFace(front);
            glBlendFuncSeparatei(0, blend[0], blend[1], blend[2], blend[3]);
            glBlendEquationSeparatei(0, blend[4], blend[5]);
            glUseProgram(program);
            glBindVertexArray(vao);
            glBindBuffer(GL_ARRAY_BUFFER, array);
            for (unsigned i = 0; i < 6; ++i)
                glBindBufferBase(GL_UNIFORM_BUFFER, i, ubos[i]);
            glBindBuffer(GL_UNIFORM_BUFFER, uniform);
            for (unsigned i = 0; i < textureCount; ++i)
            {
                glActiveTexture(GL_TEXTURE0 + i);
                glBindTexture(GL_TEXTURE_2D, textures[i]);
                glBindSampler(i, samplers[i]);
            }
            glActiveTexture(active);
            glBindFramebuffer(GL_READ_FRAMEBUFFER, read);
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, draw);
            glReadBuffer(readBuffer);
            if (draw)
                glDrawBuffers(GLsizei(drawBuffers.size()), drawBuffers.data());
            else
                glDrawBuffer(drawBuffers[0]);
        });
    }
    void OpenGLRendererAPI::PopState()
    {
        if (m_SavedStates.empty())
            throw std::logic_error("Unbalanced render state");
        auto restore = std::move(m_SavedStates.back());
        m_SavedStates.pop_back();
        restore();
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
        glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_LINE_SMOOTH);
    }

    void OpenGLRendererAPI::BeginTextCompositing() {
        if(m_TextBlend.Active)throw std::logic_error("Nested text compositing scope");
        glGetIntegeri_v(GL_BLEND_SRC_RGB,0,&m_TextBlend.SourceRGB);
        glGetIntegeri_v(GL_BLEND_DST_RGB,0,&m_TextBlend.DestinationRGB);
        glGetIntegeri_v(GL_BLEND_SRC_ALPHA,0,&m_TextBlend.SourceAlpha);
        glGetIntegeri_v(GL_BLEND_DST_ALPHA,0,&m_TextBlend.DestinationAlpha);
        glGetIntegeri_v(GL_BLEND_EQUATION_RGB,0,&m_TextBlend.EquationRGB);
        glGetIntegeri_v(GL_BLEND_EQUATION_ALPHA,0,&m_TextBlend.EquationAlpha);
        m_TextBlend.Active=true;m_TextBlend.Text=false;
    }
    void OpenGLRendererAPI::SetTextCompositing(bool text) {
        if(!m_TextBlend.Active || m_TextBlend.Text==text)return;
        if(m_TextBlend.SourceAlpha!=GL_ONE || m_TextBlend.DestinationAlpha!=GL_ONE_MINUS_SRC_ALPHA)
            glBlendFuncSeparatei(0,m_TextBlend.SourceRGB,m_TextBlend.DestinationRGB,
                text?GL_ONE:m_TextBlend.SourceAlpha,text?GL_ONE_MINUS_SRC_ALPHA:m_TextBlend.DestinationAlpha);
        if(m_TextBlend.EquationAlpha!=GL_FUNC_ADD)
            glBlendEquationSeparatei(0,m_TextBlend.EquationRGB,text?GL_FUNC_ADD:m_TextBlend.EquationAlpha);
        m_TextBlend.Text=text;
    }
    void OpenGLRendererAPI::EndTextCompositing() {
        SetTextCompositing(false);m_TextBlend.Active=false;
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
