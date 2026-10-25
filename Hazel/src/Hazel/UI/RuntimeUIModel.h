#pragma once
#include "UIScreen.h"
// Backend-independent layout and interaction model. No ImGui, GPU objects,
// managed callbacks, or world-camera coordinates belong in this layer.
#include <glm/glm.hpp>
#include <cstdint>
#include <cstddef>
#include <deque>
#include <string>
#include <vector>
#include <unordered_map>

namespace Hazel::GameUI
{
struct Rect
{
    glm::vec2 Position{}, Size{}; // viewport logical pixels, top-left origin
    bool Contains(glm::vec2 point) const;
    Rect Inset(glm::vec4 borders) const; // left, top, right, bottom
    Rect Intersect(const Rect &other) const;
};
struct CanvasLayout
{
    glm::vec2 ReferenceSize{1280, 720};
    glm::vec4 SafeInsets{16};
};
struct CanvasFit
{
    float Scale = 0;
    Rect Bounds{}, Content{};
    glm::vec2 ToLogical(glm::vec2 pixel) const;
};
CanvasFit FitCanvas(glm::vec2 viewportPixels, const CanvasLayout &config);
enum class SizeMode { Fixed, Fill, Content };
enum class Axis { Horizontal, Vertical };
enum class Align { Start, Center, End };
enum class ContainerKind { Stack, Grid, Wrap };
enum class TrackMode { Fixed, Fill };
enum class ImageFit { Stretch, Contain, Cover, Native };
struct Track
{
    TrackMode Mode = TrackMode::Fixed;
    float Value = 96, Minimum = 0, Maximum = 0;
    bool HasMaximum = false;
    bool operator==(const Track &o) const
    { return Mode==o.Mode && Value==o.Value && Minimum==o.Minimum && Maximum==o.Maximum && HasMaximum==o.HasMaximum; }
};
struct RectLayout
{
    static constexpr unsigned SchemaVersion = 3;
    SizeMode WidthMode = SizeMode::Fixed, HeightMode = SizeMode::Fixed;
    glm::vec2 FixedSize{240,48}, MaximumSize{}, MinimumSize{};
    glm::bvec2 HasMaximum{false,false};
    glm::vec2 Anchor{}, Offset{}, Pivot{};
    glm::vec4 Margin{}, Padding{};
    int GridRow=-1, GridColumn=-1;
    unsigned RowSpan=1, ColumnSpan=1;
};
struct FlowLayout
{
    ContainerKind Kind = ContainerKind::Stack;
    Axis Direction = Axis::Vertical;
    Align Main=Align::Start, Cross=Align::Start, LineAlign=Align::Start;
    float Gap=12, LineGap=12;
    glm::vec2 GridGap{12,12};
    std::vector<Track> Columns{Track{},Track{},Track{}}, Rows{Track{}};
};
void Validate(const RectLayout &config);
void Validate(const FlowLayout &config);
void Validate(const Track &config);
RectLayout EditMeasuredRect(const Rect &parent, const Rect &start, const RectLayout &initial,
                            glm::vec2 delta, unsigned edges);
Rect FramebufferCoverage(const Rect &pixels, glm::vec2 ratio, glm::vec2 framebuffer);
struct ImagePlacement { Rect Visual{}, UV{{0,0},{1,1}}; };
ImagePlacement FitImage(const Rect &box, glm::vec2 sourceSize, ImageFit fit, glm::vec2 alignment);

enum class HitPolicy
{
    PassThrough,
    Control,
    Block
};
struct Button
{
    uint64_t ID = 0;
    Rect Bounds{}, Clip{};
    std::string Action;
    int TabOrder = 0;
    bool Visible = true, Interactable = true;
    HitPolicy Hit = HitPolicy::Control;
    bool Continuous=false;
    bool Focusable=true;
};
enum class UIAction : uint8_t { Press, Up, Down, Left, Right, Next, Previous, Accept, Cancel, AdjustDecrease, AdjustIncrease, Scroll, ScrollUp, ScrollDown, ScrollLeft, ScrollRight, Count };
enum class InputDevice : uint8_t { Pointer, Keyboard, Gamepad };
struct ActionEdge { UIAction Action; bool Down=true; glm::vec2 Point{}; InputDevice Device=InputDevice::Keyboard; glm::vec2 Delta{}; };
struct InputFrame
{
    glm::vec2 Mouse{};
    std::vector<ActionEdge> Events; // Ordered, bounded device-neutral edges; navigation may repeat.
    glm::ivec2 Navigate{};
    int Adjust=0;
    glm::vec2 Scroll{};
    bool ScrollPage=false;
    bool Editing=false;
    bool NextPressed=false, PreviousPressed=false, SuppressAccept=false;
    bool KeyboardEnabled = true, FocusNavigation = false;
    bool Enabled = false, LeftDown = false, AcceptDown = false, NextDown = false,
         PreviousDown = false;
    // Optional non-repeat host edges preserve a quick click between updates.
    // Coordinates and edges must belong to this scene and viewport.
    bool PointerPressed = false, PointerReleased = false, AcceptPressed = false, CancelDown = false;
    glm::vec2 PressPosition{}, ReleasePosition{};
    bool HasReleasePosition = false;
};
enum class RoutedPhase : uint8_t { Activate, Begin, Move, End, Cancel };
struct Action
{
    uint64_t Entity = 0;
    std::string Name;
    uint64_t Screen=0, Owner=0, Sequence=0;
    UIEventKind Kind=UIEventKind::Activated;
    bool PreviousChecked=false, Checked=false;
    uint64_t PreviousSelection=0, Selection=0, Control=0;
    uint64_t SourceLifetime=0, SourceComponent=0, ControlLifetime=0, ControlComponent=0;
    uint64_t OwnerLifetime=0, OwnerComponent=0;
    std::string OwnerClass;
    float PreviousValue=0, Value=0, StartValue=0;
    uint64_t Gesture=0;
    bool Interrupted=false;
    RoutedPhase Phase=RoutedPhase::Activate;
    glm::vec2 Point{};
};
enum class ButtonState
{
    Normal,
    Hover,
    Pressed,
    Disabled,
    Focused
};
class InputRouter
{
  public:
    static constexpr size_t MaxButtons = 4096, MaxActions = 64;
    // Scene::GetIdentity, not the session identity: transitions retire queues.
    void Update(uint64_t scene, const std::vector<Button> &buttons, const InputFrame &frame,
                uint64_t generation = 0);
    // A nonzero generation belongs to an immutable published button list.
    // Reconcile changes eligibility/retirement without sampling/replaying input.
    void Reconcile(uint64_t scene, const std::vector<Button> &buttons, uint64_t generation = 0);
    bool Consume(uint64_t scene, Action &action);
    bool SetFocus(uint64_t scene, uint64_t id, const std::vector<Button> &buttons);
    ButtonState State(const Button &button) const;
    uint64_t Captured() const
    {
        return m_Capture;
    }
    uint64_t Focused() const
    {
        return m_Focus;
    }
    uint64_t Hovered() const
    {
        return m_Hover;
    }
    uint64_t NavigationTarget(const std::vector<Button>& candidates,glm::ivec2 direction,int sequential);
    static uint64_t Occluder(const std::vector<Button>& buttons,glm::vec2 point);
    void Reset();
    void CancelCapture(){m_Capture=m_KeyCapture=0;}
    void SetScope(uint64_t scene,uint64_t scope);
    uint64_t Scope() const {return m_Scope;}
    bool ConsumeCancel(uint64_t scene,uint64_t scope);
    uint64_t NavigationMisses() const {return m_NavigationMisses;}

    void Retire(uint64_t scene, uint64_t id);
    uint64_t DroppedActions() const
    {
        return m_Dropped;
    }

  private:
    void UpdateSample(uint64_t scene,const std::vector<Button>& buttons,const InputFrame& frame);
    void Navigate(const std::vector<Button>& buttons,glm::ivec2 direction);
    uint64_t m_Scope=0,m_NavigationMisses=0;
    unsigned m_Cancels=0;
    bool m_ScopeChanged=false,m_Cancel=false;
    uint64_t m_Scene = 0, m_Focus = 0, m_Hover = 0, m_Capture = 0, m_KeyCapture = 0;
    bool m_Ready = false, m_Left = false, m_Accept = false, m_Next = false, m_Previous = false;
    bool m_PointerGesture = false;
    uint64_t m_Generation = 0, m_Dropped = 0;
    std::vector<size_t> m_Order,m_NavigationScratch;
    std::vector<uint64_t> m_IDs;
    std::vector<size_t> m_Ranks;
    std::unordered_map<uint64_t, size_t> m_Index;
    std::deque<Action> m_Actions;
};
} // namespace Hazel::GameUI
