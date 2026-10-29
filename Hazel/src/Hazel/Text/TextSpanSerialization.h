#pragma once
#include "TextSource.h"
#include <yaml-cpp/yaml.h>
namespace Hazel::Text
{
std::vector<Span> ReadSpans(const YAML::Node& node,std::string_view source);
void WriteSpans(YAML::Emitter& out,std::string_view source,const std::vector<Span>& spans);
}
