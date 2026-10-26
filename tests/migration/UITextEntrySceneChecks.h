#include "UITextEntryFixture.h"
static void UITextEntrySceneChecks(const std::filesystem::path &directory)
{
    using C = Text::EditCommand;
    auto s = TextEntryFixture();
    s->ValidateUI(false);
    auto authored = SceneSerializer(s).CaptureAuthoredState();
    s->OnRuntimeStart();
    auto clip = CreateRef<SceneTextClipboard>();
    s->SetUIClipboard(clip);
    InputFrame f;
    f.Enabled = true;
    f.Mouse = {399, 299};
    auto neutral = [&] {
        f = {};
        f.Enabled = true;
        f.Mouse = {399, 299};
        s->UpdateUIInput(f);
        s->UpdateUIInput(f);
    };
    neutral();
    auto command = [&](C c, const std::string &text = {}, bool shift = false, bool word = false) {
        ActionEdge e{UIAction::Text};
        e.Text = {c, text, shift, word};
        f.Events = {e};
        s->UpdateUIInput(f);
        f.Events.clear();
    };
    auto enter = [&](uint64_t id) {
        Check(s->FocusUI(id), "Text field focus failed");
        f.Events = {{UIAction::Accept, true}, {UIAction::Accept, false}};
        s->UpdateUIInput(f);
        f.Events.clear();
        neutral();
        Check(s->IsUITextEditing(), "Accept did not enter editing");
    };
    enter(500);
    command(C::End);
    command(C::Backspace);
    Check(s->GetUITextEntry(500).empty(), "Runtime deletion split a combining cluster");
    command(C::Insert, "Name");
    Check(s->GetUITextEntry(500) == "Name", "Committed text event did not insert");
    Action event;
    Check(s->ConsumeUIEvent(900, event) && event.Kind == UIEventKind::TextChanged &&
              event.PreviousText == u8"a\u0301" && event.TextValue == "Name" &&
              !s->ConsumeUIEvent(900, event),
          "Owner text previews did not coalesce with historical payload");
    command(C::All);
    command(C::Copy);
    Check(clip->Value == "Name", "Injected clipboard copy failed");
    command(C::Cut);
    Check(s->GetUITextEntry(500).empty(), "Cut failed");
    command(C::Undo);
    Check(s->GetUITextEntry(500) == "Name", "Local text undo failed");
    command(C::Cancel);
    Check(s->GetUITextEntry(500) == u8"a\u0301" && !s->IsUITextEditing(),
          "Explicit Cancel did not restore edit start");
    while (s->ConsumeUIEvent(900, event))
    {
    }
    neutral();
    enter(500);
    command(C::All);
    command(C::Insert, "temporary");
    s->SetUITextEntry(500, "Programmatic");
    command(C::Cancel);
    Check(s->GetUITextEntry(500) == "Programmatic" && !s->IsUITextEditing(),
          "Stale Cancel overwrote a silent programmatic value");
    while (s->ConsumeUIEvent(900, event))
    {
    }
    neutral();
    f.Mouse={36,148};f.LeftDown=true;f.Events={{UIAction::Press,true,f.Mouse}};s->UpdateUIInput(f);f.Events.clear();
    Check(s->IsUITextEditing()&&s->GetUIDiagnostics().Capture==500&&s->GetUIDiagnostics().TextCaret==0,"Pointer press did not hit the shaped start caret");
    f.Mouse={390,148};s->UpdateUIInput(f);f.LeftDown=false;f.Events={{UIAction::Press,false,f.Mouse}};s->UpdateUIInput(f);f.Events.clear();
    Check(s->IsUITextEditing()&&!s->IsUICapturingPointer()&&s->GetUIDiagnostics().TextAnchor==0&&s->GetUIDiagnostics().TextCaret>0,"Outside release lost drag selection or stuck capture");
    command(C::Copy);Check(clip->Value=="Programmatic","Pointer drag selection did not follow source positions");command(C::Submit);while(s->ConsumeUIEvent(900,event)){}neutral();
    enter(501);
    command(C::End, {}, false, true);
    command(C::Enter);
    command(C::Insert, "more");
    auto target =
        Framebuffer::Create({400,
                             300,
                             {{FramebufferTextureFormat::RGBA8, FramebufferTextureFormat::RED_INTEGER,
                               FramebufferTextureFormat::Depth}}});
    target->Bind();
    RenderCommand::SetClearColor({0, 0, 0, 1});
    RenderCommand::Clear();
    target->ClearAttachment(1, -1);
    s->OnUpdateRuntime(0);
    auto layout = s->GetUITextLayout(501);
    Check(layout && layout->Lines.size() >= 5, "Notes did not consume shared wrapped layout");
    Check(target->ReadPixel(1, 40, 300 - 190) == int(uint32_t(s->GetEntityByUUID(501))),
          "Text-entry painting/picking disagreed with field clip");
    auto builds = Font::Resources().LayoutBuilds;
    auto generation = s->GetUISnapshot().Generation;
    command(C::Left);
    s->OnUpdateRuntime(.25f);
    Check(Font::Resources().LayoutBuilds == builds && s->GetUISnapshot().Generation == generation,
          "Caret movement/blink reshaped or republished text");
    target->Unbind();
    command(C::Submit);
    Check(!s->IsUITextEditing(), "Multiline Submit did not finish");
    bool terminal = false;
    while (s->ConsumeUIEvent(900, event))
        terminal |= event.Kind == UIEventKind::TextCommitted && event.Entity == 501;
    Check(terminal, "Text completion event disappeared");
    neutral();
    s->SetUITextEntry(500,"          ");enter(500);
    auto pixels=[&](int x,int y){std::array<uint8_t,4> result{};GLint saved=0;glGetIntegerv(GL_READ_BUFFER,&saved);glReadBuffer(GL_COLOR_ATTACHMENT0);glReadPixels(x,300-y,1,1,GL_RGBA,GL_UNSIGNED_BYTE,result.data());glReadBuffer(saved);return result;};
    auto paint=[&]{target->Bind();RenderCommand::SetClearColor({0,0,0,1});RenderCommand::Clear();target->ClearAttachment(1,-1);s->OnUpdateRuntime(0);};
    paint();auto base=pixels(46,148),paddingBefore=pixels(30,148),caret=pixels(36,148);Check(caret[0]>250&&caret[1]>250&&caret[2]>250,"Caret did not paint at the shaped start position");
    command(C::All);paint();auto selected=pixels(46,148),paddingAfter=pixels(30,148);auto selectionColor=s->GetEntityByUUID(500).GetComponent<UITextEntryComponent>().SelectionColor;
    for(int i=0;i<3;++i)Check(std::abs(float(selected[i])-(selectionColor[i]*selectionColor.a*255+float(base[i])*(1-selectionColor.a)))<=2,"Selection compositing disagrees with independent straight-alpha reference");Check(paddingBefore==paddingAfter,"Selection escaped padded clipping");Check(target->ReadPixel(1,46,300-148)==int(uint32_t(s->GetEntityByUUID(500))),"Selection/caret picking targeted a decorative entity");target->Unbind();command(C::Cancel);while(s->ConsumeUIEvent(900,event)){}neutral();
    s->SetUITextEntry(500,u8"漢");Check(!s->GetUITextLayout(500),"Silent text replacement exposed a stale painted layout");paint();Check(s->GetUITextEntry(500)==u8"漢"&&!s->GetUITextLayout(500),"Unsupported layout corrupted editable content or published a stale layout");auto failedBuilds=Font::Resources().LayoutBuilds;s->OnUpdateRuntime(0);Check(Font::Resources().LayoutBuilds==failedBuilds,"Failed text layout retried every warm draw");target->Unbind();enter(500);command(C::All);command(C::Copy);Check(clip->Value==u8"漢","Unavailable glyphs prevented copying accepted text");command(C::Backspace);command(C::Submit);while(s->ConsumeUIEvent(900,event)){}neutral();
    enter(500);
    command(C::Insert, "x");
    f.Enabled = false;
    ActionEdge lost{UIAction::Text};
    lost.Text = {C::Insert, "retired"};
    f.Events = {lost};
    s->UpdateUIInput(f);
    Check(!s->IsUITextEditing() && s->GetUITextEntry(500).find("retired") == std::string::npos &&
              !s->ConsumeUIEvent(900, event),
          "Ownership loss accepted/replayed stale text");
    s->OnRuntimeStop();
    auto pressure=TextEntryFixture();auto &config=pressure->GetEntityByUUID(500).GetComponent<UITextEntryComponent>();config.MaxBytes=16384;config.MaxGraphemes=4096;
    pressure->OnRuntimeStart();InputFrame q;q.Enabled=true;q.Mouse={399,299};auto qneutral=[&]{q.Events.clear();pressure->UpdateUIInput(q);pressure->UpdateUIInput(q);};qneutral();
    auto qaccept=[&](uint64_t id){Check(pressure->FocusUI(id),"Pressure focus unavailable");q.Events={{UIAction::Accept,true},{UIAction::Accept,false}};pressure->UpdateUIInput(q);q.Events.clear();};
    for(int i=0;i<64;++i)qaccept(400);qaccept(500);Check(!pressure->IsUITextEditing()&&pressure->GetUIDiagnostics().PendingEvents==64,"Full queue admitted an edit without completion reserve");while(pressure->ConsumeUIEvent(900,event)){}qneutral();
    std::string largeA,largeB;for(int i=0;i<4096;++i){largeA+=u8"ḋ";largeB+=u8"ḍ";}pressure->SetUITextEntry(500,largeA);
    auto qcommand=[&](C command,const std::string& value={}){ActionEdge edge{UIAction::Text};edge.Text={command,value};q.Events={edge};pressure->UpdateUIInput(q);q.Events.clear();};
    for(int i=0;i<2;++i){qaccept(500);qneutral();Check(pressure->IsUITextEditing(),"Payload fixture edit rejected too soon");qcommand(C::All);qcommand(C::Insert,i?largeA:largeB);qcommand(C::Submit);qneutral();}
    auto bytes=pressure->GetUIDiagnostics().TextEventBytes;Check(bytes>65536&&bytes<=128*1024,"Historical text payload was not accounted/bounded");qaccept(500);Check(!pressure->IsUITextEditing(),"Payload pressure admitted an unreportable edit");while(pressure->ConsumeUIEvent(900,event)){}qneutral();qaccept(500);qneutral();Check(pressure->IsUITextEditing(),"Text admission did not recover after draining");
    qcommand(C::Insert,"x");pressure->DestroyEntity(pressure->GetEntityByUUID(500));pressure->RefreshUI();Check(!pressure->IsUITextEditing()&&!pressure->ConsumeUIEvent(900,event),"Retired field retained editing or queued text");pressure->OnRuntimeStop();
    auto edit = TextEntryFixture();
    Check(SceneSerializer(edit).CaptureAuthoredState() == authored,
          "Runtime edit changed authored initial content");
    SceneSerializer(edit).Serialize((directory / "entry.hazel").generic_u8string());
    auto loaded = CreateRef<Scene>();
    Check(SceneSerializer(loaded).Deserialize((directory / "entry.hazel").generic_u8string()) &&
              SceneSerializer(loaded).CaptureAuthoredState() == authored,
          "Text Entry scene roundtrip failed");
    auto copy = Scene::Copy(edit);
    Check(SceneSerializer(copy).CaptureAuthoredState() == authored, "Text Entry copy failed");
    Prefab::Save(directory, "entry.hprefab", edit, edit->GetEntityByUUID(200));
    auto prefab = Prefab::Load(directory, "entry.hprefab");
    auto instance = loaded->InstantiateEntity(Prefab::GetEntity(prefab), {});
    Check(bool(instance), "Text Entry prefab failed");
    std::cout << "PASS: runtime text input/selection, typed owner coalescing, commit/cancel, clipboard/local "
                 "undo, silent replacement, warm GPU/picking, loss and scene/prefab isolation\n";
}
