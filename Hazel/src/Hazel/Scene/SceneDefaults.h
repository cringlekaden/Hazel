#pragma once
#include "Hazel/Renderer/Cameras/CameraExposure.h"
#include <glm/glm.hpp>
#include "Hazel/Renderer/Scene/DirectionalShadows.h"
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
    struct AmbientOcclusionSettings {
        bool Enabled = false;
        unsigned Quality = 2; // low, medium, high, ultra
        float Radius = .5f, Strength = 1, Falloff = .615f;
        bool operator==(const AmbientOcclusionSettings& b) const {
            return Enabled==b.Enabled && Quality==b.Quality && Radius==b.Radius && Strength==b.Strength && Falloff==b.Falloff;
        }
        void Validate() const;
    };
    struct ScreenReflectionSettings {
        bool Enabled = false;
        unsigned Quality = 1; // Maximum screen-space stride: 6, 4 or 2 pixels.
        unsigned Steps = 48;
        float Distance = 20, Thickness = .15f;
        bool operator==(const ScreenReflectionSettings& b) const {
            return Enabled==b.Enabled && Quality==b.Quality && Steps==b.Steps &&
                Distance==b.Distance && Thickness==b.Thickness;
        }
        void Validate() const;
    };
    struct BloomSettings {
        bool Enabled = false;
        float Intensity = .08f, Radius = .5f;
        bool operator==(const BloomSettings& b) const {
            return Enabled == b.Enabled && Intensity == b.Intensity && Radius == b.Radius;
        }
        void Validate() const;
    };
    // Authored linear Rec.709 RGB. Display-referred bypasses exposure/tone;
    // scene-referred applies fixed Neutral presentation before independent UI.
    struct ScenePresentation {
        bool SceneReferred = false;
        bool TemporalAA = false; // Metal 4, scene-referred rigid meshes.
        CameraExposure Exposure;
        glm::vec4 Background{0.010022826f, 0.010022826f, 0.010022826f, 1};
        SceneEnvironment Environment;
        AmbientOcclusionSettings AmbientOcclusion;
        DirectionalShadowSettings Shadows;
        ScreenReflectionSettings ScreenReflections;
        BloomSettings Bloom;
        bool SameTemporalInputs(const ScenePresentation& b) const;
        bool operator==(const ScenePresentation &b) const
        {
            return SameTemporalInputs(b) && Exposure == b.Exposure &&
                Bloom == b.Bloom;
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
    YAML::Node EncodeExposure(const CameraExposure& value);
    CameraExposure ReadExposure(const YAML::Node& node);
    YAML::Node Encode(const ScenePresentation &value);
    ScenePresentation ReadPresentation(const YAML::Node &node);
    YAML::Node Encode(const NewSceneDefaults &value);
    NewSceneDefaults Read(const YAML::Node &node);
    // CPU-only document factory shared by New Project and File > New Scene.
    std::string CreateDocument(const std::string &name, const NewSceneDefaults &defaults);
    std::string SuggestIdentifier(const std::string &name);
    }
}
