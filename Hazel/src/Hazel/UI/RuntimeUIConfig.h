#pragma once
#include "Hazel/Scene/Components.h"
#include <yaml-cpp/yaml.h>
namespace Hazel
{
class Entity;
void WriteUIComponents(YAML::Emitter &out, Entity entity);
void ReadUIComponents(const YAML::Node &node, Entity entity);
void ValidateUIConfiguration(const YAML::Node &node);
std::string UIAuthoredState(Entity entity);
UICanvasComponent ReadUICanvasComponent(const YAML::Node &node);
UIRectComponent ReadUIRectComponent(const YAML::Node &node);
UILayoutComponent ReadUILayoutComponent(const YAML::Node &node);
UITextStyleComponent ReadUITextStyleComponent(const YAML::Node &node);
UINineSliceComponent ReadUINineSliceComponent(const YAML::Node &node);
UIButtonComponent ReadUIButtonComponent(const YAML::Node &node);
UIRadioGroupComponent ReadUIRadioGroupComponent(const YAML::Node &node);
UITabsComponent ReadUITabsComponent(const YAML::Node &node);
UITabComponent ReadUITabComponent(const YAML::Node &node);
UITooltipComponent ReadUITooltipComponent(const YAML::Node &node);
UIDropdownComponent ReadUIDropdownComponent(const YAML::Node &node);
UIScrollViewComponent ReadUIScrollViewComponent(const YAML::Node &node);
UISliderComponent ReadUISliderComponent(const YAML::Node &node);
UIProgressComponent ReadUIProgressComponent(const YAML::Node &node);
} // namespace Hazel
