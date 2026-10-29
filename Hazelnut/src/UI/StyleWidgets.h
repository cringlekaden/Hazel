#pragma once
#include "Hazel/Assets/UIStyle.h"
#include "Panels/SpriteWidgets.h"
namespace Hazel
{
struct StyleWidgetState
{
    std::array<SpritePickerState, 6> Images;
};
bool StyleValuesEditor(UIStyleValues &values, StyleWidgetState &state);
} // namespace Hazel
