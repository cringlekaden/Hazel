#pragma once

#include "Hazel/Renderer/Resources/Framebuffer.h"
#include "Hazel/Renderer/Resources/Texture.h"

namespace Hazel
{
struct alignas(16) LensPresentation
{
    glm::vec4 Sensor{0}; // Vignette, radial separation in pixels, distortion, grain amplitude.
    glm::vec4 Gate{0}; // Chief-ray tangents, grain sequence, flags (flare and explicit FP32 filtering).
    glm::vec4 Dirt{0}; // Strength, optical scale, texture aspect, texture present.
};
static_assert(sizeof(LensPresentation) == 48);

struct alignas(16) FlareParameters
{
    glm::vec4 Ghost{0}; // Strength, reflection count, spacing, halo strength.
    glm::vec4 Shape{0}; // Halo radius, starburst, anamorphic response, aspect.
};
static_assert(sizeof(FlareParameters) == 32);

struct LensEffectsTiming
{
    double Milliseconds = 0;
    uint64_t WorkingBytes = 0;
};

struct LensEffectsPass
{
    Ref<Framebuffer> Flare;
    Ref<Texture2D> DirtTexture;
    LensPresentation Presentation;
    FlareParameters Parameters;
    std::shared_ptr<LensEffectsTiming> Timing;

    void PrepareFlare(const FramebufferSpecification& source)
    {
        if (!Flare) {
            FramebufferSpecification size;
            size.Width = source.Width;
            size.Height = source.Height;
            size.Attachments = {FramebufferTextureFormat::RGBA16F};
            Flare = Framebuffer::Create(size);
        } else {
            const auto& existing = Flare->GetSpecification();
            if (existing.Width != source.Width || existing.Height != source.Height)
                Flare->Resize(source.Width, source.Height);
        }
    }
};
}
