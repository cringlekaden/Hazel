#pragma once
#include <glm/glm.hpp>
#include <string>
#include <cstring>
#include <filesystem>
#include <yaml-cpp/yaml.h>
namespace Hazel {
    enum class EditorNavigation { Planar, Orbit };
    struct SceneEnvironment {
        std::filesystem::path Asset;
        float Yaw=0, Gain=1; // radians; linear radiance multiplier, independent of exposure
        bool Lighting=true, Background=true; // linked source, explicit uses
        bool operator==(const SceneEnvironment &b) const {
            return Asset==b.Asset && std::memcmp(&Yaw,&b.Yaw,4)==0 &&
                std::memcmp(&Gain,&b.Gain,4)==0 && Lighting==b.Lighting && Background==b.Background;
        }
        void Validate() const;
    };
    // Authored linear Rec.709 RGB. Display-referred bypasses exposure/tone;
    // scene-referred applies fixed Neutral presentation before independent UI.
    struct ScenePresentation {
        bool SceneReferred = false;
        float Exposure = 0; // stops; fixed Khronos PBR Neutral presentation
        glm::vec4 Background{0.010022826f, 0.010022826f, 0.010022826f, 1};
        SceneEnvironment Environment;
        bool operator==(const ScenePresentation &b) const
        {
            if (SceneReferred != b.SceneReferred ||
                std::memcmp(&Exposure, &b.Exposure, sizeof(float)))
                return false;
            for (int i = 0; i < 4; ++i)
                if (std::memcmp(&Background[i], &b.Background[i], sizeof(float)))
                    return false;
            return Environment==b.Environment;
        }
        bool operator!=(const ScenePresentation& b) const { return !(*this==b); }
        void Validate() const;
    };
    struct NewSceneDefaults {
        bool DirectStarter = false; // Content seed, not a rendering restriction.
        bool Perspective = false;
        EditorNavigation Navigation = EditorNavigation::Planar;
        ScenePresentation Presentation;
    };
    namespace SceneDefaults {
    NewSceneDefaults DirectDefaults();
    YAML::Node Encode(const ScenePresentation &value);
    ScenePresentation ReadPresentation(const YAML::Node &node);
    YAML::Node Encode(const NewSceneDefaults &value);
    NewSceneDefaults Read(const YAML::Node &node);
    // CPU-only document factory shared by New Project and File > New Scene.
    std::string CreateDocument(const std::string &name, const NewSceneDefaults &defaults);
    std::string SuggestIdentifier(const std::string &name);
    }
}
