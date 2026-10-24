void UIScreenWorkflowChecks()
{
    using namespace GameUI;
    auto &editor = m_Editor;
    auto &documents = *editor.m_Authoring;
    auto original = editor.m_EditorScenePath;
    editor.NewScene();
    auto scene = editor.m_EditorScene;
    auto &panel = editor.m_SceneHierarchyPanel;
    auto edit = [&](Entity target, const char *name, auto change)
    {
        panel.SetSelectedEntity(target);
        auto before = UIEditValues::Capture(target), after = before;
        change(after);
        UIInspectorEditing inspector;
        PropertyUI::EditActivity a;
        a.Changed = a.Committed = true;
        Check(inspector.Edit(*scene, &documents.m_SceneHistory, target.GetUUID(), name, before,
                             after, a, 0),
              inspector.Error());
    };
    Check(panel.AddUI(7), "Native screen preset missing");
    auto menu = panel.GetSelectedEntity();
    menu.GetComponent<TagComponent>().Tag = "Menu screen";
    panel.SetSelectedEntity(menu);
    Check(panel.AddUI(7), "Settings screen creation failed");
    auto settings = panel.GetSelectedEntity();
    settings.GetComponent<TagComponent>().Tag = "Settings screen";
    panel.SetSelectedEntity(menu);
    Check(panel.AddUI(7), "Confirmation screen creation failed");
    auto confirmation = panel.GetSelectedEntity();
    confirmation.GetComponent<TagComponent>().Tag = "Confirmation screen";
    menu.AddComponent<ScriptComponent>().ClassName = "Migration.ScreenOwnerProbe";
    for (auto ref :
         {std::pair<const char *, Entity>{"Settings", settings}, {"Confirmation", confirmation}})
    {
        auto &field = ScriptEngine::GetScriptFieldMap(menu)[ref.first];
        field.Field = {ScriptFieldType::Entity, ref.first, nullptr};
        field.SetValue<uint64_t>(ref.second.GetUUID());
    }
    Entity settingsBody;
    std::vector<uint64_t> first;
    for (auto screen : {menu, settings, confirmation})
    {
        edit(screen, "Screen policy",
             [&](auto &v)
             {
                 v.Canvas->ReferenceSize = {640, 480};
                 v.Canvas->SafeInsets = {};
                 v.Canvas->Screen.Managed = true;
                 v.Canvas->Screen.InitiallyOpen = screen == menu;
                 v.Canvas->Screen.EventOwner = menu.GetUUID();
                 v.Canvas->Screen.Cancel =
                     screen == menu ? ScreenCancel::Ignore : ScreenCancel::Close;
             });
        panel.SetSelectedEntity(screen);
        Check(panel.AddUI(1), "Screen panel creation failed");
        auto body = panel.GetSelectedEntity();if(screen==settings)settingsBody=body;
        edit(body, "Screen panel",
             [&](auto &v)
             {
                 v.Rect->FixedSize = screen==settings?glm::vec2(600,440):glm::vec2(440,360);
                 v.Rect->Offset = screen==settings?glm::vec2(20,20):glm::vec2(100,50);
                 v.Image->Color = {.06f, .09f, .14f, 1};
             });
        panel.SetSelectedEntity(body);
        Check(panel.AddUI(2), "Screen heading creation failed");
        auto heading = panel.GetSelectedEntity();
        edit(heading, "Heading",
             [&](auto &v)
             {
                 v.Text->Text = screen.GetName();
                 v.Rect->Offset = {24, 20};
                 v.Style->Size = 28;
             });
        for (int i = 0; i < 2; ++i)
        {
            panel.SetSelectedEntity(body);
            Check(panel.AddUI(3), "Screen button creation failed");
            auto button = panel.GetSelectedEntity();
            auto action = screen == menu       ? (i == 0 ? "settings" : "confirm")
                          : screen == settings ? (i == 0 ? "confirm" : "back")
                                               : "back";
            edit(button, "Screen command",
                 [&](auto &v)
                 {
                     v.Rect->WidthMode = v.Rect->HeightMode = SizeMode::Fixed;
                     v.Rect->FixedSize = {280, 56};
                     v.Rect->Offset = {24, float(85 + 85 * i)};
                     v.Button->Action = action;
                     v.Button->TabOrder = i;
                     v.Text->Text = screen == confirmation ? (i == 0 ? "Confirm" : "Keep editing")
                                    : i == 0 ? (screen == menu ? "Settings" : "Confirmation…")
                                    : screen == menu ? "Confirmation…"
                                                     : "Back";
                 });
            if (i == 0)
                first.push_back(button.GetUUID());
        }
    }
    panel.SetSelectedEntity(settingsBody);Check(panel.AddUI(8),"Checkbox preset failed");auto checkbox=panel.GetSelectedEntity();
    edit(checkbox,"Checkbox label/default",[](auto &v){v.Text->Text="Music";v.Button->Action="music";v.Button->InitiallyChecked=true;v.Button->TabOrder=10;v.Rect->Offset={24,255};});
    auto configuredCheckbox=SceneSerializer(scene).CaptureAuthoredState();
    Check(documents.UndoRedo(false)&&!checkbox.GetComponent<UIButtonComponent>().InitiallyChecked,"Toggle Inspector transaction did not undo");
    Check(documents.UndoRedo(true)&&SceneSerializer(scene).CaptureAuthoredState()==configuredCheckbox,"Toggle Inspector transaction did not redo");
    panel.SetSelectedEntity(settingsBody);Check(panel.AddUI(9),"Radio group preset failed");auto radio=panel.GetSelectedEntity();
    edit(radio,"Radio group configuration",[](auto &v){v.Rect->Offset={320,80};v.Rect->FixedSize.x=180;v.Group->Action="quality";});
    int index=0;for(auto id:scene->GetChildrenView(radio.GetUUID())){
        auto option=scene->GetEntityByUUID(id);int optionIndex=index++;
        edit(option,"Radio label",[&](auto &v){v.Text->Text=optionIndex==0?"Low":optionIndex==1?"Medium":"High";v.Button->TabOrder=20+optionIndex;});
    }
    panel.SetSelectedEntity(settingsBody);Check(panel.AddUI(10),"Progress preset failed");auto progress=panel.GetSelectedEntity();
    edit(progress,"Progress range/default",[](auto &v){v.Rect->Offset={24,390};v.Rect->FixedSize.x=540;v.Progress->Minimum=0;v.Progress->Maximum=1;v.Progress->InitialValue=.25f;});
    for(auto ref:{std::pair<const char*,Entity>{"Toggle",checkbox},{"Choices",radio},{"Progress",progress}}){auto &field=ScriptEngine::GetScriptFieldMap(menu)[ref.first];field.Field={ScriptFieldType::Entity,ref.first,nullptr};field.SetValue<uint64_t>(ref.second.GetUUID());}
    panel.SetSelectedEntity(settingsBody);Check(panel.AddUI(11),"Horizontal slider preset failed");auto volume=panel.GetSelectedEntity();
    edit(volume,"Stepped volume",[](auto &v){v.Rect->Offset={24,325};v.Rect->FixedSize={460,48};v.Slider->Minimum=0;v.Slider->Maximum=100;v.Slider->Step=5;v.Slider->InitialValue=50;v.Button->Action="volume";v.Button->TabOrder=30;});
    auto sliderAuthored=documents.SceneText();Check(documents.UndoRedo(false)&&volume.GetComponent<UISliderComponent>().Maximum==1,"Slider range history undo failed");Check(documents.UndoRedo(true)&&documents.SceneText()==sliderAuthored,"Slider history redo failed");
    {auto before=UIEditValues::Capture(volume),invalid=before;invalid.Slider->Step=0;UIInspectorEditing inspector;PropertyUI::EditActivity a;a.Changed=a.Committed=true;
     Check(!inspector.Edit(*scene,&documents.m_SceneHistory,volume.GetUUID(),"Invalid slider",before,invalid,a,0)&&documents.SceneText()==sliderAuthored,"Invalid slider edit changed authored/history state");inspector.Finish(*scene,&documents.m_SceneHistory,true);Check(documents.SceneText()==sliderAuthored,"Invalid slider cancellation changed authored state");}
    panel.SetSelectedEntity(settingsBody);Check(panel.AddUI(12),"Vertical slider preset failed");auto level=panel.GetSelectedEntity();
    edit(level,"Reverse vertical slider",[](auto &v){v.Rect->Offset={520,80};v.Rect->FixedSize={56,200};v.Slider->Minimum=-1;v.Slider->Maximum=1;v.Slider->Step=.25f;v.Slider->InitialValue=0;v.Button->Action="level";v.Button->TabOrder=31;});
    panel.SetSelectedEntity(settingsBody);Check(panel.AddUI(2),"Slider value label preset failed");auto feedback=panel.GetSelectedEntity();
    edit(feedback,"Slider feedback",[](auto &v){v.Rect->Offset={24,302};v.Style->Size=18;v.Text->Text="Volume 50 / Adjust: accept to edit";});
    for(auto ref:{std::pair<const char*,Entity>{"Volume",volume},{"Level",level},{"Feedback",feedback}}){auto &field=ScriptEngine::GetScriptFieldMap(menu)[ref.first];field.Field={ScriptFieldType::Entity,ref.first,nullptr};field.SetValue<uint64_t>(ref.second.GetUUID());}
    scene->ValidateUI(false);
    for (size_t i = 0; i < 3; ++i)
    {
        auto target = i == 0 ? menu : i == 1 ? settings : confirmation;
        edit(target, "Initial focus",
             [&](auto &v)
             {
                 v.Canvas->Screen.InitialFocus = first[i];
                 v.Canvas->Screen.DefaultAction = first[i];
             });
    }
    const auto configured = documents.SceneText();
    Check(documents.UndoRedo(false) && documents.SceneText() != configured,
          "Screen policy Inspector edit did not undo");
    Check(documents.UndoRedo(true) && documents.SceneText() == configured,
          "Screen policy redo failed");
    auto volumeID=uint64_t(volume.GetUUID());
    auto menuID = uint64_t(menu.GetUUID()), settingsID = uint64_t(settings.GetUUID()),
         confirmationID = uint64_t(confirmation.GetUUID());
    auto path = Project::GetAssetDirectory() / "Scenes/screens-native.hazel";
    editor.m_EditorScenePath = path;
    Check(editor.SaveScene() && editor.OpenScene(path), "Screen scene save/reopen failed");
    scene = editor.m_EditorScene;
    Prefab::Save(Project::GetAssetDirectory(), "Prefabs/screens-native.hprefab", scene,
                 scene->GetEntityByUUID(menuID), true);
    documents.SelectAsset(Project::GetAssetDirectory() / "Prefabs/screens-native.hprefab");
    Check(bool(documents.m_PrefabScene), "Screen prefab reopen failed");
    documents.m_PrefabScene->SetUIPreviewTarget(settingsID);
    documents.m_PrefabScene->OnViewportResize(640, 480);
    documents.m_PrefabScene->RefreshUI();
    Check(documents.m_PrefabScene->GetUISnapshot().Find(first[1])->LayoutResolved &&
              !documents.m_PrefabScene->GetUISnapshot().Find(first[0])->LayoutResolved,
          "Closed-screen prefab selection preview failed");
    documents.ClosePrefab();
    m_ScreenFixture = Scene::Copy(scene);
    m_ScreenFixtureMenu = menuID;
    m_ScreenFixtureSettings = settingsID;
    m_ScreenFixtureFirst = first;
    m_ScreenFixturePanel.SetContext(m_ScreenFixture);
    m_ScreenFixturePanel.SetSelectedEntity(m_ScreenFixture->GetEntityByUUID(menuID));
    m_ScreenFixturePanel.RestoreSections({{"UI Canvas", true}});
    auto accepted = documents.SceneText();
    for (int i = 0; i < 3; ++i)
    {
        Check(editor.OnScenePlay(true), "Screen fixture Play failed");
        auto runtime = editor.m_ActiveScene;
        auto &session = editor.m_RuntimeSession;
        session.Resize(640, 480);
        session.ResizeFramebuffer(640, 480);
        DeviceSample sample;
        sample.Point = {600, 450};
        auto route = [&]
        {
            session.SetInput(sample.Point, true);
            session.RouteUIInput(sample, .016f, true);
        };
        route();
        Check(runtime->GetUIDiagnostics().Focus == first[0],
              "Native authored initial focus failed");
        sample.Keys[Key::Enter] = true;
        route();
        runtime->OnUpdateRuntime(.016f);
        Check(runtime->IsUIScreenOpen(settingsID), "Native owner did not open settings");
        sample.Keys.fill(false);
        route();
        Check(runtime->FocusUI(volumeID),"Fixture slider focus failed");
        sample.Keys[Key::Enter]=true;route();runtime->OnUpdateRuntime(.016f);
        sample.Keys.fill(false);route();sample.Keys[Key::Right]=true;route();runtime->OnUpdateRuntime(.016f);
        Check(runtime->GetUISliderValue(volumeID)==55&&runtime->IsUISliderEditing(),"Fixture keyboard Adjust failed");
        sample.Keys.fill(false);route();sample.Keys[Key::Escape]=true;route();runtime->OnUpdateRuntime(.016f);
        Check(runtime->GetUISliderValue(volumeID)==50&&runtime->IsUIScreenOpen(settingsID),"Fixture Cancel failed value rollback / closed screen");
        sample.Keys.fill(false);route();Check(runtime->FocusUI(first[1]),"Fixture return to command failed");
        sample.Keys[Key::Enter] = true;
        route();
        runtime->OnUpdateRuntime(.016f);
        Check(runtime->IsUIScreenOpen(confirmationID), "Native owner did not open confirmation");
        sample.Keys.fill(false);
        route();
        sample.Keys[Key::Escape] = true;
        route();
        runtime->OnUpdateRuntime(.016f);
        Check(!runtime->IsUIScreenOpen(confirmationID), "Native confirmation Cancel did not close");
        sample.Keys.fill(false);
        route();
        Check(runtime->GetUIDiagnostics().Focus == first[1],
              "Confirmation did not return to invoking settings control");
        sample.Keys[Key::Escape] = true;
        route();
        runtime->OnUpdateRuntime(.016f);
        sample.Keys.fill(false);
        route();
        Check(runtime->GetUIDiagnostics().Focus == first[0],
              "Settings did not return to menu invoker");
        editor.OnSceneStop();
        Check(documents.SceneText() == accepted,
              "Runtime screen changes dirtied authored document");
    }
    if (const auto *output = std::getenv("HAZEL_EDITOR_FONT_FIXTURE"); output && *output)
        for (auto ref : {"Scenes/screens-native.hazel", "Prefabs/screens-native.hprefab"})
        {
            auto destination = std::filesystem::u8path(output) / ref;
            std::filesystem::create_directories(destination.parent_path());
            std::filesystem::copy_file(Project::GetAssetDirectory() / ref, destination,
                                       std::filesystem::copy_options::overwrite_existing);
        }
    Check(editor.OpenScene(original), "Screen fixture did not restore original document");
    std::cout << "PASS: native screen creation/selectors/history, scene/prefab "
                 "save/reopen and scripted nested-modal return in three "
                 "Play/Stop cycles (synthetic keyboard)\n";
}

