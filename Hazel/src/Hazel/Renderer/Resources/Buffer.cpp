#include "hzpch.h"

#include "Hazel/Renderer/Resources/Buffer.h"
#include "Hazel/Renderer/Renderer.h"
#include "Hazel/Core/Log.h"
#include "Platform/OpenGL/OpenGLBuffer.h"

#ifdef HZ_PLATFORM_MACOS
#include "Platform/macOS/Metal/MetalBuffer.h"
#endif

namespace Hazel {

    Ref<VertexBuffer> VertexBuffer::Create(std::uint32_t size)
    {
        switch (Renderer::GetAPI())
        {
#ifdef HZ_PLATFORM_MACOS
            case RendererAPI::API::Metal: return CreateRef<MetalVertexBuffer>(size);
#else
            case RendererAPI::API::Metal: throw std::runtime_error("Metal requires macOS");
#endif

            case RendererAPI::API::None:
                HZ_CORE_ASSERT(false, "RendererAPI::None is invalid...");
                return nullptr;

            case RendererAPI::API::OpenGL:
                return CreateRef<OpenGLVertexBuffer>(size);
        }
        HZ_CORE_ASSERT(false, "Unknown RendererAPI...");
        return nullptr;
    }

    Ref<VertexBuffer> VertexBuffer::Create(float* vertices, std::uint32_t size)
    {
        switch (Renderer::GetAPI())
        {
#ifdef HZ_PLATFORM_MACOS
            case RendererAPI::API::Metal: return CreateRef<MetalVertexBuffer>(size, vertices);
#else
            case RendererAPI::API::Metal: throw std::runtime_error("Metal requires macOS");
#endif

            case RendererAPI::API::None:
                HZ_CORE_ASSERT(false, "RendererAPI::None is currently not supported!");
                return nullptr;

            case RendererAPI::API::OpenGL:
                return CreateRef<OpenGLVertexBuffer>(vertices, size);
        }
        HZ_CORE_ASSERT(false, "Unknown RendererAPI!");
        return nullptr;
    }

    Ref<IndexBuffer> IndexBuffer::Create(std::uint32_t* indices, std::uint32_t count)
    {
        switch (Renderer::GetAPI())
        {
#ifdef HZ_PLATFORM_MACOS
            case RendererAPI::API::Metal: return CreateRef<MetalIndexBuffer>(indices, count);
#else
            case RendererAPI::API::Metal: throw std::runtime_error("Metal requires macOS");
#endif

            case RendererAPI::API::None:
                HZ_CORE_ASSERT(false, "RendererAPI::None is currently not supported!");
                return nullptr;

            case RendererAPI::API::OpenGL:
                return CreateRef<OpenGLIndexBuffer>(indices, count);
        }
        HZ_CORE_ASSERT(false, "Unknown RendererAPI!");
        return nullptr;
    }
}