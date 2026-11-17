#pragma once
#include "Hazel/Renderer/Resources/Texture.h"
#include "Hazel/Renderer/Resources/EnvironmentResources.h"
#include <Metal/Metal.hpp>
#include <memory>
#include <mutex>

namespace Hazel {
class MetalDevice;
struct MetalTextureVersion
{
    NS::SharedPtr<MTL::Texture> Texture;
    mutable bool MipsSubmitted = false; // renderer-thread publication after queue commit
    NS::SharedPtr<MTL::SamplerState> Sampler;
};
class MetalTextureCube final : public TextureCube
{
public:
    MetalTextureCube(std::shared_ptr<MetalDevice> device, const LightingImage& image);
    void Bind(uint32_t slot) const override;
private:
    std::shared_ptr<MetalDevice> m_Device;
    std::shared_ptr<const MetalTextureVersion> m_Version;
};
class MetalTexture2D final : public Texture2D
{
public:
    MetalTexture2D(std::shared_ptr<MetalDevice> device, const TextureSpecification& specification, std::string path = {});
    const TextureSpecification& GetSpecification() const override { return m_Specification; }
    uint32_t GetWidth() const override { return m_Specification.Width; }
    uint32_t GetHeight() const override { return m_Specification.Height; }
    uint32_t GetRendererID() const override;
    EditorTexture GetEditorTexture() const override { auto version = Snapshot(); return {reinterpret_cast<uintptr_t>(version->Texture.get()), version, false, reinterpret_cast<uintptr_t>(version->Sampler.get())}; }
    const std::string& GetPath() const override { return m_Path; }
    void SetData(const void* data, uint32_t size) override;
    void Bind(uint32_t slot) const override;
    bool IsLoaded() const override;
    bool operator==(const Texture& other) const override { return this == &other; }
    std::shared_ptr<const MetalTextureVersion> Snapshot() const;
private:
    std::shared_ptr<MetalDevice> m_Device;
    TextureSpecification m_Specification;
    std::string m_Path;
    mutable std::mutex m_Mutex;
    std::shared_ptr<const MetalTextureVersion> m_Version;
};
}
