#include "hzpch.h"
#include "Prefab.h"
#include "Hazel/Core/FileSystem.h"
#include "Hazel/Project/Project.h"
#include "RuntimeSession.h"
#include "SceneSerializer.h"
#include <set>
#include <yaml-cpp/yaml.h>
namespace Hazel
{
std::filesystem::path Prefab::Resolve(const std::filesystem::path &root,
									  const std::filesystem::path &reference)
{
	auto relative = Project::NormalizeAssetPath(reference);
	if (relative.empty() || relative.is_absolute() || relative.extension() != ".hprefab")
		throw std::runtime_error("Select a project-relative .hprefab asset");
	auto path = std::filesystem::weakly_canonical(root / relative);
	auto base = std::filesystem::canonical(root);
	auto contained = path.lexically_relative(base);
	if (contained.empty() || *contained.begin() == "..")
		throw std::runtime_error("Prefab must remain inside project Assets (including symlink targets)");
	return path;
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
										   "TransformComponent",
										   "ScriptComponent",
										   "CameraComponent",
										   "SpriteRendererComponent",
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
					if (!std::filesystem::is_regular_file(target))
						throw std::runtime_error("Missing prefab reference in field '" +
												 field["Name"].as<std::string>() + "': " + reference);
				}
			}
		}
	auto texture = entity["SpriteRendererComponent"]["TexturePath"];
	if (texture)
	{
		auto reference = Project::NormalizeAssetPath(std::filesystem::u8path(texture.as<std::string>()));
		auto absolute = std::filesystem::weakly_canonical(root / reference);
		auto relative = absolute.lexically_relative(std::filesystem::canonical(root));
		if (reference.is_absolute() || relative.empty() || *relative.begin() == "..")
			throw std::runtime_error("Move prefab textures inside project Assets and reassign them");
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
Ref<Scene> Prefab::Load(const std::filesystem::path &root, const std::filesystem::path &reference)
{
	auto path = Resolve(root, reference);
	std::ifstream file(path, std::ios::binary);
	if (!file)
		throw std::runtime_error("Missing prefab: " + reference.generic_u8string());
	std::string text{std::istreambuf_iterator<char>(file), {}};
	if (file.bad())
		throw std::runtime_error("Cannot read prefab");
	ValidateDocument(YAML::Load(text), root);
	auto scene = CreateRef<Scene>();
	if (!SceneSerializer(scene, root).DeserializeText(text))
		throw std::runtime_error("Malformed prefab or missing resource: " + reference.generic_u8string());
	RuntimeSession::Validate(scene);
	return scene;
}
void Prefab::Save(const std::filesystem::path &root, const std::filesystem::path &reference,
				  const Ref<Scene> &scene, Entity entity)
{
	if (!entity || !entity.BelongsTo(scene.get()) || scene->IsRunning())
		throw std::runtime_error("Select an authored entity before creating a prefab");
	if (entity.HasComponent<NativeScriptComponent>())
		throw std::runtime_error("Native script factories cannot be serialized into prefabs");
	auto path = Resolve(root, reference);
	auto text = std::string("PrefabVersion: 1\n") + SceneSerializer(scene, root).SerializeText(entity);
	ValidateDocument(YAML::Load(text), root);
	auto staged = CreateRef<Scene>();
	if (!SceneSerializer(staged, root).DeserializeText(text))
		throw std::runtime_error("Prefab contains malformed authored data or missing resources");
	RuntimeSession::Validate(staged);
	std::filesystem::create_directories(path.parent_path());
	FileSystem::WriteFileAtomically(path, [&](std::ostream &out) { out << text; });
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
