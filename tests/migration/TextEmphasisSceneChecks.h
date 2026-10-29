static void TextEmphasisSceneChecks(const std::filesystem::path& root) {
    auto scene=CreateRef<Scene>();scene->SetAssets(CreateRef<ProjectAssets>(root));scene->OnViewportResize(128,128);
    auto canvas=scene->CreateEntity("Rich canvas");canvas.AddComponent<UICanvasComponent>().ReferenceSize={128,128};
    auto label=scene->CreateEntity("Rich label");scene->Reparent(label,canvas,TransformPolicy::KeepLocal);
    auto& rect=label.AddComponent<UIRectComponent>();rect.WidthMode=rect.HeightMode=SizeMode::Content;
    label.AddComponent<TextComponent>();label.AddComponent<UITextStyleComponent>().Size=16;
    scene->SetDisplayText(label.GetUUID(),"Bold ordinary",{{0,4,FontStyle::Bold,glm::vec4(1,.2f,.3f,.6f)}});
    RuntimeUI ui(*scene);ui.Resolve({128,128});ui.Draw();auto generation=ui.Snapshot().Generation;auto state=Font::Resources();
    auto& text=label.GetComponent<TextComponent>();auto palette=text.ResolvedFaces;
    for(int i=0;i<4;++i){text.Spans[0].Color=glm::vec4(.1f,.2f,float(i)*.2f,1);ui.Resolve({128,128});ui.Draw();}
    Check(ui.Snapshot().Generation==generation&&Font::Resources().LayoutBuilds==state.LayoutBuilds&&Font::Resources().Generations==state.Generations,"Color-native mutation discarded rich shape/page generation reuse");
    const auto accepted=text.TextString;auto spans=text.Spans;
    bool rejected=false;try{scene->SetDisplayText(label.GetUUID(),"e\xcc\x81",{{0,1,FontStyle::Italic,{}}});}catch(const std::exception&){rejected=true;}
    Check(rejected&&text.TextString==accepted&&text.Spans==spans,"Atomic invalid emphasis changed source/span ownership");
    text.Spans[0].Face=FontStyle::Italic;ui.Resolve({128,128});ui.Draw();Check(ui.Snapshot().Generation>generation&&Font::Resources().LayoutBuilds>state.LayoutBuilds,"Face-native write did not invalidate Content/preparation");
    auto serialized=SceneSerializer(scene).SerializeText();auto reopened=CreateRef<Scene>();Check(SceneSerializer(reopened).DeserializeText(serialized)&&reopened->GetEntityByUUID(label.GetUUID()).GetComponent<TextComponent>().Spans==text.Spans,"Typed emphasis scene save/reopen lost ranges");
    auto copy=Scene::Copy(scene);auto& copied=copy->GetEntityByUUID(label.GetUUID()).GetComponent<TextComponent>();Check(copied.Spans==text.Spans&&!copied.ResolvedFaces[0],"Scene copy lost authored spans or retained derived palette");
    auto expectedSpans=text.Spans;auto duplicate=scene->DuplicateEntity(label);Check(duplicate.GetComponent<TextComponent>().Spans==expectedSpans,"Duplicate lost emphasis");
    std::filesystem::create_directory(root/"EmphasisPrefabs");Prefab::Save(root,"EmphasisPrefabs/Text.hprefab",scene,label);
    auto instance=Prefab::Instantiate(root,"EmphasisPrefabs/Text.hprefab",*scene);Check(instance.GetComponent<TextComponent>().Spans==expectedSpans,"Prefab emphasis roundtrip failed");
    scene->SetDisplayText(label.GetUUID(),"Plain replacement");Check(label.GetComponent<TextComponent>().Spans.empty(),"Plain native setter retained stale emphasis");
    AuditTextEmphasis(root,{},"Bold ordinary",spans);
    auto wrong=spans;wrong[0].End=100;rejected=false;try{AuditTextEmphasis(root,{},"short",wrong);}catch(const std::exception&){rejected=true;}Check(rejected,"Export emphasis validation accepted stale source ranges");
    const auto familyRoot=root/"rich-reload";std::filesystem::create_directory(familyRoot);
    std::filesystem::copy_file("assets/fonts/opensans/OpenSans-Regular.ttf",familyRoot/"Regular.ttf");std::filesystem::copy_file("assets/fonts/opensans/OpenSans-Bold.ttf",familyRoot/"Bold.ttf");
    WriteTestFamily(familyRoot,"Rich.hfont","Regular.ttf");auto family=ReadFontFamily(familyRoot/"Rich.hfont");family.Faces[1]={"Bold.ttf",{"font-LICENSE.txt"}};std::ofstream(familyRoot/"Rich.hfont")<<WriteFontFamilyText(family);
    auto assets=CreateRef<ProjectAssets>(familyRoot);scene->SetAssets(assets);label.GetComponent<TextComponent>().Font={"Rich.hfont"};scene->SetDisplayText(label.GetUUID(),"Bold ordinary",spans);
    ui.Resolve({128,128});ui.Draw();auto oldPalette=label.GetComponent<TextComponent>().ResolvedFaces;auto width=ui.Snapshot().Find(label.GetUUID())->Bounds.Size.x;
    auto oldLayout=oldPalette[0]->Layout("Bold ordinary",-1,false,0,0,Text::Overflow::Clip,0,-1,TextFaces(oldPalette),spans);
    {std::ofstream bad(familyRoot/"Bold.ttf",std::ios::binary);bad<<"invalid face replacement";}assets->Reload("Rich.hfont");ui.Resolve({128,128});ui.Draw();
    Check(label.GetComponent<TextComponent>().ResolvedFaces[2]==oldPalette[2]&&ui.Snapshot().Find(label.GetUUID())->Bounds.Size.x==width,"Failed rich-face reload split metrics and atlas versions");
    std::filesystem::copy_file("assets/fonts/opensans/OpenSans-Regular.ttf",familyRoot/"Bold.ttf",std::filesystem::copy_options::overwrite_existing);assets->Reload("Rich.hfont");ui.Resolve({128,128});ui.Draw();
    Check(label.GetComponent<TextComponent>().ResolvedFaces[2]!=oldPalette[2]&&std::abs(ui.Snapshot().Find(label.GetUUID())->Bounds.Size.x-width)>.01f,"Accepted rich face did not invalidate Content and prepared pages together");
    Check(!oldPalette[2]->PreparePages(*oldLayout,2).empty(),"Old mixed layout lost its immutable accepted bytes after replacement");
    std::cout<<"PASS: scene rich Content/native mutation reuse, atomic rejection/plain replacement, copy/prefab/source roundtrip and CPU export validation, rich-face failed reload/recovery and old-version pins\n";
}
