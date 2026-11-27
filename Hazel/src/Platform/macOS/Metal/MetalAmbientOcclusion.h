#pragma once
#include "Hazel/Renderer/Scene/SceneSubmission.h"
#include <Metal/Metal.hpp>
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>
namespace Hazel {
class MetalDevice;
struct AmbientOcclusionTiming;
struct MetalAOConstants {
    glm::mat4 InverseVP{1}, WorldToView{1};
    glm::vec4 ExtentRadius{};
    glm::vec4 PositionScaleBias{}; // UV to view XY; perspective multiplies by positive view Z.
    glm::uvec4 Quality{};
};
static_assert(sizeof(MetalAOConstants)==176 && offsetof(MetalAOConstants,WorldToView)==64 &&
    offsetof(MetalAOConstants,ExtentRadius)==128 && offsetof(MetalAOConstants,PositionScaleBias)==144 &&
    offsetof(MetalAOConstants,Quality)==160, "XeGTAO constant ABI");
struct MetalAOPipelines {
    NS::SharedPtr<MTL::Library> Library;
    std::array<NS::SharedPtr<MTL::ComputePipelineState>,7> States;
    explicit MetalAOPipelines(MetalDevice& device);
};
// Queue-ordered private scratch. Submitted packets retain their extent generation.
struct MetalAOResources {
    uint32_t Width=0, Height=0;
    std::shared_ptr<MetalAOPipelines> Pipelines;
    NS::SharedPtr<MTL::Texture> Depth, Raw, Edges, Scratch, Visibility;
    std::vector<NS::SharedPtr<MTL::Texture>> DepthMips;
    MetalAOResources(MetalDevice& device, std::shared_ptr<MetalAOPipelines> pipelines, uint32_t width, uint32_t height);
};
struct MetalAOCounters {
    NS::SharedPtr<MTL4::CounterHeap> Counters;
    uint64_t TimestampFrequency=0;
    explicit MetalAOCounters(MetalDevice& device);
};
struct MetalAOPass {
    unsigned DebugView=0;
    std::shared_ptr<MetalAOResources> Resources;
    std::shared_ptr<MetalAOCounters> CounterOwner;
    MetalAOConstants Constants;
    std::shared_ptr<AmbientOcclusionTiming> Timing;
    NS::SharedPtr<MTL4::CounterHeap> Counters;
    uint64_t TimestampFrequency=0;
    bool TimingsResolved=false, Trace=false;
    uint64_t Serial=0;
    void ResolveTimings();
    std::vector<NS::SharedPtr<MTL4::ArgumentTable>> Arguments;
};
MetalAOConstants AmbientOcclusionConstants(const SceneCameraView& camera, const AmbientOcclusionSettings& settings, uint32_t width, uint32_t height, unsigned temporalPhase = 0);
}
