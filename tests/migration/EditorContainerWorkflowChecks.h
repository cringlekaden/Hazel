// Runs inside the real ImGui editor harness, against a disposable project.
void ContainerWorkflowChecks()
{
    using namespace GameUI;
    auto &editor=m_Editor;auto &owner=*editor.m_Authoring;auto original=editor.m_EditorScenePath;
    editor.NewScene();auto scene=editor.m_EditorScene;auto &panel=editor.m_SceneHierarchyPanel;auto &history=owner.m_SceneHistory;
    scene->OnViewportResize(640,480);scene->CreateEntity("UI fixture camera").AddComponent<CameraComponent>();
    // Disposable source art with three distinct regions, authored by the native fixture.
    {
        unsigned char header[18]{};header[2]=2;header[12]=96;header[14]=32;header[16]=32;header[17]=0x28;
        std::ofstream out(Project::GetAssetDirectory()/"Textures/container-icons.tga",std::ios::binary);out.write(reinterpret_cast<char*>(header),18);
        for(int y=0;y<32;++y)for(int x=0;x<96;++x){unsigned char color[4]{(unsigned char)(x>=64?255:0),(unsigned char)(x>=32&&x<64?255:0),(unsigned char)(x<32?255:0),255};out.write(reinterpret_cast<char*>(color),4);}
    }
    SpriteSheetDefinition iconsSheet;iconsSheet.Texture="Textures/container-icons.tga";iconsSheet.Sampling.Width=96;iconsSheet.Sampling.Height=32;
    iconsSheet.Regions={{1,"Map",{0,0,32,32}},{2,"Journal",{32,0,32,32}},{3,"Settings",{64,0,32,32}}};SaveSpriteSheet(Project::GetAssetDirectory(),"Textures/container-icons.hsprites",iconsSheet);
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
        edit(button,"Icon presentation",[&](auto &v){v.Text->Text=i==0?"Map":i==1?"Journal":"Settings";v.Image->Source=SpriteReference{"Textures/container-icons.hsprites",uint64_t(i+1)};v.Rect->Fit=ImageFit::Contain;v.Rect->Padding=glm::vec4(4);v.Style->Size=18;});}
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
    FramebufferSpecification paint;paint.Width=640;paint.Height=480;paint.Attachments={FramebufferTextureFormat::RGBA8,FramebufferTextureFormat::RED_INTEGER,FramebufferTextureFormat::Depth};
    auto target=Framebuffer::Create(paint);auto viewport=RenderCommand::GetRasterState().Viewport;
    for(int i=0;i<3;++i){
        Check(editor.OnScenePlay(true),"Container fixture Play failed");
        auto running=editor.m_ActiveScene;running->OnViewportResize(640,480);running->SetUIFramebufferSize({640,480});
        target->Bind();RenderCommand::Clear();target->ClearAttachment(1,-1);running->OnUpdateRuntime(0);
        Check(running->GetUIDiagnostics().DrawCalls>0&&running->GetUIDiagnostics().PreparedRebuilds>0,"Editor Play omitted the real UI pass/preparation");
        running->OnUpdateRuntime(0);Check(running->GetUIDiagnostics().PreparedReuses>0,"Editor Play rebuilt unchanged glyph data");
        target->Unbind();editor.OnSceneStop();Check(owner.SceneText()==authored,"Container fixture Stop imported runtime paint/history state");
    }
    RenderCommand::SetViewport(viewport.x,viewport.y,viewport.z,viewport.w);
    m_ContainerFixture=scene;m_ContainerFixturePanel.SetContext(scene);m_ContainerFixturePanel.SetSelectedEntity(scene->FindEntityByName("UI Grid"));m_ContainerIcon=icons.front();
    Check(editor.OpenScene(original),"Container fixture failed to restore original disposable document");
    std::cout<<"PASS: real editor commands/Inspector transactions: icon grid, responsive wrap, wrapped text with image fits, history/cancel/ownership, save-reopen, prefab and Play-Stop\n";
}

