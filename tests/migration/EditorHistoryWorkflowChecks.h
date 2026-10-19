// Included inside EditorWorkflowSmoke: exercise the actual document/save/runtime owner.
void HistoryWorkflowChecks()
{
    auto &e=m_Editor;auto &a=*e.m_Authoring;
    const auto original=e.m_EditorScenePath;
    e.NewScene();auto scene=e.m_EditorScene;scene->OnViewportResize(640,480);auto &panel=e.m_SceneHierarchyPanel;
    Check(panel.AddUI(0),"History owner canvas creation failed");auto canvas=panel.GetSelectedEntity();
    Check(panel.AddUI(4), "Measured menu stack preset failed"); auto menu=panel.GetSelectedEntity();
    Check(panel.AddUI(3), "Measured stack button preset failed"); auto start=panel.GetSelectedEntity();
    start.GetComponent<TagComponent>().Tag="Menu start";
    start.GetComponent<TextComponent>().TextString="Start exploring";
    panel.SetSelectedEntity(menu);
    Check(panel.AddUI(3), "Measured stack sibling creation failed"); auto options=panel.GetSelectedEntity();
    options.GetComponent<TagComponent>().Tag="Menu options";
    scene->RefreshUI(); auto menuHeight=scene->GetUISnapshot().Find(menu.GetUUID())->Bounds.Size.y;
    options.GetComponent<TextComponent>().TextString="Options and journal settings with a deliberately longer translated label for wrapping";
    scene->RefreshUI();
    Check(scene->GetUISnapshot().Find(menu.GetUUID())->Bounds.Size.y>menuHeight &&
              options.GetComponent<UIRectComponent>().WidthMode==GameUI::SizeMode::Fill,
          "Editor menu preset did not reflow a longer native label");
    panel.SetSelectedEntity(canvas);
    Check(panel.AddUI(2), "Measured HUD text preset failed"); auto hud=panel.GetSelectedEntity();
    hud.GetComponent<TagComponent>().Tag="HUD journal";
    hud.GetComponent<TextComponent>().TextString="Journal: 1 / 9";
    auto &hr=hud.GetComponent<UIRectComponent>();
    hr.Anchor=hr.Anchor=hr.Pivot={1,1};hr.Offset={-24,-24};
    scene->RefreshUI();
    auto menuBounds=scene->GetUISnapshot().Find(menu.GetUUID())->Bounds;
    scene->OnViewportResize(960,540);scene->RefreshUI();
    Check(scene->GetUISnapshot().Find(menu.GetUUID())->Bounds.Size==menuBounds.Size &&
              scene->GetUISnapshot().Find(hud.GetUUID())->PixelBounds.Position.x>640,
          "Editor viewport resize changed Fixed stack size or failed corner HUD anchoring");
    scene->OnViewportResize(640,480);scene->RefreshUI();panel.SetSelectedEntity(canvas);
    Check(panel.AddUI(3),"History owner button creation failed");auto control=panel.GetSelectedEntity();
    a.m_ActiveDocument=EditorDocument::Scene;auto &h=a.m_SceneHistory;
    const auto initial=a.SceneText();UIInspectorEditing inspector;
    PropertyUI::EditActivity activity;activity.Item=401;activity.Label="Position";activity.Changed=true;
    auto before=UIEditValues::Capture(control),next=before;next.Rect->Offset.x+=13;
    Check(inspector.Edit(*scene,&h,control.GetUUID(),"UI Rect",before,next,activity,401),"Owned Inspector preview failed");
    const auto file=m_Directory/"AuthoringProject/Assets/Scenes/history-workflow.hazel";
    e.m_EditorScenePath=file;
    Check(e.SaveScene()&&!h.Active()&&h.Operations()==1&&!h.Dirty(a.SceneText()),"Save did not finish Inspector transaction/establish accepted point");
    inspector.Poll(*scene,&h,control.GetUUID(),true,true,false,0);
    const auto written=Read(file),saved=a.SceneText();
    Check(a.UndoRedo(false)&&a.SceneText()==initial&&h.Dirty(a.SceneText()),"Undo across successful save did not restore dirty state");
    Check(a.UndoRedo(true)&&a.SceneText()==saved&&!h.Dirty(a.SceneText()),"Redo to accepted save content did not clear dirty state");
    Check(e.m_UIEdit.Begin(scene,control.GetUUID(),4,&h)&&e.m_UIEdit.Update({20,0}),"Owned viewport history setup failed");e.m_UIEdit.Finish(false);
    const auto edited=a.SceneText();Check(h.Operations()==2&&h.Dirty(edited),"Viewport owner did not commit one entry");
    {std::ofstream out(file);out<<"Controlled foreign disk edit";}
    Check(!e.SaveScene()&&h.SavedAuthored()==saved&&a.SceneText()==edited&&Read(file)=="Controlled foreign disk edit","Failed save accepted save point or overwrote foreign file/draft");
    {std::ofstream out(file);out<<written;}
    Check(a.UndoRedo(false)&&!h.Dirty(a.SceneText()),"Undo after failed save did not reach accepted content");
    control.GetComponent<UIRectComponent>().Padding.y+=3;
    Check(h.Dirty(a.SceneText()),"Out-of-history authored mutation was hidden at save cursor");
    control.GetComponent<UIRectComponent>().Padding.y-=3;
    SpriteSheetDefinition sheet;sheet.Texture="Textures/Checkerboard.png";sheet.Sampling.Width=sheet.Sampling.Height=32;sheet.Regions={{1,"History region",{0,0,4,4}}};
    SaveSpriteSheet(Project::GetAssetDirectory(),"Textures/history-assignment.hsprites",sheet);
    panel.SetSelectedEntity(control);const auto target=a.AssignmentTarget();
    Check(a.m_Sprites.AssignSprite({"Textures/history-assignment.hsprites",1},target)&&h.UndoName()=="Assign UI sprite"&&!h.CanRedo(),"Sprite-sheet UI assignment bypassed document history or retained redo branch");
    Check(a.UndoRedo(false)&&a.SceneText()==saved,"UI assignment Undo did not restore the accepted source");
    auto retired=scene->CreateEntity("Retired assignment target");retired.AddComponent<UIRectComponent>();retired.AddComponent<SpriteRendererComponent>();panel.SetSelectedEntity(retired);
    const auto stale=a.AssignmentTarget();scene->DestroyEntity(retired);auto replacement=scene->CreateEntityWithUUID(stale.Entity,"Reused assignment UUID");replacement.AddComponent<UIRectComponent>();replacement.AddComponent<SpriteRendererComponent>();
    Check(!a.m_Sprites.AssignSprite({"Textures/history-assignment.hsprites",1},stale)&&replacement.GetComponent<SpriteRendererComponent>().Source.index()==0,"Reused UUID accepted a retired asset-assignment target");scene->DestroyEntity(replacement);
    const auto count=h.Operations();const auto identity=h.Identity();
    panel.SetSelectedEntity(control);
    for(int repeat=0;repeat<2;++repeat){
        Check(e.OnScenePlay(true),"History Play failed");
        auto runtime=e.m_ActiveScene;runtime->FindEntityByName("UI Button").GetComponent<TextComponent>().TextString="Runtime only";
        Check(!a.UndoRedo(false)&&h.Operations()==count,"Play allowed authored history mutation");e.OnSceneStop();
        Check(e.m_EditorScene==scene&&h.Identity()==identity&&h.Operations()==count&&a.SceneText()==saved&&panel.GetSelectedEntity().GetUUID()==control.GetUUID(),"Play/Stop lost history/selection or imported runtime values");
    }
    Check(e.OnSceneSimulate(true)&&!a.UndoRedo(false),"Simulate allowed history mutation");e.OnSceneStop();
    Check(a.SceneText()==saved&&h.Operations()==count,"Simulate/Stop altered authored history");
    // Unreleased gestures and Inspector edits must not enter the runtime copy.
    Check(e.m_UIEdit.Begin(scene,control.GetUUID(),0,&h)&&e.m_UIEdit.Update({12,8}),"Play cancellation gesture setup failed");
    Check(e.OnScenePlay(true)&&!e.m_UIEdit.Active()&&!h.Active()&&a.SceneText()==saved,"Play copied a partial authoring gesture");e.OnSceneStop();
    const auto prefabPath=Project::GetAssetDirectory()/"Prefabs/history-workflow.hprefab";
    Prefab::Save(Project::GetAssetDirectory(),"Prefabs/history-workflow.hprefab",scene,canvas,true);
    a.SelectAsset(prefabPath);Check(a.m_PrefabScene&&a.m_PrefabHistory.Operations()==0,"Prefab history did not bind new document");
    auto prefab=a.m_PrefabScene;auto pcontrol=prefab->FindEntityByName("UI Button");auto &ph=a.m_PrefabHistory;
    auto pbefore=UIEditValues::Capture(pcontrol),pnext=pbefore;pnext.Rect->FixedSize.x+=11;
    UIInspectorEditing pedit;PropertyUI::EditActivity immediate;immediate.Changed=immediate.Committed=true;immediate.Label="Preferred size";
    const auto retainedScene=a.SceneText();
    Check(pedit.Edit(*prefab,&ph,pcontrol.GetUUID(),"UI Rect",pbefore,pnext,immediate,0)&&ph.Operations()==1,"Prefab Inspector edit failed");
    a.m_ActiveDocument=EditorDocument::Prefab;
    Check(a.SaveActive()&&!ph.Dirty(SceneSerializer(prefab).CaptureAuthoredState()),"Prefab save point not accepted");
    const auto psaved=SceneSerializer(prefab).CaptureAuthoredState();
    Check(a.UndoRedo(false)&&ph.Dirty(SceneSerializer(prefab).CaptureAuthoredState())&&a.SceneText()==retainedScene&&h.Operations()==count,"Prefab Undo mixed document histories");
    Check(a.UndoRedo(true)&&SceneSerializer(prefab).CaptureAuthoredState()==psaved,"Prefab Redo did not restore saved state");
    a.ClosePrefab();Check(!a.m_PrefabScene&&ph.Operations()==0&&ph.Identity()==0,"Closed prefab retained stale history");
    a.SelectAsset(prefabPath);Check(a.m_PrefabScene&&a.m_PrefabHistory.Operations()==0&&SceneSerializer(a.m_PrefabScene).CaptureAuthoredState()==psaved,"Prefab save/reopen changed authored results/history lifetime");a.ClosePrefab();
    Check(!e.OpenScene(m_Directory/"missing-history-scene.hazel")&&e.m_EditorScene==scene&&h.Operations()==count,"Failed Open discarded valid history");
    Check(e.OpenScene(file)&&h.Identity()!=identity&&h.Operations()==0&&a.SceneText()==saved,"Successful Open reused old history or changed saved values");
    auto opened=e.m_EditorScene;auto openedControl=opened->FindEntityByName("UI Button");auto openedBefore=UIEditValues::Capture(openedControl),openedAfter=openedBefore;openedAfter.Rect->Offset.y+=5;
    Check(h.Begin(*opened,CreateScope<UIEditCommand>(*opened,openedControl.GetUUID(),"Future document command",openedBefore)),"Replacement transaction setup failed");
    std::string error;Check(dynamic_cast<UIEditCommand*>(h.Transaction())->Preview(*opened,openedAfter,error),error.c_str());
    e.NewScene();Check(h.Operations()==0&&!h.Active()&&SceneSerializer(opened).CaptureAuthoredState()==saved,"New retained old document history or failed to cancel a shared document transaction");
    Check(e.OpenScene(original),"History fixture did not restore the original disposable document");
    e.m_ActionError.clear();
    std::cout<<"PASS: actual scene/prefab history ownership, accepted/failed saves, independent drafts, save/reopen, Open/New/Close and repeated Play/Simulate/Stop\n";
}
