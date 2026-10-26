#include "UITextEntryFixture.h"
static void UITextEntrySessionChecks(const Ref<Project>& project){
    GameUI::UIInputAdapter adapter;GameUI::DeviceSample neutral;adapter.Sample(neutral,.016f,true);adapter.Sample(neutral,.016f,true);
    adapter.PointerEdge(0,true,{10,10},true);adapter.PointerEdge(0,false,{10,10},true);auto quick=adapter.Sample(neutral,.016f,true,false,true);
    Check(quick.Events.size()==2&&quick.Events.front().Text.Shift&&quick.Events.front().Device==GameUI::InputDevice::Pointer,"Text-mode entry lost ordered quick pointer edges or the Shift snapshot");
    auto authored=TextEntryFixture();authored->GetEntityByUUID(900).AddComponent<ScriptComponent>().ClassName="Migration.TextEntryOwnerProbe";
    auto before=SceneSerializer(authored).CaptureAuthoredState();RuntimeSession session;session.Resize(400,300);session.ResizeFramebuffer(400,300);
    for(int cycle=0;cycle<3;++cycle){session.Start(project,authored);auto s=session.GetScene();auto owner=ScriptEngine::GetEntityScriptInstance(900);Check(owner&&owner->GetFieldValue<bool>("Ready"),"Managed text value getter failed");
        auto clipboard=CreateRef<SceneTextClipboard>();s->SetUIClipboard(clipboard);
        GameUI::DeviceSample sample;sample.Point={399,299};auto route=[&]{session.SetInput(sample.Point,true);session.RouteUIInput(sample,.016f,true);};route();route();Check(s->FocusUI(500),"Session text focus failed");
        if(cycle==1){sample.Pad.Device=0;route();route();sample.Pad.Buttons[size_t(GamepadButton::A)]=true;}else sample.Keys[Key::Enter]=true;
        route();s->OnUpdateRuntime(0);sample.Keys.fill(false);sample.Pad.Buttons.fill(false);route();route();Check(s->IsUITextEditing(),"Key/controller entry failed");
        KeyPressedEvent all(Key::A,false,2);session.SubmitUIEvent(all,sample.Point,true);KeyTypedEvent character(0xe9);session.SubmitUIEvent(character,sample.Point,true);route();s->OnUpdateRuntime(0);
        Check(s->GetUITextEntry(500)==u8"é"&&owner->GetFieldValue<bool>("Owned")&&owner->GetFieldValue<int>("Changes")==1,"Committed text and Ctrl selection did not reach one owner");
        KeyPressedEvent altgr(Key::A,false,6);session.SubmitUIEvent(altgr,sample.Point,true);KeyTypedEvent more('x');session.SubmitUIEvent(more,sample.Point,true);route();s->OnUpdateRuntime(0);Check(s->GetUITextEntry(500)==u8"éx","AltGr was interpreted as Ctrl+A or text lost");
        if(cycle==1){sample.Pad.Buttons[size_t(GamepadButton::A)]=true;route();s->OnUpdateRuntime(0);for(int held=0;held<4;++held){route();s->OnUpdateRuntime(0);}Check(!s->IsUITextEditing()&&owner->GetFieldValue<int>("Commits")==1,"Held controller Accept re-entered or duplicated completion");sample.Pad.Buttons.fill(false);route();route();sample.Pad.Buttons[size_t(GamepadButton::A)]=true;route();s->OnUpdateRuntime(0);sample.Pad.Buttons.fill(false);route();route();Check(s->IsUITextEditing(),"Fresh controller gesture could not re-enter text");}
        KeyPressedEvent copy(Key::C,false,2);session.SubmitUIEvent(copy,sample.Point,true);route(); // no selection: clipboard unchanged
        owner->SetFieldValue<bool>("Replace",true);s->OnUpdateRuntime(0);Check(s->GetUITextEntry(500)==u8"Programmatic é"&&!s->IsUITextEditing(),"Managed silent replacement did not end the old edit");
        if(cycle==1){ScriptEngine::ReloadAssembly();Check(!s->IsUITextEditing()&&s->GetUITextEntry(500)==u8"a\u0301","Domain replacement retained transient text/edits");}
        session.Stop();Check(SceneSerializer(authored).CaptureAuthoredState()==before,"Text Play/Stop entered authoring state");
    }
    std::cout<<"PASS: production committed-text/key adapters, AltGr separation, synthetic key/pad entry, managed owner payloads/replacement and three Play/Stop cycles\n";
}
