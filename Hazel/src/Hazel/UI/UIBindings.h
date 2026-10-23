#pragma once
#include "RuntimeUIModel.h"
#include <array>
namespace Hazel::GameUI {
enum class ShiftMatch : uint8_t { Any, Without, With };
struct UIBinding {
    int Key=-1, Alternate=-1, Pad=-1;
    ShiftMatch Shift=ShiftMatch::Any;
};
struct UIBindings {
    std::array<UIBinding,size_t(UIAction::Count)> Actions{};
    int PointerButton=0, Stick=0; // -1 none, 0 left, 1 right
    float DeadZone=.55f, NeutralZone=.25f, RepeatDelay=.35f, RepeatInterval=.10f;
    static UIBindings Defaults();
};
const char* ActionName(UIAction action);
std::string KeyName(int key);
void Validate(const UIBindings& bindings); // conflicts reject atomically
std::string SerializeBindings(const UIBindings& bindings);
UIBindings ParseBindings(const std::string& text);
}
