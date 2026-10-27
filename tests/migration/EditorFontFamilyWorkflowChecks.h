// Inside EditorWorkflowSmoke: the normal native asset commands/document guard,
// shared Inspector history and actual ImGui asset panel; no desktop injection.
void FontFamilyWorkflowChecks() {
    auto& editor=m_Editor;auto& owner=*editor.m_Authoring;auto assets=Project::GetActive()->GetAssets();auto original=editor.m_EditorScenePath;
    assets->ImportFontFile(Resources::Resolve("fonts/opensans/OpenSans-Regular.ttf"),"Fonts/Family-Regular.ttf",true);
    assets->ImportFontFile(Resources::Resolve("fonts/opensans/OpenSans-Bold.ttf"),"Fonts/Family-Bold.ttf",true);
    assets->ImportFontFile(Resources::Resolve("fonts/opensans/LICENSE.txt"),"Fonts/Family-NOTICE.txt",false);
    Check(bool(editor.m_ContentBrowserPanel->CreateFontFamily),"Content Browser has no native font-family command");
    editor.m_ContentBrowserPanel->CreateFontFamily("Fonts");Check(owner.m_Fonts.m_Create,"Browser failed to open native family creation form");owner.m_Fonts.m_Create=false;
    FontFamilyDefinition def;def.Name="Native family";def.Faces[0]={"Fonts/Family-Regular.ttf",{"Fonts/Family-NOTICE.txt"}};def.Faces[1]={"Fonts/Family-Bold.ttf",{"Fonts/Family-NOTICE.txt"}};def.Required="English café";def.Sample="é Ω";
    owner.Guard(OperationIntent::OpenFontFamily,[&]{return owner.m_Fonts.Create("Fonts/Native.hfont",def);});
    Check(owner.m_Fonts.HasDocument() && !owner.m_Documents.Pending(),"Native family creation/document publication failed");
    owner.m_Fonts.m_Document->Draft().Name="Edited native family";owner.m_Fonts.m_Document->Changed();owner.m_ActiveDocument=EditorDocument::FontFamily;
    auto docs=owner.Documents();Check(std::any_of(docs.begin(),docs.end(),[](const auto& d){return d.Kind==EditorDocument::FontFamily && d.Dirty;}),"Family draft bypassed shared dirty-document service");
    Check(owner.SaveActive() && !owner.m_Fonts.Dirty(),"Native family Ctrl-S/save service failed");
    owner.m_Fonts.Inspect();Check(!owner.m_Fonts.m_Coverage.empty() && owner.m_Fonts.m_Coverage[0].Required.Satisfied() && owner.m_Fonts.m_Coverage[0].Sample.Satisfied(),"Native family coverage did not use shaped Greek support");
    const auto savedFamily=Read(assets->Root()/"Fonts/Native.hfont");
    owner.m_Fonts.m_Document->Draft().Name="Conflicted";owner.m_Fonts.m_Document->Changed();
    {std::ofstream out(assets->Root()/"Fonts/Native.hfont");out<<"Foreign edit";}
    Check(!owner.m_Fonts.Save() && owner.m_ShowSaveConflict && owner.m_ConflictDocument==EditorDocument::FontFamily,
          "Native family Save button bypassed common save-conflict controls");
    {std::ofstream out(assets->Root()/"Fonts/Native.hfont");out<<savedFamily;}
    owner.m_ShowSaveConflict=false;editor.m_ActionError.clear();
    const auto identity=owner.m_Fonts.Identity();owner.m_Fonts.m_Document->Draft().Name="Unsaved";owner.m_Fonts.m_Document->Changed();
    owner.SelectAsset(assets->Root()/"Fonts/Native.hfont");Check(owner.m_Documents.Pending(),"Family reopen bypassed unsaved guard");
    owner.m_Documents.Resolve(GuardChoice::Cancel,owner.Documents(),{});Check(owner.m_Fonts.Identity()==identity && owner.m_Fonts.Dirty(),"Cancelled family reopen discarded draft");
    owner.SelectAsset(assets->Root()/"Fonts/Native.hfont");Check(owner.m_Documents.Resolve(GuardChoice::Discard,owner.Documents(),{}),"Guarded family discard/reopen failed");
    Check(!owner.m_Fonts.Dirty() && owner.m_Fonts.m_Document->Draft().Name=="Edited native family","Family reopen lost accepted content");
    editor.NewScene();auto scene=editor.m_EditorScene;scene->OnViewportResize(640,480);auto& panel=editor.m_SceneHierarchyPanel;
    Check(panel.AddUI(0)&&panel.AddUI(2),"Family text authoring presets failed");auto text=panel.GetSelectedEntity();
    text.GetComponent<TextComponent>().TextString="Native family: café";
    text.GetComponent<UITextStyleComponent>().Size=32;text.GetComponent<UIRectComponent>().FixedSize={580,64};text.GetComponent<UIRectComponent>().Offset={36,36};
    text.AddComponent<SpriteRendererComponent>(glm::vec4(.08f,.12f,.22f,1));
    owner.m_ActiveDocument=EditorDocument::Scene;auto& history=owner.m_SceneHistory;auto before=UIEditValues::Capture(text),next=before;
    const auto initialOperations=history.Operations();next.Text->Font={"Fonts/Native.hfont",FontStyle::Bold};UIInspectorEditing editing;PropertyUI::EditActivity activity;activity.Changed=activity.Committed=true;
    Check(editing.Edit(*scene,&history,text.GetUUID(),"Text family",before,next,activity,0) && history.Operations()==initialOperations+1,"Family assignment did not use one shared Inspector command");
    Check(owner.UndoRedo(false) && text.GetComponent<TextComponent>().Font.Family.empty() && owner.UndoRedo(true) && text.GetComponent<TextComponent>().Font==next.Text->Font,"Family assignment Undo/Redo lost selection");
    auto path=assets->Root()/"Scenes/family-native.hazel";editor.m_EditorScenePath=path;Check(editor.SaveScene(),"Family scene save failed");
    Check(editor.OpenScene(path),"Family scene reopen failed");scene=editor.m_EditorScene;text=scene->FindEntityByName("UI Text");
    Check(text && text.GetComponent<TextComponent>().Font.Style==FontStyle::Bold,"Family scene save/reopen lost style");
    Prefab::Save(assets->Root(),"Prefabs/family-native.hprefab",scene,text);
    auto prefab=Prefab::Load(assets->Root(),"Prefabs/family-native.hprefab");Check(Prefab::GetEntity(prefab).GetComponent<TextComponent>().Font.Style==FontStyle::Bold,"Native family prefab reuse lost style");
    auto authored=owner.SceneText();
    for(int i=0;i<3;++i){Check(editor.OnScenePlay(true),"Family authored Play failed");auto running=editor.m_ActiveScene;auto live=running->FindEntityByName("UI Text");ResolveTextFont(live.GetComponent<TextComponent>(),assets);editor.OnSceneStop();Check(owner.SceneText()==authored,"Play/Stop imported font resources into authored state/history");}
    auto closure=AuditSpriteAssets(assets->Root(),{"Scenes/family-native.hazel","Prefabs/family-native.hprefab"});
    for(const auto* file:{"Fonts/Native.hfont","Fonts/Family-Regular.ttf","Fonts/Family-Bold.ttf","Fonts/Family-NOTICE.txt"})Check(std::find(closure.begin(),closure.end(),file)!=closure.end(),"Native family export closure incomplete");
    assets->ImportFontFile(std::filesystem::absolute("typography/DejaVuSans.ttf"),"Fonts/Corpus.ttf",true);
    assets->ImportFontFile(std::filesystem::absolute("typography/LICENSE-DejaVu.txt"),"Fonts/Corpus-NOTICE.txt",false);
    FontFamilyDefinition corpus;corpus.Name="Tested typography corpus";corpus.Faces[0]={"Fonts/Corpus.ttf",{"Fonts/Corpus-NOTICE.txt"}};
    corpus.Required=u8"café Ω Привет مرحبا שלום";FontFamilyDocument corpusDocument(assets);corpusDocument.Create("Fonts/Corpus.hfont",corpus);
    Check(InspectFontFamily(assets->Root(),corpus).front().Required.Satisfied(),"Native mixed-script required coverage failed");
    auto corpusScene=Scene::Copy(scene);auto corpusLabel=corpusScene->FindEntityByName("UI Text");
    corpusLabel.GetComponent<TextComponent>().Font={"Fonts/Corpus.hfont"};
    corpusLabel.GetComponent<TextComponent>().TextString=u8"café Ω Привет\nمرحبا Hazel 123\nשלום Hazel (45)";
    corpusLabel.GetComponent<UIRectComponent>().FixedSize={580,200};
    // Maintained constrained labels authored through the actual native commands
    // and shared Inspector transactions, not a handwritten scene generator.
    Check(editor.OpenScene(path),"Overflow native workflow failed to reopen document");
    scene=editor.m_EditorScene;scene->CreateEntity("Overflow fixture camera").AddComponent<CameraComponent>();
    auto canvas=scene->FindEntityByName("UI Canvas");
    std::vector<uint64_t> overflowIDs;
    for(int i=0;i<3;++i) {
        panel.SetSelectedEntity(canvas);Check(panel.AddUI(2),"Overflow native Text preset failed");auto label=panel.GetSelectedEntity();overflowIDs.push_back(label.GetUUID());
        auto start=UIEditValues::Capture(label),after=start;
        after.Text->Font={"Fonts/Corpus.hfont"};
        after.Text->Text=i==0?u8"A longer café label: ffi, composed é and decomposed e\u0301":i==1?u8"مرحبا Hazel 123 — שלום (45) — more source for a constrained paragraph":u8"One visible line\nAnother visible line\nA third line remains authored\n";
        after.Rect->WidthMode=after.Rect->HeightMode=GameUI::SizeMode::Fixed;
        after.Rect->Offset={36,float(150+i*100)};after.Rect->FixedSize={300,80};after.Rect->Padding=glm::vec4(8);
        if(i==0){after.Rect->WidthMode=GameUI::SizeMode::Fill;after.Rect->Offset.x=0;after.Rect->Margin={36,0,900,0};}
        after.Style->Size=24;after.Style->Wrap=i!=0;after.Style->Overflow=i==2?Text::Overflow::Clip:Text::Overflow::Ellipsis;after.Style->MaxLines=i==0?1:2;
        auto count=owner.m_SceneHistory.Operations();
        Check(editing.Edit(*scene,&owner.m_SceneHistory,label.GetUUID(),"Text overflow",start,after,activity,0),editing.Error());
        Check(owner.m_SceneHistory.Operations()==count+1 && owner.UndoRedo(false) && label.GetComponent<TextComponent>().TextString==start.Text->Text && owner.UndoRedo(true) && label.GetComponent<UITextStyleComponent>().MaxLines==after.Style->MaxLines,"Overflow Inspector transaction/source Undo/Redo failed");
    }
    auto overflowPath=assets->Root()/"Scenes/overflow-native.hazel";editor.m_EditorScenePath=overflowPath;Check(editor.SaveScene() && editor.OpenScene(overflowPath),"Overflow native scene save/reopen failed");scene=editor.m_EditorScene;
    Prefab::Save(assets->Root(),"Prefabs/overflow-native.hprefab",scene,scene->FindEntityByName("UI Canvas"),true);
    owner.SelectAsset(assets->Root()/"Prefabs/overflow-native.hprefab");Check(bool(owner.m_PrefabScene),"Overflow prefab editor failed");owner.ClosePrefab();
    auto overflowAuthored=owner.SceneText();
    FramebufferSpecification specification;specification.Width=640;specification.Height=480;specification.Attachments={FramebufferTextureFormat::RGBA8,FramebufferTextureFormat::RED_INTEGER,FramebufferTextureFormat::Depth};auto overflowTarget=Framebuffer::Create(specification);auto overflowViewport=RenderCommand::GetRasterState().Viewport;
    for(int iteration=0;iteration<3;++iteration) {
        Check(editor.OnScenePlay(true),"Overflow Play failed");auto running=editor.m_ActiveScene;running->OnViewportResize(640,480);running->SetUIFramebufferSize({640,480});overflowTarget->Bind();RenderCommand::Clear();running->OnUpdateRuntime(0);running->OnUpdateRuntime(0);
        Check(running->GetUIDiagnostics().PreparedReuses>=3,"Overflow runtime did not reuse prepared text");
        auto wrapped=running->GetUITextLayout(overflowIDs[1]);
        Check(wrapped && wrapped->Omitted && wrapped->Lines.size()==2 && wrapped->Lines.back().Ellipsized && running->GetUISnapshot().Find(overflowIDs[1])->Bounds.Size.x==300,"Native paragraph did not demonstrate constrained wrapped ellipsis");
        auto narrow=running->GetUITextLayout(overflowIDs.front());Check(narrow && narrow->Omitted,"Native constrained label did not truncate");
        running->OnViewportResize(960,480);running->SetUIFramebufferSize({640,480});running->OnUpdateRuntime(0);
        auto wide=running->GetUITextLayout(overflowIDs.front());Check(wide && wide->Lines[0].End>narrow->Lines[0].End,"Native overflow preview/Play resize did not restore more source");
        overflowTarget->Unbind();editor.OnSceneStop();Check(owner.SceneText()==overflowAuthored,"Overflow Play/Stop changed full authored source/history");
    }
    RenderCommand::SetViewport(overflowViewport.x,overflowViewport.y,overflowViewport.z,overflowViewport.w);
    m_OverflowFixture=scene;m_OverflowTarget=overflowIDs.front();m_OverflowFixturePanel.SetContext(scene);m_OverflowFixturePanel.SetSelectedEntity(scene->GetEntityByUUID(m_OverflowTarget));m_OverflowFixturePanel.RestoreSections({{"UI Text Style",true}});
    SceneSerializer(corpusScene).Serialize(assets->Root()/"Scenes/corpus-native.hazel");
    Prefab::Save(assets->Root(),"Prefabs/corpus-native.hprefab",corpusScene,corpusLabel);
    // Opt-in downstream package qualification retains only the useful, native-
    // authored fixture; normal runs still delete their entire temporary project.
    if(const auto* output=std::getenv("HAZEL_EDITOR_FONT_FIXTURE");output && *output) {
        const auto destination=std::filesystem::absolute(std::filesystem::u8path(output));
        for(const auto* file:{"Fonts/Native.hfont","Fonts/Family-Regular.ttf","Fonts/Family-Bold.ttf","Fonts/Family-NOTICE.txt","Scenes/family-native.hazel","Prefabs/family-native.hprefab","Fonts/Corpus.hfont","Fonts/Corpus.ttf","Fonts/Corpus-NOTICE.txt","Scenes/corpus-native.hazel","Prefabs/corpus-native.hprefab","Scenes/overflow-native.hazel","Prefabs/overflow-native.hprefab"}) {
            auto target=destination/file;std::filesystem::create_directories(target.parent_path());
            std::filesystem::copy_file(assets->Root()/file,target,std::filesystem::copy_options::overwrite_existing);
        }
    }
    // Keep the ordinary native asset editor open for real ImGui rendering/preview
    // checks on following frames; use private test resources only.
    auto& family=owner.m_Fonts;family.m_Style=1;family.m_PreviewFont=assets->FamilyFont({"Fonts/Native.hfont",FontStyle::Bold});family.m_AcceptedPreview="Native café preview";
    m_FamilyFixture=CreateScope<FontFamilyPanel>();m_FamilyFixture->Bind(assets);
    Check(m_FamilyFixture->Open(assets->Root()/"Fonts/Corpus.hfont"),"Family ImGui fixture failed to open");
    m_FamilyFixture->m_Style=0;m_FamilyFixture->m_Preview=u8"café مرحبا שלום 123";m_FamilyFixture->UpdatePreview();
    Check(m_FamilyFixture->m_PreviewFont && m_FamilyFixture->m_AtlasCoverage.find("0 .notdef")!=std::string::npos,"Accepted preview atlas coverage disagreed with actual glyphs");
    Check(editor.OpenScene(original),"Family workflow did not restore disposable scene");
    std::cout<<"PASS: native font-family create/import/edit/coverage/save/reopen guards, Inspector history, scene/prefab closure and repeated Play/Stop\n";
}

