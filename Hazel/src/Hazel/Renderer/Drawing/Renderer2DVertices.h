#pragma once
#include "Renderer2DQuad.h"

namespace Hazel {
struct CircleVertex
{
    glm::vec3 WorldPosition, LocalPosition;
    glm::vec4 Color;
    float Thickness, Fade;
    int32_t EntityID;
    glm::vec4 ClipRect;
};
struct LineVertex
{
    glm::vec3 Position;
    glm::vec4 Color;
    glm::vec4 Shape; // physical-pixel midpoint and unit tangent
    glm::vec4 Extent; // length, width and endpoint window depths
    int32_t EntityID;
    glm::vec4 ClipRect;
};
struct TextVertex
{
    glm::vec3 Position;
    glm::vec4 Color;
    glm::vec2 TexCoord;
    int32_t EntityID;
    glm::vec4 ClipRect;
};
static_assert(sizeof(CircleVertex) == 68 && offsetof(CircleVertex, EntityID) == 48 &&
    offsetof(CircleVertex, ClipRect) == 52, "Renderer2D circle ABI");
static_assert(sizeof(LineVertex) == 80 && offsetof(LineVertex, EntityID) == 60 &&
    offsetof(LineVertex, ClipRect) == 64, "Renderer2D line ABI");
static_assert(sizeof(TextVertex) == 56 && offsetof(TextVertex, EntityID) == 36 &&
    offsetof(TextVertex, ClipRect) == 40, "Renderer2D text ABI");
}
