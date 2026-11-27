#pragma once
#include "Hazel/Renderer/Resources/Shader.h"
#include "MetalShaderArtifact.h"
#include "Hazel/Renderer/RendererAPI.h"
#include <Metal/Metal.hpp>
#include <memory>

namespace Hazel {
class MetalDevice;
// Immutable library/pipeline resource. Owners of submitted work must retain the
// shader until GPU completion, like buffers/textures; this class submits no work.
class MetalShader final : public Shader
{
public:
    MetalShader(std::shared_ptr<MetalDevice> device, const std::string& name, uint32_t slots = 32, bool drawableOutput = false, const RasterState* raster = nullptr, bool reflectionSurface = false);
    const std::string& GetName() const override { return m_Artifact.Name; }
    ProgramLoadingPath GetProgramLoadingPath() const override { return ProgramLoadingPath::MetalLibrary; }
    const MetalShaderArtifact& Artifact() const { return m_Artifact; }
    MTL::RenderPipelineState* Pipeline() const { return m_Pipeline.get(); }
    size_t VertexStride() const { return m_Stride; }
    const std::string& NativeReflection() const { return m_Reflection; }
    const std::string& Identity() const { return m_Identity; }
    void Bind() const override;
    void Unbind() const override;
    void SetInt(const std::string&, int) override;
    void SetIntArray(const std::string&, const int*, uint32_t) override;
    void SetFloat(const std::string&, float) override;
    void SetFloat2(const std::string&, const glm::vec2&) override;
    void SetFloat3(const std::string&, const glm::vec3&) override;
    void SetFloat4(const std::string&, const glm::vec4&) override;
    void SetMat4(const std::string&, const glm::mat4&) override;
private:
    std::shared_ptr<MetalDevice> m_Device;
    MetalShaderArtifact m_Artifact;
    std::array<NS::SharedPtr<MTL::Library>, 2> m_Libraries;
    NS::SharedPtr<MTL::RenderPipelineState> m_Pipeline;
    std::string m_Identity, m_Reflection;
    size_t m_Stride = 0;
};
}
