#include "UIBindingEditor.h"
#include "PropertyUI.h"
#include "Hazel/Core/Input.h"
#include <imgui.h>
namespace Hazel {
namespace {
const char* PadNames[]={"None","A / South","B / East","X / West","Y / North","Left bumper","Right bumper","Back","Start","Guide","Left thumb","Right thumb","D-pad Up","D-pad Right","D-pad Down","D-pad Left"};
bool CandidateKey(int key){if(key==Key::LeftControl||key==Key::RightControl||key==Key::LeftAlt||key==Key::RightAlt||key==Key::LeftSuper||key==Key::RightSuper)return false;return (key==32||key==39||(key>=44&&key<=57)||key==59||key==61||(key>=65&&key<=93)||key==96)||(key>=161&&key<=162)||(key>=256&&key<=269)||(key>=280&&key<=284)||(key>=290&&key<=314)||(key>=320&&key<=336)||(key>=340&&key<=348);}
void KeySelect(const char* label,int& key){
    PropertyUI::Row row(label,label);
    if(ImGui::BeginCombo("##key",GameUI::KeyName(key).c_str())){
        if(ImGui::Selectable("None",key==-1))key=-1;
        for(int k=32;k<=348;++k)if(CandidateKey(k)&&ImGui::Selectable(GameUI::KeyName(k).c_str(),key==k))key=k;
        ImGui::EndCombo();
    }
}
}
bool UIBindingEditor::Valid(){try{GameUI::Validate(Values);Error.clear();return true;}catch(const std::exception& error){Error=error.what();return false;}}
void UIBindingEditor::Draw(bool editable){
    if(!editable)CancelCapture();
    ImGui::BeginDisabled(!editable);
    PropertyUI::DragInt("pointer","Pointer button",Values.PointerButton,1,0,7);
    int stick=Values.Stick+1;PropertyUI::Combo("stick","Navigation stick",stick,"None\0Left\0Right\0");Values.Stick=stick-1;
    PropertyUI::SliderFloat("dead","Engage threshold",Values.DeadZone,.1f,1);
    PropertyUI::SliderFloat("neutral","Neutral threshold",Values.NeutralZone,0,.9f);
    PropertyUI::DragFloat("delay","Repeat delay (s)",Values.RepeatDelay,.01f,.1f,2);
    PropertyUI::DragFloat("interval","Repeat interval (s)",Values.RepeatInterval,.01f,.03f,1);
    for(size_t a=1;a<size_t(GameUI::UIAction::Count);++a){ImGui::PushID(int(a));if(ImGui::TreeNode(GameUI::ActionName(GameUI::UIAction(a)))){
        auto& b=Values.Actions[a];KeySelect("Key",b.Key);KeySelect("Alternate",b.Alternate);
        int shift=int(b.Shift);PropertyUI::Combo("shift","Shift",shift,"Either\0Without Shift\0With Shift\0");b.Shift=GameUI::ShiftMatch(shift);
        {PropertyUI::Row row("pad","Gamepad button");if(ImGui::BeginCombo("##value",PadNames[b.Pad+1])){for(int p=-1;p<15;++p)if(ImGui::Selectable(PadNames[p+1],p==b.Pad))b.Pad=p;ImGui::EndCombo();}}
        if(ImGui::Button("Capture key")){CaptureAction=int(a);CaptureSlot=0;Neutral=false;}ImGui::SameLine();
        if(ImGui::Button("Capture alternate")){CaptureAction=int(a);CaptureSlot=1;Neutral=false;}
        ImGui::TreePop();}ImGui::PopID();}
    if(Capturing()){
        ImGui::TextWrapped("%s: release keys, then press a key. Escape cancels. Choose Escape from the list to bind it.",GameUI::ActionName(GameUI::UIAction(CaptureAction)));
        bool any=false;int pressed=-1;
        for(int k=32;k<=348;++k)if(CandidateKey(k)&&Input::IsKeyPressed(KeyCode(k))){any=true;if(k<340)pressed=k;}
        if(ImGui::GetIO().AppFocusLost||Input::IsKeyPressed(Key::Escape)||ImGui::Button("Cancel capture"))CancelCapture();
        else if(!any)Neutral=true;
        else if(Neutral&&pressed>=0){auto& b=Values.Actions[size_t(CaptureAction)];(CaptureSlot?b.Alternate:b.Key)=pressed;CancelCapture();}
    }
    if(ImGui::Button("Reset form to engine defaults")){Values=GameUI::UIBindings::Defaults();CancelCapture();}
    ImGui::EndDisabled();
    Valid();if(!Error.empty())PropertyUI::Validation(Error.c_str());
}
}
