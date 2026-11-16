#include "hzpch.h"
#include "MetalTexture.h"
#include "MetalDevice.h"
#include <stdexcept>
#include <cstring>
#include <algorithm>

namespace Hazel {
MetalTexture2D::MetalTexture2D(std::shared_ptr<MetalDevice> device, const TextureSpecification& specification, std::string path)
    : m_Device(std::move(device)), m_Specification(specification), m_Path(std::move(path))
{
    specification.Validate();
    if (!m_Device || specification.Width > 4096 || specification.Height > 4096 ||
        (specification.Format != ImageFormat::RGB8 && specification.Format != ImageFormat::RGBA8 && specification.Format != ImageFormat::RGBA32F))
        throw std::invalid_argument("Metal Renderer2D textures require <=4096x4096 RGB8/RGBA8 or RGBA32F");
    if (specification.Format == ImageFormat::RGBA32F &&
        (specification.MinFilter == TextureFilter::Linear || specification.MagFilter == TextureFilter::Linear) &&
        !m_Device->NativeDevice()->supports32BitFloatFiltering())
        throw std::runtime_error("Metal device does not support filtered RGBA32F inputs");
}
void MetalTexture2D::SetData(const void* data, uint32_t size)
{
    const uint32_t pixelBytes = m_Specification.Format == ImageFormat::RGB8 ? 3 : m_Specification.Format == ImageFormat::RGBA8 ? 4 : 16;
    if (!data || size != GetWidth() * GetHeight() * pixelBytes)
        throw std::invalid_argument("Metal texture upload requires the complete specified image");
    std::vector<uint8_t> expanded;
    if (pixelBytes == 3) {
        expanded.resize(size_t(GetWidth()) * GetHeight() * 4);
        const auto* source = static_cast<const uint8_t*>(data);
        for (size_t i = 0; i < size / 3; ++i) {
            std::memcpy(expanded.data() + i * 4, source + i * 3, 3);
            expanded[i * 4 + 3] = 255;
        }
        data = expanded.data();
    }
    auto pool = NS::TransferPtr(NS::AutoreleasePool::alloc()->init());
    auto version = std::make_shared<MetalTextureVersion>();
    auto descriptor = NS::TransferPtr(MTL::TextureDescriptor::alloc()->init());
    descriptor->setTextureType(MTL::TextureType2D); descriptor->setPixelFormat(m_Specification.Format != ImageFormat::RGBA32F ? (m_Specification.Encoding == TextureEncoding::SRGB ? MTL::PixelFormatRGBA8Unorm_sRGB : MTL::PixelFormatRGBA8Unorm) : MTL::PixelFormatRGBA32Float);
    descriptor->setWidth(GetWidth()); descriptor->setHeight(GetHeight());
    if (m_Specification.GenerateMips) {
        uint32_t levels = 1;
        for (uint32_t extent = std::max(GetWidth(), GetHeight()); extent > 1; extent /= 2) ++levels;
        descriptor->setMipmapLevelCount(levels);
    }
    descriptor->setStorageMode(MTL::StorageModeShared); descriptor->setUsage(MTL::TextureUsageShaderRead);
    version->Texture = NS::TransferPtr(m_Device->NativeDevice()->newTexture(descriptor.get()));
    auto sampler = NS::TransferPtr(MTL::SamplerDescriptor::alloc()->init());
    sampler->setSupportArgumentBuffers(true);
    sampler->setMinFilter((m_Specification.MinFilter == TextureFilter::Nearest || m_Specification.MinFilter == TextureFilter::NearestMipmapNearest || m_Specification.MinFilter == TextureFilter::NearestMipmapLinear) ? MTL::SamplerMinMagFilterNearest : MTL::SamplerMinMagFilterLinear);
    sampler->setMagFilter(m_Specification.MagFilter == TextureFilter::Nearest ? MTL::SamplerMinMagFilterNearest : MTL::SamplerMinMagFilterLinear);
    sampler->setMipFilter(!m_Specification.GenerateMips || unsigned(m_Specification.MinFilter) < 2 ? MTL::SamplerMipFilterNotMipmapped :
        unsigned(m_Specification.MinFilter) < 4 ? MTL::SamplerMipFilterNearest : MTL::SamplerMipFilterLinear);
    sampler->setSAddressMode(m_Specification.WrapS == TextureWrap::Repeat ? MTL::SamplerAddressModeRepeat : m_Specification.WrapS == TextureWrap::MirroredRepeat ? MTL::SamplerAddressModeMirrorRepeat : MTL::SamplerAddressModeClampToEdge);
    sampler->setTAddressMode(m_Specification.WrapT == TextureWrap::Repeat ? MTL::SamplerAddressModeRepeat : m_Specification.WrapT == TextureWrap::MirroredRepeat ? MTL::SamplerAddressModeMirrorRepeat : MTL::SamplerAddressModeClampToEdge);
    version->Sampler = NS::TransferPtr(m_Device->NativeDevice()->newSamplerState(sampler.get()));
    if (!version->Texture || !version->Sampler) throw std::runtime_error("Cannot allocate Metal texture/sampler version");
    // Top-left source rows are unchanged, matching the existing Texture2D contract.
    version->Texture->replaceRegion(MTL::Region(0, 0, GetWidth(), GetHeight()), 0, data, GetWidth() * (pixelBytes == 3 ? 4 : pixelBytes));
    std::lock_guard<std::mutex> lock(m_Mutex);
    m_Version = std::move(version); // Earlier draws retain their immutable snapshots.
}
std::shared_ptr<const MetalTextureVersion> MetalTexture2D::Snapshot() const
{
    std::lock_guard<std::mutex> lock(m_Mutex);
    if (!m_Version) throw std::logic_error("Upload Metal texture data before drawing");
    return m_Version;
}
bool MetalTexture2D::IsLoaded() const { std::lock_guard<std::mutex> lock(m_Mutex); return bool(m_Version); }
uint32_t MetalTexture2D::GetRendererID() const { throw std::logic_error("Metal textures have no OpenGL renderer ID"); }
void MetalTexture2D::Bind(uint32_t) const { throw std::logic_error("Metal textures require explicit quad argument tables"); }
}
