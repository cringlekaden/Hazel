#pragma once
#include <cstdint>
#include <string>

namespace Hazel {
    // Local extension of public Hazel's renderer/specification architecture.
    // API versions and entry points stay in the backend, outside scene formats.
    struct RendererCapabilities
    {
        std::string Vendor, Device, Driver;
        uint32_t MaxTextureSlots = 0, MaxTextureBindings = 0, MaxTextureSize = 0, MaxCubeSize = 0;
        uint32_t MaxColorAttachments = 0, MaxDrawBuffers = 0;
        uint32_t MaxUniformBindings = 0, MaxUniformBlockBytes = 0, MaxVertexAttributes = 0;
        uint32_t MaxSamples = 0, MaxColorSamples = 0, MaxIntegerSamples = 0, MaxDepthSamples = 0;
        float MinLineWidth = 1.0f, MaxLineWidth = 1.0f;
        bool Metal = false, Compute = false, TemporalAA = false;
        std::string LimitsSource = "Queried current OpenGL context";
        std::string ShaderLoading = "GLSL 410; SPIR-V when supported";
        std::string TargetFormats = "RGBA32F radiance / RGBA16F display-linear (FP32 also supported) / R32Sint ID / Depth24Stencil8; one sample in scene targets";
        std::string Features = "Renderer2D, Renderer3D PBR/IBL, Neutral tone mapping, picking, editor UI";
        bool ShaderBinaries = false, DebugOutput = false, ForwardCompatible = false;
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
