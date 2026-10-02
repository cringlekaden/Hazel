// Actual upstream project serializer; native UTF-8 I/O and transactional errors.
#include "hzpch.h"
#include "ProjectSerializer.h"
#include "Hazel/Core/FileSystem.h"

#include <fstream>
#include <stdexcept>
#include <yaml-cpp/yaml.h>

namespace Hazel {

	ProjectSerializer::ProjectSerializer(Ref<Project> project)
		: m_Project(project)
	{
	}

	bool ProjectSerializer::Serialize(const std::filesystem::path& filepath)
	{
		if (!m_Project) return false;
		const auto& config = m_Project->GetConfig();

		YAML::Emitter out;
		{
			out << YAML::BeginMap; // Root
			out << YAML::Key << "Project" << YAML::Value;
			{
				out << YAML::BeginMap;// Project
				out << YAML::Key << "Name" << YAML::Value << config.Name;
				out << YAML::Key << "StartScene" << YAML::Value << config.StartScene.generic_u8string();
				out << YAML::Key << "AssetDirectory" << YAML::Value << config.AssetDirectory.generic_u8string();
				out << YAML::Key << "ScriptModulePath" << YAML::Value << config.ScriptModulePath.generic_u8string();
				out << YAML::EndMap; // Project
			}
			out << YAML::EndMap; // Root
		}

		if (!out.good()) return false;
		try { FileSystem::WriteFileAtomically(filepath, [&](std::ostream& stream) { stream << out.c_str(); }); }
		catch (const std::runtime_error& error) { HZ_CORE_ERROR("Project save '{}': {}", filepath.generic_u8string(), error.what()); return false; }
		return true;
	}

	bool ProjectSerializer::Deserialize(const std::filesystem::path& filepath)
	{
		if (!m_Project) return false;
		try
		{
			std::ifstream input(filepath);
			if (!input) return false;
			auto data = YAML::Load(input);
			if (input.bad()) { HZ_CORE_ERROR("Cannot read project '{}'", filepath.generic_u8string()); return false; }
			auto projectNode = data["Project"];
			if (!projectNode) return false;
			// Commit only a complete parse; preserve the active config on failure.
			ProjectConfig config;
			config.Name = projectNode["Name"].as<std::string>();
			config.StartScene = Project::NormalizeAssetPath(std::filesystem::u8path(projectNode["StartScene"].as<std::string>()));
			config.AssetDirectory = Project::NormalizeAssetPath(std::filesystem::u8path(projectNode["AssetDirectory"].as<std::string>()));
			config.ScriptModulePath = Project::NormalizeAssetPath(std::filesystem::u8path(projectNode["ScriptModulePath"].as<std::string>()));
			m_Project->GetConfig() = std::move(config);
			return true;
		}
		catch (const YAML::Exception& error)
		{
			HZ_CORE_ERROR("Failed to load project '{}': {}", filepath.generic_u8string(), error.what());
			return false;
		}
	}

}
