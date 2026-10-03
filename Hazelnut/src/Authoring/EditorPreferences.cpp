#include "Authoring/EditorPreferences.h"
#include "Hazel/Core/FileSystem.h"
#include "Hazel/Core/Resources.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <yaml-cpp/yaml.h>
namespace Hazel
{
std::filesystem::path EditorPreferences::Location()
{
	return Resources::Get().UserData / "preferences.yaml";
}
EditorPreferences EditorPreferences::Load(std::string &diagnostic)
{
	diagnostic.clear();
	EditorPreferences defaults;
	try
	{
		if (!std::filesystem::exists(Location()))
			return defaults;
		std::ifstream input(Location());
		auto data = YAML::Load(input);
		if (!input || data["Version"].as<int>() != 1)
			throw std::runtime_error("Unsupported preferences version");
		EditorPreferences parsed;
		if (data["Python"])
			parsed.Python = data["Python"].as<std::string>();
		if (data["SDK"])
			parsed.SDK = data["SDK"].as<std::string>();
		if (data["ScriptEditor"])
			parsed.ScriptEditor = data["ScriptEditor"].as<std::string>();
		if (data["UIScale"])
			parsed.UIScale = data["UIScale"].as<float>();
		if (!std::isfinite(parsed.UIScale) || parsed.UIScale < .8f || parsed.UIScale > 2)
			throw std::runtime_error("UI scale must be 0.8 to 2");
		if (data["ShowColliders"])
			parsed.ShowColliders = data["ShowColliders"].as<bool>();
		if (data["RecentProjects"])
			parsed.RecentProjects = data["RecentProjects"].as<std::vector<std::string>>();
		if (parsed.RecentProjects.size() > 12)
			parsed.RecentProjects.resize(12);
		return parsed;
	}
	catch (const std::exception &error)
	{
		diagnostic =
			"Preferences recovered to defaults; original file preserved: " + std::string(error.what());
		return defaults;
	}
}
void EditorPreferences::Save() const
{
	YAML::Emitter out;
	out << YAML::BeginMap << YAML::Key << "Version" << YAML::Value << 1 << YAML::Key << "Python"
		<< YAML::Value << Python << YAML::Key << "SDK" << YAML::Value << SDK << YAML::Key << "ScriptEditor"
		<< YAML::Value << ScriptEditor << YAML::Key << "UIScale" << YAML::Value << UIScale << YAML::Key
		<< "ShowColliders" << YAML::Value << ShowColliders << YAML::Key << "RecentProjects" << YAML::Value
		<< YAML::BeginSeq;
	for (auto &path : RecentProjects)
		out << path;
	out << YAML::EndSeq << YAML::EndMap;
	if (!out.good())
		throw std::runtime_error(out.GetLastError());
	std::filesystem::create_directories(Location().parent_path());
	FileSystem::WriteFileAtomically(Location(), [&](std::ostream &stream) { stream << out.c_str(); });
}
void EditorPreferences::Remember(const std::filesystem::path &project)
{
	auto path = std::filesystem::absolute(project).generic_u8string();
	RecentProjects.erase(std::remove(RecentProjects.begin(), RecentProjects.end(), path),
						 RecentProjects.end());
	RecentProjects.insert(RecentProjects.begin(), path);
	if (RecentProjects.size() > 12)
		RecentProjects.resize(12);
}
} // namespace Hazel
