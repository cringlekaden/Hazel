#pragma once
#include "RendererCapabilities.h"
#include <optional>

namespace Hazel {
    enum class RendererBackend { Automatic, OpenGL, Metal };
    // Resolved before window creation: native Metal 4 where available, otherwise OpenGL.
    inline constexpr RendererBackend DefaultRendererBackend = RendererBackend::Automatic;
    struct BackendAvailability {
        bool Metal = false;
        std::string MetalReason;
        // OpenGL needs a current context to query actual device support.
        std::string OpenGLReason = "Built for OpenGL 4.1; device support checked at startup";
    };
    enum class ShaderLoadingRequest { Automatic, GLSLCompatibility };
    enum class DebugOutputRequest { Automatic, Disabled, Enabled };
    // Portable game requests only. Device facts and editor diagnostics never travel here.
    struct RuntimeRendererRequests {
        RendererBackend Backend = DefaultRendererBackend;
        bool VSync = true;
        uint32_t TextureSlots = 32; // Includes the white slot.
        ShaderLoadingRequest ShaderLoading = ShaderLoadingRequest::Automatic;
    };
    struct RendererResolution {
        RendererSettings Requested, Effective;
        std::string TextureReason, ShaderReason, DebugReason;
    };
    namespace RendererPolicy {
        const char* BackendName(RendererBackend backend);
        RendererBackend ParseBackend(const std::string& value);
        RendererBackend SelectBackend(RendererBackend project, const std::optional<RendererBackend>& cli,
                                      const BackendAvailability& availability);
        void Validate(const RuntimeRendererRequests &requests);
        RendererSettings Settings(const RuntimeRendererRequests &requests,
                                  DebugOutputRequest debug = DebugOutputRequest::Automatic);
        // CPU only; uses the engine/backend's existing capability record.
        RendererResolution Resolve(const RendererSettings &requests,
                                   const RendererCapabilities &caps);
        std::string RestartReason(const RuntimeRendererRequests &requests,
                                  const RendererResolution &current,
                                  const RendererCapabilities &caps,
                                  RendererBackend launched);
        const char *ShaderPath(bool binaries);
        const char *DebugName(DebugOutputRequest request);
        DebugOutputRequest ParseDebug(const std::string &value);
        std::string Describe(const RendererResolution &resolution);
    } // namespace RendererPolicy
} // namespace Hazel
