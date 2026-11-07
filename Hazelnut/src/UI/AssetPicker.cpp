#include "AssetPicker.h"
#include "PropertyUI.h"
#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>
#include <algorithm>
namespace Hazel
{
namespace
{
bool Matches(const std::filesystem::path &path, const std::string &kind)
{
    auto ext = path.extension();
    return kind == "Image" ? (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp" ||
                              ext == ".tga")
                           : ext == kind;
}
std::string Name(const std::filesystem::path &root, const std::filesystem::path &path)
{
    try
    {
        if (path.extension() == ".henv")return ReadEnvironment(root/path).Name;
        if (path.extension() == ".hmat")
            return ReadMaterial(root / path).Name;
        if (path.extension() == ".hmesh")
            return ReadMesh(root / path).Name;
    }
    catch (const std::exception &)
    {
    }
    return path.filename().u8string();
}
} // namespace
bool AssetPicker(const char *key, const char *label, std::filesystem::path &value, const char *kind,
                 const Ref<ProjectAssets> &assets, AssetPickerState &state,
                 const std::function<void(const std::filesystem::path &)> &open)
{
    if (state.Assets != assets || state.Epoch != (assets ? assets->Epoch() : 0) ||
        state.Kind != kind)
    {
        state = {};
        state.Assets = assets;
        state.Epoch = assets ? assets->Epoch() : 0;
        state.Kind = kind;
    }
    bool changed = false;
    {
        PropertyUI::Row row(key, label);
        if (value != state.Selected)
        {
            state.Selected = value;
            state.SelectedLabel = value.empty() ? "None"
                                  : assets      ? Name(assets->Root(), value)
                                                : value.generic_u8string();
        }
        auto selected = value.empty() ? std::string("None") : state.SelectedLabel;
        if (ImGui::BeginCombo("##value", selected.c_str()))
        {
            if (!state.Scanned && assets)
                try
                {
                    state.Scanned = true;
                    size_t visited = 0;
                    for (auto it = std::filesystem::recursive_directory_iterator(assets->Root());
                         it != std::filesystem::recursive_directory_iterator(); ++it)
                    {
                        if (++visited > 10000)
                            throw std::runtime_error(
                                "Asset picker scan limit (10,000 entries); use Content Browser to "
                                "reduce the project inventory");
                        if (it->is_symlink())
                        {
                            if (it->is_directory())
                                it.disable_recursion_pending();
                            continue;
                        }
                        if (it->is_directory() && it->path().filename() == "Intermediates")
                        {
                            it.disable_recursion_pending();
                            continue;
                        }
                        if (!it->is_regular_file() || !Matches(it->path(), kind))
                            continue;
                        if (state.Choices.size() >= 2048)
                            throw std::runtime_error("Asset picker choice limit (2,048)");
                        auto ref = it->path().lexically_relative(assets->Root());
                        state.Choices.push_back(
                            {ref, Name(assets->Root(), ref) + " — " + ref.generic_u8string()});
                    }
                    std::sort(state.Choices.begin(), state.Choices.end(),
                              [](auto &a, auto &b) { return a.Label < b.Label; });
                }
                catch (const std::exception &e)
                {
                    state.Error = e.what();
                }
            ImGui::InputTextWithHint("##filter", "Search names or paths", &state.Search);
            if (ImGui::Selectable("None", value.empty()))
            {
                value.clear();
                changed = true;
            }
            for (auto &choice : state.Choices)
                if (state.Search.empty() || choice.Label.find(state.Search) != std::string::npos)
                {
                    ImGui::PushID(choice.Path.generic_u8string().c_str());
                    if (ImGui::Selectable(choice.Label.c_str(), choice.Path == value))
                    {
                        value = choice.Path;
                        changed = true;
                    }
                    ImGui::PopID();
                }
            if (ImGui::SmallButton("Refresh choices"))
            {
                state.Choices.clear();
                state.Error.clear();
                state.Scanned = false;
            }
            PropertyUI::Validation(state.Error.c_str());
            ImGui::EndCombo();
        }
        row.Result(changed, true);
    }
    if (open && assets && !value.empty())
    {
        PropertyUI::Row link("open", "Asset");
        if (ImGui::SmallButton("Open selected"))
            open(assets->Root() / value);
    }
    return changed;
}
} // namespace Hazel
