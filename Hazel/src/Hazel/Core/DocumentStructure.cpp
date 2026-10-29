#include "hzpch.h"
#include "DocumentSchema.h"
#include <set>
namespace Hazel::DocumentSchema {
namespace {
void Walk(const YAML::Node& node, unsigned depth, size_t& remaining)
{
    if (depth > 64 || !remaining--)
        throw std::runtime_error("Document nesting/size exceeds supported limits");
    const auto tag = node.Tag();
    if (!tag.empty() && tag != "?" && tag != "!" && tag != "tag:yaml.org,2002:str" && tag != "tag:yaml.org,2002:int" &&
        tag != "tag:yaml.org,2002:float" && tag != "tag:yaml.org,2002:bool" && tag != "tag:yaml.org,2002:null" &&
        tag != "tag:yaml.org,2002:map" && tag != "tag:yaml.org,2002:seq")
        throw std::runtime_error("Unsupported YAML tag: " + tag);
    if (node.IsMap())
    {
        std::set<std::string> seen;
        for (auto pair : node)
        {
            const auto key = pair.first.as<std::string>();
            if (!seen.insert(key).second)
                throw std::runtime_error("Duplicate document key: " + key);
            Walk(pair.second, depth + 1, remaining);
        }
    }
    else if (node.IsSequence())
        for (auto child : node) Walk(child, depth + 1, remaining);
}
}
void Structure(const YAML::Node& node)
{
    size_t remaining = 1000000;
    Walk(node, 0, remaining);
}
void Keys(const YAML::Node& node, std::initializer_list<const char*> allowed,
          const std::string& location)
{
    if (!node.IsMap()) throw std::runtime_error(location + " must be a mapping");
    for (auto pair : node)
    {
        const auto key = pair.first.as<std::string>();
        if (std::none_of(allowed.begin(), allowed.end(), [&](const char* name) { return key == name; }))
            throw std::runtime_error("Unsupported field " + location + "." + key +
                                     "; original data must be opened with a compatible editor");
    }
}
}
