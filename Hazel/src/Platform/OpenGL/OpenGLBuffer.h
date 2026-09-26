#pragma once

#include "Hazel/Renderer/Buffer.h"

namespace Hazel {

    class OpenGLVertexBuffer : public VertexBuffer
    {
    public:
        OpenGLVertexBuffer(float* vertices, std::uint32_t size);
        ~OpenGLVertexBuffer() override;

        void Bind() const override;
        void Unbind() const override;

    private:
        std::uint32_t m_RendererID = 0;
    };

    class OpenGLIndexBuffer : public IndexBuffer
    {
    public:
        OpenGLIndexBuffer(std::uint32_t* indices, std::uint32_t count);
        ~OpenGLIndexBuffer() override;

        void Bind() const override;
        void Unbind() const override;

        std::uint32_t GetCount() const override { return m_Count; }

    private:
        std::uint32_t m_RendererID = 0;
        std::uint32_t m_Count = 0;
    };
}