#include "UICompositionFixture.h"
static void UICompositionSessionChecks(const Ref<Project> &project)
{
    using namespace GameUI;
    auto authored = CompositionFixture();
    authored->GetEntityByUUID(900).AddComponent<ScriptComponent>().ClassName =
        "Migration.CompositionOwnerProbe";
    const auto before = SceneSerializer(authored).CaptureAuthoredState();
    RuntimeSession session;
    session.Resize(400, 300);
    session.ResizeFramebuffer(400, 300);
    for (int i = 0; i < 3; ++i)
    {
        session.Start(project, authored);
        auto scene = session.GetScene();
        auto owner = ScriptEngine::GetEntityScriptInstance(900);
        Check(owner && owner->GetFieldValue<bool>("Ready"),
              "Managed Tabs/Tab/Tooltip component references failed");
        DeviceSample sample;
        sample.Point = {399, 299};
        if (i == 1)
            sample.Pad.Device = 0;
        auto route = [&] {
            session.SetInput(sample.Point, true);
            session.RouteUIInput(sample, .016f, true);
        };
        route();
        route();
        Check(scene->FocusUI(211), "Managed tab focus failed");
        if (i == 1)
            sample.Pad.Buttons[size_t(GamepadButton::A)] = true;
        else
            sample.Keys[Key::Enter] = true;
        route();
        scene->OnUpdateRuntime(.016f);
        sample.Keys.fill(false);
        sample.Pad.Buttons.fill(false);
        route();
        route();
        scene->OnUpdateRuntime(.016f);
        Check(owner->GetFieldValue<int>("Events") == 1 && owner->GetFieldValue<uint64_t>("Selected") == 211 &&
                  scene->GetUISelection(200) == 211 && !scene->GetUISnapshot().Find(220)->LayoutResolved,
              "Managed tab activation delivery or page gating failed");
        Check(scene->EnsureUIVisible(232, 231, true), "Managed page reveal failed");
        owner->SetFieldValue<uint64_t>("Requested", 210);
        scene->OnUpdateRuntime(.016f);
        route();
        route();
        scene->OnUpdateRuntime(0);
        Check(scene->GetUISelection(200) == 210 && owner->GetFieldValue<int>("Events") == 1 &&
                  scene->GetUIDiagnostics().Focus == 221,
              "Managed silent deferred selection/focus emitted an event or failed");
        scene->ResetUI();
        scene->RefreshUI();
        Action retired;
        Check(scene->GetUISelection(200) == 210 && !scene->ConsumeUIEvent(900, retired),
              "UI reset retained stale selection/events");
        session.Stop();
        Check(SceneSerializer(authored).CaptureAuthoredState() == before,
              "Compositions Play/Stop changed authored state");
    }
    std::cout << "PASS: managed composition references/selection, production key/pad actions, silent page "
                 "changes and three isolated Play/Stop cycles\n";
}
