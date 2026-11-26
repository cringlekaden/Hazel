#pragma once

#include <filesystem>
#include <glm/glm.hpp>

namespace YAML { class Node; class Emitter; }

namespace Hazel
{
inline constexpr unsigned MaxReflectionProbes = 2;
inline constexpr unsigned MaxReflectionProbeSide = 256;
struct PreparedEnvironment;
void ValidateReflectionProbeEnvironment(const PreparedEnvironment &environment);

struct ReflectionProbe
{
    std::filesystem::path Environment;
    glm::vec3 InfluenceHalfSize{4};
    glm::vec3 ProjectionOffset{0}, ProjectionHalfSize{4};
    float BlendDistance = 1;
    bool Enabled = false;

    void Validate() const;
    void ValidatePosition(const glm::vec3 &position) const;
};

// Shared by strict scene schema, deserialization and package dependency audit.
ReflectionProbe ReadReflectionProbe(const YAML::Node &node);
void WriteReflectionProbe(YAML::Emitter &out, const ReflectionProbe &probe);
}
