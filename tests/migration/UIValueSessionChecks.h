static void UIValueSessionChecks(const Ref<Project> &project)
{
    using namespace GameUI;
    auto authored = CreateRef<Scene>();
    auto canvas = authored->CreateEntityWithUUID(100, "Value screen");
    auto &c = canvas.AddComponent<UICanvasComponent>();
    c.ReferenceSize = {400, 400};
    c.SafeInsets = {};
    c.Modal = true;
    c.Screen.Managed = true;
    c.Screen.EventOwner = 100;
    c.Screen.InitialFocus = 101;
    canvas.AddComponent<ScriptComponent>().ClassName = "Migration.ValueOwnerProbe";
    auto make = [&](uint64_t id, Entity parent, glm::vec2 offset) {
        auto e = authored->CreateEntityWithUUID(id, "Value " + std::to_string(id));
        authored->Reparent(e, parent, TransformPolicy::KeepLocal);
        auto &r = e.AddComponent<UIRectComponent>();
        r.Offset = offset;
        r.FixedSize = {100, 40};
        return e;
    };
    auto toggle = make(101, canvas, {20, 20});
    toggle.AddComponent<UIButtonComponent>().Kind = ControlKind::Toggle;
    auto group = make(200, canvas, {20, 100});
    group.GetComponent<UIRectComponent>().FixedSize = {300, 80};
    group.AddComponent<UIRadioGroupComponent>().InitialSelection = 201;
    for (int i = 0; i < 3; ++i)
    {
        auto e = make(201 + i, group, {float(i * 100), 0});
        auto &b = e.AddComponent<UIButtonComponent>();
        b.Kind = ControlKind::Radio;
        b.Group = 200;
    }
    auto progress = make(300, canvas, {20, 220});
    progress.AddComponent<UIProgressComponent>().InitialValue = .25f;
    auto baseline = SceneSerializer(authored).CaptureAuthoredState();
    RuntimeSession session;
    session.Resize(400, 400);
    session.ResizeFramebuffer(400, 400);
    for (int repeat = 0; repeat < 3; ++repeat)
    {
        session.Start(project, authored);
        auto scene = session.GetScene();
        auto owner = ScriptEngine::GetEntityScriptInstance(100);
        Check(owner->GetFieldValue<bool>("SettersPassed") && !scene->IsUIChecked(101) &&
                  scene->GetUISelection(200) == 201 && scene->GetUIProgress(300) == .25f,
              "Managed control getters/silent setters failed");
        DeviceSample sample;
        sample.Point = {390, 390};
        auto route = [&] {
            session.SetInput(sample.Point, true);
            session.RouteUIInput(sample, .016f, true);
        };
        route();
        sample.Keys[Key::Enter] = true;
        route();
        scene->OnUpdateRuntime(.016f);
        route();
        scene->OnUpdateRuntime(.016f);
        Check(owner->GetFieldValue<int>("ToggleChanges") == 1 &&
                  owner->GetFieldValue<bool>("LastChecked") && scene->IsUIChecked(101),
              "Managed toggle payload/held activation failed");
        sample.Keys.fill(false);
        route();
        Check(scene->FocusUI(202), "Managed radio focus failed");
        sample.Keys[Key::Enter] = true;
        route();
        scene->OnUpdateRuntime(.016f);
        Check(owner->GetFieldValue<int>("RadioChanges") == 1 &&
                  owner->GetFieldValue<uint64_t>("Previous") == 201 &&
                  owner->GetFieldValue<uint64_t>("Selected") == 202,
              "Managed radio typed member ABI failed");
        // Standardized synthetic controller crosses the production adapter (not physical acceptance).
        sample.Keys.fill(false);
        sample.Pad.Device = 0;
        route();
        route();
        scene->FocusUI(101);
        sample.Pad.Buttons[0] = true;
        route();
        scene->OnUpdateRuntime(.016f);
        route();
        scene->OnUpdateRuntime(.016f);
        Check(owner->GetFieldValue<int>("ToggleChanges") == 2 && !scene->IsUIChecked(101),
              "Production gamepad Accept did not toggle once");
        session.Stop();
        Check(SceneSerializer(authored).CaptureAuthoredState() == baseline,
              "Runtime controls leaked into Edit document");
    }
    std::cout << "PASS: native/managed control API, typed value ABI and synthetic keyboard/gamepad "
                 "once-only events across three Play/Stop cycles\n";
}
