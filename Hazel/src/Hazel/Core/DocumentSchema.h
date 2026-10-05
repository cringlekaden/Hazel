#pragma once
#include <yaml-cpp/yaml.h>
#include <initializer_list>
#include <string>

namespace Hazel::DocumentSchema
{
// Reject data our serializers cannot represent before constructing a candidate.
// Also rejects duplicate keys, custom tags and excessively nested/aliased input.
void Structure(const YAML::Node& node);
void Keys(const YAML::Node& node, std::initializer_list<const char*> allowed,
          const std::string& location);
bool Scene(const YAML::Node& root, bool prefabDocument = false); // true: known legacy/default encoding
bool Project(const YAML::Node& root);
}
