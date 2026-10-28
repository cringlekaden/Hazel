// Maintained game-facing data checked through real scene/session services.
// Synthetic devices establish routing, not physical controller/reader acceptance.
static void ValidateExampleDocuments(const Ref<Project>& project) {
    const auto root=project->GetAssetRoot();
    for(const auto& item:std::filesystem::recursive_directory_iterator(root)) {
        const auto ext=item.path().extension();
        if(ext!=".hazel"&&ext!=".hprefab")continue;
        auto relative=std::filesystem::relative(item.path(),root);
        auto scene=ext==".hazel"?project->LoadScene(relative):Prefab::Load(root,relative);
        scene->ValidateHierarchy();scene->ValidateSprites();scene->ValidateUI();
        for(auto handle:scene->GetAllEntitiesWith<ScriptComponent>()) {
            const auto& name=Entity(handle,scene.get()).GetComponent<ScriptComponent>().ClassName;
            Check(name.empty()||ScriptEngine::EntityClassExists(name),"An example document retains an unavailable script class");
        }
    }
}

static void TutorialIntegrationChecks(const std::filesystem::path& source,
                                      const std::filesystem::path& captures,
                                      const Ref<Framebuffer>& framebuffer) {
    auto project=Project::Load(source);Check(bool(project),"Cannot load Sandbox tutorial project");
    ScriptEngine::Init(Project::GetAssetFileSystemPath(project->GetConfig().ScriptModulePath));
    ValidateExampleDocuments(project);
    auto authored=project->LoadScene("Scenes/UI Tutorial.hazel");
    const auto baseline=SceneSerializer(authored).CaptureAuthoredState();
    RuntimeSession session;session.Resize(1024,600);session.ResizeFramebuffer(1024,600);
    framebuffer->Unbind();framebuffer->Resize(1024,600);framebuffer->Bind();
    session.Start(project,authored);
    GameUI::DeviceSample sample;sample.Point={1000,590};session.SetInput(sample.Point,true);
    auto update=[&]{RenderCommand::SetClearColor({.025f,.04f,.055f,1});RenderCommand::Clear();session.GetScene()->OnUpdateRuntime(.016f);Check(session.GetScene()->GetUIError().empty(),"Tutorial published a layout/resource error");};
    auto neutral=[&]{sample.Keys.fill(false);sample.Mouse.fill(false);session.RouteUIInput(sample,.016f,true);update();};
    neutral();neutral();
    auto scene=session.GetScene();
    auto named=[&](const char* name){auto e=scene->FindEntityByName(name);Check(bool(e),name);return uint64_t(e.GetUUID());};
    auto accept=[&](uint64_t id){
        neutral();Check(scene->FocusUI(id),"Tutorial control was not focus eligible");
        sample.Keys[Key::Enter]=true;session.RouteUIInput(sample,.016f,true);update();neutral();
    };
    const auto owner=named("UI Tutorial owner"),main=named("UI Tutorial"),pages=named("Tutorial pages");
    Check(scene->GetUIDiagnostics().EventOwner==owner&&scene->GetUIDiagnostics().Scope==main,"Tutorial event recipient does not match its authored owner");
    accept(named("Advance preview"));Check(std::abs(scene->GetUIProgress(named("Preview progress"))-.35f)<.001f,"Tutorial button event did not reach its owner exactly once");
    auto invoker=named("Open tutorial dialog"),modal=named("Tutorial confirmation");
    accept(invoker);Check(scene->IsUIScreenOpen(modal)&&scene->IsGameplayInputBlocked(),"Tutorial modal did not open/block gameplay");
    sample.Keys[Key::Escape]=true;session.RouteUIInput(sample,.016f,true);update();neutral();
    Check(!scene->IsUIScreenOpen(modal)&&scene->GetUIDiagnostics().Focus==invoker,"Tutorial Cancel failed to restore the invoking focus");
    for(auto tab:{"Layout tab","Text tab","Controls tab","Compose tab","Welcome tab"}) {
        accept(named(tab));
        Check(scene->GetUISelection(pages)==named(tab),"Tutorial tab did not commit through shared selection");
        for(auto size:{glm::uvec2(1024,600),glm::uvec2(1280,720)}) {
            framebuffer->Unbind();framebuffer->Resize(size.x,size.y);framebuffer->Bind();
            session.Resize(size.x,size.y);session.ResizeFramebuffer(size.x,size.y);update();
            const auto path=captures/(std::string("UI-Tutorial-")+tab+"-"+std::to_string(size.x)+".ppm");
            Capture(path,size.x,size.y);
            const auto* bounds=scene->GetUISnapshot().Find(pages);
            Check(bounds&&bounds->Bounds.Size.x>800&&bounds->Bounds.Size.y>350,"Tutorial page lost its usable arranged area");
        }
    }
    accept(named("Controls tab"));
    auto view=named("Controls page scroll"),slider=named("Preview amount");
    Check(scene->EnsureUIVisible(view,slider,true),"Tutorial control could not be revealed");neutral();
    scene->SetUISliderValue(slider,50);update();
    Check(scene->GetUISliderValue(slider)==50,"Tutorial silent slider setter failed");
    const auto before=scene->FindEntityByName("Event feedback").GetComponent<TextComponent>().TextString;
    update();Check(scene->FindEntityByName("Event feedback").GetComponent<TextComponent>().TextString==before,"Silent setter generated a tutorial user event");
    accept(slider);Check(scene->IsUISliderEditing(),"Tutorial slider did not enter adjustment");
    scene->SetUISliderValue(slider,75);update();neutral();
    Check(!scene->IsUISliderEditing()&&scene->GetUISliderValue(slider)==75&&std::abs(scene->GetUIProgress(named("Preview progress"))-.75f)<.001f,"Interrupted slider terminal overwrote the newer silent value/preview");
    const auto entry=named("Your display name");scene->SetUITextEntry(entry,u8"Renée");update();
    Check(scene->GetUITextEntry(entry)==u8"Renée","Tutorial entry lost its UTF-8 runtime value");
    Check(scene->EnsureUIVisible(view,entry,true),"Tutorial text field could not be revealed");
    accept(entry);Check(scene->IsUITextEditing(),"Tutorial Accept did not acquire text editing");
    KeyTypedEvent typed('!');session.SubmitUIEvent(typed,sample.Point,true);
    session.RouteUIInput(sample,.016f,true);update();
    auto edited=scene->GetUITextEntry(entry);
    Check(std::count(edited.begin(),edited.end(),'!')==1,"Committed tutorial text was lost or delivered twice");
    // Editing commands follow key events, not physical key-to-character polling.
    KeyPressedEvent cancelText(Key::Escape,false);session.SubmitUIEvent(cancelText,sample.Point,true);
    sample.Keys[Key::Escape]=true;session.RouteUIInput(sample,.016f,true);update();
    KeyReleasedEvent releaseText(Key::Escape);session.SubmitUIEvent(releaseText,sample.Point,true);neutral();
    Check(!scene->IsUITextEditing()&&scene->GetUITextEntry(entry)==u8"Renée"&&scene->IsUIScreenOpen(main)&&scene->FindEntityByName("Event feedback").GetComponent<TextComponent>().TextString.find("TextCancelled")==0,"Text Cancel leaked to the tutorial screen or restored stale source");
    const auto choice=named("Choose preview"),trigger=scene->GetEntityByUUID(choice).GetComponent<UIDropdownComponent>().Trigger;
    const auto selection=scene->GetUISelection(choice);
    Check(scene->EnsureUIVisible(view,trigger,true),"Scrolled tutorial dropdown could not be revealed");accept(trigger);
    Check(scene->IsUIDropdownOpen(choice),"Tutorial scrolled dropdown did not open");
    sample.Keys[Key::Escape]=true;session.RouteUIInput(sample,.016f,true);update();neutral();
    Check(!scene->IsUIDropdownOpen(choice)&&scene->GetUISelection(choice)==selection&&scene->GetUIDiagnostics().Focus==trigger,"Popup Cancel changed selection or lost return focus");
    for(int i=0;i<3;++i)update();
    const auto generation=scene->GetUISnapshot().Generation;
    const auto resources=Font::Resources();const auto prepared=scene->GetUIDiagnostics().PreparedRebuilds;
    for(int i=0;i<8;++i)update();
    Check(scene->GetUISnapshot().Generation==generation&&Font::Resources().LayoutBuilds==resources.LayoutBuilds&&Font::Resources().Generations==resources.Generations&&scene->GetUIDiagnostics().PreparedRebuilds==prepared,"Warm tutorial rebuilt unchanged layout, shaping, pages or prepared data");
    session.Stop();Check(SceneSerializer(authored).CaptureAuthoredState()==baseline,"Tutorial runtime state changed authored content");
    session.Start(project,authored);neutral();Check(scene!=session.GetScene(),"Tutorial Play did not replace runtime state");
    scene=session.GetScene();Check(scene->GetUITextEntry(named("Your display name"))=="Keeper","Tutorial Stop retained runtime text");session.Stop();
    std::cout<<"PASS: tutorial authored documents/prefabs, typed owner delivery, modal Cancel/return, tabs, 1024x600/1280x720, focus reveal, silent values/text and Play isolation\n";
}

