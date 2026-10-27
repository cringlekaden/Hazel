#pragma once
#include "PropertyUI.h"
#include "Hazel/Scene/Components.h"
#include "Hazel/Assets/ProjectAssets.h"
#include <imgui_internal.h>
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
namespace Hazel {
// Range authoring uses grapheme indices/snippets, never encoded byte offsets.
inline void DrawTextEmphasis(TextComponent& text,bool plainOnly,const Ref<ProjectAssets>& assets) {
    if(!ImGui::TreeNode("Inline emphasis"))return;
    struct PopTree {~PopTree(){ImGui::TreePop();}} popTree;
    if(plainOnly){ImGui::TextWrapped("This computed caption / editable field is plain text. Remove emphasis to repair it.");if(!text.Spans.empty()){PropertyUI::Row row("clear-spans","Emphasis");if(row.Result(ImGui::Button("Remove all"),true))text.Spans.clear();}return;}
    const auto original=text.Spans;
    auto map=Text::SourceMap::Analyze(text.TextString);
    for(size_t i=0;i<text.Spans.size();++i){ImGui::PushID(int(i));struct PopID {~PopID(){ImGui::PopID();}} popID;auto& s=text.Spans[i];
        const auto begin=map.Offset(s.Begin,Text::SourceUnit::UTF8Byte);
        const auto shortEnd=map.Scalar(std::min(map.Offset(s.End,Text::SourceUnit::Grapheme),map.Offset(s.Begin,Text::SourceUnit::Grapheme)+32),Text::SourceUnit::Grapheme);
        auto snippet=text.TextString.substr(begin,map.Offset(shortEnd,Text::SourceUnit::UTF8Byte)-begin);
        ImGui::TextWrapped("%u–%u: %s",map.Offset(s.Begin,Text::SourceUnit::Grapheme),map.Offset(s.End,Text::SourceUnit::Grapheme),snippet.c_str());
        int face=s.Face?int(*s.Face)+1:0;if(PropertyUI::Combo("face","Face",face,"Inherit\0Regular\0Bold\0Italic\0Bold Italic\0"))s.Face=face?std::optional<FontStyle>(FontStyle(face-1)):std::nullopt;
        if(s.Face&&assets){auto status=assets->FamilyStatus({text.Font.Family,*s.Face});
            if(status.StyleFallback)ImGui::TextWrapped("%s is absent; actual face is Regular (not synthesized).",FontStyleName(*s.Face));
            if(!status.Error.empty())ImGui::TextWrapped("%s",status.Error.c_str());
            if(!status.Actual->PageError().empty())ImGui::TextWrapped("%s",status.Actual->PageError().c_str());}
        bool color=bool(s.Color);if(PropertyUI::Checkbox("explicit-color","Explicit color",color)){if(color)s.Color=glm::vec4(glm::vec3(text.Color),1);else s.Color.reset();}
        if(s.Color)PropertyUI::Color("color","Color",glm::value_ptr(*s.Color));
        bool remove=false;{PropertyUI::Row row("remove","Range");remove=ImGui::Button("Remove emphasis");row.Result(remove,true);}
        if(remove){text.Spans.erase(text.Spans.begin()+i);--i;}
    }
    auto* storage=ImGui::GetStateStorage();auto key=[](const char* name){return ImGui::GetID(name);};
    int begin=storage->GetInt(key("new-begin"),0),end=storage->GetInt(key("new-end"),int(std::min(1u,map.Graphemes()))),face=storage->GetInt(key("new-face"),2);
    PropertyUI::DragInt("new-begin","From grapheme",begin,1,0,int(map.Graphemes()));PropertyUI::DragInt("new-end","To (exclusive)",end,1,0,int(map.Graphemes()));
    storage->SetInt(key("new-begin"),begin);storage->SetInt(key("new-end"),end);
    PropertyUI::Combo("new-face","New face",face,"Inherit\0Regular\0Bold\0Italic\0Bold Italic\0");storage->SetInt(key("new-face"),face);
    bool newColor=storage->GetInt(key("new-color"),0)!=0;PropertyUI::Checkbox("new-color","New explicit color",newColor);storage->SetInt(key("new-color"),newColor);
    glm::vec4 color;const char* channels[]={"new-r","new-g","new-b","new-a"};for(int i=0;i<4;++i)color[i]=storage->GetFloat(key(channels[i]),i<3?text.Color[i]:1);
    if(newColor)PropertyUI::Color("new-rgba","New color",glm::value_ptr(color));for(int i=0;i<4;++i)storage->SetFloat(key(channels[i]),color[i]);
    if(begin>=0&&end>=begin&&uint32_t(end)<=map.Graphemes()){
        auto a=map.Scalar(begin,Text::SourceUnit::Grapheme),b=map.Scalar(end,Text::SourceUnit::Grapheme);
        auto snippet=text.TextString.substr(map.Offset(a,Text::SourceUnit::UTF8Byte),map.Offset(b,Text::SourceUnit::UTF8Byte)-map.Offset(a,Text::SourceUnit::UTF8Byte));
        PropertyUI::ReadOnly("snippet","Selected text",snippet.c_str());
    }
    bool add=false;{PropertyUI::Row row("add-span","Emphasis");ImGui::BeginDisabled(!face&&!newColor);add=ImGui::Button("Apply range");row.Result(add,true);ImGui::EndDisabled();}
    if(add){auto next=text.Spans;next.push_back({map.Scalar(uint32_t(begin),Text::SourceUnit::Grapheme),map.Scalar(uint32_t(end),Text::SourceUnit::Grapheme),face?std::optional<FontStyle>(FontStyle(face-1)):std::nullopt,newColor?std::optional<glm::vec4>(color):std::nullopt});std::sort(next.begin(),next.end(),[](const auto& a,const auto& b){return a.Begin<b.Begin;});text.Spans=Text::NormalizeSpans(map,next);}
    if(text.Spans!=original)text.Spans=Text::NormalizeSpans(map,text.Spans);
    PropertyUI::Help("Half-open grapheme range. Apply a real family face, then optionally set its color. Overlaps and shaped-cluster splits reject; remove a range before replacing it. Preview in the viewport.");
}
}
