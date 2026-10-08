#pragma once
#include "Authoring/EditorPreferences.h"
#include "Authoring/EditorState.h"
#include "Authoring/RendererLaunch.h"
#include "Hazel/Core/Resources.h"
#include "Hazel/Core/UUID.h"
#include "Hazel/Project/RendererRequestsSerializer.h"
namespace Hazel {
    static void RendererPolicyChecks() {
        RendererCapabilities caps;
        caps.MaxTextureSlots = 16;
        const auto selected = RendererPolicy::Resolve(
            RendererPolicy::Settings({}, DebugOutputRequest::Enabled), caps);
        Check(selected.Requested.TextureSlots == 32 && selected.Effective.TextureSlots == 16 &&
                  selected.Requested.PreferShaderBinaries &&
                  !selected.Effective.PreferShaderBinaries &&
                  selected.Requested.EnableDebugOutput && !selected.Effective.EnableDebugOutput &&
                  !selected.TextureReason.empty() && !selected.ShaderReason.empty() &&
                  !selected.DebugReason.empty(),
              "4.1/HD4000 policy lost requests, fallback or limitation reasons");
        RuntimeRendererRequests small;
        small.TextureSlots = 2;
        small.VSync = false;
        small.ShaderLoading = ShaderLoadingRequest::GLSLCompatibility;
        Check(!RendererPolicy::RestartReason(small, selected, caps).empty(),
              "Different batch policy did not require restart");
        auto same = small;
        same.TextureSlots = 16;
        Check(RendererPolicy::RestartReason(same, selected, caps).empty(),
              "Equivalent effective GPU policies required unnecessary recreation");
        caps.MaxTextureSlots = 32;
        caps.ShaderBinaries = true;
        caps.DebugOutput = true;
        const auto binary = RendererPolicy::Resolve(RendererPolicy::Settings({}), caps);
        Check(binary.Effective.PreferShaderBinaries &&
                  !RendererPolicy::RestartReason(same, binary, caps).empty(),
              "Forced compatibility did not distinguish loaded binary policy");
        for (auto slots : {0u, 1u, 33u, UINT32_MAX}) {
            auto bad = small;
            bad.TextureSlots = slots;
            bool rejected = false;
            try {
                RendererPolicy::Settings(bad);
            } catch (const std::exception &) {
                rejected = true;
            }
            Check(rejected, "Invalid portable batch request accepted");
        }
        auto bad = small;
        bad.ShaderLoading = ShaderLoadingRequest(99);
        bool rejected = false;
        try {
            RendererPolicy::Settings(bad);
        } catch (const std::exception &) {
            rejected = true;
        }
        Check(rejected, "Unknown renderer path accepted");
        caps.MaxTextureSlots = 1;
        rejected = false;
        try {
            RendererPolicy::Resolve({}, caps);
        } catch (const std::exception &) {
            rejected = true;
        }
        Check(rejected, "Unusable capability record produced a textured effective path");

        const auto prior = Resources::Get();
        const auto active = Project::GetActive();
        const auto root =
            std::filesystem::temp_directory_path() /
            std::filesystem::u8path("hazel renderer-é-" + std::to_string(uint64_t(UUID())));
        Resources::Configure({root, root, root / "data"});
        struct Cleanup {
            std::filesystem::path Root;
            ApplicationResourceSpecification Prior;
            ~Cleanup() {
                Resources::Configure(Prior);
                std::error_code e;
                std::filesystem::remove_all(Root, e);
            }
        } cleanup{root, prior};
        std::filesystem::create_directories(root / "Assets");
        auto project = CreateRef<Project>();
        auto &config = project->GetConfig();
        config.Name = "Renderer requests";
        config.ScriptProject = "PolicyProbe";
        config.AssetDirectory = "Assets";
        config.StartScene = "Start.hazel";
        config.ScriptModulePath = "Policy.dll";
        config.Rendering = small;
        const auto path = root / "Policy.hproj";
        const auto text = ProjectSerializer(project).SerializeText();
        FileSystem::WriteNewFile(path, text);
        auto loaded = Project::LoadCandidate(path);
        Check(loaded && loaded->GetRendererRequests().TextureSlots == 2 &&
                  !loaded->GetRendererRequests().VSync &&
                  loaded->GetRendererRequests().ShaderLoading ==
                      ShaderLoadingRequest::GLSLCompatibility &&
                  Project::GetActive() == active && text.find("Vendor") == std::string::npos &&
                  text.find("Effective") == std::string::npos,
              "Portable requests did not round-trip independently of active project/device "
              "observations");
        auto legacy = YAML::Load(text);
        legacy["Project"].remove("Rendering");
        YAML::Emitter old;
        old << legacy;
        FileSystem::WriteNewFile(root / "Legacy.hproj", old.c_str());
        const auto legacyProject = Project::LoadCandidate(root / "Legacy.hproj");
        Check(legacyProject && !legacyProject->GetConfig().Rendering &&
                  legacyProject->GetRendererRequests().TextureSlots == 32 &&
                  ProjectSerializer(legacyProject).SerializeText().find("Rendering") ==
                      std::string::npos,
              "Reading/saving an unstamped rendering policy invented portable settings");
        for (const auto &broken :
             {"{Version: 2}", "{Version: 1, HDR: true}", "{Version: 1, TextureSlots: 1}",
              "{Version: 1, TextureSlots: -1}", "{Version: 1, ShaderLoading: Vulkan}"}) {
            auto future = YAML::Load(text);
            future["Project"]["Rendering"] = YAML::Load(broken);
            YAML::Emitter out;
            out << future;
            const auto file = root / "Rejected.hproj";
            FileSystem::WriteFileAtomically(file, [&](auto &stream) { stream << out.c_str(); });
            const auto original = FileDocument::Read(file);
            DocumentLoadReport report;
            Check(!Project::LoadCandidate(file, &report) && !report.Error.empty() &&
                      FileDocument::Read(file) == original && Project::GetActive() == active,
                  "Unsupported/malformed rendering schema changed source or active project");
        }
        std::string diagnostic;
        auto prefs = EditorPreferences::Load(diagnostic);
        prefs.VSync = true;
        prefs.DebugOutput = DebugOutputRequest::Disabled;
        prefs.Save();
        EditorState state(true, "fixture");
        state.Session.LastProject = (root / "Legacy.hproj").generic_u8string();
        state.SaveSession();
        const auto prefBytes = FileDocument::Read(EditorPreferences::Location());
        const auto sessionBytes = FileDocument::Read(Resources::Get().UserData / "session.yaml");
        const auto explicitPolicy =
            EditorRendererLaunch::Read({"Hazelnut", "--project", path.generic_u8string()});
        Check(explicitPolicy.Project == path && explicitPolicy.Settings.TextureSlots == 2 &&
                  !explicitPolicy.Settings.PreferShaderBinaries &&
                  !explicitPolicy.Settings.EnableDebugOutput && explicitPolicy.VSync,
              "Editor prelaunch ignored explicit project or duplicated runtime/editor VSync scope");
        const auto nutella = RendererPolicy::Settings(loaded->GetRendererRequests());
        Check(nutella.TextureSlots == explicitPolicy.Settings.TextureSlots &&
                  nutella.PreferShaderBinaries == explicitPolicy.Settings.PreferShaderBinaries,
              "Editor and Nutella policy selection disagree");
        const auto remembered = EditorRendererLaunch::Read({"Hazelnut"});
        Check(remembered.Settings.TextureSlots == 32 && remembered.Project == root / "Legacy.hproj",
              "Renderer restore did not follow Stage D startup selection");
        const auto missing = EditorRendererLaunch::Read(
            {"Hazelnut", "--project", (root / "Missing.hproj").generic_u8string()});
        const auto noRestore = EditorRendererLaunch::Read({"Hazelnut", "--no-restore"});
        Check(missing.Project.empty() && noRestore.Project.empty() &&
                  missing.Settings.TextureSlots == 32,
              "Failed explicit/no-restore renderer selection silently used remembered requests");
        Check(FileDocument::Read(EditorPreferences::Location()) == prefBytes &&
                  FileDocument::Read(Resources::Get().UserData / "session.yaml") == sessionBytes &&
                  Project::GetActive() == active,
              "Read-only prelaunch altered preferences, session or project");
        FileSystem::WriteFileAtomically(EditorPreferences::Location(), [](auto &out) {
            out << "Version: 1\nDebugOutput: Unrecognized\n";
        });
        const auto invalid = FileDocument::Read(EditorPreferences::Location());
        const auto recovered = EditorPreferences::Load(diagnostic);
        Check(recovered.Invalid && !diagnostic.empty() &&
                  recovered.DebugOutput == DebugOutputRequest::Automatic &&
                  FileDocument::Read(EditorPreferences::Location()) == invalid,
              "Unknown graphics preferences erased original data");
        std::cout << "PASS: renderer requests/resolution/reasons, project scope/round-trip/future "
                     "preservation, editor preferences and shared prelaunch precedence/agreement\n";
    }
} // namespace Hazel
