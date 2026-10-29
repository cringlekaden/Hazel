#pragma once
#include "RuntimeUIModel.h"
namespace Hazel::GameUI
{
// Composition on the authoritative RadioGroup; IDs are scene-local authored
// parts.
struct DropdownConfig
{
    uint64_t Trigger = 0, Popup = 0;
    float MinimumWidth = 160, MaximumHeight = 240, Gap = 4;
    std::string Placeholder = "Choose…";
};
void Validate(const DropdownConfig &config);

} // namespace Hazel::GameUI
