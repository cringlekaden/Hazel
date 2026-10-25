#include "Hazel/Utils/UTF8.h"
#include "RuntimeUI.h"
#include "hzpch.h"
#include <cmath>
namespace Hazel
{
using namespace GameUI;
namespace
{
bool Area(Rect r) { return r.Size.x > 0 && r.Size.y > 0; }
} // namespace
bool RuntimeUI::OwnedPopup(uint64_t id) const { return m_DropdownPopups.count(id) != 0; }
uint64_t RuntimeUI::DropdownGroup(uint64_t trigger) const
{
    auto i = m_DropdownTriggers.find(trigger);
    return i == m_DropdownTriggers.end() ? 0 : i->second;
}
uint64_t RuntimeUI::ActiveDropdown() const
{
    if (m_OpenScreens.empty())
        return 0;
    auto screen = Screen(m_OpenScreens.back());
    return screen ? screen->Dropdown : 0;
}
size_t RuntimeUI::ReservedDropdownSlots() const { return ActiveDropdown() || m_Choice ? 1 : 0; }
void RuntimeUI::DropdownProblem(const std::string &message)
{
    ++m_Diagnostics.DropdownRejected;
    m_Diagnostics.DropdownProblem = message;
    ControlProblem(message);
}
bool RuntimeUI::DropdownOpen(uint64_t id) const
{
    auto e = m_Scene.GetEntityByUUID(id);
    return e && e.HasComponent<UIDropdownComponent>() &&
           (m_Scene.IsRunning() ? IsScreenOpen(e.GetComponent<UIDropdownComponent>().Popup)
                                : m_PreviewDropdown == id);
}
const std::string &RuntimeUI::DisplayText(Entity e) const
{
    auto id = DropdownGroup(e.GetUUID());
    auto root = m_Scene.GetEntityByUUID(id);
    if (root && root.HasComponent<UIDropdownComponent>())
    {
        auto i = m_Values.find(id);
        auto option = i == m_Values.end() ? Entity{} : m_Scene.GetEntityByUUID(i->second.Selected);
        if (option && m_Scene.IsEntityValid(option.GetUUID()) &&
            option.HasComponent<TextComponent>())
            return option.GetComponent<TextComponent>().TextString;
        return root.GetComponent<UIDropdownComponent>().Placeholder;
    }
    return e.GetComponent<TextComponent>().TextString;
}
void RuntimeUI::ValidateDropdowns(uint64_t target) const
{
    std::unordered_set<uint64_t> parts;
    size_t count = 0;
    for (auto handle : m_Scene.GetAllEntitiesWith<UIDropdownComponent>())
    {
        Entity root(handle, &m_Scene);
        if (!m_Scene.IsEntityValid(root.GetUUID()))
            continue;
        if (++count > 32)
            throw std::invalid_argument("Dropdown declaration limit exceeded (32)");
        auto id = uint64_t(root.GetUUID());
        const auto &d = root.GetComponent<UIDropdownComponent>();
        Validate(d);
        // Dependent option/part edits validate their enclosing composition, too.
        if (target && target != id && !Descendant(target, id))
            continue;
        if (!root.HasComponent<UIRectComponent>() || !root.HasComponent<UIRadioGroupComponent>() ||
            root.HasComponent<UIButtonComponent>() || root.HasComponent<UILayoutComponent>() ||
            root.HasComponent<UIScrollViewComponent>() || root.HasComponent<UICanvasComponent>() ||
            root.HasComponent<UIProgressComponent>())
            throw std::invalid_argument("Dropdown #" + std::to_string(id) +
                                        " needs a noninteractive free Rect + Radio Group");
        auto trigger = m_Scene.GetEntityByUUID(d.Trigger), popup = m_Scene.GetEntityByUUID(d.Popup);
        if (!trigger || !m_Scene.IsEntityValid(d.Trigger) ||
            !trigger.HasComponent<UIRectComponent>() || !trigger.HasComponent<TextComponent>() ||
            !trigger.HasComponent<UIButtonComponent>() ||
            trigger.GetComponent<UIButtonComponent>().Kind != ControlKind::Button ||
            m_Scene.GetRelationship(trigger).Parent != root.GetUUID())
            throw std::invalid_argument("Dropdown #" + std::to_string(id) +
                                        ": choose a direct Button/Text trigger");
        if (!popup || !m_Scene.IsEntityValid(d.Popup) || !popup.HasComponent<UIRectComponent>() ||
            !popup.HasComponent<UIScrollViewComponent>() ||
            popup.HasComponent<UICanvasComponent>() ||
            m_Scene.GetRelationship(popup).Parent != root.GetUUID() || d.Trigger == d.Popup)
            throw std::invalid_argument("Dropdown #" + std::to_string(id) +
                                        ": choose a direct Rect/ScrollView popup, not a Canvas");
        if (!parts.insert(d.Trigger).second || !parts.insert(d.Popup).second)
            throw std::invalid_argument("Dropdown parts cannot be shared");
        const auto &r = popup.GetComponent<UIRectComponent>();
        const auto &v = popup.GetComponent<UIScrollViewComponent>();
        if (!v.Vertical || v.Horizontal || r.WidthMode != SizeMode::Fixed ||
            r.HeightMode != SizeMode::Fixed || r.Anchor != glm::vec2(0) ||
            r.Offset != glm::vec2(0) || r.Pivot != glm::vec2(0) || r.Margin != glm::vec4(0) ||
            r.MinimumSize != glm::vec2(0) || r.HasMaximum.x || r.HasMaximum.y)
            throw std::invalid_argument(
                "Dropdown popup owns Fixed size/position; use zero "
                "origin/margins/minima, no maxima, and vertical ScrollView");
        auto content = m_Scene.GetEntityByUUID(v.Content);
        if (!content || !content.HasComponent<UIRectComponent>() ||
            !content.HasComponent<UILayoutComponent>() ||
            content.GetComponent<UILayoutComponent>().Kind != ContainerKind::Stack ||
            content.GetComponent<UILayoutComponent>().Direction != Axis::Vertical ||
            content.GetComponent<UIRectComponent>().WidthMode != SizeMode::Fill ||
            content.GetComponent<UIRectComponent>().HeightMode != SizeMode::Content)
            throw std::invalid_argument("Dropdown popup Content must be a vertical "
                                        "Fill-width/Content-height stack");
        size_t options = 0;
        for (auto child : m_Scene.GetChildrenView(v.Content))
        {
            auto e = m_Scene.GetEntityByUUID(child);
            if (!m_Scene.IsEntityValid(child))
                continue;
            if (++options > 128)
                throw std::invalid_argument("Dropdown #" + std::to_string(id) +
                                            ": option limit exceeded (128)");
            if (!e.HasComponent<UIRectComponent>() || !e.HasComponent<TextComponent>() ||
                !e.HasComponent<UIButtonComponent>() ||
                e.GetComponent<UIButtonComponent>().Kind != ControlKind::Radio ||
                e.GetComponent<UIButtonComponent>().Group != id ||
                e.HasComponent<UIDropdownComponent>() || e.HasComponent<UICanvasComponent>())
                throw std::invalid_argument(
                    "Dropdown Content accepts direct Radio/Text options for its group, "
                    "not arbitrary controls");
            UTF8::Validate(e.GetComponent<TextComponent>().TextString);
        }
        for (auto descendant : m_Scene.GetSubtree(root))
            if (descendant != id)
            {
                auto e = m_Scene.GetEntityByUUID(descendant);
                if (e.HasComponent<UIDropdownComponent>() || e.HasComponent<UICanvasComponent>())
                    throw std::invalid_argument("Nested dropdowns/Canvas surfaces in a "
                                                "Dropdown composition are unsupported");
            }
    }
}
void RuntimeUI::CloseOwnedPopup()
{
    for (auto i = m_OpenScreens.begin(); i != m_OpenScreens.end();)
    {
        auto entry = Screen(*i);
        if (entry && entry->Dropdown)
        {
            m_ReturnFallback = entry->Trigger;
            RememberFocus(entry->ParentScope,entry->Trigger,m_KeyboardOwns);
            i = m_OpenScreens.erase(i);
            m_DropdownFocus = 0;
        }
        else
            ++i;
    }
}
void RuntimeUI::SyncDropdownScreens()
{
    auto available = [&](uint64_t id)
    {
        for (size_t depth = 0; id && depth < Scene::MaxDepth; ++depth)
        {
            auto e = m_Scene.GetEntityByUUID(id);
            if (!e || !m_Scene.IsEntityValid(id))
                return false;
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
            id = m_Scene.GetRelationship(e).Parent;
        }
        return true;
    };
    auto retire = [&](uint64_t popup)
    {
        auto retired = Screen(popup);
        if (m_Choice && retired && m_Choice->Event.Entity == retired->Dropdown)
            m_Choice.reset();
        if (IsScreenOpen(popup))
        {
            auto entry = Screen(popup);
            if (entry)
                m_ReturnFallback = entry->Trigger;
            m_OpenScreens.erase(std::remove(m_OpenScreens.begin(), m_OpenScreens.end(), popup),
                                m_OpenScreens.end());
            CancelScreenInput();
        }
        ++m_ScreenRevision;
    };
    for (auto i = m_Screens.begin(); i != m_Screens.end();)
    {
        if (!i->Dropdown)
        {
            ++i;
            continue;
        }
        auto root = m_Scene.GetEntityByUUID(i->Dropdown);
        auto popup = m_Scene.GetEntityByUUID(i->ID);
        auto trigger = m_Scene.GetEntityByUUID(i->Trigger);
        bool valid =
            root && m_Scene.IsEntityValid(i->Dropdown) &&
            root.HasComponent<UIDropdownComponent>() &&
            root.HasComponent<UIRadioGroupComponent>() && trigger &&
            trigger.HasComponent<UIButtonComponent>() && trigger.HasComponent<TextComponent>() &&
            trigger.GetComponent<UIButtonComponent>().Kind == ControlKind::Button && popup &&
            m_Scene.IsEntityValid(i->ID) && popup.HasComponent<UIScrollViewComponent>() &&
            popup.HasComponent<UIRectComponent>();
        if (valid)
        {
            const auto &d = root.GetComponent<UIDropdownComponent>();
            valid = d.Popup == i->ID && d.Trigger == i->Trigger &&
                    m_Scene.GetAuthoringLifetime(d.Trigger) == i->TriggerLifetime &&
                    m_Scene.GetAuthoringComponentVersion(d.Trigger, typeid(UIButtonComponent)) ==
                        i->TriggerComponent &&
                    m_Scene.GetRelationship(popup).Parent == root.GetUUID() &&
                    DropdownGroup(d.Trigger) == i->Dropdown &&
                    m_Scene.GetAuthoringLifetime(i->Dropdown) == i->DropdownLifetime &&
                    m_Scene.GetAuthoringLifetime(i->ID) == i->Lifetime &&
                    m_Scene.GetAuthoringComponentVersion(
                        i->Dropdown, typeid(UIDropdownComponent)) == i->Component &&
                    CanvasOf(i->Dropdown) == i->Canvas;
        }
        if (!valid)
        {
            DropdownProblem(
                "Dropdown named parts retired or left their local composition; repair references");
            auto id = i->ID;
            retire(id);
            i = m_Screens.erase(i);
            continue;
        }
        uint64_t owner = ValueOwner(i->Dropdown);
        if (auto parent = Screen(i->Canvas); parent && !parent->Dropdown)
            owner = parent->Policy.EventOwner;
        auto entity = m_Scene.GetEntityByUUID(owner);
        auto name = entity && entity.HasComponent<ScriptComponent>()
                        ? entity.GetComponent<ScriptComponent>().ClassName
                        : std::string{};
        bool changed = i->Policy.EventOwner != owner ||
                       i->OwnerLifetime != m_Scene.GetAuthoringLifetime(owner) ||
                       i->OwnerComponent !=
                           m_Scene.GetAuthoringComponentVersion(owner, typeid(ScriptComponent)) ||
                       i->OwnerClass != name;
        if (IsScreenOpen(i->ID) && (changed || !available(i->Dropdown) || !available(i->Trigger) ||
                                    !available(i->ID) || m_OpenScreens.back() != i->ID))
        {
            retire(i->ID);
        }
        i->Policy.EventOwner = owner;
        i->OwnerLifetime = m_Scene.GetAuthoringLifetime(owner);
        i->OwnerComponent = m_Scene.GetAuthoringComponentVersion(owner, typeid(ScriptComponent));
        i->OwnerClass = name;
        ++i;
    }
    if (!m_DropdownIDs.empty())
        m_Screens.reserve(MaxScreens);
    for (auto id : m_DropdownIDs)
    {
        auto root = m_Scene.GetEntityByUUID(id);
        const auto &d = root.GetComponent<UIDropdownComponent>();
        if (!d.Popup || Screen(d.Popup))
            continue;
        auto popup = m_Scene.GetEntityByUUID(d.Popup);
        auto trigger = m_Scene.GetEntityByUUID(d.Trigger);
        if (!trigger || DropdownGroup(d.Trigger) != id ||
            !trigger.HasComponent<UIButtonComponent>() || !trigger.HasComponent<TextComponent>() ||
            trigger.GetComponent<UIButtonComponent>().Kind != ControlKind::Button)
            continue;
        if (!root.HasComponent<UIRadioGroupComponent>() || !popup ||
            !m_Scene.IsEntityValid(d.Popup) || !popup.HasComponent<UIRectComponent>() ||
            !popup.HasComponent<UIScrollViewComponent>() ||
            m_Scene.GetRelationship(popup).Parent != root.GetUUID())
            continue;
        if (m_Screens.size() >= MaxScreens)
            throw std::invalid_argument("Combined screen/owned-overlay limit exceeded (64)");
        ScreenEntry e;
        e.ID = d.Popup;
        e.Dropdown = id;
        e.DropdownLifetime = m_Scene.GetAuthoringLifetime(id);
        e.Lifetime = m_Scene.GetAuthoringLifetime(d.Popup);
        e.Component = m_Scene.GetAuthoringComponentVersion(id, typeid(UIDropdownComponent));
        e.Trigger = d.Trigger;
        e.TriggerLifetime = m_Scene.GetAuthoringLifetime(d.Trigger);
        e.TriggerComponent =
            m_Scene.GetAuthoringComponentVersion(d.Trigger, typeid(UIButtonComponent));
        e.Canvas = CanvasOf(id);
        e.Policy.Managed = true;
        e.Policy.InitiallyOpen = false;
        e.Policy.Cancel = ScreenCancel::Close;
        e.Policy.ReturnFocus = d.Trigger;
        e.Policy.EventOwner = ValueOwner(id);
        if (auto parent = Screen(e.Canvas); parent && !parent->Dropdown)
            e.Policy.EventOwner = parent->Policy.EventOwner;
        auto owner = m_Scene.GetEntityByUUID(e.Policy.EventOwner);
        e.OwnerLifetime = m_Scene.GetAuthoringLifetime(e.Policy.EventOwner);
        e.OwnerComponent =
            m_Scene.GetAuthoringComponentVersion(e.Policy.EventOwner, typeid(ScriptComponent));
        if (owner && owner.HasComponent<ScriptComponent>())
            e.OwnerClass = owner.GetComponent<ScriptComponent>().ClassName;
        m_Screens.push_back(std::move(e));
        ++m_ScreenRevision;
    }
}
bool RuntimeUI::RequestDropdown(uint64_t id, bool open)
{
    if (!m_Scene.IsRunning() || m_Scene.IsStopping())
    {
        DropdownProblem("Dropdown requests require Play; use transient preview in Edit");
        return false;
    }
    auto root = m_Scene.GetEntityByUUID(id);
    if (!root || !m_Scene.IsEntityValid(id) || !root.HasComponent<UIDropdownComponent>() ||
        !root.HasComponent<UIRadioGroupComponent>())
    {
        DropdownProblem("Dropdown target retired or lacks its Radio Group");
        return false;
    }
    if (m_ScreenRequests.size() >= MaxScreenRequests)
    {
        DropdownProblem("Screen request queue full (64); dropdown request rejected");
        return false;
    }
    if (!open && std::any_of(m_ScreenRequests.begin(), m_ScreenRequests.end(), [&](const auto &r)
                             { return r.ID == id && r.Kind == RequestKind::DropdownClose; }))
        return true;
    ScreenRequest r;
    r.Kind = open ? RequestKind::DropdownOpen : RequestKind::DropdownClose;
    r.ID = id;
    r.Lifetime = m_Scene.GetAuthoringLifetime(id);
    r.Component = m_Scene.GetAuthoringComponentVersion(id, typeid(UIDropdownComponent));
    if (!StampRequestIssuer(r))
        return false;
    m_ScreenRequests.push_back(std::move(r));
    return true;
}
bool RuntimeUI::RequestChoice(uint64_t id, uint64_t option)
{
    if (ActiveDropdown() != id || !Live(option) || !Member(id, option, true))
        return false;
    // A choice is a distinct guarded request, never a rewritten/deduplicated
    // close.
    if (m_ScreenRequests.size() >= MaxScreenRequests)
    {
        DropdownProblem("Screen request queue full (64); selection unchanged");
        return false;
    }
    if (std::any_of(m_ScreenRequests.begin(), m_ScreenRequests.end(), [&](const auto &r)
                    { return r.ID == id && r.Kind != RequestKind::DropdownOpen; }))
        return false;
    auto root = m_Scene.GetEntityByUUID(id);
    if (!root)
        return false;
    ScreenRequest request;
    request.ID = id;
    request.Kind = RequestKind::DropdownChoice;
    request.Lifetime = m_Scene.GetAuthoringLifetime(id);
    request.Component = m_Scene.GetAuthoringComponentVersion(id, typeid(UIDropdownComponent));
    if (!StampRequestIssuer(request))
        return false;
    m_ScreenRequests.push_back(std::move(request));
    auto &r = m_ScreenRequests.back();
    r.Option = option;
    r.OptionLifetime = m_Scene.GetAuthoringLifetime(option);
    r.OptionComponent = m_Scene.GetAuthoringComponentVersion(option, typeid(UIButtonComponent));
    return true;
}
bool RuntimeUI::CommitDropdownRequest(const ScreenRequest &r, bool &changed)
{
    auto root = m_Scene.GetEntityByUUID(r.ID);
    if (!root || !m_Scene.IsEntityValid(r.ID) || !root.HasComponent<UIDropdownComponent>() ||
        !root.HasComponent<UIRadioGroupComponent>() ||
        m_Scene.GetAuthoringLifetime(r.ID) != r.Lifetime ||
        m_Scene.GetAuthoringComponentVersion(r.ID, typeid(UIDropdownComponent)) != r.Component)
    {
        DropdownProblem("Dropdown request target retired");
        return false;
    }
    const auto &d = root.GetComponent<UIDropdownComponent>();
    auto existing = Screen(d.Popup);
    if (!existing || existing->Dropdown != r.ID)
    {
        DropdownProblem("Dropdown popup reference is unavailable");
        return false;
    }
    auto entry = *existing;
    if (r.Kind == RequestKind::DropdownClose)
    {
        if (IsScreenOpen(d.Popup))
        {
            CloseOwnedPopup();
            changed = true;
        }
        return true;
    }
    if (r.Kind == RequestKind::DropdownChoice)
    {
        if (ActiveDropdown() != r.ID || !Member(r.ID, r.Option, true) || !Live(r.Option) ||
            m_Scene.GetAuthoringLifetime(r.Option) != r.OptionLifetime ||
            m_Scene.GetAuthoringComponentVersion(r.Option, typeid(UIButtonComponent)) !=
                r.OptionComponent)
        {
            DropdownProblem("Dropdown option retired before choice commit");
            return false;
        }
        if (m_Values.at(r.ID).Selected != r.Option)
        {
            Action event;
            event.Kind = UIEventKind::SelectionChanged;
            event.Entity = event.Control = r.ID;
            event.Origin = d.Trigger;
            event.PresentationComponent = r.Component;
            event.Name = root.GetComponent<UIRadioGroupComponent>().Action;
            event.PreviousSelection = m_Values.at(r.ID).Selected;
            event.Selection = r.Option;
            event.SourceLifetime = m_Scene.GetAuthoringLifetime(d.Trigger);
            event.SourceComponent =
                m_Scene.GetAuthoringComponentVersion(d.Trigger, typeid(UIButtonComponent));
            event.ControlLifetime = r.Lifetime;
            event.ControlComponent =
                m_Scene.GetAuthoringComponentVersion(r.ID, typeid(UIRadioGroupComponent));
            if (!StampEventForScope(event, entry.ParentScope) ||
                event.Owner != entry.Policy.EventOwner ||
                m_Actions.size() + ReservedSliderSlots() >= InputRouter::MaxActions)
            {
                DropdownProblem("Dropdown choice has no valid owner/event admission; "
                                "selection unchanged");
                return false;
            }
            m_Choice = PendingChoice{std::move(event), entry.ParentScope, r.OptionLifetime,
                                     r.OptionComponent};
        }
        CloseOwnedPopup();
        changed = true;
        return true;
    }
    if (IsScreenOpen(d.Popup))
        return true;
    try
    {
        ValidateDropdowns(r.ID);
    }
    catch (const std::exception &e)
    {
        DropdownProblem(e.what());
        return false;
    }
    auto popupEntity = m_Scene.GetEntityByUUID(d.Popup);
    if (!popupEntity || !popupEntity.GetComponent<UIRectComponent>().Visible ||
        !popupEntity.GetComponent<UIRectComponent>().Enabled)
    {
        DropdownProblem("Dropdown popup is hidden/disabled; popup remains closed");
        return false;
    }
    auto trigger = m_Resolved.Find(d.Trigger);
    auto button = m_Scene.GetEntityByUUID(d.Trigger);
    uint64_t parentScope = m_Resolved.Scope;
    if (auto old = ActiveDropdown())
    {
        auto e = Screen(m_Scene.GetEntityByUUID(old).GetComponent<UIDropdownComponent>().Popup);
        if (e)
            parentScope = e->ParentScope;
    }
    if (!ActiveDropdown() && !m_OpenScreens.empty() && m_OpenScreens.back() != parentScope)
    {
        DropdownProblem(
            "Invoking scope changed earlier in the request batch; popup remains closed");
        return false;
    }
    if (!trigger || !trigger->LayoutResolved || !trigger->Visible || !trigger->Enabled ||
        !Area(trigger->FramebufferClip) || !button.GetComponent<UIButtonComponent>().Interactable ||
        (parentScope && trigger->Scope != parentScope))
    {
        DropdownProblem("Dropdown trigger is unavailable in the invoking scope");
        return false;
    }
    bool eligible = false;
    auto list = m_Scene.GetEntityByUUID(d.Popup).GetComponent<UIScrollViewComponent>().Content;
    for (auto child : m_Scene.GetChildrenView(list))
        eligible |= Member(r.ID, child, true);
    if (!eligible)
    {
        DropdownProblem("Dropdown has no enabled visible option; popup remains closed");
        return false;
    }
    Action probe;
    probe.Entity = r.ID;
    probe.Kind = UIEventKind::SelectionChanged;
    if (!StampEventForScope(probe, parentScope) || probe.Owner != entry.Policy.EventOwner ||
        m_Actions.size() + ReservedSliderSlots() + 1 > InputRouter::MaxActions)
    {
        DropdownProblem("Dropdown opening rejected: reserve one owner event slot first");
        return false;
    }
    CloseOwnedPopup();
    RememberFocus(parentScope, d.Trigger, m_KeyboardOwns);
    for (auto &e : m_Screens)
        if (e.ID == d.Popup)
            e.ParentScope = parentScope;
    m_OpenScreens.push_back(d.Popup);
    m_DropdownFocus = r.ID;
    if (auto state = m_ScrollState.find(d.Popup); state != m_ScrollState.end())
        state->second.Offset = {};
    m_ScreenFocus.erase(std::remove_if(m_ScreenFocus.begin(), m_ScreenFocus.end(),
                                       [&](auto &e) { return e.Scope == d.Popup; }),
                        m_ScreenFocus.end());
    changed = true;
    return true;
}
void RuntimeUI::FinishDropdownChoice()
{
    if (!m_Choice)
        return;
    auto choice = std::move(*m_Choice);
    m_Choice.reset();
    auto &event = choice.Event;
    if (choice.ReturnScope != m_Resolved.Scope || !EventValid(event) ||
        !Member(event.Entity, event.Selection, true) ||
        m_Scene.GetAuthoringLifetime(event.Selection) != choice.OptionLifetime ||
        m_Scene.GetAuthoringComponentVersion(event.Selection, typeid(UIButtonComponent)) !=
            choice.OptionComponent)
    {
        DropdownProblem("Dropdown choice source, owner or invoking scope retired before "
                        "publication; selection unchanged");
        return;
    }
    if (m_Actions.size() + ReservedSliderSlots() >= InputRouter::MaxActions)
    {
        DropdownProblem("Dropdown reserved admission unavailable; selection unchanged");
        return;
    }
    event.Sequence = ++m_EventSequence;
    try
    {
        m_Actions.push_back(std::move(event));
    }
    catch (const std::bad_alloc &)
    {
        DropdownProblem("Dropdown event allocation failed; selection unchanged, "
                        "reopen to retry");
        return;
    }
    const auto &accepted = m_Actions.back();
    auto &value = m_Values.at(accepted.Entity);
    value.Selected = accepted.Selection;
    value.SelectedLifetime = m_Scene.GetAuthoringLifetime(accepted.Selection);
    value.SelectedComponent =
        m_Scene.GetAuthoringComponentVersion(accepted.Selection, typeid(UIButtonComponent));
    ++m_DropdownRevision;
    ++m_Diagnostics.DropdownChoices;
}
void RuntimeUI::PreviewDropdown(uint64_t id, bool open)
{
    if (m_Scene.IsRunning() || m_Scene.IsSimulating())
        throw std::invalid_argument("Dropdown preview requires Edit");
    if (open)
    {
        auto e = m_Scene.GetEntityByUUID(id);
        if (!e || !e.HasComponent<UIDropdownComponent>())
            throw std::invalid_argument("Choose a Dropdown");
        ValidateDropdowns(id);
    }
    m_PreviewDropdown = open ? id : 0;
    m_DropdownFocus = open ? id : 0;
    ++m_DropdownRevision;
}
bool RuntimeUI::RouteDropdown(InputFrame &input)
{
    auto id = ActiveDropdown();
    if (!id)
        return false;
    auto root = m_Scene.GetEntityByUUID(id);
    if (!root || !root.HasComponent<UIDropdownComponent>())
        return false;
    auto popup = m_Resolved.Find(root.GetComponent<UIDropdownComponent>().Popup);
    bool outside =
        input.PointerPressed &&
        (!popup ||
         !popup->FramebufferBounds.Intersect(popup->FramebufferClip).Contains(input.PressPosition));
    bool tab = input.NextPressed || input.PreviousPressed || input.NextDown || input.PreviousDown;
    if (outside || tab)
    {
        RequestDropdown(id, false);
        CancelScreenInput();
        m_ActionOwnsFrame = true;
        return true;
    }
    return false;
}
bool RuntimeUI::ProgrammaticScrollAvailable(uint64_t id) const
{
    if (ScrollAvailable(id))
        return true;
    auto root = ActiveDropdown();
    if (!root)
        return false;
    auto e = m_Scene.GetEntityByUUID(root);
    if (!e)
        return false;
    auto screen = Screen(e.GetComponent<UIDropdownComponent>().Popup);
    auto n = m_Resolved.Find(id);
    auto g = ScrollNode(id);
    return screen && n && g && n->LayoutResolved && n->Visible && n->Enabled && Area(g->Viewport) &&
           (!screen->ParentScope || n->Scope == screen->ParentScope);
}
void RuntimeUI::ArrangeDropdown(bool measure)
{
    const auto previous =
        m_Build.Dropdowns.empty() ? DropdownGeometry{} : m_Build.Dropdowns.front();
    m_Build.Dropdowns.clear();
    m_Build.Screens.erase(std::remove_if(m_Build.Screens.begin(), m_Build.Screens.end(),
                                         [&](const auto &s)
                                         { return previous.Popup && s.ID == previous.Popup; }),
                          m_Build.Screens.end());
    if (previous.Popup && m_Build.Scope == previous.Popup)
    {
        m_Build.Scope = previous.ParentScope;
        m_Build.Modal = previous.ParentModal;
    }
    const auto id = m_Scene.IsRunning() ? ActiveDropdown() : m_PreviewDropdown;
    if (!id)
        return;
    auto root = m_Scene.GetEntityByUUID(id);
    if (!root || !root.HasComponent<UIDropdownComponent>())
        return;
    const auto d = root.GetComponent<UIDropdownComponent>();
    auto trigger = m_Build.Find(d.Trigger);
    auto popup = m_Scene.GetEntityByUUID(d.Popup);
    auto entry = Screen(d.Popup);
    auto reject = [&](const std::string &reason)
    {
        // A rejected reposition must not publish yesterday's overlay hit/paint
        // data.
        for (auto &n : m_Build.Elements)
            if (n.Scope == d.Popup)
            {
                n.LayoutResolved = false;
                n.Visible = false;
                n.Enabled = false;
            }
        m_Build.PainterOrder.erase(std::remove_if(m_Build.PainterOrder.begin(),
                                                  m_Build.PainterOrder.end(), [&](auto i)
                                                  { return m_Build.Elements[i].Scope == d.Popup; }),
                                   m_Build.PainterOrder.end());
        DropdownProblem(reason);
        if (m_Scene.IsRunning())
            RequestDropdown(id, false);
    };
    if (!trigger || !trigger->LayoutResolved || !trigger->Visible || !trigger->Enabled ||
        !Area(trigger->FramebufferClip) || !popup || !popup.HasComponent<UIScrollViewComponent>() ||
        !entry)
    {
        reject("Dropdown trigger/popup has no visible owning surface");
        return;
    }
    if (m_Scene.IsRunning() && entry->ParentScope != m_Build.Scope)
    {
        reject("Dropdown invoking scope changed; popup retired");
        return;
    }
    auto ratio = m_Build.Surface.Framebuffer / m_Build.Surface.Viewport;
    auto clipped = trigger->FramebufferBounds.Intersect(trigger->FramebufferClip);
    Rect anchor{clipped.Position / (ratio * trigger->Scale),
                clipped.Size / (ratio * trigger->Scale)};
    Rect surface{{0, 0}, m_Build.Surface.Viewport / trigger->Scale};
    if (auto canvas = m_Build.Find(trigger->Canvas))
        surface = canvas->ParentContent.Intersect(canvas->Bounds);
    if (!Area(surface))
    {
        reject("Dropdown owning safe surface is empty");
        return;
    }
    const auto canvasID = trigger->Canvas;
    const float scale = trigger->Scale;
    const auto parentScope = m_Build.Scope, parentModal = m_Build.Modal;
    const auto v = popup.GetComponent<UIScrollViewComponent>();
    const auto padding = popup.GetComponent<UIRectComponent>().Padding;
    float width = std::min(surface.Size.x, std::max(trigger->Bounds.Size.x, d.MinimumWidth));
    auto prior = m_Build.Find(d.Popup);
    auto content = m_Build.Find(v.Content);
    float wantedHeight =
        content && content->LayoutResolved
            ? std::min(d.MaximumHeight, content->LayoutBounds.Size.y + padding.y + padding.w)
            : d.MaximumHeight;
    auto placement = PlaceDropdown(surface, anchor, width, wantedHeight, d.Gap);
    if (!measure && prior && prior->LayoutResolved &&
        placement.Bounds.Size == prior->LayoutBounds.Size)
    {
        const auto delta = placement.Bounds.Position - prior->LayoutBounds.Position;
        if (delta != glm::vec2(0))
        {
            for (auto &n : m_Build.Elements)
                if (n.Scope == d.Popup)
                {
                    n.LayoutBounds.Position += delta;
                    n.LayoutParentContent.Position += delta;
                    n.LayoutSlot.Position += delta;
                }
            for (auto &g : m_Build.Guides)
                if (auto n = m_Build.Find(g.Owner); n && n->Scope == d.Popup)
                    g.LayoutBounds.Position += delta;
        }
    }
    else
    {
        // Existing solver, two bounded passes: width-constrained content, then
        // capped panel arrange.
        m_Build.PainterOrder.erase(std::remove_if(m_Build.PainterOrder.begin(),
                                                  m_Build.PainterOrder.end(), [&](auto i)
                                                  { return m_Build.Elements[i].Scope == d.Popup; }),
                                   m_Build.PainterOrder.end());
        m_Build.Guides.erase(std::remove_if(m_Build.Guides.begin(), m_Build.Guides.end(),
                                            [&](const auto &g)
                                            {
                                                auto n = m_Build.Find(g.Owner);
                                                return n && n->Scope == d.Popup;
                                            }),
                             m_Build.Guides.end());
        Rect probe{surface.Position, {width, std::min(d.MaximumHeight, surface.Size.y)}};
        MeasureTree(popup, probe, &probe);
        content = m_Build.Find(v.Content);
        wantedHeight = std::min(d.MaximumHeight,
                                (content ? content->Bounds.Size.y : 0) + padding.y + padding.w);
        placement = PlaceDropdown(surface, anchor, width, wantedHeight, d.Gap);
        if (placement.Bounds.Size.x <= padding.x + padding.z ||
            placement.Bounds.Size.y <= padding.y + padding.w)
        {
            reject("Dropdown has no usable popup content space; increase "
                   "surface/panel height or reduce padding");
            return;
        }
        if (placement.Bounds.Size != probe.Size || placement.Bounds.Position != probe.Position)
            MeasureTree(popup, placement.Bounds, &placement.Bounds);
        size_t begin = m_Build.PainterOrder.size();
        Visit(popup, placement.Bounds, surface, scale, true, canvasID, 0, true);
        for (size_t i = begin; i < m_Build.PainterOrder.size(); ++i)
        {
            auto &n = m_Build.Elements[m_Build.PainterOrder[i]];
            n.Scope = d.Popup;
            n.OverlayRoot = n.ID == d.Popup;
            n.LayoutBounds = n.Bounds;
            n.LayoutParentContent = n.ParentContent;
            n.LayoutSlot = n.Slot;
        }
        for (auto &g : m_Build.Guides)
            if (auto n = m_Build.Find(g.Owner); n && n->Scope == d.Popup)
                g.LayoutBounds = g.Bounds;
    }
    m_Build.Scope = m_Build.Modal = d.Popup;
    ApplyScrollPresentation();
    auto value = m_Values.find(id);
    m_Build.Dropdowns.push_back({id, d.Trigger, d.Popup, parentScope, parentModal,
                                 value == m_Values.end() ? 0 : value->second.Selected, anchor,
                                 placement.Bounds, placement.Above});
    m_Build.Screens.push_back({d.Popup, entry->Policy.EventOwner, true, true, true});
}
void RuntimeUI::FocusDropdownOption()
{
    const auto id = m_DropdownFocus;
    if (!id)
        return;
    m_DropdownFocus = 0;
    auto root = m_Scene.GetEntityByUUID(id);
    if (!root || !root.HasComponent<UIDropdownComponent>())
        return;
    auto popup = root.GetComponent<UIDropdownComponent>().Popup;
    if (m_Resolved.Scope != popup)
        return;
    uint64_t target = m_Values.at(id).Selected;
    if (!Member(id, target, true))
    {
        target = 0;
        int order = std::numeric_limits<int>::max();
        auto p = m_Scene.GetEntityByUUID(popup);
        auto content = p.GetComponent<UIScrollViewComponent>().Content;
        for (auto option : m_Scene.GetChildrenView(content))
            if (Member(id, option, true))
            {
                int n = m_Scene.GetEntityByUUID(option).GetComponent<UIButtonComponent>().TabOrder;
                if (!target || n < order || (n == order && uint64_t(option) < target))
                {
                    target = option;
                    order = n;
                }
            }
    }
    if (target && EnsureVisible(popup, target, false))
    {
        m_Input.SetFocus(m_Scene.GetIdentity(), target, m_Resolved.Buttons);
        m_PendingFocus = 0;
    }
}
} // namespace Hazel
