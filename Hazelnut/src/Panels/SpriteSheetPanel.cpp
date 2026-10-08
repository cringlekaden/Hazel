#include "SpriteSheetPanel.h"
#include "Hazel/Core/Resources.h"
#include "ContentBrowserPayload.h"
#include "Hazel/Project/Project.h"
#include "Hazel/Utils/PlatformUtils.h"
#include "SpriteWidgets.h"
#include "UI/PropertyUI.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <glm/gtc/type_ptr.hpp>
#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>
namespace Hazel
{
namespace
{
ImVec2 V(glm::vec2 p) { return {p.x, p.y}; }
glm::vec2 P(ImVec2 p) { return {p.x, p.y}; }
bool UInt(const char *label, uint32_t &v)
{
    return PropertyUI::Scalar(label, label, ImGuiDataType_U32, &v);
}
} // namespace
bool SpriteSheetPanel::CanEdit() const
{
    return !Availability || bool(Availability(EditorAction::EditAsset));
}
void SpriteSheetPanel::Close()
{
    m_Document.reset();
    m_Texture.reset();
    m_Open = m_Focused = false;
    m_FrameKeys.clear();
    m_Selected = m_SelectedClip = m_AddFrameRegion = 0;
    m_Playback.Reset();
    m_Gesture = 0;
    ++m_Identity;
}
bool SpriteSheetPanel::Assign(bool clip, SpriteID id)
{
    if (!CanEdit() || !m_Document)
        return false;
    const auto target = CaptureTarget ? CaptureTarget() : SceneTarget{};
    if (!target.Scene || !target.Entity)
    {
        m_Error = "Select a scene entity before assigning";
        return false;
    }
    // Saving precedes resource resolution and mutation. The controller validates
    // both captured target identities again after Save completes.
    if (Dirty() && !Save())
        return false;
    if (!CanEdit())
        return false;
    if (clip)
        return AssignClip && AssignClip({m_Document->Reference(), id}, target);
    return AssignSprite && AssignSprite({m_Document->Reference(), id}, target);
}
void SpriteSheetPanel::Bind(const Ref<ProjectAssets> &assets)
{
    m_Assets = assets;
    m_Document.reset();
    m_Texture.reset();
    m_Open = m_Focused = m_Create = m_Import = m_Recovery = m_ResumeImport = m_ResumeCreate = false;
    m_Selected = m_SelectedClip = m_DeleteID = 0;
    m_GridPreview.clear();
    m_DeleteUses.clear();
    m_Playback.Reset();
    m_Gesture = 0;
    m_PreviewStale = true;
    m_Error.clear();
    m_RecoveryPath.clear();
    m_RecoveryError.clear();
    m_FrameKeys.clear();
    m_AddFrameRegion = 0;
    m_RegionSearch.clear();
    m_ClipSearch.clear();
    ++m_Identity;
}
void SpriteSheetPanel::Fail(const std::exception &e)
{
    m_Error = e.what();
    if (ReportError)
        ReportError(m_Error);
}
bool SpriteSheetPanel::Open(const std::filesystem::path &path)
{
    auto doc = CreateScope<SpriteSheetDocument>(m_Assets);
    try
    {
        doc->Open(Project::MakeAssetReference(m_Assets->Root(), path));
    }
    catch (const std::exception &e)
    {
        m_RecoveryPath = path;
        m_Recovery = true;
        Fail(e);
        m_RecoveryError = m_Error;
        return false;
    }
    m_Document = std::move(doc);
    m_Selected = m_SelectedClip = 0;
    m_GridPreview.clear();
    m_Playback.Reset();
    m_Gesture = 0;
    m_PreviewStale = true;
    m_Open = true;
    m_Recovery = false;
    m_RecoveryPath.clear();
    m_RecoveryError.clear();
    m_Error.clear();
    m_Pan = {12, 12};
    m_FrameKeys.clear();
    m_AddFrameRegion = 0;
    ++m_Identity;
    return true;
}
ActionAvailability SpriteSheetPanel::RetryOpenAvailability() const
{
    if (GuardPending && GuardPending())
        return {"Resolve the pending document operation first"};
    if (!m_Recovery || m_RecoveryPath.empty())
        return {"No failed sheet open to retry"};
    if (!ReplaceDocument)
        return {"Sheet replacement is unavailable"};
    return Availability ? Availability(EditorAction::OpenAsset) : ActionAvailability{};
}
void SpriteSheetPanel::RequestRetryOpen()
{
    if (!RetryOpenAvailability())
        return;
    // ReplaceDocument uses the controller's OpenSheet guard and apply-time policy.
    // Keep recovery visible on cancel/save failure, and freeze the requested path.
    ReplaceDocument([this, path = m_RecoveryPath] { return Open(path); });
}
void SpriteSheetPanel::BeginCreate(const std::filesystem::path &path)
{
    if (!CanEdit())
        return;
    m_CreateTexture = Project::MakeAssetReference(m_Assets->Root(), path).generic_u8string();
    auto destination = std::filesystem::u8path(m_CreateTexture);
    destination.replace_extension(".hsprites");
    m_CreateDestination = destination.generic_u8string();
    m_Create = true;
}
void SpriteSheetPanel::BeginImport()
{
    if (!CanEdit())
        return;
    auto source = FileDialogs::OpenFile("Images\0*.png;*.jpg;*.jpeg;*.bmp;*.tga\0");
    if (source.empty())
        return;
    m_ImportSource = source;
    m_ImportDestination =
        (std::filesystem::path("Textures") / std::filesystem::u8path(source).filename())
            .generic_u8string();
    m_Import = true;
    m_Imported = false;
    m_Error.clear();
    auto sheet = std::filesystem::u8path(m_ImportDestination);
    sheet.replace_extension(".hsprites");
    m_CreateDestination = sheet.generic_u8string();
}
bool SpriteSheetPanel::Save()
{
    if (!m_Document)
        return false;
    if (!CanEdit())
    {
        m_Error = Availability ? Availability(EditorAction::SaveAsset).Reason : "Editing unavailable";
        return false;
    }
    try
    {
        m_Document->Save(Resources::Get().UserData/"recovery");
        m_Error.clear();
        if (AssetsChanged)
            AssetsChanged();
        return true;
    }
    catch (const std::exception &e)
    {
        Fail(e);
        return false;
    }
}
bool SpriteSheetPanel::Discard()
{
    if (m_Document && Dirty())
    {
        try
        {
            m_Document->Discard();
            m_PreviewStale = true;
            m_Playback.Reset();
            m_Selected = m_SelectedClip = m_AddFrameRegion = 0;
            m_FrameKeys.clear();
            m_Error.clear();
        }
        catch (const std::exception &e)
        {
            Fail(e);
            return false;
        }
    }
    return true;
}
void SpriteSheetPanel::Tick(double timestep)
{
    if (!m_Document || !m_SelectedClip || !m_Playback.Playing)
        return;
    try
    {
        m_Playback.Advance(m_Document->PreviewClip(m_SelectedClip), timestep);
    }
    catch (const std::exception &e)
    {
        m_Playback.Playing = false;
        Fail(e);
    }
}
void SpriteSheetPanel::PreviewTexture()
{
    if (!m_Document || (!m_PreviewStale && m_PreviewEpoch == m_Assets->Epoch()))
        return;
    m_PreviewStale = false;
    m_PreviewEpoch = m_Assets->Epoch();
    m_Texture.reset();
    try
    {
        auto &s = m_Document->Draft();
        s.Sampling.Validate(true);
        m_Texture = m_Assets->Texture(s.Texture, s.Sampling);
        m_Error.clear();
    }
    catch (const std::exception &e)
    {
        Fail(e);
    }
}
void SpriteSheetPanel::Sampling()
{
    auto &s = m_Document->Draft();
    bool changed = false, committed = false;
    PropertyUI::ReadOnly("texture", "Texture", s.Texture.generic_u8string().c_str());
    const auto dimensions =
        std::to_string(s.Sampling.Width) + " x " + std::to_string(s.Sampling.Height) + " px";
    PropertyUI::ReadOnly("dimensions", "Dimensions", dimensions.c_str());
    if (ImGui::Button("Select / repair texture...") && CanEdit())
    {
        auto path = FileDialogs::OpenFile("Images\0*.png;*.jpg;*.jpeg;*.bmp;*.tga\0");
        if (!path.empty() && CanEdit())
            try
            {
                m_Document->SelectTexture(
                    Project::MakeAssetReference(m_Assets->Root(), std::filesystem::u8path(path)));
                m_PreviewStale = true;
            }
            catch (const std::exception &e)
            {
                Fail(e);
            }
    }
    if (ImGui::BeginDragDropTarget())
    {
        if (auto *payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM"); payload && CanEdit())
            try
            {
                m_Document->SelectTexture(Project::MakeAssetReference(
                    m_Assets->Root(), ContentBrowserPath(payload->Data, payload->DataSize)));
                m_PreviewStale = true;
            }
            catch (const std::exception &e)
            {
                Fail(e);
            }
        ImGui::EndDragDropTarget();
    }
    auto filter = [&](const char *key, const char *label, TextureFilter &value, bool min)
    {
        PropertyUI::Row row(key, label);
        if (ImGui::BeginCombo("##value", TextureFilterName(value)))
        {
            for (unsigned i = 0; i < (min ? 6u : 2u); ++i)
            {
                auto choice = static_cast<TextureFilter>(i);
                if (ImGui::Selectable(TextureFilterName(choice), choice == value) && CanEdit())
                {
                    value = choice;
                    changed = committed = true;
                }
            }
            ImGui::EndCombo();
        }
    };
    filter("min", "Minification", s.Sampling.MinFilter, true);
    filter("mag", "Magnification", s.Sampling.MagFilter, false);
    auto wrap = [&](const char *key, const char *label, TextureWrap &value)
    {
        int w = static_cast<int>(value);
        if (PropertyUI::Combo(key, label, w, "Repeat\0ClampToEdge\0MirroredRepeat\0"))
        {
            value = static_cast<TextureWrap>(w);
            changed = committed = true;
        }
    };
    wrap("wrap-x", "Wrap X", s.Sampling.WrapS);
    wrap("wrap-y", "Wrap Y", s.Sampling.WrapT);
    if (PropertyUI::Checkbox("mips", "Mipmaps", s.Sampling.GenerateMips))
        changed = committed = true;
    int format = s.Sampling.Format == ImageFormat::R8     ? 0
                 : s.Sampling.Format == ImageFormat::RGB8 ? 1
                                                          : 2;
    if (PropertyUI::Combo("format", "Byte format", format, "R8 (red data)\0RGB8\0RGBA8\0"))
    {
        s.Sampling.Format = format == 0   ? ImageFormat::R8
                            : format == 1 ? ImageFormat::RGB8
                                          : ImageFormat::RGBA8;
        changed = committed = true;
    }
    if (ImGui::CollapsingHeader("Sampling help"))
        ImGui::TextWrapped("Regions never repeat. Linear filtering needs padding; mipmaps can blend "
                           "neighbors. Nearest / no "
                           "mipmaps is the pixel-art default. R8 samples red; no sRGB conversion.");
    if (changed)
        m_Document->Changed();
    if (committed)
        m_PreviewStale = true;
}
void SpriteSheetPanel::Regions(bool editable)
{
    auto &s = m_Document->Draft();
    if (ImGui::CollapsingHeader("Grid slicing"))
    {
        bool changed = false;
        changed |= UInt("Cell width", m_Grid.CellWidth);
        changed |= UInt("Cell height", m_Grid.CellHeight);
        changed |= UInt("Left offset", m_Grid.Left);
        changed |= UInt("Top offset", m_Grid.Top);
        changed |= UInt("Right margin", m_Grid.Right);
        changed |= UInt("Bottom margin", m_Grid.Bottom);
        changed |= UInt("Spacing X", m_Grid.SpacingX);
        changed |= UInt("Spacing Y", m_Grid.SpacingY);
        changed |= UInt("Columns (0 = derive)", m_Grid.Columns);
        changed |= UInt("Rows (0 = derive)", m_Grid.Rows);
        changed |= PropertyUI::Text("prefix", "Name prefix", m_Grid.Prefix);
        changed |= UInt("Starting number", m_Grid.Start);
        if (changed)
            m_GridPreview.clear();
        if (ImGui::Button("Preview grid"))
            try
            {
                m_GridPreview = GenerateGridPreview(s, m_Grid);
                m_Error.clear();
            }
            catch (const std::exception &e)
            {
                Fail(e);
            }
        ImGui::TextWrapped("%zu proposed cells. Incomplete edge cells are omitted. Apply appends new "
                           "rectangles and skips "
                           "exact existing rectangles; identities and edits are preserved.",
                           m_GridPreview.size());
        ImGui::BeginDisabled(!editable || m_GridPreview.empty());
        if (ImGui::Button("Apply grid") && CanEdit())
        {
            AddGridRegions(s, m_GridPreview);
            m_GridPreview.clear();
            m_Document->Changed();
        }
        ImGui::EndDisabled();
    }
    PropertyUI::Text("region-search", "Find region", m_RegionSearch);
    if (!m_RegionSearch.empty() && ImGui::SmallButton("Clear region search"))
        m_RegionSearch.clear();
    ImGui::BeginChild("Region list", {0, ImGui::GetTextLineHeightWithSpacing() * 6}, true);
    for (auto &r : s.Regions)
    {
        if (!m_RegionSearch.empty() && r.Name.find(m_RegionSearch) == std::string::npos)
            continue;
        ImGui::PushID(SpriteIDText(r.ID).c_str());
        if (ImGui::Selectable((r.Name + "###region").c_str(), m_Selected == r.ID))
            m_Selected = r.ID;
        if (ImGui::BeginDragDropSource())
        {
            auto text = SpriteDragText(m_Document->Reference(), r.ID);
            ImGui::SetDragDropPayload("HAZEL_SPRITE", text.c_str(), text.size() + 1);
            ImGui::TextUnformatted(r.Name.c_str());
            ImGui::EndDragDropSource();
        }
        ImGui::PopID();
    }
    ImGui::EndChild();
    auto it =
        std::find_if(s.Regions.begin(), s.Regions.end(), [&](auto &r) { return r.ID == m_Selected; });
    if (it == s.Regions.end())
        return;
    auto &r = *it;
    ImGui::PushID(SpriteIDText(r.ID).c_str());
    const auto targetName = TargetName ? TargetName() : std::string{};
    const auto assignment =
        Dirty() ? "Save Sheet and Assign Region to " + targetName : "Assign Region to " + targetName;
    ImGui::BeginDisabled(!editable || targetName.empty());
    if (ImGui::Button(assignment.c_str()))
        Assign(false, r.ID);
    ImGui::EndDisabled();
    if (targetName.empty())
        ImGui::TextWrapped("Select an entity in Scene Hierarchy to assign this region.");
    if (ImGui::CollapsingHeader("Region identity"))
        PropertyUI::ReadOnly("id", "ID", SpriteIDText(r.ID).c_str());
    ImGui::BeginDisabled(!editable);
    bool changed = PropertyUI::Text("name", "Region name", r.Name);
    changed |= UInt("X", r.Rect.X);
    changed |= UInt("Y", r.Rect.Y);
    changed |= UInt("Width", r.Rect.Width);
    changed |= UInt("Height", r.Rect.Height);
    float pivot[2]{r.Pivot.x, r.Pivot.y};
    const float center[2]{.5f, .5f};
    if (PropertyUI::Vector("pivot", "Pivot (0-1)", pivot, 2, .01f, center, "%.2f",
                          {"Top-left normalized. Center = 0.5, 0.5; feet = 0.5, 1."}, 0, 1))
    {
        r.Pivot = {pivot[0], pivot[1]};
        changed = true;
    }
    if (ImGui::Button("Center pivot"))
    {
        r.Pivot = {.5f, .5f};
        changed = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Feet pivot"))
    {
        r.Pivot = {.5f, 1};
        changed = true;
    }
    if (changed)
        m_Document->Changed();
    if (ImGui::Button("Delete region..."))
    {
        m_DeleteID = r.ID;
        m_DeleteClip = false;
        try
        {
            m_DeleteUses = m_Document->References(r.ID);
        }
        catch (const std::exception &e)
        {
            m_DeleteUses = {"Reference scan failed: " + std::string(e.what())};
        }
        m_DeleteRequested = true;
    }
    ImGui::EndDisabled();
    if (m_Texture && r.Rect.Width && r.Rect.Height &&
        uint64_t(r.Rect.X) + r.Rect.Width <= s.Sampling.Width &&
        uint64_t(r.Rect.Y) + r.Rect.Height <= s.Sampling.Height)
    {
        const auto uv = SpriteUV(r.Rect, s.Sampling.Width, s.Sampling.Height);
        auto anchor = P(ImGui::GetCursorScreenPos()) + glm::vec2(84, 84);
        auto top = anchor - r.Pivot * 100.f;
        auto *draw = ImGui::GetWindowDrawList();
        draw->AddImage((ImTextureID)(uintptr_t)m_Texture->GetRendererID(), V(top),
                       V(top + glm::vec2(100)), V(uv[3]), V(uv[1]));
        draw->AddLine(V(anchor - glm::vec2(7, 0)), V(anchor + glm::vec2(7, 0)),
                      IM_COL32(255, 190, 0, 255), 2);
        draw->AddLine(V(anchor - glm::vec2(0, 7)), V(anchor + glm::vec2(0, 7)),
                      IM_COL32(255, 190, 0, 255), 2);
        ImGui::Dummy({180, 170});
        ImGui::TextWrapped(
            "Cross = entity origin, unit world quad. Entity scale sets world size. Colliders "
            "keep their authored offsets.");
    }
    ImGui::PopID();
}
void SpriteSheetPanel::Clips(bool editable)
{
    auto &s = m_Document->Draft();
    ImGui::BeginDisabled(!editable);
    if (ImGui::Button("Create clip") && CanEdit())
    {
        SpriteClip c;
        c.ID = s.NewID(true);
        c.Name = "Clip " + std::to_string(s.Clips.size() + 1);
        s.Clips.push_back(c);
        m_SelectedClip = c.ID;
        m_Playback.Reset();
        m_Document->Changed();
    }
    ImGui::EndDisabled();
    PropertyUI::Text("clip-search", "Find clip", m_ClipSearch);
    if (!m_ClipSearch.empty() && ImGui::SmallButton("Clear clip search"))
        m_ClipSearch.clear();
    ImGui::BeginChild("Clips", {0, ImGui::GetTextLineHeightWithSpacing() * 4}, true);
    for (auto &c : s.Clips)
    {
        if (!m_ClipSearch.empty() && c.Name.find(m_ClipSearch) == std::string::npos)
            continue;
        ImGui::PushID(SpriteIDText(c.ID).c_str());
        if (ImGui::Selectable((c.Name + "###clip").c_str(), c.ID == m_SelectedClip))
        {
            m_SelectedClip = c.ID;
            m_Playback.Reset();
        }
        if (ImGui::BeginDragDropSource())
        {
            auto text = SpriteDragText(m_Document->Reference(), c.ID);
            ImGui::SetDragDropPayload("HAZEL_CLIP", text.c_str(), text.size() + 1);
            ImGui::TextUnformatted(c.Name.c_str());
            ImGui::EndDragDropSource();
        }
        ImGui::PopID();
    }
    ImGui::EndChild();
    auto it =
        std::find_if(s.Clips.begin(), s.Clips.end(), [&](auto &c) { return c.ID == m_SelectedClip; });
    if (it == s.Clips.end())
    {
        ImGui::TextWrapped("Select or create a clip, then choose regions here to add frames.");
        return;
    }
    auto &clip = *it;
    ImGui::PushID(SpriteIDText(clip.ID).c_str());
    const auto targetName = TargetName ? TargetName() : std::string{};
    const auto assignment =
        Dirty() ? "Save Sheet and Assign Clip to " + targetName : "Assign Clip to " + targetName;
    ImGui::BeginDisabled(!editable || targetName.empty());
    if (ImGui::Button(assignment.c_str()))
        Assign(true, clip.ID);
    ImGui::EndDisabled();
    if (targetName.empty())
        ImGui::TextWrapped("Select an entity in Scene Hierarchy to assign this clip.");
    ImGui::BeginDisabled(!editable);
    bool changed = PropertyUI::Text("name", "Clip name", clip.Name);
    changed |= PropertyUI::Checkbox("loop", "Loop", clip.Loop);
    if (std::none_of(s.Regions.begin(), s.Regions.end(),
                     [&](auto &r) { return r.ID == m_AddFrameRegion; }))
        m_AddFrameRegion = s.Regions.empty() ? 0 : s.Regions.front().ID;
    {
        PropertyUI::Row row("add-region", "Add frame");
        const auto name =
            m_AddFrameRegion ? s.Region(m_AddFrameRegion).Name : "No regions; slice a region first";
        if (ImGui::BeginCombo("##value", name.c_str()))
        {
            for (auto &region : s.Regions)
            {
                ImGui::PushID(SpriteIDText(region.ID).c_str());
                if (ImGui::Selectable(region.Name.c_str(), region.ID == m_AddFrameRegion))
                    m_AddFrameRegion = region.ID;
                ImGui::PopID();
            }
            ImGui::EndCombo();
        }
        ImGui::BeginDisabled(!m_AddFrameRegion);
        if (ImGui::Button("Add Frame") && CanEdit())
        {
            clip.Frames.push_back({m_AddFrameRegion, .1});
            changed = true;
        }
        ImGui::EndDisabled();
    }
    auto &keys = m_FrameKeys[clip.ID];
    while (keys.size() < clip.Frames.size())
        keys.push_back(m_NextFrameKey++);
    keys.resize(clip.Frames.size());
    int remove = -1, up = -1, down = -1;
    ImGui::BeginChild("Frames", {0, ImGui::GetTextLineHeightWithSpacing() * 9}, true);
    if (ImGui::BeginTable("Frame table", 4,
                          ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_RowBg |
                              ImGuiTableFlags_BordersInnerV))
    {
        ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, ImGui::GetFontSize() * 2);
        ImGui::TableSetupColumn("Region", ImGuiTableColumnFlags_WidthStretch, 2);
        ImGui::TableSetupColumn("Seconds", ImGuiTableColumnFlags_WidthStretch, 1);
        ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthFixed, ImGui::GetFontSize() * 5);
        ImGui::TableHeadersRow();
        for (size_t i = 0; i < clip.Frames.size(); ++i)
        {
            auto &frame = clip.Frames[i];
            ImGui::PushID(std::to_string(keys[i]).c_str());
            ImGui::TableNextRow();
            if (m_Playback.Frame == i)
                ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, ImGui::GetColorU32(ImGuiCol_Header));
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%zu", i + 1);
            ImGui::TableSetColumnIndex(1);
            std::string name = "Missing " + SpriteIDText(frame.Region);
            try
            {
                name = s.Region(frame.Region).Name;
            }
            catch (const std::exception &)
            {
            }
            ImGui::SetNextItemWidth(-1);
            if (ImGui::BeginCombo("##region", name.c_str()))
            {
                for (auto &r : s.Regions)
                {
                    ImGui::PushID(SpriteIDText(r.ID).c_str());
                    if (ImGui::Selectable(r.Name.c_str(), r.ID == frame.Region) && CanEdit())
                    {
                        frame.Region = r.ID;
                        changed = true;
                    }
                    ImGui::PopID();
                }
                ImGui::EndCombo();
            }
            ImGui::TableSetColumnIndex(2);
            ImGui::SetNextItemWidth(-1);
            changed |= ImGui::InputDouble("##seconds", &frame.Duration, 0, 0, "%.3f");
            PropertyUI::Help("Frame duration in seconds; must be finite and greater than zero");
            if (!std::isfinite(frame.Duration) || frame.Duration <= 0)
                PropertyUI::Validation("Use a positive duration");
            ImGui::TableSetColumnIndex(3);
            ImGui::BeginDisabled(i == 0);
            if (ImGui::SmallButton("^"))
                up = int(i);
            ImGui::EndDisabled();
            PropertyUI::Help("Move frame up");
            ImGui::SameLine();
            ImGui::BeginDisabled(i + 1 == clip.Frames.size());
            if (ImGui::SmallButton("v"))
                down = int(i);
            ImGui::EndDisabled();
            PropertyUI::Help("Move frame down");
            if (ImGui::SmallButton("Remove"))
                remove = int(i);
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
    ImGui::EndChild();
    if (CanEdit())
    {
        if (remove >= 0)
        {
            clip.Frames.erase(clip.Frames.begin() + remove);
            keys.erase(keys.begin() + remove);
            changed = true;
        }
        if (up >= 0)
        {
            std::swap(clip.Frames[up], clip.Frames[up - 1]);
            std::swap(keys[up], keys[up - 1]);
            changed = true;
        }
        if (down >= 0)
        {
            std::swap(clip.Frames[down], clip.Frames[down + 1]);
            std::swap(keys[down], keys[down + 1]);
            changed = true;
        }
    }
    if (ImGui::Button("Delete clip...") && CanEdit())
    {
        m_DeleteID = clip.ID;
        m_DeleteClip = true;
        try
        {
            m_DeleteUses = m_Document->References(clip.ID, true);
        }
        catch (const std::exception &e)
        {
            m_DeleteUses = {"Reference scan failed: " + std::string(e.what())};
        }
        m_DeleteRequested = true;
    }
    if (changed)
    {
        m_Document->Changed();
        m_Playback.Reset();
    }
    ImGui::EndDisabled();
    ImGui::Separator();
    ImGui::TextUnformatted("Asset preview (independent of gameplay)");
    try
    {
        const double total = SpritePlayback::Duration(m_Document->PreviewClip(clip.ID));
        if (ImGui::Button("Play preview"))
        {
            if (m_Playback.Finished)
                m_Playback.Reset();
            m_Playback.Playing = true;
        }
        if (ImGui::GetContentRegionAvail().x > ImGui::GetFontSize() * 18)
            ImGui::SameLine();
        if (ImGui::Button("Pause preview"))
            m_Playback.Playing = false;
        ImGui::SameLine();
        if (ImGui::Button("Stop preview"))
            m_Playback.Reset();
        float progress = static_cast<float>(m_Playback.Time / total * 100);
        if (PropertyUI::SliderFloat("scrub", "Scrub", progress, 0, 100, "%.1f%%"))
        {
            m_Playback.Playing = false;
            m_Playback.Finished = false;
            m_Playback.Scrub(clip, (double(progress) / 100) * total);
        }
        ImGui::Text("Frame %zu / %zu | %.3f / %.3f s%s", m_Playback.Frame + 1, clip.Frames.size(),
                    m_Playback.Time, total, m_Playback.Finished ? " | Finished" : "");
        const auto &r = s.Region(clip.Frames.at(m_Playback.Frame).Region);
        auto uv = SpriteUV(r.Rect, s.Sampling.Width, s.Sampling.Height);
        if (m_Texture)
            ImGui::Image((ImTextureID)(uintptr_t)m_Texture->GetRendererID(), {128, 128}, V(uv[3]),
                         V(uv[1]));
    }
    catch (const std::exception &e)
    {
        PropertyUI::Validation(("Preview unavailable: " + std::string(e.what())).c_str());
    }
    ImGui::PopID();
}
void SpriteSheetPanel::Canvas(bool editable)
{
    if (!m_Texture)
    {
        ImGui::TextWrapped(
            "Texture preview unavailable. Repair the source texture or sampling options, then retry.");
        if (ImGui::Button("Retry texture"))
            m_PreviewStale = true;
        return;
    }
    auto &sheet = m_Document->Draft();
    ImGui::TextDisabled("Scroll to zoom; middle-drag to pan");
    PropertyUI::Help("Left: select/move. Bottom-right square: resize. Create rectangle: drag. "
                     "Pivot tool: click/drag pivot. Pixel snapping is always on.");
    int tool = m_Tool == Tool::Create ? 1 : 0;
    if (PropertyUI::Combo("canvas-tool", "Canvas tool", tool, "Select / move\0Create rectangle\0"))
        m_Tool = tool ? Tool::Create : Tool::Select;
    PropertyUI::Checkbox("pivot-tool", "Pivot tool", m_PivotTool);
    PropertyUI::SliderFloat("zoom", "Zoom", m_Zoom, .125f, 32.f, "%.2fx", nullptr, {},
                            ImGuiSliderFlags_Logarithmic);
    if (ImGui::Button("Fit"))
    {
        auto a = ImGui::GetContentRegionAvail();
        m_Zoom =
            std::clamp(std::min((a.x - 24) / sheet.Sampling.Width, (a.y - 55) / sheet.Sampling.Height),
                       .125f, 32.f);
        m_Pan = {12, 12};
    }
    ImGui::BeginChild("Pixel canvas", {0, 0}, true,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    auto origin = P(ImGui::GetCursorScreenPos());
    auto size = ImGui::GetContentRegionAvail();
    size.x = std::max(size.x, 1.f);
    size.y = std::max(size.y, 1.f);
    ImGui::InvisibleButton("Canvas", size,
                           ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonMiddle);
    const bool hovered = ImGui::IsItemHovered();
    auto mouse = P(ImGui::GetMousePos());
    auto &io = ImGui::GetIO();
    if (hovered && io.MouseWheel != 0)
    {
        float next = std::clamp(m_Zoom * std::pow(1.2f, io.MouseWheel), .125f, 32.f);
        m_Pan = (mouse - origin) - (mouse - origin - m_Pan) * (next / m_Zoom);
        m_Zoom = next;
    }
    if (hovered && ImGui::IsMouseDragging(ImGuiMouseButton_Middle))
        m_Pan += P(io.MouseDelta);
    const auto top = origin + m_Pan;
    auto *draw = ImGui::GetWindowDrawList();
    draw->AddRectFilled(V(origin), V(origin + P(size)), IM_COL32(38, 38, 44, 255));
    draw->AddImage((ImTextureID)(uintptr_t)m_Texture->GetRendererID(), V(top),
                   V(top + glm::vec2(sheet.Sampling.Width, sheet.Sampling.Height) * m_Zoom), {0, 1},
                   {1, 0});
    auto pixel = (mouse - top) / m_Zoom;
    const bool insideImage = pixel.x >= 0 && pixel.y >= 0 && pixel.x < sheet.Sampling.Width &&
                             pixel.y < sheet.Sampling.Height;
    pixel.x = std::clamp(std::floor(pixel.x), 0.f, float(sheet.Sampling.Width - 1));
    pixel.y = std::clamp(std::floor(pixel.y), 0.f, float(sheet.Sampling.Height - 1));
    if (editable && hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
    {
        m_DragStart = pixel;
        m_Gesture = 0;
        if (m_Tool == Tool::Create && !m_PivotTool)
        {
            if (insideImage)
            {
                m_Gesture = 1;
                m_Original = {uint32_t(pixel.x), uint32_t(pixel.y), 1, 1};
            }
        }
        else
        {
            for (auto it = sheet.Regions.rbegin(); it != sheet.Regions.rend(); ++it)
            {
                auto &r = *it;
                auto corner =
                    top + glm::vec2(r.Rect.X + r.Rect.Width, r.Rect.Y + r.Rect.Height) * m_Zoom;
                if (r.ID == m_Selected && glm::length(mouse - corner) <= 9)
                {
                    m_Gesture = 3;
                    m_Original = r.Rect;
                    break;
                }
                if (insideImage && pixel.x >= r.Rect.X && pixel.y >= r.Rect.Y &&
                    pixel.x < uint64_t(r.Rect.X) + r.Rect.Width &&
                    pixel.y < uint64_t(r.Rect.Y) + r.Rect.Height)
                {
                    m_Selected = r.ID;
                    m_Original = r.Rect;
                    m_Gesture = m_PivotTool ? 4 : 2;
                    break;
                }
            }
        }
    }
    auto selected = std::find_if(sheet.Regions.begin(), sheet.Regions.end(),
                                 [&](auto &r) { return r.ID == m_Selected; });
    if (editable && m_Gesture && ImGui::IsMouseDown(ImGuiMouseButton_Left))
    {
        if (m_Gesture == 1)
        {
            m_Original.X = uint32_t(std::min(pixel.x, m_DragStart.x));
            m_Original.Y = uint32_t(std::min(pixel.y, m_DragStart.y));
            m_Original.Width = uint32_t(std::abs(pixel.x - m_DragStart.x)) + 1;
            m_Original.Height = uint32_t(std::abs(pixel.y - m_DragStart.y)) + 1;
        }
        else if (selected != sheet.Regions.end() && selected->Rect.Width <= sheet.Sampling.Width &&
                 selected->Rect.Height <= sheet.Sampling.Height &&
                 selected->Rect.X < sheet.Sampling.Width && selected->Rect.Y < sheet.Sampling.Height)
        {
            auto &r = *selected;
            const auto oldRect = r.Rect;
            const auto oldPivot = r.Pivot;
            if (m_Gesture == 2)
            {
                r.Rect.X = uint32_t(std::clamp(float(m_Original.X) + pixel.x - m_DragStart.x, 0.f,
                                               float(sheet.Sampling.Width - m_Original.Width)));
                r.Rect.Y = uint32_t(std::clamp(float(m_Original.Y) + pixel.y - m_DragStart.y, 0.f,
                                               float(sheet.Sampling.Height - m_Original.Height)));
            }
            else if (m_Gesture == 3)
            {
                r.Rect.Width = uint32_t(std::clamp(pixel.x - float(r.Rect.X) + 1, 1.f,
                                                   float(sheet.Sampling.Width - r.Rect.X)));
                r.Rect.Height = uint32_t(std::clamp(pixel.y - float(r.Rect.Y) + 1, 1.f,
                                                    float(sheet.Sampling.Height - r.Rect.Y)));
            }
            else
                r.Pivot = glm::clamp((pixel - glm::vec2(r.Rect.X, r.Rect.Y)) /
                                         glm::vec2(r.Rect.Width, r.Rect.Height),
                                     glm::vec2(0), glm::vec2(1));
            if (!(r.Rect == oldRect) || r.Pivot != oldPivot)
                m_Document->Changed();
        }
    }
    if (m_Gesture && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
    {
        if (m_Gesture == 1 && editable)
        {
            SpriteRegion r;
            r.ID = sheet.NewID();
            r.Name = "region_" + SpriteIDText(r.ID);
            r.Rect = m_Original;
            sheet.Regions.push_back(r);
            m_Selected = r.ID;
            m_Document->Changed();
        }
        m_Gesture = 0;
    }
    auto rectangle = [&](const PixelRect &r, ImU32 color, float thickness)
    {
        draw->AddRect(V(top + glm::vec2(r.X, r.Y) * m_Zoom),
                      V(top + glm::vec2(uint64_t(r.X) + r.Width, uint64_t(r.Y) + r.Height) * m_Zoom),
                      color, 0, 0, thickness);
    };
    for (auto &r : sheet.Regions)
    {
        const bool active = r.ID == m_Selected;
        rectangle(r.Rect, active ? IM_COL32(255, 205, 40, 255) : IM_COL32(65, 210, 240, 255),
                  active ? 2.f : 1.f);
        auto at = top + glm::vec2(r.Rect.X, r.Rect.Y) * m_Zoom;
        if (m_Zoom >= 1)
        {
            draw->AddRectFilled(
                V(at),
                V(at + glm::vec2(ImGui::CalcTextSize(r.Name.c_str()).x + 4, ImGui::GetTextLineHeight())),
                IM_COL32(0, 0, 0, 180));
            draw->AddText(V(at), IM_COL32_WHITE, r.Name.c_str());
        }
        if (active)
        {
            auto corner = top + glm::vec2(r.Rect.X + r.Rect.Width, r.Rect.Y + r.Rect.Height) * m_Zoom;
            draw->AddRectFilled(V(corner - glm::vec2(4)), V(corner + glm::vec2(4)),
                                IM_COL32(255, 205, 40, 255));
            auto pivot = top + (glm::vec2(r.Rect.X, r.Rect.Y) +
                                r.Pivot * glm::vec2(r.Rect.Width, r.Rect.Height)) *
                                   m_Zoom;
            draw->AddCircle(V(pivot), 6, IM_COL32(255, 110, 80, 255), 0, 2);
        }
    }
    for (auto &r : m_GridPreview)
        rectangle(r.Rect, IM_COL32(120, 255, 120, 200), 1);
    if (m_Gesture == 1)
        rectangle(m_Original, IM_COL32(255, 255, 255, 255), 2);
    ImGui::EndChild();
}
void SpriteSheetPanel::Render(bool editable)
{
    editable = editable && CanEdit();
    if (!GuardPending || !GuardPending())
    {
        if (m_ResumeImport)
            m_Import = true;
        if (m_ResumeCreate)
            m_Create = true;
    }
    if (m_Recovery)
    {
        ImGui::Begin("Sprite Sheet Recovery", &m_Recovery);
        ImGui::TextWrapped("Original metadata preserved: %s", m_RecoveryPath.generic_u8string().c_str());
        ImGui::TextWrapped("%s", m_RecoveryError.c_str());
        ImGui::TextWrapped(
            "Repair malformed or future-version metadata in a text editor. The current sheet "
            "and scene stay intact; no blank replacement is created.");
        if (ImGui::Button("Open file"))
            FileDialogs::OpenPath(m_RecoveryPath.u8string());
        ImGui::SameLine();
        if (ImGui::Button("Open folder"))
            FileDialogs::OpenPath(m_RecoveryPath.parent_path().u8string());
        if (ImGui::Button("Copy path"))
            ImGui::SetClipboardText(m_RecoveryPath.u8string().c_str());
        ImGui::SameLine();
        const auto retry = RetryOpenAvailability();
        ImGui::BeginDisabled(!retry);
        if (ImGui::Button("Retry open"))
            RequestRetryOpen();
        ImGui::EndDisabled();
        if (retry.Reason)
            ImGui::TextWrapped("%s", retry.Reason);
        ImGui::End();
    }
    if (m_Import)
        ImGui::OpenPopup("Import project texture");
    ImGui::SetNextWindowSize({580, 0}, ImGuiCond_Appearing);
    if (ImGui::BeginPopupModal("Import project texture", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::BeginDisabled(!editable);
        PropertyUI::ReadOnly("source", "Source", m_ImportSource.c_str());
        PropertyUI::Text("destination", "Texture path", m_ImportDestination,
                         {"Assets-relative destination. Existing files are never overwritten."},
                         m_Imported ? ImGuiInputTextFlags_ReadOnly : 0);
        PropertyUI::Checkbox("create-sheet", "Create sheet", m_ImportCreateSheet);
        if (m_ImportCreateSheet)
            PropertyUI::Text("sheet", "Sheet path", m_CreateDestination,
                             {"Assets-relative .hsprites destination"});
        if (m_Imported)
            ImGui::TextWrapped(
                "Texture already imported to %s. Retrying creates only the sheet; the texture "
                "stays even if you cancel.",
                m_ImportDestination.c_str());
        if (ImGui::Button(m_Imported ? "Retry Create Sheet / Finish" : "Import and Open") && CanEdit())
        {
            if (!m_Imported)
                try
                {
                    m_Assets->ImportTexture(std::filesystem::u8path(m_ImportSource),
                                            std::filesystem::u8path(m_ImportDestination));
                    m_Imported = true;
                    if (AssetsChanged)
                        AssetsChanged();
                    m_Error.clear();
                }
                catch (const std::exception &e)
                {
                    Fail(e);
                }
            if (m_Imported)
            {
                bool done = !m_ImportCreateSheet;
                if (m_ImportCreateSheet)
                {
                    const auto texture = m_ImportDestination, destination = m_CreateDestination;
                    m_Import = false;
                    m_ResumeImport = true;
                    ImGui::CloseCurrentPopup();
                    const auto create = [this, texture, destination]
                    {
                        if (!CanEdit())
                            return false;
                        try
                        {
                            auto doc = CreateScope<SpriteSheetDocument>(m_Assets);
                            doc->Create(std::filesystem::u8path(texture),
                                        std::filesystem::u8path(destination));
                            m_Document = std::move(doc);
                            m_Open = true;
                            m_PreviewStale = true;
                            m_Selected = m_SelectedClip = m_AddFrameRegion = 0;
                            m_FrameKeys.clear();
                            m_GridPreview.clear();
                            m_Playback.Reset();
                            m_Gesture = 0;
                            ++m_Identity;
                            m_Error.clear();
                            m_ResumeImport = false;
                            if (AssetsChanged)
                                AssetsChanged();
                            return true;
                        }
                        catch (const std::exception &e)
                        {
                            m_Error =
                                "Texture imported successfully to " + texture +
                                ". Sheet creation failed: " + e.what() +
                                ". Cancel the operation to correct the sheet path and retry, or finish "
                                "with the imported texture.";
                            if (ReportError)
                                ReportError(m_Error);
                            return false;
                        }
                    };
                    if (ReplaceDocument)
                        ReplaceDocument(create);
                    else
                        create();
                }
                if (done)
                {
                    m_Import = false;
                    ImGui::CloseCurrentPopup();
                }
            }
        }
        ImGui::EndDisabled();
        if (ImGui::Button(m_Imported ? "Finish with Imported Texture" : "Cancel import"))
        {
            m_Import = m_ResumeImport = false;
            ImGui::CloseCurrentPopup();
        }
        PropertyUI::Validation(m_Error.c_str());
        ImGui::EndPopup();
    }
    if (m_Create)
        ImGui::OpenPopup("Create sprite sheet");
    if (ImGui::BeginPopupModal("Create sprite sheet", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::BeginDisabled(!editable);
        PropertyUI::Text("source", "Texture path", m_CreateTexture);
        PropertyUI::Text("sheet", "Sheet path", m_CreateDestination);
        if (ImGui::Button("Create") && CanEdit())
        {
            const auto texture = m_CreateTexture, destination = m_CreateDestination;
            m_Create = false;
            m_ResumeCreate = true;
            ImGui::CloseCurrentPopup();
            const auto create = [this, texture, destination]
            {
                if (!CanEdit())
                    return false;
                try
                {
                    auto document = CreateScope<SpriteSheetDocument>(m_Assets);
                    document->Create(std::filesystem::u8path(texture),
                                     std::filesystem::u8path(destination));
                    m_Document = std::move(document);
                    m_Open = true;
                    m_PreviewStale = true;
                    m_Selected = m_SelectedClip = m_AddFrameRegion = 0;
                    m_GridPreview.clear();
                    m_Playback.Reset();
                    m_Gesture = 0;
                    m_Pan = {12, 12};
                    m_Recovery = false;
                    m_Error.clear();
                    m_FrameKeys.clear();
                    ++m_Identity;
                    m_ResumeCreate = false;
                    if (AssetsChanged)
                        AssetsChanged();
                    return true;
                }
                catch (const std::exception &e)
                {
                    Fail(e);
                    return false;
                }
            };
            if (ReplaceDocument)
                ReplaceDocument(create);
            else
                create();
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button("Cancel"))
        {
            m_Create = m_ResumeCreate = false;
            ImGui::CloseCurrentPopup();
        }
        if (!m_Error.empty())
            ImGui::TextWrapped("%s", m_Error.c_str());
        ImGui::EndPopup();
    }
    if (!m_Open || !m_Document)
        return;
    ImGui::SetNextWindowSize({900, 620}, ImGuiCond_FirstUseEver);
    // Closing hides the document; dirty state remains guarded until save/discard/project replacement.
    const std::string title = "Sprite Sheet: " + m_Document->Reference().filename().u8string() +
                              (Dirty() ? " *" : "") + "###Sprite Sheet";
    ImGui::Begin(title.c_str(), &m_Open);
    ImGui::PushID(std::to_string(m_Identity).c_str());
    m_Focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
    ImGui::TextWrapped("%s%s", m_Document->Reference().generic_u8string().c_str(),
                       Dirty() ? " | Unsaved" : " | Saved");
    if(m_Document->File().NeedsBackup())
        ImGui::TextWrapped("Known sheet defaults/legacy Filter encoding: Save writes the explicit sampling, list, pivot and loop values shown here, preserving the original first.");
    if(!m_Document->File().Backup().empty())
        ImGui::TextWrapped("Original: %s",m_Document->File().Backup().generic_u8string().c_str());
    if (ImGui::Button("Close Document") && RequestClose)
        RequestClose();
    if (!m_Document)
    {
        ImGui::PopID();
        ImGui::End();
        return;
    }
    ImGui::BeginDisabled(!editable);
    if (ImGui::Button("Save sheet"))
        Save();
    ImGui::SameLine();
    if (ImGui::Button("Reload from disk"))
    {
        if (!Dirty())
        {
            try
            {
                m_Document->Discard();
                m_Assets->Reload(m_Document->Reference());
                m_PreviewStale = true;
                m_Playback.Reset();
            }
            catch (const std::exception &e)
            {
                Fail(e);
            }
        }
        else
            m_Error = "Save or discard changes before reloading";
    }
    if (Dirty())
    {
        PropertyUI::WrapButton("Discard sheet changes...");
        if (ImGui::Button("Discard sheet changes..."))
            ImGui::OpenPopup("Discard sprite edits");
    }
    if (ImGui::BeginPopupModal("Discard sprite edits", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::TextUnformatted("Reload the saved sheet and discard this draft?");
        if (ImGui::Button("Discard") && CanEdit() && Discard())
            ImGui::CloseCurrentPopup();
        ImGui::SameLine();
        if (ImGui::Button("Keep editing"))
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
    ImGui::EndDisabled();
    if (!editable)
        ImGui::TextDisabled("Asset editing is disabled during Play / Simulate or a tool job; preview "
                            "stays independent.");
    if (!m_Error.empty())
        ImGui::TextColored({1, .4f, .3f, 1}, "%s", m_Error.c_str());
    PreviewTexture();
    const bool narrow = ImGui::GetContentRegionAvail().x < ImGui::GetFontSize() * 40;
    const float left =
        narrow ? 0 : std::min(ImGui::GetContentRegionAvail().x * .48f, ImGui::GetFontSize() * 30);
    ImGui::BeginChild("Sheet controls", {left, narrow ? ImGui::GetContentRegionAvail().y * .55f : 0},
                      true);
    if (ImGui::BeginTabBar("Authoring"))
    {
        if (ImGui::BeginTabItem("Regions"))
        {
            Regions(editable);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Animation"))
        {
            Clips(editable);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Texture"))
        {
            ImGui::BeginDisabled(!editable);
            Sampling();
            ImGui::EndDisabled();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    if (m_DeleteRequested)
    {
        ImGui::OpenPopup("Delete authored item");
        m_DeleteRequested = false;
    }
    if (ImGui::BeginPopupModal("Delete authored item", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::TextWrapped(
            "Deleting preserves authored references and reserves this ID forever. Existing "
            "uses will report missing content until explicitly reassigned.");
        for (auto &use : m_DeleteUses)
            ImGui::BulletText("%s", use.c_str());
        if (m_DeleteUses.empty())
            ImGui::TextDisabled("No saved uses found; unsaved scene/script references may still exist.");
        if (ImGui::Button("Delete") && editable && CanEdit())
        {
            if (m_DeleteClip)
            {
                m_Document->DeleteClip(m_DeleteID);
                m_SelectedClip = 0;
                m_Playback.Reset();
            }
            else
            {
                m_Document->DeleteRegion(m_DeleteID);
                m_Selected = 0;
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel deletion"))
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
    ImGui::EndChild();
    if (!narrow)
        ImGui::SameLine();
    ImGui::BeginChild("Sheet preview", {0, 0}, false);
    Canvas(editable);
    ImGui::EndChild();
    ImGui::PopID();
    ImGui::End();
}
} // namespace Hazel