static void LightkeeperMenuIntegrationChecks(RuntimeSession& session,
        const Ref<Framebuffer>& framebuffer,const std::filesystem::path& captures) {
    auto scene=session.GetScene();
    auto named=[&](const char* name){auto e=scene->FindEntityByName(name);Check(bool(e),name);return uint64_t(e.GetUUID());};
    GameUI::DeviceSample sample;sample.Point={10,10};session.SetInput(sample.Point,true);
    auto update=[&]{RenderCommand::SetClearColor({.025f,.04f,.055f,1});RenderCommand::Clear();scene->OnUpdateRuntime(.016f);Check(scene->GetUIError().empty(),"Lightkeeper menu/settings published a UI error");};
    auto neutral=[&]{sample.Keys.fill(false);session.RouteUIInput(sample,.016f,true);update();};
    auto press=[&](int key){sample.Keys[key]=true;session.RouteUIInput(sample,.016f,true);update();neutral();};
    auto accept=[&](uint64_t id){neutral();Check(scene->FocusUI(id),"Lightkeeper configured control is ineligible");press(Key::Enter);};
    neutral();neutral();
    for(auto size:{glm::uvec2(1024,600),glm::uvec2(1280,720)}) {
        framebuffer->Unbind();framebuffer->Resize(size.x,size.y);framebuffer->Bind();
        session.Resize(size.x,size.y);session.ResizeFramebuffer(size.x,size.y);update();
        Capture(captures/("Lightkeeper-title-"+std::to_string(size.x)+".ppm"),size.x,size.y);
    }
    auto settings=named("Settings screen"),invoke=named("Open Settings");accept(invoke);
    Check(scene->IsUIScreenOpen(settings)&&scene->GetUIDiagnostics().EventOwner==settings,"Lightkeeper settings did not acquire its declared owner");
    accept(named("Mute sound"));Check(session.GetAudioVolume()==0&&session.Storage().Read("presentation")=="UI1|1|1","Mute event did not apply/persist actual session audio");
    accept(named("Mute sound"));Check(session.GetAudioVolume()==1,"Unmute did not restore accepted master volume");
    accept(named("Master volume"));Check(scene->IsUISliderEditing(),"Real volume did not enter adjustment");
    press(Key::Left);Check(std::abs(session.GetAudioVolume()-.95f)<.001f,"Live volume feedback did not reach audio");
    press(Key::Escape);Check(session.GetAudioVolume()==1&&!scene->IsUISliderEditing(),"Volume Cancel failed to restore coherent audio/value");
    accept(named("Navigation tab"));
    auto group=named("Navigation pace");uint64_t slow=0;
    for(auto child:scene->GetChildren(group)){auto e=scene->GetEntityByUUID(child);if(e.GetComponent<TextComponent>().TextString=="Repeat: deliberate")slow=child;}
    accept(slow);Check(session.GetUIBindings().RepeatInterval==.25f&&!session.Storage().Read("engine-ui-bindings").empty(),"Navigation setting bypassed player override service");
    // Drop-down selection is delivered after its intended popup closure.
    auto choice=named("Navigation stick"),trigger=scene->GetEntityByUUID(choice).GetComponent<UIDropdownComponent>().Trigger;
    accept(trigger);Check(scene->IsUIDropdownOpen(choice),"Settings stick popup did not open");
    auto popup=scene->GetEntityByUUID(choice).GetComponent<UIDropdownComponent>().Popup;
    const auto content=scene->GetEntityByUUID(popup).GetComponent<UIScrollViewComponent>().Content;uint64_t right=0;
    for(auto child:scene->GetChildren(content)){auto e=scene->GetEntityByUUID(child);if(e.GetComponent<TextComponent>().TextString=="Right stick + D-pad")right=child;}
    Check(scene->EnsureUIVisible(popup,right,true),"Settings popup option reveal failed");accept(right);
    Check(!scene->IsUIDropdownOpen(choice)&&session.GetUIBindings().Stick==1,"Settings popup lost its selection event at close");
    for(auto size:{glm::uvec2(1024,600),glm::uvec2(1280,720)}) {
        framebuffer->Unbind();framebuffer->Resize(size.x,size.y);framebuffer->Bind();session.Resize(size.x,size.y);session.ResizeFramebuffer(size.x,size.y);update();
        Capture(captures/("Lightkeeper-settings-"+std::to_string(size.x)+".ppm"),size.x,size.y);
    }
    accept(named("Settings Back"));Check(!scene->IsUIScreenOpen(settings)&&scene->GetUIDiagnostics().Focus==invoke,"Settings Back did not restore title focus");
    Check(session.Storage().Read("journey").empty(),"Presentation settings changed journey data");
    // Quit is a host request, never Application::Close from a managed callback.
    accept(named("Quit game"));Check(session.ConsumeExitRequest()&&!session.ConsumeExitRequest(),"Game Exit did not reach the deferred host contract");
    session.ResetUIBindings();session.SetInput({},false);
    framebuffer->Unbind();framebuffer->Resize(960,720);framebuffer->Bind();session.Resize(960,720);session.ResizeFramebuffer(960,720);
    std::cout<<"PASS: Lightkeeper real mute/volume feedback and cancellation, persisted navigation/stick selection, settings return focus, title sizes, isolated journey and deferred Quit\n";
}

