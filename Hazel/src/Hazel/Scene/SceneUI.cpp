#include "hzpch.h"
#include "Entity.h"
#include "Hazel/Project/Project.h"
#include "Hazel/UI/RuntimeUI.h"
#include "Hazel/UI/RuntimeUIConfig.h"
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
bool Scene::GetUIImageVisual(uint64_t id,GameUI::UIImageVisual &result) const {CheckThread();return m_UI->ImageVisual(id,result);}
void Scene::UpdateUIInput(const GameUI::InputFrame &frame)
{
    RefreshUI();
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
void Scene::ValidateUI(bool resources)
{
    ValidateUILayoutDependencies();
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
        }
        if (e.HasComponent<UITextStyleComponent>())
        {
            if (!e.HasComponent<TextComponent>() || !e.HasComponent<UIRectComponent>())
                throw std::runtime_error("UI text style needs Text and UI Rect");
            auto path = e.GetComponent<UITextStyleComponent>().FontPath;
            if (!path.empty())
            {
                if (!m_Assets)
                    throw std::runtime_error("UI font needs project assets");
                Font::ValidateFile(Project::ResolveOwnedAsset(m_Assets->Root(), path));
                if (resources)
                    m_Assets->FontAsset(path);
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
                auto validate = [&](const SpriteSource &source) {
                    auto sprite = m_Assets->Resolve(source);
                    auto borders = e.GetComponent<UINineSliceComponent>().Borders;
                    if (!sprite->Texture)
                        throw std::runtime_error("Nine-slice needs an image");
                    auto size =
                        sprite->PixelSize();
                    if (borders.x + borders.z > size.x || borders.y + borders.w > size.y)
                        throw std::runtime_error("Nine-slice borders exceed source");
                };
                validate(e.GetComponent<SpriteRendererComponent>().Source);
                if (e.HasComponent<UIButtonComponent>())
                    for (auto &source : e.GetComponent<UIButtonComponent>().Images)
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
    GameUI::MeasuredLayout validation;std::vector<GameUI::MeasureNode> nodes;
    for(auto handle:GetAllEntitiesWith<UIRectComponent>())
    {
        Entity root(handle,this);if(!IsEntityValid(root.GetUUID())||root.HasComponent<UICanvasComponent>())continue;
        auto parent=GetEntityByUUID(GetRelationship(root).Parent);
        if(parent && !parent.HasComponent<UICanvasComponent>() && parent.HasComponent<UIRectComponent>())continue;
        nodes.clear();
        std::function<void(Entity,size_t,size_t)> collect=[&](Entity e,size_t parentIndex,size_t depth){
            if(!e||!IsEntityValid(e.GetUUID())||e.HasComponent<UICanvasComponent>()||!e.HasComponent<UIRectComponent>())return;
            if(depth>=MaxDepth||nodes.size()>=GameUI::InputRouter::MaxButtons)throw std::invalid_argument("Measured UI dependency limit exceeded");
            size_t index=nodes.size();nodes.emplace_back();auto &n=nodes.back();n.ID=e.GetUUID();n.Name=e.GetName();n.Layout=e.GetComponent<UIRectComponent>();n.Parent=parentIndex;n.Container=e.HasComponent<UILayoutComponent>();
            if(n.Container)n.Flow=e.GetComponent<UILayoutComponent>();
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
