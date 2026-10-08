// Adapted upstream texture specifications and loading with bind-based 4.1 operations.
#include "hzpch.h"
#include "Platform/OpenGL/OpenGLTexture.h"
#include "Platform/OpenGL/OpenGLCapabilities.h"
#include <limits>
#include <stdexcept>

namespace Hazel {
namespace Utils {
static GLenum Filter(TextureFilter value) {
    switch(value) {
        case TextureFilter::Nearest: return GL_NEAREST;
        case TextureFilter::Linear: return GL_LINEAR;
        case TextureFilter::NearestMipmapNearest: return GL_NEAREST_MIPMAP_NEAREST;
        case TextureFilter::LinearMipmapNearest: return GL_LINEAR_MIPMAP_NEAREST;
        case TextureFilter::NearestMipmapLinear: return GL_NEAREST_MIPMAP_LINEAR;
        case TextureFilter::LinearMipmapLinear: return GL_LINEAR_MIPMAP_LINEAR;
    }
    throw std::invalid_argument("Invalid texture filter");
}
static GLenum Wrap(TextureWrap value) {
    switch(value) {
        case TextureWrap::Repeat: return GL_REPEAT;
        case TextureWrap::ClampToEdge: return GL_CLAMP_TO_EDGE;
        case TextureWrap::MirroredRepeat: return GL_MIRRORED_REPEAT;
    }
    throw std::invalid_argument("Invalid texture wrap");
}
static GLenum HazelImageFormatToGLDataFormat(ImageFormat format)
{
    switch (format) {
        case ImageFormat::R8: return GL_RED;
        case ImageFormat::RGB8: return GL_RGB;
        case ImageFormat::RGBA8: case ImageFormat::RGBA32F: return GL_RGBA;
        default: throw std::invalid_argument("Unsupported texture image format");
    }
}
static GLenum HazelImageFormatToGLInternalFormat(ImageFormat format)
{
    switch (format) {
        case ImageFormat::R8: return GL_R8;
        case ImageFormat::RGB8: return GL_RGB8;
        case ImageFormat::RGBA8: return GL_RGBA8;
        case ImageFormat::RGBA32F: return GL_RGBA32F;
        default: throw std::invalid_argument("Unsupported texture image format");
    }
}
// Named-operation equivalent: preserve the caller's active-unit binding and packing state.
struct TextureUploadState {
    GLint Texture=0, Alignment=0, RowLength=0, SkipRows=0, SkipPixels=0, Buffer=0;
    explicit TextureUploadState(GLuint texture) {
        glGetIntegerv(GL_TEXTURE_BINDING_2D,&Texture);
        glGetIntegerv(GL_UNPACK_ALIGNMENT,&Alignment);
        glGetIntegerv(GL_UNPACK_ROW_LENGTH,&RowLength);
        glGetIntegerv(GL_UNPACK_SKIP_ROWS,&SkipRows);
        glGetIntegerv(GL_UNPACK_SKIP_PIXELS,&SkipPixels);
        glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING,&Buffer);
        glBindTexture(GL_TEXTURE_2D,texture);
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER,0);
        glPixelStorei(GL_UNPACK_ALIGNMENT,1);
        glPixelStorei(GL_UNPACK_ROW_LENGTH,0);
        glPixelStorei(GL_UNPACK_SKIP_ROWS,0);
        glPixelStorei(GL_UNPACK_SKIP_PIXELS,0);
    }
    ~TextureUploadState() {
        glBindTexture(GL_TEXTURE_2D,Texture);
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER,Buffer);
        glPixelStorei(GL_UNPACK_ALIGNMENT,Alignment);
        glPixelStorei(GL_UNPACK_ROW_LENGTH,RowLength);
        glPixelStorei(GL_UNPACK_SKIP_ROWS,SkipRows);
        glPixelStorei(GL_UNPACK_SKIP_PIXELS,SkipPixels);
    }
};
}

OpenGLTexture2D::OpenGLTexture2D(const TextureSpecification& specification)
    : m_Specification(specification), m_Width(specification.Width), m_Height(specification.Height)
{
    HZ_PROFILE_FUNCTION();
    m_Specification.Validate();
    m_InternalFormat=Utils::HazelImageFormatToGLInternalFormat(specification.Format);
    m_DataFormat=Utils::HazelImageFormatToGLDataFormat(specification.Format);
    m_DataType=specification.Format==ImageFormat::RGBA32F ? GL_FLOAT : GL_UNSIGNED_BYTE;
    m_BytesPerPixel=specification.Format==ImageFormat::R8 ? 1u :
                    specification.Format==ImageFormat::RGB8 ? 3u :
                    specification.Format==ImageFormat::RGBA8 ? 4u : 16u;
    AllocateStorage(nullptr);
}