static void LightkeeperSettingsPersistenceChecks(const Ref<Project>& project,const Ref<Scene>& authored,
                                                 const std::filesystem::path& privateRoot) {
    struct SaveRoot {
        std::filesystem::path Previous=FileSystem::GetEnvironmentPath("HAZEL_SAVE_ROOT");
        static void Set(const std::filesystem::path& value) {
#ifdef HZ_PLATFORM_WINDOWS
            Check(SetEnvironmentVariableW(L"HAZEL_SAVE_ROOT",value.empty()?nullptr:value.c_str()),"Cannot isolate player save root");
#else
            Check((value.empty()?unsetenv("HAZEL_SAVE_ROOT"):setenv("HAZEL_SAVE_ROOT",value.c_str(),1))==0,"Cannot isolate player save root");
#endif
        }
        explicit SaveRoot(const std::filesystem::path& value){Set(value);}
        ~SaveRoot(){
#ifdef HZ_PLATFORM_WINDOWS
            SetEnvironmentVariableW(L"HAZEL_SAVE_ROOT",Previous.empty()?nullptr:Previous.c_str());
#else
            if(Previous.empty())unsetenv("HAZEL_SAVE_ROOT");else setenv("HAZEL_SAVE_ROOT",Previous.c_str(),1);
#endif
        }
    } isolate(privateRoot);
    RuntimeSession session;session.SetPersistentStorage(true);session.Resize(960,720);session.ResizeFramebuffer(960,720);
    GameUI::DeviceSample sample;sample.Point={10,10};Ref<Scene> scene;
    auto update=[&]{RenderCommand::Clear();scene->OnUpdateRuntime(.016f);};
    auto neutral=[&]{sample.Keys.fill(false);session.RouteUIInput(sample,.016f,true);update();};
    auto named=[&](const char* name){auto e=scene->FindEntityByName(name);Check(bool(e),name);return uint64_t(e.GetUUID());};
    auto accept=[&](uint64_t id){neutral();Check(scene->FocusUI(id),"Persistent settings target is ineligible");sample.Keys[Key::Enter]=true;session.RouteUIInput(sample,.016f,true);update();neutral();};
    auto start=[&]{session.Start(project,authored);scene=session.GetScene();session.SetInput(sample.Point,true);neutral();neutral();};
    start();accept(named("Open Settings"));accept(named("Mute sound"));
    const auto file=privateRoot/project->GetConfig().SaveNamespace/"slot-presentation.save";
    Check(session.GetAudioVolume()==0&&std::filesystem::is_regular_file(file),"Actual player mute was not written to its isolated project slot");
    session.Stop();start();
    Check(session.GetAudioVolume()==0&&scene->IsUIChecked(named("Mute sound")),"Player settings did not restore audio and control state together");
    accept(named("Open Settings"));
    auto backup=file;backup+=".accepted";std::filesystem::rename(file,backup);std::filesystem::create_directory(file);
    accept(named("Mute sound"));
    Check(session.GetAudioVolume()==1&&std::filesystem::is_directory(file)&&std::filesystem::is_regular_file(backup)&&scene->FindEntityByName("Settings status").GetComponent<TextComponent>().TextString.find("could not save")!=std::string::npos,"Failed settings write lied about persistence or lost the accepted live value");
    std::filesystem::remove(file);std::filesystem::rename(backup,file);
    accept(named("Mute sound"));accept(named("Mute sound"));
    Check(session.Storage().Read("presentation")=="UI1|1|0"&&session.Storage().Read("journey").empty(),"Settings recovery affected progress or failed to save the retry");
    session.Stop();
    std::cout<<"PASS: isolated real player setting write/restart, restored audio/control agreement, failed-write live feedback and repair/retry; journey untouched\n";
}

