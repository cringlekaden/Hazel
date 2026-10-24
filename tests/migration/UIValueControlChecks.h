#include "Hazel/UI/RuntimeUIConfig.h"
// Value behavior uses the real scene snapshot/router/owner queue; no hardware polling.
static void UIValueControlChecks(const std::filesystem::path &root)
{
    using namespace GameUI;
    auto scene = CreateRef<Scene>();
    scene->OnViewportResize(400, 300);
    auto owner = scene->CreateEntityWithUUID(900, "Owner"),
         other = scene->CreateEntityWithUUID(901, "Other");
    auto canvas = scene->CreateEntityWithUUID(100, "Controls");
    canvas.AddComponent<UICanvasComponent>();
    canvas.GetComponent<UICanvasComponent>().ReferenceSize = {400, 300};
    canvas.GetComponent<UICanvasComponent>().SafeInsets = {};
    canvas.GetComponent<UICanvasComponent>().Modal = true;
    canvas.GetComponent<UICanvasComponent>().Screen.Managed = true;
    canvas.GetComponent<UICanvasComponent>().Screen.Cancel = ScreenCancel::Ignore;
    canvas.GetComponent<UICanvasComponent>().Screen.EventOwner = 900;
    auto rect = [&](uint64_t id, Entity parent, glm::vec2 position, glm::vec2 size) {
        auto e = scene->CreateEntityWithUUID(id, "Control " + std::to_string(id));
        scene->Reparent(e, parent, TransformPolicy::KeepLocal);
        auto &r = e.AddComponent<UIRectComponent>();
        r.Offset = position;
        r.FixedSize = size;
        return e;
    };
    auto toggle = rect(101, canvas, {10, 10}, {160, 40});
    toggle.AddComponent<UIButtonComponent>();
    toggle.GetComponent<UIButtonComponent>().Kind = ControlKind::Toggle;
    toggle.GetComponent<UIButtonComponent>().Action = "music";
    toggle.AddComponent<SpriteRendererComponent>(glm::vec4(.2f, .2f, .2f, 1));
    auto indicator = rect(102, toggle, {8, 8}, {20, 20});
    indicator.AddComponent<SpriteRendererComponent>(glm::vec4(1, 1, 1, 1));
    toggle.GetComponent<UIButtonComponent>().Indicator = 102;
    auto group = rect(200, canvas, {10, 70}, {260, 110});
    group.AddComponent<UIRadioGroupComponent>();
    group.GetComponent<UIRadioGroupComponent>().InitialSelection = 201;
    auto a = rect(201, group, {0, 0}, {80, 40}), b = rect(202, group, {90, 0}, {80, 40}),
         d = rect(203, group, {180, 0}, {80, 40});
    for (auto option : {a, b, d})
    {
        auto &o = option.AddComponent<UIButtonComponent>();
        o.Kind = ControlKind::Radio;
        o.Group = 200;
        o.TabOrder = int(uint64_t(option.GetUUID()) - 200);
    }
    auto progress = rect(300, canvas, {10, 220}, {200, 40});
    progress.AddComponent<SpriteRendererComponent>(glm::vec4(.2f, .2f, .2f, 1));
    progress.GetComponent<UIRectComponent>().Padding = glm::vec4(5);
    auto fill = rect(301, progress, {-10, -10}, {300, 90});
    fill.AddComponent<SpriteRendererComponent>(glm::vec4(1, 0, 0, 1));
    progress.AddComponent<UIProgressComponent>();
    progress.GetComponent<UIProgressComponent>().Fill = 301;
    progress.GetComponent<UIProgressComponent>().InitialValue = .5f;
    scene->ValidateUI(false);
    scene->PrepareSprites(true);
    auto authored = SceneSerializer(scene).CaptureAuthoredState();
    scene->OnRuntimeStart();
    scene->RefreshUI();
    InputFrame f;
    f.Enabled = true;
    f.Mouse = {390, 290};
    auto route = [&] { scene->UpdateUIInput(f); };
    route();
    auto accept = [&](uint64_t id) {
        f.AcceptDown = false;
        route();
        Check(scene->FocusUI(id), "Value focus failed");
        f.AcceptDown = true;
        route();
    };
    Action event;
    Check(!scene->IsUIChecked(101) && scene->GetUISelection(200) == 201, "Initial values failed");
    auto generation = scene->GetUISnapshot().Generation;
    accept(101);
    route();
    Check(scene->IsUIChecked(101) && !scene->ConsumeUIAction(101, event) &&
              !scene->ConsumeUIEvent(901, event) && scene->ConsumeUIEvent(900, event),
          "Toggle once-only exclusive delivery failed");
    Check(event.Kind == UIEventKind::ToggleChanged && !event.PreviousChecked && event.Checked &&
              event.Entity == 101 && event.Screen == 100 && event.Name == "music" &&
              !scene->ConsumeUIEvent(900, event),
          "Toggle typed payload/held semantics failed");
    Check(scene->GetUISnapshot().Generation == generation,
          "Runtime value invalidated unchanged layout");
    scene->SetUIChecked(101, false);
    Check(!scene->IsUIChecked(101) && !scene->ConsumeUIEvent(900, event),
          "Silent setter emitted an event");
    accept(202);
    Check(scene->GetUISelection(200) == 202 && scene->ConsumeUIEvent(900, event) &&
              event.Kind == UIEventKind::SelectionChanged && event.Entity == 200 &&
              event.PreviousSelection == 201 && event.Selection == 202,
          "Radio authoritative selection/payload failed");
    accept(202);
    Check(!scene->ConsumeUIEvent(900, event), "Selected radio emitted another change");
    f.AcceptDown = false;
    route();
    scene->FocusUI(201);
    Check(scene->GetUISelection(200) == 202, "Focus changed radio selection");
    scene->SetUISelection(200, 203);
    Check(scene->IsUIChecked(203) && !scene->IsUIChecked(202) && !scene->ConsumeUIEvent(900, event),
          "Programmatic selection created conflicting values/event");
    bool rejected = false;
    try
    {
        scene->SetUISelection(200, 0);
    }
    catch (...)
    {
        rejected = true;
    }
    Check(rejected && scene->GetUISelection(200) == 203, "Required clear not rejected atomically");
    Check(SceneSerializer(scene).CaptureAuthoredState() == authored,
          "Runtime value/focus changes rewrote authored defaults");
    d.GetComponent<UIRectComponent>().Visible = false;
    scene->RefreshUI();
    Check(scene->GetUISelection(200) == 201, "Hidden required selection did not fall back");
    a.GetComponent<UIButtonComponent>().Interactable = false;
    scene->RefreshUI();
    Check(scene->GetUISelection(200) == 202, "Disabled required selection did not fall back");
    scene->Reparent(b, canvas, TransformPolicy::KeepLocal);
    scene->OnUpdateRuntime(0);
    scene->RefreshUI();
    Check(scene->GetUISelection(200) == 0, "Reparented option remained a valid required member");
    scene->Reparent(b, group, TransformPolicy::KeepLocal);
    scene->OnUpdateRuntime(0);
    scene->RefreshUI();
    Check(scene->GetUISelection(200) == 202, "Required group did not recover eligible selection");
    group.GetComponent<UIRadioGroupComponent>().Required = false;
    scene->RefreshUI();
    scene->SetUISelection(200, 0);
    Check(!scene->GetUISelection(200), "Optional clear failed");
    a.GetComponent<UIButtonComponent>().Interactable = true;
    d.GetComponent<UIRectComponent>().Visible = true;
    // Source text and authored values remain intact; no event ever encodes values in its action.
    for (int i = 0; i < 64; ++i)
        accept(101);
    bool atCapacity = scene->IsUIChecked(101);
    accept(101);
    Check(scene->IsUIChecked(101) == atCapacity && scene->GetUIDiagnostics().DroppedActions >= 1,
          "Full owner queue changed value without its event");
    size_t count = 0;
    uint64_t sequence = 0;
    bool previous = false;
    while (scene->ConsumeUIEvent(900, event))
    {
        Check(event.Kind == UIEventKind::ToggleChanged && event.Sequence > sequence &&
                  event.PreviousChecked == previous && event.Checked != previous,
              "Value event order/payload lost");
        sequence = event.Sequence;
        previous = event.Checked;
        ++count;
    }
    Check(count == 64, "Capacity changed queue cardinality");
    f.AcceptDown = false;
    route();
    Check(scene->FocusUI(101), "Ordered value focus failed");
    bool orderedStart = scene->IsUIChecked(101);
    f.Events = {{UIAction::Accept, true, {}, InputDevice::Keyboard},
                {UIAction::Accept, false, {}, InputDevice::Keyboard},
                {UIAction::Accept, true, {}, InputDevice::Keyboard},
                {UIAction::Accept, false, {}, InputDevice::Keyboard}};
    route();
    f.Events.clear();
    Check(scene->IsUIChecked(101) == orderedStart && scene->ConsumeUIEvent(900, event) &&
              event.PreviousChecked == orderedStart && event.Checked != orderedStart,
          "First ordered value change missing");
    Check(scene->ConsumeUIEvent(900, event) && event.PreviousChecked != orderedStart &&
              event.Checked == orderedStart && !scene->ConsumeUIEvent(900, event),
          "Multiple changes in one sample coalesced/lost payload order");
    accept(101);
    canvas.GetComponent<UICanvasComponent>().Screen.EventOwner = 901;
    scene->RefreshUI();
    Check(!scene->ConsumeUIEvent(900, event) && !scene->ConsumeUIEvent(901, event),
          "Changed owner inherited an old value event");
    f.AcceptDown = false;
    route();
    route();
    canvas.GetComponent<UICanvasComponent>().Screen.EventOwner = 900;
    route();
    route();
    f.AcceptDown = false;
    route();
    scene->FocusUI(101);
    bool beforeCancel = scene->IsUIChecked(101);
    f.Events = {{UIAction::Accept, true, {}, InputDevice::Keyboard},
                {UIAction::Cancel, true, {}, InputDevice::Keyboard},
                {UIAction::Accept, false, {}, InputDevice::Keyboard},
                {UIAction::Cancel, false, {}, InputDevice::Keyboard}};
    route();
    f.Events.clear();
    Check(scene->IsUIChecked(101) == beforeCancel && !scene->ConsumeUIEvent(900, event),
          "Cancel lost priority over a value change");
    auto overlay = scene->CreateEntityWithUUID(1000, "Modal overlay");
    auto &oc = overlay.AddComponent<UICanvasComponent>();
    oc.ReferenceSize = {400, 300};
    oc.SafeInsets = {};
    oc.Modal = true;
    oc.Screen.Managed = true;
    oc.Screen.InitiallyOpen = false;
    oc.Screen.EventOwner = 900;
    route();
    accept(101);
    Check(scene->RequestUIScreen(ScreenOperation::Open, 1000), "Value modal request failed");
    scene->CommitUIScreens();
    Check(!scene->ConsumeUIEvent(900, event) && !scene->GetUISnapshot().Find(101)->FocusEligible,
          "Value event/eligibility escaped new modal scope");
    Check(scene->RequestUIScreen(ScreenOperation::Close, 1000), "Value modal close failed");
    scene->CommitUIScreens();
    f.AcceptDown = false;
    route();
    route();
    // Pointer press/release changes once, including decoration hit area; disabled and clips cannot
    // change.
    f.Mouse = {25, 25};
    f.LeftDown = true;
    f.PointerPressed = true;
    f.PressPosition = f.Mouse;
    route();
    f.PointerPressed = false;
    f.LeftDown = false;
    f.PointerReleased = true;
    f.ReleasePosition = f.Mouse;
    f.HasReleasePosition = true;
    bool before = scene->IsUIChecked(101);
    route();
    Check(scene->IsUIChecked(101) != before, "Pointer release failed to toggle through indicator");
    f.PointerReleased = false;
    route();
    Check(scene->ConsumeUIEvent(900, event) && !scene->ConsumeUIEvent(900, event),
          "Pointer gesture duplicated value delivery");
    toggle.GetComponent<UIButtonComponent>().Interactable = false;
    route();
    Check(!scene->GetUISnapshot().Find(101)->FocusEligible, "Disabled toggle stayed eligible");
    toggle.GetComponent<UIButtonComponent>().Interactable = true;
    toggle.GetComponent<UIRectComponent>().Offset = {500, 0};
    scene->RefreshUI();
    Check(!scene->GetUISnapshot().Find(101)->FocusEligible, "Fully clipped toggle stayed eligible");
    toggle.GetComponent<UIRectComponent>().Offset = {10, 10};
    scene->RefreshUI();
    scene->SetUIProgress(300, .25f);
    Rect bounds;
    bool visible = false;
    Check(scene->GetUIControlVisual(301, bounds, visible) && visible &&
              bounds.Position == glm::vec2(15, 225) && bounds.Size == glm::vec2(47.5f, 30),
          "Progress derived bounds exceeded padded content");
    progress.GetComponent<UIProgressComponent>().Direction = FillDirection::Left;
    scene->RefreshUI();
    Check(scene->GetUIControlVisual(301, bounds, visible) && bounds.Position == glm::vec2(157.5f, 225),
          "Reverse fill anchored wrong edge");
    scene->SetUIProgress(300, 5);
    Check(scene->GetUIProgress(300) == 1, "Progress did not clamp high");
    scene->SetUIProgress(300, -5);
    Check(scene->GetUIProgress(300) == 0 && scene->GetUIControlVisual(301, bounds, visible) && !visible,
          "Zero progress produced geometry");
    scene->SetUIProgress(300, .5f);
    rejected = false;
    try
    {
        scene->SetUIProgress(300, std::numeric_limits<float>::quiet_NaN());
    }
    catch (...)
    {
        rejected = true;
    }
    Check(rejected && scene->GetUIProgress(300) == .5f, "NaN progress changed accepted value");
    progress.GetComponent<UIProgressComponent>().Maximum =
        progress.GetComponent<UIProgressComponent>().Minimum;
    scene->RefreshUI();
    Check(scene->GetUIProgress(300) == .5f && !scene->GetUIDiagnostics().ControlProblem.empty(),
          "Invalid native range did not retain/diagnose accepted state");
    progress.GetComponent<UIProgressComponent>().Maximum = 1;
    scene->RefreshUI();
    auto rangeBefore = progress.GetComponent<UIProgressComponent>();
    progress.GetComponent<UIProgressComponent>().Minimum = -2;
    progress.GetComponent<UIProgressComponent>().Maximum = 2;
    progress.GetComponent<UIProgressComponent>().InitialValue = 0;
    scene->RefreshUI();
    scene->SetUIProgress(300, -1);
    Check(scene->GetUIControlVisual(301, bounds, visible) && bounds.Size.x == 47.5f,
          "Signed range normalization failed");
    progress.GetComponent<UIProgressComponent>() = rangeBefore;
    scene->RefreshUI();
    for (auto malformed :
         {"{Minimum: 1, Maximum: 1}", "{Minimum: .nan}", "{Direction: 4}", "{Minimum: 0, Minimum: 1}"})
    {
        bool failed = false;
        try
        {
            (void)ReadUIProgressComponent(YAML::Load(malformed));
        }
        catch (...)
        {
            failed = true;
        }
        Check(failed, "Malformed progress descriptor was accepted");
    }
    rejected = false;
    try
    {
        (void)ReadUIRadioGroupComponent(YAML::Load("{Required: true, Required: false}"));
    }
    catch (...)
    {
        rejected = true;
    }
    Check(rejected, "Duplicate group fields were accepted");
    // Targeted GPU checks: composed indicator visibility and fill picking, no extra focus targets.
    auto target =
        Framebuffer::Create({400,
                             300,
                             {{FramebufferTextureFormat::RGBA8, FramebufferTextureFormat::RED_INTEGER,
                               FramebufferTextureFormat::Depth}}});
    auto draw = [&] {
        target->Bind();
        RenderCommand::SetClearColor({0, 0, 0, 1});
        RenderCommand::Clear();
        target->ClearAttachment(1, -1);
        scene->OnUpdateRuntime(0);
    };
    scene->SetUIChecked(101, false);
    draw();
    Check(target->ReadPixel(1, 25, 300 - 25) == int(uint32_t(toggle)),
          "Unchecked decorative indicator painted/picked");
    scene->SetUIChecked(101, true);
    draw();
    Check(target->ReadPixel(1, 25, 300 - 25) == int(uint32_t(indicator)),
          "Checked non-color cue absent");
    scene->SetUIProgress(300, .5f);
    progress.GetComponent<UIProgressComponent>().Direction = FillDirection::Right;
    draw();
    Check(target->ReadPixel(1, 25, 300 - 235) == int(uint32_t(fill)) &&
              target->ReadPixel(1, 180, 300 - 235) == int(uint32_t(progress)),
          "Progress fill/picking escaped normalized geometry");
    Check(scene->GetUISnapshot().Find(300)->Hit == HitPolicy::PassThrough &&
              scene->GetUISnapshot().Find(301)->Hit == HitPolicy::PassThrough &&
              scene->GetUISnapshot().Find(102)->Hit == HitPolicy::PassThrough,
          "Decorations/progress intercepted navigation/input");
    toggle.GetComponent<UIButtonComponent>().Interactable = false;
    scene->RefreshUI();
    draw();
    Check(target->ReadPixel(1, 25, 300 - 25) == int(uint32_t(indicator)),
          "Disabled checked cue disappeared");
    toggle.GetComponent<UIButtonComponent>().Interactable = true;
    progress.GetComponent<UIProgressComponent>().Direction = FillDirection::Down;
    scene->RefreshUI();
    scene->SetUIProgress(300, .25f);
    Check(scene->GetUIControlVisual(301, bounds, visible) && bounds.Position == glm::vec2(15, 225) &&
              bounds.Size == glm::vec2(190, 7.5f),
          "Vertical fill derived wrong axis");
    progress.GetComponent<UIProgressComponent>().Direction = FillDirection::Up;
    scene->RefreshUI();
    Check(scene->GetUIControlVisual(301, bounds, visible) && bounds.Position == glm::vec2(15, 247.5f),
          "Reverse vertical fill moved wrong edge");
    target->Unbind();
    // Native initial mutations, source retirement, missing decoration diagnostics and recovery.
    accept(101);
    auto storedToggle = toggle.GetComponent<UIButtonComponent>();
    toggle.RemoveComponent<UIButtonComponent>();
    Check(!scene->ConsumeUIEvent(900, event), "Retired source delivered a value event");
    toggle.AddComponent<UIButtonComponent>(storedToggle);
    auto &replacement = toggle.GetComponent<UIButtonComponent>();
    replacement.Kind = ControlKind::Toggle;
    replacement.Indicator = 999;
    replacement.InitiallyChecked = true;
    scene->RefreshUI();
    Check(scene->IsUIChecked(101) && !scene->GetUIDiagnostics().ControlProblem.empty(),
          "Native default/replaced component or broken visual diagnostics stale");
    replacement.Indicator = 102;
    scene->RefreshUI();
    // Canonical save/reopen, exact comparison, copy, duplicate and prefab remapping include all
    // authored fields.
    scene->OnRuntimeStop();
    canvas.GetComponent<UICanvasComponent>().Screen.EventOwner = 900;
    toggle.GetComponent<UIRectComponent>().Offset = {10, 10};
    progress.GetComponent<UIProgressComponent>().Direction = FillDirection::Up;
    auto expected = SceneSerializer(scene).CaptureAuthoredState();
    auto path = root / "value-controls.hazel";
    SceneSerializer(scene).Serialize(path.generic_u8string());
    auto reopened = CreateRef<Scene>();
    Check(SceneSerializer(reopened).Deserialize(path.generic_u8string()), "Control reopen failed");
    Check(SceneSerializer(reopened).CaptureAuthoredState() == expected,
          "Control authored round trip lost data");
    auto broken = YAML::Load(SceneSerializer(scene).SerializeText());
    for (auto entity : broken["Entities"])
        if (entity["Entity"].as<uint64_t>() == 101)
            entity["UIButtonComponent"]["Indicator"] = uint64_t(999);
    YAML::Emitter brokenText;
    brokenText << broken;
    auto repairable = CreateRef<Scene>();
    SceneSerializer repairReader(repairable);
    Check(repairReader.DeserializeText(brokenText.c_str()) &&
              repairReader.Report().State == DocumentLoadState::EditableWithProblems &&
              repairable->GetEntityByUUID(101).GetComponent<UIButtonComponent>().Indicator == 999,
          "Broken control reference was lost or document became uneditable");
    bool strict = false;
    try
    {
        repairable->ValidateUI(false);
    }
    catch (...)
    {
        strict = true;
    }
    Check(strict, "Broken control reference passed strict startup/export validation");
    auto copy = Scene::Copy(scene);
    Check(SceneSerializer(copy).CaptureAuthoredState() == expected, "Control scene copy lost data");
    auto duplicate = scene->DuplicateEntity(group);
    auto selected = duplicate.GetComponent<UIRadioGroupComponent>().InitialSelection;
    Check(!selected || selected != group.GetComponent<UIRadioGroupComponent>().InitialSelection,
          "Radio duplicate did not remap initial selection");
    rejected = false;
    try
    {
        (void)Scene::ExtractSubtree(canvas);
    }
    catch (...)
    {
        rejected = true;
    }
    Check(rejected, "External prefab owner was silently retained");
    canvas.GetComponent<UICanvasComponent>().Screen.EventOwner = 100;
    auto detached = Scene::ExtractSubtree(canvas);
    detached->ValidateUI(false);
    Prefab::Save(root, "value-controls.hprefab", scene, canvas, true);
    auto prefab = Prefab::Load(root, "value-controls.hprefab");
    auto destination = CreateRef<Scene>();
    auto instance = destination->InstantiateEntity(Prefab::GetEntity(prefab), {});
    Check(instance && instance.GetComponent<UICanvasComponent>().Screen.EventOwner ==
                          uint64_t(instance.GetUUID()),
          "Prefab control/screen owner remapping failed");
    // Ordinary Canvas state uses the same owner-addressed stream, never button polling.
    canvas.GetComponent<UICanvasComponent>().Screen.Managed = false;
    toggle.GetComponent<UIButtonComponent>().EventOwner = 900;
    scene->OnRuntimeStart();
    f = {};
    f.Enabled = true;
    f.Mouse = {390, 290};
    route();
    accept(101);
    Check(scene->ConsumeUIEvent(900, event) && event.Screen == 0 &&
              event.Kind == UIEventKind::ToggleChanged && !scene->ConsumeUIAction(0, event),
          "Standalone value notification used a competing/unscoped queue");
    bool prior = scene->IsUIChecked(101);
    toggle.GetComponent<UIButtonComponent>().EventOwner = 999;
    accept(101);
    Check(scene->IsUIChecked(101) == prior && !scene->ConsumeUIEvent(900, event),
          "Missing standalone owner accepted a change without notification");
    toggle.GetComponent<UIButtonComponent>().EventOwner = 900;
    route();
    f.AcceptDown = false;
    route();
    route();
    f.AcceptDown = false;
    f.Mouse = {25, 25};
    f.PointerPressed = true;
    f.LeftDown = true;
    f.PressPosition = f.Mouse;
    route();
    f.PointerPressed = false;
    bool beforeOwnerChange = scene->IsUIChecked(101);
    toggle.GetComponent<UIButtonComponent>().EventOwner = 901;
    f.LeftDown = false;
    f.PointerReleased = true;
    f.ReleasePosition = f.Mouse;
    f.HasReleasePosition = true;
    route();
    f.PointerReleased = false;
    route();
    Check(scene->IsUIChecked(101) == beforeOwnerChange && !scene->ConsumeUIEvent(901, event),
          "Standalone owner replacement inherited an active pointer gesture");
    toggle.GetComponent<UIButtonComponent>().EventOwner = 900;
    route();
    // Equal authored values cannot hide component replacement from the transient-state initializer.
    auto same = toggle.GetComponent<UIButtonComponent>();
    scene->SetUIChecked(101, !same.InitiallyChecked);
    toggle.RemoveComponent<UIButtonComponent>();
    toggle.AddComponent<UIButtonComponent>(same);
    scene->RefreshUI();
    Check(scene->IsUIChecked(101) == same.InitiallyChecked,
          "Same-value component replacement reused retired transient state");
    scene->SetUIFramebufferSize({0, 0});
    scene->RefreshUI();
    Check(scene->GetUISnapshot().Buttons.empty(), "Minimized value controls kept routing entries");
    scene->SetUIFramebufferSize({400, 300});
    scene->RefreshUI();
    scene->OnRuntimeStop();
    std::cout << "PASS: composed toggle/radio/progress, typed admission/order/retirement, native "
                 "writes, derived GPU picking and persistence\n";
}
