// Production RuntimeSession adapter, Scene snapshot, native writes, managed API and storage.
static void UIActionSessionChecks(const Ref<Project>& project,const std::filesystem::path& directory) {
    using namespace GameUI;
    auto authored=Make("UI action API","Migration.UIActionsProbe");
    auto canvas=authored->CreateEntityWithUUID(80,"Action menu");auto& c=canvas.AddComponent<UICanvasComponent>();c.ReferenceSize={400,400};c.SafeInsets={0,0,0,0};c.Modal=true;
    for(uint64_t i=0;i<4;++i){auto e=authored->CreateEntityWithUUID(81+i,"Action button");authored->Reparent(e,canvas,TransformPolicy::KeepLocal);auto& r=e.AddComponent<UIRectComponent>();r.FixedSize={80,40};r.Offset={float((i%2)*120),float((i/2)*80)};auto& b=e.AddComponent<UIButtonComponent>();b.Action="test";b.TabOrder=int(i);}
    auto previous=project->GetConfig();project->GetConfig().SaveNamespace="ui-actions-test";project->GetConfig().StartScene="Scenes/Target.hazel";
    auto defaults=UIBindings::Defaults();defaults.Actions[size_t(UIAction::Accept)].Alternate=Key::Z;project->GetConfig().UIBindings=defaults;
    auto yaml=ProjectSerializer(project).SerializeText();auto descriptor=directory/"bindings.hproj";std::ofstream(descriptor)<<yaml;
    auto reopened=Project::LoadCandidate(descriptor);Check(reopened&&SerializeBindings(reopened->GetUIBindings())==SerializeBindings(defaults),"Project UI defaults lost persistence");
    const char* savedRoot=std::getenv("HAZEL_SAVE_ROOT");const std::string oldRoot=savedRoot?savedRoot:"";
    auto changeRoot=[](const std::string& root){
#ifdef HZ_PLATFORM_WINDOWS
        _putenv_s("HAZEL_SAVE_ROOT",root.c_str());
#else
        if(root.empty())unsetenv("HAZEL_SAVE_ROOT");else setenv("HAZEL_SAVE_ROOT",root.c_str(),1);
#endif
    };
    changeRoot((directory/"private-user").string());
    RuntimeSession session;session.SetPersistentStorage(true);session.Resize(400,400);session.Start(project,authored);
    Check(ScriptEngine::GetEntityScriptInstance(2)->GetFieldValue<bool>("Passed"),"Managed UI binding/settings round trips failed");
    DeviceSample sample;auto route=[&]{session.SetInput(sample.Point,true);session.RouteUIInput(sample,.016f,true);};
    route();sample.Pad.Device=0;route();sample.Pad.Buttons[size_t(GamepadButton::Right)]=true;route();
    auto scene=session.GetScene();Check(scene->GetUIDiagnostics().Focus==82&&scene->IsGameplayInputBlocked()&&!session.IsGameplayInputEnabled(),"Production spatial gamepad input / gameplay gate failed");
    sample.Pad.Buttons.fill(false);route();sample.Pad.Buttons[size_t(GamepadButton::A)]=true;route();Action action;
    Check(scene->ConsumeUIAction(0,action)&&action.Entity==82&&!scene->ConsumeUIAction(0,action),"Gamepad activation lost/duplicated at scene boundary");
    sample.Pad.Buttons.fill(false);route();sample.Pad.Buttons[size_t(GamepadButton::B)]=true;route();
    Check(!scene->ConsumeUICancel(0)&&scene->ConsumeUICancel(80)&&!scene->ConsumeUICancel(80),"Cancel escaped its current canvas scope or duplicated");
    sample.Pad.Buttons.fill(false);route();scene->GetEntityByUUID(82).GetComponent<UIRectComponent>().Visible=false;scene->RefreshUI();Check(scene->GetUIDiagnostics().Focus==81,"Native visibility mutation retained focus");
    // Pending cancellation must reconcile changed scope at delivery as activations do.
    sample.Pad.Buttons[size_t(GamepadButton::B)]=true;route();scene->GetEntityByUUID(80).GetComponent<UICanvasComponent>().Modal=false;Check(!scene->ConsumeUICancel(80),"Cancel targeted a retired modal scope");
    sample.Pad.Device=-1;sample.Pad.Buttons.fill(false);route();route();
    session.CancelUIGestures();route();
    for(auto key:{Key::Tab,Key::Enter}){KeyPressedEvent down(key);KeyReleasedEvent up(key);session.SubmitUIEvent(down,{300,300},true);session.SubmitUIEvent(up,{300,300},true);}
    route();Check(scene->ConsumeUIAction(0,action)&&action.Entity==81&&!scene->ConsumeUIAction(0,action),"Ordered host Tab/Accept edges were lost at nonmodal ownership entry");
    Check(scene->IsGameplayInputBlocked(),"Consumed quick UI gesture leaked to gameplay");
    sample.Keys[Key::Escape]=true;route();Check(scene->ConsumeUICancel(0)&&scene->IsGameplayInputBlocked(),"Nonmodal Cancel released its gameplay gate during the consumed frame");
    route();Check(!session.IsGameplayInputEnabled(),"Held consumed Cancel leaked to gameplay on the next update");
    sample.Keys.fill(false);route();Check(!scene->IsGameplayInputBlocked(),"Cancel did not release nonmodal keyboard ownership afterward");
    scene->GetEntityByUUID(80).GetComponent<UICanvasComponent>().Modal=true;scene->RefreshUI();route();
    MouseButtonPressedEvent pointerDown(Mouse::ButtonLeft);MouseButtonReleasedEvent pointerUp(Mouse::ButtonLeft);
    session.SubmitUIEvent(pointerDown,{10,10},true);session.SubmitUIEvent(pointerUp,{10,10},true);
    KeyPressedEvent rightDown(Key::Right);KeyReleasedEvent rightUp(Key::Right);
    session.SubmitUIEvent(rightDown,{10,10},true);session.SubmitUIEvent(rightUp,{10,10},true);route();
    Check(scene->GetUIDiagnostics().Focus==84&&scene->ConsumeUIAction(0,action)&&action.Entity==81&&!scene->ConsumeUIAction(0,action),"Ordered fresh navigation after a pointer click was lost/duplicated in the real scene");
    authored->GetEntityByUUID(2).RemoveComponent<ScriptComponent>();
    auto override=defaults;override.Actions[size_t(UIAction::Accept)].Key=Key::X;session.ApplyUIBindings(override);
    Check(ParseBindings(session.Storage().Read("engine-ui-bindings")).Actions[size_t(UIAction::Accept)].Key==Key::X,"Override was not stored through project user-data service");
    session.Stop();session.Start(project,authored);Check(session.GetUIBindings().Actions[size_t(UIAction::Accept)].Key==Key::X,"Persistent override did not survive restart");
    session.Storage().Write("engine-ui-bindings","invalid override");session.Stop();session.Start(project,authored);
    Check(session.GetUIBindings().Actions[size_t(UIAction::Accept)].Key==Key::Enter&&!session.UIBindingWarning().empty(),"Corrupt override did not retain usable project defaults");
    session.ResetUIBindings();session.Stop();session.Start(project,authored);Check(session.UIBindingWarning().empty()&&session.GetUIBindings().Actions[size_t(UIAction::Accept)].Alternate==Key::Z,"Override reset did not restore project defaults");
    // Failed persistence must leave effective state intact.
    auto root=directory/"not-a-directory";std::ofstream(root)<<"occupied";session.Storage().Configure(root,"ui-actions-test",true);
    bool failed=false;try{session.ApplyUIBindings(override);}catch(const std::exception&){failed=true;}
    Check(failed&&session.GetUIBindings().Actions[size_t(UIAction::Accept)].Key==Key::Enter,"Failed override save changed effective bindings");
    session.Stop();project->GetConfig()=previous;changeRoot(oldRoot);
    std::cout<<"PASS: production UI actions, spatial pad routing, scoped Cancel, native retirement, managed bindings and persistent/corrupt/reset/failed overrides\n";
}
