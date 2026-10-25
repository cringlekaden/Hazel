#include "RuntimeUI.h"
#include "hzpch.h"
#include <cmath>

namespace Hazel
{
using namespace GameUI;
uint64_t RuntimeUI::CanvasOf(uint64_t id) const
{
    auto e = m_Scene.GetEntityByUUID(id);
    for (size_t depth = 0; e && m_Scene.IsEntityValid(e.GetUUID()) && depth < Scene::MaxDepth; ++depth)
    {
        if (e.HasComponent<UICanvasComponent>())
            return e.GetUUID();
        e = m_Scene.GetEntityByUUID(m_Scene.GetRelationship(e).Parent);
    }
    return 0;
}
bool RuntimeUI::Member(uint64_t group, uint64_t id, bool eligible) const
{
    auto e = m_Scene.GetEntityByUUID(id), g = m_Scene.GetEntityByUUID(group);
    if (!e || !g || !g.HasComponent<UIRadioGroupComponent>() || !e.HasComponent<UIButtonComponent>())
        return false;
    const auto &b = e.GetComponent<UIButtonComponent>();
    if (b.Kind != ControlKind::Radio || b.Group != group || CanvasOf(group) != CanvasOf(id))
        return false;
    if(g.HasComponent<UIDropdownComponent>()){
        auto popup=m_Scene.GetEntityByUUID(g.GetComponent<UIDropdownComponent>().Popup);
        if(!popup||!popup.HasComponent<UIScrollViewComponent>()||m_Scene.GetRelationship(e).Parent!=UUID(popup.GetComponent<UIScrollViewComponent>().Content))return false;
    }
    if (eligible && !b.Interactable)
        return false;
    bool descendant = false;
    for (size_t depth = 0; e && depth < Scene::MaxDepth; ++depth)
    {
        if (eligible)
        {
            if (e.HasComponent<UIRectComponent>())
            {
                auto &r = e.GetComponent<UIRectComponent>();
                if (!r.Visible || !r.Enabled)
                    return false;
            }
            if (e.HasComponent<UICanvasComponent>())
            {
                auto &c = e.GetComponent<UICanvasComponent>();
                if (!c.Visible || !c.Enabled)
                    return false;
            }
        }
        if (uint64_t(e.GetUUID()) == group)
            descendant = true;
        e = m_Scene.GetEntityByUUID(m_Scene.GetRelationship(e).Parent);
    }
    return descendant;
}
bool RuntimeUI::Decoration(uint64_t owner, uint64_t visual) const
{
    auto e = m_Scene.GetEntityByUUID(visual);
    return e && m_Scene.IsEntityValid(visual) && m_Scene.GetRelationship(e).Parent == UUID(owner) &&
           e.HasComponent<UIRectComponent>() && e.HasComponent<SpriteRendererComponent>() &&
           !e.HasComponent<UIButtonComponent>() && !e.HasComponent<UIProgressComponent>() &&
           !e.HasComponent<UIRadioGroupComponent>() && !e.HasComponent<UISliderComponent>() && !e.HasComponent<UIScrollViewComponent>() && !e.HasComponent<UIDropdownComponent>() && !e.HasComponent<TextComponent>() &&
           !e.HasComponent<UICanvasComponent>() && m_Scene.GetChildrenView(visual).empty();
}
void RuntimeUI::ValidateControls(uint64_t target) const
{
    ValidateScrollViews(target);ValidateDropdowns(target);
    std::unordered_set<uint64_t> visuals;
    auto visual = [&](uint64_t owner, uint64_t id) {
        if (id && (!Decoration(owner, id) || !visuals.insert(id).second))
            throw std::invalid_argument(
                "UI control #" + std::to_string(owner) +
                ": visual must be a unique direct child Sprite/Rect leaf without interaction or text");
    };
    for (auto h : m_Scene.GetAllEntitiesWith<UIButtonComponent>())
    {
        Entity e(h, &m_Scene);
        if (!m_Scene.IsEntityValid(e.GetUUID()))
            continue;
        if (target && uint64_t(e.GetUUID()) != target)
            continue;
        auto &b = e.GetComponent<UIButtonComponent>();
        if (unsigned(b.Kind) > 3)
            throw std::invalid_argument("Invalid UI control kind");
        if (b.Kind != ControlKind::Button)
        {
            if (!e.HasComponent<UIRectComponent>())
                throw std::invalid_argument("Value control needs a Rect");
            visual(e.GetUUID(), b.Indicator);
            if (b.Kind == ControlKind::Radio && !Member(b.Group, e.GetUUID(), false))
                throw std::invalid_argument("Radio option #" + std::to_string(uint64_t(e.GetUUID())) +
                                            ": group must be an ancestor in the same Canvas");
        }
        if (b.Kind == ControlKind::Radio && b.EventOwner)
            throw std::invalid_argument("Radio events use the group/screen owner, not an option owner");
        if(b.Kind==ControlKind::Slider&&!e.HasComponent<UISliderComponent>())throw std::invalid_argument("Slider behavior needs UI Slider configuration");
        if (b.Kind != ControlKind::Radio && b.Group)
            throw std::invalid_argument("Only radio options can reference a group");
        if (b.EventOwner && !m_Scene.IsEntityValid(b.EventOwner))
            throw std::invalid_argument("Value control event owner is missing");
    }
    for (auto h : m_Scene.GetAllEntitiesWith<UIRadioGroupComponent>())
    {
        Entity e(h, &m_Scene);
        auto &g = e.GetComponent<UIRadioGroupComponent>();
        if (target && uint64_t(e.GetUUID()) != target)
        {
            if (g.InitialSelection == target && !Member(e.GetUUID(), target, false))
                throw std::invalid_argument("Radio initial selection would leave its group; change the "
                                            "group's initial target first");
            continue;
        }
        if (!e.HasComponent<UIRectComponent>() || e.HasComponent<UIButtonComponent>())
            throw std::invalid_argument("Radio group needs a noninteractive Rect");
        if (g.Action.size() > 128 || g.Action.find('\0') != std::string::npos)
            throw std::invalid_argument("Invalid radio group action (128 bytes, no NUL)");
        if (g.InitialSelection && !Member(e.GetUUID(), g.InitialSelection, false))
            throw std::invalid_argument("Radio group #" + std::to_string(uint64_t(e.GetUUID())) +
                                        ": initial selection is not a member");
        if (g.EventOwner && !m_Scene.IsEntityValid(g.EventOwner))
            throw std::invalid_argument("Radio group event owner is missing");
    }
    for(auto h:m_Scene.GetAllEntitiesWith<UISliderComponent>()){
        Entity e(h,&m_Scene);auto id=uint64_t(e.GetUUID());auto &v=e.GetComponent<UISliderComponent>();
        if(target&&target!=id&&target!=v.Track&&target!=v.Fill&&target!=v.Thumb)continue;
        v.Validate();if(!std::isfinite(v.InitialValue)||unsigned(v.Direction)>3)throw std::invalid_argument("Slider requires finite initial value and valid direction");
        if(!e.HasComponent<UIRectComponent>()||(!target&&(!e.HasComponent<UIButtonComponent>()||e.GetComponent<UIButtonComponent>().Kind!=ControlKind::Slider))||e.HasComponent<UIProgressComponent>()||e.HasComponent<UIRadioGroupComponent>())throw std::invalid_argument("Slider needs a Rect and Slider Button behavior");
        if((!target&&(!v.Track||!v.Thumb))||(v.Track&&v.Track==v.Thumb)||(v.Fill&&(v.Fill==v.Track||v.Fill==v.Thumb)))throw std::invalid_argument("Slider requires distinct track/thumb and optional fill");
        visual(id,v.Track);visual(id,v.Fill);visual(id,v.Thumb);
    }
    for (auto h : m_Scene.GetAllEntitiesWith<UIProgressComponent>())
    {
        Entity e(h, &m_Scene);
        if (target && uint64_t(e.GetUUID()) != target)
            continue;
        auto &p = e.GetComponent<UIProgressComponent>();
        if (!e.HasComponent<UIRectComponent>() || e.HasComponent<UIButtonComponent>() ||
            e.HasComponent<UIRadioGroupComponent>())
            throw std::invalid_argument("Progress needs a noninteractive Rect");
        if (!std::isfinite(p.Minimum) || !std::isfinite(p.Maximum) || !std::isfinite(p.InitialValue) ||
            p.Minimum >= p.Maximum || !std::isfinite(p.Maximum - p.Minimum) ||
            unsigned(p.Direction) > 3)
            throw std::invalid_argument(
                "Progress requires a finite increasing range/value and valid direction");
        visual(e.GetUUID(), p.Fill);
    }
}
void RuntimeUI::ControlProblem(const std::string &message)
{
    ++m_Diagnostics.RejectedChanges;
    m_Diagnostics.ControlProblem = message;
    auto now = std::chrono::steady_clock::now();
    if (now - m_LastScreenWarning >= std::chrono::seconds(1))
    {
        HZ_CORE_WARN("UI control: {}", message);
        m_LastScreenWarning = now;
    }
}
void RuntimeUI::SyncControls()
{
    m_ControlVisuals.clear();
    m_Diagnostics.ControlProblem.clear();
    auto visual = [&](uint64_t owner, uint64_t target) {
        if (!target)
            return;
        if (Decoration(owner, target))
            m_ControlVisuals.emplace(target, owner);
        else
            ControlProblem("Control #" + std::to_string(owner) + ": selected/fill visual #" +
                           std::to_string(target) + " must be a direct Sprite/Rect leaf");
    };
    auto owner = [&](uint64_t id, ValueState &v) {
        auto receiver = ValueOwner(id);
        auto entity = m_Scene.GetEntityByUUID(receiver);
        auto lifetime = m_Scene.GetAuthoringLifetime(receiver),
             component = m_Scene.GetAuthoringComponentVersion(receiver, typeid(ScriptComponent));
        const auto name = entity && entity.HasComponent<ScriptComponent>()
                              ? entity.GetComponent<ScriptComponent>().ClassName
                              : std::string{};
        if ((v.Owner || receiver) &&
            (v.Owner != receiver || v.OwnerLifetime != lifetime || v.OwnerComponent != component ||
             v.OwnerClass != name) &&
            (m_Input.Captured() == id || Member(id, m_Input.Captured(), false)))
            CancelScreenInput();
        v.Owner = receiver;
        v.OwnerLifetime = lifetime;
        v.OwnerComponent = component;
        v.OwnerClass = name;
    };
    // Called only on a new source publication, never while traversing controls.
    for (auto it = m_Values.begin(); it != m_Values.end();)
    {
        auto e = m_Scene.GetEntityByUUID(it->first);
        if (!e || !e.HasComponent<UIRectComponent>() ||
            (!e.HasComponent<UIButtonComponent>() && !e.HasComponent<UIRadioGroupComponent>() &&
             !e.HasComponent<UIProgressComponent>()&&!e.HasComponent<UISliderComponent>()))
            it = m_Values.erase(it);
        else
            ++it;
    }
    for (auto id : m_RectIDs)
    {
        auto e = m_Scene.GetEntityByUUID(id);
        auto lifetime = m_Scene.GetAuthoringLifetime(id);
        if ((e.HasComponent<UIProgressComponent>() &&
             (e.HasComponent<UIButtonComponent>() || e.HasComponent<UIRadioGroupComponent>())) ||
            (e.HasComponent<UIButtonComponent>() && e.HasComponent<UIRadioGroupComponent>()))
        {
            ControlProblem("Control #" + std::to_string(id) +
                           ": Progress and Radio Group roots must be noninteractive; remove the "
                           "competing component");
            continue;
        }

        if (e.HasComponent<UIButtonComponent>())
        {
            auto &b = e.GetComponent<UIButtonComponent>();
            if (b.Kind == ControlKind::Button)
            {
                m_Values.erase(id);
                continue;
            }
            if(b.Kind==ControlKind::Slider)continue;
            visual(id, b.Indicator);
            if (b.Kind == ControlKind::Radio && !Member(b.Group, id, false))
                ControlProblem("Radio option #" + std::to_string(id) +
                               ": group must be an ancestor in the same Canvas");
            auto version = m_Scene.GetAuthoringComponentVersion(id, typeid(UIButtonComponent));
            auto &v = m_Values[id];
            if (v.Lifetime != lifetime || v.Component != version || v.Kind != b.Kind ||
                v.InitialChecked != b.InitiallyChecked || !m_Scene.IsRunning())
            {
                v = {};
                v.Lifetime = lifetime;
                v.Component = version;
                v.Kind = b.Kind;
                v.InitialChecked = b.InitiallyChecked;
                v.Checked = b.InitiallyChecked;
            }
            owner(id, v);
        }
        else if (e.HasComponent<UIRadioGroupComponent>())
        {
            auto &g = e.GetComponent<UIRadioGroupComponent>();
            if (g.InitialSelection && !Member(id, g.InitialSelection, false))
                ControlProblem("Radio group #" + std::to_string(id) +
                               ": initial selection is unavailable; repair the reference");
            auto version = m_Scene.GetAuthoringComponentVersion(id, typeid(UIRadioGroupComponent));
            auto &v = m_Values[id];
            if (v.Lifetime != lifetime || v.Component != version ||
                v.InitialSelection != g.InitialSelection || v.Required != g.Required ||
                !m_Scene.IsRunning())
            {
                v = {};
                v.Lifetime = lifetime;
                v.Component = version;
                v.Required = g.Required;
                v.InitialSelection = g.InitialSelection;
                v.Selected = g.InitialSelection;
            }
            if (v.Selected && (!Member(id, v.Selected, true)||(v.SelectedLifetime&&(v.SelectedLifetime!=m_Scene.GetAuthoringLifetime(v.Selected)||v.SelectedComponent!=m_Scene.GetAuthoringComponentVersion(v.Selected,typeid(UIButtonComponent))))))
                v.Selected = 0;
            if (!v.Selected && g.Required)
            {
                // Tab order then stable entity identity. Independent of registry iteration and active
                // modal scope.
                int order = std::numeric_limits<int>::max();
                uint64_t best = 0;
                for (auto handle : m_Scene.GetAllEntitiesWith<UIButtonComponent>())
                {
                    Entity option(handle, &m_Scene);
                    auto member = uint64_t(option.GetUUID());
                    if (Member(id, member, true))
                    {
                        int n = option.GetComponent<UIButtonComponent>().TabOrder;
                        if (!best || n < order || (n == order && member < best))
                        {
                            order = n;
                            best = member;
                        }
                    }
                }
                v.Selected = best;
            }
            v.SelectedLifetime=m_Scene.GetAuthoringLifetime(v.Selected);v.SelectedComponent=m_Scene.GetAuthoringComponentVersion(v.Selected,typeid(UIButtonComponent));
            owner(id, v);
        }
        else if (e.HasComponent<UIProgressComponent>())
        {
            auto &p = e.GetComponent<UIProgressComponent>();
            visual(id, p.Fill);
            auto version = m_Scene.GetAuthoringComponentVersion(id, typeid(UIProgressComponent));
            if (!std::isfinite(p.Minimum) || !std::isfinite(p.Maximum) ||
                !std::isfinite(p.InitialValue) || p.Minimum >= p.Maximum ||
                !std::isfinite(p.Maximum - p.Minimum) || unsigned(p.Direction) > 3)
            {
                auto accepted = m_Values.find(id);
                if (accepted != m_Values.end() &&
                    (accepted->second.Lifetime != lifetime || accepted->second.Component != version))
                    m_Values.erase(accepted);
                ControlProblem("Progress #" + std::to_string(id) +
                               ": invalid native range; retaining accepted value/geometry");
                continue;
            }
            auto &v = m_Values[id];
            if (v.Lifetime != lifetime || v.Component != version || v.Minimum != p.Minimum ||
                v.Maximum != p.Maximum || v.Initial != p.InitialValue || !m_Scene.IsRunning())
            {
                v = {};
                v.Lifetime = lifetime;
                v.Component = version;
                v.Minimum = p.Minimum;
                v.Maximum = p.Maximum;
                v.Initial = p.InitialValue;
                v.Direction = p.Direction;
                v.Value = std::clamp(p.InitialValue, p.Minimum, p.Maximum);
            }
            v.Direction = p.Direction;
        }
    }
}
bool RuntimeUI::Checked(uint64_t id)
{
    auto e = m_Scene.GetEntityByUUID(id);
    if (!e || !e.HasComponent<UIRectComponent>() || !e.HasComponent<UIButtonComponent>() ||
        e.HasComponent<UIProgressComponent>() || e.HasComponent<UIRadioGroupComponent>())
        throw std::invalid_argument("Checked needs a live toggle/radio option");
    auto &b = e.GetComponent<UIButtonComponent>();
    if (b.Kind == ControlKind::Radio)
        return Selection(b.Group) == id;
    if (b.Kind != ControlKind::Toggle)
        throw std::invalid_argument("Checked needs a toggle/radio option");
    return m_Values.at(id).Checked;
}
void RuntimeUI::SetChecked(uint64_t id, bool value)
{
    if (!m_Scene.IsRunning() || m_Scene.IsStopping())
        throw std::invalid_argument("SetChecked requires Play; edit Initially checked in Edit");
    auto e = m_Scene.GetEntityByUUID(id);
    if (!e || !e.HasComponent<UIRectComponent>() || !e.HasComponent<UIButtonComponent>() ||
        e.HasComponent<UIProgressComponent>() || e.HasComponent<UIRadioGroupComponent>() ||
        e.GetComponent<UIButtonComponent>().Kind != ControlKind::Toggle)
        throw std::invalid_argument("SetChecked requires a toggle");
    m_Values.at(id).Checked = value;
}
uint64_t RuntimeUI::Selection(uint64_t id)
{
    auto e = m_Scene.GetEntityByUUID(id);
    if (!e || !e.HasComponent<UIRectComponent>() || !e.HasComponent<UIRadioGroupComponent>() ||
        e.HasComponent<UIButtonComponent>() || e.HasComponent<UIProgressComponent>())
        throw std::invalid_argument("Selection requires a live radio group");
    return m_Values.at(id).Selected;
}
void RuntimeUI::Select(uint64_t id, uint64_t member)
{
    if (!m_Scene.IsRunning() || m_Scene.IsStopping())
        throw std::invalid_argument("Select requires Play");
    (void)Selection(id);
    auto e = m_Scene.GetEntityByUUID(id);
    if ((!member && e.GetComponent<UIRadioGroupComponent>().Required) ||
        (member && !Member(id, member, true)))
        throw std::invalid_argument(
            "Selection must be an enabled visible member; required groups cannot clear");
    if(m_Values.at(id).Selected!=member&&e.HasComponent<UIDropdownComponent>())++m_DropdownRevision;
    auto &v=m_Values.at(id);v.Selected=member;v.SelectedLifetime=m_Scene.GetAuthoringLifetime(member);v.SelectedComponent=m_Scene.GetAuthoringComponentVersion(member,typeid(UIButtonComponent));
}
float RuntimeUI::Progress(uint64_t id)
{
    auto e = m_Scene.GetEntityByUUID(id);
    if (!e || !e.HasComponent<UIRectComponent>() || !e.HasComponent<UIProgressComponent>() ||
        e.HasComponent<UIButtonComponent>() || e.HasComponent<UIRadioGroupComponent>())
        throw std::invalid_argument("Progress requires a live progress display");
    auto it = m_Values.find(id);
    if (it == m_Values.end())
        throw std::invalid_argument("Progress has no accepted finite range; repair its properties");
    return it->second.Value;
}
void RuntimeUI::SetProgress(uint64_t id, float value)
{
    if (!m_Scene.IsRunning() || m_Scene.IsStopping() || !std::isfinite(value))
        throw std::invalid_argument("Progress updates require Play and a finite value");
    (void)Progress(id);
    auto &v = m_Values.at(id);
    v.Value = std::clamp(value, v.Minimum, v.Maximum);
}
uint64_t RuntimeUI::ValueOwner(uint64_t source) const
{
    auto e = m_Scene.GetEntityByUUID(source);
    if (!e)
        return 0;
    if (e.HasComponent<UIRadioGroupComponent>())
        return e.GetComponent<UIRadioGroupComponent>().EventOwner;
    if (e.HasComponent<UIButtonComponent>())
        return e.GetComponent<UIButtonComponent>().EventOwner;
    return 0;
}
void RuntimeUI::ActivateControl(Action event)
{
    if(m_SliderNeutral)return;
    auto e = m_Scene.GetEntityByUUID(event.Entity);
    if (!Live(event.Entity) || !e || !e.HasComponent<UIButtonComponent>())
        return;
    auto &b = e.GetComponent<UIButtonComponent>();
    if(auto group=DropdownGroup(event.Entity)){RequestDropdown(group,true);return;}
    if(b.Kind==ControlKind::Radio){auto group=m_Scene.GetEntityByUUID(b.Group);if(group&&group.HasComponent<UIDropdownComponent>()){RequestChoice(b.Group,event.Entity);return;}}
    if (b.Kind == ControlKind::Button)
    {
        QueueEvent(std::move(event));
        return;
    }
    if(b.Kind==ControlKind::Slider){if(event.Phase!=RoutedPhase::Activate)RouteSlider(event);else if(m_Slider.ID==event.Entity)EndSlider(false);else BeginSlider(event.Entity,false);return;}
    const auto source = event.Entity;
    if (b.Kind == ControlKind::Toggle)
    {
        auto &v = m_Values.at(source);
        event.Kind = UIEventKind::ToggleChanged;
        event.Control = source;
        event.PreviousChecked = v.Checked;
        event.Checked = !v.Checked;
    }
    else if (b.Kind == ControlKind::Radio && Member(b.Group, source, true))
    {
        auto &v = m_Values.at(b.Group);
        if (v.Selected == source)
            return;
        event.Kind = UIEventKind::SelectionChanged;
        event.Control = b.Group;
        event.PreviousSelection = v.Selected;
        event.Selection = source;
        event.Entity = b.Group;
        event.Name = m_Scene.GetEntityByUUID(b.Group).GetComponent<UIRadioGroupComponent>().Action;
    }
    else
    {
        ControlProblem("Radio option has no compatible live ancestor group: " + std::to_string(source));
        return;
    }
    event.SourceLifetime = m_Scene.GetAuthoringLifetime(source);
    event.SourceComponent = m_Scene.GetAuthoringComponentVersion(source, typeid(UIButtonComponent));
    event.ControlLifetime = m_Scene.GetAuthoringLifetime(event.Control);
    event.ControlComponent = m_Scene.GetAuthoringComponentVersion(
        event.Control,
        b.Kind == ControlKind::Toggle ? typeid(UIButtonComponent) : typeid(UIRadioGroupComponent));
    // Admission precedes mutation: full queues and missing/retired owners reject the whole user change.
    if (QueueEvent(event))
    {
        if (event.Kind == UIEventKind::ToggleChanged)
            m_Values.at(source).Checked = event.Checked;
        else{auto &v=m_Values.at(event.Control);v.Selected=source;v.SelectedLifetime=m_Scene.GetAuthoringLifetime(source);v.SelectedComponent=m_Scene.GetAuthoringComponentVersion(source,typeid(UIButtonComponent));}
    }
}
bool RuntimeUI::ControlVisual(uint64_t id, Rect &bounds, bool &visible) const
{
    visible = true;
    if(ScrollVisual(id,bounds,visible))return true;
    if (!m_ControlVisuals.count(id))
        return false;
    auto n = m_Resolved.Find(id);
    if (!n)
        return false;
    bounds = n->Bounds;
    auto e = m_Scene.GetEntityByUUID(id);
    if (!e)
        return false;
    auto parent = m_Scene.GetEntityByUUID(m_Scene.GetRelationship(e).Parent);
    if (!parent)
        return false;
    auto owner = uint64_t(parent.GetUUID());
    if (parent.HasComponent<UIButtonComponent>())
    {
        auto &b = parent.GetComponent<UIButtonComponent>();
        if ((b.Kind == ControlKind::Toggle || b.Kind == ControlKind::Radio) && b.Indicator == id && Decoration(owner, id))
        {
            auto it = m_Values.find(b.Kind == ControlKind::Radio ? b.Group : owner);
            visible =
                it != m_Values.end() &&
                (b.Kind == ControlKind::Radio ? it->second.Selected == owner : it->second.Checked);
            return true;
        }
    }
    if(parent.HasComponent<UISliderComponent>()){
        auto it=m_Values.find(owner);if(it==m_Values.end()){visible=false;return true;}
        try{auto g=SliderGeometryFor(owner);if(id==it->second.Thumb)bounds=g.Thumb;else if(id==it->second.Fill){bounds=g.Fill;visible=bounds.Size.x>0&&bounds.Size.y>0;}return true;}
        catch(...){visible=false;return true;}
    }
    if (parent.HasComponent<UIProgressComponent>())
    {
        auto &p = parent.GetComponent<UIProgressComponent>();
        if (p.Fill != id || !Decoration(owner, id))
            return false;
        auto it = m_Values.find(owner);
        auto root = m_Resolved.Find(owner);
        if (it == m_Values.end() || !root)
        {
            visible = false;
            return true;
        }
        const auto &v = it->second;
        bounds = bounds.Intersect(root->Bounds.Inset(parent.GetComponent<UIRectComponent>().Padding));
        float ratio = (v.Value - v.Minimum) / (v.Maximum - v.Minimum);
        int axis = unsigned(v.Direction) >= 2 ? 1 : 0;
        if (unsigned(v.Direction) > 3)
        {
            visible = false;
            return true;
        }
        float size = bounds.Size[axis] * ratio;
        if (v.Direction == FillDirection::Left || v.Direction == FillDirection::Up)
            bounds.Position[axis] += bounds.Size[axis] - size;
        bounds.Size[axis] = size;
        visible = size > 0;
        return true;
    }
    return false;
}
} // namespace Hazel
