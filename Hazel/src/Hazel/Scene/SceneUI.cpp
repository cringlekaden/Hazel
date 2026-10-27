#include "hzpch.h"
#include "Entity.h"
#include "Hazel/Project/Project.h"
#include "Hazel/UI/RuntimeUI.h"
#include "Hazel/UI/RuntimeUIConfig.h"
#include "Hazel/UI/UIStyleApplication.h"
#include "Scene.h"
#include "Hazel/Utils/UTF8.h"
namespace Hazel
{
const std::vector<UUID> &Scene::GetChildrenView(UUID parent) const
{
    CheckThread();
    static const std::vector<UUID> empty;
    auto it = m_Children.find(parent);
    return it == m_Children.end() ? empty : it->second;
}
void Scene::RefreshUI()
{
    glm::vec2 viewport{m_ViewportWidth,m_ViewportHeight};
    m_UI->ResolveSurface(GameUI::SurfaceMetrics{viewport,m_UIFramebufferExplicit?m_UIFramebufferSize:viewport}, !m_IsRunning);
}
void Scene::SetUISizing(uint64_t id, GameUI::Axis axis, GameUI::SizeMode mode,
                        float fixedSize, float minimum, std::optional<float> maximum)
{
    CheckThread();auto e=GetEntityByUUID(id);
    if(IsStopping() || !e || !IsEntityValid(id) || !e.HasComponent<UIRectComponent>())
        throw std::invalid_argument("Sizing requires a live UI Rect");
    auto &r=e.GetComponent<UIRectComponent>();
    if(axis!=GameUI::Axis::Horizontal && axis!=GameUI::Axis::Vertical)throw std::invalid_argument("Invalid UI sizing axis");
    auto parent=GetEntityByUUID(GetRelationship(e).Parent);if(parent&&parent.HasComponent<UIDropdownComponent>()&&parent.GetComponent<UIDropdownComponent>().Popup==id)throw std::invalid_argument("Dropdown owns popup sizing; edit its panel limits");
    if(parent&&parent.HasComponent<UITooltipComponent>()&&parent.GetComponent<UITooltipComponent>().Body==id)throw std::invalid_argument("Tooltip placement is owned; edit body Fixed width and text style in Properties");
    const int a=axis==GameUI::Axis::Horizontal?0:1;auto before=r,next=r;
    (a?next.HeightMode:next.WidthMode)=mode;next.FixedSize[a]=fixedSize;
    next.MinimumSize[a]=minimum;next.HasMaximum[a]=maximum.has_value();next.MaximumSize[a]=maximum.value_or(0);
    GameUI::Validate(next);r=next;
    try{ValidateUILayoutDependencies();RefreshUI();}catch(...){r=before;try{RefreshUI();}catch(...){}throw;}
}
void Scene::SetUILayout(uint64_t id,const GameUI::FlowLayout &layout)
{
    CheckThread();auto e=GetEntityByUUID(id);
    if(IsStopping()||!e||!IsEntityValid(id)||!e.HasComponent<UILayoutComponent>()||!e.HasComponent<UIRectComponent>())
        throw std::invalid_argument("Container requires a live UI Rect and UI Layout");
    GameUI::Validate(layout);auto &live=e.GetComponent<UILayoutComponent>();auto candidate=live;
    static_cast<GameUI::FlowLayout&>(candidate)=layout;
    using std::swap;swap(live,candidate);
    try{ValidateUILayoutDependencies();RefreshUI();}catch(...){swap(live,candidate);try{RefreshUI();}catch(...){}throw;}
}
void Scene::SetUIGridCell(uint64_t id,int row,int column,unsigned rowSpan,unsigned columnSpan)
{
    CheckThread();auto e=GetEntityByUUID(id);
    if(IsStopping()||!e||!IsEntityValid(id)||!e.HasComponent<UIRectComponent>())throw std::invalid_argument("Cell requires a live UI Rect");
    auto &live=e.GetComponent<UIRectComponent>();auto before=live,next=live;
    next.GridRow=row;next.GridColumn=column;next.RowSpan=rowSpan;next.ColumnSpan=columnSpan;
    GameUI::Validate(next);live=next;
    try{ValidateUILayoutDependencies();RefreshUI();}catch(...){live=before;try{RefreshUI();}catch(...){}throw;}
}
void Scene::SetUIImageFit(uint64_t id,GameUI::ImageFit fit,glm::vec2 alignment)
{
    CheckThread();auto e=GetEntityByUUID(id);
    if(IsStopping()||!e||!IsEntityValid(id)||!e.HasComponent<UIRectComponent>())throw std::invalid_argument("Image fit needs a live UI Rect");
    GameUI::FitImage({}, {1,1},fit,alignment);
    if(e.HasComponent<UINineSliceComponent>()&&fit!=GameUI::ImageFit::Stretch)throw std::invalid_argument("Nine-slice requires Stretch image fit");
    auto &r=e.GetComponent<UIRectComponent>();r.Fit=fit;r.ImageAlignment=alignment;
}
Ref<const Text::Layout> Scene::GetUITextLayout(uint64_t id) const {CheckThread();return m_UI->TextLayout(id);}
bool Scene::GetUIImageVisual(uint64_t id,GameUI::UIImageVisual &result) const {CheckThread();return m_UI->ImageVisual(id,result);}
void Scene::UpdateUIInput(const GameUI::InputFrame &frame)
{
    RefreshUI();if(m_UI->CommitScreens())RefreshUI();
    m_UI->UpdateInput(frame);
}
void Scene::RenderUI()
{
    try
    {
        RefreshUI();
        m_UI->Draw();
        m_UIError.clear();
    }
    catch (const std::exception &error)
    {
        if (m_IsRunning)
            throw;
        m_UIError = error.what();
    }
}
bool Scene::ConsumeUIAction(uint64_t entity, GameUI::Action &action)
{
    CheckThread();
    return m_UI->Consume(entity, action);
}
bool Scene::RequestUIScreen(GameUI::ScreenOperation operation,uint64_t screen){CheckThread();return m_UI->RequestScreen(operation,screen);}
void Scene::CommitUIScreens(){CheckThread();if(m_UIScriptCallbacks)throw std::logic_error("Screen commits cannot run inside script callbacks; queue RequestUIScreen instead");if(m_UI->HasScreenRequests()){RefreshUI();if(m_UI->CommitScreens())RefreshUI();}}
bool Scene::IsUIScreenOpen(uint64_t screen) const {CheckThread();return m_UI->IsScreenOpen(screen);}
void Scene::SetUIPreviewTarget(uint64_t selected){
    CheckThread();uint64_t screen=0;auto e=GetEntityByUUID(selected);
    for(size_t depth=0;!m_IsRunning&&e&&IsEntityValid(e.GetUUID())&&depth<MaxDepth;++depth){
        if(e.HasComponent<UICanvasComponent>()){if(e.GetComponent<UICanvasComponent>().Screen.Managed)screen=e.GetUUID();break;}
        e=GetEntityByUUID(GetRelationship(e).Parent);
    }
    m_UI->SetPreviewScreen(screen);
}
bool Scene::FocusUI(uint64_t target){CheckThread();RefreshUI();return m_UI->Focus(target);}
bool Scene::ConsumeUIEvent(uint64_t owner,GameUI::Action &event){CheckThread();return m_UI->ConsumeEvent(owner,event);}
bool Scene::ConsumeUICancel(uint64_t scope){CheckThread();return m_UI->ConsumeCancel(scope);}
bool Scene::IsGameplayInputBlocked() const
{
    return m_UI->BlocksGameplay();
}
bool Scene::IsUICapturingPointer() const {return m_UI->CapturesPointer();}
bool Scene::IsGameplayPointerBlocked() const
{
    return m_UI->BlocksPointer();
}
bool Scene::GetUIBounds(uint64_t entity, GameUI::Rect &pixels) const
{
    return m_UI->Bounds(entity, pixels);
}
void Scene::ResetUI()
{
    m_UI->Reset();
}
void Scene::RetireUI(uint64_t id)
{
    m_UI->Retire(id);
}
void Scene::RetireUISubtree(uint64_t id)
{
    auto e=GetEntityByUUID(id);
    if(e && IsEntityValid(id))for(auto child:GetSubtree(e))m_UI->Retire(child);
    else m_UI->Retire(id);
}
void Scene::CancelUIInput() { CheckThread();m_UI->CancelInput(); }
void Scene::SetUIFramebufferSize(glm::vec2 size)
{
    CheckThread();m_UIFramebufferSize=size;m_UIFramebufferExplicit=true;
    if(!size.x||!size.y)m_UI->CancelInput();
}
const GameUI::ResolvedSnapshot& Scene::GetUISnapshot() const { CheckThread();return m_UI->Snapshot(); }
GameUI::RuntimeUIDiagnostics Scene::GetUIDiagnostics() const { CheckThread();return m_UI->Diagnostics(); }
bool Scene::IsUIChecked(uint64_t id){CheckThread();RefreshUI();return m_UI->Checked(id);}
void Scene::SetUIChecked(uint64_t id,bool value){CheckThread();RefreshUI();m_UI->SetChecked(id,value);}
void Scene::PreviewUITab(uint64_t group,uint64_t tab){CheckThread();RefreshUI();m_UI->PreviewTab(group,tab);RefreshUI();}
void Scene::PreviewUITooltip(uint64_t source,bool open){CheckThread();RefreshUI();m_UI->PreviewTooltip(source,open);RefreshUI();}
bool Scene::RequestUIDropdown(uint64_t id,bool open){CheckThread();RefreshUI();return m_UI->RequestDropdown(id,open);}
bool Scene::IsUIDropdownOpen(uint64_t id) const {CheckThread();return m_UI->DropdownOpen(id);}
void Scene::PreviewUIDropdown(uint64_t id,bool open){CheckThread();RefreshUI();m_UI->PreviewDropdown(id,open);RefreshUI();}
uint64_t Scene::GetUISelection(uint64_t id){CheckThread();RefreshUI();return m_UI->Selection(id);}
void Scene::SetUISelection(uint64_t id,uint64_t member){CheckThread();RefreshUI();m_UI->Select(id,member);}
GameUI::ScrollGeometry Scene::GetUIScroll(uint64_t id){CheckThread();RefreshUI();return m_UI->ScrollInfo(id);}
void Scene::ScrollUITo(uint64_t id,glm::vec2 offset,bool normalized){CheckThread();RefreshUI();m_UI->ScrollTo(id,offset,normalized);}
bool Scene::EnsureUIVisible(uint64_t view,uint64_t target,bool focus){CheckThread();RefreshUI();return m_UI->EnsureVisible(view,target,focus);}
float Scene::GetUISliderValue(uint64_t id){CheckThread();RefreshUI();return m_UI->SliderValue(id);}
void Scene::SetUISliderValue(uint64_t id,float value){CheckThread();RefreshUI();m_UI->SetSliderValue(id,value);}
bool Scene::IsUISliderEditing() const {CheckThread();return m_UI->SliderEditing();}
float Scene::GetUIProgress(uint64_t id){CheckThread();RefreshUI();return m_UI->Progress(id);}
void Scene::SetUIProgress(uint64_t id,float value){CheckThread();RefreshUI();m_UI->SetProgress(id,value);}
void Scene::ValidateUIControls(uint64_t target) const {m_UI->ValidateControls(target);}
bool Scene::GetUIControlVisual(uint64_t id,GameUI::Rect &bounds,bool &visible) const {CheckThread();return m_UI->ControlVisual(id,bounds,visible);}
void Scene::ValidateUI(bool resources,bool controls)
{
    // Partial document staging retains repairable presentation references.
    // Full Play/export validation requires all declared style resources.
    m_UI->ValidateStyles(resources && controls);
    ValidateUILayoutDependencies();if(controls)ValidateUIControls();
    size_t screens=0;for(auto h:m_Registry.view<UICanvasComponent>())if(m_Registry.get<UICanvasComponent>(h).Screen.Managed)++screens;
    if(screens>RuntimeUI::MaxScreens)throw std::runtime_error("UI managed screen limit exceeded (64)");
    if(m_Registry.view<UIRectComponent>().size()>GameUI::InputRouter::MaxButtons)
        throw std::runtime_error("UI declared rect limit exceeded (4096, including hidden rects)");
    for (auto h : m_Registry.view<IDComponent>())
    {
        Entity e(h, this);
        if (!IsEntityValid(e.GetUUID()))
            continue;
        if (!e.HasComponent<UICanvasComponent>() && !e.HasComponent<UIRectComponent>() &&
            !e.HasComponent<UILayoutComponent>() && !e.HasComponent<UITextStyleComponent>() &&
            !e.HasComponent<UIButtonComponent>() && !e.HasComponent<UINineSliceComponent>())
            continue;
        if(e.HasComponent<UICanvasComponent>()&&e.HasComponent<UIRectComponent>())
            throw std::invalid_argument("Canvas owns its surface; measured sizing belongs on a child Rect");
        ValidateUIConfiguration(YAML::Load(UIAuthoredState(e)));
        if(e.HasComponent<TextComponent>() && e.HasComponent<UIRectComponent>())
        {
            const auto& text=e.GetComponent<TextComponent>().TextString;
            if(text.size()>65536)throw std::invalid_argument("UI text exceeds 64 KiB");
            UTF8::Validate(text);
            const auto& spans=e.GetComponent<TextComponent>().Spans;
            (void)Text::NormalizeSpans(text,spans);
            if(e.HasComponent<UITextEntryComponent>()&&!spans.empty())throw std::invalid_argument("Runtime text entry is plain; remove display emphasis");
            auto parent=GetEntityByUUID(GetRelationship(e).Parent);
            if(parent&&parent.HasComponent<UIDropdownComponent>()&&parent.GetComponent<UIDropdownComponent>().Trigger==uint64_t(e.GetUUID())&&!spans.empty())throw std::invalid_argument("Computed dropdown captions are plain; put emphasis on option labels");
        }
        if (e.HasComponent<UITextStyleComponent>())
        {
            if (!e.HasComponent<TextComponent>() || !e.HasComponent<UIRectComponent>())
                throw std::runtime_error("UI text style needs Text and UI Rect");
        }
        if(e.HasComponent<TextComponent>()) {
            auto& text=e.GetComponent<TextComponent>();text.Font.Validate();
            if(!text.Font.Family.empty()) {
                if(!m_Assets)throw std::runtime_error("Text family needs project assets");
                (void)Project::ResolveOwnedAsset(m_Assets->Root(),text.Font.Family);
                if(resources)ResolveTextFont(text,m_Assets);
            }
        }
        auto validateSource = [&](const SpriteSource &source) {
            if (!m_Assets)
                return;
            if (auto image = std::get_if<TextureSpriteSource>(&source);
                image && !image->Texture.empty())
                Project::ResolveOwnedAsset(m_Assets->Root(), image->Texture);
            if (auto image = std::get_if<SpriteReference>(&source); image && image->Region)
                Project::ResolveOwnedAsset(m_Assets->Root(), image->Sheet);
        };
        if (e.HasComponent<UIRectComponent>() && e.HasComponent<SpriteRendererComponent>())
            validateSource(e.GetComponent<SpriteRendererComponent>().Source);
        if (e.HasComponent<UIButtonComponent>())
        {
            if (!e.HasComponent<UIRectComponent>())
                throw std::runtime_error("UI button needs UI Rect");
            for (auto &source : e.GetComponent<UIButtonComponent>().Images)
            {
                validateSource(source);
                if (resources && m_Assets && source.index())
                    m_Assets->Resolve(source);
            }
        }
        if (e.HasComponent<UINineSliceComponent>())
        {
            if (!e.HasComponent<SpriteRendererComponent>() || !e.HasComponent<UIRectComponent>())
                throw std::runtime_error("Nine-slice needs UI Rect and Sprite Renderer");
            if (resources && m_Assets)
            {
                auto appearance = LocalUIStyle(e);
                if(e.HasComponent<UIAppearanceComponent>() && e.GetComponent<UIAppearanceComponent>().Styled) {
                    Ref<const UIStyleDefinition> accepted;std::string error;
                    appearance = ResolveUIStyle(e,m_Assets,accepted,error);
                }
                auto validate = [&](const SpriteSource &source) {
                    auto sprite = m_Assets->Resolve(source);
                    auto borders = appearance.Slice.Borders;
                    if (!sprite->Texture)
                        throw std::runtime_error("Nine-slice needs an image");
                    auto size =
                        sprite->PixelSize();
                    if (borders.x + borders.z > size.x || borders.y + borders.w > size.y)
                        throw std::runtime_error("Nine-slice borders exceed source");
                };
                validate(appearance.Image);
                if (e.HasComponent<UIButtonComponent>())
                    for (auto &source : appearance.States.Images)
                        if (source.index())
                            validate(source);
            }
        }
    }
}
void Scene::ValidateUILayoutDependencies()
{
    CheckThread();
    if(m_Registry.view<UIRectComponent>().size()>GameUI::InputRouter::MaxButtons)
        throw std::invalid_argument("UI declared rect limit exceeded (4096, including hidden rects)");
    // Geometry from this probe is deliberately discarded. Zero visual metrics
    // and definite external space exercise only authored dependency structure;
    // actual text/images are measured by RuntimeUI at the real surface boundary.
    std::unordered_set<uint64_t> popups;for(auto h:GetAllEntitiesWith<UIDropdownComponent>())popups.insert(m_Registry.get<UIDropdownComponent>(h).Popup);for(auto h:GetAllEntitiesWith<UITooltipComponent>())popups.insert(m_Registry.get<UITooltipComponent>(h).Body);
    GameUI::MeasuredLayout validation;std::vector<GameUI::MeasureNode> nodes;
    for(auto handle:GetAllEntitiesWith<UIRectComponent>())
    {
        Entity root(handle,this);if(!IsEntityValid(root.GetUUID())||root.HasComponent<UICanvasComponent>())continue;
        auto parent=GetEntityByUUID(GetRelationship(root).Parent);
        if(parent && !parent.HasComponent<UICanvasComponent>() && parent.HasComponent<UIRectComponent>()&&!popups.count(root.GetUUID()))continue;
        nodes.clear();
        std::function<void(Entity,size_t,size_t)> collect=[&](Entity e,size_t parentIndex,size_t depth){
            if(!e||!IsEntityValid(e.GetUUID())||e.HasComponent<UICanvasComponent>()||!e.HasComponent<UIRectComponent>())return;
            if(depth&&popups.count(e.GetUUID()))return;
            if(depth>=MaxDepth||nodes.size()>=GameUI::InputRouter::MaxButtons)throw std::invalid_argument("Measured UI dependency limit exceeded");
            size_t index=nodes.size();nodes.emplace_back();auto &n=nodes.back();n.ID=e.GetUUID();n.Name=e.GetName();n.Layout=e.GetComponent<UIRectComponent>();n.Parent=parentIndex;n.Container=e.HasComponent<UILayoutComponent>();
            if(n.Container)n.Flow=e.GetComponent<UILayoutComponent>();
            auto scrollParent=GetEntityByUUID(GetRelationship(e).Parent);
            if(scrollParent&&scrollParent.HasComponent<UIScrollViewComponent>()){const auto &v=scrollParent.GetComponent<UIScrollViewComponent>();if(v.Content==uint64_t(e.GetUUID()))n.Unbounded={v.Horizontal,v.Vertical};}
            n.Weight=e.GetComponent<UIRectComponent>().Flex>0?e.GetComponent<UIRectComponent>().Flex:1;
            if(parentIndex!=std::numeric_limits<size_t>::max())nodes[parentIndex].Children.push_back(index);
            for(auto id:GetChildrenView(e.GetUUID()))collect(GetEntityByUUID(id),index,depth+1);
        };
        collect(root,std::numeric_limits<size_t>::max(),0);
        try{validation.Resolve(nodes,{},[](size_t,float){return glm::vec2(0);});}
        catch(const std::exception &error){throw std::invalid_argument("Scene '"+GetName()+"' #"+std::to_string(GetIdentity())+": "+error.what());}
    }
}
} // namespace Hazel

