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
                throw std::invalid_argument(
                    "UI rectangle requires finite position and nonnegative size");
        }
        bool Usable(const Button &b)
        {
            return b.Visible && b.Interactable && b.Bounds.Size.x > 0 && b.Bounds.Size.y > 0 &&
                   b.Clip.Size.x > 0 && b.Clip.Size.y > 0;
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
        auto lo = glm::max(Position, r.Position),
             hi = glm::min(Position + Size, r.Position + r.Size);
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
        if (!Finite(c.AnchorMin) || !Finite(c.AnchorMax) || !Finite(c.Offset) ||
            !Finite(c.SizeDelta) || !Finite(c.Pivot) || !Nonnegative(c.MinimumSize) ||
            !Nonnegative(c.Margin) || !Nonnegative(c.Padding))
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
        auto size = (c.AnchorMax - c.AnchorMin) * parent.Size + c.SizeDelta;
        auto position =
            parent.Position + c.AnchorMin * parent.Size + c.Offset - c.Pivot * c.SizeDelta;
        if (!Finite(size) || !Finite(position))
            throw std::invalid_argument("UI rectangle overflow");
        Rect rect{position, glm::max(glm::vec2(0), size)};
        rect = rect.Inset(c.Margin);
        rect.Size = glm::max(c.MinimumSize, rect.Size);
        return rect;
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
    void InputRouter::Reset()
    {
        m_Scene = m_Focus = m_Hover = m_Capture = m_KeyCapture = 0;
        m_Ready = m_Left = m_Accept = m_Next = m_Previous = false;
        m_Order.clear();
        m_Actions.clear();
    }
    void InputRouter::Update(uint64_t scene, const std::vector<Button> &buttons,
                             const InputFrame &f)
    {
        if (buttons.size() > MaxButtons || !Finite(f.Mouse) ||
            (f.PointerPressed && !Finite(f.PressPosition)))
            throw std::invalid_argument("Invalid or oversized UI input frame");
        // Validation is independent of enabled state; malformed data is never hidden.
        m_IDs.clear();
        for (const auto &b : buttons)
        {
            CheckRect(b.Bounds);
            CheckRect(b.Clip);
            m_IDs.push_back(b.ID);
            if (!b.ID || b.Action.size() > 128 || b.Action.find('\0') != std::string::npos)
                throw std::invalid_argument("Invalid UI button identity or action");
        }
        std::sort(m_IDs.begin(), m_IDs.end());
        if (std::adjacent_find(m_IDs.begin(), m_IDs.end()) != m_IDs.end())
            throw std::invalid_argument("Duplicate UI button identity");
        if (scene != m_Scene)
        {
            Reset();
            m_Scene = scene;
        }
        const bool armed = m_Ready;
        m_Ready = scene != 0 && f.Enabled;
        m_Order.clear();
        m_Hover = 0;
        if (m_Ready)
        {
            for (size_t i = 0; i < buttons.size(); i++)
                if (Usable(buttons[i]))
                {
                    m_Order.push_back(i);
                    if (buttons[i].Bounds.Intersect(buttons[i].Clip).Contains(f.Mouse))
                        m_Hover = buttons[i].ID;
                }
            std::sort(m_Order.begin(), m_Order.end(),
                      [&](size_t a, size_t b)
                      {
                          return buttons[a].TabOrder != buttons[b].TabOrder
                                     ? buttons[a].TabOrder < buttons[b].TabOrder
                                     : a < b;
                      });
            auto focused = std::find_if(m_Order.begin(), m_Order.end(),
                                        [&](size_t i) { return buttons[i].ID == m_Focus; });
            if (focused == m_Order.end())
                m_Focus = m_Order.empty() ? 0 : buttons[m_Order.front()].ID;
            if (!std::any_of(m_Order.begin(), m_Order.end(),
                             [&](size_t i) { return buttons[i].ID == m_Capture; }))
                m_Capture = 0;
            if (armed && !m_Order.empty() &&
                ((f.NextDown && !m_Next) || (f.PreviousDown && !m_Previous)))
            {
                focused = std::find_if(m_Order.begin(), m_Order.end(),
                                       [&](size_t i) { return buttons[i].ID == m_Focus; });
                const size_t count = m_Order.size(), index = size_t(focused - m_Order.begin());
                m_Focus =
                    buttons[m_Order[(index + ((f.PreviousDown && !m_Previous) ? count - 1 : 1)) %
                                    count]]
                        .ID;
            }
            if (armed && (f.PointerPressed || (f.LeftDown && !m_Left)))
            {
                m_Capture = m_Hover;
                if (f.PointerPressed)
                {
                    m_Capture = 0;
                    for (const auto &b : buttons)
                        if (Usable(b) && b.Bounds.Intersect(b.Clip).Contains(f.PressPosition))
                            m_Capture = b.ID;
                }
                if (m_Capture)
                    m_Focus = m_Capture;
            }
            if (m_KeyCapture != m_Focus || !f.AcceptDown)
                m_KeyCapture = 0;
            if (armed && (f.AcceptPressed || (f.AcceptDown && !m_Accept)))
                m_KeyCapture = m_Focus;
            uint64_t activate = 0;
            if (armed && (f.PointerReleased || (!f.LeftDown && m_Left)) && m_Capture == m_Hover)
                activate = m_Capture;
            if (!f.LeftDown)
                m_Capture = 0;
            if (!activate && !m_Capture && armed &&
                (f.AcceptPressed || (f.AcceptDown && !m_Accept)))
                activate = m_Focus;
            if (activate && m_Actions.size() < MaxActions)
            {
                auto b = std::find_if(buttons.begin(), buttons.end(),
                                      [&](const auto &b) { return b.ID == activate; });
                if (b != buttons.end() && Usable(*b) && !b->Action.empty())
                    m_Actions.push_back({b->ID, b->Action});
            }
            // A retired/disabled element cannot leave delayed work for a consumer.
            m_Actions.erase(std::remove_if(m_Actions.begin(), m_Actions.end(),
                                           [&](const Action &a)
                                           {
                                               return !std::any_of(buttons.begin(), buttons.end(),
                                                                   [&](const Button &b)
                                                                   {
                                                                       return b.ID == a.Entity &&
                                                                              Usable(b) &&
                                                                              b.Action == a.Name;
                                                                   });
                                           }),
                            m_Actions.end());
        }
        else
        {
            m_Focus = m_Capture = m_KeyCapture = 0;
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
        if (scene != m_Scene || !m_Ready)
            return false;
        for (const auto &b : buttons)
            if (b.ID == id && Usable(b))
            {
                m_Focus = id;
                m_KeyCapture = 0;
                return true;
            }
        return false;
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
