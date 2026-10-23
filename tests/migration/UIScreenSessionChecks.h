static void UIScreenSessionChecks(const Ref<Project> &project)
{
    using namespace GameUI;
    auto authored = CreateRef<Scene>();
    auto thief = authored->CreateEntityWithUUID(400, "Other consumer");
    thief.AddComponent<ScriptComponent>().ClassName = "Migration.ScreenOtherProbe";
    auto menu = authored->CreateEntityWithUUID(500, "Menu"),
         settings = authored->CreateEntityWithUUID(600, "Settings");
    for (auto e : {menu, settings})
    {
        auto &c = e.AddComponent<UICanvasComponent>();
        c.ReferenceSize = {400, 400};
        c.SafeInsets = {};
        c.Modal = true;
        c.Screen.Managed = true;
        c.Screen.InitiallyOpen = e == menu;
        c.Screen.EventOwner = 500;
    }
    menu.AddComponent<ScriptComponent>().ClassName = "Migration.ScreenOwnerProbe";
    auto &field = ScriptEngine::GetScriptFieldMap(menu)["Settings"];
    field.Field = {ScriptFieldType::Entity, "Settings", nullptr};
    field.SetValue<uint64_t>(600);
    for (auto pair : {std::pair<uint64_t, Entity>{501, menu}, {601, settings}})
    {
        auto e = authored->CreateEntityWithUUID(pair.first, "Control");
        authored->Reparent(e, pair.second, TransformPolicy::KeepLocal);
        auto &r = e.AddComponent<UIRectComponent>();
        r.Offset = {20, 20};
        r.FixedSize = {100, 40};
        e.AddComponent<UIButtonComponent>().Action = pair.first == 501 ? "settings" : "back";
    }
    auto baseline = SceneSerializer(authored).CaptureAuthoredState();
    RuntimeSession session;
    session.Resize(400, 400);
    session.ResizeFramebuffer(400, 400);
    for (int repeat = 0; repeat < 3; ++repeat)
    {
        session.Start(project, authored);
        auto scene = session.GetScene();
        auto owner = ScriptEngine::GetEntityScriptInstance(500);
        auto other = ScriptEngine::GetEntityScriptInstance(400);
        DeviceSample sample;
        sample.Point = {300, 300};
        auto route = [&]
        {
            session.SetInput(sample.Point, true);
            session.RouteUIInput(sample, .016f, true);
        };
        route();
        sample.Keys[Key::Enter] = true;
        route();
        Action unscoped;
        Check(!scene->ConsumeUIAction(0, unscoped), "Managed event escaped through old stream");
        scene->OnUpdateRuntime(.016f);
        Check(owner->GetFieldValue<int>("Count") == 1 && other->GetFieldValue<int>("Stolen") == 0 &&
                  scene->IsUIScreenOpen(600) && scene->GetUISnapshot().Scope == 600,
              "Owner callback delivery/open boundary failed");
        route();
        scene->OnUpdateRuntime(.016f);
        Check(owner->GetFieldValue<int>("Count") == 1, "Held Accept activated child screen");
        sample.Keys.fill(false);
        route();
        sample.Keys[Key::Escape] = true;
        route();
        scene->OnUpdateRuntime(.016f);
        Check(!scene->IsUIScreenOpen(600), "Built-in scoped Cancel did not close child");
        sample.Keys.fill(false);
        route();
        Check(scene->GetUIDiagnostics().Focus == 501, "Session did not restore invoking focus");
        // The managed envelope must carry semantic Cancel/context, not a fabricated activation.
        auto &child = scene->GetEntityByUUID(600).GetComponent<UICanvasComponent>();
        child.Screen.Cancel = ScreenCancel::NotifyOwner;
        child.Screen.CancelTarget = 601;
        sample.Keys[Key::Enter] = true;
        route();
        scene->OnUpdateRuntime(.016f);
        sample.Keys.fill(false);
        route();
        sample.Keys[Key::Escape] = true;
        route();
        scene->OnUpdateRuntime(.016f);
        Check(!scene->IsUIScreenOpen(600) && owner->GetFieldValue<int>("Count") == 3 &&
                  owner->GetFieldValue<int>("LastKind") == int(UIEventKind::Cancel) &&
                  owner->GetFieldValue<uint64_t>("LastSource") == 601,
              "Managed semantic Cancel/context delivery failed");
        sample.Keys.fill(false);
        route();
        // Domain replacement must retire a transferred event and any uncommitted
        // screen request.
        sample.Keys[Key::Enter] = true;
        route();
        scene->RequestUIScreen(ScreenOperation::Open, 600);
        ScriptEngine::ReloadAssembly();
        Action event;
        Check(!scene->ConsumeUIEvent(500, event) && scene->GetUIDiagnostics().PendingScreens == 0,
              "Domain replacement retained event/request");
        session.Stop();
        Check(SceneSerializer(authored).CaptureAuthoredState() == baseline,
              "Play/Stop changed authored screen policy/history");
    }
    auto retiring = Scene::Copy(authored);
    auto issuer = retiring->CreateEntityWithUUID(700, "Retiring issuer");
    issuer.AddComponent<ScriptComponent>().ClassName = "Migration.ScreenRetiringProbe";
    auto &target = ScriptEngine::GetScriptFieldMap(issuer)["Target"];
    target.Field = {ScriptFieldType::Entity, "Target", nullptr};
    target.SetValue<uint64_t>(600);
    session.Start(project, retiring);
    auto running = session.GetScene();
    InputFrame neutral;
    neutral.Enabled = true;
    running->UpdateUIInput(neutral);
    running->OnUpdateRuntime(.016f);
    Check(!running->IsUIScreenOpen(600) && running->GetUIDiagnostics().RejectedScreens > 0,
          "Retired callback issuer applied queued screen request");
    session.Stop();
    std::cout << "PASS: managed declared-owner delivery, callback screen boundary, "
                 "held gate, Cancel, reload and three Play/Stop cycles\n";
}
