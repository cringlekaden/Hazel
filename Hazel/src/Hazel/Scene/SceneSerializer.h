#pragma once

#include "Scene.h"
#include "Entity.h"
#include "Hazel/Core/DocumentLoadReport.h"

namespace Hazel {

	enum class ResourceLoading { Resolve, MetadataOnly };
	class SceneSerializer
	{
	public:
		SceneSerializer(const Ref<Scene>& scene);
		SceneSerializer(const Ref<Scene>& scene, const std::filesystem::path& assetRoot, bool repair = false, const Ref<ProjectAssets>& assets = {});

		void Serialize(const std::string& filepath);
        std::string SerializeText(Entity only = {});
        // For editor dirty comparisons: authored spelling, no path canonicalization
        // or filesystem probes. File serialization keeps its existing portable contract.
        std::string SerializeAuthoredSnapshot(Entity only = {});
        bool DeserializeText(const std::string& text, bool prefabDocument = false, ResourceLoading resources = ResourceLoading::Resolve);

		bool Deserialize(const std::string& filepath);
        const DocumentLoadReport& Report() const { return m_Report; }
	private:
        std::string SerializeTextImpl(Entity only, bool portablePaths);
		Ref<Scene> m_Scene;
		std::filesystem::path m_AssetRoot;
		bool m_Repair = false;
        DocumentLoadReport m_Report;
	};

}
