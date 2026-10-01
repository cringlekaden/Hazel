#include "hzpch.h"

#include "Platform/OpenGL/OpenGLRendererAPI.h"

#include <glad/glad.h>

namespace Hazel {

#ifdef HZ_DEBUG
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
#endif


    void OpenGLRendererAPI::Init()
    {
        HZ_PROFILE_FUNCTION();
#ifdef HZ_DEBUG
        if (GLAD_GL_VERSION_4_3 && glDebugMessageCallback && glDebugMessageControl) {
            glEnable(GL_DEBUG_OUTPUT);
            glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
            glDebugMessageCallback(OpenGLMessageCallback, nullptr);
            glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DEBUG_SEVERITY_NOTIFICATION,
                                  0, nullptr, GL_FALSE);
        }
#endif
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glEnable(GL_DEPTH_TEST);
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

    void OpenGLRendererAPI::DrawIndexed(const Ref<VertexArray>& vertexArray, std::uint32_t indexCount)
    {
        const std::uint32_t count = indexCount != 0 ? indexCount : vertexArray->GetIndexBuffer()->GetCount();
        glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(count), GL_UNSIGNED_INT, nullptr);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
}