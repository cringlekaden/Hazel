#include "hzpch.h"
#include "ProjectCreation.h"
#include "Hazel/Core/DocumentSchema.h"
#include "Hazel/Core/FileDocument.h"
#include "Hazel/Core/FileSystem.h"
#include "Hazel/Core/UUID.h"
#include "Hazel/Scene/SceneSerializer.h"
#include "ProjectSerializer.h"
#include "ScriptSource.h"
#include <yaml-cpp/yaml.h>
namespace Hazel {
    void ProjectCreation::ValidateTemplates(const std::filesystem::path &folder) {
        auto contract = YAML::Load(FileDocument::Read(folder / "contract.json"));
        DocumentSchema::Structure(contract);
        DocumentSchema::Keys(contract, {"Version", "Generator", "ScriptCore", "Template"},
                             "Authoring contract");
        for (const char *key : {"Version", "Generator", "ScriptCore", "Template"})
            if (contract[key].as<int>() != 1)
                throw std::runtime_error("Incompatible authoring/ScriptCore template contract; use "
                                         "matching editor/SDK tools");
        for (const char *file : {"project.lua", "Entity.cs"})
            if (!std::filesystem::is_regular_file(folder / file))
                throw std::runtime_error("Missing native authoring template: " +
                                         (folder / file).generic_u8string());
    }
    ProjectCreateResult ProjectCreation::Create(const ProjectCreateRequest &r) {
        if (r.TemplateVersion != 1)
            throw std::runtime_error("Unsupported project template version");
        if (r.Name.empty() || r.Name.size() > 480 ||
            r.Name.find_first_not_of(" \t\r\n") == std::string::npos ||
            r.Name.find('\0') != std::string::npos)
            throw std::runtime_error(
                "Enter a nonempty project display name (at most 480 UTF-8 bytes)");
        if (!ScriptSource::ValidIdentifier(r.Identifier))
            throw std::runtime_error("Use a C# identifier beginning with a letter or "
                                     "underscore; avoid reserved names");
        if (!r.Destination.is_absolute() || r.Destination.filename().empty())
            throw std::runtime_error("Choose an absolute new project folder");
        const auto destination = std::filesystem::absolute(r.Destination).lexically_normal();
        if (std::filesystem::exists(destination) ||
            !std::filesystem::is_directory(destination.parent_path()))
            throw std::runtime_error("Choose a new folder beneath an existing parent; existing "
                                     "destinations are never replaced");
        ValidateTemplates(r.Templates);
        auto staging = destination.parent_path() /
                       (".hazel-create-" + std::to_string(static_cast<uint64_t>(UUID())));
        if (!std::filesystem::create_directory(staging))
            throw std::runtime_error("Cannot exclusively reserve project staging directory");
        struct Cleanup {
            std::filesystem::path Path;
            ~Cleanup() {
                std::error_code error;
                std::filesystem::remove_all(Path, error);
            }
        } cleanup{staging};
        for (const char *directory :
             {"Scenes", "Scripts/Source", "Scripts/Binaries", "Textures", "Prefabs"})
            std::filesystem::create_directories(staging / "Assets" / directory);
        auto project = CreateRef<Project>();
        auto &config = project->GetConfig();
        config.Name = r.Name;
        config.ScriptProject = r.Identifier;
        config.AssetDirectory = "Assets";
        config.StartScene = "Scenes/Start.hazel";
        config.ScriptModulePath =
            std::filesystem::path("Scripts/Binaries") / (r.Identifier + ".dll");
        config.AuthoringVersion = 1;
        const auto descriptor = staging / (r.Identifier + ".hproj");
        FileSystem::WriteNewFile(descriptor, ProjectSerializer(project).SerializeText());
        auto scene = CreateRef<Scene>();
        scene->SetName("Start");
        auto camera = scene->CreateEntityWithUUID(1, "Camera");
        camera.AddComponent<CameraComponent>().Camera.SetOrthographic(10, -1, 1);
        auto data = YAML::Load(SceneSerializer(scene, staging / "Assets").SerializeText());
        // Authored text DTO avoids TextComponent's runtime default font/GPU
        // dependency.
        YAML::Node welcome;
        welcome["Entity"] = uint64_t(2);
        welcome["Relationship"]["Parent"] = uint64_t(0);
        welcome["Relationship"]["Order"] = 1;
        welcome["TagComponent"]["Tag"] = "Welcome";
        auto transform = welcome["TransformComponent"];
        transform["Translation"] = std::vector<float>{-3, 0, 0};
        transform["Rotation"] = std::vector<float>{0, 0, 0};
        transform["Scale"] = std::vector<float>{.5f, .5f, 1};
        auto text = welcome["TextComponent"];
        text["TextString"] = r.Name;
        text["Color"] = std::vector<float>{1, 1, 1, 1};
        text["Kerning"] = 0.f;
        text["LineSpacing"] = 0.f;
        data["Entities"].push_back(welcome);
        DocumentSchema::Scene(data);
        YAML::Emitter encoded;
        encoded << data;
        if (!encoded.good())
            throw std::runtime_error(encoded.GetLastError());
        FileSystem::WriteNewFile(staging / "Assets/Scenes/Start.hazel", encoded.c_str());
        auto build = FileDocument::Read(r.Templates / "project.lua");
        const std::string token = "@IDENTIFIER@";
        for (size_t at = 0; (at = build.find(token, at)) != std::string::npos;
             at += r.Identifier.size())
            build.replace(at, token.size(), r.Identifier);
        FileSystem::WriteNewFile(staging / "Assets/Scripts/premake5.lua", build);
        ScriptSource::Create(staging / "Assets", "Example", r.Identifier, r.Templates);
        // Validate the descriptor through its production codec before exclusive
        // publication.
        if (!Project::LoadCandidate(descriptor))
            throw std::runtime_error("Generated descriptor validation failed");
        ProjectCreateResult result;
        result.Descriptor = destination / (r.Identifier + ".hproj");
        for (const auto &item : std::filesystem::recursive_directory_iterator(staging))
            if (item.is_regular_file())
                result.Files.push_back(destination / item.path().lexically_relative(staging));
        std::sort(result.Files.begin(), result.Files.end());
        FileSystem::PublishDirectoryNew(staging, destination);
        return result;
    }
} // namespace Hazel
