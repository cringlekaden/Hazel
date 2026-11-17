#include "hzpch.h"

#include "Hazel/Renderer/Resources/VertexArray.h"
#include "Hazel/Renderer/Renderer.h"
#include "Platform/OpenGL/OpenGLVertexArray.h"

#ifdef HZ_PLATFORM_MACOS
#include "Platform/macOS/Metal/MetalBuffer.h"
#endif

namespace Hazel {

    Ref<VertexArray> VertexArray::Create()
    {
        switch (Renderer::GetAPI())
        {
#ifdef HZ_PLATFORM_MACOS
            case RendererAPI::API::Metal: return CreateRef<MetalVertexArray>();
#else
            case RendererAPI::API::Metal: throw std::runtime_error("Metal requires macOS");
#endif

            case RendererAPI::API::None:
                HZ_CORE_ASSERT(false, "RendererAPI::None is invalid...");
                return nullptr;

            case RendererAPI::API::OpenGL:
                return CreateRef<OpenGLVertexArray>();
        }
        HZ_CORE_ASSERT(false, "Unknown RendererAPI...");
        return nullptr;
    }
}