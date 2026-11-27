#pragma once

#include "Hazel/Renderer/Resources/Framebuffer.h"
#include <array>
#include <cstddef>
#include <utility>
#include <vector>

namespace Hazel {
struct BloomTiming
{
    // Completion intervals for extraction, downsampling, upsampling and presentation.
    std::array<double, 4> Milliseconds{};
    uint64_t WorkingBytes = 0;
    bool Valid = false;
};
struct BloomPyramid
{
    struct Level
    {
        Ref<Framebuffer> Down, Up;
    };
    std::vector<Level> Levels;
    float Intensity = 0, Radius = 0;
    bool BuildBloom = true;
    std::shared_ptr<BloomTiming> Timing; // Optional instrumentation.

    glm::vec4 FilterParameters(float mode, uint32_t sourceWidth, uint32_t sourceHeight) const
    {
        return {mode, .18f + .5f * Radius, 1.f / sourceWidth, 1.f / sourceHeight};
    }
    uint64_t TexturePayloadBytes() const
    {
        uint64_t bytes = 0;
        for (const auto& level : Levels) {
            const auto& size = level.Down->GetSpecification();
            bytes += uint64_t(size.Width) * size.Height * (level.Up ? 16 : 8);
        }
        return bytes;
    }

    void Prepare(uint32_t width, uint32_t height, float intensity, float radius, bool buildBloom = true)
    {
        Intensity = intensity;
        Radius = radius;
        width = width / 2 + width % 2;
        height = height / 2 + height % 2;
        if (!Levels.empty()) {
            const auto& size = Levels.front().Down->GetSpecification();
            if (size.Width == width && size.Height == height && BuildBloom == buildBloom)
                return;
        }
        // Additional octaves keep the largest blur approximately proportional to
        // the shorter view dimension, up to the supported Retina view size.
        std::vector<Level> levels;
        const auto limit = !buildBloom ? 3u : (width > 720 && height > 720) ? 5u : 4u;
        for (unsigned i = 0; i < limit; ++i) {
            FramebufferSpecification size;
            size.Width = width;
            size.Height = height;
            size.Attachments = {FramebufferTextureFormat::RGBA16F};
            levels.push_back({Framebuffer::Create(size), {}});
            if (width == 1 && height == 1)
                break;
            width = width / 2 + width % 2;
            height = height / 2 + height % 2;
        }
        if (buildBloom)
            for (size_t i = 0; i + 1 < levels.size(); ++i)
                levels[i].Up = Framebuffer::Create(levels[i].Down->GetSpecification());
        Levels = std::move(levels);
        BuildBloom = buildBloom;
    }
};
}
