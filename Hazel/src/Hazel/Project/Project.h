// Target Project API with release-safe active-project checks.
#pragma once

#include <string>
#include <filesystem>
#include <stdexcept>

#include "Hazel/Core/Base.h"

namespace Hazel {

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
			return GetAssetDirectory() / path;
		}

		ProjectConfig& GetConfig() { return m_Config; }

		static Ref<Project> GetActive() { return s_ActiveProject; }

		static Ref<Project> New();
		static Ref<Project> Load(const std::filesystem::path& path);
		static bool SaveActive(const std::filesystem::path& path);
	private:
		ProjectConfig m_Config;
		std::filesystem::path m_ProjectDirectory;

		inline static Ref<Project> s_ActiveProject;
	};

}
