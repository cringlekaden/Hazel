#include "SceneHierarchyPanel.h"
#include "ContentBrowserPayload.h"
#include "Hazel/Scene/Components.h"

#include "Hazel/Project/Project.h"
#include "Hazel/Scene/Prefab.h"
#include "Hazel/Scripting/ScriptEngine.h"
#include "Hazel/UI/UI.h"
#include "Hazel/Core/Application.h"
#include "SpriteWidgets.h"
#include "UI/PropertyUI.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <misc/cpp/imgui_stdlib.h>

#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <cmath>
#include <cctype>
#include <cstring>
#include <cstdio>
#include <iterator>
#include <type_traits>

/* The Microsoft C++ compiler is non-compliant with the C++ standard and needs
 * the following definition to disable a security warning on std::strncpy().
 */
#ifdef _MSVC_LANG
#define _CRT_SECURE_NO_WARNINGS
#endif

namespace Hazel
{

SceneHierarchyPanel::SceneHierarchyPanel(const Ref<Scene> &context) { SetContext(context); }

void SceneHierarchyPanel::SetContext(const Ref<Scene> &context)
{
    FinishUIEdit();
    m_SelectionContext = {};
    m_HierarchyError.clear();m_Matches.clear();m_Reveal.clear();m_DeleteWanted=false;m_DeleteScene=m_DeleteEntity=0;m_DragHover=0;m_DragHoverSince=0;
    m_Pickers.clear();m_UIStatePickers={};m_UIFontInitialized=false;
    m_PrefabChoices.clear();
    m_PrefabChoicesReady = false;
    m_Context = context;
}

bool SceneHierarchyPanel::CanEdit(bool report) const
{
    const auto available =
        Availability
            ? Availability(EditOperation)
            : ActionAvailability{m_Context && !m_Context->IsRunning() ? nullptr : "Stop Play to edit"};
    if (!available && report && ReportError)
        ReportError(available.Reason);
    return bool(available);
}
bool SceneHierarchyPanel::AddEntity(const std::string &name)
{
    if (!m_Context || !CanEdit(true))
        return false;
    if(PrefabDocument) { auto roots=m_Context->GetChildren();return !roots.empty() && AddChild(m_Context->GetEntityByUUID(roots.front()),name); }
    try {m_HierarchyError.clear();return SetSelectedEntity(m_Context->CreateEntity(name));}
    catch(const std::exception& error){return HierarchyFailed(error.what());}
}
bool SceneHierarchyPanel::AddUI(int kind) {
    if(!m_Context||!CanEdit(true))return false;
    try {
        if(kind<0||kind>23)throw std::invalid_argument("Unknown UI creation preset");
        Entity parent=GetSelectedEntity();
        if(PrefabDocument&&!parent){auto roots=m_Context->GetChildren();if(!roots.empty())parent=m_Context->GetEntityByUUID(roots.front());}
        uint64_t dropdownGroup=0;
        if(kind==18||kind==20){
            auto ancestor=parent;for(size_t depth=0;ancestor&&depth<Scene::MaxDepth;++depth){
                if((kind==18&&ancestor.HasComponent<UITabsComponent>())||(kind==20&&ancestor.HasComponent<UIRadioGroupComponent>()&&!ancestor.HasComponent<UITabsComponent>()&&!ancestor.HasComponent<UIDropdownComponent>())){dropdownGroup=ancestor.GetUUID();break;}
                ancestor=m_Context->GetEntityByUUID(m_Context->GetRelationship(ancestor).Parent);
            }
            if(!dropdownGroup)throw std::invalid_argument(kind==18?"Select Tabs or one of its parts":"Select a selectable list or row");
            parent={};auto group=m_Context->GetEntityByUUID(dropdownGroup);
            for(auto id:m_Context->GetChildrenView(group.GetUUID())){auto child=m_Context->GetEntityByUUID(id);
                if(kind==18&&child.HasComponent<UILayoutComponent>())parent=child;
                if(kind==20&&child.HasComponent<UIScrollViewComponent>())parent=m_Context->GetEntityByUUID(child.GetComponent<UIScrollViewComponent>().Content);
            }
            if(!parent||m_Context->GetChildrenView(parent.GetUUID()).size()>=128)throw std::invalid_argument("Repair named strip/content first, or remove rows (limit 128)");
        }
        if(kind==21){
            if(parent&&parent.HasComponent<UIDropdownComponent>())parent=m_Context->GetEntityByUUID(parent.GetComponent<UIDropdownComponent>().Trigger);
            if(!parent||!parent.HasComponent<UIButtonComponent>()||!parent.HasComponent<UIRectComponent>())throw std::invalid_argument("Select a control to explain with a Tooltip");
            if(parent.HasComponent<UITooltipComponent>())throw std::invalid_argument("Control already has a Tooltip; edit its named body");
        }
        if(kind==16){auto ancestor=parent;for(size_t depth=0;ancestor&&depth<Scene::MaxDepth;++depth){if(ancestor.HasComponent<UIDropdownComponent>()){dropdownGroup=ancestor.GetUUID();break;}ancestor=m_Context->GetEntityByUUID(m_Context->GetRelationship(ancestor).Parent);}
            if(!dropdownGroup)throw std::invalid_argument("Select a Dropdown or one of its parts before adding an option");
            auto popup=m_Context->GetEntityByUUID(m_Context->GetEntityByUUID(dropdownGroup).GetComponent<UIDropdownComponent>().Popup);
            if(!popup||!popup.HasComponent<UIScrollViewComponent>())throw std::invalid_argument("Repair Dropdown popup first");parent=m_Context->GetEntityByUUID(popup.GetComponent<UIScrollViewComponent>().Content);
            if(!parent||m_Context->GetChildrenView(parent.GetUUID()).size()>=128)throw std::invalid_argument("Dropdown Content missing or option limit reached (128)");
        }
        if(kind!=0&&kind!=7&&(!parent||(!parent.HasComponent<UIRectComponent>()&&!parent.HasComponent<UICanvasComponent>()))) {
            auto canvas=m_Context->CreateEntity("UI Canvas");canvas.AddComponent<UICanvasComponent>();if(parent)m_Context->Reparent(canvas,parent,TransformPolicy::KeepLocal);parent=canvas;
        }
        if(kind!=0&&kind!=7&&parent&&!parent.HasComponent<UICanvasComponent>()&&parent.HasComponent<UILayoutComponent>()){
            const auto &layout=parent.GetComponent<UILayoutComponent>();GameUI::Validate(layout);
            if(layout.Kind==GameUI::ContainerKind::Grid){
                size_t occupied=0;for(auto id:m_Context->GetChildrenView(parent.GetUUID())){auto child=m_Context->GetEntityByUUID(id);
                    if(child&&!child.HasComponent<UICanvasComponent>()&&child.HasComponent<UIRectComponent>()){auto &r=child.GetComponent<UIRectComponent>();GameUI::Validate(r);occupied+=size_t(r.RowSpan)*r.ColumnSpan;}}
                if(occupied>=layout.Rows.size()*layout.Columns.size())throw std::invalid_argument("Grid has no free cell; add row/column tracks before creating another child");
            }
        }
        const char* names[]={"UI Canvas","UI Panel","UI Text","UI Button","UI Stack","UI Grid","UI Wrap","UI Screen","UI Checkbox","UI Radio Group","UI Progress","UI Slider","UI Vertical Slider","UI ScrollView","UI Horizontal ScrollView","UI Dropdown","Dropdown option","UI Tabs","Tab and page","UI Selectable List","List row","Tooltip body","UI Text Entry","UI Multiline Entry"};
        auto e=m_Context->CreateEntity(names[kind]);if(parent)m_Context->Reparent(e,parent,TransformPolicy::KeepLocal);
        if(kind==0||kind==7){auto &c=e.AddComponent<UICanvasComponent>();if(kind==7){c.Screen.Managed=true;c.Modal=true;}}
        else {
            auto& rect=e.AddComponent<UIRectComponent>();rect.Offset={24,24};
            if(kind==22||kind==23){
                rect.FixedSize={360,kind==23?160.f:48.f};rect.Padding=glm::vec4(10);rect.ClipChildren=true;
                e.AddComponent<SpriteRendererComponent>(glm::vec4(.06f,.09f,.14f,1));e.AddComponent<TextComponent>().TextString="";
                auto &style=e.AddComponent<UITextStyleComponent>();style.Size=20;style.Wrap=kind==23;style.Horizontal=style.Vertical=GameUI::Align::Start;
                auto &entry=e.AddComponent<UITextEntryComponent>();entry.Multiline=kind==23;entry.Placeholder=kind==23?"Write a note…":"Name";entry.MaxLines=kind==23?64:1;
                auto &button=e.AddComponent<UIButtonComponent>();button.Kind=GameUI::ControlKind::TextEntry;button.Action=kind==23?"note":"name";
            }
            if(kind==1){rect.FixedSize={360,240};e.AddComponent<SpriteRendererComponent>(glm::vec4(.08f,.12f,.18f,.95f));}
            if(kind==2||kind==3){auto& text=e.AddComponent<TextComponent>();text.TextString=kind==3?"Button":"Text";auto& style=e.AddComponent<UITextStyleComponent>();if(kind==3){style.Horizontal=GameUI::Align::Center;style.Vertical=GameUI::Align::Center;rect.Padding=glm::vec4(8);}}
            if(kind==3){e.AddComponent<SpriteRendererComponent>(glm::vec4(.15f,.25f,.35f,1));e.AddComponent<UIButtonComponent>();}
            if(kind==4){rect.FixedSize={360,240};auto &flow=e.AddComponent<UILayoutComponent>();flow.Cross=GameUI::Align::Start;rect.HeightMode=GameUI::SizeMode::Content;rect.Padding=glm::vec4(12);}
            if(kind==5){rect.FixedSize={360,240};auto &grid=e.AddComponent<UILayoutComponent>();grid.Kind=GameUI::ContainerKind::Grid;grid.Columns.assign(3,GameUI::Track{});grid.Rows.assign(2,GameUI::Track{});}
            if(kind==6){rect.FixedSize={360,240};auto &wrap=e.AddComponent<UILayoutComponent>();wrap.Kind=GameUI::ContainerKind::Wrap;wrap.Direction=GameUI::Axis::Horizontal;rect.HeightMode=GameUI::SizeMode::Content;rect.Padding=glm::vec4(12);}
            if(kind==2){rect.WidthMode=rect.HeightMode=GameUI::SizeMode::Content;}
            if(kind==3){rect.HeightMode=GameUI::SizeMode::Content;rect.MinimumSize={48,40};}
            if(parent && !parent.HasComponent<UICanvasComponent>() && parent.HasComponent<UIRectComponent>() && parent.HasComponent<UILayoutComponent>()){
                rect.Offset={};
                auto &container=parent.GetComponent<UILayoutComponent>();
                if(container.Kind==GameUI::ContainerKind::Grid || (container.Kind==GameUI::ContainerKind::Stack && container.Direction==GameUI::Axis::Vertical))rect.WidthMode=GameUI::SizeMode::Fill;
                if(container.Kind==GameUI::ContainerKind::Wrap && kind==3)rect.WidthMode=GameUI::SizeMode::Content;
            }
        }
        if(kind>=8&&kind<=21){
            auto &rect=e.GetComponent<UIRectComponent>();rect.FixedSize={280,44};
            auto option=[&](Entity control,const char *label,GameUI::ControlKind kind){
                auto &r=control.GetComponent<UIRectComponent>();r.Padding={40,6,8,6};
                control.AddComponent<SpriteRendererComponent>(glm::vec4(.12f,.19f,.27f,1));
                control.AddComponent<TextComponent>().TextString=label;
                control.AddComponent<UITextStyleComponent>().Vertical=GameUI::Align::Center;
                auto &b=control.AddComponent<UIButtonComponent>();b.Kind=kind;
            };
            if(kind==8){option(e,"Checkbox",GameUI::ControlKind::Toggle);e.GetComponent<UIButtonComponent>().Action="checked";}
            if(kind==9){
                rect.HeightMode=GameUI::SizeMode::Content;auto &flow=e.AddComponent<UILayoutComponent>();flow.Direction=GameUI::Axis::Vertical;flow.Gap=8;
                auto &g=e.AddComponent<UIRadioGroupComponent>();
                for(int i=0;i<3;++i){auto child=m_Context->CreateEntity("Option "+std::to_string(i+1));m_Context->Reparent(child,e,TransformPolicy::KeepLocal);
                    auto &r=child.AddComponent<UIRectComponent>();r.WidthMode=GameUI::SizeMode::Fill;r.FixedSize={280,44};
                    option(child,("Option "+std::to_string(i+1)).c_str(),GameUI::ControlKind::Radio);
                    child.GetComponent<UIButtonComponent>().Group=e.GetUUID();child.GetComponent<UIButtonComponent>().TabOrder=i;
                    if(i==0)g.InitialSelection=child.GetUUID();
                }
            }
            if(kind==17||kind==18||kind==19||kind==20||kind==21){
                auto part=[&](const char *name,Entity owner){auto child=m_Context->CreateEntity(name);m_Context->Reparent(child,owner,TransformPolicy::KeepLocal);child.AddComponent<UIRectComponent>();return child;};
                auto row=[&](Entity item,uint64_t group,const std::string &label){auto &r=item.GetComponent<UIRectComponent>();r.Offset={};r.WidthMode=GameUI::SizeMode::Fill;r.FixedSize.y=44;option(item,label.c_str(),GameUI::ControlKind::Radio);auto &b=item.GetComponent<UIButtonComponent>();b.Group=group;auto &t=item.GetComponent<UITextStyleComponent>();t.Size=18;t.Wrap=false;t.MaxLines=1;t.Overflow=Text::Overflow::Ellipsis;};
                auto page=[&](Entity tab,Entity group,int index){
                    auto p=part(("Page "+std::to_string(index)).c_str(),group);auto &r=p.GetComponent<UIRectComponent>();r.WidthMode=r.HeightMode=GameUI::SizeMode::Fill;r.Margin.y=52;r.Padding=glm::vec4(12);p.AddComponent<SpriteRendererComponent>(glm::vec4(.06f,.09f,.14f,1));
                    auto button=part("Page action",p);button.GetComponent<UIRectComponent>().FixedSize={240,44};option(button,"Page action",GameUI::ControlKind::Button);button.GetComponent<UIButtonComponent>().Action="page-action";auto &c=tab.AddComponent<UITabComponent>();c.Page=p.GetUUID();c.InitialFocus=button.GetUUID();
                };
                if(kind==17){
                    rect.FixedSize={600,360};auto &g=e.AddComponent<UIRadioGroupComponent>();g.Action="page";e.AddComponent<UITabsComponent>();
                    auto strip=part("Tab strip",e);auto &r=strip.GetComponent<UIRectComponent>();r.WidthMode=GameUI::SizeMode::Fill;r.FixedSize.y=44;auto &flow=strip.AddComponent<UILayoutComponent>();flow.Direction=GameUI::Axis::Horizontal;flow.Gap=6;
                    for(int i=0;i<3;++i){auto tab=part(("Tab "+std::to_string(i+1)).c_str(),strip);row(tab,e.GetUUID(),"Page "+std::to_string(i+1));tab.GetComponent<UIButtonComponent>().TabOrder=i;page(tab,e,i+1);if(!i)g.InitialSelection=tab.GetUUID();}
                }
                if(kind==18){rect.Offset={};row(e,dropdownGroup,"Page");page(e,m_Context->GetEntityByUUID(dropdownGroup),int(m_Context->GetChildrenView(parent.GetUUID()).size()));}
                if(kind==19){
                    rect.FixedSize={360,240};auto &g=e.AddComponent<UIRadioGroupComponent>();g.Action="entry";
                    auto view=part("List viewport",e);auto &r=view.GetComponent<UIRectComponent>();r.WidthMode=r.HeightMode=GameUI::SizeMode::Fill;r.Padding={8,8,24,8};view.AddComponent<SpriteRendererComponent>(glm::vec4(.05f,.07f,.10f,1));auto &v=view.AddComponent<UIScrollViewComponent>();
                    auto content=part("List rows",view);auto &cr=content.GetComponent<UIRectComponent>();cr.WidthMode=GameUI::SizeMode::Fill;cr.HeightMode=GameUI::SizeMode::Content;content.AddComponent<UILayoutComponent>().Gap=4;v.Content=content.GetUUID();
                    for(int i=0;i<8;++i){auto item=part(("Entry "+std::to_string(i+1)).c_str(),content);row(item,e.GetUUID(),"Journal entry "+std::to_string(i+1));item.GetComponent<UIButtonComponent>().TabOrder=i;if(!i)g.InitialSelection=item.GetUUID();}
                    auto track=part("List scroll track",view);auto &tr=track.GetComponent<UIRectComponent>();tr.FixedSize={8,0};tr.HeightMode=GameUI::SizeMode::Fill;tr.Anchor.x=1;tr.Offset.x=4;track.AddComponent<SpriteRendererComponent>(glm::vec4(.2f,.25f,.3f,1));auto thumb=part("List scroll thumb",view);thumb.GetComponent<UIRectComponent>().FixedSize={8,16};thumb.AddComponent<SpriteRendererComponent>(glm::vec4(.7f,.8f,.85f,1));v.VerticalTrack=track.GetUUID();v.VerticalThumb=thumb.GetUUID();
                }
                if(kind==20){rect.Offset={};row(e,dropdownGroup,"Journal entry");}
                if(kind==21){
                    rect.Offset={};rect.FixedSize.x=280;rect.HeightMode=GameUI::SizeMode::Content;rect.Padding=glm::vec4(8);
                    e.AddComponent<TextComponent>().TextString="Explain this control's purpose or consequences.";auto &style=e.AddComponent<UITextStyleComponent>();style.Size=18;style.MaxLines=8;style.Overflow=Text::Overflow::Ellipsis;e.AddComponent<SpriteRendererComponent>(glm::vec4(.025f,.035f,.05f,1));parent.AddComponent<UITooltipComponent>().Body=e.GetUUID();
                }
            }
            if(kind==15||kind==16){
                auto radio=[&](Entity item,uint64_t group,const std::string &label){auto &r=item.GetComponent<UIRectComponent>();r.WidthMode=GameUI::SizeMode::Fill;r.FixedSize.y=44;r.Padding={32,6,6,6};item.AddComponent<SpriteRendererComponent>(glm::vec4(.12f,.19f,.27f,1));item.AddComponent<TextComponent>().TextString=label;auto &t=item.AddComponent<UITextStyleComponent>();t.Size=20;t.Vertical=GameUI::Align::Center;t.Wrap=false;t.Overflow=Text::Overflow::Ellipsis;t.MaxLines=1;auto &b=item.AddComponent<UIButtonComponent>();b.Kind=GameUI::ControlKind::Radio;b.Group=group;};
                if(kind==16){rect.Offset={};radio(e,dropdownGroup,"Option");}
                else{
                    auto &g=e.AddComponent<UIRadioGroupComponent>();g.Action="choice";auto &d=e.AddComponent<UIDropdownComponent>();
                    auto part=[&](const char *name,Entity owner){auto child=m_Context->CreateEntity(name);m_Context->Reparent(child,owner,TransformPolicy::KeepLocal);child.AddComponent<UIRectComponent>();return child;};
                    auto trigger=part("Dropdown trigger",e);auto &tr=trigger.GetComponent<UIRectComponent>();tr.WidthMode=tr.HeightMode=GameUI::SizeMode::Fill;tr.Padding={8,6,32,6};trigger.AddComponent<UIButtonComponent>();trigger.AddComponent<SpriteRendererComponent>(glm::vec4(.12f,.19f,.27f,1));trigger.AddComponent<TextComponent>().TextString="Computed selection";auto &ts=trigger.AddComponent<UITextStyleComponent>();ts.Size=20;ts.Vertical=GameUI::Align::Center;ts.Wrap=false;ts.Overflow=Text::Overflow::Ellipsis;ts.MaxLines=1;
                    auto cue=part("Dropdown arrow",trigger);auto &ar=cue.GetComponent<UIRectComponent>();ar.FixedSize={20,32};ar.Anchor={1,.5f};ar.Pivot={1,.5f};ar.Offset={28,0};cue.AddComponent<TextComponent>().TextString="v";auto &as=cue.AddComponent<UITextStyleComponent>();as.Size=18;as.Horizontal=as.Vertical=GameUI::Align::Center;
                    auto popup=part("Dropdown popup",e);auto &pr=popup.GetComponent<UIRectComponent>();pr.FixedSize={280,240};pr.Padding={4,4,20,4};popup.AddComponent<SpriteRendererComponent>(glm::vec4(.045f,.065f,.10f,1));auto &view=popup.AddComponent<UIScrollViewComponent>();
                    auto content=part("Dropdown options",popup);auto &cr=content.GetComponent<UIRectComponent>();cr.WidthMode=GameUI::SizeMode::Fill;cr.HeightMode=GameUI::SizeMode::Content;auto &flow=content.AddComponent<UILayoutComponent>();flow.Gap=2;view.Content=content.GetUUID();
                    for(int i=0;i<8;++i){auto item=part(("Option "+std::to_string(i+1)).c_str(),content);radio(item,e.GetUUID(),"Option "+std::to_string(i+1));if(i==0)g.InitialSelection=item.GetUUID();}
                    auto track=part("Dropdown scroll track",popup);auto &rt=track.GetComponent<UIRectComponent>();rt.FixedSize={8,0};rt.HeightMode=GameUI::SizeMode::Fill;rt.Anchor.x=1;rt.Offset.x=4;track.AddComponent<SpriteRendererComponent>(glm::vec4(.20f,.25f,.30f,1));
                    auto thumb=part("Dropdown scroll thumb",popup);thumb.GetComponent<UIRectComponent>().FixedSize={8,16};thumb.AddComponent<SpriteRendererComponent>(glm::vec4(.70f,.80f,.85f,1));view.VerticalTrack=track.GetUUID();view.VerticalThumb=thumb.GetUUID();d.Trigger=trigger.GetUUID();d.Popup=popup.GetUUID();
                }
            }
            if(kind==13||kind==14){
                bool horizontal=kind==14;rect.FixedSize={360,240};rect.Padding=horizontal?glm::vec4(8,8,8,24):glm::vec4(8,8,24,8);
                e.AddComponent<SpriteRendererComponent>(glm::vec4(.06f,.09f,.14f,1));auto &v=e.AddComponent<UIScrollViewComponent>();v.Horizontal=horizontal;v.Vertical=!horizontal;
                auto content=m_Context->CreateEntity("Scroll content");m_Context->Reparent(content,e,TransformPolicy::KeepLocal);auto &r=content.AddComponent<UIRectComponent>();r.WidthMode=horizontal?GameUI::SizeMode::Content:GameUI::SizeMode::Fill;r.HeightMode=horizontal?GameUI::SizeMode::Fill:GameUI::SizeMode::Content;
                auto &flow=content.AddComponent<UILayoutComponent>();flow.Direction=horizontal?GameUI::Axis::Horizontal:GameUI::Axis::Vertical;flow.Gap=8;v.Content=content.GetUUID();
                for(int i=0;i<8;++i){auto item=m_Context->CreateEntity("Scroll item "+std::to_string(i+1));m_Context->Reparent(item,content,TransformPolicy::KeepLocal);auto &ir=item.AddComponent<UIRectComponent>();ir.FixedSize={160,40};if(horizontal)ir.HeightMode=GameUI::SizeMode::Fill;else ir.WidthMode=GameUI::SizeMode::Fill;
                    item.AddComponent<SpriteRendererComponent>(glm::vec4(.13f,.21f,.30f,1));item.AddComponent<TextComponent>().TextString="Item "+std::to_string(i+1);auto &style=item.AddComponent<UITextStyleComponent>();style.Size=18;style.Vertical=GameUI::Align::Center;ir.Padding=glm::vec4(6);auto &b=item.AddComponent<UIButtonComponent>();b.TabOrder=i;b.Action="item-"+std::to_string(i+1);}
                auto track=m_Context->CreateEntity("Scroll track");m_Context->Reparent(track,e,TransformPolicy::KeepLocal);auto &tr=track.AddComponent<UIRectComponent>();tr.FixedSize=horizontal?glm::vec2(0,10):glm::vec2(10,0);
                if(horizontal){tr.WidthMode=GameUI::SizeMode::Fill;tr.Anchor.y=1;tr.Offset.y=4;}else{tr.HeightMode=GameUI::SizeMode::Fill;tr.Anchor.x=1;tr.Offset.x=4;}
                track.AddComponent<SpriteRendererComponent>(glm::vec4(.20f,.25f,.30f,1));
                auto thumb=m_Context->CreateEntity("Scroll thumb");m_Context->Reparent(thumb,e,TransformPolicy::KeepLocal);thumb.AddComponent<UIRectComponent>().FixedSize={16,16};thumb.AddComponent<SpriteRendererComponent>(glm::vec4(.70f,.80f,.85f,1));
                if(horizontal){v.HorizontalTrack=track.GetUUID();v.HorizontalThumb=thumb.GetUUID();}else{v.VerticalTrack=track.GetUUID();v.VerticalThumb=thumb.GetUUID();}
            }
            if(kind==11||kind==12){
                bool vertical=kind==12;rect.FixedSize=vertical?glm::vec2(64,240):glm::vec2(280,48);rect.Padding=glm::vec4(8);
                e.AddComponent<SpriteRendererComponent>(glm::vec4(.07f,.10f,.15f,1));
                auto &b=e.AddComponent<UIButtonComponent>();b.Kind=GameUI::ControlKind::Slider;b.Action=vertical?"level":"volume";
                auto &v=e.AddComponent<UISliderComponent>();v.InitialValue=.5f;v.Direction=vertical?GameUI::FillDirection::Up:GameUI::FillDirection::Right;
                auto part=[&](const char *name,glm::vec4 color,bool thumb){auto child=m_Context->CreateEntity(name);m_Context->Reparent(child,e,TransformPolicy::KeepLocal);
                    auto &r=child.AddComponent<UIRectComponent>();r.Anchor=r.Pivot=glm::vec2(.5f);
                    r.FixedSize=thumb?glm::vec2(20,28):(vertical?glm::vec2(8,200):glm::vec2(240,8));
                    if(vertical&&thumb)r.FixedSize={28,20};
                    if(!thumb){if(vertical){r.HeightMode=GameUI::SizeMode::Fill;r.Anchor.y=r.Pivot.y=0;}else{r.WidthMode=GameUI::SizeMode::Fill;r.Anchor.x=r.Pivot.x=0;}}
                    child.AddComponent<SpriteRendererComponent>(color);return uint64_t(child.GetUUID());};
                v.Track=part("Slider track",{.22f,.27f,.33f,1},false);
                v.Fill=part("Slider fill",{.3f,.65f,.8f,1},false);
                v.Thumb=part("Slider thumb",{.95f,.95f,.85f,1},true);
            }
            if(kind==10){
                rect.FixedSize={280,24};rect.Padding=glm::vec4(3);e.AddComponent<SpriteRendererComponent>(glm::vec4(.08f,.12f,.18f,1));
                auto fill=m_Context->CreateEntity("Progress fill");m_Context->Reparent(fill,e,TransformPolicy::KeepLocal);
                auto &r=fill.AddComponent<UIRectComponent>();r.WidthMode=r.HeightMode=GameUI::SizeMode::Fill;r.FixedSize={};
                fill.AddComponent<SpriteRendererComponent>(glm::vec4(.35f,.75f,.65f,1));auto &p=e.AddComponent<UIProgressComponent>();p.Fill=fill.GetUUID();p.InitialValue=.5f;
            }
        }
        return SetSelectedEntity(e);
    }catch(const std::exception& error){return HierarchyFailed(error.what());}
}
void SceneHierarchyPanel::UIAddMenu(){
    if(ImGui::BeginMenu("Runtime UI")) {
        const char* names[]={"Canvas","Panel","Text","Button","Stack","Grid","Wrap","Screen","Checkbox / Toggle","Radio Group","Progress","Slider","Vertical Slider","ScrollView","Horizontal ScrollView","Dropdown","Dropdown option","Tabs","Tab and page","Selectable List","List row","Tooltip for selected control","Text Entry","Multiline Text Entry"};
        for(size_t i=0;i<std::size(names);++i)if(ImGui::MenuItem(names[i]))AddUI(int(i));
        ImGui::EndMenu();
    }
}
bool SceneHierarchyPanel::DeleteSelected(DestroyPolicy policy,TransformPolicy mode) {
    auto entity=GetSelectedEntity();if(!entity)return false;
    return ConfirmDelete(m_Context->GetIdentity(),entity.GetUUID(),m_Context->GetSubtree(entity).size(),policy,mode);
}
void SceneHierarchyPanel::DrawAssetProperties(Entity entity)
{
    if (entity != m_SelectionContext)
    {
        FinishUIEdit();
        m_Pickers.clear();m_UIStatePickers={};m_UIFontInitialized=false;
        m_SelectionContext = entity;
    }
    DrawComponents(entity);
}
void SceneHierarchyPanel::OnImGuiRender()
{
    m_Focused=false;
    const auto availability = Availability ? Availability(EditOperation) : ActionAvailability{};
    if(HierarchyVisible) {
    ImGui::Begin("Scene Hierarchy",&HierarchyVisible);
    m_Focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
    DrawHierarchy();
    ImGui::End();
    }
    if(!PropertiesVisible){FinishUIEdit();return;}
    ImGui::Begin("Properties",&PropertiesVisible);
    m_Focused |= ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
    if (auto entity = GetSelectedEntity())
    {
        ImGui::TextWrapped("Scene Entity: %s", entity.GetName().c_str());
        ImGui::BeginDisabled(!CanEdit());
        if (CreatePrefab && ImGui::Button("Create Prefab..."))
            CreatePrefab(entity);
        ImGui::EndDisabled();
        PropertyUI::Help(availability.Reason);
        DrawComponents(entity);
    }
    else {
        FinishUIEdit();
        ImGui::TextWrapped("Select a scene entity here or in the viewport. Asset editing retains that "
                           "assignment target.");
    }
    ImGui::End();
}

bool SceneHierarchyPanel::SetSelectedEntity(Entity entity)
{
    if(m_SelectionContext != entity){FinishUIEdit();}
    if (!entity.BelongsTo(m_Context.get()))
    {
        m_SelectionContext = {};
        return false;
    }
    if (!m_Context || !entity || !m_Context->IsEntityValid(entity.GetUUID()))
    {
        m_SelectionContext = {};
        return false;
    }
    if (m_SelectionContext != entity) {m_Pickers.clear();m_UIStatePickers={};m_UIFontInitialized=false;m_HierarchyError.clear();}
    m_SelectionContext = entity;
    RevealSelected();
    return true;
}

bool SceneHierarchyPanel::AssignSpriteTexture(SpriteRendererComponent &component,
                                              const std::filesystem::path &path)
{
    try
    {
        auto texture = Texture2D::Create(path.generic_u8string());
        component.SetTexture(std::move(texture));
        return true;
    }
    catch (const std::runtime_error &error)
    {
        HZ_ERROR("Texture assignment '{}': {}", path.generic_u8string(), error.what());
        return false;
    }
}

static bool SamePhysics(const Rigidbody2DComponent& a,const Rigidbody2DComponent& b){return a.Type==b.Type && a.FixedRotation==b.FixedRotation && a.GravityScale==b.GravityScale;}
static bool SamePhysics(const BoxCollider2DComponent& a,const BoxCollider2DComponent& b){return a.Offset==b.Offset && a.Size==b.Size && a.Density==b.Density && a.Friction==b.Friction && a.Restitution==b.Restitution && a.RestitutionThreshold==b.RestitutionThreshold;}
static bool SamePhysics(const CircleCollider2DComponent& a,const CircleCollider2DComponent& b){return a.Offset==b.Offset && a.Radius==b.Radius && a.Density==b.Density && a.Friction==b.Friction && a.Restitution==b.Restitution && a.RestitutionThreshold==b.RestitutionThreshold;}
template <typename T, typename UIFunction>
static void DrawComponent(const std::string &name, Entity entity, std::map<std::string,bool>& sections,std::set<std::string>& restore,UIFunction uiFunction, const std::function<void(const std::string&)>& report = {}, const std::function<void()> &beforeRemove = {})
{
    const ImGuiTreeNodeFlags treeNodeFlags =
        ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanAvailWidth |
        ImGuiTreeNodeFlags_AllowItemOverlap | ImGuiTreeNodeFlags_FramePadding;
    if (entity.HasComponent<T>())
    {
        ImGui::PushID(name.c_str());
        auto &component = entity.GetComponent<T>();
        ImVec2 contentRegionAvailable = ImGui::GetContentRegionAvail();

        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{4, 4});
        float lineHeight = GImGui->Font->FontSize + GImGui->Style.FramePadding.y * 2.0f;
        ImGui::Separator();
        if(restore.erase(name))ImGui::SetNextItemOpen(sections[name],ImGuiCond_Always);
        bool open = ImGui::TreeNodeEx("##component", treeNodeFlags, "%s", name.c_str());
        sections[name]=open;
        ImGui::PopStyleVar();
        ImGui::SameLine(contentRegionAvailable.x - lineHeight * 0.5f);
        if (ImGui::Button("+", ImVec2{lineHeight, lineHeight}))
        {
            ImGui::OpenPopup("ComponentSettings");
        }

        bool removeComponent = false;
        if (ImGui::BeginPopup("ComponentSettings"))
        {
            if (ImGui::MenuItem("Remove component", nullptr, false,
                                !std::is_same_v<T, TransformComponent>))
                removeComponent = true;

            ImGui::EndPopup();
        }

        if (open)
        {
            if constexpr(std::is_same_v<T,Rigidbody2DComponent> || std::is_same_v<T,BoxCollider2DComponent> || std::is_same_v<T,CircleCollider2DComponent>) {
                auto proposed=component;uiFunction(proposed);
                if(!SamePhysics(component,proposed))try{entity.AddOrReplaceComponent<T>(proposed);}catch(const std::exception& error){if(report)report(error.what());}
            } else uiFunction(component);
            ImGui::TreePop();
        }

        if (removeComponent)
        {
            if(beforeRemove)beforeRemove();
            entity.RemoveComponent<T>();
        }
        ImGui::PopID();
    }
}


