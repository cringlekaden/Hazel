#pragma once
#include "Hazel/Core/FileDocument.h"
#include "Hazel/Core/WindowPlacement.h"
#include <glm/glm.hpp>
#include <map>
#include <set>
namespace Hazel {
    struct EditorLaunch {
        std::filesystem::path Project, Scene;
        bool Explicit = false, Restore = true;
        std::string Error;
        static EditorLaunch Parse(const std::vector<std::string> &args);
        std::filesystem::path ProjectToOpen(bool restorePreference,
                                            const std::string &remembered) const;
    };
    struct EditorSessionState {
        WindowPlacement Window;
        std::string LastProject, ConsoleSearch;
        unsigned Panels = 15, Severities = 0x3f, Sources = 0x7f;
        bool Follow = true;
    };
    struct ProjectWorkspace {
        std::string Scene, BrowserFolder = ".", BrowserSearch, Sheet, Prefab, ExportFolder,
                           ExportName = "Game";
        uint64_t Entity = 0, Region = 0, Clip = 0;
        int BrowserType = 0;
        float Thumbnail = 64, Pitch = 0, Yaw = 0, Distance = 10;
        glm::vec3 Focus{0};
        bool SheetVisible = false, PrefabVisible = false;
        std::map<std::string, bool> Sections;
    };
    // Machine-local DTO persistence. No active project, ImGui, asset loads or runtime state.
    class EditorState {
      public:
        EditorState(bool primary, std::string instance);
        EditorSessionState Session;
        std::string Diagnostic;
        ProjectWorkspace LoadWorkspace(const std::filesystem::path &descriptor);
        void SaveSession(bool reset = false);
        void SaveWorkspace(const std::filesystem::path &descriptor, const ProjectWorkspace &value,
                           bool reset = false);
        void Associate(const std::filesystem::path &previous, const std::filesystem::path &current);
        static std::filesystem::path WorkspaceLocation(const std::filesystem::path &descriptor);
        static std::string Encode(const EditorSessionState &value);
        static std::string Encode(const ProjectWorkspace &value);

      private:
        std::string Read(const std::filesystem::path &path);
        void Write(const std::filesystem::path &path, const std::string &text, bool reset);
        bool m_Primary;
        std::string m_Instance;
        std::map<std::filesystem::path, FileDocument> m_Files;
        std::set<std::filesystem::path> m_Invalid;
        std::map<std::filesystem::path, std::string> m_LastWritten;
    };
} // namespace Hazel
