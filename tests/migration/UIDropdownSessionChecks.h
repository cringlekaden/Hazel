#include "UIDropdownFixture.h"
static void UIDropdownSessionChecks(const Ref<Project> &project)
{
    using namespace GameUI;
    auto authored = DropdownFixture();
    authored->GetEntityByUUID(900).AddComponent<ScriptComponent>().ClassName =
        "Migration.DropdownOwnerProbe";
    auto baseline = SceneSerializer(authored).CaptureAuthoredState();
    RuntimeSession session;
    session.Resize(400, 300);
    session.ResizeFramebuffer(400, 300);
    for (int i = 0; i < 3; ++i)
    {
        session.Start(project, authored);
        auto scene = session.GetScene();
        auto owner = ScriptEngine::GetEntityScriptInstance(900);
        Check(owner && owner->GetFieldValue<bool>("Ready"),
              "Managed Dropdown selection/component roundtrip failed");
        DeviceSample sample;
        sample.Point = {20, 150};
        auto route = [&](bool enabled = true)
        {
            session.SetInput(sample.Point, enabled);
            session.RouteUIInput(sample, .016f, enabled);
        };
        route();
        Check(scene->FocusUI(210), "Managed dropdown trigger focus failed");
        sample.Keys[Key::Enter] = true;
        route();
        scene->OnUpdateRuntime(0);
        Check(scene->IsUIDropdownOpen(200), "Production Accept did not open dropdown");
        sample.Keys.fill(false);
        route();
        route();
        Check(scene->FocusUI(310), "Managed dropdown choice focus failed");
        sample.Keys[Key::Enter] = true;
        route();
        scene->OnUpdateRuntime(0);
        sample.Keys.fill(false);
        route();
        scene->OnUpdateRuntime(0);
        Check(owner->GetFieldValue<int>("Events") == 1 &&
                  owner->GetFieldValue<uint64_t>("Selected") == 310 &&
                  scene->GetUISelection(200) == 310 && !scene->IsUIDropdownOpen(200),
              "Managed owner lost/duplicated dropdown selection across popup close");
        Check(scene->RequestUIDropdown(200, true), "Reopen request failed");
        scene->CommitUIScreens();
        route();
        route();
        route(false);
        scene->OnUpdateRuntime(0);
        Check(!scene->IsUIDropdownOpen(200) && scene->GetUISelection(200) == 310,
              "Input ownership loss did not dismiss coherently");
        session.Stop();
        Check(SceneSerializer(authored).CaptureAuthoredState() == baseline,
              "Dropdown Play/Stop altered authored configuration");
    }
    std::cout << "PASS: managed Dropdown values/open/close and once-only "
                 "parent-owner selection in three isolated Play/Stop cycles\n";
}
