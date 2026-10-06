#include "hzpch.h"
#include "Prefab.h"
#include "Hazel/Core/FileSystem.h"
#include "Hazel/Core/FileDocument.h"
#include "Hazel/Project/Project.h"
#include "RuntimeSession.h"
#include "SceneSerializer.h"
#include "Hazel/Scripting/ScriptEngine.h"
#include <set>
#include <yaml-cpp/yaml.h>
namespace Hazel
{
std::filesystem::path Prefab::Resolve(const std::filesystem::path &root,
									  const std::filesystem::path &reference)
{
	auto relative = Project::NormalizeAssetPath(reference);
	if (relative.extension() != ".hprefab")
		throw std::runtime_error("Select a project-relative .hprefab asset");
	return Project::ResolveOwnedAsset(root,relative);
}
static void ValidateDocument(const YAML::Node &data, const std::filesystem::path &root)
{
	if (!data["PrefabVersion"] || data["PrefabVersion"].as<int>() != 1)
		throw std::runtime_error("Unsupported or missing PrefabVersion (expected 1)");
	if (!data["Entities"].IsSequence() || data["Entities"].size() != 1)
		throw std::runtime_error("A prefab must contain exactly one entity");
	auto entity = data["Entities"][0];
	auto id = entity["Entity"].as<uint64_t>();
	if (!id)
		throw std::runtime_error("Prefab UUID zero is reserved for null references");
	const std::set<std::string> allowed = {"Entity",
										   "TagComponent",
										   "TransformComponent", "Relationship",
										   "ScriptComponent",
										   "CameraComponent",
										   "SpriteRendererComponent",
                                           "SpriteAnimationComponent",
										   "CircleRendererComponent",
										   "Rigidbody2DComponent",
										   "BoxCollider2DComponent",
										   "CircleCollider2DComponent",
										   "TextComponent"};
	for (auto it : entity)
		if (!allowed.count(it.first.as<std::string>()))
			throw std::runtime_error("Prefab contains unsupported component: " + it.first.as<std::string>());
	auto fields = entity["ScriptComponent"]["ScriptFields"];
	if (fields)
		for (auto field : fields)
		{
			if (field["Type"].as<std::string>() == "Entity")
			{
				auto reference = field["Data"].as<uint64_t>();
				if (reference && reference != id)
					throw std::runtime_error(
						"External entity reference in field '" + field["Name"].as<std::string>() +
						"'. Clear it before creating a prefab; self references are remapped.");
			}
			if (field["Type"].as<std::string>() == "Prefab")
			{
				auto reference = field["Data"].as<std::string>();
				if (!reference.empty())
				{
					auto target = Prefab::Resolve(root, std::filesystem::u8path(reference));
                (void)target; // Existence is readiness, not authored schema validity.
				}
			}
		}
	if(auto sprite=entity["SpriteRendererComponent"])
	{
        auto source=ReadSpriteSource(sprite);
        if(auto texture=std::get_if<TextureSpriteSource>(&source))Project::ResolveOwnedAsset(root,texture->Texture);
        if(auto region=std::get_if<SpriteReference>(&source))Project::ResolveOwnedAsset(root,region->Sheet);
	}
}
Entity Prefab::GetEntity(const Ref<Scene> &scene)
{
	if (!scene)
		throw std::runtime_error("Prefab editing scene is unavailable");
	auto view = scene->GetAllEntitiesWith<IDComponent>();
	if (view.size() != 1)
		throw std::runtime_error("Prefab editing scene must contain one entity");
	return {*view.begin(), scene.get()};
}
Ref<Scene> Prefab::Load(const std::filesystem::path &root, const std::filesystem::path &reference,bool repair)
{
	auto path = Resolve(root, reference);
	const auto text=FileDocument::Read(path);
	ValidateDocument(YAML::Load(text), root);
	auto scene = CreateRef<Scene>();
	if (!SceneSerializer(scene, root,repair).DeserializeText(text,true))
		throw std::runtime_error("Malformed prefab or missing resource: " + reference.generic_u8string());
    if(!repair) {
        RuntimeSession::Validate(scene);
        for(auto& [name,field] : ScriptEngine::GetScriptFieldMap(GetEntity(scene)))
            if(field.Field.Type==ScriptFieldType::Prefab && !field.AssetReference.empty() &&
               !std::filesystem::is_regular_file(Resolve(root,std::filesystem::u8path(field.AssetReference))))
                throw std::runtime_error("Missing prefab reference: "+field.AssetReference);
    }
	return scene;
}
std::string Prefab::Serialize(const std::filesystem::path &root, const Ref<Scene> &scene, Entity entity,bool allowBroken)
{
	if (!entity || !entity.BelongsTo(scene.get()) || scene->IsRunning())
		throw std::runtime_error("Select an authored entity before creating a prefab");
	if (entity.HasComponent<NativeScriptComponent>())
		throw std::runtime_error("Native script factories cannot be serialized into prefabs");
	auto text = std::string("PrefabVersion: 1\n") + SceneSerializer(scene, root).SerializeText(entity);
	ValidateDocument(YAML::Load(text), root);
	auto staged = CreateRef<Scene>();
	if (!SceneSerializer(staged, root,allowBroken).DeserializeText(text,true))
		throw std::runtime_error("Prefab contains malformed authored data or missing resources");
	if(!allowBroken)RuntimeSession::Validate(staged);
    if(!allowBroken) for(auto& [name,field] : ScriptEngine::GetScriptFieldMap(entity))
        if(field.Field.Type==ScriptFieldType::Prefab && !field.AssetReference.empty() &&
           !std::filesystem::is_regular_file(Resolve(root,std::filesystem::u8path(field.AssetReference))))
            throw std::runtime_error("Missing prefab reference: "+field.AssetReference);
    return text;
}
void Prefab::Save(const std::filesystem::path &root, const std::filesystem::path &reference,
                  const Ref<Scene> &scene, Entity entity,bool allowBroken,WriteMode mode)
{
    const auto path=Resolve(root, reference);
    const auto text=Serialize(root,scene,entity,allowBroken);
	std::filesystem::create_directories(path.parent_path());
	FileSystem::WriteFileAtomically(path, [&](std::ostream &out) { out << text; },mode);
}
Entity Prefab::Instantiate(const std::filesystem::path &root, const std::filesystem::path &reference,
						   Scene &target)
{
	auto staged = Load(root, reference);
	auto source = GetEntity(staged);
	return target.InstantiateEntity(source, source.GetComponent<TransformComponent>());
}
Entity Prefab::Instantiate(const std::filesystem::path &root, const std::filesystem::path &reference,
						   Scene &target, const TransformComponent &transform, bool replaceRotationAndScale)
{
	auto staged = Load(root, reference); // Complete validation/resource loading before destination mutation.
	auto &initial = GetEntity(staged).GetComponent<TransformComponent>();
	if (replaceRotationAndScale)
		initial = transform;
	else
		initial.Translation = transform.Translation;
	RuntimeSession::Validate(staged);
	return target.InstantiateEntity(GetEntity(staged), initial);
}
} // namespace Hazel
