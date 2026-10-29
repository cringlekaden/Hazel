// Controlled failures through public host metrics and a test-owned retired WAV.
// No production injection hook or user project/profile is involved.
static void UIFlagFailureChecks(const Ref<Project>& project) {
    auto authored=Make("Flag probe","Migration.UIFlagFailureProbe");
    auto canvas=authored->CreateEntityWithUUID(100,"Atomic canvas");
    auto& c=canvas.AddComponent<UICanvasComponent>();c.ReferenceSize={400,400};c.SafeInsets={};
    auto button=authored->CreateEntityWithUUID(101,"Atomic button");
    authored->Reparent(button,canvas,TransformPolicy::KeepLocal);
    button.AddComponent<UIRectComponent>().FixedSize={100,40};
    button.AddComponent<UIButtonComponent>().Action="retired flag gesture";
    const auto authoredBefore=SceneSerializer(authored).CaptureAuthoredState();
    RuntimeSession session;session.Resize(400,400);session.ResizeFramebuffer(400,400);session.Start(project,authored);
    auto scene=session.GetScene();auto probe=ScriptEngine::GetEntityScriptInstance(2);
    auto klass=probe->GetScriptClass();
    for(int property=0;property<6;++property) {
        GameUI::InputFrame frame;frame.Enabled=true;frame.Mouse={20,20};
        scene->UpdateUIInput(frame);
        frame.PointerPressed=frame.PointerReleased=true;frame.PressPosition=frame.Mouse;
        scene->UpdateUIInput(frame);
        frame.PointerPressed=frame.PointerReleased=false;scene->UpdateUIInput(frame);
        frame.PointerPressed=true;frame.LeftDown=true;scene->UpdateUIInput(frame);
        Check(scene->IsUICapturingPointer(),"Flag failure fixture did not capture");
        const auto before=SceneSerializer(scene).CaptureAuthoredState();
        // Valid authored data, but a faulty host surface makes the setter's
        // refresh fail after its write/retirement. Repair is explicit below.
        scene->SetUIFramebufferSize({std::numeric_limits<float>::quiet_NaN(),400});
        probe->SetFieldValue<int>("Property",property);
        klass->InvokeMethod(probe->GetManagedObject(),klass->GetMethod("ChangeFlag",0));
        Check(probe->GetFieldValue<bool>("Rejected"),"Flag refresh failure was not a managed rejection");
        Check(SceneSerializer(scene).CaptureAuthoredState()==before,
              "Rejected UI flag setter retained its partial authored write");
        Check(!scene->GetUISnapshot().Valid&&!scene->IsUICapturingPointer(),
              "Failed surface publication retained valid geometry/capture");
        scene->SetUIFramebufferSize({400,400});scene->RefreshUI();
        GameUI::Action action;
        Check(scene->GetUISnapshot().Valid&&!scene->ConsumeUIAction(0,action),
              "Surface repair resurrected retired flag input/events");
        frame.PointerPressed=false;frame.PointerReleased=true;frame.LeftDown=false;scene->UpdateUIInput(frame);
        Check(!scene->ConsumeUIAction(0,action),"Interrupted release activated after failed setter");
    }
    session.Stop();
    Check(SceneSerializer(authored).CaptureAuthoredState()==authoredBefore,
          "Flag failure/recovery changed the authored source scene");
    std::cout<<"PASS: six managed UI flags, post-write refresh rejection, unchanged authored data, fail-closed geometry and irreversible input retirement\n";
}

class RecoveryNativeProbe : public ScriptableEntity {
public:
    inline static int Creates=0,Destroys=0;
    inline static bool Ordered=true;
    void OnCreate() override {++Creates;}
    void OnDestroy() override {
        ++Destroys;
        Ordered&=!ScriptEngine::GetRuntimeSession()&&GetComponent<Rigidbody2DComponent>().RuntimeBody;
    }
};