void SceneHierarchyPanel::FinishUIEdit(bool cancel)
{
    if(m_Context)m_UIInspectorEdit.Finish(*m_Context,m_History,cancel);
}
template<class T,class F>
void SceneHierarchyPanel::DrawUIComponent(const std::string &name,Entity entity,
    std::map<std::string,bool> &sections,std::set<std::string> &restore,F ui,
    std::function<void(const std::string&)> report)
{
    if(!m_Context || !entity.BelongsTo(m_Context.get()) || !entity || !m_Context->IsEntityValid(entity.GetUUID()))return;
    if(!std::is_same_v<T,TextComponent> && !entity.HasComponent<UIRectComponent>()&&!entity.HasComponent<UICanvasComponent>()){
        DrawComponent<T>(name,entity,sections,restore,ui,report);return;
    }
    DrawComponent<T>(name,entity,sections,restore,[&](auto &component){
        try {
            const auto document=m_Context;
            const auto target=entity.GetUUID();
            auto before=UIEditValues::Capture(entity),proposed=before;
            UIEditCommand guard(*document,target,name,before);
            auto draft=component;
            PropertyUI::EditActivity activity;
            {PropertyUI::ActivityScope observation(activity);ui(draft);}
            if(m_Context!=document || GetSelectedEntity()!=entity)
                throw std::runtime_error("UI document or selection changed while editing; retry in the current Inspector");
            guard.Check(*document);
            proposed.Set(draft);
            const bool changed=!before.Same(proposed);
            if(!m_UIInspectorEdit.Edit(*document,m_History,target,name,before,
                std::move(proposed),activity,ImGui::GetActiveID()))m_HierarchyError=m_UIInspectorEdit.Error();
            else if(changed)m_HierarchyError.clear();
        }catch(const std::exception &error){m_HierarchyError=error.what();if(report)report(error.what());}
    },report,[this]{FinishUIEdit();});
}
void SceneHierarchyPanel::DrawComponents(Entity entity)
{
    if (!entity || !m_Context || !entity.BelongsTo(m_Context.get()) || !m_Context->IsEntityValid(entity.GetUUID()))return;
    const auto document=m_Context; // keep the observed scene alive through modal property callbacks
    m_UIInspectorEdit.Poll(*m_Context,m_History,entity.GetUUID(),CanEdit(),!ImGui::GetIO().AppFocusLost,ImGui::IsKeyPressed(ImGuiKey_Escape,false),ImGui::GetActiveID());
    if(m_SectionEntity!=uint64_t(entity.GetUUID())){m_Sections.clear();m_RestoreSections.clear();m_SectionEntity=entity.GetUUID();}
    ImGui::PushID(std::to_string(m_Context->GetIdentity()).c_str());
    ImGui::PushID(std::to_string(uint64_t(entity.GetUUID())).c_str());
    const bool editable = CanEdit();
    if (!editable)
        ImGui::TextWrapped("%s", Availability ? Availability(EditOperation).Reason
                                              : "Stop Play to edit authored content");
    ImGui::BeginDisabled(!editable);
    if (entity.HasComponent<TagComponent>())
        PropertyUI::Text("tag", "Name", entity.GetComponent<TagComponent>().Tag);

    if (ImGui::Button("Add Component"))
        ImGui::OpenPopup("AddComponent");

    if (ImGui::BeginPopup("AddComponent"))
    {
        DisplayAddComponentEntry<CameraComponent>("Camera");
        DisplayAddComponentEntry<ScriptComponent>("Script");
        DisplayAddComponentEntry<SpriteRendererComponent>("Sprite Renderer");
        if (entity.HasComponent<SpriteRendererComponent>())
            DisplayAddComponentEntry<SpriteAnimationComponent>("Sprite Animation");
        DisplayAddComponentEntry<CircleRendererComponent>("Circle Renderer");
        DisplayAddComponentEntry<Rigidbody2DComponent>("Rigidbody 2D");
        DisplayAddComponentEntry<BoxCollider2DComponent>("Box Collider 2D");
        DisplayAddComponentEntry<CircleCollider2DComponent>("Circle Collider 2D");
        DisplayAddComponentEntry<TextComponent>("Text Component");
        DisplayAddComponentEntry<AudioSourceComponent>("Audio Source");
        DisplayAddComponentEntry<UICanvasComponent>("UI Canvas");
        DisplayAddComponentEntry<UIRectComponent>("UI Rect");
        if(entity.HasComponent<UIRectComponent>()){
            DisplayAddComponentEntry<UILayoutComponent>("UI Container");
            DisplayAddComponentEntry<UIButtonComponent>("UI Button");
            DisplayAddComponentEntry<UIRadioGroupComponent>("UI Radio Group");
            DisplayAddComponentEntry<UIProgressComponent>("UI Progress");
            DisplayAddComponentEntry<UISliderComponent>("UI Slider");
            DisplayAddComponentEntry<UIScrollViewComponent>("UI ScrollView");
            DisplayAddComponentEntry<UITextEntryComponent>("UI Text Entry");
            DisplayAddComponentEntry<UITabsComponent>("UI Tabs");
            DisplayAddComponentEntry<UITabComponent>("UI Tab");
            DisplayAddComponentEntry<UITooltipComponent>("UI Tooltip");
            DisplayAddComponentEntry<UIDropdownComponent>("UI Dropdown");
            if(entity.HasComponent<TextComponent>())DisplayAddComponentEntry<UITextStyleComponent>("UI Text Style");
            if(entity.HasComponent<SpriteRendererComponent>())DisplayAddComponentEntry<UINineSliceComponent>("UI Nine Slice");
        }

        ImGui::EndPopup();
    }

    DrawComponent<TransformComponent>(
        "Transform",entity,m_Sections,m_RestoreSections,[this,entity](auto& component)mutable {
            const TransformComponent defaults;auto proposed=component;bool changed=false;
            changed|=bool(PropertyUI::Vector("translation","Position",glm::value_ptr(proposed.Translation),3,.1f,glm::value_ptr(defaults.Translation)));
            auto rotation=glm::degrees(proposed.Rotation);
            if(PropertyUI::Vector("rotation","Rotation (deg)",glm::value_ptr(rotation),3,.5f,glm::value_ptr(defaults.Rotation))){proposed.Rotation=glm::radians(rotation);changed=true;}
            changed|=bool(PropertyUI::Vector("scale","Scale",glm::value_ptr(proposed.Scale),3,.1f,glm::value_ptr(defaults.Scale)));
            if(changed && CanEdit(true))try{m_Context->SetLocalTransform(entity,proposed);m_HierarchyError.clear();}catch(const std::exception& e){HierarchyFailed(e.what());}
        });

    // Named, document-local choices; no cross-document drag payloads or stored pointers.
    auto targetPicker=[&](const char *key,const char *label,uint64_t &id,std::function<bool(Entity)> allowed){
        auto target=m_Context->GetEntityByUUID(id);auto current=!id?std::string("None / automatic"):target?target.GetName():"Missing #"+std::to_string(id);
        PropertyUI::Row row(key,label);bool changed=false;
        if(ImGui::BeginCombo("##value",current.c_str())){
            if(ImGui::Selectable("None / automatic",!id)){id=0;changed=true;}
            for(auto handle:m_Context->GetAllEntitiesWith<IDComponent>()){
                Entity candidate(handle,m_Context.get());if(!m_Context->IsEntityValid(candidate.GetUUID())||!allowed(candidate))continue;
                auto value=uint64_t(candidate.GetUUID());auto name=candidate.GetName()+"##"+std::to_string(value);
                if(ImGui::Selectable(name.c_str(),value==id)){id=value;changed=true;}
            }ImGui::EndCombo();
        }row.Result(changed,true);
    };
    auto visualTarget=[&](Entity candidate){return m_Context->GetRelationship(candidate).Parent==entity.GetUUID()&&candidate.HasComponent<SpriteRendererComponent>()&&candidate.HasComponent<UIRectComponent>()&&!candidate.HasComponent<UIButtonComponent>()&&!candidate.HasComponent<TextComponent>()&&!candidate.HasComponent<UIScrollViewComponent>()&&!candidate.HasComponent<UISliderComponent>()&&!candidate.HasComponent<UIProgressComponent>()&&!candidate.HasComponent<UIRadioGroupComponent>()&&!candidate.HasComponent<UICanvasComponent>()&&m_Context->GetChildrenView(candidate.GetUUID()).empty();};
    auto ownerTarget=[](Entity candidate){return candidate.HasComponent<ScriptComponent>()||candidate.HasComponent<NativeScriptComponent>();};
    DrawUIComponent<UICanvasComponent>("UI Canvas",entity,m_Sections,m_RestoreSections,[this,entity](auto& c)mutable{
        PropertyUI::Vector("reference","Reference pixels",glm::value_ptr(c.ReferenceSize),2,1,nullptr,"%.0f",{},1,8192);
        PropertyUI::Vector("insets","Safe L/T/R/B (px)",glm::value_ptr(c.SafeInsets),4,1,nullptr,"%.0f",{},0,4096);
        PropertyUI::DragInt("order","Sort order",c.SortOrder);
        PropertyUI::Checkbox("visible","Visible",c.Visible);PropertyUI::Checkbox("enabled","Enabled",c.Enabled);PropertyUI::Checkbox("modal","Block gameplay",c.Modal);
        PropertyUI::Help("Uniform reference scale; extra aspect space extends logical space. Ordinary canvases use sort order; managed screens draw above them in open order.");
        PropertyUI::Checkbox("screen","Managed screen",c.Screen.Managed);
        if(c.Screen.Managed){
            PropertyUI::Checkbox("initial-open","Initially open",c.Screen.InitiallyOpen);
            int policy=int(c.Screen.Cancel);if(PropertyUI::Combo("cancel-policy","Cancel",policy,"Ignore\0Close screen\0Notify owner\0"))c.Screen.Cancel=GameUI::ScreenCancel(policy);
            auto canvasOf=[&](Entity e){for(size_t depth=0;e&&depth<Scene::MaxDepth;++depth){if(e.HasComponent<UICanvasComponent>())return uint64_t(e.GetUUID());e=m_Context->GetEntityByUUID(m_Context->GetRelationship(e).Parent);}return uint64_t(0);};
            auto picker=[&](const char* key,const char* label,uint64_t &id,int kind){
                auto target=m_Context->GetEntityByUUID(id);std::string current=!id?"Automatic / none":target?target.GetName():"Missing target #"+std::to_string(id);
                PropertyUI::Row row(key,label);bool changed=false;
                if(ImGui::BeginCombo("##value",current.c_str())){
                    if(ImGui::Selectable("Automatic / none",!id)){id=0;changed=true;}
                    for(auto handle:m_Context->GetAllEntitiesWith<IDComponent>()){
                        Entity candidate(handle,m_Context.get());if(!m_Context->IsEntityValid(candidate.GetUUID()))continue;
                        bool valid=kind==0?(candidate.HasComponent<UIButtonComponent>()&&canvasOf(candidate)==uint64_t(entity.GetUUID())):
                            kind==1?(candidate.HasComponent<UIButtonComponent>()&&canvasOf(candidate)!=uint64_t(entity.GetUUID())):(candidate.HasComponent<ScriptComponent>()||candidate.HasComponent<NativeScriptComponent>());
                        if(!valid)continue;auto value=uint64_t(candidate.GetUUID());auto name=candidate.GetName()+"##"+std::to_string(value);
                        if(ImGui::Selectable(name.c_str(),id==value)){id=value;changed=true;}
                    }ImGui::EndCombo();
                }row.Result(changed,true);
            };
            picker("event-owner","Event owner",c.Screen.EventOwner,2);
            if(!c.Screen.EventOwner)PropertyUI::Validation("Assign one script owner for button events. Built-in Close works without an owner.");
            picker("initial-focus","Initial focus",c.Screen.InitialFocus,0);
            if(ImGui::TreeNode("Focus and Cancel details")){
                picker("default-action","Default action",c.Screen.DefaultAction,0);
                if(c.Screen.Cancel==GameUI::ScreenCancel::NotifyOwner)picker("cancel-target","Cancel context",c.Screen.CancelTarget,0);
                picker("return-focus","Return fallback",c.Screen.ReturnFocus,1);
                PropertyUI::Help("Initial focus defaults to the first eligible button. Default action is used only without eligible focus. Cancel context identifies a control in the semantic Cancel event; it never clicks it. Return fallback is tried after remembered underlying focus.");
                ImGui::TreePop();
            }
            PropertyUI::Help("Open/Close/ReplaceTop are queued runtime operations. Initially open is authored; Play stack/focus/events never modify this document.");
        }
    });
    DrawUIComponent<UIRectComponent>("UI Rect",entity,m_Sections,m_RestoreSections,[this,entity](auto& c)mutable{
        auto parent=m_Context->GetEntityByUUID(m_Context->GetRelationship(entity).Parent);
        bool derivedPosition=false,scrollContent=false;
        if(parent&&parent.HasComponent<UITooltipComponent>()&&parent.GetComponent<UITooltipComponent>().Body==uint64_t(entity.GetUUID())){PropertyUI::DragFloat("tooltip-width","Width",c.FixedSize.x,1,1,4096);PropertyUI::Vector("padding","Padding L/T/R/B",glm::value_ptr(c.Padding),4,1,nullptr,"%.0f",{},0,4096);PropertyUI::Help("Tooltip owns placement and Content height. Edit text/style on this leaf; preview from the source's Tooltip section.");return;}
        if(parent&&parent.HasComponent<UIDropdownComponent>()&&parent.GetComponent<UIDropdownComponent>().Popup==uint64_t(entity.GetUUID())){PropertyUI::Help("Dropdown owns popup placement and size on the invoking surface. Edit its minimum width/maximum height; padding reserves scrollbar gutters.");PropertyUI::Vector("padding","Padding L/T/R/B",glm::value_ptr(c.Padding),4,1,nullptr,"%.0f",{},0,4096);return;}

        if(parent&&parent.HasComponent<UIScrollViewComponent>()){
            auto &v=parent.GetComponent<UIScrollViewComponent>();scrollContent=derivedPosition=v.Content==uint64_t(entity.GetUUID());
            if(v.HorizontalThumb==uint64_t(entity.GetUUID())||v.VerticalThumb==uint64_t(entity.GetUUID())){PropertyUI::Help("ScrollView computes thumb size and position. Edit track bounds or Minimum thumb on its viewport.");return;}
            if(derivedPosition)PropertyUI::Help("ScrollView owns content origin. Fixed/Content sizes define its extent; Fill is valid only on non-scroll axes. Preview offsets never alter this Rect.");
        }
        if(parent&&parent.HasComponent<UISliderComponent>()){
            auto &s=parent.GetComponent<UISliderComponent>();derivedPosition=s.Thumb==uint64_t(entity.GetUUID());
            if(s.Fill==uint64_t(entity.GetUUID())){PropertyUI::Help("Slider computes this fill from its track and live value. Edit the track bounds or the slider range; this Rect is a decorative leaf.");return;}
            if(derivedPosition)PropertyUI::Help("Slider computes thumb position. Size remains authored; edit track bounds to change travel.");
        }
        bool flowOwned=parent&&!parent.HasComponent<UICanvasComponent>()&&parent.HasComponent<UIRectComponent>()&&parent.HasComponent<UILayoutComponent>();
        {
            auto resolved=m_Context->GetUISnapshot().Find(entity.GetUUID());
            for(int axis=0;axis<2;++axis){ImGui::PushID(axis);
                auto &mode=axis?c.HeightMode:c.WidthMode;int value=int(mode);
                if(PropertyUI::Combo("sizing",axis?"Height":"Width",value,"Fixed\0Fill\0Content\0",
                    {"Fixed authors a border size. Fill owns parent space/position; free Fill clears this axis's anchor, offset and pivot. Content measures visuals/children."})){
                    mode=GameUI::SizeMode(value);
                    if(mode==GameUI::SizeMode::Fill && !flowOwned)c.Anchor[axis]=c.Offset[axis]=c.Pivot[axis]=0;
                    if(mode==GameUI::SizeMode::Fixed && resolved&&resolved->LayoutResolved)c.FixedSize[axis]=resolved->Bounds.Size[axis];
                }
                if(mode==GameUI::SizeMode::Fixed)PropertyUI::DragFloat("fixed","Fixed size (px)",c.FixedSize[axis],1,0,FLT_MAX,"%.1f");
                else {char value[48];if(resolved&&resolved->LayoutResolved)std::snprintf(value,sizeof(value),"%.1f px",resolved->Bounds.Size[axis]);else std::snprintf(value,sizeof(value),"Unresolved");PropertyUI::ReadOnly("computed","Computed size",value);}
                PropertyUI::DragFloat("min","Minimum (px)",c.MinimumSize[axis],1,0,FLT_MAX,"%.1f");
                bool has=c.HasMaximum[axis];
                if(PropertyUI::Checkbox("bounded","Maximum enabled",has)){c.HasMaximum[axis]=has;if(has)c.MaximumSize[axis]=std::max(c.MinimumSize[axis],resolved&&resolved->LayoutResolved?resolved->Bounds.Size[axis]:c.FixedSize[axis]);}
                if(has)PropertyUI::DragFloat("max","Maximum (px)",c.MaximumSize[axis],1,0,FLT_MAX,"%.1f");
                bool owned=derivedPosition||flowOwned||mode==GameUI::SizeMode::Fill;
                if(!owned){
                PropertyUI::DragFloat("anchor","Anchor",c.Anchor[axis],.01f,0,1,"%.2f");
                PropertyUI::DragFloat("offset","Offset (px)",c.Offset[axis],1,-FLT_MAX,FLT_MAX,"%.1f");
                PropertyUI::DragFloat("pivot","Pivot",c.Pivot[axis],.01f,0,1,"%.2f");}
                if(owned)PropertyUI::Help(derivedPosition?"The composing control owns position; edit its sizing/parent viewport.":flowOwned?"The parent container owns position. Set child sizing/margins; positions are computed.":"Fill owns this axis's position and size. Margins set its insets; maximum can leave spare space.");
                if(resolved&&resolved->LayoutResolved){if(resolved->Available[axis]<0)ImGui::TextDisabled("Available unbounded | measured %.1f | arranged %.1f",resolved->Measured[axis],resolved->Bounds.Size[axis]);else ImGui::TextDisabled("Available %.1f | measured %.1f | arranged %.1f",resolved->Available[axis],resolved->Measured[axis],resolved->Bounds.Size[axis]);}
                ImGui::PopID();
            }
        }
        if(!derivedPosition && !flowOwned && c.WidthMode!=GameUI::SizeMode::Fill && c.HeightMode!=GameUI::SizeMode::Fill){
            int preset=0;if(PropertyUI::Combo("anchor-preset","Anchor preset",preset,"Custom\0Top left\0Top center\0Top right\0Center\0Bottom left\0Bottom center\0Bottom right\0")){
                const glm::vec2 anchors[]={{0,0},{0,0},{.5f,0},{1,0},{.5f,.5f},{0,1},{.5f,1},{1,1}};
                if(preset){c.Anchor=c.Pivot=anchors[preset];c.Offset={};}}
        }
        if(flowOwned && parent.GetComponent<UILayoutComponent>().Kind==GameUI::ContainerKind::Grid){
            bool automatic=c.GridRow<0;
            if(PropertyUI::Checkbox("auto-cell","Automatic cell",automatic)){
                auto resolved=m_Context->GetUISnapshot().Find(entity.GetUUID());
                c.GridRow=automatic?-1:resolved&&resolved->GridRow>=0?resolved->GridRow:0;
                c.GridColumn=automatic?-1:resolved&&resolved->GridColumn>=0?resolved->GridColumn:0;
            }
            if(!automatic){
                const auto &grid=parent.GetComponent<UILayoutComponent>();char current[48];std::snprintf(current,sizeof(current),"Row %d / Column %d",c.GridRow,c.GridColumn);
                PropertyUI::Row row("cell","Cell",{"Choose both coordinates atomically. Occupied/out-of-range spans reject without changing the document."});bool changed=false;
                if(ImGui::BeginCombo("##value",current)){
                    ImGuiListClipper clipper;clipper.Begin(int(grid.Rows.size()*grid.Columns.size()));
                    while(clipper.Step())for(int cell=clipper.DisplayStart;cell<clipper.DisplayEnd;++cell){int r=cell/int(grid.Columns.size()),col=cell%int(grid.Columns.size());char label[48];std::snprintf(label,sizeof(label),"Row %d / Column %d",r,col);
                        if(ImGui::Selectable(label,r==c.GridRow&&col==c.GridColumn)){c.GridRow=r;c.GridColumn=col;changed=true;}}
                    ImGui::EndCombo();}
                row.Result(changed,true);
            }
            int rows=int(c.RowSpan),cols=int(c.ColumnSpan);
            if(PropertyUI::DragInt("row-span","Row span",rows,1,1,64))c.RowSpan=unsigned(rows);
            if(PropertyUI::DragInt("column-span","Column span",cols,1,1,64))c.ColumnSpan=unsigned(cols);
        }
        if(entity.HasComponent<SpriteRendererComponent>()){
            int fit=int(c.Fit);
            if(entity.HasComponent<UINineSliceComponent>())PropertyUI::ReadOnly("fit-owned","Image fit","Stretch (nine-slice)");
            else if(PropertyUI::Combo("image-fit","Image fit",fit,"Stretch\0Contain\0Cover\0Native size\0"))c.Fit=GameUI::ImageFit(fit);
            if(c.Fit!=GameUI::ImageFit::Stretch)PropertyUI::Vector("image-align","Image alignment",glm::value_ptr(c.ImageAlignment),2,.01f,nullptr,"%.2f",{},0,1);
            PropertyUI::Help("Stretch paints the border box. Other modes use the padded content box and source region; alignment is 0 = left/top, 1 = right/bottom. Fit changes art, not text layout or button hit bounds. UI uses one tile; the sprite tiling value applies to world sprites.");
        }
        if(scrollContent)PropertyUI::ReadOnly("margin","Margin L/T/R/B","0 (viewport owns origin)");
        else PropertyUI::Vector("margin","Margin L/T/R/B",glm::value_ptr(c.Margin),4,1,nullptr,"%.0f",{},0,4096);
        PropertyUI::Vector("padding","Padding L/T/R/B",glm::value_ptr(c.Padding),4,1,nullptr,"%.0f",{},0,4096);
        if(flowOwned){
            const auto &container=parent.GetComponent<UILayoutComponent>();
            bool mainFill=container.Direction==GameUI::Axis::Horizontal?c.WidthMode==GameUI::SizeMode::Fill:c.HeightMode==GameUI::SizeMode::Fill;
            if(container.Kind!=GameUI::ContainerKind::Grid&&mainFill)PropertyUI::DragFloat("flex","Fill weight (0 = 1)",c.Flex,.1f,0,100);
            if(auto resolved=m_Context->GetUISnapshot().Find(entity.GetUUID());resolved&&resolved->LayoutResolved){char value[80];std::snprintf(value,sizeof(value),"%.1f, %.1f px (computed)",resolved->Bounds.Position.x,resolved->Bounds.Position.y);PropertyUI::ReadOnly("position","Canvas position",value);}
        }
        PropertyUI::Checkbox("visible","Visible",c.Visible);PropertyUI::Checkbox("enabled","Enabled",c.Enabled);PropertyUI::Checkbox("clip","Clip children",c.ClipChildren);
        PropertyUI::Help("Logical pixels, top-left origin. Drag a selected element in the viewport to move it; drag an edge/corner handle to resize. Escape cancels the gesture. Containers own child positions; Fill owns free positioning on its axis.");
    },[this](const std::string& error){m_HierarchyError=error;});
    DrawUIComponent<UILayoutComponent>("UI Container",entity,m_Sections,m_RestoreSections,[](auto& c){
        int kind=int(c.Kind),axis=int(c.Direction),main=int(c.Main),cross=int(c.Cross);
        if(PropertyUI::Combo("kind","Container",kind,"Stack\0Grid\0Wrap\0"))c.Kind=GameUI::ContainerKind(kind);
        bool gridSettings=c.Kind==GameUI::ContainerKind::Grid;
        if(!gridSettings)gridSettings=ImGui::TreeNode("Grid settings (inactive)");
        if(gridSettings){
            PropertyUI::Vector("grid-gap","Column / row gaps",glm::value_ptr(c.GridGap),2,1,nullptr,"%.1f",{},0,4096);
            auto tracks=[](const char *name,std::vector<GameUI::Track> &values){
                if(!ImGui::TreeNode(name))return;
                int count=int(values.size());if(PropertyUI::DragInt("count","Tracks",count,1,1,64)){if(count>=1&&count<=64)values.resize(size_t(count));else PropertyUI::Validation("Grid track count must be 1..64");}
                for(size_t i=0;i<values.size();++i){ImGui::PushID(int(i));
                    if(ImGui::TreeNode("track","%zu",i)){auto &t=values[i];int mode=int(t.Mode);
                        if(PropertyUI::Combo("mode","Sizing",mode,"Fixed\0Fill\0"))t.Mode=GameUI::TrackMode(mode);
                        PropertyUI::DragFloat("value",mode?"Weight":"Pixels",t.Value,1,mode?.01f:0,FLT_MAX,"%.1f");
                        PropertyUI::DragFloat("min","Minimum",t.Minimum,1,0,FLT_MAX,"%.1f");
                        if(PropertyUI::Checkbox("bounded","Maximum enabled",t.HasMaximum)&&t.HasMaximum)t.Maximum=std::max(t.Minimum,t.Value);
                        if(t.HasMaximum)PropertyUI::DragFloat("max","Maximum",t.Maximum,1,0,FLT_MAX,"%.1f");
                        ImGui::TreePop();}ImGui::PopID();}
                ImGui::TreePop();};
            tracks("Columns",c.Columns);tracks("Rows",c.Rows);
            PropertyUI::Help("Explicit cells reserve first; automatic children use the first free row-major cell. Hidden children retain cells. Track minima overflow rather than shrink.");
            if(c.Kind!=GameUI::ContainerKind::Grid)ImGui::TreePop();
        }
        if(c.Kind!=GameUI::ContainerKind::Grid){
            if(PropertyUI::Combo("axis","Direction",axis,"Horizontal\0Vertical\0"))c.Direction=GameUI::Axis(axis);
            PropertyUI::DragFloat("gap","Item gap (px)",c.Gap,1,0,4096);
            if(c.Kind==GameUI::ContainerKind::Wrap){
                PropertyUI::DragFloat("line-gap","Line gap (px)",c.LineGap,1,0,4096);
                int line=int(c.LineAlign);if(PropertyUI::Combo("line-align","Line block alignment",line,"Start\0Center\0End\0"))c.LineAlign=GameUI::Align(line);
                PropertyUI::Help("Main size must be Fixed/Fill. Items break greedily; Fill uses its minimum for breaking and shares space within the resulting line. Cross Fill fills line thickness, not the panel.");}

        }
        if(PropertyUI::Combo("main",c.Kind==GameUI::ContainerKind::Grid?"Cell horizontal":"Main alignment",main,"Start\0Center\0End\0"))c.Main=GameUI::Align(main);
        if(PropertyUI::Combo("cross",c.Kind==GameUI::ContainerKind::Grid?"Cell vertical":"Cross alignment",cross,"Start\0Center\0End\0"))c.Cross=GameUI::Align(cross);
    });
    DrawUIComponent<UITextStyleComponent>("UI Text Style",entity,m_Sections,m_RestoreSections,[this,entity](auto& c) mutable {
        PropertyUI::DragFloat("size","Size (px)",c.Size,1,1,512);
        if(entity.HasComponent<UITextEntryComponent>()){
            if(entity.GetComponent<UITextEntryComponent>().Multiline)PropertyUI::Checkbox("entry-wrap","Wrap note",c.Wrap);
            PropertyUI::ReadOnly("entry-layout","Text window","Top left / clipped internal scrolling");
            PropertyUI::Help("UI Text Entry owns content limits. Caret/selection use these font metrics; display ellipsis and line limiting are unavailable while editing.");return;
        }
        int horizontal=int(c.Horizontal),vertical=int(c.Vertical);
        if(PropertyUI::Combo("horizontal","Horizontal",horizontal,"Left\0Center\0Right\0"))c.Horizontal=GameUI::Align(horizontal);
        if(PropertyUI::Combo("vertical","Vertical",vertical,"Top\0Center\0Bottom\0"))c.Vertical=GameUI::Align(vertical);
        PropertyUI::Checkbox("wrap","Wrap",c.Wrap);
        int overflow=int(c.Overflow);
        if(PropertyUI::Combo("overflow","Overflow",overflow,"Clip\0Ellipsis\0"))c.Overflow=Text::Overflow(overflow);
        int lines=int(c.MaxLines);
        if(PropertyUI::DragInt("max-lines","Max lines",lines,1,0,4096))c.MaxLines=uint32_t(lines);
        PropertyUI::Help("0 lines means unlimited. Clip keeps partial lines at box edges; Max lines removes whole lines. Ellipsis fits complete lines and marks omitted content. Source text is unchanged.");
    });
    DrawUIComponent<UINineSliceComponent>("UI Nine Slice",entity,m_Sections,m_RestoreSections,[](auto& c){
        PropertyUI::Vector("border","Source L/T/R/B (px)",glm::value_ptr(c.Borders),4,1,nullptr,"%.0f",{},0,4096);
        PropertyUI::DragFloat("border-scale","Border scale",c.BorderScale,.1f,.01f,64);
        PropertyUI::Help("Borders must fit the source sprite region. Pixel borders retain their size; the middle stretches. Small targets clamp borders proportionally.");
    });
    DrawUIComponent<UIButtonComponent>("UI Button",entity,m_Sections,m_RestoreSections,[&,entity](auto& c)mutable{
        PropertyUI::DragInt("tab","Tab order",c.TabOrder);PropertyUI::Checkbox("interactive","Interactable",c.Interactable);
        int kind=int(c.Kind);if(PropertyUI::Combo("kind","Behavior",kind,"Button\0Toggle / Checkbox\0Radio option\0Slider\0Text Entry\0")){
            c.Kind=GameUI::ControlKind(kind);if(c.Kind!=GameUI::ControlKind::Toggle&&c.Kind!=GameUI::ControlKind::Radio){c.Indicator=0;c.InitiallyChecked=false;}if(c.Kind!=GameUI::ControlKind::Toggle&&c.Kind!=GameUI::ControlKind::Slider&&c.Kind!=GameUI::ControlKind::TextEntry)c.EventOwner=0;if(c.Kind!=GameUI::ControlKind::Radio)c.Group=0;
            else if(!c.Group){auto current=m_Context->GetEntityByUUID(m_Context->GetRelationship(entity).Parent);for(size_t depth=0;current&&depth<Scene::MaxDepth;++depth){if(current.HasComponent<UIRadioGroupComponent>()){c.Group=current.GetUUID();break;}if(current.HasComponent<UICanvasComponent>())break;current=m_Context->GetEntityByUUID(m_Context->GetRelationship(current).Parent);}}
        }
        auto composition=m_Context->GetEntityByUUID(m_Context->GetRelationship(entity).Parent);
        bool ownedTrigger=composition&&composition.HasComponent<UIDropdownComponent>()&&composition.GetComponent<UIDropdownComponent>().Trigger==uint64_t(entity.GetUUID());
        if(ownedTrigger)PropertyUI::ReadOnly("action","Action identity","Owned by Dropdown / Radio Group");
        else if(c.Kind!=GameUI::ControlKind::Radio)PropertyUI::Text("action","Action identity",c.Action);
        else PropertyUI::Help("Selection changes use the group's action identity and member references.");
        if(c.Kind==GameUI::ControlKind::Toggle||c.Kind==GameUI::ControlKind::Radio){
            if(c.Kind==GameUI::ControlKind::Toggle)PropertyUI::Checkbox("initial-checked","Initially checked",c.InitiallyChecked);
            else targetPicker("group","Radio group",c.Group,[&](Entity candidate){
                if(!candidate.HasComponent<UIRadioGroupComponent>())return false;
                auto current=m_Context->GetEntityByUUID(m_Context->GetRelationship(entity).Parent);
                for(size_t depth=0;current&&depth<Scene::MaxDepth;++depth){if(current==candidate)return true;if(current.HasComponent<UICanvasComponent>())break;current=m_Context->GetEntityByUUID(m_Context->GetRelationship(current).Parent);}return false;
            });
            targetPicker("indicator","Selected visual",c.Indicator,visualTarget);
            if(!c.Indicator)PropertyUI::Help("A square cue is drawn when selected. Optional Sprite/Rect child replaces it; layout space stays reserved.");
            if(c.Kind==GameUI::ControlKind::Toggle&&ImGui::TreeNode("Standalone notification")){targetPicker("value-owner","Event owner",c.EventOwner,ownerTarget);PropertyUI::Help("Managed screens use their declared owner. Ordinary canvases require this explicit recipient.");ImGui::TreePop();}
        }
        if(c.Kind==GameUI::ControlKind::TextEntry)PropertyUI::Help("Text Entry owns editing; configure UI Text Entry and Text Renderer. It emits typed text events, never Activated.");
        if(c.Kind==GameUI::ControlKind::Slider){
            if(ImGui::TreeNode("Standalone notification")){targetPicker("value-owner","Event owner",c.EventOwner,ownerTarget);ImGui::TreePop();}
            PropertyUI::Help("Accept enters/confirms editing; Adjust changes a step; Cancel restores the starting value. Pointer track-click/drag edits directly. Slider configuration below owns range and parts.");
        }
        static constexpr const char* states[]={"Normal","Hovered","Pressed","Disabled","Focused"};
        static_assert(std::size(states)==std::tuple_size_v<decltype(c.Colors)>);
        static_assert(std::size(states)==std::tuple_size_v<decltype(c.Images)>);
        static_assert(std::size(states)==std::tuple_size_v<decltype(m_UIStatePickers)>);
        for(size_t i=0;i<std::size(states);++i){
            ImGui::PushID(int(i));
            PropertyUI::Color("tint",states[i],glm::value_ptr(c.Colors[i]));
            if(ImGui::TreeNode("State image (optional)")){
                SpriteRendererComponent temp;temp.Source=c.Images[i];temp.Color=c.Colors[i];
                SpriteSourceEditor(temp,m_UIStatePickers[i],OpenAsset,[this]{return CanEdit(true);});
                c.Images[i]=temp.Source;c.Colors[i]=temp.Color;ImGui::TreePop();
            }
            ImGui::PopID();
        }
        if(c.Kind==GameUI::ControlKind::Button)PropertyUI::Help("C#: UIButtonComponent.TryConsumeAction on ordinary canvases; managed screens use GameUI.TryConsumeEvent. Pointer release / Accept activates once.");
        else PropertyUI::Help("Pointer release / Accept changes the value once. GameUI.TryConsumeEvent delivers a typed change to the declared owner. Normal setters are silent.");
    });

    DrawUIComponent<UIRadioGroupComponent>("UI Radio Group",entity,m_Sections,m_RestoreSections,[&](auto &c){
        if(entity.HasComponent<UITabsComponent>())PropertyUI::ReadOnly("required","Selection","Required by Tabs");else PropertyUI::Checkbox("required","Require selection",c.Required);PropertyUI::Text("action","Action identity",c.Action);
        targetPicker("initial-selection","Initial selection",c.InitialSelection,[&](Entity candidate){return candidate.HasComponent<UIButtonComponent>()&&candidate.GetComponent<UIButtonComponent>().Kind==GameUI::ControlKind::Radio&&candidate.GetComponent<UIButtonComponent>().Group==uint64_t(entity.GetUUID());});
        if(ImGui::TreeNode("Standalone notification")){targetPicker("value-owner","Event owner",c.EventOwner,ownerTarget);PropertyUI::Help("Managed screens use their declared owner. Ordinary canvases require this explicit recipient.");ImGui::TreePop();}
        PropertyUI::Help(entity.HasComponent<UIDropdownComponent>()?"Dropdown owns one group value. Navigation highlights; Accept selects and closes. Setters are silent. Required fallback: Tab order then ID.":"Tab and directions move focus; Accept selects. Activating the current option does nothing. Required selection falls back by Tab order then ID.");
    });
    DrawUIComponent<UITextEntryComponent>("UI Text Entry",entity,m_Sections,m_RestoreSections,[&](auto &c){
        PropertyUI::Text("placeholder","Placeholder",c.Placeholder);
        PropertyUI::Checkbox("multiline","Multiline",c.Multiline);PropertyUI::Checkbox("readonly","Read only / selectable",c.ReadOnly);
        int bytes=int(c.MaxBytes),graphemes=int(c.MaxGraphemes),lines=int(c.MaxLines);
        if(PropertyUI::DragInt("entry-bytes","Maximum UTF-8 bytes",bytes,1,1,16384))c.MaxBytes=uint32_t(bytes);
        if(PropertyUI::DragInt("entry-graphemes","Maximum graphemes",graphemes,1,1,4096))c.MaxGraphemes=uint32_t(graphemes);
        if(PropertyUI::DragInt("entry-lines","Maximum explicit lines",lines,1,1,256))c.MaxLines=uint32_t(lines);
        PropertyUI::Color("caret-color","Caret color",glm::value_ptr(c.CaretColor));PropertyUI::Color("selection-color","Selection color",glm::value_ptr(c.SelectionColor));
        PropertyUI::Help("Text Renderer supplies initial content/font; Button supplies action/owner. Runtime values and local text undo stay in Play. Enter submits a single line; Ctrl+Enter submits notes; Escape restores. Tab commits and traverses. GLFW exposes committed text, not preedit/candidate positioning.");
    });
    DrawUIComponent<UITabsComponent>("UI Tabs",entity,m_Sections,m_RestoreSections,[&](auto &c){
        PropertyUI::Checkbox("remember-page-focus","Remember page focus",c.RememberFocus);
        PropertyUI::Help("Radio Group owns selection. Navigation focuses; Accept selects. Pages collapse when inactive. Add Tab and page from the hierarchy; drag/reorder tab siblings independently of page identities.");
        if(!m_Context->IsRunning()&&!m_Context->IsSimulating()&&ImGui::TreeNode("Page preview")){
            for(auto h:m_Context->GetAllEntitiesWith<UITabComponent>()){Entity tab(h,m_Context.get());if(tab.HasComponent<UIButtonComponent>()&&tab.GetComponent<UIButtonComponent>().Group==uint64_t(entity.GetUUID())){ImGui::PushID(std::to_string(uint64_t(tab.GetUUID())).c_str());if(ImGui::Selectable(tab.GetName().c_str()))try{m_Context->PreviewUITab(entity.GetUUID(),tab.GetUUID());}catch(const std::exception &e){m_HierarchyError=e.what();}ImGui::PopID();}}
            if(ImGui::SmallButton("Use authored initial page"))m_Context->PreviewUITab(entity.GetUUID(),0);ImGui::TreePop();
        }
    });
    DrawUIComponent<UITabComponent>("UI Tab",entity,m_Sections,m_RestoreSections,[&](auto &c){
        auto group=entity.HasComponent<UIButtonComponent>()?entity.GetComponent<UIButtonComponent>().Group:0;
        targetPicker("page","Page",c.Page,[&](Entity e){return e.HasComponent<UIRectComponent>()&&!e.HasComponent<UICanvasComponent>()&&uint64_t(m_Context->GetRelationship(e).Parent)==group;});
        targetPicker("page-focus","Initial page focus",c.InitialFocus,[&](Entity e){if(!e.HasComponent<UIButtonComponent>())return false;for(size_t d=0;e&&d<Scene::MaxDepth;++d){if(uint64_t(e.GetUUID())==c.Page)return true;e=m_Context->GetEntityByUUID(m_Context->GetRelationship(e).Parent);}return false;});
        PropertyUI::Help("One tab owns each direct page. Initial focus is used when focus leaves departing page content; tab activation keeps focus on the tab.");
    });
    DrawUIComponent<UITooltipComponent>("UI Tooltip",entity,m_Sections,m_RestoreSections,[&](auto &c){
        targetPicker("tip-body","Text body",c.Body,[&](Entity e){return e.HasComponent<UIRectComponent>()&&e.HasComponent<TextComponent>()&&!e.HasComponent<UIButtonComponent>()&&m_Context->GetRelationship(e).Parent==entity.GetUUID();});
        PropertyUI::DragFloat("delay","Delay (seconds)",c.Delay,.05f,0,5);PropertyUI::DragFloat("gap","Surface gap",c.Gap,1,0,64);PropertyUI::Help("Purpose/consequences, not a duplicate label. Hover or navigation focus discovers this passive overlay. Dropdowns, captures and edits suppress it.");
        if(!m_Context->IsRunning()&&!m_Context->IsSimulating()){if(ImGui::SmallButton("Preview tooltip"))try{m_Context->PreviewUITooltip(entity.GetUUID(),true);}catch(const std::exception &e){m_HierarchyError=e.what();}ImGui::SameLine();if(ImGui::SmallButton("Hide tooltip"))m_Context->PreviewUITooltip(entity.GetUUID(),false);}
    });
    DrawUIComponent<UIDropdownComponent>("UI Dropdown",entity,m_Sections,m_RestoreSections,[&](auto &c){
        targetPicker("trigger","Trigger",c.Trigger,[&](Entity e){return m_Context->GetRelationship(e).Parent==entity.GetUUID()&&e.HasComponent<UIButtonComponent>()&&e.HasComponent<TextComponent>();});
        targetPicker("popup","Popup",c.Popup,[&](Entity e){return m_Context->GetRelationship(e).Parent==entity.GetUUID()&&e.HasComponent<UIScrollViewComponent>()&&e.HasComponent<UIRectComponent>()&&!e.HasComponent<UICanvasComponent>();});
        PropertyUI::Text("placeholder","Placeholder",c.Placeholder);
        PropertyUI::DragFloat("min-width","Minimum width",c.MinimumWidth,1,1,4096);PropertyUI::DragFloat("max-height","Maximum height",c.MaximumHeight,1,1,4096);PropertyUI::DragFloat("gap","Popup gap",c.Gap,1,0,64);
        PropertyUI::Help("Radio Group below owns initial/required selection, action and recipient. Add Dropdown option or duplicate/reorder option children. Trigger label is computed; option labels remain editable.");
        if(!m_Context->IsRunning()&&!m_Context->IsSimulating()){bool preview=m_Context->IsUIDropdownOpen(entity.GetUUID());if(ImGui::Button(preview?"Close popup preview":"Preview popup"))try{m_Context->PreviewUIDropdown(entity.GetUUID(),!preview);}catch(const std::exception &e){m_HierarchyError=e.what();}}
    });
    DrawUIComponent<UIScrollViewComponent>("UI ScrollView",entity,m_Sections,m_RestoreSections,[&](auto &c){
        int axes=c.Horizontal?(c.Vertical?2:1):0;if(PropertyUI::Combo("axes","Axes",axes,"Vertical\0Horizontal\0Both\0")){c.Horizontal=axes!=0;c.Vertical=axes!=1;}
        targetPicker("content","Content root",c.Content,[&](Entity candidate){return m_Context->GetRelationship(candidate).Parent==entity.GetUUID()&&candidate.HasComponent<UIRectComponent>()&&!candidate.HasComponent<UICanvasComponent>();});
        PropertyUI::DragFloat("speed","Pixels per unit",c.PixelsPerUnit,1,1,1024);PropertyUI::DragFloat("reveal","Reveal margin",c.RevealMargin,1,0,64);
        if(ImGui::TreeNode("Scrollbar parts")){PropertyUI::DragFloat("thumb-min","Minimum thumb",c.MinimumThumb,1,1,256);
            targetPicker("h-track","Horizontal track",c.HorizontalTrack,visualTarget);targetPicker("h-thumb","Horizontal thumb",c.HorizontalThumb,visualTarget);
            targetPicker("v-track","Vertical track",c.VerticalTrack,visualTarget);targetPicker("v-thumb","Vertical thumb",c.VerticalThumb,visualTarget);ImGui::TreePop();}
        PropertyUI::Help("Fixed/Fill viewport; direct content group with zero origin/margins. Scroll axes use Content/Fixed extent. Padding can reserve fixed scrollbar gutters. Parts do not reflow when hidden. No nested Canvas in content.");
        if(!m_Context->IsRunning()&&ImGui::TreeNode("Transient preview")){
            try{auto g=m_Context->GetUIScroll(entity.GetUUID());glm::vec2 normalized{g.Range.x>0?g.Offset.x/g.Range.x:0,g.Range.y>0?g.Offset.y/g.Range.y:0};
                bool changed=PropertyUI::SliderFloat("preview-x","Horizontal position",normalized.x,0,1);changed|=PropertyUI::SliderFloat("preview-y","Vertical position",normalized.y,0,1);
                if(changed)m_Context->ScrollUITo(entity.GetUUID(),normalized,true);
                ImGui::TextDisabled("View %.0f x %.0f | content %.0f x %.0f",g.Viewport.Size.x,g.Viewport.Size.y,g.Extent.x,g.Extent.y);
            }catch(const std::exception &error){PropertyUI::Validation(error.what());}ImGui::TreePop();}
    });
    DrawUIComponent<UISliderComponent>("UI Slider",entity,m_Sections,m_RestoreSections,[&](auto &c){
        PropertyUI::DragFloat("minimum","Minimum",c.Minimum);PropertyUI::DragFloat("maximum","Maximum",c.Maximum);
        PropertyUI::DragFloat("step","Step",c.Step,.01f);PropertyUI::DragFloat("initial","Initial value",c.InitialValue);
        int direction=int(c.Direction);if(PropertyUI::Combo("direction","Increasing toward",direction,"Right\0Left\0Down\0Up\0"))c.Direction=GameUI::FillDirection(direction);
        targetPicker("track","Track",c.Track,visualTarget);targetPicker("fill","Fill (optional)",c.Fill,visualTarget);targetPicker("thumb","Thumb",c.Thumb,visualTarget);
        PropertyUI::Help("Direct Sprite/Rect leaves. Track and thumb size determine travel; thumb position and fill extent are computed. Initial value clamps/snaps. Setters are silent; typed events report live, committed and cancelled edits.");
    });
    DrawUIComponent<UIProgressComponent>("UI Progress",entity,m_Sections,m_RestoreSections,[&](auto &c){
        PropertyUI::DragFloat("minimum","Minimum",c.Minimum);PropertyUI::DragFloat("maximum","Maximum",c.Maximum);PropertyUI::DragFloat("initial","Initial value",c.InitialValue);
        int direction=int(c.Direction);if(PropertyUI::Combo("direction","Fill direction",direction,"Right\0Left\0Down\0Up\0"))c.Direction=GameUI::FillDirection(direction);
        targetPicker("fill","Fill visual",c.Fill,visualTarget);if(!c.Fill)PropertyUI::Validation("Assign a direct Sprite/Rect child to show progress.");
        PropertyUI::Help("Finite values clamp to the increasing range. Fill is computed inside content bounds; no input or user events.");
    });
    if(entity.HasComponent<UICanvasComponent>()||entity.HasComponent<UIRectComponent>()) {
        if(ImGui::Button("Validate UI"))try{m_Context->ValidateUI(false);m_HierarchyError.clear();}catch(const std::exception& error){HierarchyFailed(error.what());}
        if(!m_HierarchyError.empty())PropertyUI::Validation(m_HierarchyError.c_str());
        if(!m_Context->GetUIError().empty())PropertyUI::Validation(m_Context->GetUIError().c_str());
    }
    DrawComponent<CameraComponent>(
        "Camera", entity, m_Sections,m_RestoreSections,
        [](auto &component)
        {
            auto &camera = component.Camera;

            PropertyUI::Checkbox("Primary", "Primary", component.Primary);

            const char *projectionTypeStrings[] = {"Perspective", "Orthographic"};
            const char *currentProjectionTypeString =
                projectionTypeStrings[(int)camera.GetProjectionType()];
            {
                PropertyUI::Row projectionRow("projection", "Projection");
                if (ImGui::BeginCombo("##value", currentProjectionTypeString))
                {
                    for (int i = 0; i < 2; i++)
                    {
                        bool isSelected = currentProjectionTypeString == projectionTypeStrings[i];
                        if (ImGui::Selectable(projectionTypeStrings[i], isSelected))
                        {
                            currentProjectionTypeString = projectionTypeStrings[i];
                            camera.SetProjectionType((SceneCamera::ProjectionType)i);
                        }

                        if (isSelected)
                            ImGui::SetItemDefaultFocus();
                    }

                    ImGui::EndCombo();
                }
            }
            if (camera.GetProjectionType() == SceneCamera::ProjectionType::Perspective)
            {
                float perspectiveVerticalFov = glm::degrees(camera.GetPerspectiveVerticalFOV());
                if (PropertyUI::DragFloat("vertical-fov", "Vertical FOV (deg)", perspectiveVerticalFov))
                    camera.SetPerspectiveVerticalFOV(glm::radians(perspectiveVerticalFov));

                float perspectiveNear = camera.GetPerspectiveNearClip();
                if (PropertyUI::DragFloat("Near", "Near", perspectiveNear))
                    camera.SetPerspectiveNearClip(perspectiveNear);

                float perspectiveFar = camera.GetPerspectiveFarClip();
                if (PropertyUI::DragFloat("Far", "Far", perspectiveFar))
                    camera.SetPerspectiveFarClip(perspectiveFar);
            }

            if (camera.GetProjectionType() == SceneCamera::ProjectionType::Orthographic)
            {
                float orthoSize = camera.GetOrthographicSize();
                if (PropertyUI::DragFloat("Size", "Size", orthoSize))
                    camera.SetOrthographicSize(orthoSize);

                float orthoNear = camera.GetOrthographicNearClip();
                if (PropertyUI::DragFloat("Near", "Near", orthoNear))
                    camera.SetOrthographicNearClip(orthoNear);

                float orthoFar = camera.GetOrthographicFarClip();
                if (PropertyUI::DragFloat("Far", "Far", orthoFar))
                    camera.SetOrthographicFarClip(orthoFar);

                PropertyUI::Checkbox("Fixed Aspect Ratio", "Fixed Aspect Ratio",
                                     component.FixedAspectRatio);
            }
        });

    DrawComponent<ScriptComponent>(
        "Script", entity, m_Sections,m_RestoreSections,
        [this, entity](auto &component) mutable
        {
            if (m_Context && m_Context->IsRunning())
            {
                ImGui::TextWrapped(
                    "Runtime script values are temporary. Stop Play to edit authored fields.");
                return;
            }
            auto classes = ScriptEngine::GetEntityClasses();
            std::vector<std::string> names;
            for (auto &[name, type] : classes)
                names.push_back(name);
            std::sort(names.begin(), names.end());
            {
                PropertyUI::Row classRow("class", "Class");
                if (ImGui::BeginCombo("##value", component.ClassName.empty()
                                                     ? "Select compiled class"
                                                     : component.ClassName.c_str()))
                {
                    if (ImGui::Selectable("None", component.ClassName.empty()))
                    {
                        component.ClassName.clear();
                        ScriptEngine::GetScriptFieldMap(entity).clear();
                    }
                    for (auto &name : names)
                        if (ImGui::Selectable(name.c_str(), name == component.ClassName))
                        {
                            if (component.ClassName != name)
                                ScriptEngine::GetScriptFieldMap(entity).clear();
                            component.ClassName = name;
                        }
                    ImGui::EndCombo();
                }
            }
            if (EditScript && !component.ClassName.empty() && ImGui::Button("Open Script"))
                EditScript(component.ClassName);
            if (ImGui::CollapsingHeader("Script authoring help"))
                ImGui::TextWrapped(
                    "Project > Create Script / Build Scripts. Fields are authored overrides; "
                    "unset fields use C# defaults, which are not evaluated here.");
            auto type = ScriptEngine::GetEntityClass(component.ClassName);
            if (!type)
            {
                if (!component.ClassName.empty())
                    ImGui::TextWrapped(
                        "Class unavailable. Build Scripts or select an existing compiled class.");
                auto &stored = ScriptEngine::GetScriptFieldMap(entity);
                for (auto &[name, value] : stored) {
                    if (value.Field.Type != ScriptFieldType::Entity) continue;
                    ImGui::PushID(name.c_str());
                    const auto id = value.GetValue<uint64_t>();
                    auto target = m_Context->GetEntityByUUID(id);
                    const auto text = !id ? std::string("Explicitly unassigned override")
                        : (target ? target.GetName() : std::string("Unresolved entity")) + " · " + std::to_string(id);
                    PropertyUI::ReadOnly("stored-entity", name.c_str(), text.c_str());
                    ImGui::BeginDisabled(!id);
                    if (ImGui::SmallButton("Clear stored reference"))
                        ClearStoredEntityReference(entity, name);
                    ImGui::EndDisabled();
                    PropertyUI::Help("Writes an explicit unassigned entity override. Other stored fields are retained; the C# constructor default is not evaluated.");
                    ImGui::PopID();
                }
                return;
            }
            auto &values = ScriptEngine::GetScriptFieldMap(entity);
            for (auto &[name, field] : type->GetFields())
            {
                if (field.Type == ScriptFieldType::None)
                    continue;
                ImGui::PushID(name.c_str());
                auto found = values.find(name);
                bool authored = found != values.end() && found->second.Field.Type == field.Type;
                if (!authored)
                {
                    {
                        PropertyUI::Row row("default", name.c_str());
                        ImGui::TextWrapped("C# default (value not evaluated)");
                        if (ImGui::SmallButton("Create override..."))
                            ImGui::OpenPopup("New override");
                        if (ImGui::BeginPopup("New override"))
                        {
                            ImGui::TextWrapped(
                                "An explicit override starts at zero / unassigned. This is "
                                "not the C# constructor value.");
                            if (ImGui::Button("Create explicit override"))
                            {
                                ScriptFieldInstance initial;
                                initial.Field = {field.Type, name, nullptr};
                                values[name] = initial;
                                ImGui::CloseCurrentPopup();
                            }
                            ImGui::EndPopup();
                        }
                    }
                    ImGui::PopID();
                    continue;
                }
                ScriptFieldInstance value;
                if (authored)
                    value = found->second;
                value.Field = {field.Type, name, nullptr};
                bool changed = false;
                switch (field.Type)
                {
                case ScriptFieldType::Float:
                {
                    auto x = value.GetValue<float>();
                    changed = PropertyUI::DragFloat("value", name.c_str(), x, .05f);
                    value.SetValue(x);
                    break;
                }
                case ScriptFieldType::Double:
                {
                    auto x = value.GetValue<double>();
                    changed = PropertyUI::Double("value", name.c_str(), x);
                    value.SetValue(x);
                    break;
                }
                case ScriptFieldType::Bool:
                {
                    auto x = value.GetValue<bool>();
                    changed = PropertyUI::Checkbox("value", name.c_str(), x);
                    value.SetValue(x);
                    break;
                }
                case ScriptFieldType::Vector2:
                {
                    auto x = value.GetValue<glm::vec2>();
                    changed = PropertyUI::Vector("value", name.c_str(), glm::value_ptr(x), 2, .05f);
                    value.SetValue(x);
                    break;
                }
                case ScriptFieldType::Vector3:
                {
                    auto x = value.GetValue<glm::vec3>();
                    changed = PropertyUI::Vector("value", name.c_str(), glm::value_ptr(x), 3, .05f);
                    value.SetValue(x);
                    break;
                }
                case ScriptFieldType::Vector4:
                {
                    auto x = value.GetValue<glm::vec4>();
                    changed = PropertyUI::Vector("value", name.c_str(), glm::value_ptr(x), 4, .05f);
                    value.SetValue(x);
                    break;
                }
                case ScriptFieldType::Entity:
                {
                    PropertyUI::Row row("value", name.c_str());
                    auto id = value.GetValue<uint64_t>();
                    auto ref = m_Context ? m_Context->GetEntityByUUID(id) : Entity{};
                    const auto referenceName = ref  ? ref.GetName()
                                               : id ? "Missing entity (" + std::to_string(id) + ")"
                                                    : "None";
                    if (ImGui::BeginCombo("##value", referenceName.c_str()))
                    {
                        if (ImGui::Selectable("None", !id))
                        {
                            value.SetValue<uint64_t>(0);
                            changed = true;
                        }
                        if (m_Context)
                            for (auto e : m_Context->GetAllEntitiesWith<IDComponent>())
                            {
                                Entity choice{e, m_Context.get()};
                                ImGui::PushID(static_cast<int>((entt::entity)choice));
                                if (ImGui::Selectable(choice.GetName().c_str(), choice.GetUUID() == id))
                                {
                                    value.SetValue<uint64_t>(choice.GetUUID());
                                    changed = true;
                                }
                                if (ImGui::IsItemHovered())
                                {
                                    auto p = choice.GetComponent<TransformComponent>().Translation;
                                    ImGui::SetTooltip("Local position (scene units): %.2f, %.2f, %.2f", p.x, p.y, p.z);
                                }
                                ImGui::PopID();
                            }
                        ImGui::EndCombo();
                    }
                    break;
                }
                case ScriptFieldType::Prefab:
                {
                    PropertyUI::Row row("value", name.c_str());
                    if (ImGui::BeginCombo("##value", value.AssetReference.empty()
                                                         ? "None"
                                                         : value.AssetReference.c_str()))
                    {
                        if (ImGui::Selectable("None", value.AssetReference.empty()))
                        {
                            value.AssetReference.clear();
                            changed = true;
                        }
                        auto root = Project::GetAssetDirectory();
                        if (!m_PrefabChoicesReady || ImGui::Button("Refresh prefabs"))
                        {
                            m_PrefabChoicesReady = true;
                            m_PrefabChoices.clear();
                            std::error_code error;
                            for (auto it = std::filesystem::recursive_directory_iterator(root, error);
                                 !error && it != std::filesystem::recursive_directory_iterator();
                                 it.increment(error))
                                if (it->is_regular_file(error) && it->path().extension() == ".hprefab")
                                    m_PrefabChoices.push_back(
                                        it->path().lexically_relative(root).generic_u8string());
                            std::sort(m_PrefabChoices.begin(), m_PrefabChoices.end());
                        }
                        for (auto &path : m_PrefabChoices)
                            if (ImGui::Selectable(path.c_str(), path == value.AssetReference))
                            {
                                try
                                {
                                    Prefab::Load(root, std::filesystem::u8path(path));
                                    value.AssetReference = path;
                                    changed = true;
                                }
                                catch (const std::exception &error)
                                {
                                    if (ReportError)
                                        ReportError(error.what());
                                    else
                                        HZ_ERROR("Select prefab: {}", error.what());
                                }
                            }
                        ImGui::EndCombo();
                    }
                    break;
                }
                case ScriptFieldType::Sprite:
                {
                    SpriteReference r{std::filesystem::u8path(value.AssetReference), value.AssetID};
                    changed = SpritePicker(name.c_str(), r, m_Pickers[ImGui::GetID("sprite-field")],
                                           OpenAsset, [this] { return CanEdit(); });
                    value.AssetReference = r.Sheet.generic_u8string();
                    value.AssetID = r.Region;
                    break;
                }
                case ScriptFieldType::SpriteAnimation:
                {
                    AnimationReference r{std::filesystem::u8path(value.AssetReference), value.AssetID};
                    changed = ClipPicker(name.c_str(), r, m_Pickers[ImGui::GetID("clip-field")],
                                         OpenAsset, [this] { return CanEdit(); });
                    value.AssetReference = r.Sheet.generic_u8string();
                    value.AssetID = r.Clip;
                    break;
                }
#define HZ_FIELD_NUMBER(Type, Cpp, Gui)                                                                 \
    case ScriptFieldType::Type:                                                                         \
    {                                                                                                   \
        auto x = value.GetValue<Cpp>();                                                                 \
        changed = PropertyUI::Scalar("value", name.c_str(), Gui, &x);                                   \
        value.SetValue(x);                                                                              \
        break;                                                                                          \
    }
                    HZ_FIELD_NUMBER(Char, uint16_t, ImGuiDataType_U16)
                    HZ_FIELD_NUMBER(Byte, int8_t, ImGuiDataType_S8)
                    HZ_FIELD_NUMBER(Short, int16_t, ImGuiDataType_S16)
                    HZ_FIELD_NUMBER(Int, int32_t, ImGuiDataType_S32)
                    HZ_FIELD_NUMBER(Long, int64_t, ImGuiDataType_S64)
                    HZ_FIELD_NUMBER(UByte, uint8_t, ImGuiDataType_U8)
                    HZ_FIELD_NUMBER(UShort, uint16_t, ImGuiDataType_U16)
                    HZ_FIELD_NUMBER(UInt, uint32_t, ImGuiDataType_U32)
                    HZ_FIELD_NUMBER(ULong, uint64_t, ImGuiDataType_U64)
#undef HZ_FIELD_NUMBER
                case ScriptFieldType::None:
                    break;
                }
                if (changed)
                    values[name] = value;
                {
                    PropertyUI::Row resetRow("default", "Override");
                    if (ImGui::SmallButton("Use C# default"))
                        values.erase(name);
                }
                ImGui::PopID();
            }
        });

    DrawUIComponent<SpriteRendererComponent>(
        "Sprite Renderer", entity, m_Sections,m_RestoreSections,
        [this](auto &component)
        {
            PropertyUI::Color("Color", "Color", glm::value_ptr(component.Color));

            SpriteSourceEditor(component, m_Pickers[ImGui::GetID("source-picker")], OpenAsset,
                               [this] { return CanEdit(); });
        });
    DrawComponent<SpriteAnimationComponent>(
        "Sprite Animation", entity, m_Sections,m_RestoreSections,
        [this](auto &component)
        {
            if (ClipPicker("Clip", component.DefaultClip, m_Pickers[ImGui::GetID("animation-picker")],
                           OpenAsset, [this] { return CanEdit(); }))
                component.ResetRuntime();
            PropertyUI::Checkbox("Autoplay", "Autoplay", component.Autoplay);
            if (PropertyUI::Double("speed", "Speed", component.Speed, .1))
                component.PreparedEpoch = 0;
            ImGui::TextWrapped(
                "Animation supplies the rendered sprite while assigned. Clearing/removing it "
                "restores the static sprite source. Speed zero holds the frame.");
            if (!component.Error.empty())
                ImGui::TextColored({1, .4f, .3f, 1}, "%s", component.Error.c_str());
        });

    DrawComponent<CircleRendererComponent>(
        "Circle Renderer", entity, m_Sections,m_RestoreSections,
        [](auto &component)
        {
            PropertyUI::Color("Color", "Color", glm::value_ptr(component.Color));
            PropertyUI::DragFloat("Thickness", "Thickness", component.Thickness, 0.025f, 0.0f, 1.0f);
            PropertyUI::DragFloat("Fade", "Fade", component.Fade, 0.00025f, 0.0f, 1.0f);
        });

    DrawComponent<AudioSourceComponent>("Audio Source", entity, m_Sections,m_RestoreSections,[](auto& source) {
        std::string clip=source.Clip.generic_u8string();
        if(PropertyUI::Text("Clip", "Project-relative WAV",clip))source.Clip=std::filesystem::u8path(clip);
        PropertyUI::Help("Select a WAV inside Assets. Gain is 0–1. Voices belong to this scene; Stop/scene transitions release them.");
        ImGui::SliderFloat("Gain",&source.Gain,0,1);
        ImGui::Checkbox("Loop",&source.Loop);ImGui::Checkbox("Play on start",&source.PlayOnStart);
    });
    DrawComponent<Rigidbody2DComponent>(
        "Rigidbody 2D", entity, m_Sections,m_RestoreSections,
        [](auto &component)
        {
            const Rigidbody2DComponent defaults;
            const char *bodyTypeStrings[] = {"Static", "Dynamic", "Kinematic"};
            const char *currentBodyTypeString = bodyTypeStrings[(int)component.Type];
            {
                PropertyUI::Row bodyRow("body-type", "Body Type");
                if (ImGui::BeginCombo("##value", currentBodyTypeString))
                {
                    for (int i = 0; i < 3; i++)
                    {
                        bool isSelected = currentBodyTypeString == bodyTypeStrings[i];
                        if (ImGui::Selectable(bodyTypeStrings[i], isSelected))
                        {
                            currentBodyTypeString = bodyTypeStrings[i];
                            component.Type = (Rigidbody2DComponent::BodyType)i;
                        }

                        if (isSelected)
                            ImGui::SetItemDefaultFocus();
                    }

                    ImGui::EndCombo();
                }
            }
            PropertyUI::Checkbox("Fixed Rotation", "Fixed Rotation", component.FixedRotation,
                                 &defaults.FixedRotation);
            PropertyUI::DragFloat("Gravity Scale", "Gravity Scale", component.GravityScale, 0.05f,
                                  -10.0f, 10.0f, "%.2f", &defaults.GravityScale);
        },[this](const std::string& error){HierarchyFailed(error);});

    DrawComponent<BoxCollider2DComponent>(
        "Box Collider 2D", entity, m_Sections,m_RestoreSections,
        [](auto &component)
        {
            const BoxCollider2DComponent defaults;
            PropertyUI::Vector("Offset", "Offset", glm::value_ptr(component.Offset), 2, .1f,
                               glm::value_ptr(defaults.Offset));
            PropertyUI::Vector("Size", "Size", glm::value_ptr(component.Size), 2, .1f,
                               glm::value_ptr(defaults.Size));
            PropertyUI::DragFloat("Density", "Density", component.Density, .01f, 0, 1, "%.3f",
                                  &defaults.Density);
            PropertyUI::DragFloat("Friction", "Friction", component.Friction, .01f, 0, 1, "%.3f",
                                  &defaults.Friction);
            PropertyUI::DragFloat("Restitution", "Restitution", component.Restitution, .01f, 0, 1,
                                  "%.3f", &defaults.Restitution);
            PropertyUI::DragFloat("Restitution Threshold", "Restitution Threshold",
                                  component.RestitutionThreshold, .01f, 0, 0, "%.3f",
                                  &defaults.RestitutionThreshold);
        },[this](const std::string& error){HierarchyFailed(error);});

    DrawComponent<CircleCollider2DComponent>(
        "Circle Collider 2D", entity, m_Sections,m_RestoreSections,
        [](auto &component)
        {
            const CircleCollider2DComponent defaults;
            PropertyUI::Vector("Offset", "Offset", glm::value_ptr(component.Offset), 2, .1f,
                               glm::value_ptr(defaults.Offset));
            PropertyUI::DragFloat("Radius", "Radius", component.Radius, .1f, 0, 0, "%.3f",
                                  &defaults.Radius);
            PropertyUI::DragFloat("Density", "Density", component.Density, .01f, 0, 1, "%.3f",
                                  &defaults.Density);
            PropertyUI::DragFloat("Friction", "Friction", component.Friction, .01f, 0, 1, "%.3f",
                                  &defaults.Friction);
            PropertyUI::DragFloat("Restitution", "Restitution", component.Restitution, .01f, 0, 1,
                                  "%.3f", &defaults.Restitution);
            PropertyUI::DragFloat("Restitution Threshold", "Restitution Threshold",
                                  component.RestitutionThreshold, .01f, 0, 0, "%.3f",
                                  &defaults.RestitutionThreshold);
        },[this](const std::string& error){HierarchyFailed(error);});

    DrawUIComponent<TextComponent>(
        "Text Renderer", entity, m_Sections,m_RestoreSections,
        [this,entity](auto &component) mutable
        {
            if(!m_UIFontInitialized || m_UIFontBound!=component.Font.Family){m_UIFontDraft=component.Font.Family.generic_u8string();m_UIFontBound=component.Font.Family;m_UIFontInitialized=true;}
            auto assign=[&](std::filesystem::path path){try {
                auto assets=m_Context->GetAssets();if(assets && path.is_absolute())path=Project::MakeAssetReference(assets->Root(),path);FontSelection selection{path,component.Font.Style};selection.Validate();
                if(!path.empty()) {if(!assets)throw std::runtime_error("Open a project before assigning a family");ReadFontFamily(Project::ResolveOwnedAsset(assets->Root(),path));}
                component.Font=selection;m_UIFontBound=path;m_UIFontDraft=path.generic_u8string();m_HierarchyError.clear();
            }catch(const std::exception& error){m_HierarchyError=error.what();}};
            auto edit=PropertyUI::Text("font","Font family",m_UIFontDraft);
            if(edit.Committed)assign(std::filesystem::u8path(m_UIFontDraft));
            if(ImGui::BeginDragDropTarget()){if(auto payload=ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM"))assign(ContentBrowserPath(payload->Data,payload->DataSize));ImGui::EndDragDropTarget();}
            PropertyUI::Help("Empty uses engine Open Sans. Create .hfont assets in Content Browser. Styles use real faces; an absent style uses Regular, never synthesis.");
            int face=int(component.Font.Style);if(PropertyUI::Combo("font-style","Face style",face,"Regular\0Bold\0Italic\0Bold Italic\0"))component.Font.Style=FontStyle(face);
            if(auto assets=m_Context->GetAssets()) {
                auto font=assets->FamilyStatus(component.Font);
                auto faceName=font.Face.lexically_relative(assets->Root());if(faceName.empty() || *faceName.begin()=="..")faceName=std::filesystem::path("[engine]")/font.Face.filename();
                PropertyUI::ReadOnly("actual-face","Resolved face",faceName.generic_u8string().c_str());
                if(font.StyleFallback)ImGui::TextWrapped("Requested face is absent; using Regular.");
                if(font.DefaultFallback)ImGui::TextWrapped("Using engine Regular fallback; authored selection retained.");
                if(!font.Error.empty())ImGui::TextWrapped("%s",font.Error.c_str());
                if(ImGui::TreeNode("Font details")) {
                    char description[128];std::snprintf(description,sizeof(description),"%u pages | %.1f KiB",font.Actual->PageCount(),double(font.Actual->GetAtlasBytes())/1024);
                    PropertyUI::ReadOnly("font-atlas","Pages (estimated)",description);
                    if(!font.Actual->PageError().empty())ImGui::TextWrapped("%s",font.Actual->PageError().c_str());
                    if(!font.Actual->TextWarning().empty())ImGui::TextWrapped("%s",font.Actual->TextWarning().c_str());
                    if(ImGui::SmallButton("Reload family"))assets->Reload(component.Font.Family);
                    if(!component.Font.Family.empty() && OpenAsset){ImGui::SameLine();if(ImGui::SmallButton("Edit family"))OpenAsset(Project::ResolveOwnedAsset(assets->Root(),component.Font.Family));}
                    ImGui::TextWrapped("Shaped Latin/Greek/Cyrillic/Arabic/Hebrew corpus; no cross-family fallback, CJK, color or vertical text. Inspect coverage in the family asset editor.");
                    ImGui::TreePop();
                }
            }
            auto p=m_Context->GetEntityByUUID(m_Context->GetRelationship(entity).Parent);
            if(p&&p.HasComponent<UIDropdownComponent>()&&p.GetComponent<UIDropdownComponent>().Trigger==uint64_t(entity.GetUUID()))PropertyUI::ReadOnly("text","Text","Computed from selection / placeholder");
            else PropertyUI::Multiline("text", "Text", component.TextString);
            PropertyUI::Color("Color", "Color", glm::value_ptr(component.Color));
            PropertyUI::DragFloat("Kerning", "Tracking", component.Kerning, 0.025f);
            PropertyUI::Help("Extra font-height gap per shaped cluster. OpenType kerning is already applied; Arabic joins retain their spacing.");
            PropertyUI::DragFloat("Line Spacing", "Line Spacing", component.LineSpacing, 0.025f);
        });

    m_UIInspectorEdit.Poll(*m_Context,m_History,entity.GetUUID(),CanEdit(),!ImGui::GetIO().AppFocusLost,ImGui::IsKeyPressed(ImGuiKey_Escape,false),ImGui::GetActiveID());
    if(!m_UIInspectorEdit.Error().empty())PropertyUI::Validation(m_UIInspectorEdit.Error().c_str());
    ImGui::EndDisabled();
    ImGui::PopID();
    ImGui::PopID();
}

template <typename T> void SceneHierarchyPanel::DisplayAddComponentEntry(const std::string &entryName)
{
    if (!m_SelectionContext.HasComponent<T>())
    {
        bool allowed=true;
        const char *unavailable="Unparent first: rigidbody and collider owners must be roots";
        if constexpr(std::is_same_v<T,Rigidbody2DComponent> || std::is_same_v<T,BoxCollider2DComponent> || std::is_same_v<T,CircleCollider2DComponent>)allowed=!uint64_t(m_Context->GetRelationship(m_SelectionContext).Parent);
        if constexpr(std::is_same_v<T,UINineSliceComponent>){
            allowed=m_SelectionContext.HasComponent<UIRectComponent>()&&m_SelectionContext.GetComponent<UIRectComponent>().Fit==GameUI::ImageFit::Stretch;
            unavailable="Choose Stretch in UI Rect before adding Nine Slice; it owns border fitting";
        }
        if (ImGui::MenuItem(entryName.c_str(),nullptr,false,allowed) && CanEdit(true))
        {
            try{FinishUIEdit();m_SelectionContext.AddComponent<T>();m_HierarchyError.clear();ImGui::CloseCurrentPopup();}
            catch(const std::exception& e){HierarchyFailed(e.what());}
        }
        if(!allowed)PropertyUI::Help(unavailable);
    }
}

} // namespace Hazel
