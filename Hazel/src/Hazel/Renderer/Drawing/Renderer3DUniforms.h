#pragma once
#include <glm/glm.hpp>
#include "Hazel/Renderer/Scene/Lighting.h"
#include <array>

// Shared CPU ABI for the existing GLSL std140 blocks; no backend-specific shading data.
namespace Hazel::Renderer3DUniforms
{
struct alignas(16) ViewData
{
    glm::mat4 VP;
    glm::vec4 Eye, ToCamera, LightDirection, Incident;
};
struct alignas(16) LocalLightData
{
    glm::vec4 Meta{0}; // count; remaining fields reserved for transient inspection.
    std::array<glm::vec4, MaxLocalLights * 3> Lights{};
};
static_assert(sizeof(LocalLightData) == 15376, "Local light std140 ABI");
struct alignas(16) ObjectData
{
    glm::mat4 Model, Normal;
    glm::ivec4 Identity;
};
struct alignas(16) MotionData {glm::mat4 PreviousMVP;glm::vec4 Valid;};
static_assert(sizeof(MotionData)==80,"Motion std140 ABI");
struct alignas(16) MaterialData
{
    glm::vec4 Base, Emissive, Surface, Options;
};
static_assert(sizeof(ViewData) == 128 && sizeof(ObjectData) == 144 && sizeof(MaterialData) == 64,
              "std140 direct forward layout");
struct alignas(16) EnvironmentData
{
    glm::mat4 InverseVP;
    glm::vec4 RotationGain, Options;
};
static_assert(sizeof(EnvironmentData) == 96, "std140 environment layout");
}
