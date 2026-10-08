#pragma once
#include "Authoring/EditorPreferences.h"
#include "Authoring/EditorState.h"
#include "Hazel/Core/FileLease.h"
#include "Hazel/Core/FileSystem.h"
#include "Hazel/Core/Resources.h"
#include "Hazel/Core/UUID.h"
namespace Hazel {
    static void EditorStateChecks() {
        const auto root = std::filesystem::temp_directory_path() /
                          std::filesystem::u8path("hazel workspace space-é-" +
                                                  std::to_string(static_cast<uint64_t>(UUID())));
        const auto prior = Resources::Defaults("State tests");
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
        auto lock = FileLease::Try(root / "data/test.lock");
        Check(bool(lock) && !FileLease::Try(root / "data/test.lock"),
              "Settings lease admitted two owners");
        lock.reset();
        Check(bool(FileLease::Try(root / "data/test.lock")),
              "Settings lease survived owner teardown");
        WindowPlacement p;
        p.X = 4000;
        p.Y = -4000;
        p.Width = 1600;
        p.Height = 1000;
        p.Maximized = true;
        auto fit = FitWindow(p, {{0, 0, 1024, 640, 1}});
        Check(fit.X >= 0 && fit.Y >= 0 && fit.Width <= 1024 && fit.Height <= 640 && fit.Maximized,
              "Offscreen/maximized restore lost reachability/state");
        p = {-1200, 30, 900, 600, 1, false};
        fit = FitWindow(p, {{0, 0, 1024, 640, 1}, {-1280, 0, 1280, 720, 2}});
        Check(fit.X < 0 && fit.Width <= 1280 && fit.Scale == 2,
              "Secondary monitor/DPI policy selected wrong display");
        p.Width = 1;
        p.Height = 1;
        fit = FitWindow(p, {{0, 0, 800, 480, 1}});
        Check(fit.Width >= 640 && fit.Height >= 360, "Restore admitted unusably small window");
        auto explicitLaunch = EditorLaunch::Parse(
            {"editor", "--project", "missing é.hproj", "--scene", "missing.hazel"});
        Check(explicitLaunch.Explicit &&
                  explicitLaunch.ProjectToOpen(true, "remembered.hproj") == explicitLaunch.Project,
              "Explicit project lost precedence");
        auto invalid = EditorLaunch::Parse({"editor", "--unknown"});
        Check(!invalid.Error.empty() && invalid.ProjectToOpen(true, "remembered.hproj").empty(),
              "Bad argument fell back to remembered project");
        auto none = EditorLaunch::Parse({"editor", "--no-restore"});
        Check(none.ProjectToOpen(true, "remembered.hproj").empty(), "No-restore ignored");
        auto sceneOnly = EditorLaunch::Parse({"editor", "--scene", "selected.hazel"});
        Check(sceneOnly.Explicit && sceneOnly.ProjectToOpen(true, "remembered.hproj").empty(),
              "Explicit scene borrowed remembered project");
        const auto project = root / std::filesystem::u8path("project é.hproj");
        FileSystem::WriteNewFile(project, "fixture");
        EditorState primary(true, "primary");
        primary.Session.LastProject = project.generic_u8string();
        primary.Session.Window = p;
        primary.SaveSession();
        ProjectWorkspace workspace;
        workspace.Scene = "Scenes/Last.hazel";
        workspace.Focus = {2, 3, 4};
        workspace.Entity = 42;
        workspace.Sheet = "Sprites/Lanterns.hsprites";
        workspace.Region = 7;
        workspace.ExportFolder = (root / "export").generic_u8string();
        primary.SaveWorkspace(project, workspace);
        EditorState restored(true, "restart");
        auto loaded = restored.LoadWorkspace(project);
        Check(restored.Session.LastProject == project.generic_u8string() &&
                  loaded.Scene == workspace.Scene && loaded.Entity == 42 &&
                  loaded.Focus == workspace.Focus,
              "Session/workspace DTO round trip failed");
        Check(FileDocument::Read(project) == "fixture",
              "Machine settings leaked into portable project");
        auto other = root / "other.hproj";
        Check(restored.LoadWorkspace(other).Scene.empty(),
              "Project workspace leaked across descriptors");
        const auto session = root / "data/session.yaml";
        const auto before = FileDocument::Read(session);
        EditorState secondary(false, "secondary");
        secondary.Session.LastProject = "different.hproj";
        secondary.SaveSession();
        Check(FileDocument::Read(session) == before &&
                  std::filesystem::exists(root / "data/instances/secondary/session.yaml"),
              "Secondary instance replaced shared session");
        FileSystem::WriteFileAtomically(
            session, [](auto &out) { out << "Version: 99\nFuture: preserved\n"; });
        EditorState future(true, "future");
        Check(!future.Diagnostic.empty(), "Future settings interpreted");
        bool refused = false;
        try {
            future.SaveSession();
        } catch (const std::exception &) {
            refused = true;
        }
        Check(refused && FileDocument::Read(session).find("Future: preserved") != std::string::npos,
              "Future state overwritten automatically");
        future.SaveSession(true);
        bool originalFound = false;
        for (const auto &entry : std::filesystem::directory_iterator(root / "data/recovery"))
            if (entry.path().extension() == ".original" &&
                FileDocument::Read(entry.path()).find("Future: preserved") != std::string::npos)
                originalFound = true;
        Check(originalFound, "Explicit state reset did not preserve original");
        EditorPreferences prefs;
        std::string diagnostic;
        prefs = EditorPreferences::Load(diagnostic);
        prefs.SDK = (root / "SDK é").generic_u8string();
        prefs.CustomCaption=true;
        prefs.Save();
        auto first = EditorPreferences::Load(diagnostic),
             second = EditorPreferences::Load(diagnostic);
        Check(first.CustomCaption && diagnostic.empty(), "Caption preference did not round trip");
        first.UIScale = 1.25f;
        first.Save();
        refused = false;
        try {
            second.Save();
        } catch (const std::exception &) {
            refused = true;
        }
        Check(refused, "Preferences overwrote a concurrent instance");
        first.ResetValues();
        first.Save();
        Check(EditorPreferences::Load(diagnostic).SDK.empty() && !EditorPreferences::Load(diagnostic).CustomCaption,
              "Reset defaults corrupted preference ownership");
        FileSystem::WriteFileAtomically(EditorPreferences::Location(),
                                        [](auto &out) { out << "Version: 99\nFuture: keep\n"; });
        auto broken = EditorPreferences::Load(diagnostic);
        Check(broken.Invalid && !diagnostic.empty(), "Invalid preferences silently interpreted");
        refused = false;
        try {
            broken.Save();
        } catch (const std::exception &) {
            refused = true;
        }
        Check(refused, "Automatic preference save rewrote future data");
        broken.Save(true);
        Check(!EditorPreferences::Load(diagnostic).Invalid, "Explicit preference recovery failed");
        std::cout << "PASS: production launch precedence, display/DPI bounds, native lease "
                     "teardown, scoped workspace/secondary snapshots, future settings and "
                     "concurrent preference conflicts\n";
    }
} // namespace Hazel
