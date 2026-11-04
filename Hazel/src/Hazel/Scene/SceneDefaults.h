#pragma once
#include <glm/glm.hpp>
#include <string>
#include <cstring>
#include <yaml-cpp/yaml.h>
namespace Hazel {
    enum class EditorNavigation { Planar, Orbit };
    // Authored display-linear RGB; alpha is coverage. Scene-referred presentation
    // is deliberately unsupported until the lighting/presentation checkpoint.
    struct ScenePresentation {
        glm::vec4 Background{0.010022826f, 0.010022826f, 0.010022826f, 1};
        bool operator==(const ScenePresentation& b) const { for(int i=0;i<4;++i)if(std::memcmp(&Background[i],&b.Background[i],sizeof(float)))return false;return true; }
        bool operator!=(const ScenePresentation& b) const { return !(*this==b); }
        void Validate() const;
    };
    struct NewSceneDefaults {
        bool Perspective = false;
        EditorNavigation Navigation = EditorNavigation::Planar;
        ScenePresentation Presentation;
    };
    namespace SceneDefaults {
        YAML::Node Encode(const ScenePresentation& value);
        ScenePresentation ReadPresentation(const YAML::Node& node);
        YAML::Node Encode(const NewSceneDefaults& value);
        NewSceneDefaults Read(const YAML::Node& node);
        // CPU-only document factory shared by New Project and File > New Scene.
        std::string CreateDocument(const std::string& name, const NewSceneDefaults& defaults);
        std::string SuggestIdentifier(const std::string& name);
    }
}
