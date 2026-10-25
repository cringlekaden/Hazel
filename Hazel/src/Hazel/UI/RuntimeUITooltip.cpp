#include "hzpch.h"
#include "RuntimeUI.h"
#include "OwnedOverlay.h"
#include <cmath>
namespace Hazel
{
using namespace GameUI;
void RuntimeUI::PreviewTooltip(uint64_t source, bool open)
{
    if (m_Scene.IsRunning() || m_Scene.IsSimulating())
        throw std::invalid_argument("Tooltip preview requires Edit");
    if (open)
    {
        auto e = m_Scene.GetEntityByUUID(source);
        if (!e || !e.HasComponent<UITooltipComponent>())
            throw std::invalid_argument("Choose a Tooltip source");
        ValidatePages(source);
    }
    m_TipPreview = open ? source : 0;
    ++m_DropdownRevision;
}
void RuntimeUI::Advance(float seconds)
{
    uint64_t candidate = 0;
    if (m_Scene.IsRunning() && m_InputEnabled && !m_TransitionNeutral && !m_ScrollNeutral &&
        !m_SliderNeutral && !m_Input.Captured() && !m_Slider.ID && !m_Bar.Owner && !ActiveDropdown())
    {
        auto id = m_TipPointer ? m_Input.Hovered() : m_Input.Focused();
        auto e = m_Scene.GetEntityByUUID(id);
        if (e && e.HasComponent<UITooltipComponent>() && Live(id) &&
            m_TooltipBodies.count(e.GetComponent<UITooltipComponent>().Body))
            candidate = id;
    }
    const auto lifetime = m_Scene.GetAuthoringLifetime(candidate),
               version = m_Scene.GetAuthoringComponentVersion(candidate, typeid(UITooltipComponent));
    if (candidate != m_TipCandidate || lifetime != m_TipLifetime || version != m_TipComponent)
    {
        m_TipCandidate = candidate;
        m_TipElapsed = 0;
        m_TipLifetime = lifetime;
        m_TipComponent = version;
    }
    if (candidate && std::isfinite(seconds) && seconds > 0)
        m_TipElapsed = std::min(5.f, m_TipElapsed + std::min(seconds, .25f));
    uint64_t visible = 0;
    if (candidate &&
        m_TipElapsed >= m_Scene.GetEntityByUUID(candidate).GetComponent<UITooltipComponent>().Delay)
        visible = candidate;
    if (visible != m_TipVisible)
    {
        m_TipVisible = visible;
        ++m_DropdownRevision;
    }
}
void RuntimeUI::AppendOverlay(Entity root, Rect bounds, Rect surface, float scale, uint64_t canvas,
                              uint64_t scope, bool passive)
{
    auto begin = m_Build.PainterOrder.size();
    Visit(root, bounds, surface, scale, true, canvas, 0, true);
    for (size_t i = begin; i < m_Build.PainterOrder.size(); ++i)
    {
        auto &n = m_Build.Elements[m_Build.PainterOrder[i]];
        n.Scope = scope;
        n.PassiveOverlay = passive;
        n.OverlayRoot = n.ID == uint64_t(root.GetUUID());
        n.LayoutBounds = n.Bounds;
        n.LayoutParentContent = n.ParentContent;
        n.LayoutSlot = n.Slot;
    }
    for (auto &g : m_Build.Guides)
        if (auto n = m_Build.Find(g.Owner); n && Descendant(n->ID, root.GetUUID()))
            g.LayoutBounds = g.Bounds;
}
void RuntimeUI::ArrangeTooltip(bool measure)
{
    const auto previousTip = m_Build.Tooltips.empty() ? TooltipGeometry{} : m_Build.Tooltips.front();
    m_Build.Tooltips.clear();
    const auto source = m_Scene.IsRunning() ? m_TipVisible : m_TipPreview;
    // Remove prior passive runs before relocating or retiring them. No hit entry exists.
    m_Build.PainterOrder.erase(std::remove_if(m_Build.PainterOrder.begin(), m_Build.PainterOrder.end(),
                                              [&](auto i) { return m_Build.Elements[i].PassiveOverlay; }),
                               m_Build.PainterOrder.end());
    for (auto &n : m_Build.Elements)
        if (n.PassiveOverlay)
        {
            n.LayoutResolved = false;
            n.PassiveOverlay = false;
        }
    if (!source || ActiveDropdown() || m_Slider.ID || m_Bar.Owner || m_Input.Captured())
        return;
    auto e = m_Scene.GetEntityByUUID(source);
    auto n = m_Build.Find(source);
    if (!e || !e.HasComponent<UITooltipComponent>() || !n || !n->LayoutResolved || !n->Visible ||
        !n->Enabled || n->FramebufferClip.Size.x <= 0 || n->FramebufferClip.Size.y <= 0 ||
        (m_Build.Scope && n->Scope != m_Build.Scope))
        return;
    const auto c = e.GetComponent<UITooltipComponent>();
    auto body = m_Scene.GetEntityByUUID(c.Body);
    if (!body || !body.HasComponent<UIRectComponent>() || !body.HasComponent<TextComponent>() ||
        m_Scene.GetRelationship(body).Parent != e.GetUUID())
        return;
    const auto &r = body.GetComponent<UIRectComponent>();
    if (!r.Visible || !r.Enabled)
        return;
    const float scale = n->Scale;
    const auto canvas = n->Canvas, scope = n->Scope;
    auto ratio = m_Build.Surface.Framebuffer / m_Build.Surface.Viewport;
    auto covered = n->FramebufferBounds.Intersect(n->FramebufferClip);
    if (covered.Size.x <= 0 || covered.Size.y <= 0)
        return;
    Rect anchor{covered.Position / (ratio * scale), covered.Size / (ratio * scale)};
    Rect surface{{}, m_Build.Surface.Viewport / scale};
    if (auto cn = m_Build.Find(canvas))
        surface = cn->Bounds.Intersect(cn->ParentContent);
    if (surface.Size.x <= 0 || surface.Size.y <= 0)
        return;
    const auto width = std::min(r.FixedSize.x, surface.Size.x);
    auto previous = m_Build.Find(c.Body);
    // The same measured solver supplies intrinsic wrapped height; only placement is overlay policy.
    Rect probe{surface.Position, {width, std::min(512.f, surface.Size.y)}};
    const bool rebuild = measure || previousTip.Body != c.Body || previousTip.Natural.x != width;
    if (rebuild)
        MeasureTree(body, probe, &probe, true);
    previous = m_Build.Find(c.Body);
    if (!previous || !previous->MeasureResolved)
        return;
    glm::vec2 natural{width, rebuild ? previous->Measured.y : previousTip.Natural.y};
    float height = std::min(512.f, natural.y);
    auto placed = PlaceOwnedOverlay(surface, anchor, width, height, c.Gap);
    if (placed.Bounds.Size.x <= 0 || placed.Bounds.Size.y <= 0)
        return;
    // Final arrange can cap height, but never remeasure using clipped paint as intrinsic content.
    if (rebuild || previousTip.Bounds.Size != placed.Bounds.Size)
        MeasureTree(body, placed.Bounds, &placed.Bounds);
    else
    {
        auto &out = m_Build.Elements[m_Build.Index.at(c.Body)];
        auto delta = placed.Bounds.Position - out.Bounds.Position;
        out.Bounds.Position += delta;
        out.ParentContent.Position += delta;
        out.Slot.Position += delta;
    }
    AppendOverlay(body, placed.Bounds, surface, scale, canvas, scope, true);
    ApplyScrollPresentation();
    m_Build.Tooltips.push_back({source, c.Body, placed.Bounds, natural});
}
} // namespace Hazel
