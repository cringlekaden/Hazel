#pragma once

#include "ColorGrade.h"
#include "LensEffects.h"

namespace Hazel
{
struct alignas(16) WorldPresentationParameters
{
    glm::vec4 ExposureAndLayers{0};
    glm::vec4 Defocus{0};
    LensPresentation Lens;
    ColorPresentation Color;
};
static_assert(sizeof(WorldPresentationParameters) == 176);
}
