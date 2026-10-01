#pragma once
// Graphics limits stay in this backend; common renderer APIs expose their meaning.
#include <glad/glad.h>
#include <algorithm>
#include <cstdint>
#include <stdexcept>

namespace Hazel::OpenGLCapabilities {
inline std::uint32_t FragmentTextureSlots()
{
    GLint fragment=0, combined=0;
    glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS,&fragment);
    glGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS,&combined);
    const GLint supported=std::min(fragment,combined);
    if (supported<2) throw std::runtime_error("Insufficient OpenGL texture units for textured batching");
    return static_cast<std::uint32_t>(supported);
}
}