static void TargetStartFailureChecks(const Ref<Project>& project,const std::filesystem::path& directory) {
    auto source=Make("Restartable source","Migration.SceneProbe");
    source->GetEntityByUUID(2).AddComponent<NativeScriptComponent>().Bind<RecoveryNativeProbe>();
    source->CreateEntityWithUUID(102,"Previous startup dependency").AddComponent<AudioSourceComponent>();
    auto canvas=source->CreateEntityWithUUID(100,"Restartable menu");
    auto& c=canvas.AddComponent<UICanvasComponent>();c.ReferenceSize={400,400};c.SafeInsets={};
    c.Screen.Managed=true;c.Screen.InitiallyOpen=true;c.Screen.InitialFocus=101;c.Screen.EventOwner=2;
    auto button=source->CreateEntityWithUUID(101,"Restartable action");
    source->Reparent(button,canvas,TransformPolicy::KeepLocal);
    button.AddComponent<UIRectComponent>().FixedSize={100,40};
    button.AddComponent<UIButtonComponent>().Action="retired transition edge";
    auto target=Make("Failing target","Migration.RecoveryTargetProbe");
    auto audio=target->CreateEntityWithUUID(3,"Missing startup audio");
    auto& clip=audio.AddComponent<AudioSourceComponent>();
    clip.Clip="Audio/controlled-missing.wav";clip.PlayOnStart=true;
    const auto dependency=project->GetAssetRoot()/clip.Clip;
    std::filesystem::create_directories(dependency.parent_path());
    std::ofstream(dependency)<<"test-owned dependency retired before native playback";
    auto klass=ScriptEngine::GetEntityClass("Migration.RecoveryTargetProbe");
    void* parameter=mono_string_new(mono_domain_get(),dependency.generic_u8string().c_str());
    klass->InvokeMethod(nullptr,klass->GetMethod("Configure",1),&parameter);
    const auto path=directory/"Assets/Scenes/RecoveryTarget.hazel";
    SceneSerializer(target,project->GetAssetRoot()).Serialize(path.generic_u8string());
    SceneSerializer(Make("Must not receive retired request"),project->GetAssetRoot())
        .Serialize((directory/"Assets/Scenes/RecoveryTrap.hazel").generic_u8string());
    RecoveryNativeProbe::Creates=RecoveryNativeProbe::Destroys=0;RecoveryNativeProbe::Ordered=true;
    const auto baseline=SceneSerializer(source).CaptureAuthoredState();
    RuntimeSession session;session.Resize(400,400);session.Start(project,source);
    session.Storage().Configure(directory/"recovery-storage","recovery",false);
    session.Storage().Write("prior","accepted before transition");
    session.SetAudioVolume(.25f);session.SetInput({390,390},true);session.Update(0);session.Update(0);
    const auto identity=session.GetIdentity();auto previous=session.GetScene();
    const auto sceneIdentity=previous->GetIdentity();
    auto oldInstance=ScriptEngine::GetEntityScriptInstance(2);
    oldInstance->SetFieldValue<float>("Speed",19);
    Check(previous->FocusUI(101),"Recovery fixture could not focus its action");
    auto queueActivation=[&] {
        KeyPressedEvent down(Key::Enter,false);KeyReleasedEvent up(Key::Enter);
        session.SubmitUIEvent(down,{390,390},true);session.SubmitUIEvent(up,{390,390},true);
    };
    queueActivation();session.Update(0);
    GameUI::Action action;
    Check(previous->ConsumeUIEvent(2,action),"Recovery fixture did not accept its buffered UI edge");
    queueActivation();
    Check(session.RequestSceneLoad("Scenes/RecoveryTarget.hazel"),"Startup failure request rejected before admission");
    session.Update(0);
    auto restarted=ScriptEngine::GetEntityScriptInstance(2);
    Check(session.GetScene()==previous&&previous->GetIdentity()==sceneIdentity&&
          session.GetIdentity()==identity&&previous->IsRunning()&&
          previous->GetEntityByUUID(2).GetComponent<Rigidbody2DComponent>().RuntimeBody&&
          ScriptEngine::GetSceneContext()==previous.get()&&ScriptEngine::GetRuntimeSession()==&session,
          "Native target-start failure did not restore a usable previous session");
    Check(!oldInstance->GetManagedObject()&&restarted!=oldInstance&&restarted->GetManagedObject()&&
          restarted->GetFieldValue<int>("Creates")==1&&restarted->GetFieldValue<float>("Speed")==2.5f,
          "Fallback retained retired handles/live fields instead of restarting scripts");
    Check(RecoveryNativeProbe::Creates==2&&RecoveryNativeProbe::Destroys==1&&RecoveryNativeProbe::Ordered,
          "Fallback native lifecycle/binding/physics ordering failed");
    Check(session.GetError().find("controlled-missing.wav")!=std::string::npos&&
          session.Storage().Read("prior")=="accepted before transition"&&
          session.Storage().Read("target-side-effect")=="accepted before native startup failure"&&
          session.GetAudioVolume()==.75f,
          "Fallback lost diagnostics/storage or promised rollback of accepted script side effects");
    auto* report=klass->InvokeMethod(nullptr,klass->GetMethod("Report",0));
    Check(report&&*static_cast<int*>(mono_object_unbox(report))==11111,
          "Partial target scripts/physics were not started/retired or teardown accepted session requests");
    Check(!session.ConsumeExitRequest(),"Failed target left an exit request for the recovered host");
    Check(!previous->ConsumeUIEvent(2,action),"Restart replayed input buffered for the retired runtime");
    session.Update(0);
    Check(session.GetScene()==previous,"Failed target teardown queued a later transition");
    // Repair only the disposable target and prove a later transition succeeds.
    SceneSerializer(Make("Repaired target","Migration.SceneProbe"),project->GetAssetRoot())
        .Serialize(path.generic_u8string());
    session.RequestSceneLoad("Scenes/RecoveryTarget.hazel");session.Update(0);
    Check(session.GetScene()!=previous&&!previous->IsRunning()&&
          !previous->GetEntityByUUID(2).GetComponent<Rigidbody2DComponent>().RuntimeBody&&
          !restarted->GetManagedObject()&&session.GetError().empty(),
          "Repaired target could not replace/retire the restarted source");
    session.Stop();
    Check(SceneSerializer(source).CaptureAuthoredState()==baseline,
          "Startup fallback mutated the original authored scene");

    // If the previous scene's dependencies also fail, retire both partial
    // runtimes and propagate the failure, as Start already does.
    std::ofstream(dependency)<<"test-owned dependency";
    SceneSerializer(target,project->GetAssetRoot()).Serialize(path.generic_u8string());
    session.Start(project,source);
    session.Storage().Configure(directory/"recovery-storage","recovery",false);
    auto unavailablePrevious=session.GetScene();
    auto& unavailable=unavailablePrevious->GetEntityByUUID(102).GetComponent<AudioSourceComponent>();
    unavailable.Clip="Audio/previous-missing.wav";unavailable.PlayOnStart=true;
    session.RequestSceneLoad("Scenes/RecoveryTarget.hazel");
    bool rejected=false;
    try {session.Update(0);}catch(const std::runtime_error&){rejected=true;}
    Check(rejected&&!session.GetScene()&&!session.GetIdentity()&&
          !ScriptEngine::GetRuntimeSession()&&!ScriptEngine::GetSceneContext()&&
          !unavailablePrevious->IsRunning()&&
          !unavailablePrevious->GetEntityByUUID(2).GetComponent<Rigidbody2DComponent>().RuntimeBody&&
          !session.ConsumeExitRequest(),"Failed fallback restart retained a partially running session");
    session.Start(project,source);session.Update(0);session.Stop();
    std::cout<<"PASS: native target-start failure, restarted scene identity/scripts/physics, retired requests/input, storage side effects, gain, repaired retry and failed-fallback cleanup\n";
}
