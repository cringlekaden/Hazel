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
    panel.SetSelectedEntity(menu);Check(panel.AddUI(7),"Scrollable screen preset failed");auto scrolling=panel.GetSelectedEntity();scrolling.GetComponent<TagComponent>().Tag="Scrollable settings";
    edit(scrolling,"Scrollable screen policy",[&](auto &v){v.Canvas->ReferenceSize={640,480};v.Canvas->SafeInsets={};v.Canvas->Screen.InitiallyOpen=false;v.Canvas->Screen.EventOwner=menu.GetUUID();v.Canvas->Screen.Cancel=ScreenCancel::Close;});
    panel.SetSelectedEntity(scrolling);Check(panel.AddUI(13),"ScrollView preset failed");auto scrollView=panel.GetSelectedEntity();
    edit(scrollView,"Scroll viewport",[](auto &v){v.Rect->Offset={24,60};v.Rect->FixedSize={592,380};v.Scroll->PixelsPerUnit=48;});
    edit(scrollView,"Scroll wheel speed",[](auto &v){v.Scroll->PixelsPerUnit=64;});
    auto scrollHistory=documents.SceneText();
    Check(documents.UndoRedo(false)&&scrollView.GetComponent<UIScrollViewComponent>().PixelsPerUnit==48,"Scroll property history undo failed");
    Check(documents.UndoRedo(true)&&documents.SceneText()==scrollHistory&&scrollView.GetComponent<UIScrollViewComponent>().PixelsPerUnit==64,"Scroll property history redo failed");
    {auto before=UIEditValues::Capture(scrollView),invalid=before;invalid.Scroll->PixelsPerUnit=0;UIInspectorEditing inspector;PropertyUI::EditActivity a;a.Changed=a.Committed=true;
     Check(!inspector.Edit(*scene,&documents.m_SceneHistory,scrollView.GetUUID(),"Invalid scroll speed",before,invalid,a,0)&&documents.SceneText()==scrollHistory,"Invalid scroll edit changed authored/history state");inspector.Finish(*scene,&documents.m_SceneHistory,true);}
    edit(scrollView,"Scroll wheel speed",[](auto &v){v.Scroll->PixelsPerUnit=48;});
    auto scrollContent=scene->GetEntityByUUID(scrollView.GetComponent<UIScrollViewComponent>().Content);
    panel.SetSelectedEntity(scrollContent);Check(panel.AddUI(8),"Scrolled checkbox preset failed");auto scrollToggle=panel.GetSelectedEntity();
    edit(scrollToggle,"Scrolled option",[](auto &v){v.Button->Action="extra-option";v.Button->TabOrder=50;v.Text->Text="Additional static setting";});
    panel.SetSelectedEntity(scrollContent);Check(panel.AddUI(2),"Wrapped scroll text preset failed");auto paragraph=panel.GetSelectedEntity();
    edit(paragraph,"Wrapped scroll help",[](auto &v){v.Rect->WidthMode=SizeMode::Fill;v.Rect->HeightMode=SizeMode::Content;v.Style->Size=20;v.Text->Text="This static help section wraps at the viewport width. Scroll with the wheel, Page keys or controller bumpers; Tab reveals offscreen controls without an owner script positioning them.";});
    panel.SetSelectedEntity(scrollContent);Check(panel.AddUI(14),"Nested horizontal ScrollView preset failed");auto horizontal=panel.GetSelectedEntity();
    edit(horizontal,"Nested scroll row",[](auto &v){v.Rect->WidthMode=SizeMode::Fill;v.Rect->FixedSize.y=100;v.Scroll->RevealMargin=4;});
    auto horizontalContent=scene->GetEntityByUUID(horizontal.GetComponent<UIScrollViewComponent>().Content);uint64_t farTarget=0;
    int option=0;for(auto child:scene->GetChildrenView(horizontalContent.GetUUID())){auto e=scene->GetEntityByUUID(child);int tab=60+option++;edit(e,"Nested option order",[&](auto &v){v.Button->TabOrder=tab;});farTarget=child;}
    panel.SetSelectedEntity(scrollContent);Check(panel.AddUI(15),"Dropdown preset failed");auto dropdown=panel.GetSelectedEntity();
    edit(dropdown,"Dropdown placement/selection",[](auto &v){v.Rect->WidthMode=SizeMode::Fixed;v.Rect->FixedSize={240,44};v.Rect->Margin.x=264;v.Dropdown->MinimumWidth=380;v.Dropdown->MaximumHeight=180;v.Group->Action="mode";});
    auto dropdownHistory=documents.SceneText();Check(documents.UndoRedo(false)&&dropdown.GetComponent<UIDropdownComponent>().MinimumWidth==160,"Dropdown history undo failed");Check(documents.UndoRedo(true)&&documents.SceneText()==dropdownHistory,"Dropdown history redo failed");
    auto popup=scene->GetEntityByUUID(dropdown.GetComponent<UIDropdownComponent>().Popup);auto optionContent=scene->GetEntityByUUID(popup.GetComponent<UIScrollViewComponent>().Content);
    panel.SetSelectedEntity(dropdown);Check(panel.AddUI(16),"Native Dropdown option creation failed");auto addedOption=panel.GetSelectedEntity();
    edit(addedOption,"Long dropdown option",[](auto &v){v.Text->Text="A deliberately long static option label uses the ordinary ellipsis presentation";});
    panel.SetSelectedEntity(addedOption);Check(panel.MoveSelected(-1),"Dropdown hierarchy reorder failed");Check(panel.MoveSelected(1),"Dropdown hierarchy restore order failed");
    auto dropdownOptions=scene->GetChildrenView(optionContent.GetUUID());auto firstOption=uint64_t(dropdownOptions.front()),lastOption=uint64_t(dropdownOptions.back());
    edit(scene->GetEntityByUUID(dropdownOptions[1]),"Disabled dropdown choice",[](auto &v){v.Button->Interactable=false;v.Text->Text="Unavailable choice";});
    edit(dropdown,"Dropdown default",[&](auto &v){v.Group->InitialSelection=lastOption;});
    for(auto ref:{std::pair<const char*,Entity>{"Dropdown",dropdown},{"DropdownChoice",scene->GetEntityByUUID(firstOption)}}){auto &field=ScriptEngine::GetScriptFieldMap(menu)[ref.first];field.Field={ScriptFieldType::Entity,ref.first,nullptr};field.SetValue<uint64_t>(ref.second.GetUUID());}
    panel.SetSelectedEntity(scrolling);Check(panel.AddUI(3),"Reveal command preset failed");auto reveal=panel.GetSelectedEntity();edit(reveal,"Reveal command",[](auto &v){v.Rect->Offset={24,8};v.Rect->WidthMode=v.Rect->HeightMode=SizeMode::Fixed;v.Rect->FixedSize={280,40};v.Button->Action="scroll-reveal";v.Button->TabOrder=-1;v.Text->Text="Reveal last nested option";});
    panel.SetSelectedEntity(settingsBody);Check(panel.AddUI(3),"Scrollable settings command failed");auto scrollOpen=panel.GetSelectedEntity();edit(scrollOpen,"More settings",[](auto &v){v.Rect->Offset={320,250};v.Rect->WidthMode=v.Rect->HeightMode=SizeMode::Fixed;v.Rect->FixedSize={190,48};v.Button->Action="scroll-settings";v.Button->TabOrder=32;v.Text->Text="More settings…";v.Style->Size=18;});
    for(auto ref:{std::pair<const char*,Entity>{"Scrolling",scrolling},{"ScrollView",scrollView},{"ScrollTarget",scene->GetEntityByUUID(farTarget)}}){auto &field=ScriptEngine::GetScriptFieldMap(menu)[ref.first];field.Field={ScriptFieldType::Entity,ref.first,nullptr};field.SetValue<uint64_t>(ref.second.GetUUID());}
    auto scrollConfigured=documents.SceneText();Check(documents.UndoRedo(false)&&documents.UndoRedo(true)&&documents.SceneText()==scrollConfigured,"Scroll fixture history failed");
    // One additional screen keeps the pre-existing settings/confirmation flows intact.
    panel.SetSelectedEntity(menu);Check(panel.AddUI(7),"Tabbed settings screen creation failed");auto tabbed=panel.GetSelectedEntity();
    edit(tabbed,"Tabbed screen policy",[&](auto &v){v.Canvas->ReferenceSize={640,480};v.Canvas->SafeInsets={};v.Canvas->Screen.InitiallyOpen=false;v.Canvas->Screen.EventOwner=menu.GetUUID();v.Canvas->Screen.Cancel=ScreenCancel::Close;});
    panel.SetSelectedEntity(tabbed);Check(panel.AddUI(17),"Tabs native preset failed");auto tabs=panel.GetSelectedEntity();
    edit(tabs,"Tabs sizing and identity",[](auto &v){v.Rect->Offset={16,60};v.Rect->FixedSize={608,380};v.Group->Action="settings-page";v.Tabs->RememberFocus=true;});
    auto strip=scene->GetEntityByUUID(scene->GetChildrenView(tabs.GetUUID()).front());auto tabItems=scene->GetChildrenView(strip.GetUUID());
    auto settingsTab=scene->GetEntityByUUID(tabItems[0]),journalTab=scene->GetEntityByUUID(tabItems[1]);
    auto settingsPage=scene->GetEntityByUUID(settingsTab.GetComponent<UITabComponent>().Page),journalPage=scene->GetEntityByUUID(journalTab.GetComponent<UITabComponent>().Page);
    edit(settingsTab,"Settings tab caption",[](auto &v){v.Text->Text="Settings";});edit(journalTab,"Journal tab caption",[](auto &v){v.Text->Text="Journal";});
    panel.SetSelectedEntity(tabs);Check(panel.AddUI(18),"Add tab and page native command failed");auto extraTab=panel.GetSelectedEntity();panel.MoveSelected(-1);panel.MoveSelected(1);
    edit(extraTab,"Additional tab caption",[](auto &v){v.Text->Text="Notes";});
    auto notesPage=scene->GetEntityByUUID(extraTab.GetComponent<UITabComponent>().Page);
    panel.SetSelectedEntity(notesPage);Check(panel.AddUI(22),"Single-line text-entry preset failed");auto nameField=panel.GetSelectedEntity();
    edit(nameField,"Name field initial and limit",[](auto &v){v.Rect->Offset={0,48};v.Text->Text="Ada";v.Entry->MaxBytes=512;v.Entry->MaxGraphemes=64;v.Button->Action="profile-name";});
    Check(documents.UndoRedo(false)&&documents.UndoRedo(true),"Text-entry property Undo/Redo failed");
    panel.SetSelectedEntity(notesPage);Check(panel.AddUI(23),"Multiline text-entry preset failed");auto noteField=panel.GetSelectedEntity();
    edit(noteField,"Journal note initial and viewport",[](auto &v){v.Rect->Offset={0,110};v.Rect->FixedSize={540,112};v.Text->Text="A journal note.\nSecond line.\nThird line.\nFourth line.\nFifth line.\nLast line.";v.Entry->MaxBytes=4096;v.Entry->MaxLines=64;v.Button->Action="journal-note";});
    edit(extraTab,"Notes initial focus",[&](auto &v){v.Tab->InitialFocus=nameField.GetUUID();});
    const auto nameFieldID=uint64_t(nameField.GetUUID()),noteFieldID=uint64_t(noteField.GetUUID()),notesTabID=uint64_t(extraTab.GetUUID());
    panel.SetSelectedEntity(settingsPage);Check(panel.AddUI(8),"Tabbed toggle creation failed");auto tabToggle=panel.GetSelectedEntity();edit(tabToggle,"Tabbed toggle",[](auto &v){v.Rect->Offset={0,48};v.Text->Text="Show journal hints";v.Button->Action="journal-hints";});
    panel.SetSelectedEntity(settingsPage);Check(panel.AddUI(11),"Tabbed slider creation failed");auto tabSlider=panel.GetSelectedEntity();edit(tabSlider,"Tabbed slider",[](auto &v){v.Rect->Offset={0,100};v.Rect->FixedSize={420,48};v.Slider->Minimum=0;v.Slider->Maximum=100;v.Slider->Step=5;v.Slider->InitialValue=50;v.Button->Action="tab-volume";});
    edit(settingsTab,"Settings page initial focus",[&](auto &v){v.Tab->InitialFocus=tabSlider.GetUUID();});
    panel.SetSelectedEntity(tabSlider);Check(panel.AddUI(21),"Tooltip native creation failed");auto sliderTip=panel.GetSelectedEntity();edit(sliderTip,"Purpose tooltip",[](auto &v){v.Text->Text="Adjust preview volume. Accept confirms; Cancel restores the edit's starting value.";v.Rect->FixedSize.x=300;});
    auto tipBefore=documents.SceneText();edit(tabSlider,"Tooltip discovery delay",[](auto &v){v.Tooltip->Delay=.25f;});Check(documents.UndoRedo(false)&&documents.UndoRedo(true),"Tooltip property undo/redo failed");
    panel.SetSelectedEntity(settingsPage);Check(panel.AddUI(15),"Tabbed Dropdown creation failed");auto tabDropdown=panel.GetSelectedEntity();edit(tabDropdown,"Tabbed dropdown",[](auto &v){v.Rect->Offset={0,164};v.Rect->FixedSize={280,44};v.Group->Action="journal-format";v.Dropdown->MaximumHeight=160;});
    auto journalHeading=scene->GetEntityByUUID(scene->GetChildrenView(journalPage.GetUUID()).front());edit(journalHeading,"Journal heading",[](auto &v){v.Text->Text="Journal entries";v.Button->Interactable=false;});
    panel.SetSelectedEntity(journalPage);Check(panel.AddUI(19),"Selectable List native preset failed");auto list=panel.GetSelectedEntity();edit(list,"Journal list sizing",[](auto &v){v.Rect->Offset={0,48};v.Rect->FixedSize={540,244};v.Group->Action="journal-entry";});
    panel.SetSelectedEntity(list);Check(panel.AddUI(20),"List row native creation failed");auto listLast=panel.GetSelectedEntity();edit(listLast,"Journal row caption",[](auto &v){v.Text->Text="A longer discovered journal entry with a stable identity";});
    auto listView=scene->GetEntityByUUID(scene->GetChildrenView(list.GetUUID()).front());
    edit(journalTab,"Journal initial focus",[&](auto &v){v.Tab->InitialFocus=list.GetComponent<UIRadioGroupComponent>().InitialSelection;});
    panel.SetSelectedEntity(listLast);Check(panel.AddUI(21),"List row tooltip creation failed");auto rowTip=panel.GetSelectedEntity();edit(rowTip,"Journal row explanation",[](auto &v){v.Text->Text="Select this entry to inspect its consequences. The list remembers one choice.";});
    panel.SetSelectedEntity(menu);Check(panel.AddUI(3),"Tabbed settings command failed");auto tabOpen=panel.GetSelectedEntity();edit(tabOpen,"Tabs and journal command",[](auto &v){v.Rect->Offset={24,350};v.Rect->WidthMode=v.Rect->HeightMode=SizeMode::Fixed;v.Rect->FixedSize={320,44};v.Text->Text="Settings and journal";v.Button->Action="tabs-settings";v.Button->TabOrder=90;});
    edit(tabbed,"Tabs opening focus",[&](auto &v){v.Canvas->Screen.InitialFocus=settingsTab.GetUUID();});
    for(auto ref:{std::pair<const char*,Entity>{"Tabbed",tabbed},{"Tabs",tabs},{"JournalTab",journalTab},{"SettingsTab",settingsTab},{"JournalList",list},{"JournalChoice",listLast},{"NameField",nameField},{"NoteField",noteField},{"NotesTab",extraTab}}){auto &field=ScriptEngine::GetScriptFieldMap(menu)[ref.first];field.Field={ScriptFieldType::Entity,ref.first,nullptr};field.SetValue<uint64_t>(ref.second.GetUUID());}
    const auto tabsID=uint64_t(tabs.GetUUID()),tabbedID=uint64_t(tabbed.GetUUID()),settingsTabID=uint64_t(settingsTab.GetUUID()),journalTabID=uint64_t(journalTab.GetUUID()),settingsPageID=uint64_t(settingsPage.GetUUID()),listID=uint64_t(list.GetUUID()),listViewID=uint64_t(listView.GetUUID()),listLastID=uint64_t(listLast.GetUUID()),tabOpenID=uint64_t(tabOpen.GetUUID()),tabSliderID=uint64_t(tabSlider.GetUUID()),tabDropdownID=uint64_t(tabDropdown.GetUUID()),tabDropdownTrigger=tabDropdown.GetComponent<UIDropdownComponent>().Trigger;
    m_CompositionTabs=tabsID;m_CompositionScreen=tabbedID;m_CompositionJournal=journalTabID;m_CompositionList=listID;m_CompositionView=listViewID;m_CompositionRow=listLastID;
    // Explicit shared appearance, no implicit selector/canvas cascade.
    auto style=[&](Entity target,const char* variant,uint32_t overrides=0){
        if(!target.HasComponent<UIAppearanceComponent>())target.AddComponent<UIAppearanceComponent>();
        edit(target,"Shared UI style",[&](auto& v){v.Appearance->Styled=true;v.Appearance->Selection={"Styles/Settings.hstyle",variant};v.Appearance->Overrides=overrides;});
    };
    style(scene->GetEntityByUUID(first[0]),"Control");style(scene->GetEntityByUUID(first[1]),"Control",UIStyleBit(UIStyleGroup::Foreground));
    edit(scene->GetEntityByUUID(first[1]),"Local caption override",[](auto& v){v.Text->Color={1,.85f,.35f,1};});
    style(journalHeading,"Heading");edit(journalHeading,"Engine heading style",[](auto& v){v.Appearance->Selection.Asset.clear();});
    style(nameField,"Field");style(noteField,"Field");style(volume,"Control",UIStyleBit(UIStyleGroup::Padding));
    style(scene->GetEntityByUUID(dropdown.GetComponent<UIDropdownComponent>().Trigger),"Control",UIStyleBit(UIStyleGroup::Padding));style(settingsTab,"Control",UIStyleBit(UIStyleGroup::Padding));style(journalTab,"Control",UIStyleBit(UIStyleGroup::Padding));style(listLast,"Control",UIStyleBit(UIStyleGroup::Padding));
    {
        auto assets=Project::GetActive()->GetAssets();UIStyleDocument shared(assets);shared.Open("Styles/Settings.hstyle");
        const auto authoredBefore=SceneSerializer(scene).CaptureAuthoredState();auto previous=scene->GetUIPresentation(first[0]);
        shared.Draft().Variants[0].Values.Background={.16f,.22f,.32f,1};shared.Changed();shared.Save();scene->RefreshUI();
        Check(scene->GetUIPresentation(first[0]).Background!=previous.Background&&scene->GetUIPresentation(first[1]).Foreground==glm::vec4(1,.85f,.35f,1)&&SceneSerializer(scene).CaptureAuthoredState()==authoredBefore,"Shared style update changed local overrides or authored component data");
        auto valid=Read(assets->Root()/"Styles/Settings.hstyle");{std::ofstream out(assets->Root()/"Styles/Settings.hstyle");out<<"Invalid replacement";}assets->Reload("Styles/Settings.hstyle");scene->RefreshUI();
        Check(scene->GetUIPresentation(first[0]).Background==shared.Draft().Variants[0].Values.Background,"Malformed style did not retain accepted control presentation");
        {std::ofstream out(assets->Root()/"Styles/Settings.hstyle");out<<valid;}assets->Reload("Styles/Settings.hstyle");scene->RefreshUI();
    }
    panel.SetSelectedEntity(settingsPage);Check(panel.AddUI(2),"Rich journal Text preset failed");auto rich=panel.GetSelectedEntity();rich.GetComponent<TagComponent>().Tag="Emphasis journal";
    edit(rich,"Dialogue emphasis source and spans",[](auto& v){v.Rect->Offset={0,224};v.Rect->FixedSize={540,76};v.Rect->Padding=glm::vec4(4);v.Style->Size=20;v.Style->Wrap=true;v.Style->Overflow=Text::Overflow::Ellipsis;v.Style->MaxLines=2;v.Appearance->Styled=true;v.Appearance->Selection={"","Control"};v.Appearance->Overrides=UIStyleBit(UIStyleGroup::Size)|UIStyleBit(UIStyleGroup::Padding);
        v.Text->Text="Remember the bright beacon. A quiet journal paragraph keeps its full source, wraps and indicates omitted content without changing the saved text.";
        v.Text->Spans={{0,8,FontStyle::Bold,{}},{13,26,FontStyle::Italic,glm::vec4(1,.75f,.3f,.9f)}};});
    const auto richID=uint64_t(rich.GetUUID());auto richBefore=documents.SceneText();
    Check(documents.UndoRedo(false)&&rich.GetComponent<TextComponent>().Spans.empty()&&documents.UndoRedo(true)&&documents.SceneText()==richBefore,"Source+span Inspector Undo/Redo split one edit");
    auto richSource=rich.GetComponent<TextComponent>().TextString;auto richSpans=rich.GetComponent<TextComponent>().Spans;
    edit(rich,"Source edit remaps emphasis",[](auto& v){auto next=std::string("New ")+v.Text->Text;auto remap=Text::RemapSpans(v.Text->Text,next,v.Text->Spans);v.Text->Text=next;v.Text->Spans=std::move(remap.Spans);});
    Check(rich.GetComponent<TextComponent>().Spans.front().Begin==4&&documents.UndoRedo(false)&&rich.GetComponent<TextComponent>().TextString==richSource&&rich.GetComponent<TextComponent>().Spans==richSpans,"Source remap Undo did not restore typed emphasis");
    Prefab::Save(Project::GetAssetDirectory(),"Prefabs/EmphasisParagraph.hprefab",scene,rich);
    auto richTemplate=Prefab::Load(Project::GetAssetDirectory(),"Prefabs/EmphasisParagraph.hprefab");Check(Prefab::GetEntity(richTemplate).GetComponent<TextComponent>().Spans==richSpans,"Rich paragraph prefab lost spans");
    m_EmphasisFixture=richID;
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
    auto scrollID=uint64_t(scrollView.GetUUID()),scrollingID=uint64_t(scrolling.GetUUID()),scrollOpenID=uint64_t(scrollOpen.GetUUID());
    auto volumeID=uint64_t(volume.GetUUID());
    auto dropdownID=uint64_t(dropdown.GetUUID()),dropdownTrigger=dropdown.GetComponent<UIDropdownComponent>().Trigger,dropdownPopup=dropdown.GetComponent<UIDropdownComponent>().Popup;
    auto menuID = uint64_t(menu.GetUUID()), settingsID = uint64_t(settings.GetUUID()),
         confirmationID = uint64_t(confirmation.GetUUID());
    // Exercise structural history on the actual styled composed fixture before
    // save/export, retaining the exact authored result and all named references.
    const auto structuralBaseline=documents.SceneText();panel.SetSelectedEntity(settings);
    Check(panel.DuplicateSelected()&&documents.UndoRedo(false)&&documents.SceneText()==structuralBaseline,"Styled screen duplicate/Undo lost reference data");
    panel.SetSelectedEntity(rich);Check(panel.MoveSelected(-1)&&documents.UndoRedo(false)&&documents.SceneText()==structuralBaseline,"Styled journal reorder/Undo failed");
    panel.SetSelectedEntity(settingsBody);Check(panel.DeleteSelected()&&documents.UndoRedo(false)&&documents.SceneText()==structuralBaseline,"Composed Settings restore lost rich text/styles/control relationships");
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
    m_ScreenFixtureScroll=scrollID;m_ScreenFixtureScrollCanvas=scrollingID;m_ScreenFixtureScrollTarget=farTarget;m_ScreenFixtureDropdown=dropdownID;
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
        Check(runtime->FocusUI(scrollOpenID),"Scrollable settings invoker focus failed");sample.Keys.fill(false);route();sample.Keys[Key::Enter]=true;route();runtime->OnUpdateRuntime(.016f);
        Check(runtime->IsUIScreenOpen(scrollingID),"Owner did not open scroll settings");sample.Keys.fill(false);route();route();
        MouseScrolledEvent wheel(0,-.5f);session.SubmitUIEvent(wheel,{60,140},true);route();Check(runtime->GetUIScroll(scrollID).Offset.y==24,"Native authored fixture fractional wheel failed");
        Check(runtime->EnsureUIVisible(scrollID,farTarget,true)&&runtime->GetUIDiagnostics().Focus==farTarget,"Native fixture nested focus reveal failed");
        Check(runtime->EnsureUIVisible(scrollID,dropdownTrigger,true),"Native Dropdown trigger reveal failed");
        {auto arrow=runtime->GetChildrenView(dropdownTrigger).front();auto t=runtime->GetUISnapshot().Find(dropdownTrigger),a=runtime->GetUISnapshot().Find(arrow);Check(a&&a->Bounds.Position.x>=t->Bounds.Position.x+t->Bounds.Size.x-32,"Dropdown arrow overlapped the caption area");}
        sample.Keys[Key::Enter]=true;route();runtime->OnUpdateRuntime(.016f);sample.Keys.fill(false);route();route();
        Check(runtime->IsUIDropdownOpen(dropdownID)&&runtime->GetUIDiagnostics().Focus==lastOption,"Native Dropdown selected-option reveal failed");
        auto placed=runtime->GetUISnapshot().Find(dropdownPopup);auto outer=runtime->GetUIScroll(scrollID);
        Check(placed->Bounds.Position.x+placed->Bounds.Size.x>outer.Viewport.Position.x+outer.Viewport.Size.x,"Fixture popup did not escape invoking scroll clip at surface edge");
        Check(runtime->EnsureUIVisible(dropdownPopup,firstOption,true),"Native Dropdown first choice reveal failed");sample.Keys[Key::Enter]=true;route();runtime->OnUpdateRuntime(.016f);sample.Keys.fill(false);route();runtime->OnUpdateRuntime(.016f);
        auto receiver=ScriptEngine::GetEntityScriptInstance(menuID);
        Check(runtime->GetUISelection(dropdownID)==firstOption&&!runtime->IsUIDropdownOpen(dropdownID)&&receiver->GetFieldValue<uint64_t>("LastSelection")==firstOption&&runtime->GetUIDiagnostics().Focus==dropdownTrigger,"Native owner lost Dropdown event/return focus");
        sample.Keys[Key::Escape]=true;route();runtime->OnUpdateRuntime(.016f);sample.Keys.fill(false);route();
        Check(!runtime->IsUIScreenOpen(scrollingID)&&runtime->GetUIDiagnostics().Focus==scrollOpenID,"Scroll screen did not return to invoking control");runtime->FocusUI(first[1]);
        sample.Keys[Key::Escape] = true;
        route();
        runtime->OnUpdateRuntime(.016f);
        sample.Keys.fill(false);
        route();
        Check(runtime->GetUIDiagnostics().Focus == first[0],
              "Settings did not return to menu invoker");
        Check(runtime->FocusUI(tabOpenID),"Tabbed screen invoker unavailable");sample.Keys[Key::Enter]=true;route();runtime->OnUpdateRuntime(.016f);sample.Keys.fill(false);route();route();
        Check(runtime->IsUIScreenOpen(tabbedID)&&runtime->GetUIDiagnostics().Focus==settingsTabID,"Tabbed screen initial focus failed");
        Check(runtime->FocusUI(journalTabID),"Journal tab unavailable");sample.Keys[Key::Enter]=true;route();runtime->OnUpdateRuntime(.016f);sample.Keys.fill(false);route();runtime->OnUpdateRuntime(0);route();
        Check(runtime->GetUISelection(tabsID)==journalTabID&&!runtime->GetUISnapshot().Find(settingsPageID)->LayoutResolved,"Native tab selection did not gate inactive page");
        Check(runtime->EnsureUIVisible(listViewID,listLastID,true),"Native selectable-list reveal failed");sample.Keys[Key::Enter]=true;route();runtime->OnUpdateRuntime(.016f);sample.Keys.fill(false);route();
        Check(runtime->GetUISelection(listID)==listLastID&&receiver->GetFieldValue<uint64_t>("LastSelection")==listLastID,"Native list owner selection failed");
        runtime->SetUISelection(tabsID,settingsTabID);runtime->OnUpdateRuntime(.016f);route();route();Check(runtime->GetUIDiagnostics().Focus==tabSliderID,"Page initial focus did not restore/reveal");
        sample.Keys[Key::Enter]=true;route();runtime->OnUpdateRuntime(0);sample.Keys.fill(false);route();Check(runtime->IsUISliderEditing(),"Tabbed slider did not enter editing");
        runtime->SetUISelection(tabsID,journalTabID);runtime->OnUpdateRuntime(0);route();route();Check(!runtime->IsUISliderEditing()&&runtime->GetUISliderValue(tabSliderID)==50,"Page departure left slider editing/stale rollback");
        runtime->SetUISelection(tabsID,settingsTabID);runtime->OnUpdateRuntime(0);route();route();Check(runtime->FocusUI(tabDropdownTrigger),"Tabbed dropdown trigger unavailable");sample.Keys[Key::Enter]=true;route();runtime->OnUpdateRuntime(0);sample.Keys.fill(false);route();route();Check(runtime->IsUIDropdownOpen(tabDropdownID),"Tabbed dropdown did not open");
        runtime->SetUISelection(tabsID,journalTabID);runtime->OnUpdateRuntime(0);route();route();Check(!runtime->IsUIDropdownOpen(tabDropdownID)&&runtime->GetUISnapshot().Scope==tabbedID,"Page departure retained owned dropdown scope");
        runtime->SetUISelection(tabsID,notesTabID);runtime->OnUpdateRuntime(0);route();route();Check(runtime->FocusUI(nameFieldID),"Native name entry focus failed");
        sample.Keys[Key::Enter]=true;route();runtime->OnUpdateRuntime(0);sample.Keys.fill(false);route();route();Check(runtime->IsUITextEditing(),"Native text entry did not start");
        KeyTypedEvent typed(0xe9);editor.m_RuntimeSession.SubmitUIEvent(typed,sample.Point,true);route();runtime->OnUpdateRuntime(0);
        Check(runtime->GetUITextEntry(nameFieldID).find(u8"é")!=std::string::npos,"Production committed-character adapter lost native fixture input");
        KeyPressedEvent submit(Key::Enter,false,0);editor.m_RuntimeSession.SubmitUIEvent(submit,sample.Point,true);route();runtime->OnUpdateRuntime(0);route();route();Check(!runtime->IsUITextEditing()&&receiver->GetFieldValue<int>("TextCommits")>0,"Native text commit/owner delivery failed");
        Check(runtime->FocusUI(noteFieldID),"Native multiline focus failed");sample.Keys[Key::Enter]=true;route();runtime->OnUpdateRuntime(0);sample.Keys.fill(false);route();route();
        KeyPressedEvent noteEnd(Key::End,false,2);editor.m_RuntimeSession.SubmitUIEvent(noteEnd,sample.Point,true);route();runtime->OnUpdateRuntime(0);Check(runtime->IsUITextEditing()&&runtime->GetUIDiagnostics().TextCaret>0,"Constrained note did not keep an end caret");
        KeyPressedEvent noteSubmit(Key::Enter,false,2);editor.m_RuntimeSession.SubmitUIEvent(noteSubmit,sample.Point,true);route();runtime->OnUpdateRuntime(0);route();route();Check(!runtime->IsUITextEditing(),"Multiline native submit failed");
        runtime->SetUITextEntry(noteFieldID,"Silent note\nwith another line");Check(runtime->GetUITextEntry(noteFieldID)=="Silent note\nwith another line","Native notes setter failed");
        sample.Keys[Key::Escape]=true;route();runtime->OnUpdateRuntime(0);sample.Keys.fill(false);route();Check(!runtime->IsUIScreenOpen(tabbedID)&&runtime->GetUIDiagnostics().Focus==tabOpenID,"Tabbed screen Cancel/return focus failed");
        editor.OnSceneStop();
        Check(documents.SceneText() == accepted,
              "Runtime screen changes dirtied authored document");
    }
    if (const auto *output = std::getenv("HAZEL_EDITOR_FONT_FIXTURE"); output && *output)
        for (auto ref : {"Scenes/screens-native.hazel", "Prefabs/screens-native.hprefab", "Styles/Settings.hstyle"})
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
uint64_t m_ScreenFixtureDropdown=0;
uint64_t m_CompositionTabs=0,m_CompositionScreen=0,m_CompositionJournal=0,m_CompositionList=0,m_CompositionView=0,m_CompositionRow=0;
uint64_t m_ScreenFixtureScroll=0,m_ScreenFixtureScrollCanvas=0,m_ScreenFixtureScrollTarget=0;
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
        if(item.HasComponent<UIScrollViewComponent>()||item.HasComponent<UIProgressComponent>()||item.HasComponent<UIRadioGroupComponent>()||(item.HasComponent<UIButtonComponent>()&&item.GetComponent<UIButtonComponent>().Kind!=GameUI::ControlKind::Button)){
            m_ScreenFixturePanel.SetSelectedEntity(item);m_ScreenFixturePanel.RestoreSections({{"UI Button",true},{"UI Radio Group",true},{"UI Progress",true},{"UI Slider",true},{"UI ScrollView",true},{"UI Dropdown",true},{"UI Tabs",true},{"UI Tab",true},{"UI Tooltip",true},{"UI Text Entry",true},{"UI Appearance",true}});m_ScreenFixturePanel.DrawAssetProperties(item);
        }
    }
    m_ScreenFixturePanel.SetSelectedEntity(originalSelection);

    if(auto label=m_ScreenFixture->GetEntityByUUID(m_EmphasisFixture)){
        m_ScreenFixturePanel.SetSelectedEntity(label);m_ScreenFixturePanel.RestoreSections({{"Text Renderer",true}});
        m_ScreenFixturePanel.DrawAssetProperties(label);
        auto draft=label.GetComponent<TextComponent>();auto before=UIEditValues::Capture(label);ImGui::PushID("native-emphasis-preview");ImGui::SetNextItemOpen(true,ImGuiCond_Always);DrawTextEmphasis(draft,false,m_ScreenFixture->GetAssets());ImGui::PopID();
        auto after=before;after.Set(draft);Check(before.Same(after),"Rich Inspector preview changed authored source/spans");
    }
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
    m_ScreenFixture->SetUIPreviewTarget(m_ScreenFixtureScrollCanvas);m_ScreenFixture->RefreshUI();
    auto previewBefore=SceneSerializer(m_ScreenFixture).CaptureAuthoredState();m_ScreenFixture->ScrollUITo(m_ScreenFixtureScroll,{0,.5f},true);
    Check(m_ScreenFixture->EnsureUIVisible(m_ScreenFixtureScroll,m_ScreenFixtureScrollTarget,false),"Native nested Edit preview reveal failed");
    auto dc=m_ScreenFixture->GetEntityByUUID(m_ScreenFixtureDropdown).GetComponent<UIDropdownComponent>();
    Check(m_ScreenFixture->EnsureUIVisible(m_ScreenFixtureScroll,dc.Trigger,false),"Native Dropdown Edit reveal failed");m_ScreenFixture->PreviewUIDropdown(m_ScreenFixtureDropdown,true);

    m_ScreenFixturePreview->Bind();RenderCommand::SetClearColor({0,0,0,1});RenderCommand::Clear();m_ScreenFixture->OnUpdateEditor(0,owner.m_PrefabCamera);m_ScreenFixturePreview->Unbind();
    Check(m_ScreenFixture->GetUIError().empty(),"Native scroll preview render failed");
    Check(SceneSerializer(m_ScreenFixture).CaptureAuthoredState()==previewBefore,"Transient scroll preview dirtied authored state");
    m_ScreenFixture->PreviewUIDropdown(m_ScreenFixtureDropdown,false);
    m_ScreenFixture->SetUIPreviewTarget(m_CompositionScreen);m_ScreenFixture->PreviewUITab(m_CompositionTabs,m_CompositionJournal);
    Check(m_ScreenFixture->EnsureUIVisible(m_CompositionView,m_CompositionRow,false),"Native list Edit reveal failed");m_ScreenFixture->PreviewUITooltip(m_CompositionRow,true);
    m_ScreenFixturePreview->Bind();m_ScreenFixture->OnUpdateEditor(0,owner.m_PrefabCamera);m_ScreenFixturePreview->Unbind();Check(m_ScreenFixture->GetUISnapshot().Tooltips.size()==1,"Native tooltip Edit preview failed");
    m_ScreenFixture->PreviewUITooltip(m_CompositionRow,false);m_ScreenFixture->PreviewUITab(m_CompositionTabs,0);
    GameUI::Action event;
    Check(SceneSerializer(m_ScreenFixture).CaptureAuthoredState() == authored &&
              !m_ScreenFixture->ConsumeUIAction(0, event) && !m_ScreenFixture->ConsumeUIEvent(m_ScreenFixtureMenu,event),
          "Screen Inspector/preview changed authored content or dispatched an action");
    std::cout << "PASS: actual native screen selectors and closed-screen preview, no "
                 "authored/action side effects\n";
}

uint64_t m_EmphasisFixture=0;
