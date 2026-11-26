#pragma once
#include <array>
#include <glm/glm.hpp>

namespace Hazel
{
struct SceneSubmission;
inline constexpr unsigned PointShadowFaces = 6;

struct alignas(16) PointShadowData
{
    glm::vec4 Parameters{0}; // One-based Forward+ index, side, axial near, inverse tangent.
};
static_assert(sizeof(PointShadowData) == 16, "Point shadow std140 ABI");

struct PointShadowLayout
{
    PointShadowData Data;
    std::array<glm::mat4, PointShadowFaces> VP{};
    unsigned LightIndex = 0, Side = 0, Requested = 0;
};

PointShadowLayout FitPointShadows(const SceneSubmission& frame);
} // namespace Hazel