Ref<Scene> m_ScreenFixture;
SceneHierarchyPanel m_ScreenFixturePanel;
Ref<Framebuffer> m_ScreenFixturePreview;
uint64_t m_ScreenFixtureMenu = 0, m_ScreenFixtureSettings = 0;
std::vector<uint64_t> m_ScreenFixtureFirst;
void DrawUIScreenFixture()
{
    if (!m_ScreenFixture)
        return;
    const auto authored = SceneSerializer(m_ScreenFixture).CaptureAuthoredState();
    ImGui::SetNextWindowSize({380, 600}, ImGuiCond_Always);
    ImGui::Begin("Screen authoring qualification", nullptr,
                 ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking |
                     ImGuiWindowFlags_NoInputs);
    m_ScreenFixturePanel.DrawAssetProperties(m_ScreenFixturePanel.GetSelectedEntity());
    auto originalSelection=m_ScreenFixturePanel.GetSelectedEntity();
    for(auto handle:m_ScreenFixture->GetAllEntitiesWith<UIRectComponent>()){
        Entity item(handle,m_ScreenFixture.get());
        if(item.HasComponent<UIProgressComponent>()||item.HasComponent<UIRadioGroupComponent>()||(item.HasComponent<UIButtonComponent>()&&item.GetComponent<UIButtonComponent>().Kind!=GameUI::ControlKind::Button)){
            m_ScreenFixturePanel.SetSelectedEntity(item);m_ScreenFixturePanel.RestoreSections({{"UI Button",true},{"UI Radio Group",true},{"UI Progress",true},{"UI Slider",true}});m_ScreenFixturePanel.DrawAssetProperties(item);
        }
    }
    m_ScreenFixturePanel.SetSelectedEntity(originalSelection);

    auto &owner = *m_Editor.m_Authoring;
    auto previousScene = owner.m_PrefabScene;
    auto previousPreview = owner.m_PrefabPreview;
    auto previousCamera = owner.m_PrefabCamera;
    bool previousFit = owner.m_PrefabFit;
    auto previousSelection = owner.m_PrefabInspector.GetSelectedEntity();
    owner.m_PrefabScene = m_ScreenFixture;
    owner.m_PrefabPreview = m_ScreenFixturePreview;
    owner.m_PrefabFit = true;
    owner.m_PrefabInspector.SetContext(m_ScreenFixture);
    owner.m_PrefabInspector.SetSelectedEntity(
        m_ScreenFixture->GetEntityByUUID(m_ScreenFixtureSettings));
    ImGui::GetStateStorage()->SetInt(ImGui::GetID("Subtree preview"), 1);
    owner.PrefabPreview();
    m_ScreenFixturePreview = owner.m_PrefabPreview;
    owner.m_PrefabScene = previousScene;
    owner.m_PrefabPreview = previousPreview;
    owner.m_PrefabCamera = previousCamera;
    owner.m_PrefabFit = previousFit;
    owner.m_PrefabInspector.SetContext(previousScene);
    if (previousSelection && previousSelection.BelongsTo(previousScene.get()))
        owner.m_PrefabInspector.SetSelectedEntity(previousSelection);
    ImGui::End();
    Check(bool(m_ScreenFixturePreview) && m_ScreenFixture->GetUIError().empty() &&
              m_ScreenFixture->GetUISnapshot().Find(m_ScreenFixtureFirst[1])->LayoutResolved &&
              !m_ScreenFixture->GetUISnapshot().Find(m_ScreenFixtureFirst[0])->LayoutResolved,
          "Native closed-screen selection preview failed");
    GameUI::Action event;
    Check(SceneSerializer(m_ScreenFixture).CaptureAuthoredState() == authored &&
              !m_ScreenFixture->ConsumeUIAction(0, event) && !m_ScreenFixture->ConsumeUIEvent(m_ScreenFixtureMenu,event),
          "Screen Inspector/preview changed authored content or dispatched an action");
    std::cout << "PASS: actual native screen selectors and closed-screen preview, no "
                 "authored/action side effects\n";
}
