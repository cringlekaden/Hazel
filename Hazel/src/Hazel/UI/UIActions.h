#pragma once
#include "UIBindings.h"
#include "Hazel/Core/KeyCodes.h"
#include "Hazel/Core/Gamepad.h"
#include <array>
#include <optional>

namespace Hazel::GameUI {
// Point is the viewport-local position on InputFrame. Press is its button gesture.
// Adjust actions are exclusive with navigation during an explicit slider edit.
// Scroll remains reserved for a later control milestone.
struct DeviceSample {
    std::array<bool,349> Keys{};
    std::array<bool,8> Mouse{};
    GamepadSample Pad;
    glm::vec2 Point{};
};
// Single owner; bounded event buffer. No platform polling, scene refs or callbacks.
class UIInputAdapter {
public:
    static constexpr size_t MaxEdges=64;
    void Configure(const UIBindings& bindings);
    const UIBindings& Bindings() const {return m_Bindings;}
    void KeyEdge(int key,bool down,bool repeat,bool shift,bool commandModifier=false);
    void PointerEdge(int button,bool down,glm::vec2 point);
    InputFrame Sample(const DeviceSample& sample,float seconds,bool enabled,bool adjusting=false);
    void Cancel();
    InputDevice Device() const {return m_Device;}
    UIAction LastAction() const {return m_LastAction;}
    bool GamepadReady() const {return m_PadReady;}
    uint64_t Dropped() const {return m_Dropped;}
private:
    struct QueuedEdge {ActionEdge Edge;uint8_t Source=1;};
    void Queue(ActionEdge edge,uint8_t source=1);
    UIBindings m_Bindings=UIBindings::Defaults();
    std::vector<QueuedEdge> m_Edges;
    std::array<uint8_t,size_t(UIAction::Count)> m_Sources{};
    std::array<bool,size_t(UIAction::Count)> m_Held{},m_Armed{},m_Repeating{};
    std::array<float,size_t(UIAction::Count)> m_Repeat{};
    bool m_Adjusting=false;
    bool m_Ready=false,m_Overflow=false,m_PadReady=false;
    int m_Pad=-1,m_StickDirection=-1;
    uint64_t m_PadRevision=0,m_Dropped=0;
    UIAction m_LastAction=UIAction::Count;
    InputDevice m_Device=InputDevice::Pointer;
};
}
