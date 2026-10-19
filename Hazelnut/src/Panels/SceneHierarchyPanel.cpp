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
        Entity parent=GetSelectedEntity();
        if(PrefabDocument&&!parent){auto roots=m_Context->GetChildren();if(!roots.empty())parent=m_Context->GetEntityByUUID(roots.front());}
        if(kind!=0&&(!parent||(!parent.HasComponent<UIRectComponent>()&&!parent.HasComponent<UICanvasComponent>()))) {
            auto canvas=m_Context->CreateEntity("UI Canvas");canvas.AddComponent<UICanvasComponent>();if(parent)m_Context->Reparent(canvas,parent,TransformPolicy::KeepLocal);parent=canvas;
        }
        const char* names[]={"UI Canvas","UI Panel","UI Text","UI Button","UI Stack","UI Grid","UI Wrap"};
        auto e=m_Context->CreateEntity(names[kind]);if(parent)m_Context->Reparent(e,parent,TransformPolicy::KeepLocal);
        if(kind==0)e.AddComponent<UICanvasComponent>();
        else {
            auto& rect=e.AddComponent<UIRectComponent>();rect.Offset={24,24};
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
        return SetSelectedEntity(e);
    }catch(const std::exception& error){return HierarchyFailed(error.what());}
}
void SceneHierarchyPanel::UIAddMenu(){
    if(ImGui::BeginMenu("Runtime UI")) {
        const char* names[]={"Canvas","Panel","Text","Button","Stack","Grid","Wrap"};
        for(int i=0;i<7;i++)if(ImGui::MenuItem(names[i]))AddUI(i);
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
    if(!entity.HasComponent<UIRectComponent>()&&!entity.HasComponent<UICanvasComponent>()){
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

    DrawUIComponent<UICanvasComponent>("UI Canvas",entity,m_Sections,m_RestoreSections,[](auto& c){
        PropertyUI::Vector("reference","Reference pixels",glm::value_ptr(c.ReferenceSize),2,1,nullptr,"%.0f",{},1,8192);
        PropertyUI::Vector("insets","Safe L/T/R/B (px)",glm::value_ptr(c.SafeInsets),4,1,nullptr,"%.0f",{},0,4096);
        PropertyUI::DragInt("order","Sort order",c.SortOrder);
        PropertyUI::Checkbox("visible","Visible",c.Visible);PropertyUI::Checkbox("enabled","Enabled",c.Enabled);PropertyUI::Checkbox("modal","Block gameplay",c.Modal);
        PropertyUI::Help("Uniform reference scale; extra aspect space extends the logical canvas. Higher sort order draws and receives input last. A modal canvas blocks gameplay and controls in other canvases.");
    });
    DrawUIComponent<UIRectComponent>("UI Rect",entity,m_Sections,m_RestoreSections,[this,entity](auto& c)mutable{
        auto parent=m_Context->GetEntityByUUID(m_Context->GetRelationship(entity).Parent);
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
                bool owned=flowOwned||mode==GameUI::SizeMode::Fill;
                if(!owned){
                PropertyUI::DragFloat("anchor","Anchor",c.Anchor[axis],.01f,0,1,"%.2f");
                PropertyUI::DragFloat("offset","Offset (px)",c.Offset[axis],1,-FLT_MAX,FLT_MAX,"%.1f");
                PropertyUI::DragFloat("pivot","Pivot",c.Pivot[axis],.01f,0,1,"%.2f");}
                if(owned)PropertyUI::Help(flowOwned?"The parent container owns position. Set child sizing/margins; positions are computed.":"Fill owns this axis's position and size. Margins set its insets; maximum can leave spare space.");
                if(resolved&&resolved->LayoutResolved)ImGui::TextDisabled("Available %.1f | measured %.1f | arranged %.1f",resolved->Available[axis],resolved->Measured[axis],resolved->Bounds.Size[axis]);
                ImGui::PopID();
            }
        }
        if(!flowOwned && c.WidthMode!=GameUI::SizeMode::Fill && c.HeightMode!=GameUI::SizeMode::Fill){
            int preset=0;if(PropertyUI::Combo("anchor-preset","Anchor preset",preset,"Custom\0Top left\0Top center\0Top right\0Center\0Bottom left\0Bottom center\0Bottom right\0")){
                const glm::vec2 anchors[]={{0,0},{0,0},{.5f,0},{1,0},{.5f,.5f},{0,1},{.5f,1},{1,1}};
                if(preset){c.Anchor=c.Pivot=anchors[preset];c.Offset={};}}
        }
        if(flowOwned && parent.GetComponent<UILayoutComponent>().Kind==GameUI::ContainerKind::Grid){
            bool automatic=c.GridRow<0;
            if(PropertyUI::Checkbox("auto-cell","Automatic cell",automatic)){c.GridRow=c.GridColumn=automatic?-1:0;}
            if(!automatic){PropertyUI::DragInt("row","Row",c.GridRow,1,0,63);PropertyUI::DragInt("column","Column",c.GridColumn,1,0,63);}
            int rows=int(c.RowSpan),cols=int(c.ColumnSpan);
            if(PropertyUI::DragInt("row-span","Row span",rows,1,1,64))c.RowSpan=unsigned(rows);
            if(PropertyUI::DragInt("column-span","Column span",cols,1,1,64))c.ColumnSpan=unsigned(cols);
        }
        PropertyUI::Vector("margin","Margin L/T/R/B",glm::value_ptr(c.Margin),4,1,nullptr,"%.0f",{},0,4096);
        PropertyUI::Vector("padding","Padding L/T/R/B",glm::value_ptr(c.Padding),4,1,nullptr,"%.0f",{},0,4096);
        PropertyUI::DragFloat("flex","Fill weight (0 = 1)",c.Flex,.1f,0,100);
        PropertyUI::Checkbox("visible","Visible",c.Visible);PropertyUI::Checkbox("enabled","Enabled",c.Enabled);PropertyUI::Checkbox("clip","Clip children",c.ClipChildren);
        PropertyUI::Help("Logical pixels, top-left origin. Drag a selected element in the viewport to move it; drag an edge/corner handle to resize. Escape cancels the gesture. Containers own child positions; Fill owns free positioning on its axis.");
    },[this](const std::string& error){m_HierarchyError=error;});
    DrawUIComponent<UILayoutComponent>("UI Container",entity,m_Sections,m_RestoreSections,[](auto& c){
        int kind=int(c.Kind),axis=int(c.Direction),main=int(c.Main),cross=int(c.Cross);
        if(PropertyUI::Combo("kind","Container",kind,"Stack\0Grid\0Wrap\0"))c.Kind=GameUI::ContainerKind(kind);
        if(c.Kind==GameUI::ContainerKind::Grid){
            PropertyUI::Vector("grid-gap","Column / row gaps",glm::value_ptr(c.GridGap),2,1,nullptr,"%.1f",{},0,4096);
            auto tracks=[](const char *name,std::vector<GameUI::Track> &values){
                if(!ImGui::TreeNode(name))return;
                int count=int(values.size());if(PropertyUI::DragInt("count","Tracks",count,1,1,64))values.resize(size_t(count));
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
        }else{
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
    DrawUIComponent<UITextStyleComponent>("UI Text Style",entity,m_Sections,m_RestoreSections,[this](auto& c){
        if(!m_UIFontInitialized||m_UIFontBound!=c.FontPath){m_UIFontDraft=c.FontPath.generic_u8string();m_UIFontBound=c.FontPath;m_UIFontInitialized=true;}
        auto assign=[&](std::filesystem::path path){try{if(!path.empty()){if(path.extension()!=".ttf"&&path.extension()!=".otf")throw std::runtime_error("Select a project TTF/OTF");auto root=m_Context->GetAssets();if(!root)throw std::runtime_error("Open a project before assigning a font");Font::ValidateFile(Project::ResolveOwnedAsset(root->Root(),path));}c.FontPath=path;m_UIFontBound=path;m_UIFontDraft=path.generic_u8string();m_HierarchyError.clear();}catch(const std::exception& error){m_HierarchyError=error.what();}};
        auto edit=PropertyUI::Text("font","Font asset",m_UIFontDraft);
        if(edit.Committed)assign(std::filesystem::u8path(m_UIFontDraft));
        if(ImGui::BeginDragDropTarget()){if(auto payload=ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM"))assign(ContentBrowserPath(payload->Data,payload->DataSize));ImGui::EndDragDropTarget();}
        PropertyUI::Help("Empty uses the shipped default. Drag a TTF/OTF from Assets. Use separate styled text children for mixed emphasis; no markup is parsed.");
        PropertyUI::DragFloat("size","Size (px)",c.Size,1,1,512);
        int horizontal=int(c.Horizontal),vertical=int(c.Vertical);
        if(PropertyUI::Combo("horizontal","Horizontal",horizontal,"Left\0Center\0Right\0"))c.Horizontal=GameUI::Align(horizontal);
        if(PropertyUI::Combo("vertical","Vertical",vertical,"Top\0Center\0Bottom\0"))c.Vertical=GameUI::Align(vertical);
        PropertyUI::Checkbox("wrap","Wrap",c.Wrap);
    });
    DrawUIComponent<UINineSliceComponent>("UI Nine Slice",entity,m_Sections,m_RestoreSections,[](auto& c){
        PropertyUI::Vector("border","Source L/T/R/B (px)",glm::value_ptr(c.Borders),4,1,nullptr,"%.0f",{},0,4096);
        PropertyUI::DragFloat("border-scale","Border scale",c.BorderScale,.1f,.01f,64);
        PropertyUI::Help("Borders must fit the source sprite region. Pixel borders retain their size; the middle stretches. Small targets clamp borders proportionally.");
    });
    DrawUIComponent<UIButtonComponent>("UI Button",entity,m_Sections,m_RestoreSections,[this](auto& c){
        PropertyUI::Text("action","Action identity",c.Action);PropertyUI::DragInt("tab","Tab order",c.TabOrder);PropertyUI::Checkbox("interactive","Interactable",c.Interactable);
        const char* states[]={"Normal","Hovered","Pressed","Disabled","Focused"};
        for(int i=0;i<7;i++){ImGui::PushID(i);PropertyUI::Color("tint",states[i],glm::value_ptr(c.Colors[i]));if(ImGui::TreeNode("State image (optional)")){SpriteRendererComponent temp;temp.Source=c.Images[i];temp.Color=c.Colors[i];SpriteSourceEditor(temp,m_UIStatePickers[i],OpenAsset,[this]{return CanEdit(true);});c.Images[i]=temp.Source;c.Colors[i]=temp.Color;ImGui::TreePop();}ImGui::PopID();}
        PropertyUI::Help("C#: UIButtonComponent.TryConsumeAction or GameUI.TryConsumeAction. Pointer activation occurs on release; Tab/arrows change focus; Enter/Space activate once.");
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
        [](auto &component)
        {
            PropertyUI::Multiline("text", "Text", component.TextString);
            PropertyUI::Color("Color", "Color", glm::value_ptr(component.Color));
            PropertyUI::DragFloat("Kerning", "Kerning", component.Kerning, 0.025f);
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
        if constexpr(std::is_same_v<T,Rigidbody2DComponent> || std::is_same_v<T,BoxCollider2DComponent> || std::is_same_v<T,CircleCollider2DComponent>)allowed=!uint64_t(m_Context->GetRelationship(m_SelectionContext).Parent);
        if (ImGui::MenuItem(entryName.c_str(),nullptr,false,allowed) && CanEdit(true))
        {
            try{FinishUIEdit();m_SelectionContext.AddComponent<T>();m_HierarchyError.clear();ImGui::CloseCurrentPopup();}
            catch(const std::exception& e){HierarchyFailed(e.what());}
        }
        if(!allowed)PropertyUI::Help("Unparent first: rigidbody and collider owners must be roots");
    }
}

} // namespace Hazel
