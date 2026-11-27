#include "hzpch.h"
#include "SceneDefaults.h"
#include "SceneSerializer.h"
#include "Hazel/Core/DocumentSchema.h"
#include "Hazel/Project/ScriptSource.h"
#include "Hazel/Assets/AssetData.h"
#include <cmath>
#include <type_traits>
namespace Hazel {
void SceneEnvironment::Validate() const {
    AssetPath(Asset,".henv",true);
    if(!std::isfinite(Yaw)||std::abs(Yaw)>6.283185307179586f || !std::isfinite(Gain)||Gain<0||Gain>1000)
        throw std::runtime_error("Environment yaw requires [-2pi,2pi] radians; gain requires [0,1000]");
    if(Asset.empty() && !(*this==SceneEnvironment{}))
        throw std::runtime_error("Environment instance values require an environment asset");
}
void AmbientOcclusionSettings::Validate() const {
    if(Quality>3 || !std::isfinite(Radius) || Radius<.001f || Radius>100 ||
       !std::isfinite(Strength) || Strength<0 || Strength>1 ||
       !std::isfinite(Falloff) || Falloff<.05f || Falloff>1)
        throw std::runtime_error("AO requires quality 0..3, radius .001..100, strength 0..1 and falloff .05..1");
}
void ScreenReflectionSettings::Validate() const
{
    if (Quality > 2 || Steps < 8 || Steps > 96 || !std::isfinite(Distance) ||
        Distance < .1f || Distance > 100 || !std::isfinite(Thickness) ||
        Thickness < .005f || Thickness > 1)
        throw std::runtime_error("Invalid screen-space reflection settings");
}
void BloomSettings::Validate() const
{
    if (!std::isfinite(Intensity) || Intensity < 0 || Intensity > .25f ||
        !std::isfinite(Radius) || Radius < 0 || Radius > 1)
        throw std::runtime_error("Bloom requires intensity 0..0.25 and radius 0..1");
}
bool ScenePresentation::SameTemporalInputs(const ScenePresentation& b) const
{
    if (SceneReferred != b.SceneReferred || TemporalAA != b.TemporalAA)
        return false;
    for (int i = 0; i < 4; ++i)
        if (std::memcmp(&Background[i], &b.Background[i], sizeof(float)))
            return false;
    return Environment == b.Environment && AmbientOcclusion == b.AmbientOcclusion &&
        Shadows == b.Shadows && ScreenReflections == b.ScreenReflections;
}
void ScenePresentation::Validate() const {
    AmbientOcclusion.Validate();
    ScreenReflections.Validate();
    Bloom.Validate();
    if (Bloom.Enabled && !SceneReferred)
        throw std::runtime_error("Bloom requires scene-referred HDR presentation");
    if (ScreenReflections.Enabled && !SceneReferred)
        throw std::runtime_error("Screen-space reflections require scene-referred HDR presentation");
    Shadows.Validate();
    Environment.Validate();
    if(TemporalAA && !SceneReferred)throw std::runtime_error("TAA requires scene-referred HDR presentation");
    if(!Environment.Asset.empty() && !SceneReferred)
        throw std::runtime_error("Prepared image environments require scene-referred presentation; unlit 2D remains independent");
    Exposure.Validate();
    if (!SceneReferred && !(Exposure == CameraExposure{}))
        throw std::runtime_error("Camera exposure requires scene-referred presentation");
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
    d.Presentation.Exposure.Mode = ExposureMode::Automatic;
    d.Presentation.Environment.Asset="Native/Environment_v1/Neutral.henv";
    return d;
}
YAML::Node EncodeExposure(const CameraExposure& value)
{
    value.Validate();
    const char* metering[] = {"Average", "CenterWeighted", "Spot"};
    YAML::Node n;
    n["Version"] = 1;
    n["Mode"] = value.Mode == ExposureMode::Automatic ? "Automatic" : "ManualPhysical";
    n["CompensationStops"] = value.CompensationStops;
    // Retain explicitly tuned metering settings through a manual shot.
    if (value.Mode == ExposureMode::Automatic ||
        !(value == CameraExposure{ExposureMode::ManualPhysical, value.CompensationStops})) {
        n["Metering"] = metering[unsigned(value.Metering)];
        n["LowPercentile"] = value.LowPercentile;
        n["HighPercentile"] = value.HighPercentile;
        n["SpotCenter"] = std::vector<float>{value.SpotCenter.x, value.SpotCenter.y};
        n["SpotCenter"].SetStyle(YAML::EmitterStyle::Flow);
        n["SpotRadius"] = value.SpotRadius;
        n["IncludeSky"] = value.IncludeSky;
        n["MinGainStops"] = value.MinGainStops;
        n["MaxGainStops"] = value.MaxGainStops;
        n["BrighteningRate"] = value.BrighteningRate;
        n["DarkeningRate"] = value.DarkeningRate;
        n["Locked"] = value.Locked;
    }
    return n;
}
CameraExposure ReadExposure(const YAML::Node& n)
{
    DocumentSchema::Keys(n, {"Version", "Mode", "CompensationStops", "Metering", "LowPercentile",
        "HighPercentile", "SpotCenter", "SpotRadius", "IncludeSky", "MinGainStops", "MaxGainStops",
        "BrighteningRate", "DarkeningRate", "Locked"}, "CameraExposure");
    if (n["Version"].as<unsigned>() != 1)
        throw std::runtime_error("Unsupported camera exposure version");
    CameraExposure value;
    const auto mode = n["Mode"].as<std::string>();
    if (mode == "Automatic") value.Mode = ExposureMode::Automatic;
    else if (mode != "ManualPhysical") throw std::runtime_error("Unknown camera exposure mode");
    value.CompensationStops = n["CompensationStops"].as<float>();
    if (const auto metering = n["Metering"]) {
        const auto name = metering.as<std::string>();
        if (name == "Average") value.Metering = ExposureMetering::Average;
        else if (name == "Spot") value.Metering = ExposureMetering::Spot;
        else if (name != "CenterWeighted") throw std::runtime_error("Unknown camera exposure metering");
    }
    if (const auto center = n["SpotCenter"]) {
        if (!center.IsSequence() || center.size() != 2)
            throw std::runtime_error("Camera exposure SpotCenter requires two normalized coordinates");
        value.SpotCenter = {center[0].as<float>(), center[1].as<float>()};
    }
    const auto readOptional = [&](const char* key, auto& field) {
        if (const auto property = n[key])
            field = property.as<std::decay_t<decltype(field)>>();
    };
    readOptional("LowPercentile", value.LowPercentile);
    readOptional("HighPercentile", value.HighPercentile);
    readOptional("SpotRadius", value.SpotRadius);
    readOptional("IncludeSky", value.IncludeSky);
    readOptional("MinGainStops", value.MinGainStops);
    readOptional("MaxGainStops", value.MaxGainStops);
    readOptional("BrighteningRate", value.BrighteningRate);
    readOptional("DarkeningRate", value.DarkeningRate);
    readOptional("Locked", value.Locked);
    value.Validate();
    return value;
}
YAML::Node Encode(const ScenePresentation& value) {
    value.Validate();
    YAML::Node n;
    n["Mode"] = value.SceneReferred ? "SceneReferred" : "DisplayReferred";
    if (value.SceneReferred)
        n["CameraExposure"] = EncodeExposure(value.Exposure);
    if(value.TemporalAA)n["TemporalAA"]=true;
    n["Background"]=std::vector<float>{value.Background.r,value.Background.g,value.Background.b,value.Background.a};
    n["Background"].SetStyle(YAML::EmitterStyle::Flow);
    if(!value.Environment.Asset.empty()) {
        auto e=n["Environment"];e["Asset"]=value.Environment.Asset.generic_u8string();
        e["Yaw"]=value.Environment.Yaw;e["Gain"]=value.Environment.Gain;
        e["Lighting"]=value.Environment.Lighting;e["Background"]=value.Environment.Background;
    }
    if (!(value.AmbientOcclusion==AmbientOcclusionSettings{})) {
        auto a=n["AmbientOcclusion"];const auto& v=value.AmbientOcclusion;
        a["Enabled"]=v.Enabled;a["Quality"]=v.Quality;a["Radius"]=v.Radius;
        a["Strength"]=v.Strength;a["Falloff"]=v.Falloff;
    }
    if (!(value.ScreenReflections==ScreenReflectionSettings{})) {
        auto nSSR=n["ScreenReflections"];
        const auto& ssr=value.ScreenReflections;
        nSSR["Enabled"]=ssr.Enabled; nSSR["Quality"]=ssr.Quality; nSSR["Steps"]=ssr.Steps;
        nSSR["Distance"]=ssr.Distance; nSSR["Thickness"]=ssr.Thickness;
    }
    if (!(value.Bloom == BloomSettings{})) {
        auto bloom = n["Bloom"];
        bloom["Enabled"] = value.Bloom.Enabled;
        bloom["Intensity"] = value.Bloom.Intensity;
        bloom["Radius"] = value.Bloom.Radius;
    }
    if (!(value.Shadows==DirectionalShadowSettings{})) {
        auto a=n["Shadows"];const auto& v=value.Shadows;
        a["Enabled"]=v.Enabled;a["Resolution"]=v.Resolution;a["Distance"]=v.Distance;
        a["SplitLambda"]=v.SplitLambda;a["NormalBias"]=v.NormalBias;a["DepthBias"]=v.DepthBias;a["Blend"]=v.Blend;
    }
    return n;
}
ScenePresentation ReadPresentation(const YAML::Node& n) {
    DocumentSchema::Keys(n, {"Mode", "Background", "Exposure", "CameraExposure", "Environment", "AmbientOcclusion", "TemporalAA", "Shadows", "ScreenReflections", "Bloom"}, "Presentation");
    const auto mode = n["Mode"].as<std::string>();
    if (mode != "DisplayReferred" && mode != "SceneReferred")
        throw std::runtime_error("Unknown world presentation");
    const auto camera = n["CameraExposure"];
    const auto legacy = n["Exposure"];
    if (mode == "DisplayReferred" && (camera || legacy))
        throw std::runtime_error("Display-referred presentation has no camera exposure");
    if (camera && legacy)
        throw std::runtime_error("Presentation cannot contain both CameraExposure and the legacy Exposure field");
    auto c=n["Background"]; if(!c.IsSequence() || c.size()!=4)throw std::runtime_error("Background requires four linear RGBA values");
    ScenePresentation result;
    result.SceneReferred = mode == "SceneReferred";
    if(n["TemporalAA"])result.TemporalAA=n["TemporalAA"].as<bool>();
    if (camera)
        result.Exposure = ReadExposure(camera);
    else if (result.SceneReferred)
        result.Exposure.CompensationStops = legacy.as<float>();
    for (int i = 0; i < 4; ++i)
        result.Background[i] = c[i].as<float>();
    if(auto e=n["Environment"]) {
        DocumentSchema::Keys(e,{"Asset","Yaw","Gain","Lighting","Background"},"scene environment");
        result.Environment.Asset=std::filesystem::u8path(e["Asset"].as<std::string>());
        if(result.Environment.Asset.empty())throw std::runtime_error("Omit absent scene environment instead of an empty reference");
        result.Environment.Yaw=e["Yaw"].as<float>();result.Environment.Gain=e["Gain"].as<float>();
        result.Environment.Lighting=e["Lighting"].as<bool>();result.Environment.Background=e["Background"].as<bool>();
    }
    if(auto a=n["AmbientOcclusion"]) {
        DocumentSchema::Keys(a,{"Enabled","Quality","Radius","Strength","Falloff"},"ambient occlusion");
        auto& v=result.AmbientOcclusion;v.Enabled=a["Enabled"].as<bool>();v.Quality=a["Quality"].as<unsigned>();
        v.Radius=a["Radius"].as<float>();v.Strength=a["Strength"].as<float>();v.Falloff=a["Falloff"].as<float>();
    }
    if(auto a=n["ScreenReflections"]) {
        DocumentSchema::Keys(a,{"Enabled","Quality","Steps","Distance","Thickness"},"screen-space reflections");
        auto& ssr=result.ScreenReflections;
        ssr.Enabled=a["Enabled"].as<bool>(); ssr.Quality=a["Quality"].as<unsigned>();
        ssr.Steps=a["Steps"].as<unsigned>(); ssr.Distance=a["Distance"].as<float>();
        ssr.Thickness=a["Thickness"].as<float>();
    }
    if (auto bloom = n["Bloom"]) {
        DocumentSchema::Keys(bloom, {"Enabled", "Intensity", "Radius"}, "bloom");
        result.Bloom.Enabled = bloom["Enabled"].as<bool>();
        result.Bloom.Intensity = bloom["Intensity"].as<float>();
        result.Bloom.Radius = bloom["Radius"].as<float>();
    }
    if(auto a=n["Shadows"]) {
        DocumentSchema::Keys(a,{"Enabled","Resolution","Distance","SplitLambda","NormalBias","DepthBias","Blend"},"directional shadows");
        auto& v=result.Shadows;v.Enabled=a["Enabled"].as<bool>();v.Resolution=a["Resolution"].as<unsigned>();
        v.Distance=a["Distance"].as<float>();v.SplitLambda=a["SplitLambda"].as<float>();
        v.NormalBias=a["NormalBias"].as<float>();v.DepthBias=a["DepthBias"].as<float>();v.Blend=a["Blend"].as<float>();
    }
    result.Validate();
    return result;
}
YAML::Node Encode(const NewSceneDefaults& value) {
    if (value.DirectStarter && !value.Presentation.SceneReferred)
        throw std::runtime_error("Native PBR starter content requires scene-referred presentation; "
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
        throw std::runtime_error("Native PBR starter requires scene-referred presentation");
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
