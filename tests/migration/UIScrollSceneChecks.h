#include "Hazel/UI/RuntimeUIConfig.h"
#include "Hazel/UI/UIActions.h"
// Static documents use real scene layout/publication, production actions and GL picking.
static void UIScrollSceneChecks(const std::filesystem::path &root)
{
    using namespace GameUI;
    auto scene = CreateRef<Scene>();
    scene->OnViewportResize(400, 300);
    auto owner = scene->CreateEntityWithUUID(900, "Scroll owner");
    auto canvas = scene->CreateEntityWithUUID(100, "Scroll scope");
    auto &policy = canvas.AddComponent<UICanvasComponent>();
    policy.ReferenceSize = {400, 300};
    policy.SafeInsets = {};
    policy.Modal = true;
    policy.Screen.Managed = true;
    policy.Screen.EventOwner = 900;
    policy.Screen.InitialFocus = 202;
    auto rect = [&](uint64_t id, Entity parent, glm::vec2 offset, glm::vec2 size) {
        auto e = scene->CreateEntityWithUUID(id, "Scroll " + std::to_string(id));
        scene->Reparent(e, parent, TransformPolicy::KeepLocal);
        auto &r = e.AddComponent<UIRectComponent>();
        r.Offset = offset;
        r.FixedSize = size;
        return e;
    };
    auto viewport = rect(200, canvas, {10, 10}, {240, 160});
    viewport.GetComponent<UIRectComponent>().Padding = {10, 10, 24, 10};
    viewport.AddComponent<SpriteRendererComponent>(glm::vec4(.1f, .1f, .1f, 1));
    auto &config = viewport.AddComponent<UIScrollViewComponent>();
    config.Content = 201;
    config.PixelsPerUnit = 20;
    config.RevealMargin = 0;
    auto content = rect(201, viewport, {}, {206, 320});
    auto &cr = content.GetComponent<UIRectComponent>();
    cr.WidthMode = SizeMode::Fill;
    cr.HeightMode = SizeMode::Content;
    content.AddComponent<UILayoutComponent>().Gap = 0;
    std::vector<Entity> rows;
    for (int i = 0; i < 8; ++i)
    {
        auto e = rect(202 + i, content, {}, {206, 40});
        e.GetComponent<UIRectComponent>().WidthMode = SizeMode::Fill;
        e.AddComponent<SpriteRendererComponent>(glm::vec4(1));
        e.AddComponent<TextComponent>().TextString = "Row " + std::to_string(i + 1);
        e.AddComponent<UITextStyleComponent>().Size = 18;
        auto &b = e.AddComponent<UIButtonComponent>();
        b.TabOrder = i;
        b.Action = "row";
        rows.push_back(e);
    }
    auto track = rect(210, viewport, {4, 0}, {10, 0});
    auto &tr = track.GetComponent<UIRectComponent>();
    tr.Anchor.x = 1;
    tr.HeightMode = SizeMode::Fill;
    track.AddComponent<SpriteRendererComponent>(glm::vec4(.3f));
    auto thumb = rect(211, viewport, {}, {16, 16});
    thumb.AddComponent<SpriteRendererComponent>(glm::vec4(.8f));
    config.VerticalTrack = 210;
    config.VerticalThumb = 211;
    scene->ValidateUI(false);
    auto authored = SceneSerializer(scene).CaptureAuthoredState();
    scene->OnRuntimeStart();
    scene->RefreshUI();
    auto g = scene->GetUIScroll(200);
    Check(g.Viewport.Position == glm::vec2(20) && g.Viewport.Size == glm::vec2(206, 140) &&
              g.Extent == glm::vec2(206, 320) && g.Range == glm::vec2(0, 180),
          "Scroll extent/viewport/range incorrect");
    Check(!scene->GetUISnapshot().Find(206)->FocusEligible && !scene->FocusUI(206),
          "Clipped control acquired focus before reveal");
    InputFrame f;
    f.Enabled = true;
    f.Mouse = {390, 290};
    auto route = [&] { scene->UpdateUIInput(f); };
    route();
    UIInputAdapter adapter;
    DeviceSample sample;
    sample.Point = {50, 50};
    adapter.Sample(sample, 0, true);
    auto before = scene->GetUIDiagnostics();
    adapter.ScrollEdge({0, -.5f}, sample.Point);
    scene->UpdateUIInput(adapter.Sample(sample, .016f, true));
    Check(scene->GetUIScroll(200).Offset.y == 10 &&
              scene->GetUISnapshot().Find(202)->Bounds.Position.y == 10,
          "Fractional wheel not reflected in published control geometry");
    Check(scene->GetUIDiagnostics().Resolves == before.Resolves &&
              scene->GetUIDiagnostics().Measurements == before.Measurements,
          "Scroll remeasured unchanged layout");
    auto invalidPointer = f;
    invalidPointer.Mouse.x = std::numeric_limits<float>::quiet_NaN();
    invalidPointer.Scroll = {0, 1}; invalidPointer.ScrollPage = true;
    bool invalidRejected = false;
    try {scene->UpdateUIInput(invalidPointer);}catch(const std::invalid_argument &){invalidRejected=true;}
    Check(invalidRejected && scene->GetUIScroll(200).Offset.y == 10,
          "Rejected pointer frame partially applied its scroll action");
    auto generation = scene->GetUISnapshot().Generation;
    scene->RefreshUI();
    Check(scene->GetUISnapshot().Generation == generation,
          "Idle scroll state republished generation");
    Action event;
    Check(!scene->ConsumeUIEvent(900, event), "Internal scrolling flooded owner event queue");
    auto target = Framebuffer::Create(
        {400,
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
    draw();
    Check(target->ReadPixel(1, 50, 300 - 55) == int(uint32_t(rows[1])),
          "Picking did not follow scrolled row geometry");
    target->Unbind();
    scene->ScrollUITo(200, {});
    f = {};
    f.Enabled = true;
    route();
    // Pointer-only track pages once and never emits Activated, including outside release.
    f.Mouse = {235, 130};
    f.PointerPressed = f.LeftDown = true;
    f.PressPosition = f.Mouse;
    route();
    f.PointerPressed = false;
    Check(std::abs(scene->GetUIScroll(200).Offset.y - 126) < .001f && scene->IsUICapturingPointer(),
          "Scrollbar page/capture incorrect");
    f.LeftDown = false;
    f.PointerReleased = f.HasReleasePosition = true;
    f.ReleasePosition = {500, 500};
    route();
    f.PointerReleased = false;
    route();
    Check(!scene->IsUICapturingPointer() && !scene->ConsumeUIEvent(900, event),
          "Scrollbar release became activation/stuck capture");
    scene->ScrollUITo(200, {});
    f.Mouse = {235, 30};
    route();
    f.LeftDown = f.PointerPressed = true;
    f.PressPosition = f.Mouse;
    route();
    f.PointerPressed = false;
    f.Mouse = {235, 1000};
    route();
    Check(scene->GetUIScroll(200).Offset.y == 180, "Scrollbar drag failed exact endpoint");
    f.LeftDown = false;
    f.PointerReleased = true;
    f.ReleasePosition = f.Mouse;
    route();
    f.PointerReleased = false;
    route();
    draw();
    Check(target->ReadPixel(1, 50, 300 - 25) == int(uint32_t(rows[4])) &&
              target->ReadPixel(1, 235, 300 - 110) == int(uint32_t(thumb)),
          "Scrolled row/derived scrollbar thumb pixels disagree");
    target->Unbind();
    // Once all static labels have been prepared, repeated translation reuses text/pages.
    for (int i = 0; i < 3; ++i)
    {
        scene->ScrollUITo(200, {0, 0});
        draw();
        scene->ScrollUITo(200, {0, 180});
        draw();
    }
    target->Unbind();
    auto fonts = Font::Resources();
    auto measured = scene->GetUIDiagnostics().Measurements;
    for (int i = 0; i < 4; ++i)
    {
        scene->ScrollUITo(200, {0, float((i % 2) * 180)});
        draw();
    }
    target->Unbind();
    auto warm = Font::Resources();
    Check(warm.Generations == fonts.Generations && warm.LayoutBuilds == fonts.LayoutBuilds &&
              scene->GetUIDiagnostics().Measurements == measured,
          "Warm scrolling reshaped text/regenerated pages");
    scene->ScrollUITo(200, {});
    route();
    scene->FocusUI(202);
    for (int i = 0; i < 6; ++i)
    {
        f.Events = {{UIAction::Next, true}, {UIAction::Next, false}};
        route();
        f.Events.clear();
        route();
    }
    Check(scene->GetUIDiagnostics().Focus == 208 &&
              scene->GetUISnapshot().Find(208)->FocusEligible &&
              scene->GetUIScroll(200).Offset.y > 0,
          "Tab did not reveal next logical target");
    Check(scene->EnsureUIVisible(200, 209, true) && scene->GetUIDiagnostics().Focus == 209,
          "Explicit focus reveal failed");
    Check(!scene->EnsureUIVisible(200, 900, true), "External ensure-visible target admitted");
    scene->ScrollUITo(200, {});
    route();
    f.Mouse = {235, 30};
    f.LeftDown = f.PointerPressed = true;
    f.PressPosition = f.Mouse;
    route();
    f.PointerPressed = false;
    f.CancelDown = true;
    route();
    Check(!scene->IsUICapturingPointer() && scene->IsUIScreenOpen(100),
          "Scrollbar Cancel closed screen or stuck capture");
    f = {};
    f.Enabled = true;
    route();
    // Native size changes cancel capture and clamp; loss cannot replay buffered wheel.
    f.Mouse = {235, 30};
    f.LeftDown = f.PointerPressed = true;
    f.PressPosition = f.Mouse;
    route();
    f.PointerPressed = false;
    viewport.GetComponent<UIRectComponent>().FixedSize.y = 180;
    scene->RefreshUI();
    Check(!scene->IsUICapturingPointer(), "Resize retained stale scrollbar capture");
    viewport.GetComponent<UIRectComponent>().FixedSize.y = 160;
    f = {};
    f.Enabled = true;
    route();
    scene->RefreshUI();
    f.Mouse = {235, 30};
    f.LeftDown = f.PointerPressed = true;
    f.PressPosition = f.Mouse;
    route();
    config.MinimumThumb = 100;
    scene->RefreshUI();
    Check(!scene->IsUICapturingPointer(), "Thumb metric change retained stale drag mapping");
    config.MinimumThumb = 16;
    f = {}; f.Enabled = true; route();
    thumb.GetComponent<UIRectComponent>().Enabled = false;
    scene->RefreshUI();
    f.Mouse = {235, 30}; f.LeftDown = f.PointerPressed = true; f.PressPosition = f.Mouse;
    route();
    Check(!scene->IsUICapturingPointer(), "Disabled scrollbar part accepted capture");
    thumb.GetComponent<UIRectComponent>().Enabled = true;
    f = {}; f.Enabled = true; route();
    adapter.ScrollEdge({0, -1}, sample.Point);
    scene->UpdateUIInput(adapter.Sample(sample, .016f, false));
    auto lost = scene->GetUIScroll(200).Offset;
    scene->UpdateUIInput(adapter.Sample(sample, .016f, true));
    Check(scene->GetUIScroll(200).Offset == lost, "Ownership restore replayed wheel");
    Check(SceneSerializer(scene).CaptureAuthoredState() == authored,
          "Scrolling/preview/capture dirtied authored data");
    // Nested residual: inner can take 5 pixels; only the remaining 15 reaches outer.
    auto nested = rect(300, content, {}, {206, 100});
    nested.GetComponent<UIRectComponent>().WidthMode = SizeMode::Fill;
    auto &nv = nested.AddComponent<UIScrollViewComponent>();
    nv.Content = 301;
    nv.PixelsPerUnit = 20;
    nv.RevealMargin = 0;
    auto inner = rect(301, nested, {}, {206, 200});
    inner.GetComponent<UIRectComponent>().WidthMode = SizeMode::Fill;
    inner.GetComponent<UIRectComponent>().HeightMode = SizeMode::Content;
    inner.AddComponent<UILayoutComponent>().Gap = 0;
    for (int i = 0; i < 5; ++i)
    {
        auto e = rect(302 + i, inner, {}, {206, 40});
        e.GetComponent<UIRectComponent>().WidthMode = SizeMode::Fill;
        e.AddComponent<UIButtonComponent>().TabOrder = 20 + i;
    }
    scene->OnUpdateRuntime(0); scene->RefreshUI();
    f = {};
    f.Enabled = true;
    route();
    scene->ScrollUITo(200, {0, 200});
    scene->ScrollUITo(300, {0, 95});
    adapter.ScrollEdge({0, -1}, {50, 150});
    scene->UpdateUIInput(adapter.Sample(sample, .016f, true));
    Check(scene->GetUIScroll(300).Offset.y == 100 && scene->GetUIScroll(200).Offset.y == 215,
          "Nested views consumed duplicate full deltas");
    scene->ScrollUITo(200, {});
    scene->ScrollUITo(300, {});
    Check(scene->EnsureUIVisible(200, 306, true) && scene->GetUISnapshot().Find(306)->FocusEligible,
          "Reveal could not reach nested content initially outside outer viewport");
    // Both-axis region and independent fractional components.
    auto both = rect(400, canvas, {280, 10}, {100, 100});
    auto &bv = both.AddComponent<UIScrollViewComponent>();
    bv.Content = 401;
    bv.Horizontal = bv.Vertical = true;
    bv.PixelsPerUnit = 20;
    rect(401, both, {}, {300, 300});
    scene->OnUpdateRuntime(0); scene->RefreshUI();
    adapter.ScrollEdge({.25f, -.5f}, {300, 30});
    scene->UpdateUIInput(adapter.Sample(sample, .016f, true));
    Check(scene->GetUIScroll(400).Offset == glm::vec2(5, 10),
          "Two-axis delta lost direction/fraction");
    auto blocking = scene->CreateEntityWithUUID(700, "Empty blocking modal");
    auto &bc = blocking.AddComponent<UICanvasComponent>();
    bc.ReferenceSize = {400, 300}; bc.SafeInsets = {}; bc.Modal = true;
    bc.Screen.Managed = true; bc.Screen.EventOwner = 900;
    scene->OnUpdateRuntime(0); scene->RefreshUI();
    auto isolated = scene->GetUIScroll(400).Offset;
    f = {}; f.Enabled = true; route();
    f.Scroll = {0, 1}; f.Mouse = {300, 30}; route();
    Check(scene->GetUIScroll(400).Offset == isolated && scene->IsGameplayInputBlocked(),
          "Modal boundary leaked scrolling into underlying view/gameplay");
    scene->DestroyEntity(blocking); scene->OnUpdateRuntime(0); scene->RefreshUI();
    f = {}; f.Enabled = true; route();
    auto clipper = rect(450, content, {}, {206, 40});
    clipper.GetComponent<UIRectComponent>().ClipChildren = true;
    auto unreachable = rect(451, clipper, {0, 300}, {100, 20});
    unreachable.AddComponent<UIButtonComponent>();
    scene->OnUpdateRuntime(0); scene->RefreshUI();
    auto beforeRejected = scene->GetUIScroll(200).Offset;
    Check(!scene->EnsureUIVisible(200, 451, true) &&
              scene->GetUIScroll(200).Offset == beforeRejected,
          "Unreachable non-scroll clip left partial reveal offsets");
    scene->DestroyEntity(clipper); scene->OnUpdateRuntime(0); scene->RefreshUI();
    // Slider in scroll content keeps capture geometry stable; explicit ScrollTo interrupts it.
    auto slider = rect(500, content, {}, {206, 50});
    slider.GetComponent<UIRectComponent>().WidthMode = SizeMode::Fill;
    auto &sliderButton = slider.AddComponent<UIButtonComponent>();
    sliderButton.Kind = ControlKind::Slider;
    sliderButton.TabOrder = 30;
    auto &sv = slider.AddComponent<UISliderComponent>();
    sv.Track = 501;
    sv.Thumb = 502;
    auto st = rect(501, slider, {0, 20}, {206, 10});
    st.AddComponent<SpriteRendererComponent>();
    auto sh = rect(502, slider, {}, {20, 30});
    sh.AddComponent<SpriteRendererComponent>();
    scene->OnUpdateRuntime(0); scene->RefreshUI();
    f = {};
    f.Enabled = true;
    route();
    Check(scene->EnsureUIVisible(200, 500, true), "Slider reveal failed");
    Rect scrolledThumb; bool thumbVisible = false;
    Check(scene->GetUIControlVisual(502, scrolledThumb, thumbVisible) && thumbVisible,
          "Scrolled slider lost derived thumb presentation");
    draw();
    auto center = scrolledThumb.Position + scrolledThumb.Size * .5f;
    Check(target->ReadPixel(1, int(center.x), 300 - int(center.y)) == int(uint32_t(sh)),
          "Derived slider thumb picking did not follow scroll displacement");
    target->Unbind();
    f.AcceptDown = true;
    route();
    f.AcceptDown = false;
    route();
    Check(scene->IsUISliderEditing(), "Scrolled slider did not enter edit");
    auto sliderOffset = scene->GetUIScroll(200).Offset;
    adapter.ScrollEdge({0, 1}, {50, 130});
    scene->UpdateUIInput(adapter.Sample(sample, .016f, true));
    Check(scene->GetUIScroll(200).Offset == sliderOffset && scene->IsUISliderEditing(),
          "Wheel moved geometry beneath slider edit");
    scene->ScrollUITo(200, {});
    Check(!scene->IsUISliderEditing(), "Programmatic scroll left a stale slider edit");
    // CPU wrap reflow updates extent/range; hidden content clamps while disabled keeps layout.
    auto paragraph = rect(600, content, {}, {206, 20});
    paragraph.GetComponent<UIRectComponent>().WidthMode = SizeMode::Fill;
    paragraph.GetComponent<UIRectComponent>().HeightMode = SizeMode::Content;
    paragraph.AddComponent<TextComponent>().TextString = "Wrapped text in a finite width.";
    paragraph.AddComponent<UITextStyleComponent>().Size = 20;
    scene->OnUpdateRuntime(0); scene->RefreshUI();
    float height = scene->GetUIScroll(200).Extent.y;
    paragraph.GetComponent<TextComponent>().TextString =
        std::string(30, 'W') + " a longer paragraph with several words and wrapping";
    scene->RefreshUI();
    Check(scene->GetUIScroll(200).Extent.y > height,
          "Native text mutation did not reflow scroll extent");
    auto beforeHidden = scene->GetUIScroll(200).Extent.y;
    paragraph.GetComponent<UIRectComponent>().Enabled = false;
    scene->RefreshUI();
    Check(scene->GetUIScroll(200).Extent.y == beforeHidden, "Disabled content collapsed layout");
    paragraph.GetComponent<UIRectComponent>().Visible = false;
    scene->RefreshUI();
    Check(scene->GetUIScroll(200).Extent.y < beforeHidden,
          "Hidden content did not collapse extent");
    paragraph.GetComponent<UIRectComponent>().Visible =
        paragraph.GetComponent<UIRectComponent>().Enabled = true;
    // Strict dependency and relationship failures; accepted fields restored by the test.
    bool rejected = false;
    content.GetComponent<UIRectComponent>().HeightMode = SizeMode::Fill;
    try
    {
        scene->ValidateUI(false);
    }
    catch (const std::exception &e)
    {
        rejected = std::string(e.what()).find("indefinite") != std::string::npos;
    }
    Check(rejected, "Indefinite scroll Fill accepted");
    content.GetComponent<UIRectComponent>().HeightMode = SizeMode::Content;
    scene->OnRuntimeStop();
    auto independent = scene->CreateEntity("Independent Canvas");
    independent.AddComponent<UICanvasComponent>();
    scene->Reparent(independent, content, TransformPolicy::KeepLocal);
    rejected = false;
    try
    {
        scene->ValidateUI(false);
    }
    catch (...)
    {
        rejected = true;
    }
    Check(rejected, "Nested independent canvas scrolled accidentally");
    scene->DestroyEntity(independent);
    auto finalAuthored = SceneSerializer(scene).CaptureAuthoredState();
    SceneSerializer(scene).Serialize((root / "scroll.hazel").generic_u8string());
    auto loaded = CreateRef<Scene>();
    Check(SceneSerializer(loaded).Deserialize((root / "scroll.hazel").generic_u8string()) &&
              SceneSerializer(loaded).CaptureAuthoredState() == finalAuthored,
          "Scroll scene roundtrip lost authored fields");
    auto copy = Scene::Copy(scene);
    Check(SceneSerializer(copy).CaptureAuthoredState() == finalAuthored, "Scroll copy lost fields");
    auto duplicated = scene->DuplicateEntity(viewport);
    auto &dv = duplicated.GetComponent<UIScrollViewComponent>();
    Check(dv.Content != 201 && dv.VerticalThumb != 211 &&
              scene->GetRelationship(scene->GetEntityByUUID(dv.Content)).Parent ==
                  duplicated.GetUUID(),
          "Scroll duplicate failed part/content remap");
    Prefab::Save(root, "scroll.hprefab", scene, viewport);
    auto prefab = Prefab::Load(root, "scroll.hprefab");
    auto destination = CreateRef<Scene>();
    auto instance = destination->InstantiateEntity(Prefab::GetEntity(prefab), {});
    Check(instance.GetComponent<UIScrollViewComponent>().Content != 201,
          "Scroll prefab did not remap content");
    scene->OnRuntimeStart();
    scene->RefreshUI();
    Check(scene->GetUIScroll(200).Offset == glm::vec2(0), "Stop/copy leaked offsets into new Play");
    scene->SetUIFramebufferSize({500, 375});
    scene->ScrollUITo(200, {0, 1.25f});
    auto fractional = scene->GetUISnapshot().Find(202);
    Check(std::abs(fractional->FramebufferBounds.Position.y - 23.4375f) < .001f &&
              fractional->FramebufferClip.Position.y == 25,
          "Scroll translation lost fractional framebuffer coverage");
    auto contentRect = content.GetComponent<UIRectComponent>();
    content.RemoveComponent<UIRectComponent>();
    content.AddComponent<UIRectComponent>(contentRect);
    scene->RefreshUI();
    Check(scene->GetUIScroll(200).Offset == glm::vec2(0),
          "Content component replacement retained retired offsets");
    scene->ScrollUITo(200, {0, 1000000});
    auto children = scene->GetChildrenView(content.GetUUID());
    for (auto child : children)scene->DestroyEntity(scene->GetEntityByUUID(child));
    scene->OnUpdateRuntime(0);
    Check(scene->GetUIScroll(200).Offset == glm::vec2(0) &&
              scene->GetUIScroll(200).Range == glm::vec2(0),
          "Content removal did not clamp range and offsets");
    scene->SetUIFramebufferSize({0, 0});
    scene->RefreshUI();
    Check(scene->GetUISnapshot().Buttons.empty(), "Minimized surface retained scroll hits");
    scene->SetUIFramebufferSize({400, 300});
    scene->RefreshUI();
    scene->OnRuntimeStop();
    // An automatic focus record is not keyboard ownership on an ordinary nonmodal canvas.
    auto unowned = Scene::Copy(loaded);
    auto &uc = unowned->GetEntityByUUID(100).GetComponent<UICanvasComponent>();
    uc.Modal = false;
    uc.Screen.Managed = false;
    unowned->OnViewportResize(400, 300);
    unowned->OnRuntimeStart();
    unowned->RefreshUI();
    f = {};
    f.Enabled = true;
    f.Mouse = {390, 290};
    unowned->UpdateUIInput(f);
    f.Events = {{UIAction::ScrollDown, true}, {UIAction::ScrollDown, false}};
    unowned->UpdateUIInput(f);
    Check(unowned->GetUIScroll(200).Offset.y == 0 && !unowned->IsGameplayInputBlocked(),
          "Unowned nonmodal focus stole a page action");
    f.Events.clear();
    f.NextPressed = f.FocusNavigation = true;
    unowned->UpdateUIInput(f);
    f.NextPressed = f.FocusNavigation = false;
    f.Events = {{UIAction::ScrollDown, true}, {UIAction::ScrollDown, false}};
    unowned->UpdateUIInput(f);
    Check(unowned->GetUIScroll(200).Offset.y > 0,
          "Owned keyboard focus could not scroll a nonmodal view");
    f = {}; f.Enabled = true; f.Mouse = {15, 15};
    f.PointerPressed = f.LeftDown = true; f.PressPosition = f.Mouse;
    unowned->UpdateUIInput(f);
    f.PointerPressed = f.LeftDown = false;
    f.PointerReleased = f.HasReleasePosition = true; f.ReleasePosition = f.Mouse;
    unowned->UpdateUIInput(f);
    f.PointerReleased = false;
    f.Events = {{UIAction::Cancel, true}, {UIAction::Cancel, false}};
    unowned->UpdateUIInput(f);
    auto releasedOffset = unowned->GetUIScroll(200).Offset;
    f.Events = {{UIAction::ScrollDown, true}, {UIAction::ScrollDown, false}};
    unowned->UpdateUIInput(f);
    Check(unowned->GetUIScroll(200).Offset == releasedOffset &&
              !unowned->IsGameplayInputBlocked(),
          "Remembered clicked viewport stole page input after Cancel released keyboard ownership");
    unowned->OnRuntimeStop();
    auto leading = Scene::Copy(unowned);
    leading->GetEntityByUUID(202).GetComponent<UIRectComponent>().FixedSize.y = 300;
    leading->GetEntityByUUID(202).GetComponent<UIButtonComponent>().Interactable = false;
    leading->OnViewportResize(400, 300);
    leading->OnRuntimeStart();
    f = {}; f.Enabled = true; f.Mouse = {390, 290};
    leading->UpdateUIInput(f);
    Check(!leading->GetUIDiagnostics().Focus, "Leading static content gained clipped focus");
    f.Events = {{UIAction::Next, true}, {UIAction::Next, false}};
    leading->UpdateUIInput(f);
    Check(leading->GetUIDiagnostics().Focus == 203 &&
              leading->GetUISnapshot().Find(203)->FocusEligible &&
              leading->GetUIScroll(200).Offset.y > 0,
          "First Tab could not enter nonmodal content with no initially visible control");
    leading->OnRuntimeStop();
    std::cout
        << "PASS: static scroll measurement/publication, residual wheel, scrollbar "
           "capture/picking, focus reveal, control edits, reflow, copy/prefab and retirement\n";
}
