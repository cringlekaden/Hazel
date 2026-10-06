#pragma once
#include "Hazel/Renderer/RendererPolicy.h"
#include <yaml-cpp/yaml.h>
namespace Hazel::RendererRequestsSerializer {
    RuntimeRendererRequests Read(const YAML::Node &node);
    void Write(YAML::Emitter &out, const RuntimeRendererRequests &requests);
} // namespace Hazel::RendererRequestsSerializer
