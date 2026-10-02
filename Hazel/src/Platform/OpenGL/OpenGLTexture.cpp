// Adapted upstream texture specifications and loading with bind-based 4.1 operations.
#include "hzpch.h"
#include "Platform/OpenGL/OpenGLTexture.h"
#include "Platform/OpenGL/OpenGLCapabilities.h"
#include "Hazel/Core/FileSystem.h"
#include <stb_image.h>
#include <limits>
#include <stdexcept>

namespace Hazel {
namespace Utils {
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
    m_InternalFormat=Utils::HazelImageFormatToGLInternalFormat(specification.Format);
    m_DataFormat=Utils::HazelImageFormatToGLDataFormat(specification.Format);
    m_DataType=specification.Format==ImageFormat::RGBA32F ? GL_FLOAT : GL_UNSIGNED_BYTE;
    m_BytesPerPixel=specification.Format==ImageFormat::R8 ? 1u :
                    specification.Format==ImageFormat::RGB8 ? 3u :
                    specification.Format==ImageFormat::RGBA8 ? 4u : 16u;
    AllocateStorage(nullptr);
}

OpenGLTexture2D::OpenGLTexture2D(const std::string& path)
    : m_Path(std::filesystem::absolute(std::filesystem::u8path(path)).lexically_normal().generic_u8string())
{
    HZ_PROFILE_FUNCTION();
    auto encoded=FileSystem::ReadFileBinary(std::filesystem::u8path(path));
    if (!encoded || encoded.Size>static_cast<uint64_t>(std::numeric_limits<int>::max()))
        throw std::runtime_error("Failed to read texture: "+path);
    int width=0,height=0,channels=0;
    stbi_set_flip_vertically_on_load(1);
    stbi_uc* pixels=nullptr;
    {
        HZ_PROFILE_SCOPE("stbi_load_from_memory");
        pixels=stbi_load_from_memory(encoded.Data,static_cast<int>(encoded.Size),&width,&height,&channels,0);
    }
    if (!pixels) {
        HZ_CORE_ERROR("Failed to load texture '{}': {}",path,stbi_failure_reason());
        throw std::runtime_error("Failed to load texture: "+path);
    }
    try {
        if ((channels!=1 && channels!=3 && channels!=4) || width<=0 || height<=0)
            throw std::runtime_error("Unsupported texture format: "+path);
        m_Specification.Width=m_Width=static_cast<uint32_t>(width);
        m_Specification.Height=m_Height=static_cast<uint32_t>(height);
        m_Specification.Format=channels==1 ? ImageFormat::R8 : channels==3 ? ImageFormat::RGB8 : ImageFormat::RGBA8;
        m_InternalFormat=Utils::HazelImageFormatToGLInternalFormat(m_Specification.Format);
        m_DataFormat=Utils::HazelImageFormatToGLDataFormat(m_Specification.Format);
        m_BytesPerPixel=static_cast<uint32_t>(channels);
        AllocateStorage(pixels);
    } catch (...) {
        stbi_image_free(pixels);
        throw;
    }
    stbi_image_free(pixels);
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
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,m_Specification.GenerateMips ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);
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
