#pragma once
#include "MetalFramebuffer.h"
#include <atomic>
namespace Hazel {
struct TemporalTiming;
struct MetalTemporalConstants {
    glm::vec4 ExtentJitter{};
    glm::uvec4 Options{};
    glm::mat4 SkyMotion{1}; // Current stable ray to previous clip, with translation removed.
};
static_assert(sizeof(MetalTemporalConstants)==96,"Temporal constant ABI");
struct MetalTemporalPipeline {
    NS::SharedPtr<MTL::Library> Library;
    NS::SharedPtr<MTL::ComputePipelineState> State;
    explicit MetalTemporalPipeline(MetalDevice& device);
};
struct MetalTemporalHistory {
    std::shared_ptr<const MetalFramebufferAttachments> Color; // RGB radiance / alpha depth, separate exact R32Sint IDs.
    std::shared_ptr<std::atomic<bool>> Submitted;
};
struct MetalTemporalCounters {
    NS::SharedPtr<MTL4::CounterHeap> Heap;
    uint64_t Frequency=0;
    explicit MetalTemporalCounters(MetalDevice& device);
};
struct MetalTemporalPass {
    std::shared_ptr<MetalTemporalPipeline> Pipeline;
    std::shared_ptr<MetalTemporalHistory> Current, Previous;
    MetalTemporalConstants Constants;
    NS::SharedPtr<MTL4::ArgumentTable> Arguments;
    NS::SharedPtr<MTL4::CounterHeap> Counters;
    std::shared_ptr<MetalTemporalCounters> CounterOwner;
    uint64_t Frequency=0, Serial=0;
    bool Trace=false, TimingsResolved=false;
    std::shared_ptr<TemporalTiming> Timing;
    void ResolveTimings();
};
}
