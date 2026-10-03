#pragma once
#include <filesystem>
#include <string>
#include <vector>
namespace Hazel
{
struct EditorPreferences
{
	std::string Python, SDK, ScriptEditor;
	float UIScale = 1.0f;
	bool ShowColliders = false;
	std::vector<std::string> RecentProjects;
	static std::filesystem::path Location();
	static EditorPreferences Load(std::string &diagnostic);
	void Save() const;
	void Remember(const std::filesystem::path &project);
};
} // namespace Hazel
