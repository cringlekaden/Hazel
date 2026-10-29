// Real scene/renderer publication, native mutations and persistence.
static void TextOverflowSceneChecks(const std::filesystem::path& root) {
    auto scene=CreateRef<Scene>();scene->SetAssets(CreateRef<ProjectAssets>(root));scene->OnViewportResize(128,128);
    auto canvas=scene->CreateEntity("Overflow canvas");auto& c=canvas.AddComponent<UICanvasComponent>();c.ReferenceSize={128,128};c.SafeInsets={};
    auto label=scene->CreateEntity("Overflow label");scene->Reparent(label,canvas,TransformPolicy::KeepLocal);
    auto& rect=label.AddComponent<UIRectComponent>();rect.Offset={8,8};rect.FixedSize={90,52};rect.Padding=glm::vec4(4);
    auto& text=label.AddComponent<TextComponent>();text.TextString="A long label with more than one useful line of source";
    auto& style=label.AddComponent<UITextStyleComponent>();style.Size=16;style.Wrap=false;style.Overflow=Text::Overflow::Ellipsis;style.MaxLines=1;
    FramebufferSpecification spec;spec.Width=spec.Height=128;spec.Attachments={FramebufferTextureFormat::RGBA8,FramebufferTextureFormat::RED_INTEGER,FramebufferTextureFormat::Depth};
    auto target=Framebuffer::Create(spec);RuntimeUI ui(*scene);
    auto paint=[&] {target->Bind();auto raster=RenderCommand::GetRasterState();raster.ClipEnabled=false;RenderCommand::SetRasterState(raster);RenderCommand::Clear();target->ClearAttachment(1,-1);ui.Resolve({128,128});ui.Draw();};
    const auto beforePrewarm=Font::Resources();scene->PrepareSprites(true);
    Check(Font::Resources().LayoutBuilds==beforePrewarm.LayoutBuilds && Font::Resources().Generations==beforePrewarm.Generations,"UI startup prewarmed hidden unwrapped source instead of constrained layout");
    paint();auto first=ui.TextLayout(label.GetUUID());Check(first && first->Omitted && first->Lines[0].Ellipsized,"UI did not consume final shaped overflow result");
    Check(first==text.ResolvedFont->Layout(text.TextString,82.f/16,false,0,0,Text::Overflow::Ellipsis,1,44.f/16),"Measurement/preparation did not share accepted constraints/result");
    std::vector<int> ids(128*128);glReadBuffer(GL_COLOR_ATTACHMENT1);glReadPixels(0,0,128,128,GL_RED_INTEGER,GL_INT,ids.data());size_t pixels=0;
    for(int y=0;y<128;++y)for(int x=0;x<128;++x)if(ids[y*128+x]==int(uint32_t(label))){++pixels;Check(x>=12 && x<94 && y>=72 && y<116,"Overflow ink escaped padded content clip");}
    Check(pixels>15,"Ellipsis scene has no pickable text");
    std::vector<Renderer2D::TextLine> view;auto font=text.ResolvedFont;
    Renderer2D::LayoutText("keep",font,-1,false,0,view);auto acceptedView=view.front().Result;
    auto cached=font->Layout("a\nb",-1,false);
    Check(cached->Lines.size()==2,"Line-view failure fixture lost its two-line payload");
    const auto payload=cached->Lines.size()*sizeof(Renderer2D::TextLine);
    bool failed=false;s_FailAllocationBytes=payload;
    // Incidental small/proxy allocations must succeed while the payload fault
    // is armed, including MSVC Debug's noexcept empty-vector construction.
    std::vector<char> incidental;incidental.reserve(7);
    Check(s_FailAllocationBytes==payload,"Line-view fault was consumed by incidental allocation");
    try{Renderer2D::LayoutText("a\nb",font,-1,false,0,view);}catch(const std::bad_alloc&){failed=true;}
    const bool consumed=s_FailAllocationBytes==0;s_FailAllocationBytes=0;
    Check(failed && consumed && view.size()==1 && view.front().Result==acceptedView && view.front().Text=="keep","Failed line-view publication replaced valid cache data");
    auto published=Renderer2D::LayoutText("a\nb",font,-1,false,0,view);
    Check(published==cached && view.size()==2 && view[0].Text=="a" && view[1].Text=="b" && view[0].Result==cached && view[1].Result==cached,"Line-view publication did not recover after staged allocation failure");
    auto generations=Font::Resources().Generations,builds=Font::Resources().LayoutBuilds;paint();
    Check(ui.TextLayout(label.GetUUID())==first && ui.Diagnostics().PreparedReuses==1 && Font::Resources().LayoutBuilds==builds && Font::Resources().Generations==generations,"Warm overflow reshaped or regenerated pages");
    auto generation=ui.Snapshot().Generation;style.MaxLines=2;style.Wrap=true;text.TextString="one two three four five six seven eight";paint();
    Check(ui.Snapshot().Generation==generation && ui.TextLayout(label.GetUUID())!=first && ui.Diagnostics().PreparedRebuilds==1,"Fixed-box native overflow mutation hid behind generation reuse");
    style.Overflow=Text::Overflow::Clip;paint();auto clip=ui.TextLayout(label.GetUUID());Check(clip->Lines.size()==2 && clip->Omitted && !clip->Lines[0].Ellipsized,"Clip MaxLines ignored by real UI");
    rect.HeightMode=SizeMode::Content;paint();auto two=ui.Snapshot().Find(label.GetUUID())->Bounds.Size.y;generation=ui.Snapshot().Generation;
    style.MaxLines=1;paint();Check(ui.Snapshot().Generation>generation && ui.Snapshot().Find(label.GetUUID())->Bounds.Size.y<two,"Content MaxLines missed native dependency invalidation");
    style.Overflow=Text::Overflow::Ellipsis;rect.FixedSize.x=45;paint();Check(ui.Snapshot().Find(label.GetUUID())->Bounds.Size.y>=24,"Content ellipsis height became circular or lost padding");
    const auto authored=SceneSerializer(scene,root).CaptureAuthoredState();const auto bytes=SceneSerializer(scene,root).SerializeText();
    auto reopened=CreateRef<Scene>();Check(SceneSerializer(reopened,root,false,scene->GetAssets()).DeserializeText(bytes),"Overflow scene deserialize failed");
    auto restored=reopened->GetEntityByUUID(label.GetUUID());Check(restored.GetComponent<TextComponent>().TextString==text.TextString && restored.GetComponent<UITextStyleComponent>().Overflow==style.Overflow && restored.GetComponent<UITextStyleComponent>().MaxLines==1 && SceneSerializer(reopened,root).CaptureAuthoredState()==authored,"Overflow/source scene roundtrip lost authored values");
    auto copy=Scene::Copy(scene);Check(copy->GetEntityByUUID(label.GetUUID()).GetComponent<UITextStyleComponent>().MaxLines==1,"Copy lost MaxLines");
    auto duplicate=scene->DuplicateEntity(label);Check(duplicate.GetComponent<UITextStyleComponent>().Overflow==Text::Overflow::Ellipsis,"Duplicate lost overflow");
    std::filesystem::create_directories(root/"Prefabs");Prefab::Save(root,"Prefabs/overflow.hprefab",scene,label);
    auto instance=Prefab::Instantiate(root,"Prefabs/overflow.hprefab",*scene);Check(instance.GetComponent<UITextStyleComponent>().MaxLines==1,"Prefab lost MaxLines");
    // Reacquire after registry insertion, never hold moved component references.
    auto& liveRect=label.GetComponent<UIRectComponent>();liveRect.HeightMode=SizeMode::Fixed;liveRect.FixedSize.y=8;paint();
    auto noLine=ui.TextLayout(label.GetUUID());Check(noLine && noLine->Lines.empty() && noLine->MarkerDoesNotFit,"Zero-height final result not published");
    builds=Font::Resources().LayoutBuilds;paint();Check(ui.TextLayout(label.GetUUID())==noLine && ui.Diagnostics().PreparedReuses>=1 && Font::Resources().LayoutBuilds==builds,"Empty final layout poisoned warm keys");
    target->Unbind();std::cout<<"PASS: real UI Clip/Ellipsis/MaxLines, padded ink/picking, native changes, Content reflow, scene/copy/prefab, zero height and warm reuse\n";
}
