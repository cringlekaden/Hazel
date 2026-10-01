// Actual upstream project serializer; native UTF-8 I/O and transactional errors.
#include "hzpch.h"
#include "ProjectSerializer.h"

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

		std::ofstream fout(filepath);
		if (!fout || !out.good()) return false;
		fout << out.c_str();
		fout.flush();
		return fout.good();
	}

	bool ProjectSerializer::Deserialize(const std::filesystem::path& filepath)
	{
		if (!m_Project) return false;
		try
		{
			std::ifstream input(filepath);
			if (!input) return false;
			auto data = YAML::Load(input);
			auto projectNode = data["Project"];
			if (!projectNode) return false;
			// Commit only a complete parse; preserve the active config on failure.
			ProjectConfig config;
			config.Name = projectNode["Name"].as<std::string>();
			config.StartScene = std::filesystem::u8path(projectNode["StartScene"].as<std::string>());
			config.AssetDirectory = std::filesystem::u8path(projectNode["AssetDirectory"].as<std::string>());
			config.ScriptModulePath = std::filesystem::u8path(projectNode["ScriptModulePath"].as<std::string>());
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
