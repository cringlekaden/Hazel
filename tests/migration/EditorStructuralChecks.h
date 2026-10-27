#include "Authoring/StructuralCommand.h"
static void EditorStructuralChecks()
{
    using namespace Hazel;
    auto world = CreateRef<Scene>();
    auto canvas = world->CreateEntity("Canvas");
    canvas.AddComponent<UICanvasComponent>();
    auto item = world->CreateEntity("Label");
    world->Reparent(item, canvas, TransformPolicy::KeepLocal);
    item.AddComponent<UIRectComponent>();
    item.AddComponent<TextComponent>().TextString = "Before";
    item.AddComponent<UITextStyleComponent>();
    DocumentHistory h;
    h.Bind(world->GetIdentity());
    SceneHierarchyPanel panel(world);
    panel.BindHistory(&h);
    panel.SetSelectedEntity(item);
    auto state = [&] { return SceneSerializer(world).CaptureAuthoredState(); };
    h.MarkSaved(state());
    auto before = UIEditValues::Capture(item), after = before;
    after.Text->Text = "Bold word";
    after.Text->Spans = {{0, 4, FontStyle::Bold, glm::vec4(1, .5f, .2f, 1)}};
    auto property = CreateScope<UIEditCommand>(*world, item.GetUUID(), "Emphasized label", before);
    auto *p = property.get();
    std::string error;
    Check(h.Begin(*world, std::move(property)) && p->Preview(*world, after, error) &&
              h.Commit(*world),
          "Structural property setup failed");
    const auto id = uint64_t(item.GetUUID()), life = world->GetAuthoringLifetime(id);
    auto authored = state();
    Check(panel.DeleteSelected(), "Native delete did not enter history");
    Check(!world->GetEntityByUUID(id) && h.Operations() == 2,
          "Delete history/entity count incorrect");
    Check(h.Undo(*world), h.Diagnostic());
    panel.RestoreHistorySelection(h.Selection().value_or(0));
    Check(!item, "Undo restored a retired EnTT handle rather than a fresh entity lifetime");
    Check(world->GetAuthoringLifetime(id) != life && panel.GetSelectedEntity().GetUUID() == id,
          "Deleted identity selection/lifetime restoration failed");
    Check(state() == authored && h.Undo(*world) &&
              world->GetEntityByUUID(id).GetComponent<TextComponent>().TextString == "Before",
          "Authorized restore did not reconcile property guards");
    Check(!h.Dirty(state()) && h.Redo(*world) && state() == authored && h.Redo(*world) &&
              !world->GetEntityByUUID(id),
          "Property/delete Redo or exact save point failed");
    Check(h.Undo(*world), h.Diagnostic());
    panel.RestoreHistorySelection(id);
    h.Clear();
    h.MarkSaved(state());
    Check(panel.AddAuthoredComponent<UIAppearanceComponent>(id), "Component menu command failed");
    before = UIEditValues::Capture(world->GetEntityByUUID(id));
    after = before;
    after.Appearance->Overrides = UIStyleBit(UIStyleGroup::Foreground);
    property = CreateScope<UIEditCommand>(*world, id, "Override", before);
    p = property.get();
    Check(h.Begin(*world, std::move(property)) && p->Preview(*world, after, error) &&
              h.Commit(*world),
          "Component property command failed");
    Check(panel.RemoveAuthoredComponent<UIAppearanceComponent>(id) && h.Undo(*world) &&
              h.Undo(*world),
          "Remove/restore component did not reconcile earlier property command");
    Check(world->GetEntityByUUID(id).GetComponent<UIAppearanceComponent>().Overrides == 0 &&
              h.Undo(*world) && !world->GetEntityByUUID(id).HasComponent<UIAppearanceComponent>(),
          "Undo add-component restored defaults incorrectly");
    Check(h.Redo(*world) && h.Redo(*world) && h.Redo(*world),
          "Interleaved component/property redo failed");
    Check(h.Undo(*world), h.Diagnostic());
    panel.RestoreHistorySelection(id);
    h.Clear();
    Check(panel.DuplicateSelected(), "Native duplicate failed");
    auto duplicate = panel.GetSelectedEntity().GetUUID();
    Check(duplicate != id, "Duplicate identity not allocated");
    auto duplicateState = state();
    Check(h.Undo(*world) && !world->GetEntityByUUID(duplicate) && h.Redo(*world) &&
              state() == duplicateState,
          "Duplicate redo did not preserve identities/data");
    panel.RestoreHistorySelection(id);
    Check(panel.MoveSelected(1) && h.Undo(*world) && h.Redo(*world),
          "Native sibling reorder history failed");
    auto snapshot = state();
    auto count = h.Operations();
    Check(!panel.ReparentEntity(world->GetIdentity(), canvas.GetUUID(), id,
                                TransformPolicy::KeepWorld) &&
              state() == snapshot && h.Operations() == count,
          "Cycle rejection partially changed live history/document");
    Check(panel.ReparentEntity(world->GetIdentity(), id, duplicate, TransformPolicy::KeepLocal) &&
              h.Undo(*world) && h.Redo(*world),
          "Native reparent history failed");
    // Unowned transform edits survive relationship-only history.
    world->GetEntityByUUID(id).GetComponent<TransformComponent>().Translation.x = 17;
    auto changed = state();
    Check(h.Undo(*world) &&
              world->GetEntityByUUID(id).GetComponent<TransformComponent>().Translation.x == 17 &&
              h.Redo(*world),
          "Structural history overwrote an unowned component");
    world->Reparent(world->GetEntityByUUID(id), canvas, TransformPolicy::KeepLocal);
    changed = state();
    Check(!h.Undo(*world) && state() == changed, "Conflicting external reparent was overwritten");
    // A small admission limit rejects before live mutation/selection/redo changes.
    DocumentHistory small(2, 128);
    small.Bind(world->GetIdentity());
    panel.BindHistory(&small);
    panel.RestoreHistorySelection(id);
    auto old = state();
    Check(!panel.AddUI(3) && state() == old && panel.GetSelectedEntity().GetUUID() == id &&
              small.Operations() == 0,
          "Structural payload admission partially published");
    panel.BindHistory(&h);
    // Every maintained creation command is atomic, including automatic canvas,
    // tab/page/list parts and styled template nesting.
    for (int kind = 0; kind <= 26; ++kind)
    {
        auto scene = CreateRef<Scene>();
        DocumentHistory history;
        history.Bind(scene->GetIdentity());
        SceneHierarchyPanel creator(scene);
        creator.BindHistory(&history);
        if (kind == 16)
            Check(creator.AddUI(15), "Dropdown setup failed");
        if (kind == 18)
            Check(creator.AddUI(17), "Tabs setup failed");
        if (kind == 20)
            Check(creator.AddUI(19), "List setup failed");
        if (kind == 21)
            Check(creator.AddUI(3), "Tooltip setup failed");
        history.Clear();
        auto start = SceneSerializer(scene).CaptureAuthoredState();
        Check(creator.AddUI(kind), "Native preset transaction failed");
        auto end = SceneSerializer(scene).CaptureAuthoredState();
        Check(history.Operations() == 1 && history.Undo(*scene) &&
                  SceneSerializer(scene).CaptureAuthoredState() == start && history.Redo(*scene) &&
                  SceneSerializer(scene).CaptureAuthoredState() == end,
              "Composed preset was not one reversible operation");
    }
    // UUID reuse outside history is never an authorized restoration.
    auto isolated = CreateRef<Scene>();
    SceneHierarchyPanel c(isolated);
    DocumentHistory history;
    history.Bind(isolated->GetIdentity());
    c.BindHistory(&history);
    Check(c.AddUI(2), "Reuse setup failed");
    auto target = c.GetSelectedEntity().GetUUID();
    auto e = isolated->GetEntityByUUID(target);
    isolated->DestroyEntity(e);
    isolated->CreateEntityWithUUID(target, "Unrelated");
    auto unrelated = SceneSerializer(isolated).CaptureAuthoredState();
    Check(!history.Undo(*isolated) && SceneSerializer(isolated).CaptureAuthoredState() == unrelated,
          "UUID reuse bypassed structural lifetime guards");
    // Incoming references remain authored and repairable while their target is
    // absent; restores do not silently clear/rewrite surviving owners.
    {
        auto scene = CreateRef<Scene>();
        DocumentHistory history;
        history.Bind(scene->GetIdentity());
        SceneHierarchyPanel editor(scene);
        editor.BindHistory(&history);
        editor.ReportError = [](const std::string &error) {
            std::cerr << "Structural diagnostic: " << error << "\n";
        };
        Check(editor.AddUI(8), "Reference checkbox setup failed");
        auto checkbox = editor.GetSelectedEntity();
        const auto controlID = checkbox.GetUUID();
        Check(editor.AddChild(checkbox, "Indicator"), "Named indicator creation failed");
        const auto indicator = editor.GetSelectedEntity().GetUUID();
        Check(editor.AddAuthoredComponent<UIRectComponent>(indicator) &&
                  editor.AddAuthoredComponent<SpriteRendererComponent>(indicator),
              "Indicator components failed");
        scene->GetEntityByUUID(controlID).GetComponent<UIButtonComponent>().Indicator = indicator;
        const auto start = SceneSerializer(scene).CaptureAuthoredState();
        editor.RestoreHistorySelection(indicator);
        history.Clear();
        Check(editor.DeleteSelected() &&
                  scene->GetEntityByUUID(controlID).GetComponent<UIButtonComponent>().Indicator ==
                      indicator,
              "Delete silently cleared incoming visual reference");
        Check(history.Undo(*scene) && SceneSerializer(scene).CaptureAuthoredState() == start,
              "Referenced visual restore lost authored data");
        // A changed hierarchy outside history must reject restoration atomically.
        Check(history.Redo(*scene), history.Diagnostic());
        auto foreign = scene->CreateEntity("Outside child");
        scene->Reparent(foreign, scene->GetEntityByUUID(controlID), TransformPolicy::KeepLocal);
        const auto external = SceneSerializer(scene).CaptureAuthoredState();
        Check(!history.Undo(*scene) && SceneSerializer(scene).CaptureAuthoredState() == external,
              "External sibling conflict partially restored subtree");
    }
    // No source asset is removed by instance Undo. Internal references are
    // remapped once; redo restores precisely the same allocated identities.
    {
        auto scene = CreateRef<Scene>();
        scene->SetAssets(Project::GetActive()->GetAssets());
        DocumentHistory history;
        history.Bind(scene->GetIdentity());
        SceneHierarchyPanel editor(scene);
        editor.BindHistory(&history);
        editor.ReportError = [](const std::string &error) {
            std::cerr << "Structural diagnostic: " << error << "\n";
        };
        Check(editor.AddUI(11), "Prefab slider setup failed");
        auto source = editor.GetSelectedEntity();
        const auto sourceID = source.GetUUID();
        const auto root = Project::GetAssetDirectory();
        const auto asset = std::filesystem::path("Prefabs/structural-slider.hprefab");
        Prefab::Save(root, asset, scene, source, true);
        const auto bytes = Read(root / asset);
        history.Clear();
        auto instantiate = [&] {
            auto stage = editor.AuthoringScene();
            auto value = Prefab::Instantiate(root, asset, *stage);
            return editor.SetSelectedEntity(value);
        };
        Check(editor.Structural("Instantiate prefab", instantiate), "Shared prefab command failed");
        const auto instance = editor.GetSelectedEntity().GetUUID();
        auto clone = scene->GetEntityByUUID(instance);
        auto parts = clone.GetComponent<UISliderComponent>();
        Check(instance != sourceID &&
                  scene->GetRelationship(scene->GetEntityByUUID(parts.Track)).Parent == instance &&
                  parts.Track != source.GetComponent<UISliderComponent>().Track,
              "Prefab internal references were not remapped");
        const auto end = SceneSerializer(scene).CaptureAuthoredState();
        Check(history.Undo(*scene) && !scene->GetEntityByUUID(instance) &&
                  Read(root / asset) == bytes && history.Redo(*scene) &&
                  SceneSerializer(scene).CaptureAuthoredState() == end,
              "Prefab Undo/Redo changed source or identities");
        editor.RestoreHistorySelection(instance);
        Check(editor.DuplicateSelected(), "Composed duplicate failed");
        auto twin = editor.GetSelectedEntity();
        Check(twin.GetComponent<UISliderComponent>().Thumb != parts.Thumb &&
                  scene->GetRelationship(
                           scene->GetEntityByUUID(twin.GetComponent<UISliderComponent>().Thumb))
                          .Parent == twin.GetUUID(),
              "Duplicate retained source visual references");
    }
    // Bounded operation eviction and a failed new command preserve the redo
    // branch; a successful edit discards it. Save content remains authoritative.
    {
        auto scene = CreateRef<Scene>();
        DocumentHistory history(2);
        history.Bind(scene->GetIdentity());
        SceneHierarchyPanel editor(scene);
        editor.BindHistory(&history);
        editor.ReportError = [](const std::string &error) {
            std::cerr << "Structural diagnostic: " << error << "\n";
        };
        auto state = [&] { return SceneSerializer(scene).CaptureAuthoredState(); };
        history.MarkSaved(state());
        Check(editor.AddEntity("A") && editor.AddEntity("B") && editor.AddEntity("C"),
              "Eviction setup failed");
        Check(history.Operations() == 2 && !history.SavePointRetained() &&
                  !history.Diagnostic().empty(),
              "Structural eviction/save-point accounting failed");
        Check(history.Undo(*scene), history.Diagnostic());
        const auto before = state();
        const auto redo = history.RedoName();
        Check(!editor.Structural("Reject",
                                 [] {
                                     throw std::runtime_error("Controlled failure");
                                     return true;
                                 }) &&
                  state() == before && history.RedoName() == redo,
              "Failed execute discarded redo");
        Check(editor.AddEntity("Branch") && !history.CanRedo() && history.Operations() == 2,
              "Structural branch did not discard redo");
        Check(history.Undo(*scene) && history.Undo(*scene) && !history.CanUndo() &&
                  history.Dirty(state()),
              "Evicted structural entries crossed retained boundary");
        auto other = CreateRef<Scene>();
        const auto saved = state();
        Check(!history.Redo(*other) && state() == saved, "History applied to a different document");
    }
    // All component menu types use the shared path. Required/dependent removals
    // may reject; a successful removal must restore actual authored values.
    {
        auto scene = CreateRef<Scene>();
        DocumentHistory history;
        history.Bind(scene->GetIdentity());
        SceneHierarchyPanel editor(scene);
        editor.BindHistory(&history);
        editor.ReportError = [](const std::string &error) {
            std::cerr << "Structural diagnostic: " << error << "\n";
        };
        auto basic = scene->CreateEntity("Components");
        const auto id = basic.GetUUID();
        editor.SetSelectedEntity(basic);
        auto component = [&](auto tag) {
            using T = decltype(tag);
            history.Clear();
            const auto before = SceneSerializer(scene).CaptureAuthoredState();
            Check(editor.AddAuthoredComponent<T>(id), "Supported component add command failed");
            const auto after = SceneSerializer(scene).CaptureAuthoredState();
            Check(history.Operations() == 1 && history.Undo(*scene) &&
                      SceneSerializer(scene).CaptureAuthoredState() == before &&
                      history.Redo(*scene) &&
                      SceneSerializer(scene).CaptureAuthoredState() == after,
                  "Component add was not reversible");
            Check(editor.RemoveAuthoredComponent<T>(id) && history.Undo(*scene) &&
                      SceneSerializer(scene).CaptureAuthoredState() == after &&
                      history.Redo(*scene) &&
                      SceneSerializer(scene).CaptureAuthoredState() == before,
                  "Component remove did not restore configuration");
        };
        component(CameraComponent{});
        component(ScriptComponent{});
        component(SpriteRendererComponent{});
        component(CircleRendererComponent{});
        component(TextComponent{});
        component(AudioSourceComponent{});
        component(Rigidbody2DComponent{});
        component(BoxCollider2DComponent{});
        component(CircleCollider2DComponent{});
        component(UICanvasComponent{});
        component(UIRectComponent{});
        Check(editor.AddAuthoredComponent<UIRectComponent>(id), "Rect menu setup failed");
        component(UILayoutComponent{});
        component(UIButtonComponent{});
        component(UIRadioGroupComponent{});
        component(UIProgressComponent{});
        component(UISliderComponent{});
        component(UIScrollViewComponent{});
        component(UIAppearanceComponent{});
        component(UITextEntryComponent{});
        component(UITabsComponent{});
        component(UITabComponent{});
        component(UITooltipComponent{});
        component(UIDropdownComponent{});
        Check(editor.AddAuthoredComponent<TextComponent>(id), "Text style setup failed");
        component(UITextStyleComponent{});
        Check(editor.AddAuthoredComponent<SpriteRendererComponent>(id), "Nine-slice setup failed");
        component(UINineSliceComponent{});
        component(SpriteAnimationComponent{});
        const auto before = SceneSerializer(scene).CaptureAuthoredState();
        const auto count = history.Operations();
        Check(!editor.RemoveAuthoredComponent<TransformComponent>(id) &&
                  SceneSerializer(scene).CaptureAuthoredState() == before &&
                  history.Operations() == count,
              "Required component rejection failed");
        // Transform must never be removable. The actual prior document survives.
        Check(scene->GetEntityByUUID(id).HasComponent<TransformComponent>(),
              "Required Transform was removed");
    }
    {
        auto scene = CreateRef<Scene>();
        DocumentHistory history;
        history.Bind(scene->GetIdentity());
        SceneHierarchyPanel editor(scene);
        editor.BindHistory(&history);
        Check(editor.AddUI(3), "Picker setup failed");
        auto control = editor.GetSelectedEntity();
        auto id = control.GetUUID();
        control.GetComponent<SpriteRendererComponent>().Source = TextureSpriteSource{{}, 3, {}};
        control.GetComponent<UIButtonComponent>().Images[2] =
            SpriteReference{"Textures/unassigned.hsprites", 0};
        history.Clear();
        const auto before = SceneSerializer(scene).CaptureAuthoredState();
        Check(editor.DeleteSelected() && history.Undo(*scene) &&
                  SceneSerializer(scene).CaptureAuthoredState() == before,
              "Structural restore lost incomplete authored picker modes/tiling");
        editor.RestoreHistorySelection(id);
        history.Clear();
        auto texture = Texture2D::Create(TextureSpecification{});
        scene->GetEntityByUUID(id).GetComponent<SpriteRendererComponent>().SetTexture(texture);
        Check(editor.AddEntity("Unrelated native bridge") && history.Undo(*scene) &&
                  std::get<TextureSpriteSource>(
                      scene->GetEntityByUUID(id).GetComponent<SpriteRendererComponent>().Source)
                          .Resource == texture,
              "Unrelated structural edit discarded a procedural resource");
        editor.RestoreHistorySelection(id);
        const auto native = SceneSerializer(scene).CaptureAuthoredState();
        auto count = history.Operations();
        Check(!editor.DeleteSelected() && SceneSerializer(scene).CaptureAuthoredState() == native &&
                  history.Operations() == count,
              "Native-only removal accepted an unrestorable resource");
    }
    {
        auto scene = CreateRef<Scene>();
        DocumentHistory history;
        history.Bind(scene->GetIdentity());
        SceneHierarchyPanel editor(scene);
        editor.BindHistory(&history);
        Check(editor.AddEntity("Parent"), "Keep-children parent failed");
        auto parent = editor.GetSelectedEntity();
        const auto parentID = parent.GetUUID();
        Check(editor.AddChild(parent, "Child"), "Keep-children child failed");
        auto child = editor.GetSelectedEntity();
        const auto id = child.GetUUID();
        scene->GetEntityByUUID(parent.GetUUID()).GetComponent<TransformComponent>().Translation = {
            3, 4, 0};
        editor.RestoreHistorySelection(parent.GetUUID());
        history.Clear();
        auto start = SceneSerializer(scene).CaptureAuthoredState();
        auto pose = scene->GetWorldTransform(child);
        Check(editor.DeleteSelected(DestroyPolicy::KeepChildren, TransformPolicy::KeepWorld) &&
                  !scene->GetEntityByUUID(parentID) &&
                  !uint64_t(scene->GetRelationship(scene->GetEntityByUUID(id)).Parent) &&
                  scene->GetWorldTransform(scene->GetEntityByUUID(id)) == pose,
              "Keep Children delete lost world pose");
        Check(history.Undo(*scene) && SceneSerializer(scene).CaptureAuthoredState() == start &&
                  history.Redo(*scene) && history.Undo(*scene),
              "Keep Children was not an atomic reversible operation");
        // Pending Inspector edits finish; viewport gestures cancel before creation.
        auto canvas = scene->CreateEntity("Canvas");
        canvas.AddComponent<UICanvasComponent>();
        auto rect = scene->GetEntityByUUID(id);
        rect.AddComponent<UIRectComponent>();
        scene->Reparent(rect, canvas, TransformPolicy::KeepLocal);
        scene->OnViewportResize(640, 480);
        history.Clear();
        editor.RestoreHistorySelection(id);
        UIInspectorEditing inspector;
        auto before = UIEditValues::Capture(rect), after = before;
        after.Rect->Offset.x = 12;
        PropertyUI::EditActivity activity;
        activity.Changed = true;
        activity.Item = 1001;
        Check(inspector.Edit(*scene, &history, id, "Offset", before, after, activity, 1001),
              "Pending Inspector setup failed");
        editor.BeforeStructure = [&] { inspector.Finish(*scene, &history, false); };
        Check(editor.AddEntity("After edit") && history.Operations() == 2 && history.Undo(*scene) &&
                  history.Undo(*scene) &&
                  scene->GetEntityByUUID(id).GetComponent<UIRectComponent>().Offset.x == 0,
              "Structural boundary did not finish pending property transaction");
        UIRectEditSession gesture;
        Check(gesture.Begin(scene, id, 0, &history) && gesture.Update({10, 0}),
              "Pending viewport setup failed");
        editor.BeforeStructure = [&] { gesture.Finish(true); };
        Check(editor.AddEntity("After cancel") && !gesture.Active() && !history.Active() &&
                  scene->GetEntityByUUID(id).GetComponent<UIRectComponent>().Offset.x == 0,
              "Structural boundary leaked unfinished viewport gesture");
    }
    {
        auto scene = CreateRef<Scene>();
        auto root = scene->CreateEntity("Parent");
        const auto rootID = root.GetUUID();
        auto child = scene->CreateEntity("Child");const auto childID=child.GetUUID();
        scene->Reparent(child, root, TransformPolicy::KeepLocal);
        DocumentHistory history;
        history.Bind(scene->GetIdentity());
        SceneHierarchyPanel editor(scene);
        editor.BindHistory(&history);
        editor.SetSelectedEntity(child);
        Check(editor.DeleteSelected(), "Boundary guard setup failed");
        editor.RestoreHistorySelection(rootID);
        Check(editor.DeleteSelected()&&history.Undo(*scene)&&history.Undo(*scene)&&scene->GetRelationship(scene->GetEntityByUUID(childID)).Parent==rootID,"Authorized parent restoration did not reconcile child history");
        Check(history.Redo(*scene),history.Diagnostic());
        scene->DestroyEntity(scene->GetEntityByUUID(rootID));
        scene->CreateEntityWithUUID(rootID, "Unrelated parent");
        const auto before = SceneSerializer(scene).CaptureAuthoredState();
        Check(!history.Undo(*scene) && SceneSerializer(scene).CaptureAuthoredState() == before,
              "Structural restore attached to an unrelated replacement parent");
    }
    std::cout << "PASS: structural native presets, deletion/property and component/property "
                 "interleaving, stable redo IDs, selection, reparent/reorder/cycles, admission and "
                 "external lifetime/value conflicts\n";
}
