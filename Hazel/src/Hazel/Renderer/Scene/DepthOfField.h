#pragma once

#include "Hazel/Renderer/Resources/Framebuffer.h"
#include <array>
#include <cmath>
#include <cstdint>
#include <memory>

namespace Hazel
{
struct alignas(16) DefocusParameters
{
    glm::vec4 Lens{0}; // Window depth at focus, pixels/depth, radius limit, enabled.
    glm::vec4 Gather{0}; // Gather limit and explicit FP32 filtering flag.
    std::array<glm::vec4, 44> Taps{}; // Pupil offset, radial fraction and area weight.

    void SetPupil(unsigned blades, float rotation, unsigned maxTaps)
    {
        Gather.x = float(maxTaps);
        constexpr double tau = 6.283185307179586;
        const double sector = blades ? tau / blades : tau;
        const auto ring = [&](unsigned first, unsigned count) {
            for (unsigned i = 0; i < count; ++i) {
                const double angle = i * 2.399963229728653;
                const double radius = std::sqrt((i + .5) / count);
                const double boundary = blades ? std::cos(.5 * sector) /
                    std::cos(std::remainder(angle - rotation, sector)) : 1.0;
                Taps[first + i] = {float(std::cos(angle) * radius * boundary),
                    float(std::sin(angle) * radius * boundary), float(radius), float(boundary * boundary)};
            }
        };
        ring(0, 12);
        if (maxTaps != 12)
            ring(12, maxTaps);
    }
};
static_assert(sizeof(DefocusParameters) == 736);

struct DepthOfFieldTiming
{
    std::array<double, 4> Milliseconds{};
    uint64_t WorkingBytes = 0;
};

struct DepthOfFieldPass
{
    Ref<Framebuffer> Tiles, DilatedTiles, Prefiltered, Gathered;
    DefocusParameters Parameters;
    std::shared_ptr<DepthOfFieldTiming> Timing;

    void Prepare(glm::uvec2 extent)
    {
        const auto half = (extent + 1u) / 2u;
        const auto tiles = (extent + 15u) / 16u;
        if (Gathered && Gathered->GetSpecification().Width == half.x &&
            Gathered->GetSpecification().Height == half.y)
            return;
        FramebufferSpecification spec;
        spec.Scratch = FramebufferSpecification::CameraScratch::Defocus;
        spec.Width = tiles.x;
        spec.Height = tiles.y;
        spec.Attachments = {FramebufferTextureFormat::RGBA32F};
        auto classified = Framebuffer::Create(spec);
        auto dilated = Framebuffer::Create(spec);
        spec.Width = half.x;
        spec.Height = half.y;
        auto prefilter = Framebuffer::Create(spec);
        spec.Attachments = {FramebufferTextureFormat::RGBA32F, FramebufferTextureFormat::RGBA32F};
        auto gathered = Framebuffer::Create(spec);
        Tiles = std::move(classified);
        DilatedTiles = std::move(dilated);
        Prefiltered = std::move(prefilter);
        Gathered = std::move(gathered);
    }
};
}