namespace Hazel {
std::string Scene::GetUITextEntry(uint64_t id){CheckThread();RefreshUI();return m_UI->EntryValue(id);}
void Scene::SetUITextEntry(uint64_t id,const std::string& value){CheckThread();RefreshUI();m_UI->SetEntryValue(id,value);}
bool Scene::IsUITextInputOwned() const {CheckThread();return m_UI->TextOwnsInput();}
bool Scene::IsUITextEditing() const {CheckThread();return m_UI->TextEditing();}
void Scene::SetUIClipboard(Ref<Text::Clipboard> clipboard){CheckThread();m_UI->SetClipboard(std::move(clipboard));}
}

namespace Hazel {
UIStyleValues Scene::GetUIPresentation(uint64_t id){CheckThread();RefreshUI();return m_UI->Presentation(id);}
UIStyleSelection Scene::GetUIStyle(uint64_t id) {CheckThread();auto e=GetEntityByUUID(id);if(!e||!IsEntityValid(id)||!e.HasComponent<UIAppearanceComponent>())throw std::invalid_argument("Style selection needs a live UI Appearance component");return e.GetComponent<UIAppearanceComponent>().Selection;}
void Scene::SetUIStyle(uint64_t id,const UIStyleSelection& selection,bool enabled){
 CheckThread();auto e=GetEntityByUUID(id);if(IsStopping()||!e||!IsEntityValid(id)||!e.HasComponent<UIRectComponent>())throw std::invalid_argument("Style selection needs a live UI Rect");
 selection.Validate();if(enabled){if(m_Assets){auto result=m_Assets->Style(selection.Asset);if(!result.Accepted||!result.Error.empty())throw std::invalid_argument(result.Error);result.Accepted->Find(selection.Variant);}else if(selection.Asset.empty())DefaultUIStyle().Find(selection.Variant);else throw std::invalid_argument("Style requires project assets");}
 if(enabled && e.HasComponent<UINineSliceComponent>()) {
     auto resolved=m_Assets?m_Assets->Style(selection.Asset).Accepted->Find(selection.Variant):DefaultUIStyle().Find(selection.Variant);
     CopyUIStyleGroups(resolved,LocalUIStyle(e),e.HasComponent<UIAppearanceComponent>()?e.GetComponent<UIAppearanceComponent>().Overrides:0);
     if(e.GetComponent<UIRectComponent>().Fit!=GameUI::ImageFit::Stretch)throw std::invalid_argument("Styled nine-slice requires Stretch fit");
     auto validate=[&](const SpriteSource& source){glm::vec2 size{};
         if(m_Assets){auto image=m_Assets->Resolve(source);if(image->Texture)size=image->PixelSize();}
         else if(auto image=std::get_if<TextureSpriteSource>(&source);image&&image->Resource)size={image->Resource->GetWidth(),image->Resource->GetHeight()};
         auto b=resolved.Slice.Borders;if(size.x<=0||size.y<=0||b.x+b.z>size.x||b.y+b.w>size.y)throw std::invalid_argument("Styled nine-slice needs an image with room for its source borders");
     };validate(resolved.Image);if(e.HasComponent<UIButtonComponent>())for(auto& image:resolved.States.Images)if(image.index())validate(image);
 }
 const bool added=!e.HasComponent<UIAppearanceComponent>();
 if(added)e.AddComponent<UIAppearanceComponent>();
 auto& c=e.GetComponent<UIAppearanceComponent>();auto before=c;c.Selection=selection;c.Styled=enabled;
 try{RefreshUI();}catch(...){if(added)e.RemoveComponent<UIAppearanceComponent>();else c=before;try{RefreshUI();}catch(...){}throw;}
}
void Scene::ResetUIStyleOverrides(uint64_t id,uint32_t groups){CheckThread();auto e=GetEntityByUUID(id);if(IsStopping()||!e||!IsEntityValid(id)||!e.HasComponent<UIAppearanceComponent>()||(groups&~UIStyleAll))throw std::invalid_argument("Invalid UI style reset target/groups");auto& c=e.GetComponent<UIAppearanceComponent>();auto before=c.Overrides;c.Overrides&=~groups;try{RefreshUI();}catch(...){c.Overrides=before;try{RefreshUI();}catch(...){}throw;}}
}

