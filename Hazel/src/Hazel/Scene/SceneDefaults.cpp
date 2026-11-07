#include "hzpch.h"
#include "SceneDefaults.h"
#include "SceneSerializer.h"
#include "Hazel/Core/DocumentSchema.h"
#include "Hazel/Project/ScriptSource.h"
#include "Hazel/Assets/AssetData.h"
#include <cmath>
namespace Hazel {
void SceneEnvironment::Validate() const {
    AssetPath(Asset,".henv",true);
    if(!std::isfinite(Yaw)||std::abs(Yaw)>6.283185307179586f || !std::isfinite(Gain)||Gain<0||Gain>1000)
        throw std::runtime_error("Environment yaw requires [-2pi,2pi] radians; gain requires [0,1000]");
    if(Asset.empty() && !(*this==SceneEnvironment{}))
        throw std::runtime_error("Environment instance values require an environment asset");
}
void ScenePresentation::Validate() const {
    Environment.Validate();
    if(!Environment.Asset.empty() && !SceneReferred)
        throw std::runtime_error("Prepared image environments require scene-referred presentation; unlit 2D remains independent");
    if (!std::isfinite(Exposure) || Exposure < -16 || Exposure > 16 ||
        (!SceneReferred && Exposure != 0))
        throw std::runtime_error(
            "Exposure requires scene-referred presentation and [-16,16] stops");
    if(Background.a!=1)throw std::runtime_error("Scene background must be opaque; transparent scene output is unsupported");
    for(int i=0;i<4;++i) if(!std::isfinite(Background[i]) || Background[i]<0 || Background[i]>1)
        throw std::runtime_error("Scene background requires linear RGB/alpha in [0,1]");
}
namespace SceneDefaults {
NewSceneDefaults DirectDefaults()
{
    NewSceneDefaults d;
    d.DirectStarter = true;
    d.Perspective = true;
    d.Navigation = EditorNavigation::Orbit;
    d.Presentation.SceneReferred = true;
    d.Presentation.Environment.Asset="Native/Environment_v1/Neutral.henv";
    return d;
}
YAML::Node Encode(const ScenePresentation& value) {
    value.Validate();
    YAML::Node n;
    n["Mode"] = value.SceneReferred ? "SceneReferred" : "DisplayReferred";
    if (value.SceneReferred)
        n["Exposure"] = value.Exposure;
    n["Background"]=std::vector<float>{value.Background.r,value.Background.g,value.Background.b,value.Background.a};
    n["Background"].SetStyle(YAML::EmitterStyle::Flow);
    if(!value.Environment.Asset.empty()) {
        auto e=n["Environment"];e["Asset"]=value.Environment.Asset.generic_u8string();
        e["Yaw"]=value.Environment.Yaw;e["Gain"]=value.Environment.Gain;
        e["Lighting"]=value.Environment.Lighting;e["Background"]=value.Environment.Background;
    }
    return n;
}
ScenePresentation ReadPresentation(const YAML::Node& n) {
    DocumentSchema::Keys(n, {"Mode", "Background", "Exposure", "Environment"}, "Presentation");
    const auto mode = n["Mode"].as<std::string>();
    if (mode != "DisplayReferred" && mode != "SceneReferred")
        throw std::runtime_error("Unknown world presentation");
    if (mode == "DisplayReferred" && n["Exposure"])
        throw std::runtime_error("Display-referred presentation has no exposure field");
    auto c=n["Background"]; if(!c.IsSequence() || c.size()!=4)throw std::runtime_error("Background requires four linear RGBA values");
    ScenePresentation result;
    result.SceneReferred = mode == "SceneReferred";
    if (result.SceneReferred)
        result.Exposure = n["Exposure"].as<float>();
    for (int i = 0; i < 4; ++i)
        result.Background[i] = c[i].as<float>();
    if(auto e=n["Environment"]) {
        DocumentSchema::Keys(e,{"Asset","Yaw","Gain","Lighting","Background"},"scene environment");
        result.Environment.Asset=std::filesystem::u8path(e["Asset"].as<std::string>());
        if(result.Environment.Asset.empty())throw std::runtime_error("Omit absent scene environment instead of an empty reference");
        result.Environment.Yaw=e["Yaw"].as<float>();result.Environment.Gain=e["Gain"].as<float>();
        result.Environment.Lighting=e["Lighting"].as<bool>();result.Environment.Background=e["Background"].as<bool>();
    }
    result.Validate();
    return result;
}
YAML::Node Encode(const NewSceneDefaults& value) {
    if (value.DirectStarter && !value.Presentation.SceneReferred)
        throw std::runtime_error("Direct PBR starter content requires scene-referred presentation; "
                                 "projection and navigation remain independent");
    if(value.Navigation!=EditorNavigation::Planar && value.Navigation!=EditorNavigation::Orbit)throw std::runtime_error("Unsupported navigation seed");
    YAML::Node n;n["Projection"]=value.Perspective?"Perspective":"Orthographic";
    n["Navigation"]=value.Navigation==EditorNavigation::Planar?"Planar":"Orbit";
    n["Presentation"] = Encode(value.Presentation);
    if (value.DirectStarter)
        n["Content"] = "DirectPBR";
    return n;
}
NewSceneDefaults Read(const YAML::Node& n) {
    DocumentSchema::Keys(n, {"Projection", "Navigation", "Presentation", "Content"},
                         "NewSceneDefaults");
    NewSceneDefaults result;
    if (n["Content"])
    {
        if (n["Content"].as<std::string>() != "DirectPBR")
            throw std::runtime_error("Unsupported starter content");
        result.DirectStarter = true;
    }
    auto p = n["Projection"].as<std::string>(), v = n["Navigation"].as<std::string>();
    if(p!="Orthographic" && p!="Perspective")throw std::runtime_error("Unsupported new-scene projection");
    if(v!="Planar" && v!="Orbit")throw std::runtime_error("Unsupported editor navigation seed");
    result.Perspective=p=="Perspective";result.Navigation=v=="Planar"?EditorNavigation::Planar:EditorNavigation::Orbit;
    result.Presentation = ReadPresentation(n["Presentation"]);
    if (result.DirectStarter && !result.Presentation.SceneReferred)
        throw std::runtime_error("Direct PBR starter requires scene-referred presentation");
    return result;
}
std::string SuggestIdentifier(const std::string& name) {
    std::string id;for(unsigned char c:name)if((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_')id+=char(c);
    if(id.empty())id="MyGame";if(id[0]>='0'&&id[0]<='9')id="Game"+id;
    if(!ScriptSource::ValidIdentifier(id))id="Game"+id;
    if(!ScriptSource::ValidIdentifier(id))id="MyGame";return id;
}
std::string CreateDocument(const std::string& name,const NewSceneDefaults& defaults) {
    Encode(defaults);
    auto scene = CreateRef<Scene>();
    scene->SetName(name);
    scene->Presentation() = defaults.Presentation;
    auto camera=scene->CreateEntityWithUUID(1,"Camera");
    auto& c=camera.AddComponent<CameraComponent>().Camera;
    c.SetOrthographic(10,.1f,100);c.SetPerspective(glm::radians(45.f),.1f,100);
    c.SetProjectionType(defaults.Perspective?SceneCamera::ProjectionType::Perspective:SceneCamera::ProjectionType::Orthographic);
    camera.GetComponent<TransformComponent>().Translation.z=10;
    if (defaults.DirectStarter)
    {
        camera.GetComponent<TransformComponent>().Translation.z = 3;
        auto mesh = scene->CreateEntityWithUUID(2, "One metre cube");
        mesh.AddComponent<MeshRendererComponent>().Mesh = "Native/Direct_v1/UnitCube.hmesh";
        auto sun = scene->CreateEntityWithUUID(6, "Directional key");
        sun.GetComponent<TransformComponent>().Rotation = {-.45f, .6f, 0};
        sun.AddComponent<DirectionalLightComponent>().Intensity = 3;
    }
    else
    {
        auto sprite = scene->CreateEntityWithUUID(2, "One meter sprite");
        sprite.AddComponent<SpriteRendererComponent>().Color = {.21404114f, .52252155f, 1, 1};
    }
    auto text=scene->CreateEntityWithUUID(3,"Welcome");
    auto &t = text.GetComponent<TransformComponent>();
    t.Translation = defaults.DirectStarter ? glm::vec3(-1.7f, .8f, 0) : glm::vec3(-3, 1.5f, 0);
    t.Scale = defaults.DirectStarter ? glm::vec3(.18f, .18f, 1) : glm::vec3(.5f, .5f, 1);
    text.AddComponent<TextComponent>().TextString=name;
    auto canvas=scene->CreateEntityWithUUID(4,"Screen UI");canvas.AddComponent<UICanvasComponent>();
    auto hint=scene->CreateEntityWithUUID(5,"Screen UI hint");scene->Reparent(hint,canvas,TransformPolicy::KeepLocal);
    auto& rect=hint.AddComponent<UIRectComponent>();rect.Offset={24,24};rect.FixedSize={700,40};
    hint.AddComponent<TextComponent>().TextString="Screen UI — edit this text in Properties";hint.AddComponent<UITextStyleComponent>().Size=24;
    return SceneSerializer(scene).SerializeText();
}
}
}
