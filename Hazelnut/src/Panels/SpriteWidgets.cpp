#include "SpriteWidgets.h"
#include "ContentBrowserPayload.h"
#include "Hazel/Project/Project.h"
#include "Hazel/Utils/PlatformUtils.h"
#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>
#include <unordered_map>
namespace Hazel {
std::string SpriteDragText(const std::filesystem::path& sheet,SpriteID id) {return sheet.generic_u8string()+"\n"+SpriteIDText(id);}
namespace {
struct PickerState {std::string Path,Error;std::filesystem::path Root;};
std::unordered_map<ImGuiID,PickerState> States;
std::pair<std::filesystem::path,SpriteID> DragReference(const ImGuiPayload* payload) {
    if(!payload || payload->DataSize<=0 || static_cast<const char*>(payload->Data)[payload->DataSize-1]!='\0') throw std::runtime_error("Malformed sprite payload");
    std::string text(static_cast<const char*>(payload->Data),payload->DataSize-1);auto at=text.find('\n');
    if(at==std::string::npos) throw std::runtime_error("Malformed sprite reference");
    return {std::filesystem::u8path(text.substr(0,at)),ParseSpriteID(text.substr(at+1))};
}
bool Pick(const char* label,std::filesystem::path& sheet,SpriteID& id,bool clip) {
    if(!Project::GetActive()) {ImGui::TextDisabled("Open a project to select sprites");return false;}
    ImGui::PushID(label);auto& state=States[ImGui::GetID("picker")];const auto root=Project::GetAssetDirectory();
    if(state.Root!=root) {state={};state.Root=root;}
    if(state.Path.empty()) state.Path=sheet.generic_u8string();
    bool changed=false;ImGui::TextUnformatted(label);ImGui::InputText("Sheet path (Assets-relative)",&state.Path);
    ImGui::SameLine();if(ImGui::SmallButton("Browse")) {
        auto path=FileDialogs::OpenFile("Sprite sheets\0*.hsprites\0");
        if(!path.empty()) try {auto ref=Project::MakeAssetReference(root,std::filesystem::u8path(path));Project::ResolveOwnedAsset(root,ref);state.Path=ref.generic_u8string();state.Error.clear();}catch(const std::exception& e){state.Error=e.what();}
    }
    std::string current=id?SpriteIDText(id):"Unassigned";
    try {if(id) {auto definition=Project::GetActive()->GetAssets()->Sheet(sheet);current=clip?definition->Clip(id).Name:definition->Region(id).Name;}}
    catch(const std::exception& e){current="Missing: "+SpriteIDText(id);state.Error=e.what();}
    if(ImGui::BeginCombo(clip?"Clip":"Region",current.c_str())) {
        try {
            auto ref=std::filesystem::u8path(state.Path);auto definition=Project::GetActive()->GetAssets()->Sheet(ref);
            if(clip) for(auto& c:definition->Clips) {ImGui::PushID(SpriteIDText(c.ID).c_str());if(ImGui::Selectable(c.Name.c_str(),sheet==ref&&id==c.ID)){sheet=ref;id=c.ID;changed=true;state.Error.clear();}ImGui::PopID();}
            else for(auto& r:definition->Regions) {ImGui::PushID(SpriteIDText(r.ID).c_str());if(ImGui::Selectable(r.Name.c_str(),sheet==ref&&id==r.ID)){sheet=ref;id=r.ID;changed=true;state.Error.clear();}ImGui::PopID();}
        }catch(const std::exception& e){ImGui::TextWrapped("%s",e.what());}
        ImGui::EndCombo();
    }
    if(ImGui::BeginDragDropTarget()) {
        try {
            if(auto* payload=ImGui::AcceptDragDropPayload(clip?"HAZEL_CLIP":"HAZEL_SPRITE")) {
                auto r=DragReference(payload);auto assets=Project::GetActive()->GetAssets();
                if(clip) assets->Clip({r.first,r.second});else assets->Resolve(SpriteReference{r.first,r.second});
                sheet=r.first;id=r.second;state.Path=sheet.generic_u8string();changed=true;state.Error.clear();
            }
            if(auto* payload=ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM")) {
                auto ref=Project::MakeAssetReference(root,ContentBrowserPath(payload->Data,payload->DataSize));
                Project::GetActive()->GetAssets()->Sheet(ref);state.Path=ref.generic_u8string();state.Error.clear();
            }
        }catch(const std::exception& e){state.Error=e.what();}
        ImGui::EndDragDropTarget();
    }
    if(ImGui::SmallButton("Clear reference")){sheet.clear();id=0;changed=true;state.Error.clear();}
    if(!state.Error.empty()) ImGui::TextColored({1,.4f,.3f,1},"%s",state.Error.c_str());
    ImGui::PopID();return changed;
}
}
bool SpritePicker(const char* label,SpriteReference& r) {return Pick(label,r.Sheet,r.Region,false);}
bool ClipPicker(const char* label,AnimationReference& r) {return Pick(label,r.Sheet,r.Clip,true);}
void SpriteSourceEditor(SpriteRendererComponent& component) {
    int type=static_cast<int>(component.Source.index());
    if(ImGui::Combo("Source type",&type,"Color only\0Whole texture\0Sheet region\0")) {
        if(type==0) component.Source=std::monostate{};
        else if(type==1) component.Source=TextureSpriteSource{};
        else component.Source=SpriteReference{};
    }
    if(auto* texture=std::get_if<TextureSpriteSource>(&component.Source)) {
        auto& feedback=States[ImGui::GetID("texture feedback")];
        ImGui::TextWrapped("%s",texture->Texture.empty()?"Drop a texture here":texture->Texture.generic_u8string().c_str());
        ImGui::Button("Assign texture");
        if(ImGui::BeginDragDropTarget()) {
            if(auto* payload=ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM")) {
                try {
                    auto ref=Project::MakeAssetReference(Project::GetAssetDirectory(),ContentBrowserPath(payload->Data,payload->DataSize));
                    Project::GetActive()->GetAssets()->Texture(ref,TextureSpecification::FileDefaults(),false);
                    texture->Texture=ref;texture->Resource.reset();
                    feedback.Error.clear();
                }catch(const std::exception& e){feedback.Error=e.what();}
            }
            ImGui::EndDragDropTarget();
        }
        ImGui::DragFloat("Tiling Factor",&texture->TilingFactor,.1f,0,100);
        if(!feedback.Error.empty())ImGui::TextColored({1,.4f,.3f,1},"%s",feedback.Error.c_str());
    } else if(auto* region=std::get_if<SpriteReference>(&component.Source)) {
        SpritePicker("Sprite",*region);ImGui::TextDisabled("Atlas regions use tiling 1");
    }
    if(!component.Resolved.Error.empty()) ImGui::TextColored({1,.4f,.3f,1},"%s",component.Resolved.Error.c_str());
}
}
