#pragma once
#include <glm/glm.hpp>
#include <array>
#include <cstdint>
#include "MetalTexture.h"
#include "Hazel/Renderer/RendererAPI.h"
#include "Hazel/Renderer/Resources/Framebuffer.h"
#include <Metal/Metal.hpp>
#include <memory>
#include <atomic>
#include <vector>
#include <set>
#include <string>

namespace Hazel {
class MetalShader;
class MetalComputePipeline;
struct MetalAOPass;
struct MetalTemporalPass;
struct MetalFramebufferAttachments;
struct MetalRenderer2DDraw
{
    std::vector<uint8_t> Vertices;
    // Indexed production draws retain native geometry and immutable UBO values.
    NS::SharedPtr<MTL::Buffer> MeshVertices, MeshIndices;
    std::array<std::vector<uint8_t>, 8> Uniforms;
    uint32_t FirstIndex = 0;
    bool IndexedMesh = false;
    std::shared_ptr<MetalShader> Shader;
    RasterState Raster;
    NS::SharedPtr<MTL::DepthStencilState> DepthState;
    uint32_t Count = 0;
    glm::mat4 Camera;
    std::array<std::shared_ptr<const MetalTextureVersion>, 32> Textures;
};
struct MetalRenderer2DFrame
{
    uint32_t Width = 0, Height = 0;
    glm::vec4 Clear{0};
    int32_t ClearEntity = -1;
    std::shared_ptr<MetalShader> OutputShader;
    NS::SharedPtr<MTL::SamplerState> OutputSampler;
    NS::SharedPtr<MTL4::ArgumentTable> OutputArguments;
    bool Readback = true, EditorPicking = false, QueueOrderedTemporalProducer = false;
    uint64_t PacketBytes = 0;
    std::shared_ptr<MetalRenderer2DFrame> World;
    std::shared_ptr<MetalShader> PresentationShader;
    NS::SharedPtr<MTL4::ArgumentTable> PresentationArguments;
    float Exposure = 0;
    std::shared_ptr<MetalRenderer2DFrame> AOSource;
    std::shared_ptr<MetalAOPass> AO;
    std::shared_ptr<MetalTemporalPass> Temporal;
    std::shared_ptr<MetalRenderer2DFrame> TemporalSource, MotionSource;
    std::shared_ptr<MetalRenderer2DFrame> ComputeSource;
    std::shared_ptr<MetalComputePipeline> Compute;
    std::vector<uint8_t> ComputeParameters;
    NS::SharedPtr<MTL4::ArgumentTable> ComputeArguments;
    std::shared_ptr<MetalRenderer2DFrame> DisplaySource;
    std::shared_ptr<MetalShader> OutlineShader;
    int OutlineEntity = -1, OutlineRadius = 0;
    glm::vec4 OutlineColor{0};
    NS::SharedPtr<MTL4::ArgumentTable> OutlineArguments;
    std::vector<MetalRenderer2DDraw> Draws;
    std::vector<std::shared_ptr<void>> ResourceOwners;
    std::shared_ptr<FramebufferReadback> Result;
    std::vector<glm::vec4> CompletedColor;
    std::vector<int32_t> CompletedEntity;
    std::vector<float> CompletedDepth;
    std::shared_ptr<std::string> FeedbackError = std::make_shared<std::string>();
    std::shared_ptr<std::atomic<bool>> Feedback = std::make_shared<std::atomic<bool>>(false);
    std::shared_ptr<const MetalFramebufferAttachments> Attachments;
    std::vector<NS::SharedPtr<MTL::Buffer>> Buffers;
    std::vector<NS::SharedPtr<MTL4::ArgumentTable>> Arguments;
    NS::SharedPtr<MTL::ResidencySet> Residency;
    void Complete();
    void ResolveAmbientOcclusionTimings();
    void MarkMipmapsSubmitted() const;
    void CollectAllocations(std::set<const MTL::Allocation*>& allocations) const;
};
}
