#include "hzpch.h"
#include "Platform/OpenGL/OpenGLCapabilities.h"
#include <glad/glad.h>
#include <algorithm>
#include <stdexcept>

namespace Hazel::OpenGLCapabilities {
namespace {
    RendererCapabilities s_Capabilities;
    RendererResolution s_Resolution;
    int s_Major = 0, s_Minor = 0;
    bool s_Initialized = false;
    uint32_t Limit(GLenum name) {
        GLint value = 0; glGetIntegerv(name, &value);
        return static_cast<uint32_t>(std::max(0, value));
    }
    std::string Description(GLenum name) {
        const auto* value = glGetString(name);
        return value ? reinterpret_cast<const char*>(value) : "";
    }
}
void Reset() {
    s_Capabilities = {}; s_Resolution = {};
    s_Major = s_Minor = 0; s_Initialized = false;
}
void Initialize() {
    Reset();
    if (!glGetString || !glGetString(GL_VERSION))
        throw std::runtime_error("OpenGL capabilities require a loaded, current context");
    glGetIntegerv(GL_MAJOR_VERSION, &s_Major); glGetIntegerv(GL_MINOR_VERSION, &s_Minor);
    if (s_Major < 4 || (s_Major == 4 && s_Minor < 1))
        throw std::runtime_error("Hazel OpenGL backend requires OpenGL 4.1 or newer");
    auto& caps = s_Capabilities;
    caps.Vendor = Description(GL_VENDOR); caps.Device = Description(GL_RENDERER); caps.Driver = Description(GL_VERSION);
    caps.MaxTextureBindings = Limit(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS);
    caps.MaxTextureSlots = std::min(Limit(GL_MAX_TEXTURE_IMAGE_UNITS), caps.MaxTextureBindings);
    if (caps.MaxTextureSlots < 2) throw std::runtime_error("Insufficient OpenGL texture units for textured batching");
    caps.MaxTextureSize = Limit(GL_MAX_TEXTURE_SIZE);
    caps.MaxColorAttachments = Limit(GL_MAX_COLOR_ATTACHMENTS); caps.MaxDrawBuffers = Limit(GL_MAX_DRAW_BUFFERS);
    caps.MaxSamples = Limit(GL_MAX_SAMPLES);
    caps.MaxColorSamples = std::min(caps.MaxSamples, Limit(GL_MAX_COLOR_TEXTURE_SAMPLES));
    caps.MaxIntegerSamples = std::min(caps.MaxSamples, Limit(GL_MAX_INTEGER_SAMPLES));
    caps.MaxDepthSamples = std::min(caps.MaxSamples, Limit(GL_MAX_DEPTH_TEXTURE_SAMPLES));
    GLfloat widths[2]{1, 1}; glGetFloatv(GL_SMOOTH_LINE_WIDTH_RANGE, widths);
    caps.MinLineWidth = widths[0]; caps.MaxLineWidth = widths[1];
    // The retained loader exposes core functions. Extension-only contexts use
    // the complete shaderc/Cross pipeline and GLSL 410 program loading.
    caps.ShaderBinaries = GLAD_GL_VERSION_4_6 && glShaderBinary && glSpecializeShader;
    caps.DebugOutput = GLAD_GL_VERSION_4_3 && glDebugMessageCallback && glDebugMessageControl;
    s_Resolution = RendererPolicy::Resolve(RendererSettings{}, caps);
    s_Initialized = true;
}
const RendererCapabilities& Get() { if (!s_Initialized) Initialize(); return s_Capabilities; }
const RendererResolution& GetResolution() { Get(); return s_Resolution; }
const RendererSettings& GetSettings() { return GetResolution().Effective; }
void Configure(const RendererSettings& requested) {
    const auto& caps = Get();
    auto candidate = RendererPolicy::Resolve(requested, caps);
    s_Resolution = std::move(candidate);
}
bool UseShaderBinaries() { return Get().ShaderBinaries && GetSettings().PreferShaderBinaries; }
}
