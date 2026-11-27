#pragma once

#include "Hazel/Renderer/Scene/ExposureMeter.h"
#include <Metal/Metal.hpp>

namespace Hazel {
struct MetalFramebufferAttachments;
struct MetalExposureMeter {
    std::shared_ptr<const MetalFramebufferAttachments> Histogram, Previous, Current;
    ExposureMeterParameters Parameters;
    std::shared_ptr<std::atomic<bool>> Submitted;
    std::array<NS::SharedPtr<MTL4::ArgumentTable>, 2> Arguments;
    std::shared_ptr<ExposureMeterTiming> Timing;
    NS::SharedPtr<MTL4::CounterHeap> Counters;
    uint64_t Frequency = 0;
    void ResolveTiming();
};
}
