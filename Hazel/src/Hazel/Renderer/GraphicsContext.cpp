// Adapted from TheCherno/Hazel 1feb705 for local ownership and native Linux/Windows portability.
#include "hzpch.h"
#include <stdexcept>
#include "Hazel/Renderer/GraphicsContext.h"

#include "Hazel/Renderer/Renderer.h"
#include "Platform/OpenGL/OpenGLContext.h"

namespace Hazel {

	Scope<GraphicsContext> GraphicsContext::Create(void* window)
	{
		switch (Renderer::GetAPI())
		{
			case RendererAPI::API::None:    HZ_CORE_ASSERT(false, "RendererAPI::None is currently not supported!"); return nullptr;
			case RendererAPI::API::OpenGL:  return CreateScope<OpenGLContext>(static_cast<GLFWwindow*>(window));
		}

		HZ_CORE_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
	}

    void GraphicsContext::ConfigureWindowHints()
    {
        switch (Renderer::GetAPI()) {
            case RendererAPI::API::OpenGL: OpenGLContext::ConfigureWindowHints(); return;
            default: throw std::runtime_error("Unsupported graphics API for window creation");
        }
    }

}
