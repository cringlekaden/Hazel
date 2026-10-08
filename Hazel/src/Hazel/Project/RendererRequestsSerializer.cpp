#include "hzpch.h"
#include "RendererRequestsSerializer.h"
#include "Hazel/Core/DocumentSchema.h"
namespace Hazel::RendererRequestsSerializer {
    RuntimeRendererRequests Read(const YAML::Node &node) {
        DocumentSchema::Structure(node);
        DocumentSchema::Keys(node, {"Version", "VSync", "TextureSlots", "ShaderLoading"},
                             "Rendering");
        if (node["Version"].as<int>() != 1)
            throw std::runtime_error(
                "Unsupported Rendering Version; use a compatible editor/runtime");
        RuntimeRendererRequests r;
        if (node["VSync"])
            r.VSync = node["VSync"].as<bool>();
        if (node["TextureSlots"])
            r.TextureSlots = node["TextureSlots"].as<uint32_t>();
        if (node["ShaderLoading"]) {
            const auto value = node["ShaderLoading"].as<std::string>();
            if (value == "Automatic")
                r.ShaderLoading = ShaderLoadingRequest::Automatic;
            else if (value == "GLSLCompatibility")
                r.ShaderLoading = ShaderLoadingRequest::GLSLCompatibility;
            else
                throw std::runtime_error("ShaderLoading must be Automatic or GLSLCompatibility");
        }
        RendererPolicy::Validate(r);
        return r;
    }
    void Write(YAML::Emitter &out, const RuntimeRendererRequests &r) {
        RendererPolicy::Validate(r);
        out << YAML::Key << "Rendering" << YAML::Value << YAML::BeginMap << YAML::Key << "Version"
            << YAML::Value << 1 << YAML::Key << "VSync" << YAML::Value << r.VSync << YAML::Key
            << "TextureSlots" << YAML::Value << r.TextureSlots << YAML::Key << "ShaderLoading"
            << YAML::Value
            << (r.ShaderLoading == ShaderLoadingRequest::Automatic ? "Automatic"
                                                                   : "GLSLCompatibility")
            << YAML::EndMap;
    }
} // namespace Hazel::RendererRequestsSerializer
