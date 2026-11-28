#include "hzpch.h"
#include "MetalTexture.h"
#include "MetalDevice.h"
#include "MetalRendererAPI.h"
#include "Hazel/Renderer/RenderCommand.h"
#include <stdexcept>
#include <filesystem>
#include <cstring>
#include <algorithm>

namespace Hazel {
namespace {
void BindVersion(const std::shared_ptr<MetalDevice>& device, uint32_t slot,
                 const std::shared_ptr<const MetalTextureVersion>& version)
{
    auto* backend = dynamic_cast<MetalRendererAPI*>(&RenderCommand::Backend());
    if (!backend || backend->Device() != device) throw std::logic_error("Metal texture binding requires its owning backend");
    backend->BindTexture(slot, version);
}
}
MetalTexture2D::MetalTexture2D(std::shared_ptr<MetalDevice> device, const TextureSpecification& specification, std::string path)
    : m_Device(std::move(device)), m_Specification(specification),
      m_Path(path.empty() ? std::string{} : std::filesystem::absolute(std::filesystem::u8path(path)).lexically_normal().generic_u8string())
{
    specification.Validate();
    if (!m_Device || specification.Width > 16384 || specification.Height > 16384 ||
        (specification.Format != ImageFormat::RGB8 && specification.Format != ImageFormat::RGBA8 && specification.Format != ImageFormat::RGBA32F && specification.Format != ImageFormat::RG32F && specification.Format != ImageFormat::RGBA16F))
        throw std::invalid_argument("Metal textures require <=16384x16384 RGB8/RGBA8, RG32F, RGBA16F or RGBA32F");
    if ((specification.Format == ImageFormat::RGBA32F || specification.Format == ImageFormat::RG32F) &&
        (specification.MinFilter == TextureFilter::Linear || specification.MagFilter == TextureFilter::Linear) &&
        !m_Device->NativeDevice()->supports32BitFloatFiltering())
        throw std::runtime_error("Metal device does not support filtered RGBA32F inputs");
}
void MetalTexture2D::SetData(const void* data, uint32_t size)
{
    const uint32_t pixelBytes = m_Specification.Format == ImageFormat::RGB8 ? 3 : m_Specification.Format == ImageFormat::RGBA8 ? 4 : (m_Specification.Format == ImageFormat::RG32F || m_Specification.Format == ImageFormat::RGBA16F) ? 8 : 16;
    if (!data || uint64_t(size) != uint64_t(GetWidth()) * GetHeight() * pixelBytes)
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
    descriptor->setTextureType(MTL::TextureType2D);
    auto format = m_Specification.Encoding == TextureEncoding::SRGB ? MTL::PixelFormatRGBA8Unorm_sRGB : MTL::PixelFormatRGBA8Unorm;
    if (m_Specification.Format == ImageFormat::RGBA16F) format = MTL::PixelFormatRGBA16Float;
    if (m_Specification.Format == ImageFormat::RGBA32F) format = MTL::PixelFormatRGBA32Float;
    if (m_Specification.Format == ImageFormat::RG32F) format = MTL::PixelFormatRG32Float;
    descriptor->setPixelFormat(format);
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
void MetalTexture2D::Bind(uint32_t slot) const
{
    BindVersion(m_Device, slot, Snapshot());
}
MetalTextureCube::MetalTextureCube(std::shared_ptr<MetalDevice> device, const LightingImage& image)
    : m_Device(device)
{
    image.Validate();
    if (!device || image.Faces() != 6 || image.Side > 16384) throw std::invalid_argument("Invalid Metal prepared cubemap");
    if (!device->NativeDevice()->supports32BitFloatFiltering()) throw std::runtime_error("Metal prepared IBL requires FP32 filtering");
    auto version = std::make_shared<MetalTextureVersion>();
    auto descriptor = NS::TransferPtr(MTL::TextureDescriptor::alloc()->init());
    descriptor->setTextureType(MTL::TextureTypeCube); descriptor->setPixelFormat(MTL::PixelFormatRGBA32Float);
    descriptor->setWidth(image.Side); descriptor->setHeight(image.Side); descriptor->setMipmapLevelCount(image.Mips.size());
    descriptor->setStorageMode(MTL::StorageModeShared); descriptor->setUsage(MTL::TextureUsageShaderRead);
    version->Texture = NS::TransferPtr(device->NativeDevice()->newTexture(descriptor.get()));
    auto sampler = NS::TransferPtr(MTL::SamplerDescriptor::alloc()->init());
    sampler->setSupportArgumentBuffers(true);
    sampler->setMinFilter(MTL::SamplerMinMagFilterLinear); sampler->setMagFilter(MTL::SamplerMinMagFilterLinear);
    sampler->setMipFilter(image.Mips.size() > 1 ? MTL::SamplerMipFilterLinear : MTL::SamplerMipFilterNotMipmapped);
    sampler->setSAddressMode(MTL::SamplerAddressModeClampToEdge); sampler->setTAddressMode(MTL::SamplerAddressModeClampToEdge);
    sampler->setRAddressMode(MTL::SamplerAddressModeClampToEdge);
    version->Sampler = NS::TransferPtr(device->NativeDevice()->newSamplerState(sampler.get()));
    if (!version->Texture || !version->Sampler) throw std::runtime_error("Cannot allocate prepared Metal cube");
    uint32_t side = image.Side;
    for (size_t mip = 0; mip < image.Mips.size(); ++mip) {
        std::vector<float> rgba(size_t(side) * side * 4);
        for (size_t face = 0; face < 6; ++face) {
            const auto* rgb = image.Mips[mip].data() + face * size_t(side) * side * 3;
            for (size_t pixel = 0; pixel < size_t(side) * side; ++pixel) {
                for (size_t c = 0; c < 3; ++c) rgba[pixel * 4 + c] = rgb[pixel * 3 + c];
                rgba[pixel * 4 + 3] = 1;
            }
            version->Texture->replaceRegion(MTL::Region(0, 0, side, side), mip, face, rgba.data(), size_t(side) * 16, size_t(side) * side * 16);
        }
        side = std::max(1u, side / 2);
    }
    version->MipsSubmitted = true; // Complete prepared levels: never generate or reconstruct them.
    m_Version = std::move(version);
}
void MetalTextureCube::Bind(uint32_t slot) const
{
    BindVersion(m_Device, slot, m_Version);
}
}
