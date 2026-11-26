#include "hzpch.h"
#include "ReflectionProbe.h"
#include "Hazel/Assets/Environment.h"
#include "Hazel/Core/DocumentSchema.h"
#include <cmath>
#include <stdexcept>
#include <yaml-cpp/yaml.h>

namespace Hazel
{
void ValidateReflectionProbeEnvironment(const PreparedEnvironment &environment)
{
    if (environment.Specular.Side > MaxReflectionProbeSide || environment.Irradiance.Side > MaxReflectionProbeSide)
        throw std::runtime_error("Reflection probe cubemaps are limited to 512 pixels per face");
}

void ReflectionProbe::Validate() const
{
    AssetPath(Environment, ".henv", !Enabled);
    if (BakeResolution != 128 && BakeResolution != 256 && BakeResolution != 512)
        throw std::runtime_error("Probe bake resolution requires 128, 256 or 512 pixels per face");
    for (int axis = 0; axis < 3; ++axis)
    {
        const float size = InfluenceHalfSize[axis], extent = ProjectionHalfSize[axis];
        const float offset = std::abs(ProjectionOffset[axis]);
        if (!std::isfinite(size) || size < .01f || !std::isfinite(extent) ||
            extent > 10000 || !std::isfinite(offset) || extent < size + offset)
            throw std::runtime_error("Projection box must enclose the influence box (half sizes .01 to 10000)");
        if (!std::isfinite(BlendDistance) || BlendDistance < .001f || BlendDistance > size)
            throw std::runtime_error("Probe blend distance must be at least .001 and fit inside the influence box");
    }
}

void ReflectionProbe::ValidatePosition(const glm::vec3 &position) const
{
    for (int axis = 0; axis < 3; ++axis)
    {
        const float p = position[axis], reach = InfluenceHalfSize[axis];
        if (!std::isfinite(p) || p - reach >= p || p + reach <= p ||
            !std::isfinite(p - reach) || !std::isfinite(p + reach))
            throw std::runtime_error("Reflection probe requires a finite, representable world-space influence box");
    }
}

ReflectionProbe ReadReflectionProbe(const YAML::Node &node)
{
    DocumentSchema::Keys(node, {"Environment", "InfluenceHalfSize", "ProjectionOffset",
                               "ProjectionHalfSize", "BlendDistance", "Enabled", "BakeResolution"}, "ReflectionProbeComponent");
    auto vector = [&](const char *name) {
        const auto value = node[name];
        if (!value.IsSequence() || value.size() != 3)
            throw std::runtime_error(std::string(name) + " requires three values");
        return glm::vec3(value[0].as<float>(), value[1].as<float>(), value[2].as<float>());
    };
    ReflectionProbe probe;
    probe.Environment = std::filesystem::u8path(node["Environment"].as<std::string>());
    probe.InfluenceHalfSize = vector("InfluenceHalfSize");
    probe.ProjectionOffset = vector("ProjectionOffset");
    probe.ProjectionHalfSize = vector("ProjectionHalfSize");
    probe.BlendDistance = node["BlendDistance"].as<float>();
    probe.Enabled = node["Enabled"].as<bool>();
    if (auto size = node["BakeResolution"]) probe.BakeResolution = size.as<unsigned>();
    probe.Validate();
    return probe;
}

void WriteReflectionProbe(YAML::Emitter &out, const ReflectionProbe &probe)
{
    probe.Validate();
    auto vector = [&](const char *name, const glm::vec3 &value) {
        out << YAML::Key << name << YAML::Value << YAML::Flow << YAML::BeginSeq
            << value.x << value.y << value.z << YAML::EndSeq;
    };
    out << YAML::BeginMap << YAML::Key << "Environment" << YAML::Value << probe.Environment.generic_u8string();
    vector("InfluenceHalfSize", probe.InfluenceHalfSize);
    vector("ProjectionOffset", probe.ProjectionOffset);
    vector("ProjectionHalfSize", probe.ProjectionHalfSize);
    out << YAML::Key << "BlendDistance" << YAML::Value << probe.BlendDistance
        << YAML::Key << "Enabled" << YAML::Value << probe.Enabled;
    if (probe.BakeResolution != 256)
        out << YAML::Key << "BakeResolution" << YAML::Value << probe.BakeResolution;
    out << YAML::EndMap;
}
}
