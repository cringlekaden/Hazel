#pragma once

#include "Hazel/Renderer/Resources/Framebuffer.h"
#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <utility>

namespace Hazel
{
struct alignas(16) ShutterParameters
{
    glm::mat4 CurrentToPrevious{1}, SkyToPrevious{1};
    glm::vec4 Shutter{0}, Projection{0};
    glm::uvec4 MovingIDMask{0}; // Two 32-bit buckets with no false-negative IDs.
};
static_assert(sizeof(ShutterParameters) == 176);

struct CameraMotionTiming
{
    std::array<double, 3> Milliseconds{};
    uint64_t WorkingBytes = 0;
};

struct CameraMotionPass
{
    Ref<Framebuffer> Vectors, TileMax, NeighborMax, Filtered;
    ShutterParameters Parameters;
    std::shared_ptr<std::atomic<bool>> Submitted;
    std::shared_ptr<CameraMotionTiming> Timing;
    bool Valid = false;

    void Prepare(glm::uvec2 extent)
    {
        const auto half = (extent + 1u) / 2u;
        if (Filtered && Filtered->GetSpecification().Width == half.x &&
            Filtered->GetSpecification().Height == half.y)
            return;
        FramebufferSpecification spec;
        spec.Scratch = FramebufferSpecification::CameraScratch::MotionBlur;
        spec.Width = (extent.x + 15u) / 16u;
        spec.Height = (extent.y + 15u) / 16u;
        spec.Attachments = {FramebufferTextureFormat::RGBA32F};
        auto tiles = Framebuffer::Create(spec);
        auto neighbors = Framebuffer::Create(spec);
        spec.Width = half.x;
        spec.Height = half.y;
        auto filtered = Framebuffer::Create(spec);
        TileMax = std::move(tiles);
        NeighborMax = std::move(neighbors);
        Filtered = std::move(filtered);
    }
};
}
