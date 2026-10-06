#include "hzpch.h"
#include "DocumentSchema.h"
#include "Hazel/Project/RendererRequestsSerializer.h"
#include "Hazel/Scripting/ScriptEngine.h"
#include <set>
#include <cmath>

namespace Hazel::DocumentSchema
{
namespace
{
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
void Finite(const YAML::Node& node, bool wide = false)
{
    if (node.IsSequence()) { for (auto value : node) Finite(value,wide); }
    else if (!std::isfinite(wide?node.as<double>():static_cast<double>(node.as<float>())))
        throw std::runtime_error("Non-finite authored numeric value");
}
void Vector(const YAML::Node& node,size_t size)
{
    if(!node.IsSequence() || node.size()!=size)throw std::runtime_error("Wrong authored vector size (expected "+std::to_string(size)+")");
    Finite(node);
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
bool Project(const YAML::Node& root)
{
    Structure(root);
    Keys(root, {"Project"}, "root");
    auto project = root["Project"];
    Keys(project, {"Version", "Name", "ScriptProject", "StartScene", "AssetDirectory", "ScriptModulePath", "AuthoringVersion", "Rendering"}, "Project");
    if(project["Rendering"])RendererRequestsSerializer::Read(project["Rendering"]);
    if(project["AuthoringVersion"] && project["AuthoringVersion"].as<int>()!=1)
        throw std::runtime_error("Unsupported project authoring/template contract; use a compatible editor");
    if (project["Version"] && project["Version"].as<int>() != 1)
        throw std::runtime_error("Unsupported Project Version (expected 1 or known legacy without Version)");
    for(const char* key:{"Name","StartScene","AssetDirectory","ScriptModulePath"}) {
        if(!project[key].IsScalar())throw std::runtime_error(std::string("Missing/invalid required Project.")+key);
        const auto value=project[key].as<std::string>();
        if(value.empty() || value.find('\0')!=std::string::npos)throw std::runtime_error(std::string("Empty/invalid required Project.")+key);
    }
    const auto rendering=project["Rendering"];
    return !project["Version"] || !project["ScriptProject"] ||
           (rendering && (!rendering["VSync"] || !rendering["TextureSlots"] || !rendering["ShaderLoading"]));
}
bool Scene(const YAML::Node& root, bool prefabDocument)
{
    Structure(root);
    if(prefabDocument) {
        Keys(root, {"Scene", "SceneVersion", "PrefabVersion", "Entities"}, "root");
        if(!root["PrefabVersion"])throw std::runtime_error("Missing required PrefabVersion");
    } else Keys(root, {"Scene", "SceneVersion", "Entities"}, "Scene (open prefabs through Content Browser)");
    if (root["SceneVersion"] && root["SceneVersion"].as<int>() != 1)
        throw std::runtime_error("Unsupported SceneVersion (expected 1 or known unversioned scene)");
    if (root["PrefabVersion"] && root["PrefabVersion"].as<int>() != 1)
        throw std::runtime_error("Unsupported PrefabVersion (expected 1)");
    root["Scene"].as<std::string>();
    auto entities = root["Entities"];
    if (entities && !entities.IsSequence()) throw std::runtime_error("Entities must be a sequence");
    if (root["PrefabVersion"] && (!entities || entities.size()!=1))
        throw std::runtime_error("PrefabVersion 1 requires exactly one entity");
    bool legacy = !root["SceneVersion"] || !entities;
    std::set<uint64_t> ids;
    for (auto entity : entities)
    {
        Keys(entity, {"Entity", "TagComponent", "TransformComponent", "CameraComponent", "ScriptComponent",
                      "SpriteRendererComponent", "SpriteAnimationComponent", "CircleRendererComponent",
                      "Rigidbody2DComponent", "BoxCollider2DComponent", "CircleCollider2DComponent", "TextComponent"}, "Entity");
        const auto id = entity["Entity"].as<uint64_t>();
        if (!id || !ids.insert(id).second) throw std::runtime_error("Duplicate or null entity UUID");
        if (auto n = entity["TagComponent"]) { Keys(n, {"Tag"}, "TagComponent"); n["Tag"].as<std::string>(); }
        else legacy=true;
        if (auto n = entity["TransformComponent"])
        {
            Keys(n, {"Translation", "Rotation", "Scale"}, "TransformComponent");
            for (const char* key : {"Translation", "Rotation", "Scale"}) Vector(n[key],3);
        }
        else legacy = true; // documented identity transform
        if (auto n = entity["CameraComponent"])
        {
            Keys(n, {"Camera", "Primary", "FixedAspectRatio"}, "CameraComponent");
            auto camera = n["Camera"];
            Keys(camera, {"ProjectionType", "PerspectiveFOV", "PerspectiveNear", "PerspectiveFar",
                          "OrthographicSize", "OrthographicNear", "OrthographicFar"}, "Camera");
            const auto type = camera["ProjectionType"].as<int>();
            if (type < 0 || type > 1) throw std::runtime_error("Unsupported camera ProjectionType");
            for (auto field : camera) Finite(field.second);
        }
        if (auto n = entity["ScriptComponent"])
        {
            Keys(n, {"ClassName", "ScriptFields"}, "ScriptComponent");
            auto fields = n["ScriptFields"];
            if (fields && !fields.IsSequence()) throw std::runtime_error("ScriptFields must be a sequence");
            std::set<std::string> names;
            for (auto field : fields)
            {
                Keys(field, {"Name", "Type", "Data"}, "ScriptField");
                if (!names.insert(field["Name"].as<std::string>()).second)
                    throw std::runtime_error("Duplicate stored script field name");
                const auto type=field["Type"].as<std::string>();
                if(Utils::ScriptFieldTypeFromString(type)==ScriptFieldType::None)throw std::runtime_error("Unsupported stored script field type");
                if(type=="Vector2" || type=="Vector3" || type=="Vector4")Vector(field["Data"],size_t(type.back()-'0'));
                if(type=="Float" || type=="Double" || type=="Vector2" || type=="Vector3" || type=="Vector4")Finite(field["Data"],type=="Double");
            }
        }
        if (auto n = entity["SpriteRendererComponent"])
        {
            Keys(n, {"Color", "Source", "TexturePath", "TilingFactor"}, "SpriteRendererComponent");
            Vector(n["Color"],4);
            if (!n["Source"] || (n["Source"]["Type"].as<std::string>()=="Texture" && !n["Source"]["TilingFactor"])) legacy = true;
        }
        if(auto n=entity["SpriteAnimationComponent"])
            if(!n["DefaultClip"] || !n["Autoplay"] || !n["Speed"])legacy=true;
        // Sprite Source / animation / typed references have their own strict readers.
        if (auto n = entity["CircleRendererComponent"])
        {
            Keys(n, {"Color", "Thickness", "Fade"}, "CircleRendererComponent");
            Vector(n["Color"],4);
            for (auto field : n) Finite(field.second);
        }
        if (auto n = entity["Rigidbody2DComponent"])
        {
            Keys(n, {"BodyType", "FixedRotation", "GravityScale"}, "Rigidbody2DComponent");
            if (n["GravityScale"]) Finite(n["GravityScale"]); else legacy = true;
        }
        for (const char* component : {"BoxCollider2DComponent", "CircleCollider2DComponent"})
            if (auto n = entity[component])
            {
                if (std::string(component) == "BoxCollider2DComponent")
                    Keys(n, {"Offset", "Size", "Density", "Friction", "Restitution", "RestitutionThreshold"}, component);
                else Keys(n, {"Offset", "Radius", "Density", "Friction", "Restitution", "RestitutionThreshold"}, component);
                Vector(n["Offset"],2);
                if(n["Size"])Vector(n["Size"],2);
                for (auto field : n) Finite(field.second);
            }
        if (auto n = entity["TextComponent"])
        {
            Keys(n, {"TextString", "Color", "Kerning", "LineSpacing"}, "TextComponent");
            Vector(n["Color"],4);
            for (const char* key : {"Color", "Kerning", "LineSpacing"}) Finite(n[key]);
        }
    }
    return legacy;
}
}
