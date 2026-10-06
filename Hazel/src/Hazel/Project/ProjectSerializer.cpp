// Actual upstream project serializer; native UTF-8 I/O and transactional errors.
#include "hzpch.h"
#include "RendererRequestsSerializer.h"
#include "ProjectSerializer.h"
#include "Hazel/Core/FileSystem.h"
#include "Hazel/Core/DocumentSchema.h"
#include "Hazel/Core/FileDocument.h"

#include <fstream>
#include <stdexcept>
#include <yaml-cpp/yaml.h>

namespace Hazel {

	ProjectSerializer::ProjectSerializer(Ref<Project> project)
		: m_Project(project)
	{
	}

	std::string ProjectSerializer::SerializeText() const
	{
		if (!m_Project) throw std::runtime_error("No project to serialize");
		const auto& config = m_Project->GetConfig();

		YAML::Emitter out;
		{
			out << YAML::BeginMap; // Root
			out << YAML::Key << "Project" << YAML::Value;
			{
				out << YAML::BeginMap;// Project
				out << YAML::Key << "Version" << YAML::Value << 1;
                if(config.Rendering)RendererRequestsSerializer::Write(out,*config.Rendering);
                out << YAML::Key << "ScriptProject" << YAML::Value << config.ScriptProject;
                if(config.AuthoringVersion)out<<YAML::Key<<"AuthoringVersion"<<YAML::Value<<config.AuthoringVersion;
                out << YAML::Key << "Name" << YAML::Value << config.Name;
				out << YAML::Key << "StartScene" << YAML::Value << config.StartScene.generic_u8string();
				out << YAML::Key << "AssetDirectory" << YAML::Value << config.AssetDirectory.generic_u8string();
				out << YAML::Key << "ScriptModulePath" << YAML::Value << config.ScriptModulePath.generic_u8string();
				out << YAML::EndMap; // Project
			}
			out << YAML::EndMap; // Root
		}

        if (!out.good()) throw std::runtime_error(out.GetLastError());
        return out.c_str();
    }
    bool ProjectSerializer::Serialize(const std::filesystem::path& filepath)
    {
		try { auto text=SerializeText(); FileSystem::WriteFileAtomically(filepath, [&](std::ostream& stream) { stream << text; }); }
		catch (const std::runtime_error& error) { HZ_CORE_ERROR("Project save '{}': {}", filepath.generic_u8string(), error.what()); return false; }
		return true;
	}

	bool ProjectSerializer::Deserialize(const std::filesystem::path& filepath)
	{
        m_Report={};
		if (!m_Project) return false;
		try
		{
			auto data = YAML::Load(FileDocument::Read(filepath));
            m_Report.Migration=DocumentSchema::Project(data);
			auto projectNode = data["Project"];
            if (!projectNode["Version"]) m_Report.Problems.push_back({0,"Encoding","Save adds Project Version: 1; original bytes are preserved first",{},true});
            if (!projectNode["ScriptProject"]) m_Report.Problems.push_back({0,"Script project","Known legacy default: project Name; Save writes ScriptProject explicitly",{},true});
			if (!projectNode) return false;
			if(projectNode["Version"] && projectNode["Version"].as<int>()!=1) return false;
            if(projectNode["Rendering"] && (!projectNode["Rendering"]["VSync"] || !projectNode["Rendering"]["TextureSlots"] || !projectNode["Rendering"]["ShaderLoading"]))
                m_Report.Problems.push_back({0,"Runtime rendering","Known Rendering Version 1 defaults: VSync On, batch 32, automatic shaders. Save writes omitted defaults explicitly; original bytes are preserved first",{},true});
            // Commit only a complete parse; preserve the active config on failure.
			ProjectConfig config;
            if(projectNode["Rendering"])config.Rendering=RendererRequestsSerializer::Read(projectNode["Rendering"]);
			config.Name = projectNode["Name"].as<std::string>();
            if(projectNode["AuthoringVersion"])config.AuthoringVersion=projectNode["AuthoringVersion"].as<int>();
            config.ScriptProject = projectNode["ScriptProject"] ? projectNode["ScriptProject"].as<std::string>() : config.Name;
			config.StartScene = Project::NormalizeAssetPath(std::filesystem::u8path(projectNode["StartScene"].as<std::string>()));
			config.AssetDirectory = Project::NormalizeAssetPath(std::filesystem::u8path(projectNode["AssetDirectory"].as<std::string>()));
			config.ScriptModulePath = Project::NormalizeAssetPath(std::filesystem::u8path(projectNode["ScriptModulePath"].as<std::string>()));
			m_Project->GetConfig() = std::move(config);
            m_Report.State=DocumentLoadState::Ready;
			return true;
		}
		catch (const std::exception& error)
		{
            m_Report.Error=error.what();
			HZ_CORE_ERROR("Failed to load project '{}': {}", filepath.generic_u8string(), error.what());
			return false;
		}
	}

}
