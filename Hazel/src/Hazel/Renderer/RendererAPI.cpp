#include "hzpch.h"

#include "Hazel/Renderer/RendererAPI.h"
#include "Platform/OpenGL/OpenGLRendererAPI.h"
#include <stdexcept>

namespace Hazel {

    RendererAPI::API RendererAPI::s_API = RendererAPI::API::OpenGL;

    // Actual upstream factory, retaining Scope and release-mode diagnostics.
    Scope<RendererAPI> RendererAPI::Create()
    {
        switch (s_API) {
            case API::OpenGL: return CreateScope<OpenGLRendererAPI>();
            default: throw std::runtime_error("Unsupported renderer API");
        }
    }

}
