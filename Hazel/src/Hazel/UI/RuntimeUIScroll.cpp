#include "hzpch.h"

#include "RuntimeUI.h"
#include <cmath>
namespace Hazel
{
using namespace GameUI;
namespace
{
bool Area(Rect r)
{
    return r.Size.x > 0 && r.Size.y > 0;
}
Rect Shift(Rect r, glm::vec2 delta)
{
    r.Position += delta;
    return r;
}
std::array<uint64_t, 4> Parts(const ScrollConfig &v)
{
    return {v.HorizontalTrack, v.HorizontalThumb, v.VerticalTrack, v.VerticalThumb};
}
} // namespace
void RuntimeUI::ScrollProblem(const std::string &message)
{
    m_Diagnostics.ScrollProblem = message;
    ControlProblem(message);
}
bool RuntimeUI::Descendant(uint64_t child, uint64_t ancestor) const
{
    for (size_t depth = 0; child && depth < Scene::MaxDepth; ++depth)
    {
        if (child == ancestor)
            return true;
        auto e = m_Scene.GetEntityByUUID(child);
        if (!e)
            return false;
        child = m_Scene.GetRelationship(e).Parent;
    }
    return false;
}
void RuntimeUI::ValidateScrollViews(uint64_t target) const
{
    size_t count = 0;
    for (auto h : m_Scene.GetAllEntitiesWith<UIScrollViewComponent>())
    {
        Entity e(h, &m_Scene);
        if (++count > 64)
            throw std::invalid_argument("ScrollView declaration limit exceeded (64)");
        auto id = uint64_t(e.GetUUID());
        const auto &v = e.GetComponent<UIScrollViewComponent>();
        auto parts = Parts(v);
        if (target && target != id && target != v.Content &&
            std::find(parts.begin(), parts.end(), target) == parts.end() &&
            !Descendant(target, v.Content))
            continue;
        Validate(v);
        if (!e.HasComponent<UIRectComponent>() || e.HasComponent<UICanvasComponent>() ||
            e.HasComponent<UILayoutComponent>() || e.HasComponent<UIButtonComponent>() ||
            e.HasComponent<UISliderComponent>() || e.HasComponent<UIProgressComponent>() ||
            e.HasComponent<UIRadioGroupComponent>())
            throw std::invalid_argument("ScrollView #" + std::to_string(id) +
                                        " needs a noninteractive free Rect viewport");
        auto content = m_Scene.GetEntityByUUID(v.Content);
        if (!content || !m_Scene.IsEntityValid(v.Content) ||
            !content.HasComponent<UIRectComponent>() || content.HasComponent<UICanvasComponent>() ||
            m_Scene.GetRelationship(content).Parent != e.GetUUID())
            throw std::invalid_argument("ScrollView #" + std::to_string(id) +
                                        ": choose a direct Rect content child");
        const auto &r = e.GetComponent<UIRectComponent>(),
                   &c = content.GetComponent<UIRectComponent>();
        for (int axis = 0; axis < 2; ++axis)
        {
            bool scroll = axis ? v.Vertical : v.Horizontal;
            auto mode = axis ? r.HeightMode : r.WidthMode;
            if (scroll && mode == SizeMode::Content)
                throw std::invalid_argument("ScrollView #" + std::to_string(id) +
                                            (axis ? ".height" : ".width") +
                                            ": viewport must be Fixed/Fill on a scroll axis");
            if (c.Anchor[axis] != 0 || c.Pivot[axis] != 0 || c.Offset[axis] != 0 ||
                c.Margin[axis] != 0 || c.Margin[axis + 2] != 0)
                throw std::invalid_argument(
                    "Scroll content #" + std::to_string(v.Content) +
                    ": viewport owns position; zero anchors, pivots, offsets and margins");
            if (scroll && (axis ? c.HeightMode : c.WidthMode) == SizeMode::Fill)
                throw std::invalid_argument("Scroll content #" + std::to_string(v.Content) +
                                            (axis ? ".height" : ".width") +
                                            ": indefinite Fill; choose Fixed/Content");
        }
        for (auto child : m_Scene.GetSubtree(content))
            if (m_Scene.GetEntityByUUID(child).HasComponent<UICanvasComponent>())
                throw std::invalid_argument("Scroll content #" + std::to_string(v.Content) +
                                            ": nested Canvas #" + std::to_string(child) +
                                            " is an independent surface; move it outside content");
        std::unordered_set<uint64_t> unique;
        for (int axis = 0; axis < 2; ++axis)
        {
            auto track = axis ? v.VerticalTrack : v.HorizontalTrack,
                 thumb = axis ? v.VerticalThumb : v.HorizontalThumb;
            if (!target && bool(track) != bool(thumb))
                throw std::invalid_argument(
                    "ScrollView scrollbar needs both track and thumb, or neither");
            for (auto part : {track, thumb})
                if (part &&
                    (!unique.insert(part).second || part == v.Content || !Decoration(id, part)))
                    throw std::invalid_argument(
                        "ScrollView #" + std::to_string(id) +
                        ": scrollbar parts must be distinct direct Sprite/Rect leaves");
        }
    }
}
void RuntimeUI::PrepareScrollState()
{
    m_ScrollParts.clear();
    m_Diagnostics.ScrollProblem.clear();
    if (m_ScrollIDs.size() > 64)
        throw std::invalid_argument("ScrollView declaration limit exceeded (64)");
    for (auto it = m_ScrollState.begin(); it != m_ScrollState.end();)
        if (std::find(m_ScrollIDs.begin(), m_ScrollIDs.end(), it->first) == m_ScrollIDs.end())
            it = m_ScrollState.erase(it);
        else
            ++it;
    for (auto id : m_ScrollIDs)
    {
        auto e = m_Scene.GetEntityByUUID(id);
        const auto &v = e.GetComponent<UIScrollViewComponent>();
        if (!e.HasComponent<UIRectComponent>() || e.HasComponent<UICanvasComponent>() ||
            e.HasComponent<UILayoutComponent>() || e.HasComponent<UIButtonComponent>() ||
            e.HasComponent<UISliderComponent>() || e.HasComponent<UIProgressComponent>() ||
            e.HasComponent<UIRadioGroupComponent>())
            throw std::invalid_argument("ScrollView #" + std::to_string(id) +
                                        " needs a noninteractive free Rect viewport");
        auto content = m_Scene.GetEntityByUUID(v.Content);
        if (!content || !content.HasComponent<UIRectComponent>() ||
            content.HasComponent<UICanvasComponent>() ||
            m_Scene.GetRelationship(content).Parent != e.GetUUID())
        {
            m_ScrollState.erase(id);
            ScrollProblem("ScrollView #" + std::to_string(id) +
                          ": invalid local content reference");
            continue;
        }
        for (auto child : m_Scene.GetSubtree(content))
            if (m_Scene.GetEntityByUUID(child).HasComponent<UICanvasComponent>())
                throw std::invalid_argument("Scroll content #" + std::to_string(v.Content) +
                                            ": nested Canvas #" + std::to_string(child) +
                                            " is independent; move it outside content");
        auto &s = m_ScrollState[id];
        auto lifetime = m_Scene.GetAuthoringLifetime(id),
             component = m_Scene.GetAuthoringComponentVersion(id, typeid(UIScrollViewComponent));
        auto cl = m_Scene.GetAuthoringLifetime(v.Content),
             cc = m_Scene.GetAuthoringComponentVersion(v.Content, typeid(UIRectComponent));
        if (s.Lifetime != lifetime || s.Component != component || s.Content != v.Content ||
            s.ContentLifetime != cl || s.ContentComponent != cc)
        {
            s = {};
            s.Lifetime = lifetime;
            s.Component = component;
            s.Content = v.Content;
            s.ContentLifetime = cl;
            s.ContentComponent = cc;
        }
        for (int axis = 0; axis < 2; ++axis)
        {
            auto track = axis ? v.VerticalTrack : v.HorizontalTrack,
                 thumb = axis ? v.VerticalThumb : v.HorizontalThumb;
            for (auto part : {track, thumb})
                if (part)
                {
                    if (Decoration(id, part))
                        m_ScrollParts[part] = {id, axis};
                    else
                        ScrollProblem("ScrollView #" + std::to_string(id) +
                                      ": broken scrollbar part #" + std::to_string(part));
                }
        }
    }
}
void RuntimeUI::ApplyScrollPresentation()
{
    m_Build.Scrolls.clear();
    // Layout sizes are immutable inputs to presentation. Clamping uses no paint/text metrics.
    for (auto id : m_ScrollIDs)
    {
        auto it = m_ScrollState.find(id);
        if (it == m_ScrollState.end())
            continue;
        auto root = m_Build.Find(id), content = m_Build.Find(it->second.Content);
        if (!root || !root->LayoutResolved)
            continue;
        auto e = m_Scene.GetEntityByUUID(id);
        const auto &v = e.GetComponent<UIScrollViewComponent>();
        auto view = root->LayoutBounds.Inset(e.GetComponent<UIRectComponent>().Padding);
        auto extent =
            content && content->LayoutResolved ? content->LayoutBounds.Size : glm::vec2(0);
        if (extent.x > 1e6f || extent.y > 1e6f)
            throw std::invalid_argument("ScrollView #" + std::to_string(id) +
                                        ": static content extent exceeds 1000000 logical units");
        glm::vec2 range = glm::max(glm::vec2(0), extent - view.Size);
        if (!v.Horizontal || view.Size.x <= 0)
            range.x = 0;
        if (!v.Vertical || view.Size.y <= 0)
            range.y = 0;
        it->second.Offset = glm::clamp(it->second.Offset, glm::vec2(0), range);
        m_Build.Scrolls.push_back({id, v.Content, 0, view, extent, it->second.Offset, range});
    }
    for (auto &n : m_Build.Elements)
        if (n.IsCanvas)
        {
            n.Displacement = {};
            n.ScrollParent = 0;
            n.InheritedClip = n.Bounds;
        }
    const auto surface = m_Build.Surface;
    auto ratio = surface.Framebuffer / surface.Viewport;
    for (auto index : m_Build.PainterOrder)
    {
        auto &n = m_Build.Elements[index];
        auto parent = m_Build.Find(n.Parent);
        n.Displacement = {};
        n.ScrollParent = 0;
        auto inherited = n.InheritedClip;
        if (parent && parent->LayoutResolved && parent->Canvas == n.Canvas)
        {
            n.Displacement = parent->Displacement;
            n.ScrollParent = parent->ScrollParent;
            inherited = parent->IsCanvas ? parent->Clip : parent->InheritedClip;
            auto p = m_Scene.GetEntityByUUID(parent->ID);
            bool content = false;
            if (p.HasComponent<UIScrollViewComponent>())
                if (auto state = m_ScrollState.find(parent->ID);
                    state != m_ScrollState.end() && state->second.Content == n.ID)
                {
                    n.Displacement -= state->second.Offset;
                    n.ScrollParent = parent->ID;
                    inherited = inherited.Intersect(
                        parent->Bounds.Inset(p.GetComponent<UIRectComponent>().Padding));
                    content = true;
                }
            if (!content && !parent->IsCanvas && p.GetComponent<UIRectComponent>().ClipChildren)
                inherited = inherited.Intersect(parent->Bounds);
        }
        n.Bounds = Shift(n.LayoutBounds, n.Displacement);
        n.Slot = Shift(n.LayoutSlot, n.Displacement);
        n.ParentContent =
            Shift(n.LayoutParentContent, parent ? parent->Displacement : glm::vec2(0));
        n.InheritedClip = inherited;
        n.Clip = inherited.Intersect(n.Bounds);
        n.PixelBounds = {n.Bounds.Position * n.Scale, n.Bounds.Size * n.Scale};
        n.FramebufferBounds = {n.PixelBounds.Position * ratio, n.PixelBounds.Size * ratio};
        n.FramebufferClip = FramebufferCoverage({n.Clip.Position * n.Scale, n.Clip.Size * n.Scale},
                                                ratio, surface.Framebuffer);
        n.PixelClip = {n.FramebufferClip.Position / ratio, n.FramebufferClip.Size / ratio};
        n.FocusEligible = false;
        n.Hit = HitPolicy::PassThrough;
        n.ButtonIndex = std::numeric_limits<size_t>::max();
    }
    for (auto &g : m_Build.Guides)
        if (auto owner = m_Build.Find(g.Owner))
            g.Bounds = Shift(g.LayoutBounds, owner->Displacement);
    std::sort(m_Build.Scrolls.begin(), m_Build.Scrolls.end(), [&](const auto &a, const auto &b) {
        auto x = m_Build.Find(a.ID)->Order, y = m_Build.Find(b.ID)->Order;
        return x == y ? a.ID < b.ID : x < y;
    });
    for (auto &g : m_Build.Scrolls)
    {
        auto root = m_Build.Find(g.ID);
        auto e = m_Scene.GetEntityByUUID(g.ID);
        const auto &v = e.GetComponent<UIScrollViewComponent>();
        g.Parent = root->ScrollParent;
        g.Viewport = Shift(g.Viewport, root->Displacement);
        for (int axis = 0; axis < 2; ++axis)
        {
            auto track = axis ? v.VerticalTrack : v.HorizontalTrack,
                 thumb = axis ? v.VerticalThumb : v.HorizontalThumb;
            auto n = m_Build.Find(track), h = m_Build.Find(thumb);
            if (n && h && n->LayoutResolved && h->LayoutResolved && n->Visible && h->Visible &&
                n->Enabled && h->Enabled &&
                Decoration(g.ID, track) && Decoration(g.ID, thumb))
            {
                g.Tracks[axis] = n->Bounds.Intersect(root->Bounds);
                g.Thumbs[axis] = ScrollThumb(g.Tracks[axis], g.Viewport.Size[axis], g.Extent[axis],
                                             g.Offset[axis], v.MinimumThumb, axis);
            }
        }
    }
}
const ScrollGeometry *RuntimeUI::ScrollNode(uint64_t id) const
{
    for (const auto &g : m_Resolved.Scrolls)
        if (g.ID == id)
            return &g;
    return nullptr;
}
bool RuntimeUI::ScrollAvailable(uint64_t id) const
{
    auto n = m_Resolved.Find(id);
    auto g = ScrollNode(id);
    return n && g && n->LayoutResolved && n->Visible && n->Enabled &&
           (n->Canvas || (m_Resolved.PreviewOrphans && !m_Scene.IsRunning())) &&
           (!m_Resolved.Scope || n->Canvas == m_Resolved.Scope) && Area(g->Viewport);
}
bool RuntimeUI::ScrollEligible(uint64_t id) const
{
    auto n = m_Resolved.Find(id);
    return ScrollAvailable(id) && Area(n->FramebufferClip);
}
ScrollGeometry RuntimeUI::ScrollInfo(uint64_t id) const
{
    auto g = ScrollNode(id);
    if (!g)
        throw std::invalid_argument("ScrollView has no published viewport; open/preview it first");
    return *g;
}
void RuntimeUI::RepublishScroll()
{
    const auto start = std::chrono::steady_clock::now();
    m_Build = m_Resolved;
    ApplyScrollPresentation();
    BuildHitTargets();
    m_Build.Generation = ++m_NextGeneration;
    std::swap(m_Resolved, m_Build);
    PublishScope();
    ReconcileSlider();
    ReconcileScrollCapture();
    PruneActions();
    ++m_Diagnostics.ScrollPublications;
    m_Diagnostics.LastScrollMilliseconds =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
}
void RuntimeUI::ScrollTo(uint64_t id, glm::vec2 offset, bool normalized)
{
    if (!std::isfinite(offset.x) || !std::isfinite(offset.y) || !ScrollAvailable(id))
        throw std::invalid_argument(
            "ScrollTo #" + std::to_string(id) + " requires finite offsets and an available viewport in scope #" + std::to_string(m_Resolved.Scope));
    auto g = *ScrollNode(id);
    if (normalized)
        offset = glm::clamp(offset, glm::vec2(0), glm::vec2(1)) * g.Range;
    offset = glm::clamp(offset, glm::vec2(0), g.Range);
    if (offset == g.Offset)
        return;
    if (m_Bar.Owner)
        CancelScrollCapture(true);
    m_ScrollState.at(id).Offset = offset;
    RepublishScroll();
}
bool RuntimeUI::ScrollVisual(uint64_t id, Rect &bounds, bool &visible) const
{
    auto it = m_ScrollParts.find(id);
    if (it == m_ScrollParts.end())
        return false;
    auto g = ScrollNode(it->second.first);
    int axis = it->second.second;
    visible = g && g->Range[axis] > 0 && Area(g->Tracks[axis]);
    if (!visible)
        return true;
    auto e = m_Scene.GetEntityByUUID(g->ID);
    auto &v = e.GetComponent<UIScrollViewComponent>();
    bounds = id == (axis ? v.VerticalThumb : v.HorizontalThumb) ? g->Thumbs[axis] : g->Tracks[axis];
    return true;
}
void RuntimeUI::CancelScrollCapture(bool neutral)
{
    if (!m_Bar.Owner)
        return;
    m_Bar = {};
    m_Input.CancelCapture();
    m_ScrollNeutral |= neutral;
}
void RuntimeUI::ReconcileScrollCapture()
{
    if (!m_Bar.Owner)
        return;
    auto g = ScrollNode(m_Bar.Owner);
    auto e = m_Scene.GetEntityByUUID(m_Bar.Owner);
    const auto *config = e && e.HasComponent<UIScrollViewComponent>()
                             ? &e.GetComponent<UIScrollViewComponent>() : nullptr;
    auto track = m_Resolved.Find(m_Bar.Track);
    if (!g || !e || !ScrollEligible(m_Bar.Owner) ||
        !config || (m_Bar.Axis ? config->VerticalTrack : config->HorizontalTrack) != m_Bar.Track ||
        (m_Bar.Axis ? config->VerticalThumb : config->HorizontalThumb) != m_Bar.Thumb ||
        config->MinimumThumb != m_Bar.MinimumThumb || g->Range[m_Bar.Axis] <= 0 ||
        !track || !Area(track->FramebufferClip) ||
        m_Scene.GetAuthoringLifetime(m_Bar.Owner) != m_Bar.Lifetime ||
        m_Scene.GetAuthoringComponentVersion(m_Bar.Owner, typeid(UIScrollViewComponent)) !=
            m_Bar.Component ||
        !Decoration(m_Bar.Owner, m_Bar.Track) || !Decoration(m_Bar.Owner, m_Bar.Thumb) ||
        g->Tracks[m_Bar.Axis].Position != m_Bar.Bounds.Position ||
        g->Tracks[m_Bar.Axis].Size != m_Bar.Bounds.Size || g->Extent != m_Bar.Extent ||
        g->Viewport.Size != m_Bar.Viewport || m_Resolved.Surface.Framebuffer != m_Bar.Surface)
        CancelScrollCapture(true);
}
void RuntimeUI::RouteScrollBar(const Action &a)
{
    auto part = m_ScrollParts.find(a.Entity);
    if (part == m_ScrollParts.end())
        return;
    auto owner = part->second.first;
    int axis = part->second.second;
    auto g = ScrollNode(owner);
    if (!g || !ScrollEligible(owner))
        return;
    auto n = m_Resolved.Find(owner);
    auto ratio = m_Resolved.Surface.Framebuffer / m_Resolved.Surface.Viewport;
    auto point = a.Point / (ratio * n->Scale);
    if (a.Phase == RoutedPhase::Begin)
    {
        if (m_Slider.ID || !Area(g->Tracks[axis]))
        {
            m_Input.CancelCapture();
            return;
        }
        auto e = m_Scene.GetEntityByUUID(owner);
        auto &v = e.GetComponent<UIScrollViewComponent>();
        m_Bar = {};
        m_Bar.Owner = owner;
        m_Bar.Track = a.Entity;
        m_Bar.Thumb = axis ? v.VerticalThumb : v.HorizontalThumb;
        m_Bar.Axis = axis;
        m_Bar.MinimumThumb = v.MinimumThumb;
        m_Bar.Lifetime = m_Scene.GetAuthoringLifetime(owner);
        m_Bar.Component =
            m_Scene.GetAuthoringComponentVersion(owner, typeid(UIScrollViewComponent));
        m_Bar.Bounds = g->Tracks[axis];
        m_Bar.Extent = g->Extent;
        m_Bar.Viewport = g->Viewport.Size;
        m_Bar.Surface = m_Resolved.Surface.Framebuffer;
        m_Bar.Drag = g->Thumbs[axis].Contains(point);
        m_Bar.Grab = point[axis] - g->Thumbs[axis].Position[axis];
        m_ScrollFocus = owner;
        if (!m_Bar.Drag)
        {
            float page = g->Viewport.Size[axis] * .9f;
            auto next = g->Offset;
            next[axis] += point[axis] < g->Thumbs[axis].Position[axis] ? -page : page;
            m_ScrollState.at(owner).Offset = glm::clamp(next, glm::vec2(0), g->Range);
            RepublishScroll();
        }
    }
    else if (m_Bar.Owner == owner && (a.Phase == RoutedPhase::Move || a.Phase == RoutedPhase::End))
    {
        if (m_Bar.Drag)
        {
            float travel = g->Tracks[axis].Size[axis] - g->Thumbs[axis].Size[axis];
            if (travel > 0)
            {
                auto next = g->Offset;
                next[axis] =
                    std::clamp((point[axis] - m_Bar.Grab - g->Tracks[axis].Position[axis]) / travel,
                               0.f, 1.f) *
                    g->Range[axis];
                if (next != g->Offset)
                {
                    m_ScrollState.at(owner).Offset = next;
                    RepublishScroll();
                }
            }
        }
        if (a.Phase == RoutedPhase::End)
            CancelScrollCapture(true);
    }
}
uint64_t RuntimeUI::HoverScroll(glm::vec2 point) const
{
    uint64_t best = 0;
    size_t order = 0;
    for (const auto &g : m_Resolved.Scrolls)
    {
        auto n = m_Resolved.Find(g.ID);
        if (ScrollEligible(g.ID) &&
            n->FramebufferBounds.Intersect(n->FramebufferClip).Contains(point) &&
            (!best || n->Order >= order))
        {
            best = g.ID;
            order = n->Order;
        }
    }
    if (best)
    {
        auto hit = InputRouter::Occluder(m_Resolved.Buttons, point);
        if (hit && !Descendant(hit, best))
            return 0;
    }
    return best;
}
bool RuntimeUI::RouteScroll(const InputFrame &input)
{
    if (!input.Enabled || input.CancelDown || (!input.Scroll.x && !input.Scroll.y))
        return false;
    if (!std::isfinite(input.Scroll.x) || !std::isfinite(input.Scroll.y) ||
        std::abs(input.Scroll.x) > 1024 || std::abs(input.Scroll.y) > 1024)
        throw std::invalid_argument("Scroll delta must be finite and within 1024 units per axis");
    uint64_t id = 0;
    if (input.ScrollPage)
    {
        if (!m_KeyboardOwns && !m_Resolved.Modal)
            return false;
        auto focus = (m_KeyboardOwns || m_Resolved.Modal)
                         ? m_Resolved.Find(m_Input.Focused()) : nullptr;
        id = focus ? focus->ScrollParent : 0;
        if (!id && ScrollEligible(m_ScrollFocus))
            id = m_ScrollFocus;
        if (!id)
            for (const auto &g : m_Resolved.Scrolls)
                if (ScrollEligible(g.ID) && (m_Resolved.Modal || m_KeyboardOwns))
                {
                    id = g.ID;
                    break;
                }
    }
    else
        id = HoverScroll(input.Mouse);
    if (!id)
        return m_Resolved.Modal != 0;
    if (m_Input.Captured() || m_Slider.ID || m_Bar.Owner)
        return true; // capture owns geometry until release/cancel
    auto first = ScrollNode(id);
    auto firstEntity = m_Scene.GetEntityByUUID(id);
    const auto &settings = firstEntity.GetComponent<UIScrollViewComponent>();
    auto units = input.Scroll;
    if (input.ScrollPage && !settings.Vertical && settings.Horizontal && !units.x)
    {
        units.x = units.y;
        units.y = 0;
    }
    auto e = m_Scene.GetEntityByUUID(id);
    glm::vec2 remaining =
        units * (input.ScrollPage
                     ? first->Viewport.Size * .9f
                     : glm::vec2(e.GetComponent<UIScrollViewComponent>().PixelsPerUnit));
    bool changed = false;
    for (size_t depth = 0; id && depth < Scene::MaxDepth; ++depth)
    {
        auto g = ScrollNode(id);
        if (!g || !ScrollEligible(id))
            break;
        auto before = m_ScrollState.at(id).Offset;
        auto after = glm::clamp(before + remaining, glm::vec2(0), g->Range);
        remaining -= after - before;
        m_ScrollState.at(id).Offset = after;
        changed |= after != before;
        id = g->Parent;
    }
    if (changed)
        RepublishScroll();
    ++m_Diagnostics.ScrollRoutes;
    return true;
}
bool RuntimeUI::EnsureVisible(uint64_t view, uint64_t target, bool focus)
{
    auto n = m_Resolved.Find(target);
    if (!n || !n->LayoutResolved || !n->Visible || !n->Enabled || !n->ScrollParent ||
        !Area(n->Bounds) || (!m_Resolved.Scope ? false : n->Canvas != m_Resolved.Scope) ||
        (view &&
         (!ScrollAvailable(view) ||
          !Descendant(
              target,
              m_Scene.GetEntityByUUID(view).GetComponent<UIScrollViewComponent>().Content))))
    {
        ScrollProblem(
            "EnsureVisible requires a live descendant in the available scroll/screen scope");
        return false;
    }
    if (focus &&
        (!m_InputEnabled || !m_Scene.GetEntityByUUID(target).HasComponent<UIButtonComponent>() ||
         !m_Scene.GetEntityByUUID(target).GetComponent<UIButtonComponent>().Interactable))
    {
        ScrollProblem(
            "EnsureVisible focus requires an enabled input owner and interactive control");
        return false;
    }
    m_RevealBackup.clear();
    Rect box = n->Bounds;
    auto id = n->ScrollParent;
    bool changed = false;
    for (size_t depth = 0; id && depth < Scene::MaxDepth; ++depth)
    {
        auto g = ScrollNode(id);
        if (!g || !ScrollAvailable(id))
        {
            for (auto [saved, offset] : m_RevealBackup)
                m_ScrollState.at(saved).Offset = offset;
            return false;
        }
        auto &state = m_ScrollState.at(id);
        m_RevealBackup.emplace_back(id, state.Offset);
        auto &c = m_Scene.GetEntityByUUID(id).GetComponent<UIScrollViewComponent>();
        glm::vec2 delta{};
        for (int axis = 0; axis < 2; ++axis)
            if (axis ? c.Vertical : c.Horizontal)
                delta[axis] = RevealDelta(g->Viewport, box, c.RevealMargin, axis);
        auto after = glm::clamp(state.Offset + delta, glm::vec2(0), g->Range);
        box.Position -= after - state.Offset;
        changed |= after != state.Offset;
        state.Offset = after;
        id = g->Parent;
    }
    m_Build = m_Resolved;
    ApplyScrollPresentation();
    BuildHitTargets();
    auto result = m_Build.Find(target);
    bool ok = result && Area(result->FramebufferClip) && (!focus || result->FocusEligible);
    if (!ok)
    {
        for (auto [saved, offset] : m_RevealBackup)
            m_ScrollState.at(saved).Offset = offset;
        ScrollProblem(
            "EnsureVisible cannot reveal target through non-scroll clips or zero coverage");
        return false;
    }
    if (changed)
    {
        if (m_Bar.Owner)
            CancelScrollCapture(true);
        RepublishScroll();
    }
    if (focus && !Focus(target))
        return false;
    ++m_Diagnostics.ScrollReveals;
    return true;
}
bool RuntimeUI::NavigateScroll(InputFrame &input)
{
    if (m_Resolved.Scrolls.empty() || !input.Enabled || input.Editing || m_Input.Captured() ||
        m_Bar.Owner || input.CancelDown ||
        !(input.Navigate.x || input.Navigate.y || input.NextPressed || input.PreviousPressed))
        return false;
    m_NavigationButtons.clear();
    for (auto b : m_Resolved.Buttons)
    {
        if (!b.Focusable)
            continue;
        auto n = m_Resolved.Find(b.ID);
        if (n->ScrollParent && n->Visible && n->Enabled && b.Hit != HitPolicy::PassThrough)
        {
            auto &c = m_Scene.GetEntityByUUID(b.ID).GetComponent<UIButtonComponent>();
            if (c.Interactable && (c.Kind != ControlKind::Radio || Member(c.Group, b.ID, false)))
            {
                b.Interactable = true;
                b.Hit = HitPolicy::Control;
                b.Clip = b.Bounds;
            }
        }
        m_NavigationButtons.push_back(std::move(b));
    }
    // First entry follows logical order, even when no control currently has
    // framebuffer coverage. A remembered automatic focus is not UI ownership.
    if (!m_KeyboardOwns && !m_Resolved.Modal)
        m_Input.SetFocus(m_Scene.GetIdentity(), 0, m_Resolved.Buttons);
    for (size_t tries = 0; tries < 64; ++tries)
    {
        auto next = m_Input.NavigationTarget(m_NavigationButtons, input.Navigate,
                                             input.PreviousPressed ? -1
                                             : input.NextPressed   ? 1
                                                                   : 0);
        if (!next)
            break;
        auto n = m_Resolved.Find(next);
        if (n && ((n->ScrollParent && EnsureVisible(0, next, false)) || Live(next)) && Live(next))
        {
            m_KeyboardOwns = true;
            m_Input.SetFocus(m_Scene.GetIdentity(), next, m_Resolved.Buttons);
            break;
        }
        auto it = std::find_if(m_NavigationButtons.begin(), m_NavigationButtons.end(),
                               [&](auto &b) { return b.ID == next; });
        if (it == m_NavigationButtons.end())
            break;
        it->Interactable = false;
    }
    input.Navigate = {};
    input.NextDown = input.PreviousDown = input.NextPressed = input.PreviousPressed = false;
    input.FocusNavigation = false;
    return true;
}
} // namespace Hazel
