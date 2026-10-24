// Real Scene snapshot, router, admitted owner stream and derived visual
// geometry.
static void UISliderSceneChecks(const std::filesystem::path &root)
{
    using namespace GameUI;
    auto scene = CreateRef<Scene>();
    scene->OnViewportResize(400, 300);
    auto owner = scene->CreateEntityWithUUID(900, "Slider owner");
    auto canvas = scene->CreateEntityWithUUID(100, "Slider scope");
    auto &c = canvas.AddComponent<UICanvasComponent>();
    c.ReferenceSize = {400, 300};
    c.SafeInsets = {};
    c.Modal = true;
    c.Screen.Managed = true;
    c.Screen.EventOwner = 900;
    c.Screen.Cancel = ScreenCancel::Close;
    auto node = [&](uint64_t id, Entity parent, glm::vec2 offset, glm::vec2 size) {
        auto e = scene->CreateEntityWithUUID(id, "Slider part");
        scene->Reparent(e, parent, TransformPolicy::KeepLocal);
        auto &r = e.AddComponent<UIRectComponent>();
        r.Offset = offset;
        r.FixedSize = size;
        e.AddComponent<SpriteRendererComponent>(glm::vec4(1));
        return e;
    };
    auto slider = node(101, canvas, {20, 20}, {200, 50});
    auto &b = slider.AddComponent<UIButtonComponent>();
    b.Kind = ControlKind::Slider;
    b.Action = "volume";
    auto track = node(102, slider, {10, 20}, {180, 10}),
         fill = node(103, slider, {10, 20}, {180, 10}), thumb = node(104, slider, {0, 0}, {20, 30});
    auto &s = slider.AddComponent<UISliderComponent>();
    s.Minimum = 0;
    s.Maximum = 10;
    s.Step = 3;
    s.InitialValue = 3;
    s.Track = 102;
    s.Fill = 103;
    s.Thumb = 104;
    auto toggle = node(200, canvas, {250, 20}, {100, 40});
    toggle.AddComponent<UIButtonComponent>().Kind = ControlKind::Toggle;
    scene->ValidateUI(false);
    auto authored = SceneSerializer(scene).CaptureAuthoredState();
    scene->OnRuntimeStart();
    scene->RefreshUI();
    InputFrame f;
    f.Enabled = true;
    f.Mouse = {390, 290};
    auto route = [&] { scene->UpdateUIInput(f); };
    route();
    Action event;
    auto drain = [&] {
        while (scene->ConsumeUIEvent(900, event))
        {
        }
    };
    auto key = [&](UIAction a) {
        f.Events = {{a, true}, {a, false}};
        route();
        f.Events.clear();
        route();
    };
    auto begin = [&] {
        f = {};
        f.Enabled = true;
        f.Mouse = {390, 290};
        route();
        Check(scene->FocusUI(101), "Slider focus failed");
        key(UIAction::Accept);
        Check(scene->IsUISliderEditing(), "Accept did not enter slider edit");
    };
    begin();
    auto generation = scene->GetUISnapshot().Generation;
    key(UIAction::AdjustIncrease);
    key(UIAction::AdjustIncrease);
    Check(scene->GetUISliderValue(101) == 9 && scene->GetUISnapshot().Generation == generation,
          "Adjust lost lattice or invalidated immutable geometry");
    Check(scene->ConsumeUIEvent(900, event) && event.Kind == UIEventKind::SliderChanged &&
              event.PreviousValue == 3 && event.Value == 9 && event.StartValue == 3 &&
              event.Gesture,
          "Live preview did not coalesce typed values");
    auto gesture = event.Gesture;
    key(UIAction::Cancel);
    Check(!scene->IsUISliderEditing() && scene->GetUISliderValue(101) == 3 &&
              scene->IsUIScreenOpen(100),
          "Cancel failed rollback or leaked to screen close");
    Check(scene->ConsumeUIEvent(900, event) && event.Kind == UIEventKind::SliderChanged &&
              event.PreviousValue == 9 && event.Value == 3,
          "Cancel restoration preview missing");
    Check(scene->ConsumeUIEvent(900, event) && event.Kind == UIEventKind::SliderCancelled &&
              event.Value == 3 && event.Gesture == gesture && !event.Interrupted &&
              !scene->ConsumeUIEvent(900, event),
          "Cancel terminal missing/duplicated");
    begin();
    key(UIAction::AdjustIncrease);
    scene->SetUISliderValue(101, 10);
    key(UIAction::Cancel);
    Check(scene->GetUISliderValue(101) == 10 && !scene->IsUISliderEditing(),
          "Stale rollback overwrote programmatic setter");
    Check(scene->ConsumeUIEvent(900, event) && event.Kind == UIEventKind::SliderChanged &&
              event.Value == 6,
          "Preview before programmatic interruption lost");
    Check(scene->ConsumeUIEvent(900, event) && event.Kind == UIEventKind::SliderCancelled &&
              event.Interrupted && event.Value == 6,
          "Programmatic interruption terminal missing");
    drain();
    // A pointer pickup continues the keyboard transaction, retaining one start/terminal.
    scene->SetUISliderValue(101, 3);
    begin();
    key(UIAction::AdjustIncrease);
    f.Mouse = {140, 45};
    f.PointerPressed = f.LeftDown = true;
    f.PressPosition = f.Mouse;
    route();
    f.PointerPressed = false;
    Check(scene->IsUISliderEditing() && scene->GetUIDiagnostics().SliderPointer &&
              scene->GetUISliderValue(101) == 6,
          "Pointer could not pick up keyboard edit without jumping");
    f.Mouse = {1000, 45};
    route();
    f.LeftDown = false;
    f.PointerReleased = f.HasReleasePosition = true;
    f.ReleasePosition = f.Mouse;
    route();
    f.PointerReleased = false;
    route();
    Check(scene->ConsumeUIEvent(900, event) && event.Kind == UIEventKind::SliderChanged &&
              event.PreviousValue == 3 && event.Value == 10,
          "Pickup lost shared preview/start");
    auto pickup = event.Gesture;
    Check(scene->ConsumeUIEvent(900, event) && event.Kind == UIEventKind::SliderCommitted &&
              event.StartValue == 3 && event.Gesture == pickup &&
              !scene->ConsumeUIEvent(900, event),
          "Pickup split or duplicated gesture terminal");
    // Track clicks map full travel. Press offset on thumb preserves value.
    scene->SetUISliderValue(101, 3);
    f = {};
    f.Enabled = true;
    f.Mouse = {88, 45};
    route();
    f.PointerPressed = f.LeftDown = true;
    f.PressPosition = f.Mouse;
    route();
    f.PointerPressed = false;
    Check(scene->IsUISliderEditing() && scene->GetUISliderValue(101) == 3,
          "Thumb offset changed value on press");
    f.Mouse = {1000, 45};
    route();
    Check(scene->GetUISliderValue(101) == 10 && scene->IsUICapturingPointer(),
          "Outside drag lost capture/clamp");
    f.LeftDown = false;
    f.PointerReleased = f.HasReleasePosition = true;
    f.ReleasePosition = f.Mouse;
    route();
    f.PointerReleased = false;
    route();
    Check(!scene->IsUISliderEditing() && !scene->IsUICapturingPointer() &&
              scene->ConsumeUIEvent(900, event) && event.Value == 10,
          "Outside release lost coherent preview");
    Check(scene->ConsumeUIEvent(900, event) && event.Kind == UIEventKind::SliderCommitted &&
              event.Value == 10,
          "Outside release did not commit");
    begin();
    key(UIAction::AdjustDecrease);
    track.GetComponent<UIRectComponent>().FixedSize.x = 160;
    scene->RefreshUI();
    Check(!scene->IsUISliderEditing() && scene->GetUISliderValue(101) == 9,
          "Geometry interruption rolled back live value");
    drain();
    begin();
    key(UIAction::AdjustDecrease);
    thumb.RemoveComponent<SpriteRendererComponent>();
    scene->RefreshUI();
    Check(!scene->IsUISliderEditing() && !scene->GetUIDiagnostics().ControlProblem.empty(),
          "Retired thumb left active edit/stale diagnostics");
    thumb.AddComponent<SpriteRendererComponent>(glm::vec4(1));
    scene->RefreshUI();
    drain();
    Check(scene->GetUIDiagnostics().ControlProblem.empty(), "Restored part stayed broken");
    // Clipping changes eligibility, never the full value range/travel.
    track.GetComponent<UIRectComponent>().FixedSize.x = 180;
    slider.GetComponent<UIRectComponent>().Offset.x = -100;
    scene->RefreshUI();
    scene->SetUISliderValue(101, 0);
    f = {};
    f.Enabled = true;
    f.Mouse = {0, 45};
    route();
    f.LeftDown = f.PointerPressed = true;
    f.PressPosition = f.Mouse;
    route();
    Check(scene->GetUISliderValue(101) == 6, "Clipped track shortened the value range");
    f.PointerPressed = false;
    f.CancelDown = true;
    route();
    Check(scene->GetUISliderValue(101) == 0 && !scene->IsUISliderEditing(),
          "Pointer Cancel did not restore start");
    f.LeftDown = false;
    f.CancelDown = false;
    f.AcceptDown = f.AcceptPressed = true;
    f.PointerReleased = true;
    f.ReleasePosition = {1000, 45};
    f.HasReleasePosition = true;
    route();
    Check(!scene->IsUISliderEditing(), "Cancelled release fell through to fresh Accept");
    f = {};
    f.Enabled = true;
    route();
    drain();
    slider.GetComponent<UIRectComponent>().Offset.x = 20;
    thumb.GetComponent<UIRectComponent>().FixedSize.x = 180;
    scene->RefreshUI();
    f.Mouse = {100, 45};
    route();
    f.PointerPressed = f.LeftDown = true;
    f.PressPosition = f.Mouse;
    route();
    Check(!scene->IsUISliderEditing() && !scene->IsUICapturingPointer(),
          "Zero-travel pointer edit captured");
    f = {};
    f.Enabled = true;
    route();
    thumb.GetComponent<UIRectComponent>().FixedSize.x = 20;
    scene->RefreshUI();
    // Reserve preview+terminal at admission; other controls cannot consume those
    // slots.
    f = {};
    f.Enabled = true;
    route();
    for (int i = 0; i < 62; ++i)
    {
        scene->FocusUI(200);
        key(UIAction::Accept);
    }
    begin();
    key(UIAction::AdjustIncrease);
    key(UIAction::Accept);
    Check(!scene->IsUISliderEditing(), "Queue pressure stranded slider edit");
    size_t count = 0;
    bool terminal = false;
    while (scene->ConsumeUIEvent(900, event))
    {
        ++count;
        terminal |= event.Kind == UIEventKind::SliderCommitted;
    }
    Check(count == 64 && terminal, "Reserved gesture events lost under pressure");
    // Admission refusal never starts a transaction, and inactive input retires queued terminals.
    for (int i = 0; i < 63; ++i)
    {
        scene->FocusUI(200);
        key(UIAction::Accept);
    }
    scene->FocusUI(101);
    float atPressure = scene->GetUISliderValue(101);
    key(UIAction::Accept);
    Check(!scene->IsUISliderEditing() && scene->GetUISliderValue(101) == atPressure,
          "63 pending records admitted an unreportable edit");
    drain();
    begin();
    key(UIAction::AdjustIncrease);
    key(UIAction::Accept);
    f.Enabled = false;
    route();
    Check(!scene->ConsumeUIEvent(900, event),
          "Ownership loss retained neutral-latched slider terminal");
    f.Enabled = true;
    route();
    begin();
    key(UIAction::AdjustDecrease);
    scene->SetUIFramebufferSize({600, 450});
    scene->RefreshUI();
    Check(!scene->IsUISliderEditing(), "Framebuffer scale change retained stale drag mapping");
    drain();
    scene->SetUIFramebufferSize({400, 300});
    scene->RefreshUI();
    begin();
    key(UIAction::AdjustDecrease);
    fill.GetComponent<UIRectComponent>().Visible = false;
    scene->RefreshUI();
    Check(!scene->IsUISliderEditing() && !scene->GetUIDiagnostics().ControlProblem.empty(),
          "Hidden assigned visual retained edit");
    fill.GetComponent<UIRectComponent>().Visible = true;
    scene->RefreshUI();
    drain();
    begin();
    key(UIAction::AdjustIncrease);
    canvas.GetComponent<UICanvasComponent>().Screen.EventOwner = 200;
    scene->RefreshUI();
    Check(!scene->IsUISliderEditing() && !scene->ConsumeUIEvent(900, event) &&
              !scene->ConsumeUIEvent(200, event),
          "Changed owner inherited an edit/event");
    canvas.GetComponent<UICanvasComponent>().Screen.EventOwner = 900;
    route();
    route();
    slider.GetComponent<UIButtonComponent>().Action = "";
    scene->RefreshUI();
    begin();
    key(UIAction::Accept);
    Check(scene->ConsumeUIEvent(900, event) && event.Kind == UIEventKind::SliderCommitted &&
              event.Name.empty(),
          "Empty optional slider action disabled keyboard editing");
    slider.GetComponent<UIButtonComponent>().Action = "volume";
    scene->RefreshUI();
    canvas.GetComponent<UICanvasComponent>().Screen.Managed = false;
    canvas.GetComponent<UICanvasComponent>().Modal = false;
    slider.GetComponent<UIButtonComponent>().EventOwner = 900;
    route();
    route();
    begin();
    f.Mouse = {390, 290};
    f.LeftDown = f.PointerPressed = true;
    f.PressPosition = f.Mouse;
    route();
    Check(!scene->IsUISliderEditing() && scene->IsGameplayInputBlocked(),
          "Outside press stranded nonmodal keyboard edit or leaked gesture");
    f.PointerPressed = false;
    route();
    Check(scene->IsGameplayInputBlocked(), "Interrupted press released gameplay before neutral");
    f.LeftDown = false;
    f.PointerReleased = true;
    route();
    f.PointerReleased = false;
    drain();
    route();
    Check(!scene->IsGameplayInputBlocked(),
          "Outside interruption failed to release nonmodal ownership after neutral");
    canvas.GetComponent<UICanvasComponent>().Screen.Managed = true;
    canvas.GetComponent<UICanvasComponent>().Modal = true;
    slider.GetComponent<UIButtonComponent>().EventOwner = 0;
    route();
    route();
    // Native invalid writes retain accepted range but a replacement component
    // cannot inherit it.
    s.Step = 0;
    scene->RefreshUI();
    Check(scene->GetUISliderValue(101) >= 0 && !scene->GetUIDiagnostics().ControlProblem.empty(),
          "Invalid native range replaced valid state");
    s.Step = 3;
    scene->RefreshUI();
    track.GetComponent<UIRectComponent>().FixedSize.x = 160;
    scene->RefreshUI();
    scene->SetUISliderValue(101, 6);
    Rect visual;
    bool visible = false;
    Check(scene->GetUIControlVisual(104, visual, visible) && visible &&
              std::abs(visual.Position.x - 114) < .001f,
          "Derived thumb used wrong travel");
    auto target = Framebuffer::Create(
        {400,
         300,
         {{FramebufferTextureFormat::RGBA8, FramebufferTextureFormat::RED_INTEGER,
           FramebufferTextureFormat::Depth}}});
    target->Bind();
    RenderCommand::SetClearColor({0, 0, 0, 1});
    RenderCommand::Clear();
    target->ClearAttachment(1, -1);
    scene->OnUpdateRuntime(0);
    Check(target->ReadPixel(1, 120, 300 - 45) == int(uint32_t(thumb)),
          "Derived thumb pixels/picking disagree");
    target->Unbind();
    // Persist only authored defaults. Copy/duplicate/prefab references remain
    // local.
    scene->OnRuntimeStop();
    track.GetComponent<UIRectComponent>().FixedSize.x = 180;
    Check(SceneSerializer(scene).CaptureAuthoredState() == authored,
          "Runtime edit dirtied authored slider");
    auto path = root / "sliders.hazel";
    SceneSerializer(scene).Serialize(path.generic_u8string());
    auto reopened = CreateRef<Scene>();
    Check(SceneSerializer(reopened).Deserialize(path.generic_u8string()) &&
              SceneSerializer(reopened).CaptureAuthoredState() == authored,
          "Slider save/reopen fields differ");
    auto copy = Scene::Copy(scene);
    Check(SceneSerializer(copy).CaptureAuthoredState() == authored, "Slider copy lost fields");
    Prefab::Save(root, "sliders.hprefab", scene, slider);
    auto prefab = Prefab::Load(root, "sliders.hprefab");
    auto destination = CreateRef<Scene>();
    auto instance = destination->InstantiateEntity(Prefab::GetEntity(prefab), {});
    Check(instance.GetComponent<UISliderComponent>().Thumb != s.Thumb &&
              destination
                      ->GetRelationship(destination->GetEntityByUUID(
                          instance.GetComponent<UISliderComponent>().Thumb))
                      .Parent == instance.GetUUID(),
          "Slider prefab remap failed");
    for (auto malformed : {"{Minimum: 1, Maximum: 1}", "{Step: .nan}", "{Step: 0}",
                           "{Direction: 4}", "{Step: 1, Step: 2}"})
    {
        bool failed = false;
        try
        {
            ReadUISliderComponent(YAML::Load(malformed));
        }
        catch (...)
        {
            failed = true;
        }
        Check(failed, "Malformed slider fields accepted");
    }
    auto duplicate = scene->DuplicateEntity(slider);
    auto &d = duplicate.GetComponent<UISliderComponent>();
    Check(d.Track != s.Track && d.Thumb != s.Thumb &&
              scene->GetRelationship(scene->GetEntityByUUID(d.Thumb)).Parent == duplicate.GetUUID(),
          "Slider duplication did not remap parts");
    scene->OnRuntimeStart();
    scene->RefreshUI();
    Check(scene->GetUISliderValue(101) == 3, "Repeated Play retained runtime value");
    scene->OnRuntimeStop();
    std::cout << "PASS: slider scene edits, capture, owner previews/terminals, "
                 "pressure, interruption, derived picking and persistence\n";
}
