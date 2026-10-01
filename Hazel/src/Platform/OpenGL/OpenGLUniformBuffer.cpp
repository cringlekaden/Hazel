// Adapted from TheCherno/Hazel 1feb705: local headers/ownership and OpenGL 4.2 compatibility.
#include "hzpch.h"
#include "OpenGLUniformBuffer.h"

#include <glad/glad.h>

namespace Hazel {

	OpenGLUniformBuffer::OpenGLUniformBuffer(std::uint32_t size, std::uint32_t binding)
	{
		// Named buffer operations require newer OpenGL. Preserve the generic
		// binding while establishing the requested indexed UBO binding on 4.2.
		GLint previousBinding = 0;
		glGetIntegerv(GL_UNIFORM_BUFFER_BINDING, &previousBinding);
		glGenBuffers(1, &m_RendererID);
		glBindBuffer(GL_UNIFORM_BUFFER, m_RendererID);
		glBufferData(GL_UNIFORM_BUFFER, size, nullptr, GL_DYNAMIC_DRAW);
		glBindBufferBase(GL_UNIFORM_BUFFER, binding, m_RendererID);
		glBindBuffer(GL_UNIFORM_BUFFER, static_cast<GLuint>(previousBinding));
	}

	OpenGLUniformBuffer::~OpenGLUniformBuffer()
	{
		glDeleteBuffers(1, &m_RendererID);
	}


	void OpenGLUniformBuffer::SetData(const void* data, std::uint32_t size, std::uint32_t offset)
	{
		GLint previousBinding = 0;
		glGetIntegerv(GL_UNIFORM_BUFFER_BINDING, &previousBinding);
		glBindBuffer(GL_UNIFORM_BUFFER, m_RendererID);
		glBufferSubData(GL_UNIFORM_BUFFER, offset, size, data);
		glBindBuffer(GL_UNIFORM_BUFFER, static_cast<GLuint>(previousBinding));
	}

}
