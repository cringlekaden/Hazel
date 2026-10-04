#pragma once

#include "Scene.h"
#include "Entity.h"

namespace Hazel {

	class SceneSerializer
	{
	public:
		SceneSerializer(const Ref<Scene>& scene);
		SceneSerializer(const Ref<Scene>& scene, const std::filesystem::path& assetRoot, bool repair = false, const Ref<ProjectAssets>& assets = {});

		void Serialize(const std::string& filepath);
        std::string SerializeText(Entity only = {});
        bool DeserializeText(const std::string& text);

		bool Deserialize(const std::string& filepath);
	private:
		Ref<Scene> m_Scene;
		std::filesystem::path m_AssetRoot;
		bool m_Repair = false;
	};

}