void DrawContainerFixture()
{
    if(!m_ContainerFixture)return;
    auto before=SceneSerializer(m_ContainerFixture).CaptureAuthoredState();
    const auto origin=ImGui::GetMainViewport()->Pos;
    ImGui::SetNextWindowPos({origin.x+40,origin.y+60});ImGui::SetNextWindowSize({440,380});
    ImGui::Begin("Container qualification",nullptr,ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoDocking|ImGuiWindowFlags_NoFocusOnAppearing|ImGuiWindowFlags_NoInputs);
    auto &a=*m_Editor.m_Authoring;auto scene=a.m_PrefabScene;auto buffer=a.m_PrefabPreview;auto camera=a.m_PrefabCamera;bool fit=a.m_PrefabFit;
    a.m_PrefabScene=m_ContainerFixture;a.m_PrefabPreview=m_ContainerPreview;a.m_PrefabFit=true;
    ImGui::GetStateStorage()->SetInt(ImGui::GetID("Subtree preview"),1);a.PrefabPreview();
    Check(bool(a.m_PrefabPreview)&&m_ContainerFixture->GetUIError().empty(),"Actual native container preview failed");
    auto node=m_ContainerFixture->GetUISnapshot().Find(m_ContainerIcon);Check(node&&node->LayoutResolved,"Preview did not arrange the authored icon grid");
    auto point=node->PixelBounds.Position+node->PixelBounds.Size*.5f;auto spec=a.m_PrefabPreview->GetSpecification();a.m_PrefabPreview->Bind();
    auto entity=m_ContainerFixture->GetEntityByUUID(m_ContainerIcon);
    Check(a.m_PrefabPreview->ReadPixel(1,int(point.x),int(spec.Height-point.y))==int(uint32_t(entity)),"Actual native preview picking disagreed with grid geometry");a.m_PrefabPreview->Unbind();
    m_ContainerPreview=a.m_PrefabPreview;a.m_PrefabScene=scene;a.m_PrefabPreview=buffer;a.m_PrefabCamera=camera;a.m_PrefabFit=fit;
    m_ContainerFixturePanel.DrawAssetProperties(m_ContainerFixturePanel.GetSelectedEntity());ImGui::End();
    // Reproduce F5 workspace restoration with a selected, expanded button. The
    // grid-only Inspector above does not exercise button-state array indexing.
    ImGui::SetNextWindowPos({origin.x+500,origin.y+60});ImGui::SetNextWindowSize({400,700});
    ImGui::Begin("Button Inspector qualification",nullptr,ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoDocking|ImGuiWindowFlags_NoFocusOnAppearing|ImGuiWindowFlags_NoInputs);
    Check(m_ContainerFixturePanel.SetSelectedEntity(entity),"Button Inspector selection retired unexpectedly");
    m_ContainerFixturePanel.RestoreSections({{"UI Button",true},{"UI Text Style",true}});
    ImGui::PushID(std::to_string(m_ContainerFixture->GetIdentity()).c_str());
    ImGui::PushID(std::to_string(uint64_t(entity.GetUUID())).c_str());
    ImGui::PushID("UI Button");ImGui::PushID("##component");
    for(size_t i=0;i<entity.GetComponent<UIButtonComponent>().Colors.size();++i){
        ImGui::PushID(int(i));ImGui::GetStateStorage()->SetInt(ImGui::GetID("State image (optional)"),1);ImGui::PopID();
    }
    ImGui::PopID();ImGui::PopID();ImGui::PopID();ImGui::PopID();
    m_ContainerFixturePanel.DrawAssetProperties(entity);
    Check(m_ContainerFixturePanel.Sections().at("UI Button"),"Restored button Inspector stayed collapsed");
    ImGui::End();
    std::cout<<"PASS: restored selected button Inspector with all five tint/state-image editors expanded\n";
    GameUI::Action action;Check(SceneSerializer(m_ContainerFixture).CaptureAuthoredState()==before&&!m_ContainerFixture->ConsumeUIAction(0,action),"Native Inspector/preview changed authored values or dispatched actions");
    std::cout<<"PASS: actual ImGui container Inspector/static preview, arranged GPU picking and no authoring/action side effects\n";
}
