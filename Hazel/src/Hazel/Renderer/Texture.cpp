#include "hzpch.h"
#include "Hazel/Renderer/Texture.h"
#include "Hazel/Renderer/Renderer.h"
#include "Platform/OpenGL/OpenGLTexture.h"
#include "Hazel/Core/FileSystem.h"
#include <stb_image.h>
#include <mutex>
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
TextureSpecification TextureSpecification::FileDefaults() {
    TextureSpecification result; result.Width=result.Height=0; result.Format=ImageFormat::None; return result;
}
void TextureSpecification::Validate(bool file) const {
    TextureFilterName(MinFilter); TextureFilterName(MagFilter); TextureWrapName(WrapS); TextureWrapName(WrapT);
    if(MagFilter!=TextureFilter::Nearest && MagFilter!=TextureFilter::Linear) throw std::invalid_argument("Magnification cannot use mipmap filters");
    if(!GenerateMips && static_cast<unsigned>(MinFilter)>=2) throw std::invalid_argument("Mipmap minification requires GenerateMips");
    if(!file && (!Width || !Height || Format==ImageFormat::None)) throw std::invalid_argument("Generated textures require positive dimensions and a format");
    if(Format!=ImageFormat::None && Format!=ImageFormat::R8 && Format!=ImageFormat::RGB8 && Format!=ImageFormat::RGBA8 && Format!=ImageFormat::RGBA32F)
        throw std::invalid_argument("Unsupported texture format");
    if(file && Format==ImageFormat::RGBA32F) throw std::invalid_argument("RGBA32F requires explicit generated float data, not an 8-bit file");
}
TextureImage Texture2D::ReadImage(const std::filesystem::path& path, ImageFormat format) {
    TextureSpecification spec=TextureSpecification::FileDefaults(); spec.Format=format; spec.Validate(true);
    ScopedBuffer encoded(FileSystem::ReadFileBinary(path));
    if(!encoded || encoded.Size()>static_cast<uint64_t>(std::numeric_limits<int>::max())) throw std::runtime_error("Cannot read texture: "+path.generic_u8string());
    const int requested=format==ImageFormat::R8?1:format==ImageFormat::RGB8?3:format==ImageFormat::RGBA8?4:0;
    std::lock_guard<std::mutex> lock(DecodeMutex);
    stbi_set_flip_vertically_on_load(0); // All shared file decoding has top-left rows.
    int width=0,height=0,channels=0;
    if(!stbi_info_from_memory(encoded.Data(),static_cast<int>(encoded.Size()),&width,&height,&channels) || width<=0 || height<=0)
        throw std::runtime_error("Cannot inspect texture: "+path.generic_u8string());
    // Bound CPU allocations without needing a graphics context; backend enforces device limits too.
    if(static_cast<uint64_t>(width)*height>67108864) throw std::runtime_error("Texture exceeds 64 million pixel decode limit");
    int output=requested ? requested : channels==2 ? 4 : channels;
    auto* pixels=stbi_load_from_memory(encoded.Data(),static_cast<int>(encoded.Size()),&width,&height,&channels,output);
    if(!pixels) throw std::runtime_error("Cannot decode texture '"+path.generic_u8string()+"': "+stbi_failure_reason());
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
