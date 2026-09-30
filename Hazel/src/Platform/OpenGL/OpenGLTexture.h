#pragma once

#include "Hazel/Renderer/Texture.h"

#include <glad/glad.h>

namespace Hazel {

    class OpenGLTexture2D : public Texture2D
    {
    public:
        OpenGLTexture2D(std::uint32_t width, std::uint32_t height);
        explicit OpenGLTexture2D(const std::string& path);
        ~OpenGLTexture2D() override;

        std::uint32_t GetWidth() const override
        {
            return m_Width;
        }

        std::uint32_t GetHeight() const override
        {
            return m_Height;
        }

        void SetData(const void* data, std::uint32_t size) override;
        void Bind(std::uint32_t slot = 0) const override;
        bool operator==(const Texture& other) const override;
    private:
        std::string m_Path;
        std::uint32_t m_Width = 0;
        std::uint32_t m_Height = 0;
        std::uint32_t m_RendererID = 0;
        GLenum m_InternalFormat = 0;
        GLenum m_DataFormat = 0;
    };
}