// Native creation commands + shared Inspector transactions; synthetic device samples are explicit.
void UIActionWorkflowChecks() {
    using namespace GameUI;
    auto& editor=m_Editor;auto& owner=*editor.m_Authoring;auto original=editor.m_EditorScenePath;
    editor.NewScene();auto scene=editor.m_EditorScene;auto& panel=editor.m_SceneHierarchyPanel;
    auto edit=[&](Entity target,const char* label,auto change){panel.SetSelectedEntity(target);auto before=UIEditValues::Capture(target),after=before;change(after);UIInspectorEditing inspector;PropertyUI::EditActivity activity;activity.Changed=activity.Committed=true;Check(inspector.Edit(*scene,&owner.m_SceneHistory,target.GetUUID(),label,before,after,activity,0),inspector.Error());};
    Check(panel.AddUI(0),"UI action fixture Canvas creation failed");auto canvas=panel.GetSelectedEntity();auto canvasID=uint64_t(canvas.GetUUID());
    edit(canvas,"Menu scope",[](auto& v){v.Canvas->ReferenceSize={640,480};v.Canvas->SafeInsets={};v.Canvas->Modal=true;});
    std::vector<uint64_t> ids;
    for(int i=0;i<4;++i){panel.SetSelectedEntity(canvas);Check(panel.AddUI(3),"UI action fixture Button creation failed");auto e=panel.GetSelectedEntity();ids.push_back(e.GetUUID());edit(e,"Spatial button",[&](auto& v){v.Rect->WidthMode=v.Rect->HeightMode=SizeMode::Fixed;v.Rect->FixedSize={200,56};v.Rect->Offset={float(40+(i%2)*260),float(60+(i/2)*100)};v.Text->Text=i==0?"Continue":i==1?"New game":i==2?"Options":"Back";v.Button->TabOrder=i;v.Button->Action="menu-"+std::to_string(i);});}
    auto path=Project::GetAssetDirectory()/"Scenes/actions-native.hazel";editor.m_EditorScenePath=path;
    Check(editor.SaveScene()&&editor.OpenScene(path),"UI action scene save/reopen failed");scene=editor.m_EditorScene;
    Prefab::Save(Project::GetAssetDirectory(),"Prefabs/actions-native.hprefab",scene,scene->GetEntityByUUID(canvasID),true);
    owner.SelectAsset(Project::GetAssetDirectory()/"Prefabs/actions-native.hprefab");Check(bool(owner.m_PrefabScene),"UI action prefab reopen failed");owner.ClosePrefab();
    auto authored=owner.SceneText();auto bindingBaseline=Project::GetActive()->GetUIBindings();
    // Real settings form, existing file transaction and exact descriptor round trip.
    owner.m_UIBindings.Values=bindingBaseline;owner.m_UIBindings.Values.Actions[size_t(UIAction::Accept)].Alternate=Key::Z;Check(owner.m_UIBindings.Valid(),"Native binding form rejected valid alternate");
    auto acceptedBindings=SerializeBindings(owner.m_UIBindings.Values);
    owner.m_UIBindings.CaptureAction=int(UIAction::Accept);owner.m_UIBindings.CancelCapture();
    Check(!owner.m_UIBindings.Capturing()&&SerializeBindings(owner.m_UIBindings.Values)==acceptedBindings,"Cancelled native rebinding modified the form");
    auto candidate=CreateRef<Project>();candidate->GetConfig()=Project::GetActive()->GetConfig();candidate->GetConfig().UIBindings=owner.m_UIBindings.Values;
    auto file=editor.m_ProjectFile;file.Save(ProjectSerializer(candidate).SerializeText(),Resources::Get().UserData/"recovery");editor.m_ProjectFile=std::move(file);Project::GetActive()->GetConfig().UIBindings=owner.m_UIBindings.Values;
    auto reopened=Project::LoadCandidate(editor.m_ProjectPath);Check(reopened&&reopened->GetUIBindings().Actions[size_t(UIAction::Accept)].Alternate==Key::Z,"Native project binding save/reopen lost defaults");
    for(int repeat=0;repeat<3;++repeat){Check(editor.OnScenePlay(true),"UI action fixture Play failed");auto running=editor.m_ActiveScene;auto& session=editor.m_RuntimeSession;session.Resize(640,480);session.ResizeFramebuffer(640,480);DeviceSample sample;sample.Point={600,450};
        auto route=[&]{session.SetInput(sample.Point,true);session.RouteUIInput(sample,.016f,true);};route();sample.Keys[Key::Right]=true;route();Check(running->GetUIDiagnostics().Focus==ids[1],"Native menu directional keyboard navigation failed");
        sample.Keys.fill(false);route();sample.Keys[Key::Z]=true;route();Action action;Check(running->ConsumeUIAction(0,action)&&action.Entity==ids[1]&&!running->ConsumeUIAction(0,action),"Authored alternate Accept failed/duplicated in editor Play");
        sample.Keys.fill(false);route();sample.Keys[Key::Escape]=true;route();Check(running->ConsumeUICancel(canvasID),"Editor Play scoped Cancel failed");
        sample.Keys.fill(false);route();sample.Keys[Key::Z]=true;route();
        auto displaySize=editor.m_ViewportSize;bool focused=editor.m_ViewportFocused;
        editor.m_ViewportSize={};editor.m_ViewportFocused=true;
        MouseButtonPressedEvent outside(Mouse::ButtonLeft);editor.OnEvent(outside);
        Check(!session.IsInputEnabled()&&!running->ConsumeUIAction(0,action),"Outside editor press retained viewport ownership/pending activation before ImGui updated focus");
        editor.m_ViewportSize=displaySize;editor.m_ViewportFocused=focused;
        editor.OnSceneStop();Check(owner.SceneText()==authored,"UI navigation or overrides entered authoring history");
    }
    owner.m_UIBindings.Values=bindingBaseline;
    if(const auto* output=std::getenv("HAZEL_EDITOR_FONT_FIXTURE");output&&*output){for(auto file:{"Scenes/actions-native.hazel","Prefabs/actions-native.hprefab"}){auto target=std::filesystem::u8path(output)/file;std::filesystem::create_directories(target.parent_path());std::filesystem::copy_file(Project::GetAssetDirectory()/file,target,std::filesystem::copy_options::overwrite_existing);}
        std::ofstream(std::filesystem::u8path(output)/"Bindings.hproj")<<ProjectSerializer(candidate).SerializeText();
    }
    Check(editor.OpenScene(original),"UI action fixture did not restore editor document");
    std::cout<<"PASS: native menu/spatial authoring, binding descriptor save/reopen, prefab and three keyboard adapter Play/Stop checks (synthetic input)\n";
}
