// Target Project API with release-safe active-project checks.
#pragma once

#include <string>
#include <filesystem>
#include <stdexcept>
#include <vector>

#include "Hazel/Core/Base.h"

namespace Hazel {
    class Scene;

	struct ProjectConfig
	{
		std::string Name = "Untitled";

		std::filesystem::path StartScene;

		std::filesystem::path AssetDirectory;
		std::filesystem::path ScriptModulePath;
	};

	class Project
	{
	public:
		static const std::filesystem::path& GetProjectDirectory()
		{
			if (!s_ActiveProject) throw std::logic_error("No active Hazel project");
			return s_ActiveProject->m_ProjectDirectory;
		}

		static std::filesystem::path GetAssetDirectory()
		{
			if (!s_ActiveProject) throw std::logic_error("No active Hazel project");
			return GetProjectDirectory() / s_ActiveProject->m_Config.AssetDirectory;
		}

		// TODO(Yan): move to asset manager when we have one
		static std::filesystem::path GetAssetFileSystemPath(const std::filesystem::path& path)
		{
			if (!s_ActiveProject) throw std::logic_error("No active Hazel project");
			return ResolveAssetPath(GetAssetDirectory(), path);
		}

		static std::filesystem::path NormalizeAssetPath(const std::filesystem::path& path);
		static std::filesystem::path ResolveAssetPath(const std::filesystem::path& assetRoot, const std::filesystem::path& reference);
		static std::filesystem::path MakeAssetReference(const std::filesystem::path& assetRoot, const std::filesystem::path& loadedPath);
		std::filesystem::path GetAssetRoot() const { return m_ProjectDirectory / m_Config.AssetDirectory; }

		Ref<Scene> LoadScene(const std::filesystem::path& assetReference) const;

        ProjectConfig& GetConfig() { return m_Config; }

		static Ref<Project> GetActive() { return s_ActiveProject; }

        // Sorted root-level descriptors only; hosts decide how to select/open them.
        static std::vector<std::filesystem::path> Discover(const std::filesystem::path& directory);
		static Ref<Project> New();
		static Ref<Project> Load(const std::filesystem::path& path);
		static Ref<Project> LoadCandidate(const std::filesystem::path& path);
		static void SetActive(const Ref<Project>& project) { s_ActiveProject = project; }
		static bool SaveActive(const std::filesystem::path& path);
	private:
		ProjectConfig m_Config;
		std::filesystem::path m_ProjectDirectory;

		inline static Ref<Project> s_ActiveProject;
	};

}
