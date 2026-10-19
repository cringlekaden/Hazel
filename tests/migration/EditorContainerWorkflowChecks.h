// Runs inside the real ImGui editor harness, against a disposable project.
void ContainerWorkflowChecks()
{
    using namespace GameUI;
    auto &editor=m_Editor;auto &owner=*editor.m_Authoring;auto original=editor.m_EditorScenePath;
    editor.NewScene();auto scene=editor.m_EditorScene;auto &panel=editor.m_SceneHierarchyPanel;auto &history=owner.m_SceneHistory;
    scene->OnViewportResize(640,480);scene->CreateEntity("UI fixture camera").AddComponent<CameraComponent>();
    auto edit=[&](Entity target,const char *name,auto change){
        panel.SetSelectedEntity(target);auto before=UIEditValues::Capture(target),after=before;change(after);
        UIInspectorEditing inspector;PropertyUI::EditActivity activity;activity.Changed=activity.Committed=true;
        Check(inspector.Edit(*scene,&history,target.GetUUID(),name,before,after,activity,0),inspector.Error());
    };
    Check(panel.AddUI(0),"Container fixture Canvas command failed");auto canvas=panel.GetSelectedEntity();
    edit(canvas,"Reference surface",[](auto &v){v.Canvas->ReferenceSize={640,480};v.Canvas->SafeInsets={};});
    Check(panel.AddUI(5),"Grid creation command failed");auto grid=panel.GetSelectedEntity();
    edit(grid,"Grid tracks",[](auto &v){v.Rect->Offset={20,20};v.Rect->FixedSize={300,160};v.Flow->Columns.assign(3,{TrackMode::Fill,1});v.Flow->Rows.assign(2,{TrackMode::Fixed,64});v.Flow->GridGap={8,8};});
    std::vector<uint64_t> icons;
    for(int i=0;i<3;++i){panel.SetSelectedEntity(grid);Check(panel.AddUI(3),"Grid button creation command failed");auto button=panel.GetSelectedEntity();icons.push_back(button.GetUUID());
        edit(button,"Icon presentation",[&](auto &v){v.Text->Text=i==0?"Map":i==1?"Journal":"Settings";v.Image->Source=TextureSpriteSource{"Textures/history-image.png",1,{}};v.Rect->Fit=ImageFit::Contain;v.Rect->Padding=glm::vec4(4);v.Style->Size=18;});}
    edit(grid,"Full grid",[](auto &v){v.Flow->Rows.resize(1);});auto full=SceneSerializer(scene).CaptureAuthoredState();panel.SetSelectedEntity(grid);
    Check(!panel.AddUI(3)&&SceneSerializer(scene).CaptureAuthoredState()==full,"Full grid creation left an invalid child/entity");
    panel.SetSelectedEntity(canvas);Check(panel.AddUI(6),"Wrap creation command failed");auto wrap=panel.GetSelectedEntity();
    edit(wrap,"Responsive wrapping",[](auto &v){v.Rect->WidthMode=SizeMode::Fill;v.Rect->Offset={0,220};v.Rect->Margin={20,0,20,0};v.Flow->Gap=8;v.Flow->LineGap=10;});
    std::vector<uint64_t> chips;
    for(int i=0;i<3;++i){panel.SetSelectedEntity(wrap);Check(panel.AddUI(3),"Wrap button command failed");auto button=panel.GetSelectedEntity();chips.push_back(button.GetUUID());
        edit(button,"Wrapping item",[&](auto &v){v.Rect->WidthMode=SizeMode::Fixed;v.Rect->FixedSize.x=220;v.Text->Text=i==0?"Collected items":i==1?"Available quests":"Discovered locations";});}
    panel.SetSelectedEntity(canvas);Check(panel.AddUI(1),"Image panel command failed");auto imagePanel=panel.GetSelectedEntity();
    edit(imagePanel,"Image panel position",[](auto &v){v.Rect->Offset={360,20};v.Rect->FixedSize={240,180};});
    Check(panel.AddUI(3),"Combined image/text command failed");auto combined=panel.GetSelectedEntity();
    edit(combined,"Cover with wrapped text",[](auto &v){v.Rect->Offset={8,8};v.Rect->FixedSize.x=220;v.Rect->Fit=ImageFit::Cover;v.Image->Source=TextureSpriteSource{"Textures/history-image.png",1,{}};v.Text->Text="A panel with a longer journal caption wrapping over an image";v.Style->Size=20;});
    scene->RefreshUI();auto narrow=scene->GetUISnapshot().Find(wrap.GetUUID())->Bounds.Size.y;
    scene->OnViewportResize(960,480);scene->RefreshUI();Check(scene->GetUISnapshot().Find(wrap.GetUUID())->Bounds.Size.y<narrow,"Native authored wrap failed wider viewport reflow");
    scene->OnViewportResize(640,480);scene->RefreshUI();
    const auto accepted=SceneSerializer(scene).CaptureAuthoredState();auto count=history.Operations();
    edit(grid,"Column weights",[](auto &v){v.Flow->Columns[0].Value=2;});
    Check(history.Operations()==count+1&&owner.UndoRedo(false)&&SceneSerializer(scene).CaptureAuthoredState()==accepted&&owner.UndoRedo(true),"Actual document grid edit failed Undo/Redo");
    Check(owner.UndoRedo(false),"Fixture restore failed");panel.SetSelectedEntity(grid);
    UIRectEditSession gesture;Check(gesture.Begin(scene,grid.GetUUID(),4,&history)&&gesture.Update({20,0}),"Grid container Fixed viewport resize failed");gesture.Finish(true);
    Check(SceneSerializer(scene).CaptureAuthoredState()==accepted,"Cancelled grid gesture changed authored fixture");
    Check(!gesture.Begin(scene,icons[0],0,&history),"Grid-owned child accepted ineffective move");
    auto path=m_Directory/"AuthoringProject/Assets/Scenes/container-workflow.hazel";editor.m_EditorScenePath=path;
    Check(editor.SaveScene()&&editor.OpenScene(path)&&owner.SceneText()==accepted,"Container fixture save/reopen lost authored values");scene=editor.m_EditorScene;
    auto reopenedCanvas=scene->FindEntityByName("UI Canvas");Prefab::Save(Project::GetAssetDirectory(),"Prefabs/container-workflow.hprefab",scene,reopenedCanvas,true);
    owner.SelectAsset(Project::GetAssetDirectory()/"Prefabs/container-workflow.hprefab");Check(owner.m_PrefabScene&&owner.m_PrefabScene->FindEntityByName("UI Grid").GetComponent<UILayoutComponent>().Columns.size()==3,"Container prefab editing lost tracks");owner.ClosePrefab();
    panel.SetSelectedEntity(scene->FindEntityByName("UI Grid"));auto authored=owner.SceneText();
    Check(editor.OnScenePlay(true),"Container fixture Play failed");editor.OnSceneStop();Check(owner.SceneText()==authored,"Container fixture Stop imported runtime state");
    Check(editor.OpenScene(original),"Container fixture failed to restore original disposable document");
    std::cout<<"PASS: real editor commands/Inspector transactions: icon grid, responsive wrap, wrapped text with image fits, history/cancel/ownership, save-reopen, prefab and Play-Stop\n";
}
