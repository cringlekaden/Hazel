#pragma once
#include "Hazel/Assets/ProjectAssets.h"
#include <functional>
namespace Hazel
{
struct AssetPickerState
{
    Ref<ProjectAssets> Assets;
    uint64_t Epoch = 0;
    std::string Kind, Search, Error, SelectedLabel;
    std::filesystem::path Selected;
    bool Scanned = false;
    struct Choice
    {
        std::filesystem::path Path;
        std::string Label;
    };
    std::vector<Choice> Choices;
};
// Bounded, named project choices; paths remain authored references. Refresh is
// tied to the existing asset epoch, not a second file watcher/asset database.
bool AssetPicker(const char *key, const char *label, std::filesystem::path &value, const char *kind,
                 const Ref<ProjectAssets> &assets, AssetPickerState &state,
                 const std::function<void(const std::filesystem::path &)> &open = {});
} // namespace Hazel
