#pragma once
// Backend-independent layout and interaction model. No ImGui, GPU objects,
// managed callbacks, or world-camera coordinates belong in this layer.
#include <glm/glm.hpp>
#include <cstdint>
#include <cstddef>
#include <deque>
#include <string>
#include <vector>

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
    struct RectLayout
    {
        glm::vec2 AnchorMin{}, AnchorMax{}, Offset{}, SizeDelta{240, 48}, Pivot{};
        glm::vec2 MinimumSize{};
        glm::vec4 Margin{}, Padding{};
    };
    void Validate(const RectLayout &config);
    Rect ResolveRect(const Rect &parent, const RectLayout &config);
    enum class Axis
    {
        Horizontal,
        Vertical
    };
    enum class Align
    {
        Start,
        Center,
        End,
        Stretch
    };
    struct FlowLayout
    {
        Axis Direction = Axis::Vertical;
        Align Main = Align::Start, Cross = Align::Stretch;
        float Gap = 12;
    };
    struct FlowItem
    {
        glm::vec2 PreferredSize{}, MinimumSize{};
        glm::vec4 Margin{};
        float Flex = 0;
        bool Visible = true;
    };
    // Caller retains output capacity. Hidden children consume neither space nor gaps.
    void ArrangeFlow(const Rect &content, const FlowLayout &config,
                     const std::vector<FlowItem> &items, std::vector<Rect> &output);

    struct Button
    {
        uint64_t ID = 0;
        Rect Bounds{}, Clip{};
        std::string Action;
        int TabOrder = 0;
        bool Visible = true, Interactable = true;
    };
    struct InputFrame
    {
        glm::vec2 Mouse{};
        bool KeyboardEnabled = true, FocusNavigation = false;
        bool Enabled = false, LeftDown = false, AcceptDown = false, NextDown = false,
             PreviousDown = false;
        // Optional non-repeat host edges preserve a quick click between updates.
        // Coordinates and edges must belong to this scene and viewport.
        bool PointerPressed = false, PointerReleased = false, AcceptPressed = false, CancelDown = false;
        glm::vec2 PressPosition{}, ReleasePosition{};
        bool HasReleasePosition=false;
    };
    struct Action
    {
        uint64_t Entity = 0;
        std::string Name;
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
        void Update(uint64_t scene, const std::vector<Button> &buttons, const InputFrame &frame);
        bool Consume(uint64_t scene, Action &action);
        bool SetFocus(uint64_t scene, uint64_t id, const std::vector<Button> &buttons);
        ButtonState State(const Button &button) const;
        uint64_t Captured() const { return m_Capture; }
        uint64_t Focused() const
        {
            return m_Focus;
        }
        uint64_t Hovered() const
        {
            return m_Hover;
        }
        void Reset();
        void Retire(uint64_t scene, uint64_t id);

      private:
        uint64_t m_Scene = 0, m_Focus = 0, m_Hover = 0, m_Capture = 0, m_KeyCapture = 0;
        bool m_Ready = false, m_Left = false, m_Accept = false, m_Next = false, m_Previous = false;
        std::vector<size_t> m_Order;
        std::vector<uint64_t> m_IDs;
        std::deque<Action> m_Actions;
    };
} // namespace Hazel::GameUI
