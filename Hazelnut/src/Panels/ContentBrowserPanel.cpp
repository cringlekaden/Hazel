#include "ContentBrowserPanel.h"
#include "Hazel/Core/Resources.h"
#include "Hazel/Project/Project.h"
#include "UI/PropertyUI.h"
#include <algorithm>
#include <cctype>
#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

namespace Hazel
{
namespace
{
std::string Lower(std::string text)
{
    std::transform(text.begin(), text.end(), text.begin(),
                   [](unsigned char c) { return char(std::tolower(c)); });
    return text;
}
const char *Type(const std::filesystem::path &path)
{
    const auto extension = Lower(path.extension().u8string());
    if (extension == ".png" || extension == ".jpg" || extension == ".jpeg" || extension == ".bmp" ||
        extension == ".tga")
        return "Texture";
    if (extension == ".hsprites")
        return "Sprite Sheet";
    if (extension == ".hprefab")
        return "Prefab";
    if (extension == ".hazel")
        return "Scene";
    if (extension == ".cs")
        return "Script";
    return "Other";
}
} // namespace
ContentBrowserPanel::ContentBrowserPanel() : ContentBrowserPanel(Project::GetAssetDirectory()) {}
ContentBrowserPanel::ContentBrowserPanel(const std::filesystem::path &assetRoot)
    : m_BaseDirectory(assetRoot), m_CurrentDirectory(assetRoot)
{
    m_DirectoryIcon =
        Texture2D::Create(Resources::Resolve("Icons/ContentBrowser/DirectoryIcon.png").generic_u8string());
    m_FileIcon =
        Texture2D::Create(Resources::Resolve("Icons/ContentBrowser/FileIcon.png").generic_u8string());
    Refresh();
}
bool ContentBrowserPanel::Allowed(EditorAction action) const
{
    return !Availability || bool(Availability(action));
}
bool ContentBrowserPanel::Navigate(const std::filesystem::path &path)
{
    try
    {
        const auto base = std::filesystem::weakly_canonical(m_BaseDirectory),
                   candidate = std::filesystem::weakly_canonical(path);
        const auto relative = candidate.lexically_relative(base);
        if (relative.empty() || relative.is_absolute() || *relative.begin() == ".." ||
            !std::filesystem::is_directory(candidate))
            throw std::runtime_error("Folder must remain inside this project's Assets");
        m_CurrentDirectory = path;
        m_Selected.clear();
        m_Search.clear();
        m_Error.clear();
        return true;
    }
    catch (const std::exception &error)
    {
        m_Error = error.what();
        return false;
    }
}
void ContentBrowserPanel::Reveal(const std::filesystem::path &path)
{
    if (Navigate(path.parent_path()))
    {
        m_Selected = path;
        Refresh();
        ImGui::SetWindowFocus("Content Browser");
    }
}
void ContentBrowserPanel::Refresh()
{
    m_Entries.clear();
    m_Error.clear();
    std::error_code error;
    const auto root = std::filesystem::weakly_canonical(m_BaseDirectory, error);
    if (error)
    {
        m_Error = error.message();
        return;
    }
    for (auto it = std::filesystem::recursive_directory_iterator(
             m_BaseDirectory, std::filesystem::directory_options::skip_permission_denied, error);
         !error && it != std::filesystem::recursive_directory_iterator(); it.increment(error))
    {
        auto canonical = std::filesystem::weakly_canonical(it->path(), error);
        if (error)
            break;
        auto relative = canonical.lexically_relative(root);
        if (relative.empty() || relative.is_absolute() || *relative.begin() == "..")
        {
            if (it->is_directory(error))
                it.disable_recursion_pending();
            continue;
        }
        const bool directory = it->is_directory(error);
        if (error)
            break;
        if (directory || it->is_regular_file(error))
            m_Entries.push_back(
                {it->path(), it->path().lexically_relative(m_BaseDirectory).generic_u8string(),
                 it->path().filename().u8string(), directory ? "Folder" : Type(it->path()), directory});
    }
    if (error)
        m_Error = "Cannot read asset directory: " + error.message();
    std::sort(m_Entries.begin(), m_Entries.end(),
              [](const Entry &a, const Entry &b)
              {
                  if (a.Directory != b.Directory)
                      return a.Directory;
                  auto x = Lower(a.Relative), y = Lower(b.Relative);
                  return x == y ? a.Relative < b.Relative : x < y;
              });
}
void ContentBrowserPanel::OnImGuiRender()
{
    if(!Visible)return;
    ImGui::Begin("Content Browser",&Visible);
    const auto canImport = Availability ? Availability(EditorAction::EditAsset) : ActionAvailability{};
    ImGui::BeginDisabled(!canImport);
    if (ImGui::Button("Import Texture...") && Allowed(EditorAction::EditAsset) && ImportTexture)
        ImportTexture();
    ImGui::EndDisabled();
    PropertyUI::Help(canImport.Reason ? canImport.Reason
                                      : "Import a project texture; optionally create its sprite sheet");
    PropertyUI::WrapButton("Refresh");
    if (ImGui::Button("Refresh"))
        Refresh();
    PropertyUI::WrapButton("View...");
    if (ImGui::Button("View..."))
        ImGui::OpenPopup("Browser view");
    if (ImGui::BeginPopup("Browser view"))
    {
        PropertyUI::SliderFloat("size", "Tile size", m_ThumbnailSize, 32, 160, "%.0f px");
        ImGui::EndPopup();
    }
    // Breadcrumbs wrap at narrow widths. Clicking a folder clears filters so the
    // user sees the folder they navigated to rather than an unrelated search.
    if (ImGui::SmallButton("Assets"))
        Navigate(m_BaseDirectory);
    auto walked = m_BaseDirectory;
    const auto relative = m_CurrentDirectory.lexically_relative(m_BaseDirectory);
    for (const auto &part : relative)
        if (part != ".")
        {
            walked /= part;
            ImGui::PushID(walked.generic_u8string().c_str());
            PropertyUI::WrapButton(part.u8string().c_str());
            if (ImGui::SmallButton(part.u8string().c_str()))
                Navigate(walked);
            ImGui::PopID();
        }
    ImGui::SetNextItemWidth(
        std::max(1.f, ImGui::GetContentRegionAvail().x - ImGui::GetFrameHeightWithSpacing()));
    ImGui::InputTextWithHint("##search", "Search Assets...", &m_Search);
    ImGui::SameLine();
    if (ImGui::Button("X##clear-search"))
        m_Search.clear();
    PropertyUI::Help("Clear search");
    PropertyUI::Combo("type", "Type", m_Type, "All\0Texture\0Sprite Sheet\0Prefab\0Scene\0Script\0Other\0");
    if ((!m_Search.empty() || m_Type) && ImGui::SmallButton("Reset filters"))
    {
        m_Search.clear();
        m_Type = 0;
    }
    if (!m_Search.empty() || m_Type)
        ImGui::TextDisabled("Searching all Assets");
    PropertyUI::Validation(m_Error.c_str());
    const char *types[] = {"All", "Texture", "Sprite Sheet", "Prefab", "Scene", "Script", "Other"};
    std::vector<const Entry *> visible;
    const auto query = Lower(m_Search);
    for (const auto &entry : m_Entries)
    {
        if (query.empty() && !m_Type)
        {
            if (entry.Path.parent_path() != m_CurrentDirectory)
                continue;
        }
        else
        {
            if (entry.Directory)
                continue;
            if (!query.empty() && Lower(entry.Relative).find(query) == std::string::npos)
                continue;
            if (m_Type && entry.Type != types[m_Type])
                continue;
        }
        visible.push_back(&entry);
    }
    ImGui::TextDisabled("%zu items%s", visible.size(),
                        m_Selected.empty() ? "" : " | Enter or double-click to open selection");
    ImGui::BeginChild("Asset entries", {0, 0}, false);
    const float size = m_ThumbnailSize * ImGui::GetIO().FontGlobalScale;
    const int columns =
        std::max(1, int(ImGui::GetContentRegionAvail().x / (size + ImGui::GetFontSize() * 2)));
    std::filesystem::path openPath;
    if (ImGui::BeginTable("Assets", columns, ImGuiTableFlags_SizingStretchSame))
    {
        for (const auto *entry : visible)
        {
            ImGui::TableNextColumn();
            ImGui::PushID(entry->Relative.c_str());
            ImGui::PushStyleColor(ImGuiCol_Button, m_Selected == entry->Path
                                                       ? ImGui::GetStyleColorVec4(ImGuiCol_HeaderActive)
                                                       : ImVec4{0, 0, 0, 0});
            auto icon = entry->Directory ? m_DirectoryIcon : m_FileIcon;
            if (ImGui::ImageButton("##file", (ImTextureID)(uintptr_t)icon->GetRendererID(), {size, size},
                                   {0, 1}, {1, 0}))
                m_Selected = entry->Path;
            const bool hovered = ImGui::IsItemHovered();
            if (hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                openPath = entry->Path;
            if (ImGui::BeginDragDropSource())
            {
                const auto path = entry->Path.generic_u8string();
                ImGui::SetDragDropPayload("CONTENT_BROWSER_ITEM", path.c_str(), path.size() + 1);
                ImGui::TextUnformatted(entry->Name.c_str());
                ImGui::EndDragDropSource();
            }
            if (ImGui::BeginPopupContextItem())
            {
                m_Selected = entry->Path;
                if (ImGui::MenuItem(entry->Directory ? "Open Folder" : "Open / Inspect"))
                    openPath = entry->Path;
                ImGui::BeginDisabled(!canImport);
                if (!entry->Directory && entry->Type == "Texture" && CreateSpriteSheet &&
                    ImGui::MenuItem("Create Sprite Sheet...") && Allowed(EditorAction::EditAsset))
                    CreateSpriteSheet(entry->Path);
                ImGui::EndDisabled();
                ImGui::EndPopup();
            }
            ImGui::PopStyleColor();
            ImGui::TextWrapped("%s", entry->Name.c_str());
            if (!query.empty() || m_Type)
                ImGui::TextDisabled("%s", entry->Relative.c_str());
            else
                ImGui::TextDisabled("%s", entry->Type.c_str());
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
    if (visible.empty())
        ImGui::TextWrapped("No matching assets. Reset filters or import a texture above.");
    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && !ImGui::GetIO().WantTextInput &&
        !ImGui::IsAnyItemActive() && !ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId) &&
        ImGui::IsKeyPressed(ImGuiKey_Enter, false))
        openPath = m_Selected;
    const auto blank = ImGui::GetContentRegionAvail();
    if (blank.y > 0 && ImGui::InvisibleButton("##blank", {std::max(1.f, blank.x), blank.y}))
        m_Selected.clear();
    ImGui::EndChild();
    if (!openPath.empty())
    {
        auto found = std::find_if(m_Entries.begin(), m_Entries.end(),
                                  [&](auto &entry) { return entry.Path == openPath; });
        if (found != m_Entries.end() && found->Directory)
            Navigate(openPath);
        else if (SelectAsset)
            SelectAsset(openPath);
    }
    ImGui::End();
}
} // namespace Hazel
