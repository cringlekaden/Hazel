#include "hzpch.h"
#include "RendererPolicy.h"
#include <algorithm>
#include <stdexcept>
#include <sstream>

namespace Hazel::RendererPolicy {
    const char* BackendName(RendererBackend backend) {
        switch (backend) {
        case RendererBackend::Automatic: return "Automatic";
        case RendererBackend::OpenGL: return "OpenGL";
        case RendererBackend::Metal: return "Metal 4";
        }
        throw std::invalid_argument("Unsupported renderer backend request");
    }
    RendererBackend ParseBackend(const std::string& value) {
        if (value == "Automatic") return RendererBackend::Automatic;
        if (value == "OpenGL") return RendererBackend::OpenGL;
        if (value == "Metal") return RendererBackend::Metal;
        throw std::invalid_argument("Backend must be Automatic, OpenGL or Metal");
    }
    RendererBackend SelectBackend(RendererBackend project, const std::optional<RendererBackend>& cli,
                                  const BackendAvailability& availability) {
        BackendName(project);
        const auto request = cli.value_or(project);
        BackendName(request);
        if (request == RendererBackend::Metal && !availability.Metal)
            throw std::runtime_error("Metal 4 requested but unavailable: " + availability.MetalReason +
                                     ". Select OpenGL in Project Settings or launch with --opengl.");
        if (request == RendererBackend::Automatic)
            return availability.Metal ? RendererBackend::Metal : RendererBackend::OpenGL;
        return request;
    }
    void Validate(const RuntimeRendererRequests &r) {
        BackendName(r.Backend);
        if (r.TextureSlots < 2 || r.TextureSlots > 32)
            throw std::invalid_argument("Texture batch slots must be 2–32, including white");
        if (r.ShaderLoading != ShaderLoadingRequest::Automatic &&
            r.ShaderLoading != ShaderLoadingRequest::GLSLCompatibility)
            throw std::invalid_argument("Unsupported shader loading request");
    }
    RendererSettings Settings(const RuntimeRendererRequests &r, DebugOutputRequest debug) {
        Validate(r);
        RendererSettings result;
        result.TextureSlots = r.TextureSlots;
        result.PreferShaderBinaries = r.ShaderLoading == ShaderLoadingRequest::Automatic;
        switch (debug) {
        case DebugOutputRequest::Automatic:
            break; // Debug build on, Release/Dist off.
        case DebugOutputRequest::Disabled:
            result.EnableDebugOutput = false;
            break;
        case DebugOutputRequest::Enabled:
            result.EnableDebugOutput = true;
            break;
        default:
            throw std::invalid_argument("Unsupported GL debug output request");
        }
        return result;
    }
    RendererResolution Resolve(const RendererSettings &r, const RendererCapabilities &caps) {
        if (r.TextureSlots < 2)
            throw std::invalid_argument(
                "Renderer texture slots must include white and a textured slot");
        if (caps.MaxTextureSlots < 2)
            throw std::runtime_error("Device cannot provide textured batching");
        RendererResolution result;
        result.Requested = r;
        result.Effective = r;
        result.Effective.TextureSlots = std::min({r.TextureSlots, 32u, caps.MaxTextureSlots});
        result.Effective.PreferShaderBinaries = r.PreferShaderBinaries && caps.ShaderBinaries;
        result.Effective.EnableDebugOutput = r.EnableDebugOutput && caps.DebugOutput;
        if (result.Effective.TextureSlots != r.TextureSlots)
            result.TextureReason = "Limited by device texture units and the engine's 32-slot batch";
        result.ShaderReason = !r.PreferShaderBinaries ? "GLSL compatibility requested"
                              : !caps.ShaderBinaries
                                  ? "SPIR-V specialization unavailable; Auto uses GLSL 410"
                                  : "Auto uses available SPIR-V specialization";
        if (caps.Metal)
            result.ShaderReason = "Offline Metal libraries (required); GLSL compatibility applies only to OpenGL";
        if (r.EnableDebugOutput && !caps.DebugOutput)
            result.DebugReason = caps.Metal ? "GL debug callback does not apply; use native Metal validation" : "Core 4.3 debug functions unavailable; request retained";
        return result;
    }
    std::string RestartReason(const RuntimeRendererRequests &r, const RendererResolution &current,
                              const RendererCapabilities &caps, RendererBackend launched) {
        if (r.Backend != launched)
            return "Restart required: save requests, close normally and reopen this project to apply the backend request. CLI overrides still take precedence";
        const auto desired = Resolve(Settings(r), caps);
        const bool slots = desired.Effective.TextureSlots != current.Effective.TextureSlots;
        const bool shaders =
            desired.Effective.PreferShaderBinaries != current.Effective.PreferShaderBinaries;
        if (!slots && !shaders)
            return {};
        return std::string("Restart Hazelnut with this saved project to apply ") +
               (slots && shaders ? "texture batching and shader loading"
                : slots          ? "texture batching"
                                 : "shader loading") +
               "; the current renderer and drafts are retained";
    }
    const char *ShaderPath(bool binaries) {
        return binaries ? "SPIR-V specialization" : "shaderc/Cross → GLSL 410";
    }
    const char *DebugName(DebugOutputRequest r) {
        switch (r) {
        case DebugOutputRequest::Automatic:
            return "Automatic";
        case DebugOutputRequest::Disabled:
            return "Off";
        case DebugOutputRequest::Enabled:
            return "On";
        }
        throw std::invalid_argument("Unsupported GL debug output request");
    }
    DebugOutputRequest ParseDebug(const std::string &value) {
        if (value == "Automatic")
            return DebugOutputRequest::Automatic;
        if (value == "Off")
            return DebugOutputRequest::Disabled;
        if (value == "On")
            return DebugOutputRequest::Enabled;
        throw std::invalid_argument("DebugOutput must be Automatic, Off or On");
    }
    std::string Describe(const RendererResolution &r) {
        std::ostringstream out;
        out << "Texture batch: requested " << r.Requested.TextureSlots << ", effective "
            << r.Effective.TextureSlots;
        if (!r.TextureReason.empty())
            out << " (" << r.TextureReason << ')';
        out << "\nShader loading: requested "
            << (r.Requested.PreferShaderBinaries ? "Automatic" : "GLSL compatibility")
            << ", resolution: " << r.ShaderReason << "\nGL debug output: requested "
            << (r.Requested.EnableDebugOutput ? "On" : "Off") << ", effective "
            << (r.Effective.EnableDebugOutput ? "On" : "Off");
        if (!r.DebugReason.empty())
            out << " (" << r.DebugReason << ')';
        return out.str();
    }
} // namespace Hazel::RendererPolicy
