#pragma once

#include "Hazel/Renderer/Resources/Framebuffer.h"
#include <array>
#include <atomic>
#include <cstdint>
#include <memory>

namespace Hazel {
struct alignas(16) ExposureMeterParameters {
    glm::vec4 Range; // Accepted percentiles, minimum and maximum gain.
    glm::vec4 Metering; // Top-left spot center, radius in short-edge units, mode.
    glm::vec4 Step; // Brightening, darkening, previous valid, include sky.
};
static_assert(sizeof(ExposureMeterParameters) == 48);

struct ExposureMeterTiming {
    double Milliseconds = 0;
    uint64_t WorkingBytes = 0;
};
struct ExposureMeterPass {
    Ref<Framebuffer> Histogram, Previous, Current;
    ExposureMeterParameters Parameters;
    std::shared_ptr<std::atomic<bool>> Submitted;
    std::shared_ptr<ExposureMeterTiming> Timing;
};
}
