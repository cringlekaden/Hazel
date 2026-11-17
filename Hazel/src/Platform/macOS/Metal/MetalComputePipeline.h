#pragma once
#include "Hazel/Renderer/Resources/ComputePipeline.h"
#include <Metal/Metal.hpp>
#include <memory>
namespace Hazel {
class MetalDevice;
class MetalComputePipeline final : public ComputePipeline {
public:
    MetalComputePipeline(std::shared_ptr<MetalDevice> device, const std::filesystem::path& library, const std::string& entry);
    const std::shared_ptr<MetalDevice>& Device() const { return m_Device; }
    size_t ParameterBytes() const { return m_ParameterBytes; }
    MTL::ComputePipelineState* Native() const { return m_Pipeline.get(); }
private:
    size_t m_ParameterBytes = 0;
    std::shared_ptr<MetalDevice> m_Device;
    NS::SharedPtr<MTL::Library> m_Library;
    NS::SharedPtr<MTL::ComputePipelineState> m_Pipeline;
};
}
