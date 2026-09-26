#pragma once

#include "Hazel/Renderer/Texture.h"

namespace Hazel {

    class OpenGLTexture2D : public Texture2D
    {
    public:
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

        void Bind(std::uint32_t slot = 0) const override;
    private:
        std::string m_Path;
        std::uint32_t m_Width = 0;
        std::uint32_t m_Height = 0;
        std::uint32_t m_RendererID = 0;
    };
}