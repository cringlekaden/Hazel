static void UIScrollSessionChecks(const Ref<Project> &project)
{
    using namespace GameUI;
    auto authored = CreateRef<Scene>();
    auto canvas = authored->CreateEntityWithUUID(100, "Scroll owner");
    auto &c = canvas.AddComponent<UICanvasComponent>();
    c.ReferenceSize = {400, 300};
    c.SafeInsets = {};
    c.Modal = true;
    c.Screen.Managed = true;
    c.Screen.EventOwner = 100;
    c.Screen.InitialFocus = 300;
    canvas.AddComponent<ScriptComponent>().ClassName = "Migration.ScrollOwnerProbe";
    auto make = [&](uint64_t id, const char *name, Entity parent, glm::vec2 offset,
                    glm::vec2 size) {
        auto e = authored->CreateEntityWithUUID(id, name);
        authored->Reparent(e, parent, TransformPolicy::KeepLocal);
        auto &r = e.AddComponent<UIRectComponent>();
        r.Offset = offset;
        r.FixedSize = size;
        return e;
    };
    auto view = make(200, "Scroll root", canvas, {}, {100, 100});
    auto &config = view.AddComponent<UIScrollViewComponent>();
    config.Content = 201;
    config.RevealMargin = 0;
    config.PixelsPerUnit = 20;
    auto content = make(201, "Scroll content", view, {}, {100, 300});
    content.GetComponent<UIRectComponent>().WidthMode = SizeMode::Fill;
    auto far = make(202, "Scroll far", content, {0, 260}, {100, 40});
    far.AddComponent<UIButtonComponent>();
    auto command = make(300, "Reveal command", canvas, {200, 0}, {100, 40});
    command.AddComponent<UIButtonComponent>().Action = "reveal";
    auto baseline = SceneSerializer(authored).CaptureAuthoredState();
    RuntimeSession session;
    session.Resize(400, 300);
    session.ResizeFramebuffer(400, 300);
    for (int repeat = 0; repeat < 3; ++repeat)
    {
        session.Start(project, authored);
        auto scene = session.GetScene();
        auto owner = ScriptEngine::GetEntityScriptInstance(100);
        Check(owner && owner->GetFieldValue<bool>("Ready") &&
                  scene->GetUIScroll(200).Offset == glm::vec2(0),
              "Managed scroll offset/normalized/reveal API failed");
        DeviceSample sample;
        sample.Point = {390, 290};
        auto route = [&](bool enabled = true) {
            session.SetInput(sample.Point, enabled);
            session.RouteUIInput(sample, .016f, enabled);
        };
        route();
        sample.Keys[Key::Enter] = true;
        route();
        scene->OnUpdateRuntime(.016f);
        Check(owner->GetFieldValue<bool>("Revealed") && scene->GetUIDiagnostics().Focus == 202 &&
                  scene->GetUIScroll(200).Offset.y == 200,
              "Managed reveal did not publish before focus");
        sample.Keys.fill(false);
        route();
        sample.Keys[Key::PageUp] = true;
        route();
        scene->OnUpdateRuntime(.016f);
        Check(scene->GetUIScroll(200).Offset.y == 110 && owner->GetFieldValue<int>("Events") == 1,
              "Page action failed or emitted owner events");
        sample.Keys.fill(false);
        route();
        auto bindings = session.GetUIBindings();
        bindings.Actions[size_t(UIAction::ScrollDown)].Key = Key::K;
        session.ApplyUIBindings(bindings);
        scene->ScrollUITo(200, {});
        sample.Pad.Device = 0;
        route();
        route();
        sample.Pad.Buttons[size_t(GamepadButton::RightBumper)] = true;
        route();
        Check(scene->GetUIScroll(200).Offset.y == 90, "Controller Scroll action failed");
        sample.Pad.Buttons.fill(false);
        route();
        sample.Keys[Key::K] = true;
        route();
        Check(scene->GetUIScroll(200).Offset.y == 180, "Remapped Scroll action failed");
        MouseScrolledEvent wheel(0, 1);
        session.SubmitUIEvent(wheel, {50, 50}, true);
        route(false);
        auto offset = scene->GetUIScroll(200).Offset;
        sample.Keys.fill(false);
        route();
        route();
        Check(scene->GetUIScroll(200).Offset == offset,
              "Input restoration replayed buffered wheel");
        session.ResetUIBindings();
        session.Stop();
        Check(SceneSerializer(authored).CaptureAuthoredState() == baseline,
              "Runtime scroll entered authored state/history");
    }
    std::cout << "PASS: managed scroll offsets/normalized/reveal, remapped keyboard/controller "
                 "Scroll and three isolated Play/Stop cycles\n";
}
