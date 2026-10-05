#include "SpriteWidgets.h"
#include "ContentBrowserPayload.h"
#include "Hazel/Project/Project.h"
#include "Hazel/Utils/PlatformUtils.h"
#include "UI/PropertyUI.h"
#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>
namespace Hazel
{
std::string SpriteDragText(const std::filesystem::path &sheet, SpriteID id)
{
    return sheet.generic_u8string() + "\n" + SpriteIDText(id);
}
namespace
{
std::pair<std::filesystem::path, SpriteID> DragReference(const ImGuiPayload *payload)
{
    if (!payload || payload->DataSize <= 0 ||
        static_cast<const char *>(payload->Data)[payload->DataSize - 1] != '\0')
        throw std::runtime_error("Malformed sprite payload");
    std::string text(static_cast<const char *>(payload->Data), payload->DataSize - 1);
    auto at = text.find('\n');
    if (at == std::string::npos)
        throw std::runtime_error("Malformed sprite reference");
    return {std::filesystem::u8path(text.substr(0, at)), ParseSpriteID(text.substr(at + 1))};
}
void Bind(SpritePickerState &state, const std::filesystem::path &path, SpriteID id, bool clip)
{
    const auto spelling = path.generic_u8string();
    auto *owner = Project::GetActive().get();
    auto assets =
        state.ProjectOwner == owner && state.Assets ? state.Assets : Project::GetActive()->GetAssets();
    const bool referenceChanged = state.ProjectOwner != owner || !state.Bound ||
                                  state.BoundPath != spelling || state.BoundID != id;
    if (!referenceChanged && state.Epoch == assets->Epoch())
        return;
    if (referenceChanged)
    {
        state = {};
        state.Bound = true;
        state.BoundPath = spelling;
        state.BoundID = id;
        state.Path = spelling;
    }
    state.Assets = assets;
    state.ProjectOwner = owner;
    state.Epoch = assets->Epoch();
    state.PathChanged = true;
    state.Choices.reset();
    state.Name = id ? "Missing: " + SpriteIDText(id) : "Unassigned";
    if (id)
        try
        {
            auto sheet = assets->Sheet(path);
            state.Name = clip ? sheet->Clip(id).Name : sheet->Region(id).Name;
        }
        catch (const std::exception &e)
        {
            state.Error = e.what();
        }
}
void Reveal(const std::filesystem::path &reference)
{
    FileDialogs::OpenPath(Project::ResolveOwnedAsset(Project::GetAssetDirectory(), reference)
                              .parent_path()
                              .generic_u8string());
}
bool Pick(const char *label, std::filesystem::path &sheet, SpriteID &id, bool clip,
          SpritePickerState &state, const OpenAssetAction &open, const CanEditAction &canEdit)
{
    if (!Project::GetActive())
    {
        PropertyUI::ReadOnly("unavailable", label, "Open a project to select sprites");
        return false;
    }
    ImGui::PushID(label);
    Bind(state, sheet, id, clip);
    auto assets = state.Assets;
    const auto root = assets->Root();
    bool changed = false;
    auto edit = PropertyUI::Text("sheet-path", "Sheet path", state.Path,
                                 {"Assets-relative .hsprites path. Choose a "
                                  "region/clip to accept the reference.",
                                  state.Error.c_str()});
    state.PathChanged |= edit.Changed;
    if (ImGui::SmallButton("Browse sheet..."))
    {
        auto path = FileDialogs::OpenFile("Sprite sheets\0*.hsprites\0");
        if (!path.empty())
            try
            {
                state.Path =
                    Project::MakeAssetReference(root, std::filesystem::u8path(path)).generic_u8string();
                state.PathChanged = true;
                state.Error.clear();
            }
            catch (const std::exception &e)
            {
                state.Error = e.what();
            }
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("Refresh choices"))
        state.PathChanged = true;
    {
        PropertyUI::Row row("identity", label);
        if (ImGui::BeginCombo("##value", state.Name.c_str()))
        {
            if (state.PathChanged)
            {
                state.PathChanged = false;
                state.Choices.reset();
                try
                {
                    state.Choices = assets->Sheet(std::filesystem::u8path(state.Path));
                    state.Error.clear();
                }
                catch (const std::exception &e)
                {
                    state.Error = e.what();
                }
            }
            if (state.Choices)
            {
                auto ref = std::filesystem::u8path(state.Path);
                const auto choose = [&](SpriteID candidate, const std::string &name)
                {
                    ImGui::PushID(SpriteIDText(candidate).c_str());
                    if (ImGui::Selectable(name.c_str(), sheet == ref && id == candidate) &&
                        (!canEdit || canEdit()))
                    {
                        // Validate at application, not when text is typed.
                        try
                        {
                            if (clip)
                                assets->Clip({ref, candidate});
                            else
                                assets->Resolve(SpriteReference{ref, candidate});
                            sheet = ref;
                            id = candidate;
                            changed = true;
                            state.Error.clear();
                        }
                        catch (const std::exception &e)
                        {
                            state.Error = e.what();
                        }
                    }
                    ImGui::PopID();
                };
                if (clip)
                    for (auto &c : state.Choices->Clips)
                        choose(c.ID, c.Name);
                else
                    for (auto &r : state.Choices->Regions)
                        choose(r.ID, r.Name);
            }
            PropertyUI::Validation(state.Error.c_str());
            ImGui::EndCombo();
        }
        if ((!canEdit || canEdit()) && ImGui::BeginDragDropTarget())
        {
            try
            {
                if (auto *payload = ImGui::AcceptDragDropPayload(clip ? "HAZEL_CLIP" : "HAZEL_SPRITE"))
                {
                    auto r = DragReference(payload);
                    if (clip)
                        assets->Clip({r.first, r.second});
                    else
                        assets->Resolve(SpriteReference{r.first, r.second});
                    sheet = r.first;
                    id = r.second;
                    changed = true;
                    state.Error.clear();
                }
                if (auto *payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM"))
                {
                    auto ref = Project::MakeAssetReference(
                        root, ContentBrowserPath(payload->Data, payload->DataSize));
                    assets->Sheet(ref);
                    state.Path = ref.generic_u8string();
                    state.PathChanged = true;
                    state.Error.clear();
                }
            }
            catch (const std::exception &e)
            {
                state.Error = e.what();
            }
            ImGui::EndDragDropTarget();
        }
        if (ImGui::SmallButton("Clear reference") && (!canEdit || canEdit()))
        {
            sheet.clear();
            id = 0;
            changed = true;
            state.Error.clear();
        }
    }
    if (!sheet.empty())
        try
        {
            if (open && ImGui::SmallButton("Open Sheet"))
                open(Project::ResolveOwnedAsset(root, sheet));
            ImGui::SameLine();
            if (ImGui::SmallButton("Reveal Asset"))
                Reveal(sheet);
        }
        catch (const std::exception &e)
        {
            state.Error = e.what();
        }
    if (changed)
        state.Bound = false;
    ImGui::PopID();
    return changed;
}
} // namespace
bool SpritePicker(const char *label, SpriteReference &r, SpritePickerState &state,
                  const OpenAssetAction &open, const CanEditAction &canEdit)
{
    return Pick(label, r.Sheet, r.Region, false, state, open, canEdit);
}
bool ClipPicker(const char *label, AnimationReference &r, SpritePickerState &state,
                const OpenAssetAction &open, const CanEditAction &canEdit)
{
    return Pick(label, r.Sheet, r.Clip, true, state, open, canEdit);
}
void SpriteSourceEditor(SpriteRendererComponent &component, SpritePickerState &state,
                        const OpenAssetAction &open, const CanEditAction &canEdit)
{
    int type = static_cast<int>(component.Source.index());
    if (PropertyUI::Combo("source-type", "Source", type, "Color only\0Whole texture\0Sheet region\0") &&
        (!canEdit || canEdit()))
    {
        if (type == 0)
            component.Source = std::monostate{};
        else if (type == 1)
            component.Source = TextureSpriteSource{};
        else
            component.Source = SpriteReference{};
        state = {};
    }
    if (auto *texture = std::get_if<TextureSpriteSource>(&component.Source))
    {
        const auto accepted = texture->Texture.generic_u8string();
        if (!state.Bound || state.BoundPath != accepted)
        {
            state = {};
            state.Bound = true;
            state.BoundPath = accepted;
            state.Path = accepted;
        }
        auto edit =
            PropertyUI::Text("texture-path", "Texture path", state.Path,
                             {"Assets-relative or supported external texture path. Apply validates "
                              "without changing accepted content on failure.",
                              state.Error.c_str()});
        bool apply = edit.Committed;
        if (ImGui::SmallButton("Browse texture..."))
        {
            auto path = FileDialogs::OpenFile("Images\0*.png;*.jpg;*.jpeg;*.bmp;*.tga\0");
            if (!path.empty())
            {
                state.Path = path;
                apply = true;
            }
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Apply / drop texture"))
            apply = true;
        if ((!canEdit || canEdit()) && ImGui::BeginDragDropTarget())
        {
            if (auto *payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM"))
                try
                {
                    state.Path = ContentBrowserPath(payload->Data, payload->DataSize).generic_u8string();
                    apply = true;
                }
                catch (const std::exception &e)
                {
                    state.Error = e.what();
                }
            ImGui::EndDragDropTarget();
        }
        if (apply && (!canEdit || canEdit()))
            try
            {
                auto ref = Project::MakeAssetReference(Project::GetAssetDirectory(),
                                                       std::filesystem::u8path(state.Path));
                Project::GetActive()->GetAssets()->Texture(ref, TextureSpecification::FileDefaults(),
                                                           false);
                texture->Texture = ref;
                texture->Resource.reset();
                state.Bound = false;
                state.Error.clear();
            }
            catch (const std::exception &e)
            {
                state.Error = e.what();
            }
        const float defaultTiling = 1;
        PropertyUI::DragFloat("tiling", "Tiling", texture->TilingFactor, .1f, 0, 100, "%.2f",
                              &defaultTiling);
        if (!texture->Texture.empty() && ImGui::SmallButton("Reveal Asset"))
            try
            {
                FileDialogs::OpenPath(
                    Project::ResolveAssetPath(Project::GetAssetDirectory(), texture->Texture)
                        .parent_path()
                        .generic_u8string());
            }
            catch (const std::exception &e)
            {
                state.Error = e.what();
            }
    }
    else if (auto *region = std::get_if<SpriteReference>(&component.Source))
    {
        SpritePicker("Region", *region, state, open, canEdit);
    }
    PropertyUI::Validation(component.Resolved.Error.c_str());
}
} // namespace Hazel
