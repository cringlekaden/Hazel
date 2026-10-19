#pragma once
#include "RuntimeUIModel.h"
#include <array>
#include <functional>
#include <limits>
namespace Hazel::GameUI
{
// Indexed CPU layout inputs; callbacks supply visual metrics in logical units.
// One contiguous UI rect tree is measured once. Its external parent has a definite content rectangle.
struct MeasureNode
{
    uint64_t ID = 0;
    std::string Name;
    RectLayout Layout;
    FlowLayout Flow;
    bool Container = false, Visible = true;
    float Weight = 1;
    size_t Parent = (std::numeric_limits<size_t>::max)();
    std::vector<size_t> Children;
};
struct MeasureResult
{
    Rect Bounds;
    glm::vec2 Available{}, Measured{};
    bool Overflow = false;
    Rect Slot{};
    int Row=-1, Column=-1, Line=-1;
};
struct LayoutGuide { size_t Node=0; int Axis=0, Index=0; bool Line=false; Rect Bounds{}; };
class MeasuredLayout
{
  public:
    // width < 0 requests unwrapped natural visual width. Otherwise measure
    // visual height at this content width, using the same line layout as draw.
    using Intrinsic = std::function<glm::vec2(size_t, float)>;
    void Resolve(const std::vector<MeasureNode> &nodes, Rect external, Intrinsic intrinsic,
                 size_t activeCount = 0);
    const std::vector<MeasureResult> &Results() const { return m_Results; }
    const std::vector<LayoutGuide> &Guides() const { return m_Guides; }

  private:
    float Size(size_t node, int axis);
    float Available(size_t node, int axis);
    void Distribute(size_t parent);
    void PlaceGrid(size_t parent);
    void WrapLines(size_t parent);
    struct WrapLine {size_t Begin,End;float Used,Thickness,Spare;};
    void GridTracks(size_t parent, int axis);
    struct Allocation { size_t Index; float Minimum,Maximum,Weight,Result; bool Capped; };
    float Allocate(float free);
    struct Group {
        bool Placed=false;
        unsigned char WrapState=0;
        std::vector<size_t> Items;
        std::vector<WrapLine> Lines;
        std::array<unsigned char,2> GridState{};
        std::array<std::vector<float>,2> Tracks,Starts;
    };
    std::vector<Group> m_Groups;
    std::vector<Allocation> m_Allocations;
    std::vector<LayoutGuide> m_Guides;
    std::vector<glm::vec2> m_SlotSizes,m_SlotOffsets;
    std::vector<size_t> m_Depths,m_Incoming;
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
