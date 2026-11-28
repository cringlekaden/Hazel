#pragma once
#include <yaml-cpp/yaml.h>
#include <initializer_list>
#include <string>

namespace Hazel { struct ColorGradeSettings; struct DepthOfFieldSettings; struct LensEffectsSettings; struct MotionBlurSettings; struct PhysicalCamera; }

namespace Hazel::DocumentSchema
{
// Reject data our serializers cannot represent before constructing a candidate.
// Also rejects duplicate keys, custom tags and excessively nested/aliased input.
void Structure(const YAML::Node& node);
void Keys(const YAML::Node& node, std::initializer_list<const char*> allowed,
          const std::string& location);
bool Scene(const YAML::Node& root, bool prefabDocument = false); // true: known legacy/default encoding
bool Project(const YAML::Node& root);
PhysicalCamera ReadPhysicalLens(const YAML::Node& node);
DepthOfFieldSettings ReadDepthOfField(const YAML::Node& node);
MotionBlurSettings ReadMotionBlur(const YAML::Node& node);
LensEffectsSettings ReadLensEffects(const YAML::Node& node);
YAML::Node EncodeLensEffects(const LensEffectsSettings& settings);
ColorGradeSettings ReadColorGrade(const YAML::Node& node);
YAML::Node EncodeColorGrade(const ColorGradeSettings& settings);
}