static void LightkeeperPauseIntegrationChecks(RuntimeSession& session) {
    auto scene=session.GetScene();GameUI::DeviceSample sample;sample.Point={10,10};session.SetInput(sample.Point,true);
    auto named=[&](const char* name){auto e=scene->FindEntityByName(name);Check(bool(e),name);return uint64_t(e.GetUUID());};
    auto update=[&]{RenderCommand::Clear();scene->OnUpdateRuntime(.016f);Check(scene->GetUIError().empty(),"Pause/settings published an invalid UI state");};
    auto neutral=[&]{sample.Keys.fill(false);session.RouteUIInput(sample,.016f,true);update();};
    auto press=[&](int key){sample.Keys[key]=true;session.RouteUIInput(sample,.016f,true);update();neutral();};
    auto accept=[&](uint64_t id){neutral();Check(scene->FocusUI(id),"Pause control is ineligible");press(Key::Enter);};
    neutral();neutral();accept(named("Pause button"));
    auto pause=named("Pause screen"),settings=named("Settings screen");
    Check(scene->IsUIScreenOpen(pause)&&scene->IsGameplayInputBlocked(),"HUD event did not open the managed pause screen");
    accept(named("Settings"));Check(scene->IsUIScreenOpen(settings)&&scene->IsUIScreenOpen(pause),"Pause settings did not preserve its underlying screen");
    accept(named("Settings Back"));Check(!scene->IsUIScreenOpen(settings)&&scene->IsUIScreenOpen(pause),"Settings Back discarded pause");
    press(Key::Escape);Check(!scene->IsUIScreenOpen(pause),"Pause Cancel did not resume the game");
    // Cancel releases a restored nonmodal HUD's keyboard ownership. Its frame
    // must not also become the gameplay pause shortcut.
    press(Key::Escape);neutral();neutral();
    Check(!scene->IsUIScreenOpen(pause)&&session.IsGameplayInputEnabled(),"HUD Cancel reopened pause or stranded gameplay ownership");
    session.SetInput({},false);
    std::cout<<"PASS: HUD typed Pause, nested settings/Back, semantic Cancel, restored HUD cancellation and gameplay ownership\n";
}
