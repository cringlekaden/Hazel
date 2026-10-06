#include "Authoring/EditorPreferences.h"
#include "Hazel/Core/DocumentSchema.h"
#include "Hazel/Core/FileDocument.h"
#include "Hazel/Core/FileLease.h"
#include "Hazel/Core/FileSystem.h"
#include "Hazel/Core/Resources.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <yaml-cpp/yaml.h>
namespace Hazel {
    std::filesystem::path EditorPreferences::Location() {
        return Resources::Get().UserData / "preferences.yaml";
    }
    EditorPreferences EditorPreferences::Load(std::string &diagnostic) {
        diagnostic.clear();
        EditorPreferences defaults;
        try {
            if (!std::filesystem::exists(Location()))
                return defaults;
            defaults.ObservedExists = true;
            defaults.AcceptedBytes = FileDocument::Read(Location());
            auto data = YAML::Load(defaults.AcceptedBytes);
            DocumentSchema::Structure(data);
            DocumentSchema::Keys(data,
                                 {"Version", "Python", "SDK", "ScriptEditor", "UIScale",
                                  "ShowColliders", "ConsoleCapture", "RecentProjects",
                                  "RestoreSession", "VSync", "DebugOutput"},
                                 "Preferences");
            if (data["Version"].as<int>() != 1)
                throw std::runtime_error("Unsupported preferences version");
            EditorPreferences parsed;
            parsed.ObservedExists = true;
            parsed.AcceptedBytes = defaults.AcceptedBytes;
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
            if (data["RestoreSession"])
                parsed.RestoreSession = data["RestoreSession"].as<bool>();
            if (data["DebugOutput"])parsed.DebugOutput=RendererPolicy::ParseDebug(data["DebugOutput"].as<std::string>());
            if (data["VSync"])
                parsed.VSync = data["VSync"].as<bool>();
            if (data["ConsoleCapture"])
                parsed.ConsoleCapture = data["ConsoleCapture"].as<int>();
            if (parsed.ConsoleCapture < 0 || parsed.ConsoleCapture > 5)
                throw std::runtime_error("Console capture must be Trace through Critical");
            if (data["RecentProjects"])
                parsed.RecentProjects = data["RecentProjects"].as<std::vector<std::string>>();
            if (parsed.RecentProjects.size() > 12)
                parsed.RecentProjects.resize(12);
            return parsed;
        } catch (const std::exception &error) {
            diagnostic = "Preferences recovered to defaults; original file preserved: " +
                         std::string(error.what());
            defaults.Invalid = true;
            return defaults;
        }
    }
    void EditorPreferences::Save(bool replaceInvalid) const {
        if (!std::isfinite(UIScale) || UIScale < .8f || UIScale > 2 || ConsoleCapture < 0 ||
            ConsoleCapture > 5 || RecentProjects.size() > 12)
            throw std::runtime_error("Invalid preference values");
        RendererPolicy::DebugName(DebugOutput); // Validate before creating/writing anything.
        for (const auto *value : {&Python, &SDK, &ScriptEditor})
            if (value->size() > 32768 || value->find('\0') != std::string::npos)
                throw std::runtime_error("Invalid tool path in preferences");
        YAML::Emitter out;
        out << YAML::BeginMap << YAML::Key << "Version" << YAML::Value << 1 << YAML::Key << "Python"
            << YAML::Value << Python << YAML::Key << "SDK" << YAML::Value << SDK << YAML::Key
            << "ScriptEditor" << YAML::Value << ScriptEditor << YAML::Key << "UIScale"
            << YAML::Value << UIScale << YAML::Key << "ShowColliders" << YAML::Value
            << ShowColliders << YAML::Key << "ConsoleCapture" << YAML::Value << ConsoleCapture
            << YAML::Key << "RecentProjects" << YAML::Value << YAML::BeginSeq;
        for (auto &path : RecentProjects)
            out << path;
        out << YAML::EndSeq << YAML::Key << "RestoreSession" << YAML::Value << RestoreSession
            << YAML::Key << "VSync" << YAML::Value << VSync
            << YAML::Key << "DebugOutput" << YAML::Value << RendererPolicy::DebugName(DebugOutput) << YAML::EndMap;
        if (!out.good())
            throw std::runtime_error(out.GetLastError());
        std::filesystem::create_directories(Location().parent_path());
        auto lease = FileLease::Try(Location().parent_path() / "preferences.lock");
        if (!lease)
            throw std::runtime_error("Another editor is saving preferences; retry");
        if (std::filesystem::exists(Location()) != ObservedExists ||
            (ObservedExists && FileDocument::Read(Location()) != AcceptedBytes))
            throw std::runtime_error("Preferences changed externally. Reload saved preferences "
                                     "before applying this draft.");
        if (Invalid && !replaceInvalid)
            throw std::runtime_error("Malformed/future preferences retained; explicit Apply and "
                                     "Save preserves an original backup first");
        const std::string text = out.c_str();
        if (ObservedExists) {
            FileDocument file;
            file.Open(Location(), Invalid);
            file.Save(text, Resources::Get().UserData / "recovery");
        } else
            FileSystem::WriteFileAtomically(
                Location(), [&](auto &stream) { stream << text; }, WriteMode::CreateNew);
        AcceptedBytes = text;
        ObservedExists = true;
        Invalid = false;
    }
    void EditorPreferences::ResetValues() {
        auto bytes = AcceptedBytes;
        const auto existed = ObservedExists, invalid = Invalid;
        *this = {};
        AcceptedBytes = std::move(bytes);
        ObservedExists = existed;
        Invalid = invalid;
    }
    void EditorPreferences::Remember(const std::filesystem::path &project) {
        auto path = std::filesystem::absolute(project).generic_u8string();
        RecentProjects.erase(std::remove(RecentProjects.begin(), RecentProjects.end(), path),
                             RecentProjects.end());
        RecentProjects.insert(RecentProjects.begin(), path);
        if (RecentProjects.size() > 12)
            RecentProjects.resize(12);
    }
} // namespace Hazel
