#pragma once
#include "RuntimeUIModel.h"
#include <array>
#include <functional>
#include <limits>
namespace Hazel::GameUI
{
// Indexed CPU layout inputs; callbacks supply visual metrics in logical units.
// One tree is a v2 island. Its external parent has a definite content rectangle.
struct MeasureNode
{
    uint64_t ID = 0;
    std::string Name;
    RectLayout Layout;
    FlowLayout Flow;
    bool Stack = false, Visible = true;
    float Weight = 1;
    size_t Parent = (std::numeric_limits<size_t>::max)();
    std::vector<size_t> Children;
};
struct MeasureResult
{
    Rect Bounds;
    glm::vec2 Available{}, Measured{};
    bool Overflow = false;
};
class MeasuredLayout
{
  public:
    // width < 0 requests unwrapped natural visual width. Otherwise measure
    // visual height at this content width, using the same line layout as draw.
    using Intrinsic = std::function<glm::vec2(size_t, float)>;
    void Resolve(const std::vector<MeasureNode> &nodes, Rect external, Intrinsic intrinsic,
                 size_t activeCount = 0);
    const std::vector<MeasureResult> &Results() const { return m_Results; }

  private:
    float Size(size_t node, int axis);
    float Available(size_t node, int axis);
    void Distribute(size_t parent);
    Rect Content(size_t node);
    void Arrange(size_t node);
    [[noreturn]] void Error(size_t node, int axis, const std::string &message) const;
    const std::vector<MeasureNode> *m_Nodes = nullptr; // borrowed only inside Resolve
    Intrinsic m_Intrinsic;
    Rect m_External;
    std::vector<MeasureResult> m_Results;
    std::vector<std::array<unsigned char, 2>> m_State;
    std::vector<glm::vec2> m_Sizes;
    std::vector<glm::vec2> m_Positions;
    std::vector<unsigned char> m_Distribution;
    std::vector<float> m_Spare;
    std::vector<std::pair<double, size_t>> m_Caps;
    std::vector<double> m_CapWeights;
    std::vector<std::pair<size_t, int>> m_Path;
};
} // namespace Hazel::GameUI
