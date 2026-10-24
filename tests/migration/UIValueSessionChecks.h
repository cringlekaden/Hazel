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
    auto slider = make(500, canvas, {20, 300});
    slider.GetComponent<UIRectComponent>().FixedSize = {200, 50};
    auto &sb = slider.AddComponent<UIButtonComponent>();
    sb.Kind = ControlKind::Slider;
    sb.Action = "volume";
    auto &sv = slider.AddComponent<UISliderComponent>();
    sv.Minimum = 0;
    sv.Maximum = 10;
    sv.Step = 3;
    sv.InitialValue = 3;
    auto part = [&](uint64_t id, glm::vec2 size) {
        auto child = make(id, slider, {});
        child.GetComponent<UIRectComponent>().FixedSize = size;
        child.AddComponent<SpriteRendererComponent>();
        return id;
    };
    sv.Track = part(501, {180, 12});
    sv.Thumb = part(502, {20, 30});
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
        // Standardized synthetic controller crosses the production adapter (not physical
        // acceptance).
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
        sample.Keys.fill(false);
        sample.Pad.Buttons.fill(false);
        route();
        Check(scene->FocusUI(500), "Managed slider focus failed");
        sample.Pad.Buttons[0] = true;
        route();
        scene->OnUpdateRuntime(.016f);
        Check(scene->IsUISliderEditing(), "Controller Accept did not enter slider edit");
        sample.Pad.Buttons.fill(false);
        route();
        sample.Pad.Buttons[size_t(GamepadButton::Right)] = true;
        route();
        scene->OnUpdateRuntime(.016f);
        Check(scene->GetUISliderValue(500) == 6 &&
                  owner->GetFieldValue<int>("SliderChanges") == 1 &&
                  owner->GetFieldValue<float>("SliderPrevious") == 3 &&
                  owner->GetFieldValue<float>("SliderValue") == 6,
              "Managed controller Adjust/preview ABI failed");
        for (int n = 0; n < 10; ++n)
            route();
        Check(scene->GetUISliderValue(500) == 6, "Held Adjust repeated before delay");
        sample.Pad.Buttons.fill(false);
        route();
        sample.Pad.Buttons[1] = true;
        route();
        scene->OnUpdateRuntime(.016f);
        Check(!scene->IsUISliderEditing() && scene->GetUISliderValue(500) == 3 &&
                  owner->GetFieldValue<int>("SliderCancels") == 1 &&
                  owner->GetFieldValue<uint64_t>("SliderGesture") != 0,
              "Managed Cancel/gesture ABI failed");
        sample.Pad.Buttons.fill(false);
        route();
        sample.Pad.Buttons[0] = true;
        route();
        sample.Pad.Buttons.fill(false);
        route();
        sample.Pad.Buttons[size_t(GamepadButton::Right)] = true;
        route();
        sample.Pad.Buttons.fill(false);
        route();
        sample.Pad.Buttons[0] = true;
        route();
        scene->OnUpdateRuntime(.016f);
        Check(owner->GetFieldValue<int>("SliderCommits") == 1 &&
                  owner->GetFieldValue<float>("SliderValue") == 6,
              "Managed commit signal missing");
        // Remap through the actual session override service, not a slider-private key path.
        auto remapped = session.GetUIBindings();
        remapped.Actions[size_t(UIAction::AdjustIncrease)] = {Key::K, -1, int(GamepadButton::X)};
        session.ApplyUIBindings(remapped);
        sample.Keys.fill(false);
        sample.Pad.Buttons.fill(false);
        route();
        route();
        scene->FocusUI(500);
        sample.Keys[Key::Enter] = true;
        route();
        sample.Keys.fill(false);
        route();
        sample.Keys[Key::Right] = true;
        route();
        Check(scene->GetUISliderValue(500) == 6, "Retired Adjust binding still changed a value");
        sample.Keys.fill(false);
        route();
        sample.Keys[Key::K] = true;
        route();
        Check(scene->GetUISliderValue(500) == 9,
              "Remapped keyboard Adjust did not use session override");
        sample.Keys.fill(false);
        route();
        sample.Pad.Buttons[size_t(GamepadButton::X)] = true;
        route();
        Check(scene->GetUISliderValue(500) == 10,
              "Remapped controller Adjust missed short endpoint interval");
        sample.Pad.Buttons.fill(false);
        route();
        sample.Keys[Key::Enter] = true;
        route();
        Action remapEvent;
        Check(scene->ConsumeUIEvent(100, remapEvent) &&
                  remapEvent.Kind == UIEventKind::SliderChanged && remapEvent.PreviousValue == 6 &&
                  remapEvent.Value == 10,
              "Remapped edit lost coalesced owner payload");
        Check(scene->ConsumeUIEvent(100, remapEvent) &&
                  remapEvent.Kind == UIEventKind::SliderCommitted && remapEvent.StartValue == 6,
              "Remapped edit did not commit");
        session.ResetUIBindings();
        session.Stop();
        Check(SceneSerializer(authored).CaptureAuthoredState() == baseline,
              "Runtime controls leaked into Edit document");
    }
    std::cout << "PASS: native/managed control API, typed value ABI and synthetic keyboard/gamepad "
                 "once-only events across three Play/Stop cycles\n";
}
