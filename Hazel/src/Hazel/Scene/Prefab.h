#pragma once
#include "Entity.h"
#include "Scene.h"
#include "SceneSerializer.h"
#include "Hazel/Core/FileSystem.h"
#include <filesystem>
namespace Hazel
{
// One detached authored subtree. Instances own values and remapped identities; no linked overrides.
class Prefab
{
  public:
	static Ref<Scene> Load(const std::filesystem::path &assetRoot, const std::filesystem::path &reference, bool repair = false, ResourceLoading resources = ResourceLoading::Resolve);
	static void Save(const std::filesystem::path &assetRoot, const std::filesystem::path &reference,
					 const Ref<Scene> &scene, Entity entity, bool allowBroken = false, WriteMode mode = WriteMode::Replace);
    static std::string Serialize(const std::filesystem::path& root, const Ref<Scene>& scene, Entity entity, bool allowBroken = false);
	static Entity Instantiate(const std::filesystem::path &assetRoot, const std::filesystem::path &reference,
							  Scene &target);
	static Entity Instantiate(const std::filesystem::path &assetRoot, const std::filesystem::path &reference,
							  Scene &target, const TransformComponent &transform,
							  bool replaceRotationAndScale = true);
	static std::filesystem::path Resolve(const std::filesystem::path &root,
										 const std::filesystem::path &reference);
	static Entity GetEntity(const Ref<Scene> &scene); // Explicit sole root, including legacy single-entity assets.
    static void Validate(const std::filesystem::path& root, const Ref<Scene>& scene, bool requireResources = true);
};
} // namespace Hazel
