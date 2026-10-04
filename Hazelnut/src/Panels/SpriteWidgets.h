#pragma once
#include "Hazel/Assets/ProjectAssets.h"
#include "Hazel/Scene/Components.h"
namespace Hazel {
bool SpritePicker(const char* label,SpriteReference& value);
bool ClipPicker(const char* label,AnimationReference& value);
void SpriteSourceEditor(SpriteRendererComponent& component);
std::string SpriteDragText(const std::filesystem::path& sheet,SpriteID id);
}