OpenGLTexture2D::OpenGLTexture2D(const std::string& path, const TextureSpecification& specification)
    : m_Specification(specification), m_Path(std::filesystem::absolute(std::filesystem::u8path(path)).lexically_normal().generic_u8string())
{
    HZ_PROFILE_FUNCTION();
    specification.Validate(true);
    auto image=Texture2D::ReadImage(std::filesystem::u8path(path),specification.Format);
    if((specification.Width && specification.Width!=image.Width) || (specification.Height && specification.Height!=image.Height))
        throw std::runtime_error("Texture dimensions changed: "+path);
    m_Specification.Width=m_Width=image.Width; m_Specification.Height=m_Height=image.Height;
    m_Specification.Format=image.Format;
    m_InternalFormat=Utils::HazelImageFormatToGLInternalFormat(image.Format);
    m_DataFormat=Utils::HazelImageFormatToGLDataFormat(image.Format);
    m_BytesPerPixel=image.Format==ImageFormat::R8?1:image.Format==ImageFormat::RGB8?3:4;
    const size_t stride=static_cast<size_t>(m_Width)*m_BytesPerPixel;
    for(uint32_t y=0;y<m_Height/2;++y)
        std::swap_ranges(image.Pixels.begin()+y*stride,image.Pixels.begin()+(y+1)*stride,image.Pixels.begin()+(m_Height-1-y)*stride);
    AllocateStorage(image.Pixels.data());
}

void OpenGLTexture2D::AllocateStorage(const void* data)
{
    const auto maximum=OpenGLCapabilities::Get().MaxTextureSize;
    if (!m_Width || !m_Height || m_Width>static_cast<uint32_t>(maximum) || m_Height>static_cast<uint32_t>(maximum))
        throw std::invalid_argument("Texture dimensions exceed supported positive range");
    glGenTextures(1,&m_RendererID);
    if (!m_RendererID) throw std::runtime_error("Failed to create OpenGL texture");
    try {
        Utils::TextureUploadState state(m_RendererID);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,Utils::Filter(m_Specification.MinFilter));
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,Utils::Filter(m_Specification.MagFilter));
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,Utils::Wrap(m_Specification.WrapS));
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,Utils::Wrap(m_Specification.WrapT));
        glTexImage2D(GL_TEXTURE_2D,0,m_InternalFormat,static_cast<GLsizei>(m_Width),static_cast<GLsizei>(m_Height),
                     0,m_DataFormat,m_DataType,data);
        GLint width=0;
        glGetTexLevelParameteriv(GL_TEXTURE_2D,0,GL_TEXTURE_WIDTH,&width);
        if (width!=static_cast<GLint>(m_Width)) throw std::runtime_error("OpenGL texture allocation failed");
        if (m_Specification.GenerateMips) glGenerateMipmap(GL_TEXTURE_2D);
        m_IsLoaded=true;
    } catch (...) {
        glDeleteTextures(1,&m_RendererID); m_RendererID=0;
        throw;
    }
}

OpenGLTexture2D::~OpenGLTexture2D()
{
    HZ_PROFILE_FUNCTION();
    if (m_RendererID) glDeleteTextures(1,&m_RendererID);
}

void OpenGLTexture2D::SetData(const void* data,uint32_t size)
{
    HZ_PROFILE_FUNCTION();
    const uint64_t expected=static_cast<uint64_t>(m_Width)*m_Height*m_BytesPerPixel;
    if (!data || size!=expected) throw std::invalid_argument("Texture data must cover the entire texture");
    Utils::TextureUploadState state(m_RendererID);
    glTexSubImage2D(GL_TEXTURE_2D,0,0,0,static_cast<GLsizei>(m_Width),static_cast<GLsizei>(m_Height),m_DataFormat,m_DataType,data);
    if (m_Specification.GenerateMips) glGenerateMipmap(GL_TEXTURE_2D);
}

void OpenGLTexture2D::Bind(uint32_t slot) const
{
    HZ_PROFILE_FUNCTION();
    const auto maximum=OpenGLCapabilities::Get().MaxTextureBindings;
    if (slot>=static_cast<uint32_t>(maximum)) throw std::out_of_range("Texture slot exceeds hardware limit");
    glActiveTexture(GL_TEXTURE0+slot);
    glBindTexture(GL_TEXTURE_2D,m_RendererID);
}
}
