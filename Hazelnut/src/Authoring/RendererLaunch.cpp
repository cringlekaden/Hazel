#include "RendererLaunch.h"
#include "EditorPreferences.h"
#include "EditorState.h"
#include "Hazel/Core/FileSystem.h"
#include "Hazel/Project/Project.h"
namespace Hazel {
    EditorRendererLaunch EditorRendererLaunch::Read(const std::vector<std::string> &arguments) {
        EditorRendererLaunch result;
        std::string diagnostic;
        const auto preferences = EditorPreferences::Load(diagnostic);
        result.Settings = RendererPolicy::Settings({}, preferences.DebugOutput);
        result.VSync = preferences.VSync;
        const auto launch = EditorLaunch::Parse(arguments);
        if (!launch.Error.empty())
            return result; // Ordinary Open reports it; no remembered fallback.
        const EditorState state(false, "prelaunch-read-only");
        auto path = launch.ProjectToOpen(preferences.RestoreSession, state.Session.LastProject);
        if (path.empty() && !launch.Explicit && launch.Restore &&
            state.Session.LastProject.empty()) {
            const auto bundled =
                Project::Discover(FileSystem::GetExecutablePath().parent_path() / "Example");
            if (bundled.size() == 1)
                path = bundled.front();
        }
        if (!path.empty()) {
            if (const auto project = Project::LoadCandidate(path)) {
                result.Project = path;
                result.Settings = RendererPolicy::Settings(project->GetRendererRequests(),
                                                           preferences.DebugOutput);
            }
        }
        return result;
    }
} // namespace Hazel
