#pragma once

#include "Hazel/Renderer/Resources/Texture.h"
#include <filesystem>
#include <glm/glm.hpp>

namespace Hazel
{
struct alignas(16) ColorPresentation
{
    glm::vec4 White[3]{glm::vec4(0), glm::vec4(0), glm::vec4(0)}; // RGB matrix rows, with the enable flag in the first row's W.
    glm::vec4 Tone{0}; // Saturation delta, contrast, shadows and highlights.
    glm::vec4 LUTScale{0}, LUTBias{0}; // Input domain transform, grid size and mix.
};
static_assert(sizeof(ColorPresentation) == 96);

struct ColorGradePass
{
    ColorPresentation Parameters;
    Ref<Texture2D> LUT;
};

struct ColorGradeLUT
{
    Ref<Texture2D> Texture;
    glm::vec3 Scale{1}, Bias{0};
    unsigned Size = 0;
};

ColorGradeLUT LoadColorGradeLUT(const std::filesystem::path& path);
}
