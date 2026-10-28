#include "hzpch.h"
#include "Hazel/Scene/RuntimeSession.h"
#include "Hazel/Scripting/ScriptEngine.h"
#include "RuntimeUI.h"

namespace Hazel
{
using namespace GameUI;
const RuntimeUI::ScreenEntry *RuntimeUI::Screen(uint64_t id) const
{
    auto it =
        std::find_if(m_Screens.begin(), m_Screens.end(), [&](const auto &s) { return s.ID == id; });
    return it == m_Screens.end() ? nullptr : &*it;
}
void RuntimeUI::ScreenProblem(const std::string &message)
{
    ++m_Diagnostics.RejectedScreens;
    auto now = std::chrono::steady_clock::now();
    if (now - m_LastScreenWarning >= std::chrono::seconds(1))
    {
        HZ_CORE_WARN("UI screen: {}", message);
        m_LastScreenWarning = now;
    }
    m_Diagnostics.ScreenProblem = message;
}
void RuntimeUI::CancelScreenInput()
{
    struct ScopeCancel {bool &Flag;bool Old;ScopeCancel(bool &v):Flag(v),Old(v){Flag=true;}~ScopeCancel(){Flag=Old;}} scopeCancel(m_ScopeCancellation);
    EndTextField(false,true,false);
    EndSlider(false,true,false);
    if (auto session = ScriptEngine::GetRuntimeSession();
        session && session->GetScene().get() == &m_Scene)
        session->CancelUIGestures();
    else
        CancelInput();
    m_TransitionNeutral = true;
}
void RuntimeUI::AcceptScreenOwner(ScreenEntry &entry, const ScreenPolicy &policy)
{
    auto owner = m_Scene.GetEntityByUUID(policy.EventOwner);
    bool alive = owner && m_Scene.IsEntityValid(policy.EventOwner);
    uint64_t lifetime = alive ? m_Scene.GetAuthoringLifetime(policy.EventOwner) : 0;
    uint64_t component =
        alive ? m_Scene.GetAuthoringComponentVersion(policy.EventOwner, typeid(ScriptComponent))
              : 0;
    static const std::string empty;
    const auto &name = alive && owner.HasComponent<ScriptComponent>()
                           ? owner.GetComponent<ScriptComponent>().ClassName
                           : empty;
    if (entry.Policy.EventOwner == policy.EventOwner && entry.OwnerLifetime == lifetime &&
        entry.OwnerComponent == component && entry.OwnerClass == name)
        return;
    if (m_Resolved.Valid && m_Resolved.Scope == entry.ID &&
        (m_InputEnabled || m_Input.Focused() || m_Input.Captured()))
    {
        RememberFocus(entry.ID, m_Input.Focused(), m_KeyboardOwns);
        CancelScreenInput();
    }
    entry.OwnerLifetime = lifetime;
    entry.OwnerComponent = component;
    entry.OwnerClass = name;
    ++m_ScreenRevision;
}
void RuntimeUI::SyncScreens()
{
    size_t count = 0;
    for (auto id : m_CanvasIDs)
        if (auto &policy = m_Scene.GetEntityByUUID(id).GetComponent<UICanvasComponent>().Screen;
            policy.Managed)
        {
            if (unsigned(policy.Cancel) > unsigned(ScreenCancel::NotifyOwner))
                throw std::invalid_argument("Invalid screen Cancel policy");
            ++count;
        }
    if (count > MaxScreens)
        throw std::runtime_error("UI managed screen limit exceeded (64)");
    if (count)
    {
        m_Screens.reserve(MaxScreens);
        m_OpenScreens.reserve(MaxScreens);
        m_ScreenFocus.reserve(MaxScreens);
    }
    for (auto it = m_Screens.begin(); it != m_Screens.end();)
    {
        if(it->Dropdown){++it;continue;}
        auto entity = m_Scene.GetEntityByUUID(it->ID);
        if (!entity || !m_Scene.IsEntityValid(it->ID) ||
            !entity.HasComponent<UICanvasComponent>() ||
            !entity.GetComponent<UICanvasComponent>().Screen.Managed ||
            (m_Scene.GetAuthoringLifetime(it->ID) != it->Lifetime ||
             m_Scene.GetAuthoringComponentVersion(it->ID, typeid(UICanvasComponent)) !=
                 it->Component))
        {
            auto id = it->ID;
            if (m_Resolved.Valid && m_Resolved.Scope == id)
            {
                CancelScreenInput();
            }
            m_OpenScreens.erase(std::remove(m_OpenScreens.begin(), m_OpenScreens.end(), id),
                                m_OpenScreens.end());
            it = m_Screens.erase(it);
            m_ScreenFocus.erase(std::remove_if(m_ScreenFocus.begin(), m_ScreenFocus.end(),
                                               [&](const auto &saved)
                                               { return saved.Scope == id; }),
                                m_ScreenFocus.end());
            ++m_ScreenRevision;
        }
        else
        {
            auto next = entity.GetComponent<UICanvasComponent>().Screen;
            AcceptScreenOwner(*it, next);
            if (unsigned(next.Cancel) > unsigned(ScreenCancel::NotifyOwner))
                throw std::invalid_argument("Invalid screen Cancel policy");
            if (it->Policy != next)
            {
                if (!m_Scene.IsRunning() && it->Policy.InitiallyOpen != next.InitiallyOpen)
                {
                    m_OpenScreens.erase(
                        std::remove(m_OpenScreens.begin(), m_OpenScreens.end(), it->ID),
                        m_OpenScreens.end());
                    if (next.InitiallyOpen)
                        m_OpenScreens.push_back(it->ID);
                }
                it->Policy = next;
                ++m_ScreenRevision;
            }
            ++it;
        }
    }
    bool newScreen = false;
    for (auto id : m_CanvasIDs)
        if (m_Scene.GetEntityByUUID(id).GetComponent<UICanvasComponent>().Screen.Managed &&
            !Screen(id))
        {
            newScreen = true;
            break;
        }
    if (!newScreen)
        return;
    // Initial order is the same authored order as ordinary canvases. Later opens
    // append.
    std::sort(m_CanvasIDs.begin(), m_CanvasIDs.end(),
              [&](uint64_t a, uint64_t b)
              {
                  auto x = m_Scene.GetEntityByUUID(a).GetComponent<UICanvasComponent>().SortOrder;
                  auto y = m_Scene.GetEntityByUUID(b).GetComponent<UICanvasComponent>().SortOrder;
                  return x == y ? a < b : x < y;
              });
    for (auto id : m_CanvasIDs)
    {
        auto &c = m_Scene.GetEntityByUUID(id).GetComponent<UICanvasComponent>();
        if (c.Screen.Managed && !Screen(id))
        {
            ScreenEntry entry;
            entry.ID = id;
            entry.Lifetime = m_Scene.GetAuthoringLifetime(id);
            entry.Component = m_Scene.GetAuthoringComponentVersion(id, typeid(UICanvasComponent));
            entry.Policy = c.Screen;
            // A Canvas becoming managed must not transfer its old unscoped gesture.
            if (m_Resolved.Valid && m_Resolved.Scope == id)
            {
                CancelScreenInput();
                m_ScreenFocus.erase(std::remove_if(m_ScreenFocus.begin(), m_ScreenFocus.end(),
                                                   [&](const auto &saved)
                                                   { return saved.Scope == id; }),
                                    m_ScreenFocus.end());
            }
            AcceptScreenOwner(entry, c.Screen);
            m_Screens.push_back(std::move(entry));
            if (c.Screen.InitiallyOpen)
                m_OpenScreens.push_back(id);
            ++m_ScreenRevision;
        }
    }
}
bool RuntimeUI::IsScreenOpen(uint64_t id) const
{
    return std::find(m_OpenScreens.begin(), m_OpenScreens.end(), id) != m_OpenScreens.end();
}
bool RuntimeUI::RequestScreen(ScreenOperation operation, uint64_t id)
{
    m_Scene.CheckThread();
    if (!m_Scene.IsRunning() || m_Scene.IsStopping())
    {
        ScreenProblem("Screen requests require a running scene");
        return false;
    }
    if (unsigned(operation) > unsigned(ScreenOperation::Replace))
    {
        ScreenProblem("Invalid screen operation");
        return false;
    }
    uint64_t lifetime = 0;
    if (operation != ScreenOperation::Pop)
    {
        auto e = m_Scene.GetEntityByUUID(id);
        auto entry=Screen(id);
        if(entry&&entry->Dropdown){if(operation==ScreenOperation::Close)return RequestDropdown(entry->Dropdown,false);ScreenProblem("Use the Dropdown owner to open its popup");return false;}
        if (!e || !m_Scene.IsEntityValid(id) || !e.HasComponent<UICanvasComponent>() ||
            !e.GetComponent<UICanvasComponent>().Screen.Managed)
        {
            ScreenProblem("Request target " + std::to_string(id) + " is not a live managed Canvas");
            return false;
        }
        lifetime = m_Scene.GetAuthoringLifetime(id);
    }
    if (m_ScreenRequests.size() == MaxScreenRequests)
    {
        ScreenProblem("Request queue full (64); newest request rejected");
        return false;
    }
    ScreenRequest request;
    request.Operation = operation;
    request.ID = id;
    request.Lifetime = lifetime;
    request.Component = m_Scene.GetAuthoringComponentVersion(id, typeid(UICanvasComponent));
    if(!StampRequestIssuer(request))return false;
    m_ScreenRequests.push_back(std::move(request));
    return true;
}
bool RuntimeUI::StampRequestIssuer(ScreenRequest &request)
{
    if (auto issuer = ScriptEngine::GetExecutingEntity())
    {
        auto entity = m_Scene.GetEntityByUUID(issuer);
        if (!entity || !m_Scene.IsEntityValid(issuer) || !entity.HasComponent<ScriptComponent>())
        {
            ScreenProblem("Request issuer retired before enqueue");
            return false;
        }
        request.Issuer = issuer;
        request.IssuerLifetime = m_Scene.GetAuthoringLifetime(issuer);
        request.IssuerComponent =
            m_Scene.GetAuthoringComponentVersion(issuer, typeid(ScriptComponent));
        const auto &issuerClass = entity.GetComponent<ScriptComponent>().ClassName;
        auto instance = ScriptEngine::GetEntityScriptInstance(issuer);
        if (!instance || instance->GetScriptClass() != ScriptEngine::GetEntityClass(issuerClass))
        {
            ScreenProblem("Request issuer class changed; recreate/reload its script before "
                          "requesting screens");
            return false;
        }
        if (issuerClass.size() > 512 || issuerClass.find('\0') != std::string::npos)
        {
            ScreenProblem("Request issuer class exceeds 512 bytes");
            return false;
        }
        request.IssuerClass = issuerClass;
    }
    return true;
}
bool RuntimeUI::CommitScreens()
{
    if (m_ScreenRequests.empty())
        return false;
    m_OpenScreens.reserve(MaxScreens);
    m_ScreenFocus.reserve(MaxScreens);
    // Called by Scene before input/update and after scripts, never by Consume or
    // Resolve.
    auto oldFocus = m_Input.Focused(), oldScope = m_Input.Scope();
    bool changed = false,pageChanged=false;
    auto close = [&](uint64_t id)
    {
        auto it = std::find(m_OpenScreens.begin(), m_OpenScreens.end(), id);
        if (it == m_OpenScreens.end())
            return;
        if (auto s = Screen(id))
            m_ReturnFallback = s->Policy.ReturnFocus;
        m_OpenScreens.erase(it);
        changed = true;
    };
    while (!m_ScreenRequests.empty())
    {
        auto request = std::move(m_ScreenRequests.front());
        m_ScreenRequests.pop_front();
        if (request.Issuer)
        {
            auto issuer = m_Scene.GetEntityByUUID(request.Issuer);
            if (!issuer || !m_Scene.IsEntityValid(request.Issuer) ||
                !issuer.HasComponent<ScriptComponent>() ||
                m_Scene.GetAuthoringLifetime(request.Issuer) != request.IssuerLifetime ||
                m_Scene.GetAuthoringComponentVersion(request.Issuer, typeid(ScriptComponent)) !=
                    request.IssuerComponent ||
                issuer.GetComponent<ScriptComponent>().ClassName != request.IssuerClass)
            {
                ScreenProblem("Request issuer retired before commit: " +
                              std::to_string(request.Issuer));
                continue;
            }
        }

        if(request.Kind==RequestKind::TabChoice){pageChanged|=CommitTab(request);continue;}
        if(request.Kind!=RequestKind::Screen){CommitDropdownRequest(request,changed);continue;}
        m_Choice.reset(); // unrelated scope operations never inherit a staged choice
        if(ActiveDropdown()&&request.Operation==ScreenOperation::Pop){CloseOwnedPopup();changed=true;continue;}
        if (request.Operation == ScreenOperation::Pop)
        {
            if (!m_OpenScreens.empty())
                close(m_OpenScreens.back());
            continue;
        }
        auto entry = Screen(request.ID);
        if (!entry ||
            (entry->Lifetime != request.Lifetime || entry->Component != request.Component))
        {
            ScreenProblem("Request target retired before commit: " + std::to_string(request.ID));
            continue;
        }
        if (request.Operation == ScreenOperation::Close)
        {
            if(IsScreenOpen(request.ID)&&ActiveDropdown()){CloseOwnedPopup();changed=true;}
            close(request.ID);
            continue;
        }
        if (IsScreenOpen(request.ID))
        {
            if (request.Operation == ScreenOperation::Open ||
                (!m_OpenScreens.empty() && m_OpenScreens.back() == request.ID))
                continue;
            ScreenProblem(
                "Replace target is already open below the top; close it or pop to it first: " +
                std::to_string(request.ID));
            continue;
        }
        auto n = m_Resolved.Find(request.ID);
        if (!n || !n->Visible || !n->Enabled)
        {
            ScreenProblem("Cannot open unavailable screen " + std::to_string(request.ID) +
                          "; make its authored ancestors visible/enabled first");
            continue;
        }
        if(ActiveDropdown()){CloseOwnedPopup();changed=true;}
        if (request.Operation == ScreenOperation::Replace && !m_OpenScreens.empty() &&
            m_OpenScreens.back() != request.ID)
            close(m_OpenScreens.back());
        if (!IsScreenOpen(request.ID))
        {
            m_OpenScreens.push_back(request.ID);
            changed = true;
            // Reopening starts at the authored initial target, not a retired visit.
            m_ScreenFocus.erase(std::remove_if(m_ScreenFocus.begin(), m_ScreenFocus.end(),
                                               [&](auto p) { return p.Scope == request.ID; }),
                                m_ScreenFocus.end());
        }
    }
    if (!changed)
        return pageChanged;
    m_RetainPageEvents=false;
    RememberFocus(oldScope, oldFocus, m_KeyboardOwns);
    CancelScreenInput();
    ++m_ScreenRevision;
    return true;
}
void RuntimeUI::SetPreviewScreen(uint64_t screen)
{
    m_PreviewScreen = m_Scene.IsRunning() ? 0 : screen;
}
void RuntimeUI::RememberFocus(uint64_t scope, uint64_t target, bool keyboard)
{
    auto it = std::find_if(m_ScreenFocus.begin(), m_ScreenFocus.end(),
                           [&](const auto &p) { return p.Scope == scope; });
    if (it != m_ScreenFocus.end())
    {
        it->Target = target;
        it->Keyboard = keyboard;
    }
    else
    {
        if (m_ScreenFocus.size() == MaxScreens)
            m_ScreenFocus.erase(m_ScreenFocus.begin());
        m_ScreenFocus.push_back({scope, target, keyboard});
    }
}
void RuntimeUI::PublishScope()
{
    const auto scope = m_Resolved.Scope;
    if (scope != m_LastScope || m_Input.Scope() != scope)
    {
        const bool liveInput = m_InputEnabled;
        if (liveInput || m_Input.Focused())
            RememberFocus(m_LastScope, m_Input.Focused(), m_KeyboardOwns);
        if (liveInput && scope != m_LastScope && (Screen(scope) || Screen(m_LastScope)))
            CancelScreenInput();
        if(!m_RetainPageEvents){m_Diagnostics.RetiredEvents += m_Actions.size();m_Actions.clear();}
        m_Input.SetScope(m_Scene.GetIdentity(), scope);
        m_Input.Reconcile(m_Scene.GetIdentity(), m_Resolved.Buttons, m_Resolved.Generation);
        uint64_t target = 0;
        bool keyboard = Screen(scope) != nullptr;
        for (const auto &saved : m_ScreenFocus)
            if (saved.Scope == scope)
            {
                if (Live(saved.Target))
                    target = saved.Target;
                keyboard = saved.Keyboard;
            }
        if (!target && Live(m_ReturnFallback))
            target = m_ReturnFallback;
        if (!target)
            if (auto s = Screen(scope))
            {
                if (Live(s->Policy.InitialFocus))
                    target = s->Policy.InitialFocus;
                else if (s->Policy.InitialFocus)
                    ScreenProblem("Initial focus is unavailable in screen " +
                                  std::to_string(scope) + "; using first eligible control");
            }
        m_PendingFocus = target;
        if (target && m_Input.SetFocus(m_Scene.GetIdentity(), target, m_Resolved.Buttons))
            m_PendingFocus = 0;
        m_ReturnFallback = 0;
        m_LastScope = scope;
        m_KeyboardOwns = keyboard;
    }
    else
        m_Input.Reconcile(m_Scene.GetIdentity(), m_Resolved.Buttons, m_Resolved.Generation);
    if (m_PendingFocus && !Live(m_PendingFocus))
        m_PendingFocus = 0;
}
bool RuntimeUI::StampEvent(Action &event){return StampEventForScope(event,m_Resolved.Scope);}
bool RuntimeUI::StampEventForScope(Action &event,uint64_t scope)
{
    const bool value=event.Kind==UIEventKind::ToggleChanged||event.Kind==UIEventKind::SelectionChanged||event.Kind>=UIEventKind::SliderChanged;
    if (auto screen = Screen(scope)) {
        event.Screen=screen->ID;event.Owner=screen->Policy.EventOwner;
    } else if(value || event.Kind==UIEventKind::Activated) event.Owner=ValueOwner(event.Entity);
    if(event.Screen||value||event.Owner){
        auto owner=m_Scene.GetEntityByUUID(event.Owner);
        if(!owner||!m_Scene.IsEntityValid(event.Owner)){if(value)ControlProblem("User change rejected: assign a live event owner on its screen or standalone control/group");else ScreenProblem("UI event requires a live declared owner");return false;}
        if(owner.HasComponent<ScriptComponent>()&&!owner.GetComponent<ScriptComponent>().ClassName.empty()){
            auto instance=ScriptEngine::GetEntityScriptInstance(event.Owner);
            if(!instance||instance->GetScriptClass()!=ScriptEngine::GetEntityClass(owner.GetComponent<ScriptComponent>().ClassName)){
                ScreenProblem("Declared event owner script is not live; user change rejected");return false;
            }
        }
        event.OwnerLifetime=m_Scene.GetAuthoringLifetime(event.Owner);
        event.OwnerComponent=m_Scene.GetAuthoringComponentVersion(event.Owner,typeid(ScriptComponent));
        if(owner.HasComponent<ScriptComponent>())event.OwnerClass=owner.GetComponent<ScriptComponent>().ClassName;
        if(event.OwnerClass.size()>512){ScreenProblem("Event owner class exceeds 512 bytes");return false;}
    }
    return true;
}
bool RuntimeUI::QueueEvent(Action event){
    const bool value=event.Kind==UIEventKind::ToggleChanged||event.Kind==UIEventKind::SelectionChanged||event.Kind>=UIEventKind::SliderChanged;
    if(!StampEvent(event))return false;
    if(m_Actions.size()+ReservedSliderSlots()+ReservedDropdownSlots()+ReservedTextSlots()>=InputRouter::MaxActions){++m_OuterDrops;if(value)ControlProblem("Event queue full (64); user value change rejected");return false;}
    event.Sequence=++m_EventSequence;m_Actions.push_back(std::move(event));return true;
}
bool RuntimeUI::EventValid(const Action &event) const
{
    if(event.Owner){
        auto owner=m_Scene.GetEntityByUUID(event.Owner);
        if(!owner||!m_Scene.IsEntityValid(event.Owner)||m_Scene.GetAuthoringLifetime(event.Owner)!=event.OwnerLifetime||
            m_Scene.GetAuthoringComponentVersion(event.Owner,typeid(ScriptComponent))!=event.OwnerComponent||
            (owner.HasComponent<ScriptComponent>()?owner.GetComponent<ScriptComponent>().ClassName:std::string{})!=event.OwnerClass)return false;
    }
    if (event.Screen) {
        auto screen=Screen(event.Screen);
        if(!screen||m_Resolved.Scope!=event.Screen||!IsScreenOpen(event.Screen)||screen->Policy.EventOwner!=event.Owner)return false;
        if(event.Kind==UIEventKind::Cancel)return screen->Policy.Cancel==ScreenCancel::NotifyOwner&&
            event.Entity==(Live(screen->Policy.CancelTarget)?screen->Policy.CancelTarget:event.Screen);
    }
    if(event.Kind>=UIEventKind::TextChanged){
        auto e=m_Scene.GetEntityByUUID(event.Entity);
        return Live(event.Entity)&&e&&e.HasComponent<UITextEntryComponent>()&&e.HasComponent<UIButtonComponent>()&&e.GetComponent<UIButtonComponent>().Kind==ControlKind::TextEntry&&e.GetComponent<UIButtonComponent>().Action==event.Name&&m_Scene.GetAuthoringLifetime(event.Entity)==event.SourceLifetime&&m_Scene.GetAuthoringComponentVersion(event.Entity,typeid(UIButtonComponent))==event.SourceComponent&&m_Scene.GetAuthoringComponentVersion(event.Entity,typeid(UITextEntryComponent))==event.ControlComponent&&(event.Screen||ValueOwner(event.Entity)==event.Owner);
    }
    if(event.Kind>=UIEventKind::SliderChanged){
        auto e=m_Scene.GetEntityByUUID(event.Entity);
        return Live(event.Entity)&&e&&e.HasComponent<UISliderComponent>()&&e.HasComponent<UIButtonComponent>()&&
            e.GetComponent<UIButtonComponent>().Kind==ControlKind::Slider&&e.GetComponent<UIButtonComponent>().Action==event.Name&&
            m_Scene.GetAuthoringLifetime(event.Entity)==event.SourceLifetime&&
            m_Scene.GetAuthoringComponentVersion(event.Entity,typeid(UIButtonComponent))==event.SourceComponent&&
            m_Scene.GetAuthoringComponentVersion(event.Entity,typeid(UISliderComponent))==event.ControlComponent&&
            (event.Screen||(!Screen(CanvasOf(event.Entity))&&ValueOwner(event.Entity)==event.Owner));
    }
    // Copied Play scenes legitimately use component token zero. Origin identifies
    // this envelope; version tokens are compared below, never used as presence bits.
    if(event.Kind==UIEventKind::SelectionChanged&&event.Page){
        auto root=m_Scene.GetEntityByUUID(event.Entity),tab=m_Scene.GetEntityByUUID(event.Selection);
        if(!root||!root.HasComponent<UITabsComponent>()||!tab||!tab.HasComponent<UITabComponent>()||tab.GetComponent<UITabComponent>().Page!=event.Page||m_Scene.GetAuthoringComponentVersion(event.Selection,typeid(UITabComponent))!=event.PresentationComponent)return false;
    }
    if(event.Kind==UIEventKind::SelectionChanged&&event.Origin){
        auto root=m_Scene.GetEntityByUUID(event.Entity);auto trigger=m_Scene.GetEntityByUUID(event.Origin);
        if(!root||!root.HasComponent<UIDropdownComponent>()||!root.HasComponent<UIRadioGroupComponent>()||!trigger||!trigger.HasComponent<UIButtonComponent>()||!Live(event.Origin))return false;
        const auto &d=root.GetComponent<UIDropdownComponent>();const auto &g=root.GetComponent<UIRadioGroupComponent>();
        return d.Trigger==event.Origin&&m_Scene.GetAuthoringLifetime(event.Entity)==event.ControlLifetime&&m_Scene.GetAuthoringComponentVersion(event.Entity,typeid(UIRadioGroupComponent))==event.ControlComponent&&m_Scene.GetAuthoringComponentVersion(event.Entity,typeid(UIDropdownComponent))==event.PresentationComponent&&m_Scene.GetAuthoringLifetime(event.Origin)==event.SourceLifetime&&m_Scene.GetAuthoringComponentVersion(event.Origin,typeid(UIButtonComponent))==event.SourceComponent&&g.Action==event.Name&&(event.Screen||(!Screen(CanvasOf(event.Entity))&&ValueOwner(event.Entity)==event.Owner));
    }
    if(event.Kind==UIEventKind::ToggleChanged||event.Kind==UIEventKind::SelectionChanged){
        auto source=event.Kind==UIEventKind::ToggleChanged?event.Entity:event.Selection;
        auto e=m_Scene.GetEntityByUUID(source),control=m_Scene.GetEntityByUUID(event.Control);
        if(!e||!control||!Live(source)||!e.HasComponent<UIButtonComponent>()||
            m_Scene.GetAuthoringLifetime(source)!=event.SourceLifetime||
            m_Scene.GetAuthoringComponentVersion(source,typeid(UIButtonComponent))!=event.SourceComponent||
            m_Scene.GetAuthoringLifetime(event.Control)!=event.ControlLifetime)return false;
        const auto &b=e.GetComponent<UIButtonComponent>();
        if(event.Kind==UIEventKind::ToggleChanged){if(b.Kind!=ControlKind::Toggle||b.Action!=event.Name)return false;}
        else if(!Member(event.Control,source,true)||!control.HasComponent<UIRadioGroupComponent>()||control.GetComponent<UIRadioGroupComponent>().Action!=event.Name)return false;
        if(m_Scene.GetAuthoringComponentVersion(event.Control,event.Kind==UIEventKind::ToggleChanged?typeid(UIButtonComponent):typeid(UIRadioGroupComponent))!=event.ControlComponent)return false;
        return event.Screen||(!Screen(CanvasOf(source))&&ValueOwner(event.Entity)==event.Owner);
    }
    auto n=m_Resolved.Find(event.Entity);
    if(!event.Screen&&n&&(Screen(n->Scope)||ValueOwner(event.Entity)!=event.Owner))return false;
    return Live(event.Entity)&&n->ButtonIndex<m_Resolved.Buttons.size()&&m_Resolved.Buttons[n->ButtonIndex].Action==event.Name;
}
bool RuntimeUI::Focus(uint64_t target)
{
    if (target && (!m_InputEnabled || !Live(target)))
    {
        ScreenProblem("Focus target is not eligible in the active scope: " +
                      std::to_string(target));
        return false;
    }
    m_PendingFocus = target;
    if (m_Input.SetFocus(m_Scene.GetIdentity(), target, m_Resolved.Buttons))
        m_PendingFocus = 0;
    if (target){m_KeyboardOwns = true;m_TipPointer=false;}
    ReconcileSlider();
    ReconcileTextField();
    return true;
}
bool RuntimeUI::ConsumeEvent(uint64_t owner, Action &event)
{
    if (!owner || std::none_of(m_Actions.begin(), m_Actions.end(), [&](const auto &event)
                               { return event.Owner == owner; }))
        return false;
    ResolveSurface(m_Resolved.Surface, m_Resolved.PreviewOrphans);
    PruneActions();
    for (auto it = m_Actions.begin(); it != m_Actions.end(); ++it)
        if (it->Owner == owner)
        {
            event = std::move(*it);
            m_Actions.erase(it);
            return true;
        }
    return false;
}
} // namespace Hazel
