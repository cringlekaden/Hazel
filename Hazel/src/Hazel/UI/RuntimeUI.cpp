#include "hzpch.h"
#include "RuntimeUI.h"
#include "Hazel/Renderer/MSDFData.h"
#include "Hazel/Renderer/RenderCommand.h"
#include "Hazel/UI/RuntimeUIConfig.h"
#include <cmath>
#include <limits>
#include <glm/gtc/matrix_transform.hpp>
namespace Hazel
{
using namespace GameUI;
void RuntimeUI::Reset()
{
    m_Input.Reset();
    m_Actions.clear();
    m_Nodes.clear();
    m_Buttons.clear();
    m_Cache.clear();
    m_Modal = 0;
    m_PointerOver = false;
    m_KeyboardOwns = false;
}
void RuntimeUI::Retire(uint64_t id)
{
    if (m_Input.Focused() == id)
        m_KeyboardOwns = false;
    m_Input.Retire(m_Scene.GetIdentity(), id);
    m_Cache.erase(id);
    m_Actions.erase(
        std::remove_if(m_Actions.begin(), m_Actions.end(), [&](auto &a) { return a.Entity == id; }),
        m_Actions.end());
}
void RuntimeUI::Resolve(glm::vec2 viewport, bool previewOrphans)
{
    m_Viewport = viewport;
    m_Nodes.clear();
    m_Buttons.clear();
    m_Canvases.clear();
    m_Modal = 0;
    if (viewport.x <= 0 || viewport.y <= 0)
        return;
    for (auto h : m_Scene.GetAllEntitiesWith<UICanvasComponent>())
    {
        Entity e(h, &m_Scene);
        if (m_Scene.IsEntityValid(e.GetUUID()))
            m_Canvases.push_back(e);
    }
    std::sort(m_Canvases.begin(), m_Canvases.end(), [](Entity a, Entity b) {
        auto x = a.GetComponent<UICanvasComponent>().SortOrder,
             y = b.GetComponent<UICanvasComponent>().SortOrder;
        return x == y ? uint64_t(a.GetUUID()) < uint64_t(b.GetUUID()) : x < y;
    });
    for (auto canvas : m_Canvases)
    {
        auto &c = canvas.GetComponent<UICanvasComponent>();
        if (!c.Visible)
            continue;
        auto p = m_Scene.GetRelationship(canvas).Parent;
        bool visible = true, enabled = c.Enabled;
        while (uint64_t(p))
        {
            auto e = m_Scene.GetEntityByUUID(p);
            if (e.HasComponent<UIRectComponent>())
            {
                auto &r = e.GetComponent<UIRectComponent>();
                visible &= r.Visible;
                enabled &= r.Enabled;
            }
            if (e.HasComponent<UICanvasComponent>())
            {
                auto &a = e.GetComponent<UICanvasComponent>();
                visible &= a.Visible;
                enabled &= a.Enabled;
            }
            p = m_Scene.GetRelationship(e).Parent;
        }
        if (!visible)
            continue;
        auto fit = FitCanvas(viewport, c);
        if (!fit.Scale)
            continue;
        if (c.Modal && enabled)
            m_Modal = canvas.GetUUID();
        for (auto id : m_Scene.GetChildrenView(canvas.GetUUID()))
        {
            auto e = m_Scene.GetEntityByUUID(id);
            if (!e.HasComponent<UICanvasComponent>())
                Visit(e, fit.Content, fit.Bounds, fit.Scale, enabled, canvas.GetUUID(), 0);
        }
    }
    if (previewOrphans)
    {
        auto fit = FitCanvas(viewport, CanvasLayout{});
        for (auto h : m_Scene.GetAllEntitiesWith<UIRectComponent>())
        {
            Entity e(h, &m_Scene);
            if (!m_Scene.IsEntityValid(e.GetUUID()) || e.HasComponent<UICanvasComponent>())
                continue;
            auto parent = m_Scene.GetRelationship(e).Parent;
            if (!uint64_t(parent))
                Visit(e, fit.Content, fit.Bounds, fit.Scale, true, 0, 0);
        }
    }
    for (auto &n : m_Nodes)
        if (n.Element.HasComponent<UIButtonComponent>())
        {
            auto &b = n.Element.GetComponent<UIButtonComponent>();
            const bool usable = n.Enabled && (!m_Modal || n.Canvas == m_Modal);
            m_Buttons.push_back(
                {n.ID, n.Bounds, n.Clip, b.Action, b.TabOrder, true, usable && b.Interactable});
        }
}
void RuntimeUI::Visit(Entity e, Rect parent, Rect clip, float scale, bool enabled, uint64_t canvas,
                      size_t depth, const Rect *arranged)
{
    if (!m_Scene.IsEntityValid(e.GetUUID()) || !e.HasComponent<UIRectComponent>())
        return;
    if (depth >= Scene::MaxDepth || m_Nodes.size() >= InputRouter::MaxButtons)
        throw std::runtime_error("UI layout limit exceeded");
    auto &r = e.GetComponent<UIRectComponent>();
    Validate(r);
    if (!r.Visible)
        return;
    auto bounds = arranged ? *arranged : ResolveRect(parent, r);
    enabled &= r.Enabled;
    m_Nodes.push_back({uint64_t(e.GetUUID()), e, bounds, clip, scale, enabled, canvas});
    auto content = bounds.Inset(r.Padding);
    auto childClip = r.ClipChildren ? clip.Intersect(bounds) : clip;
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
        // Recursive insertion may rehash the map, but references to its values
        // remain valid.
        for (size_t i = 0; i < children.size(); i++)
        {
            auto child = m_Scene.GetEntityByUUID(children[i]);
            if (cache.Items[i].Visible)
                Visit(child, content, childClip, scale, enabled, canvas, depth + 1,
                      &cache.Arranged[i]);
        }
    }
    else
        for (auto id : children)
        {
            auto child = m_Scene.GetEntityByUUID(id);
            if (!child.HasComponent<UICanvasComponent>())
                Visit(child, content, childClip, scale, enabled, canvas, depth + 1);
        }
}
void RuntimeUI::UpdateInput(const InputFrame &pixels)
{
    m_PointerOver = false;
    auto input = pixels;
    // Canvases can have different reference scales. Dispatch in physical pixels.
    for (auto &b : m_Buttons)
    {
        auto it =
            std::find_if(m_Nodes.begin(), m_Nodes.end(), [&](auto &n) { return n.ID == b.ID; });
        b.Bounds.Position *= it->Scale;
        b.Bounds.Size *= it->Scale;
        b.Clip.Position *= it->Scale;
        b.Clip.Size *= it->Scale;
        if (b.Bounds.Intersect(b.Clip).Contains(pixels.Mouse))
            m_PointerOver = true;
    }
    uint64_t entryFocus = 0;
    int entryOrder =
        input.PreviousDown ? std::numeric_limits<int>::min() : std::numeric_limits<int>::max();
    for (const auto &b : m_Buttons)
    {
        auto clipped = b.Bounds.Intersect(b.Clip);
        if (!b.Interactable || clipped.Size.x <= 0 || clipped.Size.y <= 0)
            continue;
        if (!entryFocus ||
            (input.PreviousDown ? b.TabOrder >= entryOrder : b.TabOrder < entryOrder))
        {
            entryFocus = b.ID;
            entryOrder = b.TabOrder;
        }
    }
    bool entering = !m_Modal && !m_KeyboardOwns && input.FocusNavigation && entryFocus &&
                    input.Enabled && !input.CancelDown;
    if (!input.Enabled || input.CancelDown || (input.PointerPressed && !m_PointerOver))
        m_KeyboardOwns = false;
    else if (input.FocusNavigation && entryFocus)
        m_KeyboardOwns = true;
    // Enter focus before allowing activation; the held-key history is still sampled.
    input.KeyboardEnabled = !entering && (m_Modal != 0 || m_KeyboardOwns);
    m_Input.Update(m_Scene.GetIdentity(), m_Buttons, input);
    if (entering)
        m_Input.SetFocus(m_Scene.GetIdentity(), entryFocus, m_Buttons);
    Action a;
    while (m_Input.Consume(m_Scene.GetIdentity(), a))
        if (m_Actions.size() < InputRouter::MaxActions)
            m_Actions.push_back(std::move(a));
    m_Actions.erase(std::remove_if(m_Actions.begin(), m_Actions.end(),
                                   [&](auto &a) { return !Live(a.Entity); }),
                    m_Actions.end());
}
bool RuntimeUI::Live(uint64_t id) const
{
    if (!m_Scene.IsEntityValid(id))
        return false;
    auto e = m_Scene.GetEntityByUUID(id);
    if (!e.HasComponent<UIButtonComponent>() || !e.GetComponent<UIButtonComponent>().Interactable)
        return false;
    uint64_t canvas = 0;
    size_t depth = 0;
    while (e && depth++ < Scene::MaxDepth)
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
            if (!canvas)
                canvas = e.GetUUID();
        }
        e = m_Scene.GetEntityByUUID(m_Scene.GetRelationship(e).Parent);
    }
    return canvas && (!m_Modal || canvas == m_Modal);
}
bool RuntimeUI::Consume(uint64_t entity, Action &action)
{
    for (auto it = m_Actions.begin(); it != m_Actions.end();)
    {
        if (!Live(it->Entity) ||
            m_Scene.GetEntityByUUID(it->Entity).GetComponent<UIButtonComponent>().Action !=
                it->Name)
        {
            it = m_Actions.erase(it);
            continue;
        }
        if (!entity || it->Entity == entity)
        {
            action = std::move(*it);
            m_Actions.erase(it);
            return true;
        }
        ++it;
    }
    return false;
}
bool RuntimeUI::Bounds(uint64_t id, Rect &pixels) const
{
    for (auto n : m_Nodes)
        if (n.ID == id && m_Scene.IsEntityValid(id))
        {
            pixels = n.Bounds;
            pixels.Position *= n.Scale;
            pixels.Size *= n.Scale;
            return true;
        }
    return false;
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
    if (m_Nodes.empty() || m_Viewport.x <= 0 || m_Viewport.y <= 0)
        return;
    auto prior = RenderCommand::GetRasterState();
    auto state = prior;
    state.DepthTest = false;
    state.ClipEnabled = true;
    Camera camera(glm::ortho(0.f, m_Viewport.x, -m_Viewport.y, 0.f, -1.f, 1.f));
    try
    {
        for (auto &n : m_Nodes)
        {
            auto e = n.Element;
            if (!m_Scene.IsEntityValid(e.GetUUID()))
                continue;
            auto clip = n.Clip.Intersect(n.Bounds); // own drawable is clipped to its hit rectangle
            const auto ratio = glm::vec2(prior.Viewport.z, prior.Viewport.w) / m_Viewport;
            const auto p = clip.Position * n.Scale * ratio, z = clip.Size * n.Scale * ratio;
            if (z.x <= 0 || z.y <= 0)
                continue;
            int left = int(std::ceil(p.x)), top = int(std::ceil(p.y)),
                right = int(std::floor(p.x + z.x)), bottom = int(std::floor(p.y + z.y));
            state.ClipRect = {prior.Viewport.x + left, prior.Viewport.y + prior.Viewport.w - bottom,
                              std::max(0, right - left), std::max(0, bottom - top)};
            RenderCommand::SetRasterState(state);
            Renderer2D::BeginScene(camera, glm::mat4(1));
            auto tint = glm::vec4(1);
            SpriteSource overrideImage;
            if (e.HasComponent<UIButtonComponent>())
            {
                auto &b = e.GetComponent<UIButtonComponent>();
                auto it = std::find_if(m_Buttons.begin(), m_Buttons.end(),
                                       [&](auto &a) { return a.ID == uint64_t(e.GetUUID()); });
                int index = it == m_Buttons.end() ? 0 : int(m_Input.State(*it));
                if (index == int(ButtonState::Focused) && !m_Modal && !m_KeyboardOwns)
                    index = 0;
                tint = b.Colors[index];
                overrideImage = b.Images[index];
            }
            const UINineSliceComponent *nine = e.HasComponent<UINineSliceComponent>()
                                                   ? &e.GetComponent<UINineSliceComponent>()
                                                   : nullptr;
            if (e.HasComponent<SpriteRendererComponent>())
            {
                auto &s = e.GetComponent<SpriteRendererComponent>();
                auto *sprite = m_Scene.RenderedSprite(e);
                Ref<const ResolvedSprite> overridden;
                if (overrideImage.index() != 0)
                {
                    if (!m_Scene.GetAssets())
                        throw std::runtime_error("UI image needs project assets");
                    overridden = m_Scene.GetAssets()->Resolve(overrideImage);
                    sprite = overridden.get();
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
                UITextStyleComponent style;
                if (e.HasComponent<UITextStyleComponent>())
                    style = e.GetComponent<UITextStyleComponent>();
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
                auto it = std::find_if(m_Buttons.begin(), m_Buttons.end(),
                                       [&](auto &b) { return b.ID == uint64_t(e.GetUUID()); });
                if (it != m_Buttons.end() && m_Input.State(*it) == ButtonState::Focused &&
                    (m_Modal || m_KeyboardOwns))
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
