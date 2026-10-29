// Native asset/document, typed property history and detached prefab workflow.
void UIStyleWorkflowChecks()
{
    auto &owner = *m_Editor.m_Authoring;
    auto assets = Project::GetActive()->GetAssets();
    Check(bool(m_Editor.m_ContentBrowserPanel->CreateUIStyle),
          "Content Browser has no native style creation command");
    m_Editor.m_ContentBrowserPanel->CreateUIStyle("Styles");
    Check(owner.m_Styles.m_Create, "Native style creation form did not open");
    owner.m_Styles.m_Create = false;
    UIStyleDefinition definition = DefaultUIStyle();
    definition.Name = "Maintained settings";
    // Existing native-imported, Apache-licensed faces also exercise style-owned
    // family/notice closure in the relocated fixture (no new shipped font).
    definition.Variants[0].Values.Font.Family = "Fonts/Native.hfont";
    definition.Variants[1].Values.Font.Family = "Fonts/Native.hfont";
    owner.Guard(OperationIntent::OpenUIStyle,
                [&] { return owner.m_Styles.Create("Styles/Settings.hstyle", definition); });
    Check(owner.m_Styles.HasDocument(), "Style asset creation failed");
    owner.m_Styles.m_Document->Draft().Name = "Edited settings";
    owner.m_Styles.m_Document->Changed();
    owner.m_ActiveDocument = EditorDocument::UIStyle;
    Check(owner.SaveActive() && !owner.m_Styles.Dirty(),
          "Style Save did not use common document service");
    const auto saved = Read(assets->Root() / "Styles/Settings.hstyle");
    owner.m_Styles.m_Document->Draft().Name = "Unsaved style";
    owner.m_Styles.m_Document->Changed();
    {
        std::ofstream out(assets->Root() / "Styles/Settings.hstyle");
        out << "Foreign edit";
    }
    Check(!owner.m_Styles.Save() && owner.m_ShowSaveConflict &&
              owner.m_ConflictDocument == EditorDocument::UIStyle,
          "Style draft bypassed shared conflict handling");
    {
        std::ofstream out(assets->Root() / "Styles/Settings.hstyle", std::ios::binary);
        out << saved;
    }
    Check(Read(assets->Root() / "Styles/Settings.hstyle") == saved,
          "Style workflow repair changed accepted bytes");
    owner.m_ShowSaveConflict = false;
    m_Editor.m_ActionError.clear();
    owner.Guard(OperationIntent::CloseUIStyle, [&] {
        owner.m_Styles.Close();
        return true;
    });
    Check(owner.m_Documents.Pending(), "Dirty style close bypassed guard");
    owner.m_Documents.Resolve(GuardChoice::Cancel, owner.Documents(),
                              [&](auto &doc) { return owner.SaveDocument(doc); });
    Check(owner.m_Styles.Dirty(), "Cancel discarded style draft");
    owner.m_Styles.Open(assets->Root() / "Styles/Settings.hstyle");
    owner.m_Styles.UpdatePreview();
    Check(owner.m_Styles.m_Preview && owner.m_Styles.m_Error.empty(),
          "Style control preview did not use accepted draft values");
    owner.m_ActiveDocument = EditorDocument::Scene;
    auto authored = m_Editor.m_EditorScene;
    auto &panel = m_Editor.m_SceneHierarchyPanel;
    auto originalSelection = panel.GetSelectedEntity();
    auto roots = authored->GetChildren();
    panel.SetSelectedEntity({});
    Check(panel.AddUI(0), "Style fixture canvas preset failed");
    auto canvas = panel.GetSelectedEntity();
    Check(panel.AddUI(26), "Styled panel composition starter missing");
    panel.SetSelectedEntity(canvas);
    Check(panel.AddUI(24), "Styled button starter missing");
    auto button = panel.GetSelectedEntity();
    auto before = UIEditValues::Capture(button), after = before;
    after.Appearance->Selection = {"Styles/Settings.hstyle", "Control"};
    UIInspectorEditing inspector;
    PropertyUI::EditActivity activity;
    activity.Changed = activity.Committed = true;
    Check(inspector.Edit(*authored, &owner.m_SceneHistory, button.GetUUID(), "Style assignment",
                         before, after, activity, 0),
          "Style assignment could not enter shared history");
    auto assigned = authored->GetUIStyle(button.GetUUID());
    Check(owner.UndoRedo(false) && authored->GetUIStyle(button.GetUUID()).Asset.empty(),
          "Style assignment did not undo");
    Check(owner.UndoRedo(true) && authored->GetUIStyle(button.GetUUID()) == assigned,
          "Style assignment redo failed");
    auto shown = button.GetComponent<TextComponent>();
    ApplyUIStyle(shown, authored->GetUIPresentation(button.GetUUID()));
    auto edited = shown;
    edited.Color = {0, 0, 0, 0};
    auto local = button.GetComponent<TextComponent>();
    auto groups = MergeUIStyleEdit(button, shown, edited, local);
    Check(groups == UIStyleBit(UIStyleGroup::Foreground) && local.Color == glm::vec4(0),
          "Inspector override merge copied unchanged inherited fields");
    before = UIEditValues::Capture(button);
    after = before;
    after.Appearance->Overrides = UIStyleBit(UIStyleGroup::Foreground);
    Check(inspector.Edit(*authored, &owner.m_SceneHistory, button.GetUUID(), "Local foreground",
                         before, after, activity, 0),
          "Override presence did not enter history");
    before = after;
    after.Appearance->Overrides = 0;
    Check(inspector.Edit(*authored, &owner.m_SceneHistory, button.GetUUID(),
                         "Reset foreground to style", before, after, activity, 0) &&
              owner.UndoRedo(false) &&
              button.GetComponent<UIAppearanceComponent>().Overrides ==
                  UIStyleBit(UIStyleGroup::Foreground),
          "Reset-to-style did not undo local presence");
    Check(owner.UndoRedo(true) && button.GetComponent<UIAppearanceComponent>().Overrides == 0,
          "Reset-to-style did not redo inherited presence");
    Prefab::Save(assets->Root(), "Prefabs/StyledButton.hprefab", authored, button, true);
    panel.SetSelectedEntity(canvas);
    owner.InstantiatePrefab(assets->Root() / "Prefabs/StyledButton.hprefab", false);
    auto first = panel.GetSelectedEntity();
    panel.SetSelectedEntity(canvas);
    owner.InstantiatePrefab(assets->Root() / "Prefabs/StyledButton.hprefab", false);
    auto second = panel.GetSelectedEntity();
    Check(first.GetUUID() != second.GetUUID() &&
              authored->GetRelationship(first).Parent == canvas.GetUUID() &&
              authored->GetRelationship(second).Parent == canvas.GetUUID(),
          "Native styled template placement/identity failed");
    Check(authored->GetUIStyle(first.GetUUID()) == assigned &&
              authored->GetUIStyle(second.GetUUID()) == assigned,
          "Detached templates lost shared style references");
    m_StylePanelFixture.Bind(assets);
    Check(m_StylePanelFixture.Open(assets->Root() / "Styles/Settings.hstyle"),
          "Native fixture style panel could not reopen");
    m_StylePanelFixture.UpdatePreview();
    authored->DestroyEntity(canvas);
    panel.SetSelectedEntity(originalSelection);
    owner.m_Styles.Close();
    owner.m_ActiveDocument = EditorDocument::Scene;
    std::cout << "PASS: native style creation/edit/preview/save/conflict/cancel, shared assignment "
                 "Undo/Redo, explicit override merge and detached UI prefab placement\n";
}

UIStylePanel m_StylePanelFixture;
