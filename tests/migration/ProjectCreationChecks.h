#pragma once
#include "Authoring/AuthoringReadiness.h"
#include "Hazel/Core/FileDocument.h"
#include "Hazel/Core/FileSystem.h"
#include "Hazel/Core/UUID.h"
#include "Hazel/Project/Project.h"
#include "Hazel/Project/ProjectCreation.h"
namespace Hazel {
    static void ProjectCreationChecks() {
        const auto root =
            std::filesystem::temp_directory_path() /
            std::filesystem::u8path("hazel native creation-é-" + std::to_string(uint64_t(UUID())));
        std::filesystem::create_directories(root / "templates");
        struct Cleanup {
            std::filesystem::path Root;
            ~Cleanup() {
                std::error_code e;
                std::filesystem::remove_all(Root, e);
            }
        } cleanup{root};
        FileSystem::WriteNewFile(root / "templates/contract.json",
                                 "{\"Version\":1,\"Generator\":1,\"ScriptCore\":1,\"Template\":1}");
        FileSystem::WriteNewFile(root / "templates/project.lua", "workspace \"@IDENTIFIER@\"\n");
        FileSystem::WriteNewFile(root / "templates/Entity.cs",
                                 "using Hazel; namespace @NAMESPACE@ { public class "
                                 "@CLASS@ : Entity {} }\n");
        ProjectCreateRequest request{u8"Content garden é", "GardenProbe",
                                     root / std::filesystem::u8path("new folder é"),
                                     root / "templates"};
        const auto active = Project::GetActive();
        const auto result = ProjectCreation::Create(request);
        auto candidate = Project::LoadCandidate(result.Descriptor);
        Check(candidate && Project::GetActive() == active,
              "Native creation changed active project/session");
        Check(candidate->GetConfig().AuthoringVersion == 1 &&
                  candidate->GetConfig().ScriptModulePath == "Scripts/Binaries/GardenProbe.dll" &&
                  !std::filesystem::exists(request.Destination /
                                           "Assets/Scripts/Binaries/GardenProbe.dll"),
              "Native generation pretended to compile or lost intended artifact "
              "identity");
        auto scene =
            YAML::Load(FileDocument::Read(request.Destination / "Assets/Scenes/Start.hazel"));
        Check(scene["SceneVersion"].as<int>() == 1 && scene["Entities"].size() == 2 &&
                  scene["Entities"][0]["CameraComponent"]["Primary"].as<bool>(),
              "Native starter scene lacked primary camera/content");
        Check(FileDocument::Read(request.Destination / "Assets/Scripts/premake5.lua")
                          .find("GardenProbe") != std::string::npos &&
                  FileDocument::Read(request.Destination / "Assets/Scripts/Source/Example.cs")
                          .find("namespace GardenProbe") != std::string::npos,
              "Native shared template substitution failed");
        const auto before = FileDocument::Read(result.Descriptor);
        bool rejected = false;
        try {
            ProjectCreation::Create(request);
        } catch (const std::exception &) {
            rejected = true;
        }
        Check(rejected && FileDocument::Read(result.Descriptor) == before,
              "Native creation replaced existing content");
        auto bad = request;
        bad.Destination = root / "invalid";
        bad.Identifier = "class";
        rejected = false;
        try {
            ProjectCreation::Create(bad);
        } catch (const std::exception &) {
            rejected = true;
        }
        Check(rejected && !std::filesystem::exists(bad.Destination),
              "Invalid generator request published partial project");
        bad.Identifier = "Valid";
        std::filesystem::remove(root / "templates/Entity.cs");
        rejected = false;
        try {
            ProjectCreation::Create(bad);
        } catch (const std::exception &) {
            rejected = true;
        }
        Check(rejected && !std::filesystem::exists(bad.Destination),
              "Missing templates published partial project");
        FileSystem::WriteNewFile(root / "templates/Entity.cs", "namespace @NAMESPACE@ {}\n");
        FileSystem::WriteFileAtomically(root / "templates/contract.json", [](std::ostream &out) {
            out << "{\"Version\":1,\"Generator\":2,"
                   "\"ScriptCore\":1,\"Template\":1}";
        });
        rejected = false;
        try {
            ProjectCreation::Create(bad);
        } catch (const std::exception &) {
            rejected = true;
        }
        Check(rejected && !std::filesystem::exists(bad.Destination),
              "Incompatible templates published a project");
        const auto future = root / "Future.hproj";
        auto futureData = YAML::Load(before);
        futureData["Project"]["AuthoringVersion"] = 2;
        YAML::Emitter encoded;
        encoded << futureData;
        FileSystem::WriteNewFile(future, encoded.c_str());
        const auto original = FileDocument::Read(future);
        DocumentLoadReport report;
        Check(!Project::LoadCandidate(future, &report) && !report.Error.empty() &&
                  FileDocument::Read(future) == original && Project::GetActive() == active,
              "Future authoring contract accepted or changed source/session");
        const auto reserved = root / "reserved";
        std::filesystem::create_directory(reserved);
        const auto source = root / "source";
        std::filesystem::create_directory(source);
        FileSystem::WriteNewFile(source / "sentinel", "retained");
        rejected = false;
        try {
            FileSystem::PublishDirectoryNew(source, reserved);
        } catch (const std::exception &) {
            rejected = true;
        }
        Check(rejected && FileDocument::Read(source / "sentinel") == "retained" &&
                  std::filesystem::is_empty(reserved),
              "Exclusive directory publication replaced a concurrent empty "
              "destination");
        AuthoringReadinessInput state;
        state.Project = true;
        state.Saved = true;
        auto readiness = AuthoringReadiness::Evaluate(state);
        Check(readiness.Editing && readiness.Playing && !readiness.Scripts && !readiness.Exporting,
              "Content editing/script-free Play required tools");
        state.AssignedScripts = true;
        readiness = AuthoringReadiness::Evaluate(state);
        Check(!readiness.Playing, "Assigned scripts accepted missing assembly");
        state.AssemblyLoaded = true;
        state.ClassesAvailable = true;
        readiness = AuthoringReadiness::Evaluate(state);
        Check(readiness.Playing && !readiness.Scripts,
              "Prepared scripted Play unnecessarily required SDK");
        state.ToolSDK = true;
        state.ToolsChecked = true;
        readiness = AuthoringReadiness::Evaluate(state);
        Check(readiness.Scripts && readiness.Exporting, "Checked tools failed readiness gate");
        std::cout << "PASS: native canonical content creation without GPU/compiler, "
                     "intended "
                     "module identity, template/failure/active-session preservation, "
                     "exclusive "
                     "publication and distinct readiness scopes\n";
    }
} // namespace Hazel
