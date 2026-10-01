// Adapted from TheCherno/Hazel 1feb705: local headers/ownership and OpenGL 4.2 compatibility.
#include "hzpch.h"
#include "UniformBuffer.h"

#include "Hazel/Renderer/Renderer.h"
#include "Platform/OpenGL/OpenGLUniformBuffer.h"

namespace Hazel {

	Ref<UniformBuffer> UniformBuffer::Create(std::uint32_t size, std::uint32_t binding)
	{
		switch (Renderer::GetAPI())
		{
			case RendererAPI::API::None:    HZ_CORE_ASSERT(false, "RendererAPI::None is currently not supported!"); return nullptr;
			case RendererAPI::API::OpenGL:  return CreateRef<OpenGLUniformBuffer>(size, binding);
		}

		HZ_CORE_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
	}

}
