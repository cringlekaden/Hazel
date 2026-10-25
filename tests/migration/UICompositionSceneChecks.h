#include "UICompositionFixture.h"
static void UICompositionSceneChecks(const std::filesystem::path &directory)
{
    auto s = CompositionFixture();
    s->ValidateUI(false);
    s->OnRuntimeStart();
    InputFrame f;
    f.Enabled = true;
    f.Mouse = {399, 299};
    auto route = [&] { s->UpdateUIInput(f); };
    auto neutral = [&] {
        f = {};
        f.Enabled = true;
        f.Mouse = {399, 299};
        route();
        route();
    };
    neutral();
    auto accept = [&](uint64_t id) {
        Check(s->FocusUI(id), "Composition focus failed");
        f.Events = {{UIAction::Accept, true}, {UIAction::Accept, false}};
        route();
        f.Events.clear();
    };
    auto commit = [&] {
        s->CommitUIScreens();
        s->RefreshUI();
        neutral();
    };
    Check(s->GetUISelection(200) == 210 && s->GetUISnapshot().Find(221)->LayoutResolved &&
              !s->GetUISnapshot().Find(230)->LayoutResolved && !s->GetUISnapshot().Find(231)->FocusEligible,
          "Inactive page entered resolved paint/hit geometry");
    accept(400);
    accept(221);
    accept(211);
    Check(s->GetUISelection(200) == 210, "Tab changed during input iteration");
    commit();
    Check(s->GetUISelection(200) == 211 && !s->GetUISnapshot().Find(220)->LayoutResolved &&
              s->GetUIDiagnostics().Focus == 211,
          "Page switch did not preserve tab focus/collapse old content");
    Action event;
    Check(s->ConsumeUIEvent(900, event) && event.Entity == 400,
          "Page switch discarded unrelated owner event");
    Check(s->ConsumeUIEvent(900, event) && event.Kind == UIEventKind::SelectionChanged &&
              event.Entity == 200 && event.PreviousSelection == 210 && event.Selection == 211 &&
              !s->ConsumeUIEvent(900, event),
          "Departing event survived or tab selection event was lost/duplicated");
    Check(s->EnsureUIVisible(232, 231, true), "Page content reveal failed");
    s->SetUISelection(200, 210);
    commit();
    Check(s->GetUISelection(200) == 210 && s->GetUIDiagnostics().Focus == 221 &&
              !s->ConsumeUIEvent(900, event),
          "Silent page switch did not focus configured page target");
    s->SetUISelection(200, 211);
    commit();
    Check(s->GetUIDiagnostics().Focus == 231, "Remembered eligible page focus was not revealed/restored");
    auto framebuffer =
        Framebuffer::Create({400,
                             300,
                             {{FramebufferTextureFormat::RGBA8, FramebufferTextureFormat::RED_INTEGER,
                               FramebufferTextureFormat::Depth}}});
    auto draw = [&](float seconds) {
        framebuffer->Bind();
        RenderCommand::SetClearColor({0, 0, 0, 1});
        RenderCommand::Clear();
        framebuffer->ClearAttachment(1, -1);
        s->OnUpdateRuntime(seconds);
    };
    s->GetEntityByUUID(230).GetComponent<SpriteRendererComponent>().Color.a = 0;
    draw(.05f);
    Check(framebuffer->ReadPixel(1, 370, 300 - 270) == int(uint32_t(s->GetEntityByUUID(400))),
          "Inactive page painted/picked through transparent active content");
    s->GetEntityByUUID(230).GetComponent<SpriteRendererComponent>().Color.a = 1;
    Check(s->GetUISnapshot().Tooltips.empty(), "Tooltip bypassed discovery delay");
    draw(.05f);
    Check(s->GetUISnapshot().Tooltips.size() == 1 && s->GetUISnapshot().Scope == 100 &&
              s->GetUIDiagnostics().Focus == 231 && !s->IsUICapturingPointer(),
          "Tooltip acquired scope/focus/capture or failed discovery");
    auto originalRaster = RenderCommand::GetRasterState();
    auto masked = originalRaster;
    masked.PickingMask = {true, false, true, false};
    RenderCommand::SetRasterState(masked);
    draw(0);
    Check(RenderCommand::GetRasterState().PickingMask == masked.PickingMask,
          "Passive tooltip draw failed to restore picking mask");
    RenderCommand::SetRasterState(originalRaster);
    draw(0);
    auto tip = s->GetUISnapshot().Tooltips.front();
    Check(tip.Bounds.Position.x >= 0 && tip.Bounds.Position.y >= 0 &&
              tip.Bounds.Position.x + tip.Bounds.Size.x <= 400 &&
              tip.Bounds.Position.y + tip.Bounds.Size.y <= 300,
          "Tooltip escaped owning surface");
    auto point = tip.Bounds.Position + tip.Bounds.Size * .5f;
    int tipPick = framebuffer->ReadPixel(1, int(point.x), 300 - int(point.y));
    Check(tipPick != int(uint32_t(s->GetEntityByUUID(241))), "Tooltip overwrote picking attachment");
    auto builds = Font::Resources().LayoutBuilds;
    auto generation = s->GetUISnapshot().Generation;
    draw(0);
    Check(Font::Resources().LayoutBuilds == builds && s->GetUISnapshot().Generation == generation,
          "Warm tooltip reshaped or republished unchanged geometry");
    s->CancelUIInput();
    draw(0);
    int basePick = framebuffer->ReadPixel(1, int(point.x), 300 - int(point.y));
    Check(basePick == tipPick && s->GetUISnapshot().Tooltips.empty(),
          "Tooltip picking was not pass-through or ownership loss retained it");
    framebuffer->Unbind();
    neutral();
    s->SetUISelection(200, 210);
    commit();
    accept(400);
    for (int i = 1; i < 64; ++i)
        accept(400);
    accept(211);
    commit();
    Check(s->GetUISelection(200) == 210 && s->GetUIDiagnostics().PendingEvents == 64,
          "Queue pressure accepted an unreported page change");
    while (s->ConsumeUIEvent(900, event))
    {
    }
    neutral();
    s->SetUISelection(200, 211);
    commit();
    s->GetEntityByUUID(211).GetComponent<UIButtonComponent>().Interactable = false;
    s->RefreshUI();
    Check(s->GetUISelection(200) == 210 && !s->GetUISnapshot().Find(230)->LayoutResolved,
          "Retired/disabled tab did not repair selection and visibility");
    s->OnRuntimeStop();
    auto edit = CompositionFixture();
    auto authored = SceneSerializer(edit).CaptureAuthoredState();
    edit->RefreshUI();
    edit->PreviewUITab(200, 211);
    Check(edit->GetUISelection(200) == 210 && edit->GetUISnapshot().Find(230)->LayoutResolved &&
              !edit->GetUISnapshot().Find(220)->LayoutResolved,
          "Edit page preview rewrote authoritative default");
    edit->EnsureUIVisible(232, 231, false);
    edit->PreviewUITooltip(231, true);
    Check(edit->GetUISnapshot().Tooltips.size() == 1 &&
              SceneSerializer(edit).CaptureAuthoredState() == authored && !edit->ConsumeUIEvent(900, event),
          "Tooltip/page preview dirtied content or dispatched events");
    edit->PreviewUITooltip(231, false);
    edit->PreviewUITab(200, 0);
    SceneSerializer(edit).Serialize((directory / "compositions.hazel").generic_u8string());
    auto loaded = CreateRef<Scene>();
    Check(SceneSerializer(loaded).Deserialize((directory / "compositions.hazel").generic_u8string()) &&
              SceneSerializer(loaded).CaptureAuthoredState() == authored,
          "Composition serialization mismatch");
    auto copy = Scene::Copy(edit);
    Check(SceneSerializer(copy).CaptureAuthoredState() == authored,
          "Composition scene copy lost authored configuration");
    auto duplicate = edit->DuplicateEntity(edit->GetEntityByUUID(200));
    Check(duplicate.HasComponent<UITabsComponent>() &&
              duplicate.GetComponent<UIRadioGroupComponent>().InitialSelection != 210,
          "Tabs duplicate lost identity remapping");
    Prefab::Save(directory, "compositions.hprefab", edit, edit->GetEntityByUUID(200));
    auto prefab = Prefab::Load(directory, "compositions.hprefab");
    auto target = CreateRef<Scene>();
    auto instance = target->InstantiateEntity(Prefab::GetEntity(prefab), {});
    Check(instance.HasComponent<UITabsComponent>() &&
              instance.GetComponent<UIRadioGroupComponent>().InitialSelection != 210,
          "Tab/page/tooltip prefab remapping failed");
    auto nested = CompositionFixture();
    auto nestedRoot = nested->DuplicateEntity(nested->GetEntityByUUID(200));
    nested->Reparent(nestedRoot, nested->GetEntityByUUID(220), TransformPolicy::KeepLocal);
    auto &nr = nestedRoot.GetComponent<UIRectComponent>();
    nr.Offset = {0, 60};
    nr.FixedSize = {320, 140};
    auto nestedTab =
        nested->GetEntityByUUID(nestedRoot.GetComponent<UIRadioGroupComponent>().InitialSelection);
    auto nestedPage = nestedTab.GetComponent<UITabComponent>().Page;
    auto nestedFocus = nestedTab.GetComponent<UITabComponent>().InitialFocus;
    nested->ValidateUI(false);
    nested->OnRuntimeStart();
    InputFrame nf;
    nf.Enabled = true;
    nested->UpdateUIInput(nf);
    nested->UpdateUIInput(nf);
    Check(nested->FocusUI(nestedFocus), "Nested page initial focus unavailable");
    nested->SetUISelection(200, 211);
    nested->CommitUIScreens();
    nested->RefreshUI();
    nested->UpdateUIInput(nf);
    nested->UpdateUIInput(nf);
    Check(!nested->GetUISnapshot().Find(nestedPage)->LayoutResolved &&
              nested->GetUIDiagnostics().Focus == 231,
          "Outer page departure did not collapse nested group or transfer focus");
    nested->SetUISelection(200, 210);
    nested->CommitUIScreens();
    nested->RefreshUI();
    nested->UpdateUIInput(nf);
    nested->UpdateUIInput(nf);
    Check(nested->GetUIDiagnostics().Focus == nestedFocus, "Nested eligible focus was not restored");
    nested->OnRuntimeStop();
    // Queued work is stamped to its mapping and owner, not just a still-live UUID.
    auto retired = CompositionFixture();
    retired->OnRuntimeStart();
    InputFrame rf;
    rf.Enabled = true;
    retired->UpdateUIInput(rf);
    retired->UpdateUIInput(rf);
    auto request = [&] {
        Check(retired->FocusUI(211), "Retirement fixture tab unavailable");
        rf.Events = {{UIAction::Accept, true}, {UIAction::Accept, false}};
        retired->UpdateUIInput(rf);
        rf.Events.clear();
    };
    request();
    auto &mapping = retired->GetEntityByUUID(211).GetComponent<UITabComponent>();
    mapping.InitialFocus = 0;
    mapping.Page = 0;
    retired->CommitUIScreens();
    Check(retired->GetUISelection(200) == 210 && !retired->ConsumeUIEvent(900, event),
          "Queued tab request survived a native page reassignment");
    mapping.Page = 230;
    retired->RefreshUI();
    retired->UpdateUIInput(rf);
    retired->UpdateUIInput(rf);
    request();
    retired->CommitUIScreens();
    retired->RefreshUI();
    retired->GetEntityByUUID(100).GetComponent<UICanvasComponent>().Screen.EventOwner = 400;
    retired->RefreshUI();
    Check(!retired->ConsumeUIEvent(400, event) && !retired->ConsumeUIEvent(900, event),
          "Changed owner inherited a previous tab event");
    retired->OnRuntimeStop();
    auto contentSized = CompositionFixture();
    contentSized->GetEntityByUUID(200).GetComponent<UIRectComponent>().HeightMode = SizeMode::Content;
    contentSized->GetEntityByUUID(220).GetComponent<UIRectComponent>().HeightMode = SizeMode::Fixed;
    auto &shortPage = contentSized->GetEntityByUUID(230).GetComponent<UIRectComponent>();
    shortPage.HeightMode = SizeMode::Fixed;
    shortPage.FixedSize.y = 100;
    contentSized->RefreshUI();
    auto large = contentSized->GetUISnapshot().Find(200)->Measured.y;
    contentSized->PreviewUITab(200, 211);
    Check(contentSized->GetUISnapshot().Find(200)->Measured.y < large,
          "Inactive page still contributed to Content measurement");
    auto broken = CompositionFixture();
    broken->GetEntityByUUID(211).GetComponent<UITabComponent>().Page = 220;
    bool rejected = false;
    try
    {
        broken->ValidateUI(false);
    }
    catch (const std::exception &)
    {
        rejected = true;
    }
    Check(rejected, "Duplicate page ownership passed validation");
    std::cout << "PASS: composed tabs, selective retirement/owner admission, remembered/revealed focus, "
                 "passive delayed tooltip GPU picking, preview isolation and reference roundtrips\n";
}
