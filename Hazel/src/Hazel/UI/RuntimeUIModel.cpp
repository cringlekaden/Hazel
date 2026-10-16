#include "hzpch.h"
#include "RuntimeUIModel.h"
#include <cmath>
#include <stdexcept>
namespace Hazel::GameUI
{
namespace
{
template <glm::length_t N> bool Finite(const glm::vec<N, float> &v)
{
    for (int i = 0; i < N; i++)
        if (!std::isfinite(v[i]))
            return false;
    return true;
}
template <glm::length_t N> bool Nonnegative(const glm::vec<N, float> &v)
{
    if (!Finite(v))
        return false;
    for (int i = 0; i < N; i++)
        if (v[i] < 0)
            return false;
    return true;
}
void CheckRect(const Rect &r)
{
    if (!Finite(r.Position) || !Nonnegative(r.Size) || !Finite(r.Position + r.Size))
        throw std::invalid_argument("UI rectangle requires finite position and nonnegative size");
}
bool Usable(const Button &b)
{
    auto visible = b.Bounds.Intersect(b.Clip);
    return b.Visible && b.Interactable && b.Hit == HitPolicy::Control && visible.Size.x > 0 &&
           visible.Size.y > 0;
}
} // namespace
bool Rect::Contains(glm::vec2 p) const
{
    return Size.x > 0 && Size.y > 0 && p.x >= Position.x && p.y >= Position.y &&
           p.x < Position.x + Size.x && p.y < Position.y + Size.y;
}
Rect Rect::Inset(glm::vec4 b) const
{
    CheckRect(*this);
    if (!Nonnegative(b))
        throw std::invalid_argument("UI insets must be finite and nonnegative");
    return {Position + glm::vec2(b.x, b.y),
            glm::max(glm::vec2(0), Size - glm::vec2(b.x + b.z, b.y + b.w))};
}
Rect Rect::Intersect(const Rect &r) const
{
    CheckRect(*this);
    CheckRect(r);
    auto lo = glm::max(Position, r.Position), hi = glm::min(Position + Size, r.Position + r.Size);
    return {lo, glm::max(glm::vec2(0), hi - lo)};
}
glm::vec2 CanvasFit::ToLogical(glm::vec2 p) const
{
    return Scale > 0 ? p / Scale : glm::vec2(0);
}
CanvasFit FitCanvas(glm::vec2 pixels, const CanvasLayout &config)
{
    if (!Nonnegative(pixels) || !Finite(config.ReferenceSize) || config.ReferenceSize.x <= 0 ||
        config.ReferenceSize.y <= 0 || !Nonnegative(config.SafeInsets))
        throw std::invalid_argument("Invalid UI canvas dimensions or safe insets");
    CanvasFit fit;
    if (pixels.x == 0 || pixels.y == 0)
        return fit;
    fit.Scale = std::min(pixels.x / config.ReferenceSize.x, pixels.y / config.ReferenceSize.y);
    if (!std::isfinite(fit.Scale) || fit.Scale <= 0)
        throw std::invalid_argument("UI canvas scale out of range");
    fit.Bounds.Size = pixels / fit.Scale;
    CheckRect(fit.Bounds);
    fit.Content = fit.Bounds.Inset(config.SafeInsets);
    return fit;
}
void Validate(const RectLayout &c)
{
    if (c.LayoutVersion != 1 && c.LayoutVersion != 2)
        throw std::invalid_argument("Unsupported UI layout version");
    if (c.LayoutVersion == 2)
    {
        if (!Nonnegative(c.FixedSize) || !Nonnegative(c.MaximumSize) ||
            c.WidthMode < SizeMode::Fixed || c.WidthMode > SizeMode::Content ||
            c.HeightMode < SizeMode::Fixed || c.HeightMode > SizeMode::Content)
            throw std::invalid_argument("Invalid measured UI sizing rule");
        for (int axis = 0; axis < 2; ++axis)
        {
            if (c.AnchorMin[axis] != c.AnchorMax[axis])
                throw std::invalid_argument("Measured UI uses point anchors; sizing owns dimensions");
            if (c.HasMaximum[axis] && c.MaximumSize[axis] < c.MinimumSize[axis])
                throw std::invalid_argument("UI maximum must not be smaller than minimum");
        }
    }
    if (!Finite(c.AnchorMin) || !Finite(c.AnchorMax) || !Finite(c.Offset) || !Finite(c.SizeDelta) ||
        !Finite(c.Pivot) || !Nonnegative(c.MinimumSize) || !Nonnegative(c.Margin) ||
        !Nonnegative(c.Padding))
        throw std::invalid_argument("Invalid UI layout number");
    for (int i = 0; i < 2; i++)
        if (c.AnchorMin[i] < 0 || c.AnchorMax[i] > 1 || c.AnchorMin[i] > c.AnchorMax[i] ||
            c.Pivot[i] < 0 || c.Pivot[i] > 1)
            throw std::invalid_argument(
                "UI anchors and pivot require ordered normalized coordinates");
}
Rect ResolveRect(const Rect &parent, const RectLayout &c)
{
    CheckRect(parent);
    Validate(c);
    if(c.LayoutVersion!=1)throw std::invalid_argument("Measured UI requires measure/arrange, not legacy ResolveRect");
    auto size = (c.AnchorMax - c.AnchorMin) * parent.Size + c.SizeDelta;
    auto position = parent.Position + c.AnchorMin * parent.Size + c.Offset - c.Pivot * c.SizeDelta;
    if (!Finite(size) || !Finite(position))
        throw std::invalid_argument("UI rectangle overflow");
    Rect rect{position, glm::max(glm::vec2(0), size)};
    rect = rect.Inset(c.Margin);
    rect.Size = glm::max(c.MinimumSize, rect.Size);
    return rect;
}
Rect FramebufferCoverage(const Rect &pixels, glm::vec2 ratio, glm::vec2 framebuffer)
{
    CheckRect(pixels);
    if (!Finite(ratio) || ratio.x <= 0 || ratio.y <= 0 || !Nonnegative(framebuffer) ||
        framebuffer.x > 2147483000.f || framebuffer.y > 2147483000.f)
        throw std::invalid_argument("Invalid UI framebuffer mapping");
    Rect scaled{pixels.Position * ratio, pixels.Size * ratio};
    auto clipped = scaled.Intersect({{}, framebuffer});
    auto lo = glm::ceil(clipped.Position), hi = glm::floor(clipped.Position + clipped.Size);
    return {lo, glm::max(glm::vec2(0), hi - lo)};
}
RectLayout EditRect(const Rect &parent, const RectLayout &initial, glm::vec2 delta, unsigned edges)
{
    if (!Finite(delta) || edges > 15 || ((edges & 1) && (edges & 4)) ||
        ((edges & 2) && (edges & 8)))
        throw std::invalid_argument("Invalid UI edit gesture");
    auto start = ResolveRect(parent, initial), target = start;
    if (!edges)
        target.Position += delta;
    else
        for (int axis = 0; axis < 2; ++axis)
        {
            const unsigned nearEdge = axis ? 2 : 1, farEdge = axis ? 8 : 4;
            if (edges & nearEdge)
            {
                float end = start.Position[axis] + start.Size[axis];
                target.Size[axis] =
                    std::max(initial.MinimumSize[axis], start.Size[axis] - delta[axis]);
                target.Position[axis] = end - target.Size[axis];
            }
            else if (edges & farEdge)
                target.Size[axis] =
                    std::max(initial.MinimumSize[axis], start.Size[axis] + delta[axis]);
        }
    CheckRect(target);
    auto result = initial;
    for (int axis = 0; axis < 2; ++axis)
    {
        if (target.Size[axis] == start.Size[axis] && target.Position[axis] == start.Position[axis])
            continue;
        // Preserve authored values when a minimum clamps an attempted resize,
        // or during movement of an already minimum-constrained rectangle.
        if (target.Size[axis] != start.Size[axis])
            result.SizeDelta[axis] =
                target.Size[axis] + initial.Margin[axis] + initial.Margin[axis + 2] -
                (initial.AnchorMax[axis] - initial.AnchorMin[axis]) * parent.Size[axis];
        result.Offset[axis] = target.Position[axis] - parent.Position[axis] -
                              initial.AnchorMin[axis] * parent.Size[axis] +
                              initial.Pivot[axis] * result.SizeDelta[axis] - initial.Margin[axis];
    }
    Validate(result);
    auto check = ResolveRect(parent, result);
    if (glm::length(check.Position - target.Position) > .01f ||
        glm::length(check.Size - target.Size) > .01f)
        throw std::invalid_argument("UI edit cannot preserve the requested resolved edges");
    return result;
}
RectLayout EditMeasuredRect(const Rect &parent, const Rect &start, const RectLayout &initial,
                            glm::vec2 delta, unsigned edges)
{
    Validate(initial);CheckRect(parent);CheckRect(start);
    if(initial.LayoutVersion!=2 || !Finite(delta) || edges>15 || ((edges&1)&&(edges&4)) || ((edges&2)&&(edges&8)))
        throw std::invalid_argument("Invalid measured UI gesture");
    auto result=initial;
    for(int axis=0;axis<2;++axis)
    {
        auto mode=axis?initial.HeightMode:initial.WidthMode;
        unsigned leading=axis?2:1,trailing=axis?8:4;
        if(!edges)
        {
            if(mode==SizeMode::Fill)throw std::invalid_argument("Fill owns position; edit margins in Properties");
            result.Offset[axis]+=delta[axis];continue;
        }
        if(!(edges&(leading|trailing)))continue;
        if(mode!=SizeMode::Fixed)throw std::invalid_argument("Fill/Content owns this dimension; edit its sizing constraint in Properties");
        float desired=std::max(initial.MinimumSize[axis],start.Size[axis]+((edges&leading)?-delta[axis]:delta[axis]));
        if(initial.HasMaximum[axis])desired=std::min(desired,initial.MaximumSize[axis]);
        if(desired==start.Size[axis])continue;
        float position=start.Position[axis]+((edges&leading)?start.Size[axis]-desired:0);
        result.FixedSize[axis]=desired;
        result.Offset[axis]=position-parent.Position[axis]-initial.AnchorMin[axis]*parent.Size[axis]+
            initial.Pivot[axis]*desired-initial.Margin[axis];
    }
    Validate(result);return result;
}
void ArrangeFlow(const Rect &content, const FlowLayout &c, const std::vector<FlowItem> &items,
                 std::vector<Rect> &out)
{
    CheckRect(content);
    if (items.size() > InputRouter::MaxButtons || !std::isfinite(c.Gap) || c.Gap < 0 ||
        (c.Direction != Axis::Horizontal && c.Direction != Axis::Vertical) ||
        c.Main < Align::Start || c.Main > Align::End || c.Cross < Align::Start ||
        c.Cross > Align::Stretch)
        throw std::invalid_argument("Invalid or oversized UI flow group");
    const int main = c.Direction == Axis::Horizontal ? 0 : 1, cross = 1 - main;
    size_t visible = 0;
    float used = 0, flex = 0;
    for (const auto &item : items)
    {
        if (!Nonnegative(item.PreferredSize) || !Nonnegative(item.MinimumSize) ||
            !Nonnegative(item.Margin) || !std::isfinite(item.Flex) || item.Flex < 0)
            throw std::invalid_argument("Invalid UI flow item");
        if (!item.Visible)
            continue;
        ++visible;
        used += std::max(item.PreferredSize[main], item.MinimumSize[main]) + item.Margin[main] +
                item.Margin[main + 2];
        flex += item.Flex;
    }
    used += (visible ? visible - 1 : 0) * c.Gap;
    if (!std::isfinite(used) || !std::isfinite(flex))
        throw std::invalid_argument("UI flow size overflow");
    float free = std::max(0.f, content.Size[main] - used);
    float cursor = content.Position[main] + (flex > 0 ? 0
                                                      : free * (c.Main == Align::Center ? .5f
                                                                : c.Main == Align::End  ? 1
                                                                                        : 0));
    out.resize(items.size());
    for (size_t i = 0; i < items.size(); i++)
    {
        const auto &item = items[i];
        if (!item.Visible)
        {
            out[i] = {};
            continue;
        }
        glm::vec2 size = glm::max(item.PreferredSize, item.MinimumSize);
        if (flex > 0)
            size[main] += free * item.Flex / flex;
        float available =
            std::max(0.f, content.Size[cross] - item.Margin[cross] - item.Margin[cross + 2]);
        if (c.Cross == Align::Stretch)
            size[cross] = std::max(item.MinimumSize[cross], available);
        glm::vec2 pos = content.Position;
        pos[main] = cursor + item.Margin[main];
        pos[cross] += item.Margin[cross] +
                      std::max(0.f, available - size[cross]) * (c.Cross == Align::Center ? .5f
                                                                : c.Cross == Align::End  ? 1
                                                                                         : 0);
        out[i] = {pos, size};
        CheckRect(out[i]);
        cursor += size[main] + item.Margin[main] + item.Margin[main + 2] + c.Gap;
    }
}
namespace
{
constexpr size_t NoRank = static_cast<size_t>(-1);
uint64_t Hit(const std::vector<Button> &buttons, glm::vec2 point)
{
    for (auto it = buttons.rbegin(); it != buttons.rend(); ++it)
    {
        if (!it->Visible || it->Hit == HitPolicy::PassThrough ||
            !it->Bounds.Intersect(it->Clip).Contains(point))
            continue;
        return Usable(*it) ? it->ID : 0; // Block/disabled controls occlude lower controls.
    }
    return 0;
}
} // namespace
void InputRouter::Reset()
{
    m_Scene = m_Focus = m_Hover = m_Capture = m_KeyCapture = m_Generation = m_Dropped = 0;
    m_Ready = m_Left = m_Accept = m_Next = m_Previous = m_PointerGesture = false;
    m_Order.clear();
    m_IDs.clear();
    m_Index.clear();
    m_Ranks.clear();
    m_Actions.clear();
}
void InputRouter::Reconcile(uint64_t scene, const std::vector<Button> &buttons, uint64_t generation)
{
    if (scene != m_Scene)
    {
        Reset();
        m_Scene = scene;
    }
    if (generation && generation == m_Generation)
        return;
    if (buttons.size() > MaxButtons)
        throw std::invalid_argument("UI exceeds 4096 hit entries");
    m_IDs.clear();
    for (auto &b : buttons)
    {
        CheckRect(b.Bounds);
        CheckRect(b.Clip);
        if (!b.ID || b.Action.size() > 128 || b.Action.find('\0') != std::string::npos ||
            (b.Hit != HitPolicy::PassThrough && b.Hit != HitPolicy::Control && b.Hit != HitPolicy::Block))
            throw std::invalid_argument("Invalid UI button identity/action");
        m_IDs.push_back(b.ID);
    }
    std::sort(m_IDs.begin(), m_IDs.end());
    if (std::adjacent_find(m_IDs.begin(), m_IDs.end()) != m_IDs.end())
        throw std::invalid_argument("Duplicate UI button identity");
    bool sameIDs = m_Index.size() == buttons.size();
    if (sameIDs)
        for (size_t i = 0; i < buttons.size(); ++i)
        {
            auto found = m_Index.find(buttons[i].ID);
            if (found == m_Index.end() || found->second != i)
            {
                sameIDs = false;
                break;
            }
        }
    if (!sameIDs)
    {
        m_Index.clear();
        m_Index.reserve(buttons.size());
        for (size_t i = 0; i < buttons.size(); ++i)
            m_Index.emplace(buttons[i].ID, i);
    }
    m_Order.clear();
    m_Ranks.assign(buttons.size(), NoRank);
    for (size_t i = 0; i < buttons.size(); ++i)
        if (Usable(buttons[i]))
            m_Order.push_back(i);
    std::sort(m_Order.begin(), m_Order.end(), [&](size_t a, size_t b) {
        return buttons[a].TabOrder != buttons[b].TabOrder
                   ? buttons[a].TabOrder < buttons[b].TabOrder
                   : a < b;
    });
    for (size_t rank = 0; rank < m_Order.size(); ++rank)
        m_Ranks[m_Order[rank]] = rank;
    auto eligible = [&](uint64_t id) {
        auto found = m_Index.find(id);
        return found != m_Index.end() && m_Ranks[found->second] != NoRank;
    };
    if (!m_Ready)
        m_Focus = 0;
    else if (!eligible(m_Focus))
        m_Focus = m_Order.empty() ? 0 : buttons[m_Order.front()].ID;
    if (!eligible(m_Capture))
        m_Capture = 0;
    if (!eligible(m_KeyCapture))
        m_KeyCapture = 0;
    if (!eligible(m_Hover))
        m_Hover = 0;
    m_Actions.erase(std::remove_if(m_Actions.begin(), m_Actions.end(),
                                   [&](const Action &a) {
                                       auto found = m_Index.find(a.Entity);
                                       return found == m_Index.end() ||
                                              !Usable(buttons[found->second]) ||
                                              buttons[found->second].Action != a.Name;
                                   }),
                    m_Actions.end());
    m_Generation = generation;
}
void InputRouter::Update(uint64_t scene, const std::vector<Button> &buttons, const InputFrame &f,
                         uint64_t generation)
{
    if (!Finite(f.Mouse) || (f.PointerPressed && !Finite(f.PressPosition)) ||
        (f.PointerReleased && f.HasReleasePosition && !Finite(f.ReleasePosition)))
        throw std::invalid_argument("Invalid UI pointer frame");
    Reconcile(scene, buttons, generation);
    const bool armed = m_Ready;
    m_Ready = scene != 0 && f.Enabled;
    m_Hover = 0;
    if (m_Ready)
    {
        m_Hover = Hit(buttons, f.Mouse);
        if (!m_Focus && !m_Order.empty())
            m_Focus = buttons[m_Order.front()].ID;
        if (armed && f.KeyboardEnabled && !m_Order.empty() &&
            ((f.NextDown && !m_Next) || (f.PreviousDown && !m_Previous)))
        {
            auto found = m_Index.find(m_Focus);
            size_t rank = found == m_Index.end() ? 0 : m_Ranks[found->second];
            const size_t count = m_Order.size();
            m_Focus =
                buttons[m_Order[(rank + ((f.PreviousDown && !m_Previous) ? count - 1 : 1)) % count]]
                    .ID;
        }
        const bool gestureOwns = m_PointerGesture || m_Capture || m_Left || f.LeftDown ||
                                 f.PointerPressed || f.PointerReleased;
        if (armed && (f.PointerPressed || (f.LeftDown && !m_Left)))
        {
            m_PointerGesture = true;
            m_Capture = Hit(buttons, f.PointerPressed ? f.PressPosition : f.Mouse);
            if (m_Capture)
                m_Focus = m_Capture;
        }
        if (f.CancelDown)
        {
            m_Capture = m_KeyCapture = 0;
            m_Actions.clear();
        }
        if (m_KeyCapture != m_Focus || !f.AcceptDown || !f.KeyboardEnabled || gestureOwns)
            m_KeyCapture = 0;
        const bool accept = armed && f.KeyboardEnabled && !f.CancelDown && !gestureOwns &&
                            (f.AcceptPressed || (f.AcceptDown && !m_Accept));
        if (accept)
            m_KeyCapture = m_Focus;
        uint64_t activate = 0;
        if (armed && !f.CancelDown && (f.PointerReleased || (!f.LeftDown && m_Left)))
        {
            auto release = Hit(
                buttons, f.PointerReleased && f.HasReleasePosition ? f.ReleasePosition : f.Mouse);
            if (m_Capture == release)
                activate = m_Capture;
        }
        if (!f.LeftDown)
        {
            m_Capture = 0;
            m_PointerGesture = false;
        }
        if (!activate && accept)
            activate = m_Focus;
        if (activate)
        {
            auto found = m_Index.find(activate);
            if (found != m_Index.end())
            {
                const auto &b = buttons[found->second];
                if (Usable(b) && !b.Action.empty())
                {
                    if (m_Actions.size() < MaxActions)
                        m_Actions.push_back({b.ID, b.Action});
                    else
                        ++m_Dropped; // Explicit drop-newest; host reports the counter.
                }
            }
        }
    }
    else
    {
        m_Focus = m_Hover = m_Capture = m_KeyCapture = 0;
        m_PointerGesture = false;
        m_Actions.clear();
    }
    m_Left = f.LeftDown;
    m_Accept = f.AcceptDown;
    m_Next = f.NextDown;
    m_Previous = f.PreviousDown;
}
void InputRouter::Retire(uint64_t scene, uint64_t id)
{
    if (scene != m_Scene || !id)
        return;
    if (m_Focus == id)
        m_Focus = 0;
    if (m_Hover == id)
        m_Hover = 0;
    if (m_Capture == id)
        m_Capture = 0;
    if (m_KeyCapture == id)
        m_KeyCapture = 0;
    m_Actions.erase(std::remove_if(m_Actions.begin(), m_Actions.end(),
                                   [id](const Action &a) { return a.Entity == id; }),
                    m_Actions.end());
}
bool InputRouter::Consume(uint64_t scene, Action &a)
{
    if (!scene || scene != m_Scene || m_Actions.empty())
        return false;
    a = std::move(m_Actions.front());
    m_Actions.pop_front();
    return true;
}
bool InputRouter::SetFocus(uint64_t scene, uint64_t id, const std::vector<Button> &buttons)
{
    auto found = m_Index.find(id);
    if (scene != m_Scene || !m_Ready || found == m_Index.end() || found->second >= buttons.size() ||
        buttons[found->second].ID != id || !Usable(buttons[found->second]))
        return false;
    m_Focus = id;
    m_KeyCapture = 0;
    return true;
}
ButtonState InputRouter::State(const Button &b) const
{
    if (!b.Interactable)
        return ButtonState::Disabled;
    if ((m_Capture == b.ID && m_Left && m_Hover == b.ID) || (m_KeyCapture == b.ID && m_Accept))
        return ButtonState::Pressed;
    if (m_Hover == b.ID)
        return ButtonState::Hover;
    if (m_Focus == b.ID)
        return ButtonState::Focused;
    return ButtonState::Normal;
}
} // namespace Hazel::GameUI
