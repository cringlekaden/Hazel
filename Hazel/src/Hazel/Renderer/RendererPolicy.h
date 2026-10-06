#pragma once
#include "RendererCapabilities.h"

namespace Hazel {
    enum class ShaderLoadingRequest { Automatic, GLSLCompatibility };
    enum class DebugOutputRequest { Automatic, Disabled, Enabled };
    // Portable game requests only. Device facts and editor diagnostics never travel here.
    struct RuntimeRendererRequests {
        bool VSync = true;
        uint32_t TextureSlots = 32; // Includes the white slot.
        ShaderLoadingRequest ShaderLoading = ShaderLoadingRequest::Automatic;
    };
    struct RendererResolution {
        RendererSettings Requested, Effective;
        std::string TextureReason, ShaderReason, DebugReason;
    };
    namespace RendererPolicy {
        void Validate(const RuntimeRendererRequests &requests);
        RendererSettings Settings(const RuntimeRendererRequests &requests,
                                  DebugOutputRequest debug = DebugOutputRequest::Automatic);
        // CPU only; uses the engine/backend's existing capability record.
        RendererResolution Resolve(const RendererSettings &requests,
                                   const RendererCapabilities &caps);
        std::string RestartReason(const RuntimeRendererRequests &requests,
                                  const RendererResolution &current,
                                  const RendererCapabilities &caps);
        const char *ShaderPath(bool binaries);
        const char *DebugName(DebugOutputRequest request);
        DebugOutputRequest ParseDebug(const std::string &value);
        std::string Describe(const RendererResolution &resolution);
    } // namespace RendererPolicy
} // namespace Hazel
