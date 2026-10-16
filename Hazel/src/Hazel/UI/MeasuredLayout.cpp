#include "hzpch.h"
#include "MeasuredLayout.h"
#include <cmath>
#include <stdexcept>
namespace Hazel::GameUI
{
namespace
{
constexpr size_t None = std::numeric_limits<size_t>::max();
SizeMode Mode(const RectLayout &r, int axis) { return axis ? r.HeightMode : r.WidthMode; }
float Padding(const RectLayout &r, int axis) { return r.Padding[axis] + r.Padding[axis + 2]; }
float Margin(const RectLayout &r, int axis) { return r.Margin[axis] + r.Margin[axis + 2]; }
float Clamp(float value, const RectLayout &r, int axis)
{
    if (!std::isfinite(value) || value < 0)
        throw std::invalid_argument("UI measured dimension overflow");
    value = std::max(value, r.MinimumSize[axis]);
    return r.HasMaximum[axis] ? std::min(value, r.MaximumSize[axis]) : value;
}
int Main(const MeasureNode &n) { return n.Flow.Direction == Axis::Horizontal ? 0 : 1; }
} // namespace
void MeasuredLayout::Error(size_t index, int axis, const std::string &message) const
{
    std::string path;
    for (auto [n, a] : m_Path)
        path += (*m_Nodes)[n].Name + "#" + std::to_string((*m_Nodes)[n].ID) +
                (a ? ".height -> " : ".width -> ");
    const auto &node = (*m_Nodes)[index];
    throw std::invalid_argument("UI dependency: " + path + node.Name + "#" +
                                std::to_string(node.ID) + (axis ? ".height: " : ".width: ") +
                                message);
}
float MeasuredLayout::Available(size_t i, int axis)
{
    const auto &n = (*m_Nodes)[i];
    float parent =
        n.Parent == None
            ? m_External.Size[axis]
            : std::max(0.f, Size(n.Parent, axis) - Padding((*m_Nodes)[n.Parent].Layout, axis));
    return std::max(0.f, parent - n.Layout.Margin[axis] - n.Layout.Margin[axis + 2]);
}
float MeasuredLayout::Size(size_t i, int axis)
{
    if (m_State[i][axis] == 2)
        return m_Sizes[i][axis];
    if (m_State[i][axis] == 1)
        Error(i, axis, "Content/Fill or anchored Content dependency is indefinite");
    const auto &n = (*m_Nodes)[i];
    const auto &r = n.Layout;
    if (!n.Visible)
    {
        m_State[i][axis] = 2;
        return m_Sizes[i][axis] = 0;
    }
    m_State[i][axis] = 1;
    m_Path.emplace_back(i, axis);
    float desired = 0;
    switch (Mode(r, axis))
    {
    case SizeMode::Fixed:
        desired = r.FixedSize[axis];
        break;
    case SizeMode::Fill:
        if (n.Parent != None && (*m_Nodes)[n.Parent].Stack && Main((*m_Nodes)[n.Parent]) == axis)
        {
            Distribute(n.Parent);
            desired = m_Sizes[i][axis];
        }
        else
            desired = Available(i, axis);
        break;
    case SizeMode::Content:
    {
        float visualWidth = axis ? std::max(0.f, Size(i, 0) - Padding(r, 0)) : -1;
        auto visual = m_Intrinsic(i, visualWidth);
        if (!std::isfinite(visual[axis]) || visual[axis] < 0)
            Error(i, axis, "invalid intrinsic measurement");
        float children = 0;
        size_t visible = 0;
        for (auto child : n.Children)
        {
            const auto &c = (*m_Nodes)[child];
            if (!c.Visible)
                continue;
            if (!n.Stack && (c.Layout.AnchorMin[axis] != 0 || c.Layout.Pivot[axis] != 0))
                Error(child, axis,
                      "a Content parent requires zero child anchor/pivot on this axis");
            float extent = Size(child, axis) + Margin(c.Layout, axis);
            if (n.Stack && Main(n) == axis)
            {
                children += extent;
                ++visible;
            }
            else
                children = std::max(children, extent + (n.Stack ? 0 : c.Layout.Offset[axis]));
        }
        if (n.Stack && Main(n) == axis && visible)
            children += (visible - 1) * n.Flow.Gap;
        desired = std::max(visual[axis], children) + Padding(r, axis);
        break;
    }
    }
    if (!std::isfinite(desired) || desired < 0)
        Error(i, axis, "measured dimension overflow");
    m_Results[i].Measured[axis] = desired;
    m_Sizes[i][axis] = Clamp(desired, r, axis);
    m_State[i][axis] = 2;
    m_Path.pop_back();
    return m_Sizes[i][axis];
}
void MeasuredLayout::Distribute(size_t parent)
{
    if (m_Distribution[parent] == 2)
        return;
    const auto &p = (*m_Nodes)[parent];
    const int axis = Main(p);
    if (m_Distribution[parent] == 1)
        Error(parent, axis, "recursive stack allocation");
    m_Distribution[parent] = 1;
    float available = std::max(0.f, Size(parent, axis) - Padding(p.Layout, axis));
    float used = 0;
    size_t count = 0;
    for (auto i : p.Children)
    {
        const auto &n = (*m_Nodes)[i];
        if (!n.Visible)
            continue;
        ++count;
        float size =
            Mode(n.Layout, axis) == SizeMode::Fill ? n.Layout.MinimumSize[axis] : Size(i, axis);
        if (Mode(n.Layout, axis) == SizeMode::Fill)
            m_Sizes[i][axis] = size;
        used += size + n.Layout.Margin[axis] + n.Layout.Margin[axis + 2];
    }
    if (count)
        used += (count - 1) * p.Flow.Gap;
    if (!std::isfinite(used))
        Error(parent, axis, "stack extent overflow");
    m_Results[parent].Overflow |= used > available;
    float free = std::max(0.f, available - used), weight = 0;
    double uncappedWeight = 0;
    m_Caps.clear();
    for (auto i : p.Children)
    {
        const auto &n = (*m_Nodes)[i];
        if (!n.Visible || Mode(n.Layout, axis) != SizeMode::Fill)
            continue;
        weight += n.Weight;
        if (n.Layout.HasMaximum[axis])
            m_Caps.emplace_back(
                (double(n.Layout.MaximumSize[axis]) - n.Layout.MinimumSize[axis]) / n.Weight, i);
        else
            uncappedWeight += n.Weight;
    }
    if (!std::isfinite(weight))
        Error(parent, axis, "Fill weight overflow");
    // Capped weighted water filling by sorted saturation thresholds: bounded
    // O(n log n), with no convergence solver. Uncapped arithmetic retains the
    // legacy proportional-flow operation order for exact conversion geometry.
    double level = 0, remaining = free;
    std::sort(m_Caps.begin(), m_Caps.end());
    m_CapWeights.assign(m_Caps.size() + 1, uncappedWeight);
    for (size_t k = m_Caps.size(); k > 0; --k)
        m_CapWeights[k - 1] = m_CapWeights[k] + (*m_Nodes)[m_Caps[k - 1].second].Weight;
    for (size_t k = 0; k < m_Caps.size(); ++k)
    {
        auto [threshold, i] = m_Caps[k];
        (void)i;
        double activeWeight = m_CapWeights[k];
        if (activeWeight <= 0)
            break;
        double cost = (threshold - level) * activeWeight;
        if (remaining <= cost)
        {
            level += remaining / activeWeight;
            remaining = 0;
            break;
        }
        remaining -= cost;
        level = threshold;
    }
    if (uncappedWeight > 0)
    {
        level += remaining / uncappedWeight;
        remaining = 0;
    }
    m_Spare[parent] = weight ? float(remaining) : free;
    for (auto i : p.Children)
    {
        const auto &n = (*m_Nodes)[i];
        if (n.Visible && Mode(n.Layout, axis) == SizeMode::Fill)
        {
            double capped = double(m_Sizes[i][axis]) + double(n.Weight) * level;
            if (n.Layout.HasMaximum[axis])
                capped = std::min(capped, double(n.Layout.MaximumSize[axis]));
            float size =
                m_Caps.empty() ? m_Sizes[i][axis] + free * n.Weight / weight : float(capped);
            m_Sizes[i][axis] = Clamp(size, n.Layout, axis);
            m_State[i][axis] = 2;
            m_Results[i].Measured[axis] = m_Sizes[i][axis];
        }
    }
    m_Distribution[parent] = 2;
}
Rect MeasuredLayout::Content(size_t i)
{
    return m_Results[i].Bounds.Inset((*m_Nodes)[i].Layout.Padding);
}
void MeasuredLayout::Arrange(size_t i)
{
    const auto &n = (*m_Nodes)[i];
    if (!n.Visible)
        return;
    auto parent = n.Parent == None ? m_External : Content(n.Parent);
    auto &out = m_Results[i];
    out.Bounds.Size = m_Sizes[i];
    for (int axis = 0; axis < 2; ++axis)
    {
        out.Available[axis] = std::max(0.f, parent.Size[axis] - Margin(n.Layout, axis));
        out.Bounds.Position[axis] = parent.Position[axis] +
                                    n.Layout.AnchorMin[axis] * parent.Size[axis] +
                                    n.Layout.Offset[axis] + n.Layout.Margin[axis] -
                                    n.Layout.Pivot[axis] * out.Bounds.Size[axis];
    }
    if (n.Parent != None && (*m_Nodes)[n.Parent].Stack)
        out.Bounds.Position = m_Positions[i];
    for (int axis = 0; axis < 2; ++axis)
        if (!std::isfinite(out.Bounds.Position[axis]) ||
            !std::isfinite(out.Bounds.Position[axis] + out.Bounds.Size[axis]))
            Error(i, axis, "arranged position overflow");
    auto inside = out.Bounds.Intersect(parent);
    out.Overflow |= inside.Position != out.Bounds.Position || inside.Size != out.Bounds.Size;
    if (n.Stack)
    {
        Distribute(i);
        int axis = Main(n), cross = 1 - axis;
        auto content = Content(i);
        float spare = m_Spare[i];
        float cursor = content.Position[axis] + spare * (n.Flow.Main == Align::Center ? .5f
                                                         : n.Flow.Main == Align::End  ? 1.f
                                                                                      : 0.f);
        for (auto c : n.Children)
        {
            const auto &child = (*m_Nodes)[c];
            if (!child.Visible)
                continue;
            m_Positions[c][axis] = cursor + child.Layout.Margin[axis];
            float crossAvailable = std::max(0.f, content.Size[cross] - child.Layout.Margin[cross] -
                                                     child.Layout.Margin[cross + 2]);
            float crossSpare = std::max(0.f, crossAvailable - m_Sizes[c][cross]);
            m_Positions[c][cross] = content.Position[cross] + child.Layout.Margin[cross] +
                                    crossSpare * (n.Flow.Cross == Align::Center ? .5f
                                                  : n.Flow.Cross == Align::End  ? 1.f
                                                                                : 0.f);
            cursor += m_Sizes[c][axis] + child.Layout.Margin[axis] + child.Layout.Margin[axis + 2] +
                      n.Flow.Gap;
        }
    }
    for (auto child : n.Children)
        Arrange(child);
}
void MeasuredLayout::Resolve(const std::vector<MeasureNode> &nodes, Rect external,
                             Intrinsic intrinsic, size_t activeCount)
{
    const size_t count = activeCount ? activeCount : nodes.size();
    m_Nodes = &nodes;
    m_Intrinsic = std::move(intrinsic);
    m_External = external;
    m_Path.clear();
    try
    {
        if (!count || count > nodes.size() || count > InputRouter::MaxButtons)
            throw std::invalid_argument("Invalid measured UI tree size");
        external.Inset({});
        m_Results.assign(count, {});
        m_State.assign(count, {});
        m_Sizes.assign(count, {});
        m_Positions.assign(count, {});
        m_Distribution.assign(count, 0);
        m_Spare.assign(count, 0);
        std::vector<size_t> depths(count), incoming(count);
        for (size_t i = 0; i < count; ++i)
        {
            const auto &n = nodes[i];
            Validate(n.Layout);
            if (n.Layout.LayoutVersion != 2 || (n.Parent != None && n.Parent >= i) ||
                !std::isfinite(n.Weight) || n.Weight <= 0)
                Error(i, 0, "invalid measured tree/weight");
            depths[i] = n.Parent == None ? 1 : depths[n.Parent] + 1;
            if (depths[i] > 256)
                Error(i, 0, "layout depth exceeds 256");
            if (n.Stack)
            {
                std::vector<Rect> discard;
                ArrangeFlow({}, n.Flow, {}, discard);
            }
            for (auto child : n.Children)
            {
                if (child <= i || child >= count || nodes[child].Parent != i)
                    Error(i, 0, "invalid child index");
                if (++incoming[child] != 1)
                    Error(child, 0, "duplicate layout child");
            }
            if (n.Parent == None || !nodes[n.Parent].Stack)
                for (int axis = 0; axis < 2; ++axis)
                    if (Mode(n.Layout, axis) == SizeMode::Fill &&
                        (n.Layout.AnchorMin[axis] != 0 || n.Layout.Offset[axis] != 0 ||
                         n.Layout.Pivot[axis] != 0))
                        Error(i, axis,
                              "Fill owns free position; use margins, zero anchor/offset/pivot");
        }
        for (size_t i = 0; i < count; ++i)
            if (nodes[i].Parent != None && incoming[i] != 1)
                Error(i, 0, "missing layout child");
        for (size_t i = 0; i < count; ++i)
        {
            Size(i, 0);
            Size(i, 1);
        }
        for (size_t i = 0; i < count; ++i)
            if (nodes[i].Parent == None)
                Arrange(i);
        m_Intrinsic = {};
        m_Nodes = nullptr;
    }
    catch (...)
    {
        m_Intrinsic = {};
        m_Nodes = nullptr;
        throw;
    }
}
} // namespace Hazel::GameUI
