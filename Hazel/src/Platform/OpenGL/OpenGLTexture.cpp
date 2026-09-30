#include "hzpch.h"

#include "Platform/OpenGL/OpenGLTexture.h"
#include "Hazel/Core/Log.h"

#include <glad/glad.h>
#include <stb_image.h>

#include <stdexcept>

namespace Hazel {

    OpenGLTexture2D::OpenGLTexture2D(std::uint32_t width, std::uint32_t height)
        : m_Width(width),
          m_Height(height),
          m_InternalFormat(GL_RGBA8),
          m_DataFormat(GL_RGBA)
    {
        HZ_PROFILE_FUNCTION();
        if (width == 0 || height == 0)
            throw std::runtime_error("Texture dimensions must be positive...");
        glGenTextures(1, &m_RendererID);
        if (m_RendererID == 0)
            throw std::runtime_error("Failed to create OpenGL texture");
        glBindTexture(GL_TEXTURE_2D, m_RendererID);
        glTexParameteri(
            GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(
            GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(
            GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(
            GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexImage2D(
            GL_TEXTURE_2D,
            0,
            m_InternalFormat,
            static_cast<GLsizei>(m_Width),
            static_cast<GLsizei>(m_Height),
            0,
            m_DataFormat,
            GL_UNSIGNED_BYTE,
            nullptr);
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    OpenGLTexture2D::OpenGLTexture2D(const std::string& path) : m_Path(path)
    {
        HZ_PROFILE_FUNCTION();
        stbi_set_flip_vertically_on_load(1);
        int width = 0;
        int height = 0;
        int channels = 0;
        stbi_uc* pixels = nullptr;
        {
            HZ_PROFILE_SCOPE("stbi_load");
            pixels = stbi_load(
                path.c_str(),
                &width,
                &height,
                &channels,
                0);
        }
        if (!pixels)
        {
            HZ_CORE_ERROR("Failed to load texture '{}': {}", path, stbi_failure_reason());
            throw std::runtime_error("Failed to load texture: " + path);
        }
        const GLenum format =
            channels == 4 ? GL_RGBA :
            channels == 3 ? GL_RGB : 0;
        const GLint internalFormat =
            channels == 4 ? GL_RGBA8 :
            channels == 3 ? GL_RGB8 : 0;
        if (format == 0 || width <= 0 || height <= 0)
        {
            stbi_image_free(pixels);
            HZ_CORE_ERROR("Unsupported texture format in '{}' ({} channels)", path, channels);
            throw std::runtime_error("Unsupported texture format: " + path);
        }
        m_Width = static_cast<std::uint32_t>(width);
        m_Height = static_cast<std::uint32_t>(height);
        m_InternalFormat = static_cast<GLenum>(internalFormat);
        m_DataFormat = format;
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
        HZ_PROFILE_FUNCTION();
        if (m_RendererID != 0)
            glDeleteTextures(1, &m_RendererID);
    }

    void OpenGLTexture2D::SetData(const void* data, std::uint32_t size)
    {
        HZ_PROFILE_FUNCTION();
        const std::uint32_t bytesPerPixel = m_DataFormat == GL_RGBA ? 4u : 3u;
        const std::uint64_t expectedSize =
            static_cast<std::uint64_t>(m_Width) *
            m_Height *
            bytesPerPixel;
        if (!data || size != expectedSize)
            throw std::runtime_error("Texture data must cover the entire texture");
        GLint previousAlignment = 4;
        glGetIntegerv(GL_UNPACK_ALIGNMENT, &previousAlignment);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glBindTexture(GL_TEXTURE_2D, m_RendererID);
        glTexSubImage2D(
            GL_TEXTURE_2D,
            0,
            0,
            0,
            static_cast<GLsizei>(m_Width),
            static_cast<GLsizei>(m_Height),
            m_DataFormat,
            GL_UNSIGNED_BYTE,
            data);
        glBindTexture(GL_TEXTURE_2D, 0);
        glPixelStorei(GL_UNPACK_ALIGNMENT, previousAlignment);
    }

    void OpenGLTexture2D::Bind(std::uint32_t slot) const
    {
        HZ_PROFILE_FUNCTION();
        glActiveTexture(GL_TEXTURE0 + slot);
        glBindTexture(GL_TEXTURE_2D, m_RendererID);
    }

    bool OpenGLTexture2D::operator==(const Texture& other) const
    {
        const auto* otherTexture = dynamic_cast<const OpenGLTexture2D*>(&other);
        return otherTexture && m_RendererID == otherTexture->m_RendererID;
    }
}