namespace Hazel {
void Scene::SetDisplayText(uint64_t id,const std::string& source,const std::vector<Text::Span>& input) {
    CheckThread();auto e=GetEntityByUUID(id);
    if(IsStopping()||!e||!IsEntityValid(id)||!e.HasComponent<TextComponent>())throw std::invalid_argument("Display text needs a live scene Text component");
    auto spans=Text::NormalizeSpans(source,input);
    if(e.HasComponent<UITextEntryComponent>()) {
        if(!spans.empty())throw std::invalid_argument("Runtime text entry is plain; remove display emphasis");
        SetUITextEntry(id,source);return;
    }
    auto parent=GetEntityByUUID(GetRelationship(e).Parent);
    if(parent&&parent.HasComponent<UIDropdownComponent>()&&parent.GetComponent<UIDropdownComponent>().Trigger==id&&!spans.empty())throw std::invalid_argument("Computed dropdown captions are plain; put emphasis on option labels");
    auto candidate=e.GetComponent<TextComponent>();candidate.TextString=source;candidate.Spans=std::move(spans);
    auto effective=candidate;
    if(e.HasComponent<UIRectComponent>()){
        Ref<const UIStyleDefinition> definition;std::string error;
        try{ApplyUIStyle(effective,ResolveUIStyle(e,m_Assets,definition,error));}
        catch(const std::exception&){ApplyUIStyle(effective,m_UI->Presentation(id));}
    }
    const auto& palette=ResolveTextFonts(effective,m_Assets,candidate.Spans);
    (void)palette[0]->Layout(source,-1,false,effective.Kerning,effective.LineSpacing,Text::Overflow::Clip,0,-1,TextFaces(palette),candidate.Spans);
    auto& accepted=e.GetComponent<TextComponent>();accepted.TextString=std::move(candidate.TextString);accepted.Spans=std::move(candidate.Spans);
}
}
