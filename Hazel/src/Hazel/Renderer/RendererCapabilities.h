#pragma once
#include <cstdint>
#include <string>

namespace Hazel {
    // Local extension of public Hazel's renderer/specification architecture.
    // API versions and entry points stay in the backend, outside scene formats.
    struct RendererCapabilities
    {
        std::string Vendor, Device, Driver;
        uint32_t MaxTextureSlots = 0, MaxTextureBindings = 0, MaxTextureSize = 0;
        uint32_t MaxColorAttachments = 0, MaxDrawBuffers = 0;
        uint32_t MaxSamples = 0, MaxColorSamples = 0, MaxIntegerSamples = 0, MaxDepthSamples = 0;
        float MinLineWidth = 1.0f, MaxLineWidth = 1.0f;
        bool ShaderBinaries = false, DebugOutput = false;
    };
    struct RendererSettings
    {
        // Includes white; clamped to the device and upstream's 32-slot batch.
        uint32_t TextureSlots = 32;
        bool PreferShaderBinaries = true;
#ifdef HZ_DEBUG
        bool EnableDebugOutput = true;
#else
        bool EnableDebugOutput = false;
#endif
    };
}
