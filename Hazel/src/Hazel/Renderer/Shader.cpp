#include "hzpch.h"

#include "Hazel/Renderer/Shader.h"
#include "Hazel/Renderer/Renderer.h"
#include "Hazel/Core.h"
#include "Platform/OpenGL/OpenGLShader.h"

namespace Hazel {

    Shader* Shader::Create(const std::string& vertexSource, const std::string& fragmentSource)
    {
        switch (Renderer::GetAPI())
        {
            case RendererAPI::API::None:
                HZ_CORE_ASSERT(false, "RendererAPI::None is invalid...");
                return nullptr;

            case RendererAPI::API::OpenGL:
                return new OpenGLShader(vertexSource, fragmentSource);
        }
        HZ_CORE_ASSERT(false, "Unknown RendererAPI...");
        return nullptr;
    }
}