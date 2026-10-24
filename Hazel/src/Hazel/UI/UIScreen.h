#pragma once
#include <cstdint>

namespace Hazel::GameUI {
// Authored policy on a Canvas. IDs are local to its scene; no live state is
// copied.
enum class ScreenCancel : uint8_t { Ignore, Close, NotifyOwner };
struct ScreenPolicy {
  bool Managed = false, InitiallyOpen = true;
  ScreenCancel Cancel = ScreenCancel::Close;
  uint64_t InitialFocus = 0, DefaultAction = 0, CancelTarget = 0,
           ReturnFocus = 0, EventOwner = 0;
  bool operator==(const ScreenPolicy &b) const {
    return Managed == b.Managed && InitiallyOpen == b.InitiallyOpen &&
           Cancel == b.Cancel && InitialFocus == b.InitialFocus &&
           DefaultAction == b.DefaultAction && CancelTarget == b.CancelTarget &&
           ReturnFocus == b.ReturnFocus && EventOwner == b.EventOwner;
  }
  bool operator!=(const ScreenPolicy &b) const { return !(*this == b); }
};
enum class ScreenOperation : uint8_t { Open, Close, Pop, Replace };
enum class UIEventKind : uint8_t { Activated, Cancel, ToggleChanged, SelectionChanged };
enum class ControlKind : uint8_t { Button, Toggle, Radio };
enum class FillDirection : uint8_t { Right, Left, Down, Up };
struct ScreenState {
  uint64_t ID = 0, Owner = 0;
  bool Open = false, Available = false, Modal = false;
};
} // namespace Hazel::GameUI
