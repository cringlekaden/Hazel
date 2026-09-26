#pragma once

#include <cstdint>

namespace Hazel {

    class VertexBuffer
    {
    public:
        virtual ~VertexBuffer() = default;

        virtual void Bind() const = 0;
        virtual void Unbind() const = 0;

        static VertexBuffer* Create(float* vertices, std::uint32_t size);
    };

    class IndexBuffer
    {
    public:
        virtual ~IndexBuffer() = default;

        virtual void Bind() const = 0;
        virtual void Unbind() const = 0;

        virtual std::uint32_t GetCount() const = 0;

        static IndexBuffer* Create(std::uint32_t* indices, std::uint32_t count);
    };

}