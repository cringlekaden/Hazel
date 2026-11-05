#include "hzpch.h"
#include "Hazel/Renderer/Texture.h"
#include "Hazel/Renderer/Renderer.h"
#include "Platform/OpenGL/OpenGLTexture.h"
#include "Hazel/Core/FileSystem.h"
#include <stb_image.h>
#include "ImageDecodeMemory.h"
#include <mutex>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace Hazel {
namespace {
const char* Filters[] = {"Nearest", "Linear", "NearestMipmapNearest", "LinearMipmapNearest", "NearestMipmapLinear", "LinearMipmapLinear"};
const char* Wraps[] = {"Repeat", "ClampToEdge", "MirroredRepeat"};
std::mutex DecodeMutex;
}
const char* TextureFilterName(TextureFilter value) {
    auto i = static_cast<unsigned>(value); if(i >= 6) throw std::invalid_argument("Invalid texture filter"); return Filters[i];
}
const char* TextureWrapName(TextureWrap value) {
    auto i = static_cast<unsigned>(value); if(i >= 3) throw std::invalid_argument("Invalid texture wrap"); return Wraps[i];
}
TextureFilter ParseTextureFilter(const std::string& text) {
    for(unsigned i=0;i<6;++i) if(text==Filters[i]) return static_cast<TextureFilter>(i);
    throw std::invalid_argument("Unknown texture filter: " + text);
}
TextureWrap ParseTextureWrap(const std::string& text) {
    for(unsigned i=0;i<3;++i) if(text==Wraps[i]) return static_cast<TextureWrap>(i);
    throw std::invalid_argument("Unknown texture wrap: " + text);
}
TextureSpecification TextureSpecification::ImGuiFile() {auto s=FileDefaults();s.Encoding=TextureEncoding::Linear;return s;}
TextureSpecification TextureSpecification::FileDefaults() {
    TextureSpecification result; result.Width=result.Height=0; result.Format=ImageFormat::None; result.Encoding=TextureEncoding::SRGB; return result;
}
void TextureSpecification::Validate(bool file) const {
    if(Encoding!=TextureEncoding::Linear && Encoding!=TextureEncoding::SRGB)throw std::invalid_argument("Unknown texture encoding");
    if(Encoding==TextureEncoding::SRGB && (Format==ImageFormat::R8 || Format==ImageFormat::RGBA32F))throw std::invalid_argument("sRGB requires RGB8/RGBA8 color pixels; data and float pixels are linear");
    TextureFilterName(MinFilter); TextureFilterName(MagFilter); TextureWrapName(WrapS); TextureWrapName(WrapT);
    if(MagFilter!=TextureFilter::Nearest && MagFilter!=TextureFilter::Linear) throw std::invalid_argument("Magnification cannot use mipmap filters");
    if(!GenerateMips && static_cast<unsigned>(MinFilter)>=2) throw std::invalid_argument("Mipmap minification requires GenerateMips");
    if(!file && (!Width || !Height || Format==ImageFormat::None)) throw std::invalid_argument("Generated textures require positive dimensions and a format");
    if(Format!=ImageFormat::None && Format!=ImageFormat::R8 && Format!=ImageFormat::RGB8 && Format!=ImageFormat::RGBA8 && Format!=ImageFormat::RGBA32F)
        throw std::invalid_argument("Unsupported texture format");
    if(file && Format==ImageFormat::RGBA32F) throw std::invalid_argument("RGBA32F requires explicit generated float data, not an 8-bit file");
}
TextureImage Texture2D::ReadImage(const std::filesystem::path& path, ImageFormat format) {
    TextureSpecification spec=TextureSpecification::FileDefaults(); spec.Format=format; spec.Encoding=TextureEncoding::Linear; spec.Validate(true);
    ScopedBuffer encoded(FileSystem::ReadFileBinary(path));
    if(!encoded || encoded.Size()>static_cast<uint64_t>(std::numeric_limits<int>::max())) throw std::runtime_error("Cannot read texture: "+path.generic_u8string());
    return ReadImageBytes(encoded.Data(),size_t(encoded.Size()),format,67108864);
}
TextureImage Texture2D::ReadImageBytes(const uint8_t* bytes,size_t size,ImageFormat format,uint64_t pixelLimit,bool embeddedPNGJPEG) {
    TextureSpecification spec=TextureSpecification::FileDefaults();spec.Format=format;spec.Encoding=TextureEncoding::Linear;spec.Validate(true);
    if(!bytes || !size || size>uint64_t(std::numeric_limits<int>::max()) || (embeddedPNGJPEG && size>67108864))throw std::runtime_error("Image input exceeds 64 MiB byte limit");
    if(embeddedPNGJPEG && !((size>=8 && std::memcmp(bytes,"\x89PNG\r\n\x1a\n",8)==0) || (size>=3 && bytes[0]==255 && bytes[1]==216 && bytes[2]==255)))throw std::runtime_error("Embedded image requires PNG or JPEG bytes");
    const int requested=format==ImageFormat::R8?1:format==ImageFormat::RGB8?3:format==ImageFormat::RGBA8?4:0;
    std::lock_guard<std::mutex> lock(DecodeMutex);
    ImageDecodeBudget budget(embeddedPNGJPEG?96*1024*1024:0);
    stbi_set_flip_vertically_on_load(0); // All shared file decoding has top-left rows.
    if(stbi_is_hdr_from_memory(bytes,static_cast<int>(size)))
        throw std::runtime_error("HDR radiance is unsupported by the ordinary 8-bit texture path; use display-referred color/data images at this checkpoint");
    int width=0,height=0,channels=0;
    if(!stbi_info_from_memory(bytes,static_cast<int>(size),&width,&height,&channels) || width<=0 || height<=0)
        throw std::runtime_error("Cannot inspect image bytes");
    // Bound CPU allocations without needing a graphics context; backend enforces device limits too.
    if(!pixelLimit || static_cast<uint64_t>(width)*height>pixelLimit) throw std::runtime_error("Image exceeds remaining pixel decode budget");
    if(embeddedPNGJPEG && stbi_is_16_bit_from_memory(bytes,static_cast<int>(size)))throw std::runtime_error("Embedded 16-bit PNG is unsupported; export 8-bit PNG/JPEG");
    int output=requested ? requested : channels==2 ? 4 : channels;
    auto* pixels=stbi_load_from_memory(bytes,static_cast<int>(size),&width,&height,&channels,output);
    if(!pixels || budget.Failed()){if(pixels)stbi_image_free(pixels);throw std::runtime_error(budget.Failed()?"Image decoder exceeded its 96 MiB import allocation budget":std::string("Cannot decode image bytes: ")+stbi_failure_reason());}
    TextureImage image;
    try {
        image.Width=static_cast<uint32_t>(width); image.Height=static_cast<uint32_t>(height);
        image.Format=output==1?ImageFormat::R8:output==3?ImageFormat::RGB8:ImageFormat::RGBA8;
        image.Pixels.assign(pixels,pixels+static_cast<size_t>(width)*height*output);
    } catch(...) { stbi_image_free(pixels); throw; }
    stbi_image_free(pixels); return image;
}
Ref<Texture2D> Texture2D::Create(const TextureSpecification& specification) {
    specification.Validate();
    if(Renderer::GetAPI()!=RendererAPI::API::OpenGL) throw std::runtime_error("Texture backend unavailable");
    return CreateRef<OpenGLTexture2D>(specification);
}
Ref<Texture2D> Texture2D::Create(uint32_t width,uint32_t height) {
    TextureSpecification specification; specification.Width=width; specification.Height=height; return Create(specification);
}
Ref<Texture2D> Texture2D::Create(const std::string& path) { return Create(path,TextureSpecification::FileDefaults()); }
Ref<Texture2D> Texture2D::Create(const std::string& path,const TextureSpecification& specification) {
    specification.Validate(true);
    if(Renderer::GetAPI()!=RendererAPI::API::OpenGL) throw std::runtime_error("Texture backend unavailable");
    return CreateRef<OpenGLTexture2D>(path,specification);
}
}
