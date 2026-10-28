// Optional repository-data qualification in a private project copy. Ordinary
// regression fixtures remain independent of the user's Sandbox startup draft.
void ExampleIntegrationWorkflowChecks() {
    const auto source=FileSystem::GetEnvironmentPath("HAZEL_FINAL_UI_PROJECT");
    if(source.empty())return;
    auto& e=m_Editor;
    auto priorProject=e.m_ProjectPath,priorScene=e.m_EditorScenePath;
    const auto copy=m_Directory/"Final tutorial project";
    std::filesystem::copy(source.parent_path(),copy,std::filesystem::copy_options::recursive);
    const auto descriptor=copy/source.filename();
    auto project=YAML::Load(Read(descriptor));project["Project"]["StartScene"]="Scenes/UI Tutorial.hazel";
    {YAML::Emitter out;out<<project;std::ofstream(descriptor)<<out.c_str();}
    Check(e.OpenProject(descriptor),e.m_ActionError);
    auto& documents=*e.m_Authoring;auto& panel=e.m_SceneHierarchyPanel;
    Check(e.m_EditorScene->GetName()=="UI Tutorial","Native project did not open authored tutorial");
    auto scene=e.m_EditorScene;
    const auto baseline=SceneSerializer(scene).CaptureAuthoredState();
    auto heading=scene->FindEntityByName("Tutorial heading");const auto headingID=heading.GetUUID();
    auto before=UIEditValues::Capture(heading),after=before;
    after.Text->Text="A native-authored interface";
    UIInspectorEditing inspector;PropertyUI::EditActivity activity;activity.Changed=activity.Committed=true;
    Check(inspector.Edit(*scene,&documents.m_SceneHistory,headingID,"Tutorial title",before,after,activity,0),inspector.Error());
    panel.SetSelectedEntity(scene->FindEntityByName("Welcome page"));Check(panel.AddUI(24),"Native styled control creation failed in tutorial");
    Check(panel.DuplicateSelected(),"Tutorial native duplicate failed");
    Check(panel.DeleteSelected(DestroyPolicy::Subtree),"Tutorial native delete failed");
    Check(documents.UndoRedo(false)&&documents.UndoRedo(false)&&documents.UndoRedo(false)&&documents.UndoRedo(false),"Tutorial structural/property undo sequence failed");
    Check(SceneSerializer(scene).CaptureAuthoredState()==baseline,"Tutorial command restoration changed authored source/spans/styles/references");
    const auto count=scene->GetAllEntitiesWith<IDComponent>().size();
    documents.InstantiatePrefab(Project::GetAssetDirectory()/"Prefabs/Tutorial card.hprefab");
    Check(scene->GetAllEntitiesWith<IDComponent>().size()>count,"Native tutorial prefab instantiation failed");
    Check(documents.UndoRedo(false)&&SceneSerializer(scene).CaptureAuthoredState()==baseline,"Tutorial prefab instantiation did not undo atomically");
    panel.SetSelectedEntity(scene->FindEntityByName("Tutorial heading"));
    before=UIEditValues::Capture(panel.GetSelectedEntity());after=before;after.Text->Text="Saved tutorial title";
    Check(inspector.Edit(*scene,&documents.m_SceneHistory,headingID,"Saved tutorial title",before,after,activity,0),inspector.Error());
    Check(e.SaveScene(),e.m_ActionError);const auto saved=SceneSerializer(scene).CaptureAuthoredState();
    const auto scenePath=e.m_EditorScenePath;Check(e.OpenScene(scenePath),e.m_ActionError);
    Check(SceneSerializer(e.m_EditorScene).CaptureAuthoredState()==saved,"Tutorial native save/reopen lost authored data");
    for(int i=0;i<2;++i){
        e.OnScenePlay();Check(e.m_SceneState==EditorLayer::SceneState::Play,e.m_ActionError);
        if(i==1){e.m_RuntimeSession.RequestExit();e.OnUpdate(.016f);Check(e.m_SceneState==EditorLayer::SceneState::Edit,"Game Exit closed/retained Play instead of restoring authoring");}
        else e.OnSceneStop();
    }
    Check(SceneSerializer(e.m_EditorScene).CaptureAuthoredState()==saved,"Tutorial Play/Stop changed authoring state");
    Check(e.OpenProject(priorProject),e.m_ActionError);if(!priorScene.empty())Check(e.OpenScene(priorScene),e.m_ActionError);
    std::cout<<"PASS: native tutorial Inspector, composed creation/duplicate/delete/property undo, prefab instantiate, save/reopen and repeated Play/Stop in a private project\n";
}
