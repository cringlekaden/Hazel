// Adapted from TheCherno/Hazel 1feb705: local headers/ownership and OpenGL 4.2 compatibility.
#include "hzpch.h"
#include "Hazel/Renderer/Resources/UniformBuffer.h"

#include "Hazel/Renderer/Renderer.h"
#include "Platform/OpenGL/OpenGLUniformBuffer.h"

#ifdef HZ_PLATFORM_MACOS
#include "Platform/macOS/Metal/MetalBuffer.h"
#endif

namespace Hazel {

	Ref<UniformBuffer> UniformBuffer::Create(std::uint32_t size, std::uint32_t binding)
	{
		switch (Renderer::GetAPI())
		{
#ifdef HZ_PLATFORM_MACOS
            case RendererAPI::API::Metal: return CreateRef<MetalUniformBuffer>(size, binding);
#else
            case RendererAPI::API::Metal: throw std::runtime_error("Metal requires macOS");
#endif

			case RendererAPI::API::None:    HZ_CORE_ASSERT(false, "RendererAPI::None is currently not supported!"); return nullptr;
			case RendererAPI::API::OpenGL:  return CreateRef<OpenGLUniformBuffer>(size, binding);
		}

		HZ_CORE_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
	}

}
