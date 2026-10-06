#include "EditorState.h"
#include "Hazel/Core/DocumentSchema.h"
#include "Hazel/Core/FileSystem.h"
#include "Hazel/Core/Resources.h"
#include <cmath>
#include <iomanip>
#include <sstream>
#include <yaml-cpp/yaml.h>
namespace Hazel {
    namespace {
        template <class T> void Field(const YAML::Node &n, const char *key, T &value) {
            if (n[key])
                value = n[key].as<T>();
        }
        void Version(const YAML::Node &n) {
            DocumentSchema::Structure(n);
            if (!n["Version"] || n["Version"].as<int>() != 1)
                throw std::runtime_error("Unsupported editor state Version (expected 1)");
        }
        void Text(const std::string &s) {
            if (s.size() > 32768 || s.find('\0') != std::string::npos)
                throw std::runtime_error("Invalid/oversized editor state text");
        }
        void Validate(const EditorSessionState &s) {
            const auto &w = s.Window;
            if (w.Width < 1 || w.Width > 100000 || w.Height < 1 || w.Height > 100000 ||
                std::llabs(static_cast<long long>(w.X)) > 1000000 ||
                std::llabs(static_cast<long long>(w.Y)) > 1000000 || !std::isfinite(w.Scale) ||
                w.Scale < .25f || w.Scale > 8 || s.Panels > 31 || s.Severities > 0x3f ||
                s.Sources > 0x7f)
                throw std::runtime_error("Invalid window/filter settings");
            Text(s.LastProject);
            Text(s.ConsoleSearch);
        }
        void Validate(const ProjectWorkspace &s) {
            for (const auto *text : {&s.Scene, &s.BrowserFolder, &s.BrowserSearch, &s.Sheet,
                                     &s.Prefab, &s.ExportFolder, &s.ExportName})
                Text(*text);
            if (s.BrowserType < 0 || s.BrowserType > 6 || !std::isfinite(s.Thumbnail) ||
                s.Thumbnail < 32 || s.Thumbnail > 160 || !std::isfinite(s.Pitch) ||
                !std::isfinite(s.Yaw) || !std::isfinite(s.Distance) || s.Distance < .1f ||
                s.Distance > 100000)
                throw std::runtime_error("Invalid workspace controls/camera");
            if (s.Sections.size() > 16)
                throw std::runtime_error("Too many component sections");
            for (const auto &section : s.Sections)
                Text(section.first);
            for (int i = 0; i < 3; ++i)
                if (!std::isfinite(s.Focus[i]))
                    throw std::runtime_error("Invalid workspace camera focal point");
        }
        std::string EmitState(const YAML::Node &n) {
            YAML::Emitter out;
            out.SetFloatPrecision(9);
            out << n;
            if (!out.good())
                throw std::runtime_error(out.GetLastError());
            return out.c_str();
        }
    } // namespace
    EditorLaunch EditorLaunch::Parse(const std::vector<std::string> &args) {
        EditorLaunch result;
        try {
            for (size_t i = 1; i < args.size(); ++i) {
                const auto &arg = args[i];
                if (arg == "--no-restore") {
                    result.Restore = false;
                    continue;
                }
                auto path = [&]() {
                    if (++i == args.size() || args[i].empty() || args[i].rfind("--", 0) == 0)
                        throw std::runtime_error("Missing path after " + arg);
                    return std::filesystem::absolute(std::filesystem::u8path(args[i]))
                        .lexically_normal();
                };
                if (arg == "--project") {
                    result.Explicit = true;
                    if (!result.Project.empty())
                        throw std::runtime_error("Project specified twice");
                    result.Project = path();
                } else if (arg == "--scene") {
                    result.Explicit = true;
                    if (!result.Scene.empty())
                        throw std::runtime_error("Scene specified twice");
                    result.Scene = path();
                } else if (arg.rfind("--", 0) == 0)
                    throw std::runtime_error("Unknown editor argument: " + arg);
                else {
                    result.Explicit = true;
                    if (!result.Project.empty())
                        throw std::runtime_error("Use --project and --scene for multiple paths");
                    result.Project =
                        std::filesystem::absolute(std::filesystem::u8path(arg)).lexically_normal();
                }
            }
        } catch (const std::exception &e) {
            result.Explicit = true;
            result.Error = e.what();
        }
        return result;
    }
    std::filesystem::path EditorLaunch::ProjectToOpen(bool preference,
                                                      const std::string &remembered) const {
        if (!Error.empty())
            return {};
        if (Explicit)
            return Project;
        return Restore && preference && !remembered.empty() ? std::filesystem::u8path(remembered)
                                                            : std::filesystem::path{};
    }
    std::filesystem::path EditorState::WorkspaceLocation(const std::filesystem::path &descriptor) {
        const auto path = std::filesystem::weakly_canonical(descriptor).generic_u8string();
        uint64_t hash = 14695981039346656037ull;
        for (unsigned char c : path) {
            hash ^= c;
            hash *= 1099511628211ull;
        }
        std::ostringstream name;
        name << std::hex << hash;
        return Resources::Get().UserData / "workspaces" / (name.str() + ".yaml");
    }
    std::string EditorState::Read(const std::filesystem::path &path) {
        if (!std::filesystem::exists(path))
            return {};
        if (m_Files.size() > 32)
            for (auto it = m_Files.begin(); it != m_Files.end();) {
                if (it->first != path && it->first.filename() != "session.yaml") {
                    m_LastWritten.erase(it->first);
                    m_Invalid.erase(it->first);
                    it = m_Files.erase(it);
                    if (m_Files.size() <= 32)
                        break;
                } else
                    ++it;
            }
        if (std::filesystem::file_size(path) > 128 * 1024)
            throw std::runtime_error(
                "Editor state exceeds 128 KiB; original retained, move it aside before resetting");
        auto &file = m_Files[path];
        file.Open(path);
        const auto &text = file.Original();
        if (text.size() > 128 * 1024)
            throw std::runtime_error("Editor state exceeds 128 KiB");
        return text;
    }
    EditorState::EditorState(bool primary, std::string instance)
        : m_Primary(primary), m_Instance(std::move(instance)) {
        const auto path = Resources::Get().UserData / "session.yaml";
        try {
            auto text = Read(path);
            if (text.empty()) {
                if (!m_Files[path].Path().empty())
                    throw std::runtime_error("Empty session file");
                return;
            }
            const auto n = YAML::Load(text);
            Version(n);
            DocumentSchema::Keys(n,
                                 {"Version", "Window", "LastProject", "Panels", "Severities",
                                  "Sources", "ConsoleSearch", "Follow"},
                                 "Editor session");
            auto w = n["Window"];
            DocumentSchema::Keys(w, {"X", "Y", "Width", "Height", "Scale", "Maximized"}, "Window");
            auto &p = Session.Window;
            Field(w, "X", p.X);
            Field(w, "Y", p.Y);
            Field(w, "Width", p.Width);
            Field(w, "Height", p.Height);
            Field(w, "Scale", p.Scale);
            Field(w, "Maximized", p.Maximized);
            Field(n, "LastProject", Session.LastProject);
            Field(n, "Panels", Session.Panels);
            Field(n, "Severities", Session.Severities);
            Field(n, "Sources", Session.Sources);
            Field(n, "ConsoleSearch", Session.ConsoleSearch);
            Field(n, "Follow", Session.Follow);
            Validate(Session);
        } catch (const std::exception &e) {
            Session = {};
            m_Invalid.insert(path);
            Diagnostic = "Session not restored; original retained at " + path.generic_u8string() +
                         ": " + e.what();
        }
    }
    ProjectWorkspace EditorState::LoadWorkspace(const std::filesystem::path &descriptor) {
        const auto path = WorkspaceLocation(descriptor);
        ProjectWorkspace s;
        try {
            auto text = Read(path);
            if (text.empty()) {
                if (!m_Files[path].Path().empty())
                    throw std::runtime_error("Empty workspace file");
                return s;
            }
            auto n = YAML::Load(text);
            Version(n);
            DocumentSchema::Keys(n, {"Version",       "Scene",     "BrowserFolder", "BrowserSearch",
                                     "BrowserType",   "Thumbnail", "Entity",        "Sheet",
                                     "Region",        "Clip",      "SheetVisible",  "Prefab",
                                     "PrefabVisible", "Sections",  "Focus",         "Pitch",
                                     "Yaw",           "Distance",  "ExportFolder",  "ExportName"},
                                 "Project workspace");
            Field(n, "Scene", s.Scene);
            Field(n, "BrowserFolder", s.BrowserFolder);
            Field(n, "BrowserSearch", s.BrowserSearch);
            Field(n, "BrowserType", s.BrowserType);
            Field(n, "Thumbnail", s.Thumbnail);
            Field(n, "Entity", s.Entity);
            Field(n, "Sheet", s.Sheet);
            Field(n, "Region", s.Region);
            Field(n, "Clip", s.Clip);
            Field(n, "SheetVisible", s.SheetVisible);
            Field(n, "Prefab", s.Prefab);
            Field(n, "PrefabVisible", s.PrefabVisible);
            if (n["Sections"]) {
                if (!n["Sections"].IsMap() || n["Sections"].size() > 16)
                    throw std::runtime_error("Invalid component section settings");
                for (auto entry : n["Sections"]) {
                    const auto key = entry.first.as<std::string>();
                    Text(key);
                    s.Sections[key] = entry.second.as<bool>();
                }
            }
            Field(n, "Pitch", s.Pitch);
            Field(n, "Yaw", s.Yaw);
            Field(n, "Distance", s.Distance);
            Field(n, "ExportFolder", s.ExportFolder);
            Field(n, "ExportName", s.ExportName);
            if (n["Focus"]) {
                auto v = n["Focus"].as<std::vector<float>>();
                if (v.size() != 3)
                    throw std::runtime_error("Wrong focal point size");
                s.Focus = {v[0], v[1], v[2]};
            }
            Validate(s);
            return s;
        } catch (const std::exception &e) {
            m_Invalid.insert(path);
            Diagnostic = "Workspace not restored; original retained at " + path.generic_u8string() +
                         ": " + e.what();
            return {};
        }
    }
    std::string EditorState::Encode(const EditorSessionState &s) {
        Validate(s);
        YAML::Node n;
        n["Version"] = 1;
        auto w = n["Window"];
        w["X"] = s.Window.X;
        w["Y"] = s.Window.Y;
        w["Width"] = s.Window.Width;
        w["Height"] = s.Window.Height;
        w["Scale"] = s.Window.Scale;
        w["Maximized"] = s.Window.Maximized;
        n["LastProject"] = s.LastProject;
        n["Panels"] = s.Panels;
        n["Severities"] = s.Severities;
        n["Sources"] = s.Sources;
        n["ConsoleSearch"] = s.ConsoleSearch;
        n["Follow"] = s.Follow;
        return EmitState(n);
    }
    std::string EditorState::Encode(const ProjectWorkspace &s) {
        Validate(s);
        YAML::Node n;
        n["Version"] = 1;
        n["Scene"] = s.Scene;
        n["BrowserFolder"] = s.BrowserFolder;
        n["BrowserSearch"] = s.BrowserSearch;
        n["BrowserType"] = s.BrowserType;
        n["Thumbnail"] = s.Thumbnail;
        n["Entity"] = s.Entity;
        n["Sheet"] = s.Sheet;
        n["Region"] = s.Region;
        n["Clip"] = s.Clip;
        n["SheetVisible"] = s.SheetVisible;
        n["Prefab"] = s.Prefab;
        n["PrefabVisible"] = s.PrefabVisible;
        for (const auto &section : s.Sections)
            n["Sections"][section.first] = section.second;
        n["Focus"] = std::vector<float>{s.Focus.x, s.Focus.y, s.Focus.z};
        n["Pitch"] = s.Pitch;
        n["Yaw"] = s.Yaw;
        n["Distance"] = s.Distance;
        n["ExportFolder"] = s.ExportFolder;
        n["ExportName"] = s.ExportName;
        return EmitState(n);
    }
    void EditorState::Write(const std::filesystem::path &shared, const std::string &text,
                            bool reset) {
        const auto path = m_Primary ? shared
                                    : Resources::Get().UserData / "instances" / m_Instance /
                                          shared.lexically_relative(Resources::Get().UserData);
        if (text.size() > 128 * 1024)
            throw std::runtime_error("Editor state exceeds 128 KiB; in-memory state retained");
        if (m_LastWritten[path] == text && !reset)
            return;
        if (m_Primary && m_Invalid.count(shared) && !reset)
            throw std::runtime_error("Invalid/future editor state retained; explicitly reset with "
                                     "original backup before saving: " +
                                     shared.generic_u8string());
        std::filesystem::create_directories(path.parent_path());
        auto &file = m_Files[path];
        if (!file.Path().empty()) {
            if (reset)
                file.PreserveOriginal();
            file.Save(text, Resources::Get().UserData / "recovery");
        } else {
            FileSystem::WriteFileAtomically(
                path, [&](auto &out) { out << text; }, WriteMode::CreateNew);
            file.Open(path);
        }
        m_LastWritten[path] = text;
        m_Invalid.erase(shared);
    }
    void EditorState::SaveSession(bool reset) {
        Write(Resources::Get().UserData / "session.yaml", Encode(Session), reset);
    }
    void EditorState::SaveWorkspace(const std::filesystem::path &descriptor,
                                    const ProjectWorkspace &s, bool reset) {
        Write(WorkspaceLocation(descriptor), Encode(s), reset);
    }
    void EditorState::Associate(const std::filesystem::path &previous,
                                const std::filesystem::path &current) {
        auto s = LoadWorkspace(previous);
        if (m_Invalid.count(WorkspaceLocation(previous)))
            throw std::runtime_error(Diagnostic);
        LoadWorkspace(current);
        SaveWorkspace(current, s, true);
    }
} // namespace Hazel
