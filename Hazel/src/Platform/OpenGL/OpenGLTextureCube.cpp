#include "OpenGLTextureCube.h"
#include "OpenGLCapabilities.h"
#include "hzpch.h"
#include <glad/glad.h>
namespace Hazel
{
OpenGLTextureCube::OpenGLTextureCube(const LightingImage &d)
{
    d.Validate();
    if (d.Faces() != 6 || d.Side > OpenGLCapabilities::Get().MaxCubeSize)
        throw std::runtime_error("Cubemap exceeds detected cube size or has invalid faces");
    GLint previous, buffer, alignment, row, rows, pixels, swap;
    glGetIntegerv(GL_TEXTURE_BINDING_CUBE_MAP, &previous);
    glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING, &buffer);
    glGetIntegerv(GL_UNPACK_ALIGNMENT, &alignment);
    glGetIntegerv(GL_UNPACK_ROW_LENGTH, &row);
    glGetIntegerv(GL_UNPACK_SKIP_ROWS, &rows);
    glGetIntegerv(GL_UNPACK_SKIP_PIXELS, &pixels);
    glGetIntegerv(GL_UNPACK_SWAP_BYTES, &swap);
    struct Restore
    {
        GLint Texture, Buffer, Alignment, Row, Rows, Pixels, Swap;
        ~Restore()
        {
            glBindTexture(GL_TEXTURE_CUBE_MAP, Texture);
            glBindBuffer(GL_PIXEL_UNPACK_BUFFER, Buffer);
            glPixelStorei(GL_UNPACK_ALIGNMENT, Alignment);
            glPixelStorei(GL_UNPACK_ROW_LENGTH, Row);
            glPixelStorei(GL_UNPACK_SKIP_ROWS, Rows);
            glPixelStorei(GL_UNPACK_SKIP_PIXELS, Pixels);
            glPixelStorei(GL_UNPACK_SWAP_BYTES, Swap);
        }
    } restore{previous, buffer, alignment, row, rows, pixels, swap};
    glGenTextures(1, &m_ID);
    if (!m_ID)
        throw std::runtime_error("Cannot allocate cubemap identity");
    try
    {
        glBindTexture(GL_TEXTURE_CUBE_MAP, m_ID);
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
        glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
        glPixelStorei(GL_UNPACK_SWAP_BYTES, GL_FALSE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_BASE_LEVEL, 0);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAX_LEVEL, GLint(d.Mips.size() - 1));
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER,
                        d.Mips.size() > 1 ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        for (auto wrap : {GL_TEXTURE_WRAP_S, GL_TEXTURE_WRAP_T, GL_TEXTURE_WRAP_R})
            glTexParameteri(GL_TEXTURE_CUBE_MAP, wrap, GL_CLAMP_TO_EDGE);
        unsigned side = d.Side;
        for (unsigned mip = 0; mip < d.Mips.size(); ++mip)
        {
            for (unsigned face = 0; face < 6; ++face)
            {
                auto target = GL_TEXTURE_CUBE_MAP_POSITIVE_X + face;
                glTexImage2D(target, mip, GL_RGB32F, side, side, 0, GL_RGB, GL_FLOAT,
                             d.Mips[mip].data() + size_t(face) * side * side * 3);
                GLint width = 0, format = 0;
                glGetTexLevelParameteriv(target, mip, GL_TEXTURE_WIDTH, &width);
                glGetTexLevelParameteriv(target, mip, GL_TEXTURE_INTERNAL_FORMAT, &format);
                if (width != int(side) || format != GL_RGB32F)
                    throw std::runtime_error("OpenGL float cubemap allocation/format failed");
            }
            side = std::max(1u, side / 2);
        }
    }
    catch (...)
    {
        glDeleteTextures(1, &m_ID);
        m_ID = 0;
        throw;
    }
}
OpenGLTextureCube::~OpenGLTextureCube()
{
    if (m_ID)
        glDeleteTextures(1, &m_ID);
}
void OpenGLTextureCube::Bind(uint32_t slot) const
{
    if (slot >= OpenGLCapabilities::Get().MaxTextureBindings)
        throw std::runtime_error("Cubemap sampler slot exceeds device limit");
    glActiveTexture(GL_TEXTURE0 + slot);
    glBindTexture(GL_TEXTURE_CUBE_MAP, m_ID);
    glBindSampler(slot, 0);
}
} // namespace Hazel
