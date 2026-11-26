#pragma once
#include <array>
#include <cstdint>
#include <glm/glm.hpp>

namespace Hazel
{
struct SceneSubmission;
inline constexpr unsigned MaxSpotShadows = 4;

// std140 binding 1. The local-light indices are one-based; zero is unused.
struct alignas(16) SpotShadowData
{
    std::array<glm::mat4, MaxSpotShadows> VP{}; // Applies to light-relative positions.
    std::array<glm::vec4, MaxSpotShadows> Tiles{}; // X origin, side, near, inverse projection scale.
    glm::ivec4 LightIndices{0};
};
static_assert(sizeof(SpotShadowData) == 336, "Spot shadow std140 ABI");

struct SpotShadowLayout
{
    SpotShadowData Data;
    glm::uvec2 Extent{0};
    unsigned Count = 0, Requested = 0;
};

SpotShadowLayout FitSpotShadows(const SceneSubmission& frame);
} // namespace Hazel
