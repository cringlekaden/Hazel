#include "hzpch.h"

#include "Hazel/Renderer/RendererAPI.h"
#include "Platform/OpenGL/OpenGLRendererAPI.h"
#include <stdexcept>
#ifdef HZ_PLATFORM_MACOS
#include "Platform/macOS/Metal/MetalRendererAPI.h"
#include "Platform/macOS/Metal/MetalDevice.h"
#endif

namespace Hazel {

    RendererAPI::API RendererAPI::s_API = RendererAPI::API::OpenGL;

    BackendAvailability RendererAPI::GetBackendAvailability() {
        BackendAvailability result;
#ifdef HZ_PLATFORM_MACOS
        result.MetalReason = MetalDevice::AvailabilityReason();
        result.Metal = result.MetalReason.empty();
#else
        result.MetalReason = "Requires native Apple Silicon and macOS 27 or newer";
#endif
        return result;
    }

    // Actual upstream factory, retaining Scope and release-mode diagnostics.
    Scope<RendererAPI> RendererAPI::Create()
    {
        switch (s_API) {
            case API::OpenGL: return CreateScope<OpenGLRendererAPI>();
#ifdef HZ_PLATFORM_MACOS
            case API::Metal: return CreateScope<MetalRendererAPI>();
#endif
            default: throw std::runtime_error("Unsupported renderer API");
        }
    }

}
