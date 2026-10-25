#include "hzpch.h"
#include "RuntimeUI.h"
#include <cmath>
namespace Hazel
{
using namespace GameUI;
void RuntimeUI::CompositionProblem(const std::string &message)
{
    m_Diagnostics.CompositionProblem = message;
    ControlProblem(message);
}
uint64_t RuntimeUI::PageSelection(uint64_t group) const
{
    if (!m_Scene.IsRunning())
    {
        auto p = m_TabPreviews.find(group);
        if (p != m_TabPreviews.end() && Member(group, p->second, false))
            return p->second;
    }
    auto v = m_Values.find(group);
    return v == m_Values.end() ? 0 : v->second.Selected;
}
bool RuntimeUI::PageActive(uint64_t id) const
{
    for (size_t depth = 0; id && depth < Scene::MaxDepth; ++depth)
    {
        auto page = m_PageTabs.find(id);
        if (page != m_PageTabs.end())
        {
            auto tab = m_Scene.GetEntityByUUID(page->second);
            if (!tab || !tab.HasComponent<UIButtonComponent>() ||
                PageSelection(tab.GetComponent<UIButtonComponent>().Group) != page->second)
                return false;
        }
        auto e = m_Scene.GetEntityByUUID(id);
        if (!e || !m_Scene.IsEntityValid(id))
            return false;
        id = m_Scene.GetRelationship(e).Parent;
    }
    return true;
}
void RuntimeUI::CaptureCompositions()
{
    m_PageTabs.clear();
    m_TooltipBodies.clear();
    m_TabGroups.clear();
    m_TooltipIDs.clear();
    for (auto h : m_Scene.GetAllEntitiesWith<UITabsComponent>())
    {
        Entity e(h, &m_Scene);
        if (m_Scene.IsEntityValid(e.GetUUID()))
            m_TabGroups.push_back(e.GetUUID());
    }
    for (auto i = m_TabPreviews.begin(); i != m_TabPreviews.end();)
        if (std::find(m_TabGroups.begin(), m_TabGroups.end(), i->first) == m_TabGroups.end())
            i = m_TabPreviews.erase(i);
        else
            ++i;
    if (m_TabGroups.size() > 32)
        throw std::invalid_argument("Tabs declaration limit exceeded (32)");
    for (auto h : m_Scene.GetAllEntitiesWith<UITabComponent>())
    {
        Entity e(h, &m_Scene);
        if (!m_Scene.IsEntityValid(e.GetUUID()))
            continue;
        auto page = e.GetComponent<UITabComponent>().Page;
        auto p = m_Scene.GetEntityByUUID(page);
        if (!p || !p.HasComponent<UIRectComponent>() || !e.HasComponent<UIButtonComponent>())
            continue;
        auto group = e.GetComponent<UIButtonComponent>().Group;
        auto g = m_Scene.GetEntityByUUID(group);
        if (g && g.HasComponent<UITabsComponent>() && m_Scene.GetRelationship(p).Parent == UUID(group) &&
            Descendant(e.GetUUID(), group))
        {
            if (!m_PageTabs.emplace(page, e.GetUUID()).second)
                throw std::invalid_argument("Tab page #" + std::to_string(page) + " has duplicate owners");
        }
    }
    for (auto h : m_Scene.GetAllEntitiesWith<UITooltipComponent>())
    {
        Entity e(h, &m_Scene);
        if (!m_Scene.IsEntityValid(e.GetUUID()))
            continue;
        m_TooltipIDs.push_back(e.GetUUID());
        const auto &c = e.GetComponent<UITooltipComponent>();
        if (!std::isfinite(c.Delay) || c.Delay < 0 || c.Delay > 5 || !std::isfinite(c.Gap) || c.Gap < 0 ||
            c.Gap > 64)
            throw std::invalid_argument("Tooltip delay 0..5 and gap 0..64 must be finite");
        auto body = m_Scene.GetEntityByUUID(c.Body);
        if (body && body.HasComponent<UIRectComponent>() &&
            m_Scene.GetRelationship(body).Parent == e.GetUUID())
        {
            if (!m_TooltipBodies.emplace(c.Body, e.GetUUID()).second)
                throw std::invalid_argument("Tooltip body cannot be shared");
        }
    }
    if (m_TooltipIDs.size() > 128)
        throw std::invalid_argument("Tooltip declaration limit exceeded (128)");
    std::sort(m_TabGroups.begin(), m_TabGroups.end());
    std::sort(m_TooltipIDs.begin(), m_TooltipIDs.end());
}
void RuntimeUI::ValidatePages(uint64_t target) const
{
    size_t count = 0;
    std::unordered_set<uint64_t> pages, bodies;
    for (auto h : m_Scene.GetAllEntitiesWith<UITabsComponent>())
    {
        Entity root(h, &m_Scene);
        if (!m_Scene.IsEntityValid(root.GetUUID()))
            continue;
        if (++count > 32)
            throw std::invalid_argument("Tabs declaration limit exceeded (32)");
        auto id = uint64_t(root.GetUUID());
        if (target && target != id && !Descendant(target, id))
            continue;
        if (!root.HasComponent<UIRectComponent>() || !root.HasComponent<UIRadioGroupComponent>() ||
            !root.GetComponent<UIRadioGroupComponent>().Required || root.HasComponent<UIButtonComponent>() ||
            root.HasComponent<UILayoutComponent>() || root.HasComponent<UIDropdownComponent>() ||
            root.HasComponent<UIScrollViewComponent>() || root.HasComponent<UICanvasComponent>())
            throw std::invalid_argument("Tabs #" + std::to_string(id) +
                                        " needs a free Rect and required Radio Group");
        size_t tabs = 0;
        for (auto bh : m_Scene.GetAllEntitiesWith<UIButtonComponent>())
        {
            Entity tab(bh, &m_Scene);
            const auto &b = tab.GetComponent<UIButtonComponent>();
            if (b.Group != id)
                continue;
            if (++tabs > 128)
                throw std::invalid_argument("Tabs #" + std::to_string(id) + ": tab limit exceeded (128)");
            if (b.Kind != ControlKind::Radio || !tab.HasComponent<UITabComponent>() ||
                !Descendant(tab.GetUUID(), id) || CanvasOf(tab.GetUUID()) != CanvasOf(id))
                throw std::invalid_argument("Tabs require local Radio options with UI Tab configuration");
            const auto &c = tab.GetComponent<UITabComponent>();
            auto p = m_Scene.GetEntityByUUID(c.Page);
            if (!p || !m_Scene.IsEntityValid(c.Page) || !p.HasComponent<UIRectComponent>() ||
                p.HasComponent<UICanvasComponent>() || p.HasComponent<UIButtonComponent>() ||
                p.HasComponent<UITabComponent>() || m_Scene.GetRelationship(p).Parent != root.GetUUID() ||
                Descendant(tab.GetUUID(), c.Page))
                throw std::invalid_argument("Tab #" + std::to_string(uint64_t(tab.GetUUID())) +
                                            ": page must be a direct ordinary Rect child of its group, "
                                            "outside the tab strip (cycles are invalid)");
            for (auto descendant : m_Scene.GetSubtree(p))
            {
                auto child = m_Scene.GetEntityByUUID(descendant);
                if (child.HasComponent<UICanvasComponent>() ||
                    ((child.HasComponent<SpriteRendererComponent>() || child.HasComponent<TextComponent>()) &&
                     !child.HasComponent<UIRectComponent>()))
                    throw std::invalid_argument("Tab pages contain ordinary UI Rect content; independent "
                                                "Canvas/world visuals belong outside pages");
            }
            if (!pages.insert(c.Page).second)
                throw std::invalid_argument("Duplicate tab page ownership #" + std::to_string(c.Page));
            if (c.InitialFocus)
            {
                auto f = m_Scene.GetEntityByUUID(c.InitialFocus);
                if (!f || !f.HasComponent<UIButtonComponent>() || !Descendant(c.InitialFocus, c.Page))
                    throw std::invalid_argument("Tab initial focus must be a control inside its page");
            }
        }
    }
    for (auto h : m_Scene.GetAllEntitiesWith<UITabComponent>())
    {
        Entity tab(h, &m_Scene);
        if (target && target != uint64_t(tab.GetUUID()))
            continue;
        if (!tab.HasComponent<UIButtonComponent>())
            throw std::invalid_argument("UI Tab requires a Radio Button");
        auto root = m_Scene.GetEntityByUUID(tab.GetComponent<UIButtonComponent>().Group);
        if (!root || !root.HasComponent<UITabsComponent>())
            throw std::invalid_argument("UI Tab requires its Tabs ancestor group");
    }
    count = 0;
    for (auto h : m_Scene.GetAllEntitiesWith<UITooltipComponent>())
    {
        Entity source(h, &m_Scene);
        if (++count > 128)
            throw std::invalid_argument("Tooltip declaration limit exceeded (128)");
        auto id = uint64_t(source.GetUUID());
        const auto &c = source.GetComponent<UITooltipComponent>();
        if (target && target != id && target != c.Body)
            continue;
        auto body = m_Scene.GetEntityByUUID(c.Body);
        if (!std::isfinite(c.Delay) || c.Delay < 0 || c.Delay > 5 || !std::isfinite(c.Gap) || c.Gap < 0 ||
            c.Gap > 64)
            throw std::invalid_argument("Tooltip delay 0..5 and gap 0..64 must be finite");
        if (!source.HasComponent<UIButtonComponent>() || !source.HasComponent<UIRectComponent>() || !body ||
            !m_Scene.IsEntityValid(c.Body) || !body.HasComponent<UIRectComponent>() ||
            !body.HasComponent<TextComponent>() || body.HasComponent<UIButtonComponent>() ||
            body.HasComponent<UICanvasComponent>() || body.HasComponent<UILayoutComponent>() ||
            body.HasComponent<UIScrollViewComponent>() || body.HasComponent<UITooltipComponent>() ||
            m_Scene.GetRelationship(body).Parent != source.GetUUID() ||
            !m_Scene.GetChildrenView(c.Body).empty())
            throw std::invalid_argument(
                "Tooltip needs a Button/Rect source and a direct Text/Rect leaf body (optional Sprite)");
        const auto &r = body.GetComponent<UIRectComponent>();
        if (r.WidthMode != SizeMode::Fixed || r.HeightMode != SizeMode::Content || r.Anchor != glm::vec2(0) ||
            r.Offset != glm::vec2(0) || r.Pivot != glm::vec2(0) || r.Margin != glm::vec4(0) ||
            r.MinimumSize != glm::vec2(0) || r.HasMaximum.x || r.HasMaximum.y)
            throw std::invalid_argument("Tooltip body uses Fixed width, Content height, zero "
                                        "origin/margins/minima and no maxima; overlay owns its placement");
        if (!bodies.insert(c.Body).second || pages.count(c.Body))
            throw std::invalid_argument("Tooltip body cannot be shared or own a tab page");
    }
}
void RuntimeUI::PreviewTab(uint64_t group, uint64_t tab)
{
    if (m_Scene.IsRunning() || m_Scene.IsSimulating())
        throw std::invalid_argument("Page preview requires Edit");
    auto root = m_Scene.GetEntityByUUID(group);
    if (!root || !root.HasComponent<UITabsComponent>() || (tab && !Member(group, tab, false)))
        throw std::invalid_argument("Choose a local tab from this group");
    ValidatePages(group);
    if (tab)
        m_TabPreviews[group] = tab;
    else
        m_TabPreviews.erase(group);
    ++m_DropdownRevision;
}
bool RuntimeUI::RequestTab(uint64_t group, uint64_t tab, bool notify)
{
    auto root = m_Scene.GetEntityByUUID(group), option = m_Scene.GetEntityByUUID(tab);
    if (!m_Scene.IsRunning() || m_Scene.IsStopping() || !root || !root.HasComponent<UITabsComponent>() ||
        !root.HasComponent<UIRadioGroupComponent>() || !Member(group, tab, true) ||
        !option.HasComponent<UITabComponent>())
        return false;
    if (notify && !Live(tab))
        return false;
    if (m_ScreenRequests.size() >= MaxScreenRequests)
    {
        CompositionProblem("UI request queue full (64); tab change rejected");
        return false;
    }
    ScreenRequest r;
    r.Kind = RequestKind::TabChoice;
    r.ID = group;
    r.Option = tab;
    r.Notify = notify;
    r.Lifetime = m_Scene.GetAuthoringLifetime(group);
    r.Component = m_Scene.GetAuthoringComponentVersion(group, typeid(UIRadioGroupComponent));
    r.RootComponent = m_Scene.GetAuthoringComponentVersion(group, typeid(UITabsComponent));
    r.OptionLifetime = m_Scene.GetAuthoringLifetime(tab);
    r.OptionComponent = m_Scene.GetAuthoringComponentVersion(tab, typeid(UIButtonComponent));
    r.PresentationComponent = m_Scene.GetAuthoringComponentVersion(tab, typeid(UITabComponent));
    r.Page = option.GetComponent<UITabComponent>().Page;
    r.PageLifetime = m_Scene.GetAuthoringLifetime(r.Page);
    r.PageComponent = m_Scene.GetAuthoringComponentVersion(r.Page, typeid(UIRectComponent));
    r.Name = root.GetComponent<UIRadioGroupComponent>().Action;
    if (!StampRequestIssuer(r))
        return false;
    m_ScreenRequests.push_back(std::move(r));
    return true;
}
bool RuntimeUI::CommitTab(const ScreenRequest &r)
{
    auto root = m_Scene.GetEntityByUUID(r.ID), tab = m_Scene.GetEntityByUUID(r.Option);
    if (!root || !root.HasComponent<UITabsComponent>() || !root.HasComponent<UIRadioGroupComponent>() ||
        !tab || !tab.HasComponent<UITabComponent>() || !Member(r.ID, r.Option, true) ||
        m_Scene.GetAuthoringLifetime(r.ID) != r.Lifetime ||
        m_Scene.GetAuthoringComponentVersion(r.ID, typeid(UIRadioGroupComponent)) != r.Component ||
        m_Scene.GetAuthoringComponentVersion(r.ID, typeid(UITabsComponent)) != r.RootComponent ||
        m_Scene.GetAuthoringLifetime(r.Option) != r.OptionLifetime ||
        m_Scene.GetAuthoringComponentVersion(r.Option, typeid(UIButtonComponent)) != r.OptionComponent ||
        m_Scene.GetAuthoringComponentVersion(r.Option, typeid(UITabComponent)) != r.PresentationComponent ||
        tab.GetComponent<UITabComponent>().Page != r.Page ||
        m_Scene.GetAuthoringLifetime(r.Page) != r.PageLifetime ||
        m_Scene.GetAuthoringComponentVersion(r.Page, typeid(UIRectComponent)) != r.PageComponent ||
        root.GetComponent<UIRadioGroupComponent>().Action != r.Name || (r.Notify && !Live(r.Option)))
    {
        CompositionProblem("Tab request source/page retired or became unavailable; selection unchanged");
        return false;
    }
    auto &v = m_Values.at(r.ID);
    if (v.Selected == r.Option)
        return false;
    if (r.Notify)
    {
        Action e;
        e.Entity = e.Control = r.ID;
        e.Kind = UIEventKind::SelectionChanged;
        e.Name = r.Name;
        e.PreviousSelection = v.Selected;
        e.Selection = r.Option;
        e.Page = r.Page;
        e.PresentationComponent = r.PresentationComponent;
        e.SourceLifetime = r.OptionLifetime;
        e.SourceComponent = r.OptionComponent;
        e.ControlLifetime = r.Lifetime;
        e.ControlComponent = r.Component;
        if (!QueueEvent(std::move(e)))
            return false;
    }
    v.Selected = r.Option;
    v.SelectedLifetime = r.OptionLifetime;
    v.SelectedComponent = r.OptionComponent;
    ++m_DropdownRevision;
    ++m_Diagnostics.PageChanges;
    return true;
}
void RuntimeUI::PreparePages()
{
    // Parents retire first even when UUID order differs. One pending focus transfer
    // then represents departure from the outermost changed page.
    auto published = m_Resolved.Pages;
    auto depth = [&](uint64_t id) {
        size_t n = 0;
        for (; id && n < Scene::MaxDepth; ++n)
        {
            auto e = m_Scene.GetEntityByUUID(id);
            if (!e)
                break;
            id = m_Scene.GetRelationship(e).Parent;
        }
        return n;
    };
    std::stable_sort(published.begin(), published.end(),
                     [&](const auto &a, const auto &b) { return depth(a.Page) < depth(b.Page); });
    for (const auto &old : published)
    {
        uint64_t page = 0;
        auto selected = m_Scene.GetEntityByUUID(PageSelection(old.Group));
        if (selected && selected.HasComponent<UITabComponent>() && PageActive(old.Group))
            page = selected.GetComponent<UITabComponent>().Page;
        if (page == old.Page)
            continue;
        auto focus = m_Input.Focused();
        const bool keyboard = m_KeyboardOwns;
        bool departing = focus && Descendant(focus, old.Page);
        auto root = m_Scene.GetEntityByUUID(old.Group);
        bool remember = root && root.HasComponent<UITabsComponent>() &&
                        root.GetComponent<UITabsComponent>().RememberFocus;
        if (departing && remember)
            RememberFocus(old.Page, focus, m_KeyboardOwns);
        const auto groupNode = m_Resolved.Find(old.Group);
        const bool ownsScope = !m_Resolved.Scope || (groupNode && groupNode->Scope == m_Resolved.Scope);
        const auto popup = ActiveDropdown();
        const bool ownsPopup = popup && Descendant(popup, old.Page);
        // A background page change must not interrupt the foreground modal's gesture.
        // Retire only its old content unless it owns the active interaction boundary.
        // This boundary preserves unrelated scope events; only old content retires.
        for (const auto &n : m_Resolved.Elements)
            if (Descendant(n.ID, old.Page))
                m_Input.Retire(m_Scene.GetIdentity(), n.ID);
        if (!ownsScope && !ownsPopup && !departing)
            continue;
        m_RetainPageEvents = true;
        if (auto d = ActiveDropdown())
        {
            auto root = m_Scene.GetEntityByUUID(d);
            if (root && Descendant(d, old.Page))
            {
                CloseOwnedPopup();
                ++m_ScreenRevision;
            }
        }
        if (focus)
            RememberFocus(m_Resolved.Scope, focus, keyboard);
        if (m_Slider.ID)
            EndSlider(false, true, true);
        CancelScreenInput();
        m_KeyboardOwns = keyboard;
        if (departing)
        {
            m_PageFocus = 0;
            if (remember)
                for (const auto &saved : m_ScreenFocus)
                    if (saved.Scope == page)
                        m_PageFocus = saved.Target;
            if (!m_PageFocus && selected && selected.HasComponent<UITabComponent>())
                m_PageFocus = selected.GetComponent<UITabComponent>().InitialFocus;
            // Store the destination page as a fallback search request, not a second focus stack.
            m_PageFocusPage = page;
            m_PageFallback = selected ? uint64_t(selected.GetUUID()) : 0;
        }
        else if (focus)
            m_PageFocus = focus;
    }
}
void RuntimeUI::FocusPage()
{
    if (!m_RetainPageEvents)
        return;
    auto target = m_PageFocus;
    auto page = m_PageFocusPage;
    auto fallback = m_PageFallback;
    m_PageFocus = m_PageFocusPage = m_PageFallback = 0;
    auto eligible = [&](uint64_t id) {
        if (!id)
            return false;
        auto n = m_Resolved.Find(id);
        return n && n->LayoutResolved && n->Visible && n->Enabled &&
               (!m_Resolved.Scope || n->Scope == m_Resolved.Scope) &&
               m_Scene.GetEntityByUUID(id).HasComponent<UIButtonComponent>() &&
               ((n->ScrollParent && EnsureVisible(0, id, false)) || Live(id)) && Live(id);
    };
    // A target can remain live in this screen after moving out of the page.
    // Test membership before EnsureVisible so stale memories cannot scroll it.
    auto pageTarget = [&](uint64_t id) { return (!page || Descendant(id, page)) && eligible(id); };
    if (!pageTarget(target))
    {
        target = 0;
        auto tab = m_Scene.GetEntityByUUID(fallback);
        if (tab && tab.HasComponent<UITabComponent>() &&
            pageTarget(tab.GetComponent<UITabComponent>().InitialFocus))
            target = tab.GetComponent<UITabComponent>().InitialFocus;
    }
    if (!target)
    {
        std::vector<std::pair<int, uint64_t>> candidates;
        for (const auto &n : m_Resolved.Elements)
            if (page && Descendant(n.ID, page) && n.LayoutResolved)
            {
                auto e = m_Scene.GetEntityByUUID(n.ID);
                if (e.HasComponent<UIButtonComponent>())
                    candidates.emplace_back(e.GetComponent<UIButtonComponent>().TabOrder, n.ID);
            }
        std::sort(candidates.begin(), candidates.end());
        for (auto [order, id] : candidates)
            if (eligible(id))
            {
                target = id;
                break;
            }
        if (!target && eligible(fallback))
            target = fallback;
    }
    if (target)
    {
        m_Input.SetFocus(m_Scene.GetIdentity(), target, m_Resolved.Buttons);
        m_PendingFocus = 0;
    }
    m_RetainPageEvents = false;
}
} // namespace Hazel
