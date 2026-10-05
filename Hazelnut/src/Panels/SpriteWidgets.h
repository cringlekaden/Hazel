#pragma once
#include "Hazel/Assets/ProjectAssets.h"
#include "Hazel/Scene/Components.h"
#include <functional>
namespace Hazel
{
// Owned by the inspector, cleared on document/selection changes. Accepted reference
// and typed draft are distinct; cache refresh follows the project's asset epoch.
struct SpritePickerState
{
    std::string Path, Error, Name, BoundPath;
    SpriteID BoundID = 0;
    uint64_t Epoch = 0;
    bool Bound = false, PathChanged = true;
    Ref<const SpriteSheetDefinition> Choices;
    Ref<ProjectAssets> Assets;
    const void* ProjectOwner=nullptr;
};
using CanEditAction = std::function<bool()>;
using OpenAssetAction = std::function<void(const std::filesystem::path &)>;
bool SpritePicker(const char *label, SpriteReference &value, SpritePickerState &state,
                  const OpenAssetAction &open = {}, const CanEditAction &canEdit = {});
bool ClipPicker(const char *label, AnimationReference &value, SpritePickerState &state,
                const OpenAssetAction &open = {}, const CanEditAction &canEdit = {});
void SpriteSourceEditor(SpriteRendererComponent &component, SpritePickerState &state,
                        const OpenAssetAction &open = {}, const CanEditAction &canEdit = {});
std::string SpriteDragText(const std::filesystem::path &sheet, SpriteID id);
} // namespace Hazel
