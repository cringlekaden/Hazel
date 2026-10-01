// Adapted from TheCherno/Hazel 1feb705: local headers/ownership and OpenGL 4.2 compatibility.
#pragma once

#include "Hazel/Renderer/UniformBuffer.h"

namespace Hazel {

	class OpenGLUniformBuffer : public UniformBuffer
	{
	public:
		OpenGLUniformBuffer(std::uint32_t size, std::uint32_t binding);
		virtual ~OpenGLUniformBuffer();

		virtual void SetData(const void* data, std::uint32_t size, std::uint32_t offset = 0) override;
	private:
		std::uint32_t m_RendererID = 0;
	};
}
