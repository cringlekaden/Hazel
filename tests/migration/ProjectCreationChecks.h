#pragma once
#include "Authoring/AuthoringReadiness.h"
#include "Hazel/Assets/NativeStarter.h"
#include "Hazel/Assets/ProjectAssets.h"
#include "Hazel/Core/FileDocument.h"
#include "Hazel/Core/FileSystem.h"
#include "Hazel/Core/UUID.h"
#include "Hazel/Project/Project.h"
#include "Hazel/Project/ProjectCreation.h"
#include "Hazel/Project/ScriptSource.h"
#include "Hazel/Renderer/Color.h"
#include "Hazel/Scene/RuntimeSession.h"
#include "Hazel/Scene/SceneSerializer.h"
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
        Check(scene["SceneVersion"].as<int>() == 3 && scene["Entities"].size() == 5 &&
                  scene["Entities"][0]["CameraComponent"]["Primary"].as<bool>(),
              "Native starter scene lacked primary camera/content");
        Check(FileDocument::Read(request.Destination / "Assets/Scripts/premake5.lua")
                          .find("GardenProbe") != std::string::npos &&
                  FileDocument::Read(request.Destination / "Assets/Scripts/Source/Example.cs")
                          .find("namespace GardenProbe") != std::string::npos,
              "Native shared template substitution failed");
        auto cpu=CreateRef<Scene>();SceneSerializer codec(cpu);
        Check(codec.DeserializeText(YAML::Dump(scene),false,ResourceLoading::MetadataOnly),"Native factory metadata-only load");
        const auto retained=codec.CaptureAuthoredState();auto defaults=candidate->GetConfig().SceneDefaults;
        defaults.Perspective=true;defaults.Navigation=EditorNavigation::Orbit;defaults.Presentation.Background={.1f,.2f,.3f,1};
        candidate->GetConfig().SceneDefaults=defaults;
        Check(codec.CaptureAuthoredState()==retained,"Project defaults rewrote an existing scene");
        const auto futureScene=SceneDefaults::CreateDocument("Future",defaults);auto next=CreateRef<Scene>();
        Check(SceneSerializer(next).DeserializeText(futureScene,false,ResourceLoading::MetadataOnly)&&next->Presentation()==defaults.Presentation&&next->GetEntityByUUID(1).GetComponent<CameraComponent>().Camera.GetProjectionType()==SceneCamera::ProjectionType::Perspective,"New Scene defaults lost projection/presentation");
        auto copy=Scene::Copy(next);Check(SceneSerializer(copy).CaptureAuthoredState()==SceneSerializer(next).CaptureAuthoredState(),"Scene copy lost authored presentation");
        Check(SceneDefaults::SuggestIdentifier("123 hello!")=="Game123hello" && SceneDefaults::SuggestIdentifier(u8"你好")=="MyGame" && ScriptSource::ValidIdentifier(SceneDefaults::SuggestIdentifier("class")),"Technical identifier suggestion is not deterministic/valid");
        auto unsupported=YAML::Load(futureScene);unsupported["Presentation"]["Mode"]="SceneReferred";
        Check(!SceneSerializer(next).DeserializeText(YAML::Dump(unsupported), false,
                                                     ResourceLoading::MetadataOnly) &&
                  SceneSerializer(next).CaptureAuthoredState() ==
                      SceneSerializer(copy).CaptureAuthoredState(),
              "Incomplete scene-referred presentation published or changed accepted document");
        NativeStarter::Generate(root / "templates");
        auto directRequest = request;
        directRequest.DirectPBR = true;
        directRequest.Destination = root / "direct content";
        const auto directResult = ProjectCreation::Create(directRequest);
        auto directProject = Project::LoadCandidate(directResult.Descriptor);
        Check(directProject && directProject->GetConfig().SceneDefaults.DirectStarter &&
                  directProject->GetConfig().SceneDefaults.Presentation.SceneReferred,
              "Direct preset lost independent defaults");
        auto directScene = CreateRef<Scene>();
        auto directAssets = CreateRef<ProjectAssets>(directProject->GetAssetRoot());
        SceneSerializer directCodec(directScene, directProject->GetAssetRoot(), false,
                                    directAssets);
        Check(directCodec.DeserializeText(
                  FileDocument::Read(directProject->GetAssetRoot() / "Scenes/Start.hazel"), false,
                  ResourceLoading::MetadataOnly),
              "Native direct starter metadata load");
        RuntimeSession::Validate(directScene, false);
        auto directState = directCodec.CaptureAuthoredState();
        auto light = directScene->GetEntityByUUID(6);
        light.GetComponent<DirectionalLightComponent>().Intensity = 4;
        Check(directCodec.CaptureAuthoredState() != directState,
              "Directional edit was omitted from exact authored state");
        light.GetComponent<DirectionalLightComponent>().Intensity = 3;
        auto directCopy = Scene::Copy(directScene);
        Check(SceneSerializer(directCopy).CaptureAuthoredState() == directState,
              "Copy lost direct light/presentation");
        auto futureLight = YAML::Load(directCodec.SerializeText());
        futureLight["Entities"][5]["AlienLight"] = true;
        Check(!directCodec.DeserializeText(YAML::Dump(futureLight), false,
                                           ResourceLoading::MetadataOnly) &&
                  directCodec.CaptureAuthoredState() == directState,
              "Future light data published partially");
        directScene->Presentation().Exposure = 1;
        auto exposure = directCodec.SerializeText();
        auto restored = CreateRef<Scene>();
        Check(SceneSerializer(restored, directProject->GetAssetRoot(), false, directAssets)
                      .DeserializeText(exposure, false, ResourceLoading::MetadataOnly) &&
                  restored->Presentation().Exposure == 1,
              "Scene-referred exposure round trip");
        directScene->Presentation().SceneReferred = false;
        directScene->Presentation().Exposure = 0;
        bool unsupportedDirect = false;
        try
        {
            RuntimeSession::Validate(directScene, false);
        }
        catch (const std::exception &)
        {
            unsupportedDirect = true;
        }
        Check(unsupportedDirect, "Lit display-referred content silently accepted");
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