void DrawFontFamilyFixture() {
    if(m_OverflowFixture) {
        auto authored=SceneSerializer(m_OverflowFixture).CaptureAuthoredState();
        ImGui::SetNextWindowPos({560,80},ImGuiCond_Always);ImGui::SetNextWindowSize({440,520},ImGuiCond_Always);
        ImGui::Begin("Overflow qualification",nullptr,ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoDocking|ImGuiWindowFlags_NoInputs);
        auto& owner=*m_Editor.m_Authoring;auto savedScene=owner.m_PrefabScene;auto savedPreview=owner.m_PrefabPreview;auto camera=owner.m_PrefabCamera;bool fit=owner.m_PrefabFit;
        owner.m_PrefabScene=m_OverflowFixture;owner.m_PrefabPreview=m_OverflowPreview;owner.m_PrefabFit=true;
        ImGui::GetStateStorage()->SetInt(ImGui::GetID("Subtree preview"),1);owner.PrefabPreview();
        m_OverflowPreview=owner.m_PrefabPreview;owner.m_PrefabScene=savedScene;owner.m_PrefabPreview=savedPreview;owner.m_PrefabCamera=camera;owner.m_PrefabFit=fit;
        Check(bool(m_OverflowPreview) && m_OverflowFixture->GetUIError().empty() && bool(m_OverflowFixture->GetUITextLayout(m_OverflowTarget)),"Native overflow scene preview failed");
        m_OverflowFixturePanel.DrawAssetProperties(m_OverflowFixturePanel.GetSelectedEntity());ImGui::End();
        GameUI::Action action;Check(SceneSerializer(m_OverflowFixture).CaptureAuthoredState()==authored && !m_OverflowFixture->ConsumeUIAction(0,action),"Overflow Inspector/preview modified source or dispatched an action");
        std::cout<<"PASS: actual native overflow Inspector and scene preview, authored source unchanged\n";
    }
    if(!m_FamilyFixture)return;
    auto& panel=*m_FamilyFixture;const auto before=panel.CopyDraftText();auto generations=Font::Resources().Generations;
    ImGui::SetNextWindowPos({60,80},ImGuiCond_Always);ImGui::SetNextWindowSize({460,640},ImGuiCond_Always);
    auto* previousFocus=ImGui::GetCurrentContext()->NavWindow;panel.Render(false);ImGui::FocusWindow(previousFocus);
    Check(bool(panel.m_PreviewTarget) && panel.CopyDraftText()==before && !panel.Dirty() && Font::Resources().Generations==generations,
          "Real family ImGui/preview mutated draft or generated fonts while inspecting");
    const auto raster=RenderCommand::GetRasterState();panel.m_PreviewTarget->Bind();
    std::vector<int> ids(640*96);glReadBuffer(GL_COLOR_ATTACHMENT1);glReadPixels(0,0,640,96,GL_RED_INTEGER,GL_INT,ids.data());
    panel.m_PreviewTarget->Unbind();RenderCommand::SetRasterState(raster);
    Check(std::find(ids.begin(),ids.end(),0)!=ids.end(),"Native family preview framebuffer contains no actual glyphs");
    std::cout<<"PASS: actual native font-family asset properties and face framebuffer preview, no authored/atlas-generation side effects\n";
}
