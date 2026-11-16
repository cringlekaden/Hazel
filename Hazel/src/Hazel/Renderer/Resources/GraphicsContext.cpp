// Adapted from TheCherno/Hazel 1feb705 for local ownership and native Linux/Windows portability.
#include "hzpch.h"
#include <stdexcept>
#include <GLFW/glfw3.h>
#include "Hazel/Renderer/Resources/GraphicsContext.h"

#include "Hazel/Renderer/Renderer.h"
#include "Platform/OpenGL/OpenGLContext.h"
#ifdef HZ_PLATFORM_MACOS
#include "Platform/macOS/Metal/MetalContext.h"
#endif

namespace Hazel {

	Scope<GraphicsContext> GraphicsContext::Create(void* window)
	{
		switch (Renderer::GetAPI())
		{
			case RendererAPI::API::None:    HZ_CORE_ASSERT(false, "RendererAPI::None is currently not supported!"); return nullptr;
			case RendererAPI::API::OpenGL:  return CreateScope<OpenGLContext>(static_cast<GLFWwindow*>(window));
#ifdef HZ_PLATFORM_MACOS
            case RendererAPI::API::Metal: return CreateScope<MetalContext>(static_cast<GLFWwindow*>(window));
#endif
		}

		HZ_CORE_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
	}

    void GraphicsContext::ConfigureWindowHints()
    {
        glfwDefaultWindowHints(); // No stale NO_API hints after a prior renderer lifetime.
        switch (Renderer::GetAPI()) {
            case RendererAPI::API::OpenGL: OpenGLContext::ConfigureWindowHints(); return;
#ifdef HZ_PLATFORM_MACOS
            case RendererAPI::API::Metal: glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); return;
#endif
            default: throw std::runtime_error("Unsupported graphics API for window creation");
        }
    }

}
