#pragma once

#include "Scene.h"

namespace Hazel {

	class SceneSerializer
	{
	public:
		SceneSerializer(const Ref<Scene>& scene);
		SceneSerializer(const Ref<Scene>& scene, const std::filesystem::path& assetRoot);

		void Serialize(const std::string& filepath);

		bool Deserialize(const std::string& filepath);
	private:
		Ref<Scene> m_Scene;
		std::filesystem::path m_AssetRoot;
	};

}
