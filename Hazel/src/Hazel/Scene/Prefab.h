#pragma once
#include "Entity.h"
#include "Scene.h"
#include <filesystem>
namespace Hazel
{
// One detached authored entity. Scenes/instances own values; assets own no runtime state.
class Prefab
{
  public:
	static Ref<Scene> Load(const std::filesystem::path &assetRoot, const std::filesystem::path &reference);
	static void Save(const std::filesystem::path &assetRoot, const std::filesystem::path &reference,
					 const Ref<Scene> &scene, Entity entity);
	static Entity Instantiate(const std::filesystem::path &assetRoot, const std::filesystem::path &reference,
							  Scene &target, const TransformComponent &transform,
							  bool replaceRotationAndScale = true);
	static std::filesystem::path Resolve(const std::filesystem::path &root,
										 const std::filesystem::path &reference);
	static Entity GetEntity(const Ref<Scene> &scene);
};
} // namespace Hazel
