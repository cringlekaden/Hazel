#pragma once
#include <filesystem>
#include <string>
#include <vector>
#include "Hazel/Renderer/RendererPolicy.h"
namespace Hazel
{
struct EditorPreferences
{
	std::string Python, SDK, ScriptEditor;
	float UIScale = 1.0f;
	bool ShowColliders = false, RestoreSession = true, VSync = true;
    int ConsoleCapture = 2;
    DebugOutputRequest DebugOutput = DebugOutputRequest::Automatic;
	std::vector<std::string> RecentProjects;
	static std::filesystem::path Location();
	static EditorPreferences Load(std::string &diagnostic);
	void Save(bool replaceInvalid = false) const;
    void ResetValues();
    mutable std::string AcceptedBytes;
    mutable bool ObservedExists=false, Invalid=false;
	void Remember(const std::filesystem::path &project);
};
} // namespace Hazel
