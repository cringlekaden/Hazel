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
    next.Text->Font={"Fonts/Native.hfont",FontStyle::Bold};UIInspectorEditing editing;PropertyUI::EditActivity activity;activity.Changed=activity.Committed=true;
    Check(editing.Edit(*scene,&history,text.GetUUID(),"Text family",before,next,activity,0) && history.Operations()==1,"Family assignment did not use one shared Inspector command");
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
    SceneSerializer(corpusScene).Serialize(assets->Root()/"Scenes/corpus-native.hazel");
    Prefab::Save(assets->Root(),"Prefabs/corpus-native.hprefab",corpusScene,corpusLabel);
    // Opt-in downstream package qualification retains only the useful, native-
    // authored fixture; normal runs still delete their entire temporary project.
    if(const auto* output=std::getenv("HAZEL_EDITOR_FONT_FIXTURE");output && *output) {
        const auto destination=std::filesystem::absolute(std::filesystem::u8path(output));
        for(const auto* file:{"Fonts/Native.hfont","Fonts/Family-Regular.ttf","Fonts/Family-Bold.ttf","Fonts/Family-NOTICE.txt","Scenes/family-native.hazel","Prefabs/family-native.hprefab","Fonts/Corpus.hfont","Fonts/Corpus.ttf","Fonts/Corpus-NOTICE.txt","Scenes/corpus-native.hazel","Prefabs/corpus-native.hprefab"}) {
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
