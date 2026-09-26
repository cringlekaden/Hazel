#include "hzpch.h"

#include "Hazel/Renderer/Buffer.h"
#include "Hazel/Renderer/Renderer.h"
#include "Hazel/Log.h"
#include "Platform/OpenGL/OpenGLBuffer.h"

namespace Hazel {

    VertexBuffer* VertexBuffer::Create(float* vertices, std::uint32_t size)
    {
        switch (Renderer::GetAPI())
        {
            case RendererAPI::None:
                HZ_CORE_ASSERT(false, "RendererAPI::None is currently not supported!");
                return nullptr;

            case RendererAPI::OpenGL:
                return new OpenGLVertexBuffer(vertices, size);
        }
        HZ_CORE_ASSERT(false, "Unknown RendererAPI!");
        return nullptr;
    }

    IndexBuffer* IndexBuffer::Create(std::uint32_t* indices, std::uint32_t count)
    {
        switch (Renderer::GetAPI())
        {
            case RendererAPI::None:
                HZ_CORE_ASSERT(false, "RendererAPI::None is currently not supported!");
                return nullptr;

            case RendererAPI::OpenGL:
                return new OpenGLIndexBuffer(indices, count);
        }
        HZ_CORE_ASSERT(false, "Unknown RendererAPI!");
        return nullptr;
    }
}