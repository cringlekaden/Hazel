#include "UIRectEditing.h"
#include "UIEditCommand.h"
#include <cmath>
namespace Hazel
{
namespace
{
bool Same(const UIRectComponent &a, const UIRectComponent &b)
{
    UIEditValues first,second;first.Set(a);second.Set(b);return first.Same(second);
}
} // namespace
void UIRectEditSession::Remember(const GameUI::ResolvedElement &n)
{
    m_ExpectedParentContent=n.ParentContent;m_ExpectedSlot=n.Slot;
    m_ExpectedRow=n.GridRow;m_ExpectedColumn=n.GridColumn;m_ExpectedLine=n.Line;
}
bool UIRectEditSession::Begin(const Ref<Scene> &scene, uint64_t entity, unsigned edges, DocumentHistory *history)
{
    Finish(true);
    m_Error.clear();
    try
    {
        if (!scene || scene->IsRunning() || scene->IsSimulating())
            throw std::invalid_argument("Stop Play / Simulate before editing UI");
        scene->RefreshUI();
        auto n = scene->GetUISnapshot().Find(entity);
        auto e = scene->GetEntityByUUID(entity);
        if (!scene->GetUISnapshot().Valid || !n || !n->LayoutResolved || n->IsCanvas || !e || !e.HasComponent<UIRectComponent>() ||
            n->Scale <= 0)
            throw std::invalid_argument("Selected rect has no resolved editable geometry");
        auto parent=scene->GetEntityByUUID(scene->GetRelationship(e).Parent);
        if(parent&&parent.HasComponent<UISliderComponent>()){
            auto &s=parent.GetComponent<UISliderComponent>();
            if(s.Thumb==entity||s.Fill==entity)throw std::invalid_argument("Slider owns this part's placement: edit thumb size or track bounds in Properties");
        }
        if (n->ContainerOwned && (!edges || (edges&3)))
            throw std::invalid_argument(
                "Container owns position: use Properties, or resize measured Fixed dimensions from right/bottom");
        if (edges > 15 || ((edges & 1) && (edges & 4)) || ((edges & 2) && (edges & 8)))
            throw std::invalid_argument("Unsupported resize edges");
        {
            if(!edges && (n->WidthMode==GameUI::SizeMode::Fill || n->HeightMode==GameUI::SizeMode::Fill))
                throw std::invalid_argument("Fill owns position; edit margins in Properties");
            if(((edges&5)&&n->WidthMode!=GameUI::SizeMode::Fixed)||((edges&10)&&n->HeightMode!=GameUI::SizeMode::Fixed))
                throw std::invalid_argument("Fill/Content owns this dimension; edit sizing in Properties");
        }
        m_Scene = scene;
        m_Entity = entity;
        m_Identity = scene->GetIdentity();
        m_Parent = n->Parent;
        m_Edges = edges;
        m_Scale = n->Scale;
        m_ParentContent = n->ParentContent;
        Remember(*n);m_ContainerOwned=n->ContainerOwned;
        m_InitialBounds = n->Bounds;
        m_Initial = m_Last = e.GetComponent<UIRectComponent>();
        if(history && (!history->Begin(*scene,CreateScope<UIEditCommand>(*scene,entity,edges?"Resize UI rect":"Move UI rect",UIEditValues::Capture(e))))) {
            m_Scene.reset();throw std::runtime_error(history->Diagnostic());
        }
        m_History=history;
        return true;
    }
    catch (const std::exception &error)
    {
        m_Error = error.what();
        return false;
    }
}
bool UIRectEditSession::Update(glm::vec2 delta)
{
    if (!m_Scene)
        return false;
    try
    {
        auto e = m_Scene->GetEntityByUUID(m_Entity);
        if (m_Scene->GetIdentity() != m_Identity || !m_Scene->IsEntityValid(m_Entity) ||
            !e.HasComponent<UIRectComponent>())
        {
            m_Error = "UI gesture target retired; cancelled";
            Finish(true);
            return false;
        }
        if (m_History) try {
            auto command = m_History->Transaction();
            if (!command) throw std::runtime_error("Viewport document transaction was interrupted");
            command->Check(*m_Scene);
        } catch (const std::exception &error) {
            m_Error = error.what();
            Finish(true);
            return false;
        }
        // Never overwrite another authoring edit during the gesture.
        if (!Same(e.GetComponent<UIRectComponent>(), m_Last))
        {
            m_Error = "UI rect changed outside this gesture; cancelled";
            Finish(true);
            return false;
        }
        m_Scene->RefreshUI();
        auto n = m_Scene->GetUISnapshot().Find(m_Entity);
        if (!m_Scene->GetUISnapshot().Valid || !n || !n->LayoutResolved || n->ContainerOwned!=m_ContainerOwned || n->Parent != m_Parent ||
            n->Scale != m_Scale || n->ParentContent.Position != m_ExpectedParentContent.Position ||
            n->ParentContent.Size != m_ExpectedParentContent.Size || n->Slot.Position!=m_ExpectedSlot.Position || n->Slot.Size!=m_ExpectedSlot.Size || n->GridRow!=m_ExpectedRow || n->GridColumn!=m_ExpectedColumn || n->Line!=m_ExpectedLine)
        {
            m_Error = "UI parent/surface changed during gesture; cancelled";
            Finish(true);
            return false;
        }
        auto next = m_Initial;
        static_cast<GameUI::RectLayout &>(next) =
            GameUI::EditMeasuredRect(m_ParentContent,m_InitialBounds,m_Initial,delta,m_Edges);
        if(m_ContainerOwned)next.Offset=m_Initial.Offset;
        auto checkResolvedGesture=[&]{
            auto resolved=m_Scene->GetUISnapshot().Find(m_Entity);
            if(!resolved||!resolved->LayoutResolved)throw std::invalid_argument("Layout no longer resolves this gesture target");
            const auto &bounds=resolved->Bounds;
            if(!m_Edges){
                if(glm::length(bounds.Position-m_InitialBounds.Position-delta)>.01f || glm::length(bounds.Size-m_InitialBounds.Size)>.01f)
                    throw std::invalid_argument("Parent reflow owns this move; edit Offset in Properties");
            }
            else for(int axis=0;axis<2;++axis){
                unsigned leading=axis?2:1,trailing=axis?8:4;
                if(!(m_Edges&(leading|trailing)))continue;
                float before=m_InitialBounds.Position[axis]+((m_Edges&leading)?m_InitialBounds.Size[axis]:0);
                float after=bounds.Position[axis]+((m_Edges&leading)?bounds.Size[axis]:0);
                if(std::abs(before-after)>.01f)throw std::invalid_argument("Parent reflow/alignment owns the opposite edge; edit Fixed size in Properties");
            }
        };
        if(m_History) {
            auto command=dynamic_cast<UIEditCommand*>(m_History->Transaction());
            if(!command)throw std::runtime_error("Viewport document transaction was interrupted");
            auto values=UIEditValues::Capture(e);values.Set(next);
            if(!command->Preview(*m_Scene,std::move(values),m_Error))return false;
            try{checkResolvedGesture();}catch(const std::exception &error){
                const std::string reason=error.what();auto restore=UIEditValues::Capture(e);restore.Set(m_Last);std::string rejected;
                if(!command->Preview(*m_Scene,std::move(restore),rejected))Finish(true);
                m_Error=reason;return false;
            }
            m_Last=next;Remember(*m_Scene->GetUISnapshot().Find(m_Entity));m_Error.clear();return true;
        }
        e.GetComponent<UIRectComponent>() = next;
        try
        {
            m_Scene->RefreshUI();
            checkResolvedGesture();
        }
        catch (...)
        {
            // A valid edited rect can still overflow a descendant's layout.
            // Publish no authored change unless the whole resolution succeeds.
            e.GetComponent<UIRectComponent>() = m_Last;
            try { m_Scene->RefreshUI(); } catch (...) {} // Keep authored rollback even on OOM.
            throw;
        }
        m_Last = next;
        Remember(*m_Scene->GetUISnapshot().Find(m_Entity));
        m_Error.clear();
        return true;
    }
    catch (const std::exception &error)
    {
        m_Error = error.what();
        return false;
    }
}
void UIRectEditSession::Finish(bool cancel)
{
    if(m_Scene && !cancel)try {
        auto e=m_Scene->GetEntityByUUID(m_Entity);
        if(!m_Scene->IsEntityValid(m_Entity)||!e.HasComponent<UIRectComponent>()||!Same(e.GetComponent<UIRectComponent>(),m_Last))
            throw std::runtime_error("UI gesture target changed before release; cancelled");
        m_Scene->RefreshUI();auto n=m_Scene->GetUISnapshot().Find(m_Entity);
        if(!n||!n->LayoutResolved||n->ContainerOwned!=m_ContainerOwned||n->Parent!=m_Parent||n->Scale!=m_Scale||n->ParentContent.Position!=m_ExpectedParentContent.Position||n->ParentContent.Size!=m_ExpectedParentContent.Size||n->Slot.Position!=m_ExpectedSlot.Position||n->Slot.Size!=m_ExpectedSlot.Size||n->GridRow!=m_ExpectedRow||n->GridColumn!=m_ExpectedColumn||n->Line!=m_ExpectedLine)
            throw std::runtime_error("UI parent/surface changed before release; cancelled");
    }catch(const std::exception &error){m_Error=error.what();cancel=true;}
    if(m_Scene && m_History) {
        if(cancel)m_History->Cancel(*m_Scene);
        else if(!m_History->Commit(*m_Scene))m_History->Cancel(*m_Scene);
        if(!m_History->Diagnostic().empty())m_Error=m_History->Diagnostic();
    }
    if (m_Scene && !m_History && cancel && m_Scene->GetIdentity() == m_Identity &&
        m_Scene->IsEntityValid(m_Entity))
    {
        auto e = m_Scene->GetEntityByUUID(m_Entity);
        if (e.HasComponent<UIRectComponent>())
        {
            auto &r = e.GetComponent<UIRectComponent>();
            if (r.Offset == m_Last.Offset)
                r.Offset = m_Initial.Offset;
            if(r.FixedSize==m_Last.FixedSize)r.FixedSize=m_Initial.FixedSize;
        }
    }
    m_Scene.reset();m_History=nullptr;
    m_Entity = m_Identity = m_Parent = 0;
}
} // namespace Hazel
