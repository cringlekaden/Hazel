#include "UIRectEditing.h"
#include "UIEditCommand.h"
#include <cmath>
namespace Hazel
{
namespace
{
bool Same(const UIRectComponent &a, const UIRectComponent &b)
{
    return a.AnchorMin == b.AnchorMin && a.AnchorMax == b.AnchorMax && a.Offset == b.Offset &&
           a.SizeDelta == b.SizeDelta && a.Pivot == b.Pivot && a.MinimumSize == b.MinimumSize &&
           a.Margin == b.Margin && a.Padding == b.Padding && a.Flex == b.Flex &&
           a.Visible == b.Visible && a.Enabled == b.Enabled && a.ClipChildren == b.ClipChildren;
}
} // namespace
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
        if (n->FlowOwned)
            throw std::invalid_argument(
                "Flow owns position/arranged size; edit preferred size and margins in Properties");
        if (edges > 15 || ((edges & 1) && (edges & 4)) || ((edges & 2) && (edges & 8)))
            throw std::invalid_argument("Unsupported resize edges");
        m_Scene = scene;
        m_Entity = entity;
        m_Identity = scene->GetIdentity();
        m_Parent = n->Parent;
        m_Edges = edges;
        m_Scale = n->Scale;
        m_ParentContent = n->ParentContent;
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
        if (!m_Scene->GetUISnapshot().Valid || !n || !n->LayoutResolved || n->FlowOwned || n->Parent != m_Parent ||
            n->Scale != m_Scale || n->ParentContent.Position != m_ParentContent.Position ||
            n->ParentContent.Size != m_ParentContent.Size)
        {
            m_Error = "UI parent/surface changed during gesture; cancelled";
            Finish(true);
            return false;
        }
        auto next = m_Initial;
        static_cast<GameUI::RectLayout &>(next) =
            GameUI::EditRect(m_ParentContent, m_Initial, delta, m_Edges);
        if(m_History) {
            auto command=dynamic_cast<UIEditCommand*>(m_History->Transaction());
            if(!command)throw std::runtime_error("Viewport document transaction was interrupted");
            auto values=UIEditValues::Capture(e);values.Set(next);
            if(!command->Preview(*m_Scene,std::move(values),m_Error))return false;
            m_Last=next;m_Error.clear();return true;
        }
        e.GetComponent<UIRectComponent>() = next;
        try
        {
            m_Scene->RefreshUI();
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
        if(!n||!n->LayoutResolved||n->FlowOwned||n->Parent!=m_Parent||n->Scale!=m_Scale||n->ParentContent.Position!=m_ParentContent.Position||n->ParentContent.Size!=m_ParentContent.Size)
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
            if (r.SizeDelta == m_Last.SizeDelta)
                r.SizeDelta = m_Initial.SizeDelta;
        }
    }
    m_Scene.reset();m_History=nullptr;
    m_Entity = m_Identity = m_Parent = 0;
}
} // namespace Hazel
