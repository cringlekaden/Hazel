#pragma once
#include <glm/glm.hpp>
#include <array>
#include <cstddef>
#include <cstdint>

namespace Hazel {
// Packed stream shared by Renderer2D's existing quad path and native backends.
struct QuadVertex
{
    glm::vec3 Position;
    glm::vec4 Color;
    glm::vec2 TexCoord;
    float TexIndex, TilingFactor;
    int32_t EntityID;
    glm::vec4 ClipRect;
};
static_assert(sizeof(QuadVertex) == 64 && offsetof(QuadVertex, Color) == 12 &&
    offsetof(QuadVertex, EntityID) == 44 && offsetof(QuadVertex, ClipRect) == 48, "Renderer2D quad ABI");
inline constexpr std::array<uint32_t, 6> Renderer2DQuadIndices{0, 1, 2, 2, 3, 0};
inline std::array<QuadVertex, 4> BuildRenderer2DQuad(const glm::mat4& transform,
    const std::array<glm::vec2, 4>& corners, const std::array<glm::vec2, 4>& uv,
    const glm::vec4& color, float tiling, float textureIndex, int32_t entity, const glm::vec4& clip)
{
    std::array<QuadVertex, 4> vertices;
    for (size_t i = 0; i < vertices.size(); ++i)
        vertices[i] = {glm::vec3(transform * glm::vec4(corners[i], 0, 1)), color, uv[i], textureIndex, tiling, entity, clip};
    return vertices;
}
}
