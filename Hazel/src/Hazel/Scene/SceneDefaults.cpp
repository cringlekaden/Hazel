#include "hzpch.h"
#include "SceneDefaults.h"
#include "SceneSerializer.h"
#include "Hazel/Core/DocumentSchema.h"
#include "Hazel/Project/ScriptSource.h"
#include <cmath>
namespace Hazel {
void ScenePresentation::Validate() const {
    if(Background.a!=1)throw std::runtime_error("Display-referred scene background must be opaque; transparent scene output is unsupported");
    for(int i=0;i<4;++i) if(!std::isfinite(Background[i]) || Background[i]<0 || Background[i]>1)
        throw std::runtime_error("Display-referred background requires linear RGB/alpha in [0,1]");
}
namespace SceneDefaults {
YAML::Node Encode(const ScenePresentation& value) {
    value.Validate(); YAML::Node n; n["Mode"]="DisplayReferred";
    n["Background"]=std::vector<float>{value.Background.r,value.Background.g,value.Background.b,value.Background.a};
    n["Background"].SetStyle(YAML::EmitterStyle::Flow); return n;
}
ScenePresentation ReadPresentation(const YAML::Node& n) {
    DocumentSchema::Keys(n,{"Mode","Background"},"Presentation");
    if(n["Mode"].as<std::string>()!="DisplayReferred")
        throw std::runtime_error("Unsupported world presentation; this checkpoint supports DisplayReferred unlit content only");
    auto c=n["Background"]; if(!c.IsSequence() || c.size()!=4)throw std::runtime_error("Background requires four linear RGBA values");
    ScenePresentation result;for(int i=0;i<4;++i)result.Background[i]=c[i].as<float>();result.Validate();return result;
}
YAML::Node Encode(const NewSceneDefaults& value) {
    if(value.Navigation!=EditorNavigation::Planar && value.Navigation!=EditorNavigation::Orbit)throw std::runtime_error("Unsupported navigation seed");
    YAML::Node n;n["Projection"]=value.Perspective?"Perspective":"Orthographic";
    n["Navigation"]=value.Navigation==EditorNavigation::Planar?"Planar":"Orbit";
    n["Presentation"]=Encode(value.Presentation);return n;
}
NewSceneDefaults Read(const YAML::Node& n) {
    DocumentSchema::Keys(n,{"Projection","Navigation","Presentation"},"NewSceneDefaults");
    NewSceneDefaults result;auto p=n["Projection"].as<std::string>(),v=n["Navigation"].as<std::string>();
    if(p!="Orthographic" && p!="Perspective")throw std::runtime_error("Unsupported new-scene projection");
    if(v!="Planar" && v!="Orbit")throw std::runtime_error("Unsupported editor navigation seed");
    result.Perspective=p=="Perspective";result.Navigation=v=="Planar"?EditorNavigation::Planar:EditorNavigation::Orbit;
    result.Presentation=ReadPresentation(n["Presentation"]);return result;
}
std::string SuggestIdentifier(const std::string& name) {
    std::string id;for(unsigned char c:name)if((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_')id+=char(c);
    if(id.empty())id="MyGame";if(id[0]>='0'&&id[0]<='9')id="Game"+id;
    if(!ScriptSource::ValidIdentifier(id))id="Game"+id;
    if(!ScriptSource::ValidIdentifier(id))id="MyGame";return id;
}
std::string CreateDocument(const std::string& name,const NewSceneDefaults& defaults) {
    auto scene=CreateRef<Scene>();scene->SetName(name);scene->Presentation()=defaults.Presentation;
    auto camera=scene->CreateEntityWithUUID(1,"Camera");
    auto& c=camera.AddComponent<CameraComponent>().Camera;
    c.SetOrthographic(10,.1f,100);c.SetPerspective(glm::radians(45.f),.1f,100);
    c.SetProjectionType(defaults.Perspective?SceneCamera::ProjectionType::Perspective:SceneCamera::ProjectionType::Orthographic);
    camera.GetComponent<TransformComponent>().Translation.z=10;
    auto sprite=scene->CreateEntityWithUUID(2,"One meter sprite");
    sprite.AddComponent<SpriteRendererComponent>().Color={.21404114f,.52252155f,1,1};
    auto text=scene->CreateEntityWithUUID(3,"Welcome");
    auto& t=text.GetComponent<TransformComponent>();t.Translation={-3,1.5f,0};t.Scale={.5f,.5f,1};
    text.AddComponent<TextComponent>().TextString=name;
    auto canvas=scene->CreateEntityWithUUID(4,"Screen UI");canvas.AddComponent<UICanvasComponent>();
    auto hint=scene->CreateEntityWithUUID(5,"Screen UI hint");scene->Reparent(hint,canvas,TransformPolicy::KeepLocal);
    auto& rect=hint.AddComponent<UIRectComponent>();rect.Offset={24,24};rect.FixedSize={700,40};
    hint.AddComponent<TextComponent>().TextString="Screen UI — edit this text in Properties";hint.AddComponent<UITextStyleComponent>().Size=24;
    return SceneSerializer(scene).SerializeText();
}
}
}
