#pragma once

#include "Hazel/Renderer/Texture.h"

#include "Authoring/EditorActions.h"
#include <filesystem>
#include <functional>
#include <vector>

namespace Hazel
{

class ContentBrowserPanel
{
  public:
    ContentBrowserPanel();
    explicit ContentBrowserPanel(const std::filesystem::path &assetRoot);

    void OnImGuiRender();
    bool Visible=true;
    std::filesystem::path Folder() const {return m_CurrentDirectory.lexically_relative(m_BaseDirectory);}
    const std::string& Search() const {return m_Search;}
    int TypeFilter() const {return m_Type;}
    float Thumbnail() const {return m_ThumbnailSize;}
    bool Restore(const std::filesystem::path& folder,const std::string& search,int type,float thumbnail) {
        if(!Navigate(m_BaseDirectory/folder))return false;
        m_Search=search;m_Type=type;m_ThumbnailSize=thumbnail;return true;
    }
    void Refresh();
    void Reveal(const std::filesystem::path &path);
    std::function<ActionAvailability(EditorAction)> Availability;
    std::function<void(const std::filesystem::path &)> SelectAsset;
    std::function<void(const std::filesystem::path &)> CreateSpriteSheet;
    std::function<void()> ImportTexture;

  private:
    struct Entry
    {
        std::filesystem::path Path;
        std::string Relative, Name, Type;
        bool Directory = false;
    };
    bool Navigate(const std::filesystem::path &path);
    bool Allowed(EditorAction action) const;
    std::filesystem::path m_BaseDirectory;
    std::filesystem::path m_CurrentDirectory;
    std::filesystem::path m_Selected;
    std::vector<Entry> m_Entries;
    std::string m_Search, m_Error;
    int m_Type = 0;
    float m_ThumbnailSize = 64;

    Ref<Texture2D> m_DirectoryIcon;
    Ref<Texture2D> m_FileIcon;
};

} // namespace Hazel
