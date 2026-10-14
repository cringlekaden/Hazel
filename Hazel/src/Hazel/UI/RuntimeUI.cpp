#include "hzpch.h"
#include "RuntimeUI.h"
#include "Hazel/Renderer/MSDFData.h"
#include "Hazel/Renderer/RenderCommand.h"
#include <cmath>
#include <cstring>
#include <limits>
#include <glm/gtc/matrix_transform.hpp>
namespace Hazel
{
using namespace GameUI;
namespace
{
struct DependencyBytes
{
    std::vector<uint8_t> &Bytes;
    template <class T> void Add(T value)
    {
        static_assert(std::is_arithmetic_v<T>);
        auto begin = reinterpret_cast<const uint8_t *>(&value);
        Bytes.insert(Bytes.end(), begin, begin + sizeof(value));
    }
    template <glm::length_t L, class T, glm::qualifier Q> void Add(const glm::vec<L, T, Q> &value)
    {
        for (int i = 0; i < L; ++i)
            Add(value[i]);
    }
    void Add(const std::string &value)
    {
        Add(value.size());
        Bytes.insert(Bytes.end(), value.begin(), value.end());
    }
};
Rect Scaled(Rect r, glm::vec2 scale)
{
    return {r.Position * scale, r.Size * scale};
}
bool Nonempty(Rect r)
{
    return r.Size.x > 0 && r.Size.y > 0;
}
} // namespace
void RuntimeUI::CancelInput()
{
    if (m_InputEnabled || m_Input.Captured() || m_Input.Focused() || !m_Actions.empty())
    {
        m_OuterDrops += m_Input.DroppedActions();
        m_Input.Reset();
    }
    m_Actions.clear();
    m_InputEnabled = false;
    m_PointerOver = m_KeyboardOwns = false;
}
void RuntimeUI::Reset()
{
    CancelInput();
    m_Resolved.Valid = false;
    m_Resolved.Modal = 0;
    m_Resolved.Elements.clear();
    m_Resolved.Index.clear();
    m_Resolved.Buttons.clear();
    m_Resolved.PainterOrder.clear();
    m_Resolved.Warnings.clear();
    m_Source.clear();
    m_Cache.clear();
    m_Diagnostics = {};
    m_OuterDrops = m_ReportedDrops = 0;
    // Generation IDs never alias an older published snapshot, including Play/Stop.
}
void RuntimeUI::Retire(uint64_t id)
{
    if (m_Input.Focused() == id)
        m_KeyboardOwns = false;
    m_Input.Retire(m_Scene.GetIdentity(), id);
    m_Cache.erase(id);
    m_Actions.erase(std::remove_if(m_Actions.begin(), m_Actions.end(),
                                   [&](const Action &a) { return a.Entity == id; }),
                    m_Actions.end());
}
void RuntimeUI::CaptureSources(SurfaceMetrics surface, bool previewOrphans)
{
    ++m_Diagnostics.SourceChecks;
    m_Candidate.clear();
    m_RectIDs.clear();
    m_CanvasIDs.clear();
    DependencyBytes state{m_Candidate};
    state.Add(m_Scene.GetIdentity());
    state.Add(surface.Viewport);
    state.Add(surface.Framebuffer);
    state.Add(previewOrphans);
    // Only layout/input dependencies are inspected. Text/image/color edits are
    // paint dependencies, read by Draw and its text cache, and never cause reflow.
    for (auto h : m_Scene.GetAllEntitiesWith<UIRectComponent>())
    {
        Entity e(h, &m_Scene);
        auto id = uint64_t(e.GetUUID());
        if (!m_Scene.IsEntityValid(id))
            continue;
        m_RectIDs.push_back(id);
        state.Add(id);
        auto relation = m_Scene.GetRelationship(e);
        state.Add(uint64_t(relation.Parent));
        state.Add(relation.Order);
        // Interrupted chains still publish canvas/inherited-state diagnostics.
        // Observe their ordinary ancestor links until another tracked UI node.
        auto ancestor = m_Scene.GetEntityByUUID(relation.Parent);
        size_t depth = 0;
        while (ancestor && !ancestor.HasComponent<UIRectComponent>() &&
               !ancestor.HasComponent<UICanvasComponent>())
        {
            if (depth++ >= Scene::MaxDepth)
                throw std::runtime_error("UI interrupted ancestry exceeds depth 256");
            state.Add(uint64_t(ancestor.GetUUID()));
            auto link = m_Scene.GetRelationship(ancestor);
            state.Add(uint64_t(link.Parent));
            state.Add(link.Order);
            ancestor = m_Scene.GetEntityByUUID(link.Parent);
        }
        state.Add(uint64_t(0));
        auto &r = e.GetComponent<UIRectComponent>();
        state.Add(r.AnchorMin);
        state.Add(r.AnchorMax);
        state.Add(r.Offset);
        state.Add(r.SizeDelta);
        state.Add(r.Pivot);
        state.Add(r.MinimumSize);
        state.Add(r.Margin);
        state.Add(r.Padding);
        state.Add(r.Visible);
        state.Add(r.Enabled);
        state.Add(r.ClipChildren);
        state.Add(r.Flex);
        state.Add(e.HasComponent<UILayoutComponent>());
        if (e.HasComponent<UILayoutComponent>())
        {
            auto &f = e.GetComponent<UILayoutComponent>();
            state.Add(int(f.Direction));
            state.Add(int(f.Main));
            state.Add(int(f.Cross));
            state.Add(f.Gap);
        }
        state.Add(e.HasComponent<UIButtonComponent>());
        if (e.HasComponent<UIButtonComponent>())
        {
            auto &b = e.GetComponent<UIButtonComponent>();
            state.Add(b.Action);
            state.Add(b.TabOrder);
            state.Add(b.Interactable);
        }
        state.Add(e.HasComponent<UICanvasComponent>());
    }
    state.Add(uint64_t(0));
    for (auto h : m_Scene.GetAllEntitiesWith<UICanvasComponent>())
    {
        Entity e(h, &m_Scene);
        auto id = uint64_t(e.GetUUID());
        if (!m_Scene.IsEntityValid(id))
            continue;
        m_CanvasIDs.push_back(id);
        state.Add(id);
        auto &c = e.GetComponent<UICanvasComponent>();
        state.Add(c.ReferenceSize);
        state.Add(c.SafeInsets);
        state.Add(c.SortOrder);
        state.Add(c.Visible);
        state.Add(c.Enabled);
        state.Add(c.Modal);
        // Independent canvases may have ordinary world ancestors. Their parent
        // links are dependencies too, without scanning unrelated world entities.
        auto ancestor = e;
        size_t depth = 0;
        while (ancestor)
        {
            if (depth++ >= Scene::MaxDepth)
                throw std::runtime_error("UI canvas ancestry exceeds depth 256");
            auto rel = m_Scene.GetRelationship(ancestor);
            state.Add(uint64_t(rel.Parent));
            state.Add(rel.Order);
            ancestor = m_Scene.GetEntityByUUID(rel.Parent);
            if (ancestor && !m_Scene.IsEntityValid(ancestor.GetUUID()))
                break;
        }
        state.Add(uint64_t(0));
    }
    state.Add(uint64_t(0));
    for (auto h : m_Scene.GetAllEntitiesWith<UILayoutComponent>())
    {
        Entity e(h, &m_Scene);
        if (m_Scene.IsEntityValid(e.GetUUID()) && !e.HasComponent<UIRectComponent>())
            state.Add(uint64_t(e.GetUUID()));
    }
    state.Add(uint64_t(0));
    for (auto h : m_Scene.GetAllEntitiesWith<UIButtonComponent>())
    {
        Entity e(h, &m_Scene);
        if (m_Scene.IsEntityValid(e.GetUUID()) && !e.HasComponent<UIRectComponent>())
            state.Add(uint64_t(e.GetUUID()));
    }
    if (m_RectIDs.size() > InputRouter::MaxButtons)
        throw std::runtime_error("UI declared rect limit exceeded (4096, including hidden rects)");
}
void RuntimeUI::ResolveSurface(SurfaceMetrics surface, bool previewOrphans)
{
    m_Scene.CheckThread();
    const auto start = std::chrono::steady_clock::now();
    try
    {
        CaptureSources(surface, previewOrphans);
        if (m_Resolved.Valid && m_Candidate == m_Source)
        {
            ++m_Diagnostics.Reuses;
            return;
        }
        Build(surface, previewOrphans);
        m_Build.Generation = ++m_NextGeneration;
        m_Build.Valid = true;
        std::swap(m_Resolved, m_Build);
        m_Source.swap(m_Candidate);
        m_Input.Reconcile(m_Scene.GetIdentity(), m_Resolved.Buttons, m_Resolved.Generation);
        PruneActions();
        if (!m_Input.Focused())
            m_KeyboardOwns = false;
        // Remove retired cache entries, including now-interrupted rect subtrees.
        for (auto it = m_Cache.begin(); it != m_Cache.end();)
        {
            auto n = m_Resolved.Find(it->first);
            if (!n || !n->LayoutResolved)
                it = m_Cache.erase(it);
            else
                ++it;
        }
        ++m_Diagnostics.Resolves;
        m_Diagnostics.LastResolveMilliseconds =
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start)
                .count();
    }
    catch (...)
    {
        CancelInput();
        m_Resolved.Valid = false;
        throw;
    }
}
void RuntimeUI::Build(SurfaceMetrics surface, bool previewOrphans)
{
    // FitCanvas supplies finite/nonnegative logical validation, even at zero size.
    FitCanvas(surface.Viewport, {});
    if (!std::isfinite(surface.Framebuffer.x) || !std::isfinite(surface.Framebuffer.y) ||
        surface.Framebuffer.x < 0 || surface.Framebuffer.y < 0 ||
        surface.Framebuffer.x > 2147483000.f || surface.Framebuffer.y > 2147483000.f)
        throw std::invalid_argument("Invalid UI framebuffer size");
    m_Build.Elements.clear();
    m_Build.PainterOrder.clear();
    m_Build.Buttons.clear();
    m_Build.Warnings.clear();
    m_Build.Modal = 0;
    m_Build.Valid = false;
    m_Build.Surface = surface;
    m_Build.PreviewOrphans = previewOrphans;
    m_Build.SceneIdentity = m_Scene.GetIdentity();
    auto add = [&](uint64_t id) {
        auto previous = m_Build.Index.find(id);
        if (previous != m_Build.Index.end() && previous->second < m_Build.Elements.size() &&
            m_Build.Elements[previous->second].ID == id)
            return;
        auto e = m_Scene.GetEntityByUUID(id);
        ResolvedElement n;
        n.ID = id;
        n.Parent = m_Scene.GetRelationship(e).Parent;
        n.IsCanvas = e.HasComponent<UICanvasComponent>();
        n.Visible = n.Enabled = true;
        auto ancestor = e;
        size_t depth = 0;
        bool interrupted = false, foundCanvas = false;
        while (ancestor)
        {
            if (depth++ >= Scene::MaxDepth)
                throw std::runtime_error("UI ancestry exceeds depth 256");
            if (ancestor.HasComponent<UIRectComponent>())
            {
                auto &r = ancestor.GetComponent<UIRectComponent>();
                Validate(r);
                if (!n.IsCanvas || ancestor != e)
                {
                    n.Visible &= r.Visible;
                    n.Enabled &= r.Enabled;
                }
            }
            if (ancestor.HasComponent<UICanvasComponent>())
            {
                auto &c = ancestor.GetComponent<UICanvasComponent>();
                FitCanvas(surface.Viewport, c);
                n.Visible &= c.Visible;
                n.Enabled &= c.Enabled;
                if (!foundCanvas)
                {
                    n.Canvas = ancestor.GetUUID();
                    foundCanvas = true;
                }
            }
            else if (ancestor != e && !foundCanvas && !ancestor.HasComponent<UIRectComponent>())
                interrupted = true;
            ancestor = m_Scene.GetEntityByUUID(m_Scene.GetRelationship(ancestor).Parent);
        }
        if (!n.IsCanvas && interrupted)
            m_Build.Warnings.push_back(
                {id, "Interrupted UI rect chain: an ancestor has no UI Rect/Canvas"});
        else if (!n.IsCanvas && !foundCanvas)
            m_Build.Warnings.push_back({id, "Orphan UI rect: runtime requires a Canvas ancestor"});
        if (n.IsCanvas && uint64_t(n.Parent))
            m_Build.Warnings.push_back(
                {id, "Nested/parented Canvas resolves independently against the viewport"});
        auto parent = m_Scene.GetEntityByUUID(n.Parent);
        n.FlowOwned = !n.IsCanvas && parent && parent.HasComponent<UIRectComponent>() &&
                      parent.HasComponent<UILayoutComponent>();
        if (n.FlowOwned)
            m_Build.Warnings.push_back(
                {id, "Flow owns position/arranged size; viewport move/resize is unavailable"});
        if (e.HasComponent<UIRectComponent>())
        {
            auto &r = e.GetComponent<UIRectComponent>();
            if (!std::isfinite(r.Flex) || r.Flex < 0)
                throw std::invalid_argument("Invalid UI flex");
        }
        if (e.HasComponent<UIButtonComponent>())
        {
            auto &b = e.GetComponent<UIButtonComponent>();
            if (b.Action.size() > 128 || b.Action.find('\0') != std::string::npos)
                throw std::invalid_argument("Invalid UI action (128 bytes, no NUL)");
        }
        // Keep hash nodes on stable membership, including changed-layout frames.
        m_Build.Index.insert_or_assign(id, m_Build.Elements.size());
        m_Build.Elements.push_back(n);
    };
    for (auto id : m_RectIDs)
        add(id);
    for (auto id : m_CanvasIDs)
        add(id);
    for (auto it = m_Build.Index.begin(); it != m_Build.Index.end();)
        if (it->second >= m_Build.Elements.size() || m_Build.Elements[it->second].ID != it->first)
            it = m_Build.Index.erase(it);
        else
            ++it;
    // Diagnose component configurations that otherwise silently disappear.
    for (auto h : m_Scene.GetAllEntitiesWith<UILayoutComponent>())
    {
        Entity e(h, &m_Scene);
        if (m_Scene.IsEntityValid(e.GetUUID()) && !e.HasComponent<UIRectComponent>())
            m_Build.Warnings.push_back({e.GetUUID(), "UI Flow needs a UI Rect"});
    }
    for (auto h : m_Scene.GetAllEntitiesWith<UIButtonComponent>())
    {
        Entity e(h, &m_Scene);
        if (m_Scene.IsEntityValid(e.GetUUID()) && !e.HasComponent<UIRectComponent>())
            m_Build.Warnings.push_back({e.GetUUID(), "UI Button needs a UI Rect"});
    }
    if (surface.Viewport.x == 0 || surface.Viewport.y == 0 || surface.Framebuffer.x == 0 ||
        surface.Framebuffer.y == 0)
        return;
    m_Canvases.clear();
    for (auto id : m_CanvasIDs)
        m_Canvases.push_back(m_Scene.GetEntityByUUID(id));
    std::sort(m_Canvases.begin(), m_Canvases.end(), [](Entity a, Entity b) {
        auto x = a.GetComponent<UICanvasComponent>().SortOrder,
             y = b.GetComponent<UICanvasComponent>().SortOrder;
        return x == y ? uint64_t(a.GetUUID()) < uint64_t(b.GetUUID()) : x < y;
    });
    for (auto canvas : m_Canvases)
    {
        auto &n = m_Build.Elements[m_Build.Index.at(canvas.GetUUID())];
        auto &c = canvas.GetComponent<UICanvasComponent>();
        if (!n.Visible)
            continue;
        auto fit = FitCanvas(surface.Viewport, c);
        n.Scale = fit.Scale;
        n.Bounds = fit.Bounds;
        n.ParentContent = fit.Content;
        n.Clip = fit.Bounds;
        n.PixelBounds = Scaled(n.Bounds, glm::vec2(n.Scale));
        auto ratio = surface.Framebuffer / surface.Viewport;
        n.FramebufferBounds = Scaled(n.PixelBounds, ratio);
        n.FramebufferClip = FramebufferCoverage(n.PixelBounds, ratio, surface.Framebuffer);
        n.PixelClip = {n.FramebufferClip.Position / ratio, n.FramebufferClip.Size / ratio};
        n.LayoutResolved = true;
        if (c.Modal && n.Enabled)
            m_Build.Modal = n.ID;
        for (auto id : m_Scene.GetChildrenView(canvas.GetUUID()))
        {
            auto e = m_Scene.GetEntityByUUID(id);
            if (!e.HasComponent<UICanvasComponent>())
                Visit(e, fit.Content, fit.Bounds, fit.Scale, n.Enabled, canvas.GetUUID(), 0);
        }
    }
    if (previewOrphans)
    {
        auto fit = FitCanvas(surface.Viewport, {});
        for (auto id : m_RectIDs)
        {
            auto e = m_Scene.GetEntityByUUID(id);
            if (!e.HasComponent<UICanvasComponent>() &&
                !uint64_t(m_Scene.GetRelationship(e).Parent))
                Visit(e, fit.Content, fit.Bounds, fit.Scale, true, 0, 0);
        }
    }
    for (auto index : m_Build.PainterOrder)
    {
        auto &n = m_Build.Elements[index];
        auto e = m_Scene.GetEntityByUUID(n.ID);
        if (!e.HasComponent<UIButtonComponent>())
            continue;
        auto &b = e.GetComponent<UIButtonComponent>();
        bool scope = n.Canvas && (!m_Build.Modal || n.Canvas == m_Build.Modal);
        n.FocusEligible = scope && n.Enabled && b.Interactable && Nonempty(n.PixelClip);
        n.Hit = !scope            ? HitPolicy::PassThrough
                : n.FocusEligible ? HitPolicy::Control
                                  : HitPolicy::Block;
        n.ButtonIndex = m_Build.Buttons.size();
        // Route in framebuffer space against the exact integer coverage used by
        // scissoring. Converting that region back to viewport pixels for hit
        // tests can exclude a covered edge through reciprocal rounding.
        m_Build.Buttons.push_back({n.ID, n.FramebufferBounds, n.FramebufferClip, b.Action,
                                  b.TabOrder, true, n.FocusEligible, n.Hit});
    }
}
void RuntimeUI::Visit(Entity e, Rect parent, Rect clip, float scale, bool enabled, uint64_t canvas,
                      size_t depth, const Rect *arranged)
{
    if (!e || !m_Scene.IsEntityValid(e.GetUUID()) || !e.HasComponent<UIRectComponent>())
        return;
    if (depth >= Scene::MaxDepth)
        throw std::runtime_error("UI layout depth exceeded (256)");
    auto &r = e.GetComponent<UIRectComponent>();
    if (!r.Visible)
        return;
    auto &n = m_Build.Elements[m_Build.Index.at(e.GetUUID())];
    n.Bounds = arranged ? *arranged : ResolveRect(parent, r);
    n.ParentContent = parent;
    n.Scale = scale;
    n.Enabled = enabled && r.Enabled;
    n.Visible = true;
    n.Canvas = canvas;
    n.LayoutResolved = true;
    n.Order = m_Build.PainterOrder.size();
    m_Build.PainterOrder.push_back(m_Build.Index.at(n.ID));
    n.Clip = clip.Intersect(n.Bounds);
    n.PixelBounds = Scaled(n.Bounds, glm::vec2(scale));
    auto ratio = m_Build.Surface.Framebuffer / m_Build.Surface.Viewport;
    n.FramebufferBounds = Scaled(n.PixelBounds, ratio);
    n.FramebufferClip =
        FramebufferCoverage(Scaled(n.Clip, glm::vec2(scale)), ratio, m_Build.Surface.Framebuffer);
    n.PixelClip = {n.FramebufferClip.Position / ratio, n.FramebufferClip.Size / ratio};
    auto content = n.Bounds.Inset(r.Padding),
         childClip = r.ClipChildren ? clip.Intersect(n.Bounds) : clip;
    auto inside = parent.Intersect(n.Bounds);
    if (inside.Position != n.Bounds.Position || inside.Size != n.Bounds.Size)
        m_Build.Warnings.push_back({n.ID, "Resolved rect overflows its parent content"});
    const auto &children = m_Scene.GetChildrenView(e.GetUUID());
    if (e.HasComponent<UILayoutComponent>())
    {
        auto &cache = m_Cache[e.GetUUID()];
        cache.Items.clear();
        for (auto id : children)
        {
            auto child = m_Scene.GetEntityByUUID(id);
            FlowItem item;
            item.Visible = child.HasComponent<UIRectComponent>() &&
                           !child.HasComponent<UICanvasComponent>() && m_Scene.IsEntityValid(id);
            if (item.Visible)
            {
                auto &c = child.GetComponent<UIRectComponent>();
                item = {c.SizeDelta, c.MinimumSize, c.Margin, c.Flex, c.Visible};
            }
            cache.Items.push_back(item);
        }
        ArrangeFlow(content, e.GetComponent<UILayoutComponent>(), cache.Items, cache.Arranged);
        for (size_t i = 0; i < children.size(); ++i)
            if (cache.Items[i].Visible)
                Visit(m_Scene.GetEntityByUUID(children[i]), content, childClip, scale, n.Enabled,
                      canvas, depth + 1, &cache.Arranged[i]);
    }
    else
        for (auto id : children)
        {
            auto child = m_Scene.GetEntityByUUID(id);
            if (!child.HasComponent<UICanvasComponent>())
                Visit(child, content, childClip, scale, n.Enabled, canvas, depth + 1);
        }
}
void RuntimeUI::PruneActions()
{
    m_Actions.erase(std::remove_if(m_Actions.begin(), m_Actions.end(),
                                   [&](const Action &a) {
                                       auto n = m_Resolved.Find(a.Entity);
                                       return !Live(a.Entity) ||
                                              n->ButtonIndex >= m_Resolved.Buttons.size() ||
                                              m_Resolved.Buttons[n->ButtonIndex].Action != a.Name;
                                   }),
                    m_Actions.end());
}
bool RuntimeUI::Live(uint64_t id) const
{
    auto n = m_Resolved.Find(id);
    return m_Resolved.Valid && n && n->LayoutResolved && n->Visible && n->FocusEligible &&
           m_Scene.IsEntityValid(id);
}
void RuntimeUI::WarnOverflow()
{
    auto drops = m_OuterDrops + m_Input.DroppedActions();
    m_Diagnostics.DroppedActions = drops;
    auto now = std::chrono::steady_clock::now();
    if (drops != m_ReportedDrops &&
        (m_ReportedDrops == 0 || now - m_LastWarning >= std::chrono::seconds(1)))
    {
        HZ_CORE_WARN(
            "Runtime UI action queue full (64); dropping newest activation; total dropped {}",
            drops);
        m_ReportedDrops = drops;
        m_LastWarning = now;
    }
}
void RuntimeUI::UpdateInput(const InputFrame &pixels)
{
    if (!m_Resolved.Valid)
        throw std::logic_error("UI input requires a resolved snapshot");
    const auto start = std::chrono::steady_clock::now();
    m_PointerOver = false;
    auto input = pixels;
    const auto &surface = m_Resolved.Surface;
    const bool usableSurface = surface.Viewport.x > 0 && surface.Viewport.y > 0 &&
                               surface.Framebuffer.x > 0 && surface.Framebuffer.y > 0;
    input.Enabled &= usableSurface;
    auto ratio = usableSurface ? surface.Framebuffer / surface.Viewport : glm::vec2(1);
    input.Mouse *= ratio;
    input.PressPosition *= ratio;
    input.ReleasePosition *= ratio;
    m_InputEnabled = input.Enabled;
    uint64_t entryFocus = 0;
    int entryOrder =
        input.PreviousDown ? std::numeric_limits<int>::min() : std::numeric_limits<int>::max();
    for (auto &b : m_Resolved.Buttons)
    {
        if (b.Hit != HitPolicy::PassThrough && b.Bounds.Intersect(b.Clip).Contains(input.Mouse))
            m_PointerOver = true;
        if (!b.Interactable)
            continue;
        if (!entryFocus ||
            (input.PreviousDown ? b.TabOrder >= entryOrder : b.TabOrder < entryOrder))
        {
            entryFocus = b.ID;
            entryOrder = b.TabOrder;
        }
    }
    bool entering = !m_Resolved.Modal && !m_KeyboardOwns && input.FocusNavigation && entryFocus &&
                    input.Enabled && !input.CancelDown;
    if (!input.Enabled || input.CancelDown || (input.PointerPressed && !m_PointerOver))
        m_KeyboardOwns = false;
    else if (input.FocusNavigation && entryFocus)
        m_KeyboardOwns = true;
    input.KeyboardEnabled = !entering && (m_Resolved.Modal || m_KeyboardOwns);
    m_Input.Update(m_Scene.GetIdentity(), m_Resolved.Buttons, input, m_Resolved.Generation);
    if (entering)
        m_Input.SetFocus(m_Scene.GetIdentity(), entryFocus, m_Resolved.Buttons);
    if (!input.Enabled)
    {
        m_Actions.clear();
        m_PointerOver = false;
    }
    else
    {
        if (input.CancelDown)
            m_Actions.clear();
        Action a;
        while (m_Input.Consume(m_Scene.GetIdentity(), a))
        {
            if (m_Actions.size() < InputRouter::MaxActions)
                m_Actions.push_back(std::move(a));
            else
                ++m_OuterDrops;
        }
        PruneActions();
    }
    m_Diagnostics.InputGeneration = m_Resolved.Generation;
    m_Diagnostics.LastRoutingMilliseconds =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    WarnOverflow();
}
bool RuntimeUI::Consume(uint64_t entity, Action &action)
{
    if (m_Actions.empty())
        return false; // Idle per-control polling does no dependency traversal.
    ResolveSurface(m_Resolved.Surface,
                   m_Resolved.PreviewOrphans); // catches unnotified native writes before delivery
    PruneActions();
    for (auto it = m_Actions.begin(); it != m_Actions.end(); ++it)
        if (!entity || it->Entity == entity)
        {
            action = std::move(*it);
            m_Actions.erase(it);
            return true;
        }
    return false;
}
bool RuntimeUI::Bounds(uint64_t id, Rect &pixels) const
{
    auto n = m_Resolved.Find(id);
    if (!m_Resolved.Valid || !n || !n->LayoutResolved || !m_Scene.IsEntityValid(id))
        return false;
    pixels = n->PixelBounds;
    return true;
}
RuntimeUIDiagnostics RuntimeUI::Diagnostics() const
{
    auto result = m_Diagnostics;
    result.Focus = m_Input.Focused();
    result.Capture = m_Input.Captured();
    result.Hover = m_Input.Hovered();
    result.Modal = m_Resolved.Modal;
    result.InputEnabled = m_InputEnabled;
    result.GameplayBlocked = BlocksGameplay();
    result.PointerBlocked = BlocksPointer();
    return result;
}
namespace
{
glm::mat4 Transform(Rect r, float scale)
{
    return glm::translate(glm::mat4(1), glm::vec3((r.Position.x + r.Size.x * .5f) * scale,
                                                  -(r.Position.y + r.Size.y * .5f) * scale, 0)) *
           glm::scale(glm::mat4(1), glm::vec3(r.Size * scale, 1));
}
void Image(Rect bounds, float scale, const ResolvedSprite &sprite, const glm::vec4 &color, int id,
           const UINineSliceComponent *nine)
{
    auto draw = sprite;
    draw.Corners = SpriteCorners({.5f, .5f});
    draw.TilingFactor = 1;
    if (!nine)
    {
        Renderer2D::DrawSprite(Transform(bounds, scale), draw, color, id);
        return;
    }
    if (!sprite.Texture)
        throw std::runtime_error("Nine-slice requires a texture/sprite");
    const glm::vec2 sourceSize = glm::abs(sprite.UV[2] - sprite.UV[0]) *
                                 glm::vec2(sprite.Texture->GetWidth(), sprite.Texture->GetHeight());
    if (!std::isfinite(nine->BorderScale) || nine->BorderScale <= 0)
        throw std::invalid_argument("Invalid nine-slice border scale");
    auto b = nine->Borders;
    for (int i = 0; i < 4; i++)
        if (!std::isfinite(b[i]) || b[i] < 0)
            throw std::runtime_error("Invalid nine-slice border");
    if (b.x + b.z > sourceSize.x || b.y + b.w > sourceSize.y)
        throw std::runtime_error("Nine-slice borders exceed source region");
    float horizontal =
              b.x + b.z ? std::min(1.f, bounds.Size.x / ((b.x + b.z) * nine->BorderScale)) : 1,
          vertical =
              b.y + b.w ? std::min(1.f, bounds.Size.y / ((b.y + b.w) * nine->BorderScale)) : 1;
    float x[] = {0, b.x * horizontal * nine->BorderScale,
                 bounds.Size.x - b.z * horizontal * nine->BorderScale, bounds.Size.x},
          y[] = {0, b.y * vertical * nine->BorderScale,
                 bounds.Size.y - b.w * vertical * nine->BorderScale, bounds.Size.y};
    float u[] = {0, b.x / sourceSize.x, 1 - b.z / sourceSize.x, 1},
          v[] = {1, 1 - b.y / sourceSize.y, b.w / sourceSize.y, 0};
    auto uv = [&](float u, float v) {
        return sprite.UV[0] + (sprite.UV[2] - sprite.UV[0]) * glm::vec2(u, v);
    };
    for (int row = 0; row < 3; row++)
        for (int col = 0; col < 3; col++)
        {
            draw.UV = {uv(u[col], v[row + 1]), uv(u[col + 1], v[row + 1]), uv(u[col + 1], v[row]),
                       uv(u[col], v[row])};
            Rect cell{bounds.Position + glm::vec2(x[col], y[row]),
                      {x[col + 1] - x[col], y[row + 1] - y[row]}};
            if (cell.Size.x > 0 && cell.Size.y > 0)
                Renderer2D::DrawSprite(Transform(cell, scale), draw, color, id);
        }
}
} // namespace
void RuntimeUI::Draw()
{
    if (!m_Resolved.Valid || m_Resolved.PainterOrder.empty() ||
        m_Resolved.Surface.Viewport.x <= 0 || m_Resolved.Surface.Viewport.y <= 0)
        return;
    auto prior = RenderCommand::GetRasterState();
    auto state = prior;
    state.DepthTest = false;
    state.ClipEnabled = true;
    Camera camera(glm::ortho(0.f, m_Resolved.Surface.Viewport.x, -m_Resolved.Surface.Viewport.y,
                             0.f, -1.f, 1.f));
    try
    {
        for (auto index : m_Resolved.PainterOrder)
        {
            const auto &n = m_Resolved.Elements[index];
            auto e = m_Scene.GetEntityByUUID(n.ID);
            if (!m_Scene.IsEntityValid(e.GetUUID()))
                continue;
            const auto p = n.FramebufferClip.Position, z = n.FramebufferClip.Size;
            if (z.x <= 0 || z.y <= 0)
                continue;
            // Exact same inward-snapped coverage was used for hit eligibility.
            state.ClipRect = {prior.Viewport.x + int(p.x),
                              prior.Viewport.y + prior.Viewport.w - int(p.y + z.y), int(z.x),
                              int(z.y)};
            RenderCommand::SetRasterState(state);
            Renderer2D::BeginScene(camera, glm::mat4(1));
            auto tint = glm::vec4(1);
            const SpriteSource *overrideImage = nullptr;
            int stateIndex = 0;
            if (e.HasComponent<UIButtonComponent>())
            {
                auto &b = e.GetComponent<UIButtonComponent>();
                int index = n.ButtonIndex >= m_Resolved.Buttons.size()
                                ? 0
                                : int(m_Input.State(m_Resolved.Buttons[n.ButtonIndex]));
                if (m_Resolved.PreviewOrphans && !n.Canvas)
                    index = n.Enabled && b.Interactable ? int(ButtonState::Normal)
                                                       : int(ButtonState::Disabled);
                if (index == int(ButtonState::Focused) && !m_Resolved.Modal && !m_KeyboardOwns)
                    index = 0;
                tint = b.Colors[index];
                stateIndex = index;
                overrideImage = &b.Images[index];
            }
            const UINineSliceComponent *nine = e.HasComponent<UINineSliceComponent>()
                                                   ? &e.GetComponent<UINineSliceComponent>()
                                                   : nullptr;
            if (e.HasComponent<SpriteRendererComponent>())
            {
                auto &s = e.GetComponent<SpriteRendererComponent>();
                auto *sprite = m_Scene.RenderedSprite(e);
                if (overrideImage && overrideImage->index() != 0)
                {
                    if (!m_Scene.GetAssets())
                        throw std::runtime_error("UI image needs project assets");
                    const auto &assets = m_Scene.GetAssets();
                    auto &cache = m_Cache[n.ID];
                    if (!cache.Images[stateIndex] ||
                        cache.ImageEpochs[stateIndex] != assets->Epoch() ||
                        !(cache.ImageSources[stateIndex] == *overrideImage))
                    {
                        auto candidate = assets->Resolve(*overrideImage);
                        cache.Images[stateIndex] = std::move(candidate);
                        cache.ImageSources[stateIndex] = *overrideImage;
                        cache.ImageEpochs[stateIndex] = assets->Epoch();
                    }
                    sprite = cache.Images[stateIndex].get();
                }
                if (sprite)
                    Image(n.Bounds, n.Scale, *sprite, s.Color * tint, int(uint32_t(e)), nine);
            }
            // A text/image element still respects painter order across renderer
            // batches.
            Renderer2D::EndScene();
            Renderer2D::BeginScene(camera, glm::mat4(1));
            if (e.HasComponent<TextComponent>())
            {
                auto &t = e.GetComponent<TextComponent>();
                const UITextStyleComponent defaults;
                const auto &style = e.HasComponent<UITextStyleComponent>()
                                        ? e.GetComponent<UITextStyleComponent>()
                                        : defaults;
                if (!std::isfinite(style.Size) || style.Size <= 0 || style.Size > 512 ||
                    style.Horizontal < Align::Start || style.Horizontal > Align::End ||
                    style.Vertical < Align::Start || style.Vertical > Align::End ||
                    !std::isfinite(t.Kerning) || !std::isfinite(t.LineSpacing))
                    throw std::invalid_argument("Invalid native UI text style/spacing");
                if (!style.FontPath.empty() && style.FontPath.extension() != ".ttf" &&
                    style.FontPath.extension() != ".otf")
                    throw std::invalid_argument("UI font must be a project TTF/OTF");
                auto &cache = m_Cache[e.GetUUID()];
                const auto assets = m_Scene.GetAssets();
                const auto epoch = assets ? assets->Epoch() : 1;
                if (!cache.FontAsset || cache.FontPath != style.FontPath || cache.Epoch != epoch)
                {
                    cache.FontAsset =
                        assets ? assets->FontAsset(style.FontPath) : Font::GetDefault();
                    cache.FontPath = style.FontPath;
                    cache.Epoch = epoch;
                    cache.Width = -1;
                }
                auto content = n.Bounds.Inset(e.GetComponent<UIRectComponent>().Padding);
                float width = content.Size.x / style.Size;
                if (cache.Text != t.TextString || cache.Width != width ||
                    cache.Kerning != t.Kerning || cache.Wrap != style.Wrap)
                {
                    Renderer2D::LayoutText(t.TextString, cache.FontAsset, width, style.Wrap,
                                           t.Kerning, cache.Lines);
                    cache.Text = t.TextString;
                    cache.Width = width;
                    cache.Kerning = t.Kerning;
                    cache.Wrap = style.Wrap;
                }
                auto metrics = cache.FontAsset->GetMSDFData()->FontGeometry.getMetrics();
                const float normalized = 1.f / float(metrics.ascenderY - metrics.descenderY);
                float lineHeight =
                    (float(metrics.lineHeight) * normalized + t.LineSpacing) * style.Size;
                float height = style.Size + (cache.Lines.size() - 1) * lineHeight;
                float y = content.Position.y;
                if (style.Vertical == Align::Center)
                    y += (content.Size.y - height) * .5f;
                else if (style.Vertical == Align::End)
                    y += content.Size.y - height;
                y += float(metrics.ascenderY) * normalized * style.Size;
                for (auto &line : cache.Lines)
                {
                    float x = content.Position.x;
                    if (style.Horizontal == Align::Center)
                        x += (content.Size.x - line.Width * style.Size) * .5f;
                    else if (style.Horizontal == Align::End)
                        x += content.Size.x - line.Width * style.Size;
                    auto transform =
                        glm::translate(glm::mat4(1), glm::vec3(x * n.Scale, -y * n.Scale, 0)) *
                        glm::scale(glm::mat4(1), glm::vec3(style.Size * n.Scale));
                    Renderer2D::DrawString(line.Text, cache.FontAsset, transform,
                                           {t.Color * tint, t.Kerning, t.LineSpacing},
                                           int(uint32_t(e)));
                    y += lineHeight;
                }
            }
            if (e.HasComponent<UIButtonComponent>())
            {
                if (n.FocusEligible && m_Input.Focused() == n.ID &&
                    (m_Resolved.Modal || m_KeyboardOwns))
                {
                    auto inset = n.Bounds.Inset(glm::vec4(2));
                    Renderer2D::DrawRect(Transform(inset, n.Scale), {1, .9f, .65f, 1},
                                         int(uint32_t(e)));
                }
            }
            Renderer2D::EndScene();
        }
    }
    catch (...)
    {
        Renderer2D::EndScene();
        RenderCommand::SetRasterState(prior);
        throw;
    }
    RenderCommand::SetRasterState(prior);
}
} // namespace Hazel
