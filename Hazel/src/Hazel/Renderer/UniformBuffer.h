// Adapted from TheCherno/Hazel 1feb705: local headers/ownership and OpenGL 4.2 compatibility.
#pragma once

#include "Hazel/Core/Core.h"
#include <cstdint>

namespace Hazel {

	class UniformBuffer
	{
	public:
		virtual ~UniformBuffer() {}
		virtual void SetData(const void* data, std::uint32_t size, std::uint32_t offset = 0) = 0;

		static Ref<UniformBuffer> Create(std::uint32_t size, std::uint32_t binding);
	};

}
