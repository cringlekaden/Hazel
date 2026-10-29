#include "UIDropdownFixture.h"
static void UIDropdownSceneChecks(const std::filesystem::path &directory)
{
    auto scene = DropdownFixture();
    scene->ValidateUI(false);
    auto authored = SceneSerializer(scene).CaptureAuthoredState();
    auto target = Framebuffer::Create(
        {400,
         300,
         {{FramebufferTextureFormat::RGBA8, FramebufferTextureFormat::RED_INTEGER,
           FramebufferTextureFormat::Depth}}});
    auto draw = [&]
    {
        target->Bind();
        RenderCommand::SetClearColor({0, 0, 0, 1});
        RenderCommand::Clear();
        target->ClearAttachment(1, -1);
        scene->OnUpdateRuntime(0);
    };
    scene->OnRuntimeStart();
    InputFrame f;
    f.Enabled = true;
    f.Mouse = {20, 150};
    auto route = [&] { scene->UpdateUIInput(f); };
    auto neutral = [&]
    {
        f = {};
        f.Enabled = true;
        f.Mouse = {20, 150};
        route();
        route();
    };
    neutral();
    Check(scene->GetUISelection(200) == 311 && !scene->GetUISnapshot().Find(220)->LayoutResolved,
          "Closed dropdown still laid out its popup or lost initial value");
    draw();
    Check(scene->GetUITextLayout(210) && scene->GetUITextLayout(210)->Source == "Choice 12",
          "Closed label did not consume authoritative group selection");
    target->Unbind();
    auto open = [&]
    {
        Check(scene->RequestUIDropdown(200, true), "Dropdown open enqueue failed");
        scene->CommitUIScreens();
        scene->RefreshUI();
        neutral();
        Check(scene->IsUIDropdownOpen(200), "Dropdown failed to open");
    };
    auto close = [&]
    {
        Check(scene->RequestUIDropdown(200, false), "Dropdown close enqueue failed");
        scene->CommitUIScreens();
        scene->RefreshUI();
        neutral();
    };
    open();
    auto &snapshot = scene->GetUISnapshot();
    Check(snapshot.Scope == 220 && snapshot.Modal == 220 && snapshot.Find(311)->Scope == 220 &&
              snapshot.Find(311)->Canvas == 100 && snapshot.Find(220)->OverlayRoot,
          "Overlay scope/surface membership was conflated");
    Check(snapshot.Dropdowns.size() == 1 && snapshot.Dropdowns[0].Above &&
              snapshot.Find(220)->Bounds.Position == glm::vec2(220, 126),
          "Popup did not flip/clamp against visible trigger geometry");
    Check(scene->GetUIDiagnostics().Focus == 311 && scene->GetUIScroll(220).Offset.y == 272,
          "Opening did not reveal selected offscreen option");
    draw();
    auto n = scene->GetUISnapshot().Find(309);
    auto center = n->Bounds.Position + glm::vec2(40, 16);
    Check(center.y < 210 && target->ReadPixel(1, int(center.x), 300 - int(center.y)) ==
                                int(uint32_t(scene->GetEntityByUUID(309))),
          "Popup painting/picking inherited invoking ScrollView clip");
    target->Unbind();
    Action event;
    Check(!scene->ConsumeUIEvent(900, event), "Opening emitted a button/value event");
    // Shared production adapter: a fresh controller Accept chooses once,
    // navigation does not.
    UIInputAdapter adapter;
    DeviceSample sample;
    sample.Point = {20, 150};
    sample.Pad.Device = 0;
    scene->UpdateUIInput(adapter.Sample(sample, .016f, true));
    scene->UpdateUIInput(adapter.Sample(sample, .016f, true));
    Check(scene->FocusUI(310), "Visible option focus failed");
    sample.Pad.Buttons[size_t(GamepadButton::A)] = true;
    scene->UpdateUIInput(adapter.Sample(sample, .016f, true));
    Check(scene->GetUISelection(200) == 311,
          "Choice mutated before bounded close/publication commit");
    scene->CommitUIScreens();
    scene->RefreshUI();
    Check(!scene->IsUIDropdownOpen(200) && scene->GetUISelection(200) == 310 &&
              scene->GetUIDiagnostics().Focus == 210,
          "Choice/close/return focus transaction failed");
    Check(scene->ConsumeUIEvent(900, event) && event.Kind == UIEventKind::SelectionChanged &&
              event.Entity == 200 && event.PreviousSelection == 311 && event.Selection == 310 &&
              event.Screen == 100 && event.Owner == 900 && !scene->ConsumeUIEvent(900, event),
          "Selection event lost on intended close, duplicated or misaddressed");
    neutral();
    open();
    f.Events = {{UIAction::Accept, true}, {UIAction::Accept, false}};
    route();
    scene->CommitUIScreens();
    scene->RefreshUI();
    neutral();
    Check(!scene->IsUIDropdownOpen(200) && !scene->ConsumeUIEvent(900, event),
          "Same-choice activation emitted change or stayed open");
    scene->SetUISelection(200, 305);
    Check(!scene->ConsumeUIEvent(900, event), "Programmatic dropdown setter notified");
    open();
    Check(scene->GetUIDiagnostics().Focus == 305, "Silent selection was not revealed next open");
    f.Events = {{UIAction::Next, true}, {UIAction::Next, false}};
    route();
    scene->CommitUIScreens();
    scene->RefreshUI();
    neutral();
    Check(!scene->IsUIDropdownOpen(200) && scene->GetUISelection(200) == 305 &&
              scene->GetUIDiagnostics().Focus == 210,
          "Tab did not dismiss without selection");
    open();
    f.Events = {{UIAction::Cancel, true}, {UIAction::Cancel, false}};
    route();
    scene->CommitUIScreens();
    scene->RefreshUI();
    neutral();
    Check(!scene->IsUIDropdownOpen(200) && scene->GetUISelection(200) == 305,
          "Cancel changed selection or failed dismissal");
    open();
    f.Events = {{UIAction::Press, true, {20, 20}, InputDevice::Pointer}};
    f.LeftDown = true;
    route();
    scene->CommitUIScreens();
    scene->RefreshUI();
    f.Events = {{UIAction::Press, false, {20, 20}, InputDevice::Pointer}};
    f.LeftDown = false;
    route();
    neutral();
    Check(!scene->IsUIDropdownOpen(200) && !scene->IsUIChecked(400) &&
              !scene->ConsumeUIEvent(900, event),
          "Outside dismissal clicked through into underlying control");
    open();
    Check(scene->EnsureUIVisible(220, 304, false), "Pointer choice reveal failed");
    auto point = scene->GetUISnapshot().Find(304)->Bounds.Position + glm::vec2(40, 16);
    f.Events = {{UIAction::Press, true, point, InputDevice::Pointer},
                {UIAction::Press, false, point, InputDevice::Pointer}};
    route();
    scene->CommitUIScreens();
    scene->RefreshUI();
    neutral();
    Check(scene->GetUISelection(200) == 304 && !scene->IsUIDropdownOpen(200) &&
              scene->ConsumeUIEvent(900, event) && event.Selection == 304 &&
              !scene->ConsumeUIEvent(900, event),
          "Pointer option activation lost/duplicated selection");
    scene->SetUISelection(200, 305);
    Check(scene->FocusUI(400),"Programmatic invoker setup failed");open();close();
    Check(scene->GetUIDiagnostics().Focus==210,"Programmatic popup close did not return to its trigger");
    // Actual queue pressure rejects before reservation/open; no silent user value
    // acceptance.
    Check(scene->FocusUI(400), "Pressure control focus failed");
    for (int i = 0; i < 64; ++i)
    {
        f.Events = {{UIAction::Accept, true}, {UIAction::Accept, false}};
        route();
    }
    Check(scene->GetUIDiagnostics().PendingEvents == 64,
          "Pressure fixture did not fill owner stream");
    Check(scene->RequestUIDropdown(200, true), "Pressure request was not queued");
    scene->CommitUIScreens();
    scene->RefreshUI();
    Check(!scene->IsUIDropdownOpen(200) && scene->GetUISelection(200) == 305,
          "Full queue accepted unreported dropdown interaction");
    int count = 0;
    while (scene->ConsumeUIEvent(900, event))
        ++count;
    Check(count == 64, "Pressure retirement silently discarded parent events");
    neutral();
    // Native layout/label writes, fractional surfaces and parent scrolling use
    // the same snapshot.
    open();
    auto layouts = Font::Resources().LayoutBuilds;
    auto measurements = scene->GetUIDiagnostics().Measurements;
    scene->ScrollUITo(220, {0, 20});
    scene->ScrollUITo(220, {0, 40});
    Check(scene->GetUISnapshot().Screens.size() == 2,
          "Scroll publication duplicated owned screen records");
    Check(scene->GetUIDiagnostics().Measurements == measurements &&
              Font::Resources().LayoutBuilds == layouts,
          "Popup scrolling reshaped or remeasured unchanged text");
    scene->ScrollUITo(150, {0, 10});
    Check(scene->GetUISnapshot().Find(220)->Bounds.Position.y == 116,
          "Popup failed to follow invoking parent scroll");
    scene->SetUIFramebufferSize({500, 375});
    scene->RefreshUI();
    Check(scene->GetUISnapshot().Find(220)->FramebufferBounds.Position ==
              scene->GetUISnapshot().Find(220)->Bounds.Position * 1.25f,
          "Popup framebuffer conversion inconsistent");
    close();
    scene->GetEntityByUUID(305).GetComponent<UIButtonComponent>().Interactable = false;
    scene->RefreshUI();
    Check(scene->GetUISelection(200) == 300, "Required dropdown fallback not deterministic");
    scene->GetEntityByUUID(200).GetComponent<UIRadioGroupComponent>().Required = false;
    scene->RefreshUI();
    scene->SetUISelection(200, 0);
    Check(scene->GetUISelection(200) == 0, "Optional dropdown clear failed");
    scene->GetEntityByUUID(300).GetComponent<TextComponent>().TextString = "Updated label";
    scene->SetUISelection(200, 300);
    draw();
    Check(scene->GetUITextLayout(210)->Source == "Updated label",
          "Native option label write left stale closed text");
    target->Unbind();
    scene->OnRuntimeStop();
    SceneSerializer(scene).Serialize((directory / "dropdown.hazel").generic_u8string());
    auto loaded = CreateRef<Scene>();
    Check(SceneSerializer(loaded).Deserialize((directory / "dropdown.hazel").generic_u8string()) &&
              SceneSerializer(loaded).CaptureAuthoredState() ==
                  SceneSerializer(scene).CaptureAuthoredState(),
          "Dropdown serialization lost configuration");
    auto duplicated = scene->DuplicateEntity(scene->GetEntityByUUID(200));
    auto &d = duplicated.GetComponent<UIDropdownComponent>();
    Check(d.Trigger != 210 && d.Popup != 220, "Dropdown duplicate did not remap owned references");
    Prefab::Save(directory, "dropdown.hprefab", scene, scene->GetEntityByUUID(200));
    auto prefab = Prefab::Load(directory, "dropdown.hprefab");
    auto destination = CreateRef<Scene>();
    auto instance = destination->InstantiateEntity(Prefab::GetEntity(prefab), {});
    Check(instance.GetComponent<UIDropdownComponent>().Popup != 220,
          "Connected dropdown prefab remap failed");
    auto edit = DropdownFixture();
    auto before = SceneSerializer(edit).CaptureAuthoredState();
    edit->RefreshUI();
    edit->PreviewUIDropdown(200, true);
    Check(edit->IsUIDropdownOpen(200) && edit->GetUISnapshot().Scope == 220,
          "Native Edit popup preview failed");
    edit->ScrollUITo(220, {0, 80});
    edit->PreviewUIDropdown(200, false);
    Check(SceneSerializer(edit).CaptureAuthoredState() == before &&
              !edit->ConsumeUIEvent(900, event),
          "Transient dropdown preview dirtied authored state or dispatched events");
    // Lifecycle/adversarial cases use the same production snapshot/router stream.
    auto life = DropdownFixture();
    auto second = life->DuplicateEntity(life->GetEntityByUUID(200));
    const auto secondID = uint64_t(second.GetUUID());
    second.GetComponent<UIRectComponent>().Offset.y = 0;
    auto screen2 = life->CreateEntityWithUUID(800, "Temporary modal");
    auto &c2 = screen2.AddComponent<UICanvasComponent>();
    c2.Screen.Managed = true;
    c2.Screen.InitiallyOpen = false;
    c2.Screen.EventOwner = 900;
    c2.Modal = true;
    life->MoveSibling(life->GetEntityByUUID(311), -1);
    life->OnRuntimeStart();
    Check(life->GetUISelection(200) == 311, "Reordering changed stable option selection");
    auto ready = [&]
    {
        InputFrame n;
        n.Enabled = true;
        n.Mouse = {20, 150};
        life->UpdateUIInput(n);
        life->UpdateUIInput(n);
    };
    auto openLife = [&](uint64_t id = 200)
    {
        Check(life->RequestUIDropdown(id, true), "Lifecycle open request failed");
        life->CommitUIScreens();
        life->RefreshUI();
        ready();
    };
    auto choose = [&](uint64_t id)
    {
        Check(life->EnsureUIVisible(220, id, true), "Lifecycle option reveal failed");
        InputFrame n;
        n.Enabled = true;
        n.Events = {{UIAction::Accept, true}, {UIAction::Accept, false}};
        life->UpdateUIInput(n);
        life->CommitUIScreens();
        life->RefreshUI();
        ready();
    };
    ready();
    openLife();
    // Repeated opens do not reset option scroll/focus; another owner switches atomically.
    auto focus = life->GetUIDiagnostics().Focus;
    openLife();
    Check(life->GetUIDiagnostics().Focus == focus, "Repeated open reset popup focus");
    openLife(secondID);
    Check(!life->IsUIDropdownOpen(200) && life->IsUIDropdownOpen(secondID),
          "Second dropdown did not replace owned scope");
    Check(life->RequestUIDropdown(secondID, false), "Second popup close failed");
    life->CommitUIScreens();
    life->RefreshUI();
    ready();
    openLife();
    Check(life->RequestUIScreen(ScreenOperation::Open, 800), "Nested modal request failed");
    life->CommitUIScreens();
    life->RefreshUI();
    ready();
    Check(!life->IsUIDropdownOpen(200) && life->IsUIScreenOpen(800),
          "Screen transition retained child popup");
    life->RequestUIScreen(ScreenOperation::Close, 800);
    life->CommitUIScreens();
    life->RefreshUI();
    ready();
    openLife();
    Check(life->RequestUIDropdown(200, false), "Queued dismissal failed");
    InputFrame accept;
    accept.Enabled = true;
    accept.Events = {{UIAction::Accept, true}, {UIAction::Accept, false}};
    life->UpdateUIInput(accept);
    life->CommitUIScreens();
    life->RefreshUI();
    ready();
    Check(life->GetUISelection(200) == 311 && !life->ConsumeUIEvent(900, event),
          "Accept rewrote pending dismissal into selection");
    openLife();
    choose(310);
    // The historical chosen ID need not stay alive after delivery admission.
    life->DestroyEntity(life->GetEntityByUUID(310));
    life->OnUpdateRuntime(0);
    Check(life->ConsumeUIEvent(900, event) && event.Selection == 310,
          "Accepted choice incorrectly depended on historical option lifetime");
    openLife();
    choose(309);
    life->GetEntityByUUID(100).GetComponent<UICanvasComponent>().Screen.EventOwner = 800;
    life->RefreshUI();
    Check(!life->ConsumeUIEvent(900, event) && !life->ConsumeUIEvent(800, event),
          "New owner inherited old selection event");
    life->GetEntityByUUID(100).GetComponent<UICanvasComponent>().Screen.EventOwner = 900;
    life->RefreshUI();
    ready();
    openLife();
    life->GetEntityByUUID(220).GetComponent<UIRectComponent>().Visible = false;
    life->RefreshUI();
    ready();
    Check(!life->IsUIDropdownOpen(200), "Hidden popup retained active scope");
    life->GetEntityByUUID(220).GetComponent<UIRectComponent>().Visible = true;
    life->RefreshUI();
    ready();
    openLife();
    life->ScrollUITo(150, {0, 200});
    life->CommitUIScreens();
    life->RefreshUI();
    ready();
    Check(!life->IsUIDropdownOpen(200) && !life->GetUISnapshot().Find(220)->LayoutResolved,
          "Fully clipped trigger retained stale popup geometry");
    life->ScrollUITo(150, {});
    life->RefreshUI();
    ready();
    openLife();
    life->GetEntityByUUID(200).RemoveComponent<UIDropdownComponent>();
    life->RefreshUI();
    Check(life->GetUISnapshot().Scope == 100 && !life->ConsumeUIEvent(900, event),
          "Dropdown component retirement retained owned scope/events");
    life->OnRuntimeStop();
    auto retired = DropdownFixture();
    retired->OnRuntimeStart();
    auto neutralRetired = [&]
    {
        InputFrame n;
        n.Enabled = true;
        retired->UpdateUIInput(n);
        retired->UpdateUIInput(n);
    };
    neutralRetired();
    retired->RequestUIDropdown(200, true);
    retired->CommitUIScreens();
    retired->RefreshUI();
    neutralRetired();
    Check(retired->FocusUI(310), "Retirement choice focus failed");
    InputFrame edge;
    edge.Enabled = true;
    edge.Events = {{UIAction::Accept, true}, {UIAction::Accept, false}};
    retired->UpdateUIInput(edge);
    auto optionRect = retired->GetEntityByUUID(310).GetComponent<UIRectComponent>();
    retired->GetEntityByUUID(310).RemoveComponent<UIRectComponent>();
    retired->GetEntityByUUID(310).AddComponent<UIRectComponent>(optionRect);
    retired->CommitUIScreens();
    retired->RefreshUI();
    neutralRetired();
    Check(retired->GetUISelection(200) == 311 && !retired->ConsumeUIEvent(900, event),
          "Retired/replaced option Rect accepted an older gesture");
    retired->GetEntityByUUID(210).RemoveComponent<UIButtonComponent>();
    retired->CommitUIScreens();
    retired->RefreshUI();
    neutralRetired();
    auto generation = retired->GetUISnapshot().Generation;
    retired->RefreshUI();
    retired->RefreshUI();
    Check(!retired->IsUIDropdownOpen(200) && generation == retired->GetUISnapshot().Generation,
          "Broken trigger retained overlay or repeatedly rebuilt it");
    retired->OnRuntimeStop();
    std::cout << "PASS: owned dropdown composition, atomic selection/close delivery, "
                 "shared pad/actions, dismissal, queue reservation, detached "
                 "clipping/picking, scroll/resize, silent values and persistence\n";
}
