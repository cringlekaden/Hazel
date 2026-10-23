// Production Scene APIs and published eligibility, no physical device polling.
static void UIScreenSceneChecks(const std::filesystem::path &root)
{
    auto scene = CreateRef<Scene>();
    scene->OnViewportResize(640, 480);
    auto owner = scene->CreateEntityWithUUID(900, "Screen owner");
    auto other = scene->CreateEntityWithUUID(901, "Other owner");
    auto makeScreen = [&](uint64_t id, bool open, bool modal)
    {
        auto e = scene->CreateEntityWithUUID(id, "Screen " + std::to_string(id));
        auto &c = e.AddComponent<UICanvasComponent>();
        c.ReferenceSize = {640, 480};
        c.SafeInsets = {};
        c.Modal = modal;
        c.Screen.Managed = true;
        c.Screen.InitiallyOpen = open;
        c.Screen.EventOwner = 900;
        return e;
    };
    auto makeButton = [&](uint64_t id, Entity screen, float x)
    {
        auto e = scene->CreateEntityWithUUID(id, "Button " + std::to_string(id));
        auto &r = e.AddComponent<UIRectComponent>();
        r.Offset = {x, 40};
        r.FixedSize = {100, 40};
        e.AddComponent<UIButtonComponent>().Action = "button-" + std::to_string(id);
        scene->Reparent(e, screen, TransformPolicy::KeepLocal);
        return e;
    };
    auto menu = makeScreen(100, true, true), settings = makeScreen(200, false, true),
         confirm = makeScreen(300, false, true);
    auto a = makeButton(101, menu, 20), b = makeButton(102, menu, 160),
         c = makeButton(201, settings, 20), d = makeButton(301, confirm, 20);
    menu.GetComponent<UICanvasComponent>().Screen.InitialFocus = 102;
    menu.GetComponent<UICanvasComponent>().Screen.DefaultAction = 101;
    settings.GetComponent<UICanvasComponent>().Screen.ReturnFocus = 101;
    confirm.GetComponent<UICanvasComponent>().Screen.Cancel = ScreenCancel::NotifyOwner;
    confirm.GetComponent<UICanvasComponent>().Screen.CancelTarget = 301;
    const auto authored = SceneSerializer(scene).CaptureAuthoredState();
    scene->OnRuntimeStart();
    scene->RefreshUI();
    Check(scene->GetUIDiagnostics().Focus == 102,
          "Initial focus was not published while input rearms");
    InputFrame f;
    f.Enabled = true;
    f.Mouse = {600, 450};
    auto route = [&] { scene->UpdateUIInput(f); };
    route();
    Check(scene->IsUIScreenOpen(100) && !scene->IsUIScreenOpen(200) &&
              scene->GetUIDiagnostics().Focus == 102,
          "Screen initial state/configured focus failed");
    auto stable = scene->GetUISnapshot().Generation;
    route();
    Check(scene->GetUISnapshot().Generation == stable, "Unchanged screens rebuilt snapshot");
    Action event;
    f.AcceptDown = true;
    route();
    Check(!scene->ConsumeUIAction(0, event) && !scene->ConsumeUIEvent(901, event) &&
              scene->ConsumeUIEvent(900, event) && event.Screen == 100 && event.Entity == 102 &&
              event.Kind == UIEventKind::Activated && !scene->ConsumeUIEvent(900, event),
          "Declared owner did not exclusively receive once-only activation");
    f.AcceptDown = false;
    route();
    scene->FocusUI(0);
    f.AcceptDown = true;
    route();
    Check(scene->ConsumeUIEvent(900, event) && event.Entity == 101,
          "Unfocused Accept did not select configured default");
    f.AcceptDown = false;
    route();
    Check(scene->FocusUI(102), "Focus invoking control failed");
    Check(scene->RequestUIScreen(ScreenOperation::Open, 200) && !scene->IsUIScreenOpen(200),
          "Open mutated stack before commit");
    scene->CommitUIScreens();
    Check(scene->IsUIScreenOpen(200) && scene->GetUISnapshot().Scope == 200,
          "Open did not publish new scope");
    Check(SceneSerializer(scene).CaptureAuthoredState() == authored,
          "Transient screen stack/focus/events changed authored content");
    f.AcceptDown = true;
    route();
    Check(!scene->ConsumeUIEvent(900, event), "Held transition Accept activated next screen");
    f.AcceptDown = false;
    route();
    Check(scene->GetUIDiagnostics().Focus == 201, "Child initial focus failed after neutral");
    Check(scene->RequestUIScreen(ScreenOperation::Open, 200) &&
              scene->RequestUIScreen(ScreenOperation::Open, 300),
          "Nested/repeated open rejected");
    scene->CommitUIScreens();
    route();
    Check(scene->GetUISnapshot().Scope == 300 && scene->GetUISnapshot().Modal == 300,
          "Nested modal scope/order failed");
    f.CancelDown = true;
    route();
    Check(!scene->ConsumeUICancel(300) && scene->ConsumeUIEvent(900, event) &&
              event.Kind == UIEventKind::Cancel && event.Entity == 301 &&
              !scene->ConsumeUIEvent(900, event),
          "Semantic Cancel escaped/duplicated or lost context");
    f.CancelDown = false;
    route();
    Check(scene->RequestUIScreen(ScreenOperation::Close, 200), "Non-top Close rejected");
    scene->CommitUIScreens();
    route();
    Check(scene->GetUISnapshot().Scope == 300 && !scene->IsUIScreenOpen(200),
          "Closing non-top screen disturbed top");
    Check(scene->RequestUIScreen(ScreenOperation::Pop), "Pop rejected");
    scene->CommitUIScreens();
    route();
    Check(scene->GetUIDiagnostics().Focus == 102, "Close did not restore remembered invoker");
    // Reparented/disabled remembered focus must use the configured fallback, then
    // first.
    scene->RequestUIScreen(ScreenOperation::Open, 200);
    scene->CommitUIScreens();
    route();
    b.GetComponent<UIButtonComponent>().Interactable = false;
    f.CancelDown = true;
    route();
    scene->CommitUIScreens();
    f.CancelDown = false;
    route();
    Check(scene->GetUIDiagnostics().Focus == 101, "Ineligible return focus did not use fallback");
    // One stream: owner change, source retirement and native writes prune pending
    // delivery.
    f.AcceptDown = true;
    route();
    menu.GetComponent<UICanvasComponent>().Screen.EventOwner = 901;
    Check(!scene->ConsumeUIEvent(900, event) && !scene->ConsumeUIEvent(901, event),
          "Owner replacement inherited old records");
    f.AcceptDown = false;
    route();
    f.AcceptDown = true;
    route();
    a.GetComponent<UIRectComponent>().Visible = false;
    Check(!scene->ConsumeUIEvent(901, event), "Hidden source delivered event");
    a.GetComponent<UIRectComponent>().Visible = true;
    f.AcceptDown = false;
    route();
    f.AcceptDown = true;
    route();
    scene->DestroyEntity(other);
    Check(!scene->ConsumeUIEvent(901, event), "Retired owner received event");
    menu.GetComponent<UICanvasComponent>().Screen.EventOwner = 900;
    f.AcceptDown = false;
    route();
    // A live owner UUID is insufficient after script-component/class replacement.
    owner.AddComponent<ScriptComponent>();
    route();
    route();
    f.Mouse = f.PressPosition = {25, 45};
    f.PointerPressed = f.LeftDown = true;
    route();
    Check(scene->IsUICapturingPointer(), "Owner replacement capture fixture did not capture");
    owner.GetComponent<ScriptComponent>().ClassName = "ChangedOwner";
    scene->RefreshUI();
    Check(!scene->IsUICapturingPointer(), "Native owner class replacement retained capture");
    f.PointerPressed = f.LeftDown = false;
    f.PointerReleased = f.AcceptDown = true;
    route();
    Check(!scene->ConsumeUIEvent(900, event),
          "Retired owner gesture transferred into replacement class");
    f.PointerReleased = f.AcceptDown = false;
    route();
    owner.GetComponent<ScriptComponent>().ClassName.clear();
    scene->RefreshUI();
    route();
    f.Mouse = {600, 450};
    // Native modal/nonmodal availability uses the same authority/list, never a
    // router-side second filter.
    settings.GetComponent<UICanvasComponent>().Modal = false;
    scene->RequestUIScreen(ScreenOperation::Open, 200);
    scene->CommitUIScreens();
    route();
    Check(scene->GetUISnapshot().Scope == 200 && scene->GetUISnapshot().Modal == 100 &&
              !scene->GetUISnapshot().Find(101)->FocusEligible,
          "Nonmodal overlay escaped underlying modal boundary");
    settings.GetComponent<UICanvasComponent>().Enabled = false;
    route();
    Check(scene->GetUISnapshot().Scope == 100, "Unavailable screen retained interaction scope");
    settings.GetComponent<UICanvasComponent>().Enabled = true;
    route();
    // Ordered contradictory requests are FIFO; a stale queued target cannot
    // affect its replacement.
    scene->RequestUIScreen(ScreenOperation::Close, 200);
    scene->RequestUIScreen(ScreenOperation::Open, 200);
    scene->RequestUIScreen(ScreenOperation::Close, 200);
    scene->CommitUIScreens();
    route();
    Check(!scene->IsUIScreenOpen(200), "Conflicting requests were not FIFO");
    scene->RequestUIScreen(ScreenOperation::Open, 200);
    settings.RemoveComponent<UICanvasComponent>();
    {
        auto &replacement = settings.AddComponent<UICanvasComponent>();
        replacement.Screen.Managed = true;
        replacement.Screen.InitiallyOpen = false;
    }
    scene->CommitUIScreens();
    Check(!scene->IsUIScreenOpen(200) || scene->GetUISnapshot().Scope != 200,
          "Retired request affected replacement Canvas");
    // Queue pressure is bounded and recovers after a normal boundary.
    for (size_t i = 0; i < RuntimeUI::MaxScreenRequests; ++i)
        Check(scene->RequestUIScreen(ScreenOperation::Open, 100),
              "Bounded queue prematurely rejected");
    Check(!scene->RequestUIScreen(ScreenOperation::Open, 100),
          "Screen request overflow admitted newest");
    scene->CommitUIScreens();
    Check(scene->GetUIDiagnostics().PendingScreens == 0, "Screen queue did not recover");
    scene->RequestUIScreen(ScreenOperation::Replace, 300);
    scene->CommitUIScreens();
    route();
    Check(scene->GetUISnapshot().Scope == 300 && !scene->IsUIScreenOpen(100),
          "Replace did not replace the top screen");
    scene->RequestUIScreen(ScreenOperation::Open, 100);
    scene->CommitUIScreens();
    route();
    auto repeatedGeneration = scene->GetUISnapshot().Generation;
    scene->RequestUIScreen(ScreenOperation::Replace, 300);
    scene->CommitUIScreens();
    route();
    Check(scene->GetUISnapshot().Scope == 100 && scene->IsUIScreenOpen(300),
          "Replace with an already-open lower target partially changed order");
    scene->RequestUIScreen(ScreenOperation::Open, 100);
    scene->CommitUIScreens();
    route();
    Check(scene->GetUISnapshot().Generation == repeatedGeneration,
          "Repeated Open rebuilt/reset the screen");
    scene->ResetUI();
    Check(scene->GetUISnapshot().Scope==0&&scene->GetUISnapshot().Screens.empty(),"Reset retained published screen identities");
    route();
    Check(!scene->ConsumeUIEvent(900, event), "Reset/domain retirement retained scoped events");
    scene->OnRuntimeStop();
    // Classification changes keep one delivery stream, even with an unchanged modal UUID.
    {
        auto world = CreateRef<Scene>();
        world->OnViewportResize(400, 400);
        auto root = world->CreateEntityWithUUID(1, "Canvas");
        auto &canvas = root.AddComponent<UICanvasComponent>();
        canvas.ReferenceSize = {400, 400};
        canvas.SafeInsets = {};
        canvas.Modal = true;
        world->CreateEntityWithUUID(3, "Owner");
        auto button = world->CreateEntityWithUUID(2, "Button");
        button.AddComponent<UIRectComponent>().FixedSize = {100, 40};
        button.AddComponent<UIButtonComponent>();
        world->Reparent(button, root, TransformPolicy::KeepLocal);
        world->OnRuntimeStart();
        InputFrame input;
        input.Enabled = true;
        input.Mouse = {10, 10};
        world->UpdateUIInput(input);
        input.AcceptDown = true;
        world->UpdateUIInput(input);
        canvas.Screen.Managed = true;
        canvas.Screen.EventOwner = 3;
        Check(!world->ConsumeUIAction(0, event) && !world->ConsumeUIEvent(3, event),
              "Unscoped event transferred into managed ownership");
        input.AcceptDown = false;
        world->UpdateUIInput(input);
        input.PointerPressed = input.LeftDown = true;
        input.PressPosition = input.Mouse;
        world->UpdateUIInput(input);
        Check(world->IsUICapturingPointer(), "Classification fixture did not capture");
        canvas.Screen.Managed = false;
        world->RefreshUI();
        input.PointerPressed = input.LeftDown = false;
        input.PointerReleased = input.AcceptDown = true;
        input.ReleasePosition = input.Mouse;
        input.HasReleasePosition = true;
        world->UpdateUIInput(input);
        Check(!world->ConsumeUIAction(0, event) && !world->ConsumeUIEvent(3, event),
              "Managed gesture transferred into unscoped delivery");
        world->OnRuntimeStop();
    }
    // Ordinary HUD/UI scopes retain their keyboard ownership across pointer/keyboard-opened
    // screens.
    for (bool keyboard : {false, true})
    {
        auto world = CreateRef<Scene>();
        world->OnViewportResize(400, 400);
        auto base = world->CreateEntityWithUUID(10, "Ordinary canvas");
        auto &bc = base.AddComponent<UICanvasComponent>();
        bc.ReferenceSize = {400, 400};
        bc.SafeInsets = {};
        auto button = world->CreateEntityWithUUID(11, "Invoker");
        button.AddComponent<UIRectComponent>().FixedSize = {100, 40};
        button.AddComponent<UIButtonComponent>();
        world->Reparent(button, base, TransformPolicy::KeepLocal);
        auto modal = world->CreateEntityWithUUID(20, "Empty closeable modal");
        auto &mc = modal.AddComponent<UICanvasComponent>();
        mc.Screen.Managed = true;
        mc.Screen.InitiallyOpen = false;
        mc.Modal = true;
        world->OnRuntimeStart();
        InputFrame input;
        input.Enabled = true;
        input.Mouse = {300, 300};
        world->UpdateUIInput(input);
        if (keyboard)
        {
            input.FocusNavigation = input.NextPressed = true;
            world->UpdateUIInput(input);
            input.FocusNavigation = input.NextPressed = false;
            world->UpdateUIInput(input);
        }
        world->RequestUIScreen(ScreenOperation::Open, 20);
        world->CommitUIScreens();
        world->UpdateUIInput(input);
        Check(world->IsGameplayInputBlocked(), "Empty modal released gameplay");
        input.CancelDown = true;
        world->UpdateUIInput(input);
        world->CommitUIScreens();
        input.CancelDown = false;
        world->UpdateUIInput(input);
        Check(world->IsGameplayInputBlocked() == keyboard,
              "Closing screen changed underlying keyboard ownership");
        world->OnRuntimeStop();
    }
    auto excessive = CreateRef<Scene>();
    for (size_t i = 0; i <= RuntimeUI::MaxScreens; ++i)
        excessive->CreateEntity().AddComponent<UICanvasComponent>().Screen.Managed = true;
    bool limited = false;
    try
    {
        excessive->ValidateUI(false);
    }
    catch (const std::runtime_error &error)
    {
        limited = std::string(error.what()).find("64") != std::string::npos;
    }
    Check(limited, "Declared screen cap not validated");
    // Persistence/copy/duplicate remap of internal references; runtime stack
    // never serializes.
    auto document = CreateRef<Scene>();
    auto canvas = document->CreateEntityWithUUID(700, "Prefab screen");
    auto &policy = canvas.AddComponent<UICanvasComponent>().Screen;
    policy.Managed = true;
    auto target = document->CreateEntityWithUUID(701, "Owner and button");
    target.AddComponent<UIRectComponent>();
    target.AddComponent<UIButtonComponent>();
    document->Reparent(target, canvas, TransformPolicy::KeepLocal);
    policy.EventOwner = policy.InitialFocus = policy.DefaultAction = policy.CancelTarget = 701;
    auto copy = Scene::Copy(document);
    Check(SceneSerializer(copy).CaptureAuthoredState() ==
              SceneSerializer(document).CaptureAuthoredState(),
          "Screen policy copy lost authored state");
    auto duplicated = document->DuplicateEntity(canvas);
    auto duplicateID = document->GetChildrenView(duplicated.GetUUID()).front();
    auto &remapped = duplicated.GetComponent<UICanvasComponent>().Screen;
    Check(remapped.EventOwner == uint64_t(duplicateID) &&
              remapped.InitialFocus == uint64_t(duplicateID) &&
              remapped.DefaultAction == uint64_t(duplicateID),
          "Screen duplicate references were not remapped");
    auto text = SceneSerializer(document, root).SerializeText();
    auto reopened = CreateRef<Scene>();
    Check(SceneSerializer(reopened, root).DeserializeText(text), "Screen roundtrip rejected");
    Check(SceneSerializer(reopened).CaptureAuthoredState() ==
              SceneSerializer(document).CaptureAuthoredState(),
          "Screen save/reopen changed state");
    Prefab::Save(root, "screen.hprefab", document, canvas, true);
    auto prefab = Prefab::Load(root, "screen.hprefab", true, ResourceLoading::MetadataOnly);
    auto instance = document->InstantiateEntity(Prefab::GetEntity(prefab), {});
    Check(instance.GetComponent<UICanvasComponent>().Screen.EventOwner ==
              uint64_t(document->GetChildrenView(instance.GetUUID()).front()),
          "Screen prefab reference remap failed");
    std::cout << "PASS: scene screen order/focus/Cancel, owner-only stream, "
                 "native eligibility, transitions, bounded requests and "
                 "reference roundtrips\n";
}
