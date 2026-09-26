#include "hzpch.h"

#include "Platform/OpenGL/OpenGLTexture.h"
#include "Hazel/Log.h"

#include <glad/glad.h>
#include <stb_image.h>

#include <stdexcept>

namespace Hazel {

    OpenGLTexture2D::OpenGLTexture2D(const std::string& path) : m_Path(path)
    {
        stbi_set_flip_vertically_on_load(1);
        int width = 0;
        int height = 0;
        int channels = 0;
        stbi_uc* pixels = stbi_load(path.c_str(), &width, &height, &channels, 0);
        if (!pixels)
        {
            HZ_CORE_ERROR("Failed to load texture '{}': {}", path, stbi_failure_reason());
            throw std::runtime_error("Failed to load texture: " + path);
        }
        const GLenum format = channels == 4 ? GL_RGBA : channels == 3 ? GL_RGB : 0;
        const GLint internalFormat = channels == 4 ? GL_RGBA8 : channels == 3 ? GL_RGB8 : 0;
        if (format == 0 || width <= 0 || height <= 0)
        {
            stbi_image_free(pixels);
            HZ_CORE_ERROR("Unsupported texture format in '{}' ({} channels)", path, channels);
            throw std::runtime_error("Unsupported texture format: " + path);
        }
        m_Width = static_cast<std::uint32_t>(width);
        m_Height = static_cast<std::uint32_t>(height);
        glGenTextures(1, &m_RendererID);
        if (m_RendererID == 0)
        {
            stbi_image_free(pixels);
            throw std::runtime_error("Failed to create OpenGL texture: " + path);
        }
        glBindTexture(GL_TEXTURE_2D, m_RendererID);
        glTexParameteri(
            GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(
            GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(
            GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(
            GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        GLint previousAlignment = 4;
        glGetIntegerv(GL_UNPACK_ALIGNMENT, &previousAlignment);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(
            GL_TEXTURE_2D,
            0,
            internalFormat,
            width,
            height,
            0,
            format,
            GL_UNSIGNED_BYTE,
            pixels);
        glPixelStorei(GL_UNPACK_ALIGNMENT, previousAlignment);
        glBindTexture(GL_TEXTURE_2D, 0);
        stbi_image_free(pixels);
    }

    OpenGLTexture2D::~OpenGLTexture2D()
    {
        if (m_RendererID != 0)
            glDeleteTextures(1, &m_RendererID);
    }

    void OpenGLTexture2D::Bind(std::uint32_t slot) const
    {
        glActiveTexture(GL_TEXTURE0 + slot);
        glBindTexture(GL_TEXTURE_2D, m_RendererID);
    }
}