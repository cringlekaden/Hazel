#include "hzpch.h"
#include "MetalTexture.h"
#include "MetalDevice.h"
#include <stdexcept>
#include <cstring>

namespace Hazel {
MetalTexture2D::MetalTexture2D(std::shared_ptr<MetalDevice> device, const TextureSpecification& specification)
    : m_Device(std::move(device)), m_Specification(specification)
{
    specification.Validate();
    if (!m_Device || specification.Width > 32 || specification.Height > 32 ||
        (specification.Format != ImageFormat::RGBA8 && specification.Format != ImageFormat::RGBA32F) || specification.Encoding != TextureEncoding::Linear ||
        specification.GenerateMips ||
        (specification.MinFilter != TextureFilter::Nearest && specification.MinFilter != TextureFilter::Linear) ||
        (specification.MagFilter != TextureFilter::Nearest && specification.MagFilter != TextureFilter::Linear) ||
        (specification.WrapS != TextureWrap::Repeat && specification.WrapS != TextureWrap::ClampToEdge) ||
        (specification.WrapT != TextureWrap::Repeat && specification.WrapT != TextureWrap::ClampToEdge))
        throw std::invalid_argument("Metal offscreen quad textures require <=32x32 RGBA8/RGBA32F linear, no mips, nearest/linear and repeat/clamp sampling");
    if (specification.Format == ImageFormat::RGBA32F &&
        (specification.MinFilter == TextureFilter::Linear || specification.MagFilter == TextureFilter::Linear) &&
        !m_Device->NativeDevice()->supports32BitFloatFiltering())
        throw std::runtime_error("Metal device does not support filtered RGBA32F inputs");
}
void MetalTexture2D::SetData(const void* data, uint32_t size)
{
    const uint32_t pixelBytes = m_Specification.Format == ImageFormat::RGBA8 ? 4 : 16;
    if (!data || size != GetWidth() * GetHeight() * pixelBytes)
        throw std::invalid_argument("Metal texture upload requires the complete specified image");
    const auto* bytes = static_cast<const uint8_t*>(data);
    for (uint32_t offset = 0; offset < size; offset += pixelBytes)
    {
        float alpha = 1;
        if (pixelBytes == 16) std::memcpy(&alpha, bytes + offset + 12, 4);
        if ((pixelBytes == 4 && bytes[offset + 3] != 255) || alpha != 1)
            throw std::invalid_argument("Metal offscreen quad profile requires opaque texture data; blending is unavailable");
    }
    auto pool = NS::TransferPtr(NS::AutoreleasePool::alloc()->init());
    auto version = std::make_shared<MetalTextureVersion>();
    auto descriptor = NS::TransferPtr(MTL::TextureDescriptor::alloc()->init());
    descriptor->setTextureType(MTL::TextureType2D); descriptor->setPixelFormat(m_Specification.Format == ImageFormat::RGBA8 ? MTL::PixelFormatRGBA8Unorm : MTL::PixelFormatRGBA32Float);
    descriptor->setWidth(GetWidth()); descriptor->setHeight(GetHeight());
    descriptor->setStorageMode(MTL::StorageModeShared); descriptor->setUsage(MTL::TextureUsageShaderRead);
    version->Texture = NS::TransferPtr(m_Device->NativeDevice()->newTexture(descriptor.get()));
    auto sampler = NS::TransferPtr(MTL::SamplerDescriptor::alloc()->init());
    sampler->setSupportArgumentBuffers(true);
    sampler->setMinFilter(m_Specification.MinFilter == TextureFilter::Nearest ? MTL::SamplerMinMagFilterNearest : MTL::SamplerMinMagFilterLinear);
    sampler->setMagFilter(m_Specification.MagFilter == TextureFilter::Nearest ? MTL::SamplerMinMagFilterNearest : MTL::SamplerMinMagFilterLinear);
    sampler->setSAddressMode(m_Specification.WrapS == TextureWrap::Repeat ? MTL::SamplerAddressModeRepeat : MTL::SamplerAddressModeClampToEdge);
    sampler->setTAddressMode(m_Specification.WrapT == TextureWrap::Repeat ? MTL::SamplerAddressModeRepeat : MTL::SamplerAddressModeClampToEdge);
    version->Sampler = NS::TransferPtr(m_Device->NativeDevice()->newSamplerState(sampler.get()));
    if (!version->Texture || !version->Sampler) throw std::runtime_error("Cannot allocate Metal texture/sampler version");
    // Top-left source rows are unchanged, matching the existing Texture2D contract.
    version->Texture->replaceRegion(MTL::Region(0, 0, GetWidth(), GetHeight()), 0, data, GetWidth() * pixelBytes);
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
