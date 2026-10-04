#pragma once
#include "ProjectAssets.h"
namespace Hazel {
// One native authoring document. UI interactions/selection never enter persisted metadata.
class SpriteSheetDocument {
public:
    explicit SpriteSheetDocument(Ref<ProjectAssets> assets):m_Assets(std::move(assets)) {}
    void Open(const std::filesystem::path& reference);
    void Create(const std::filesystem::path& texture,const std::filesystem::path& destination);
    void Save();
    void Discard();
    void SelectTexture(const std::filesystem::path& texture);
    void DeleteRegion(SpriteID id);
    void DeleteClip(SpriteID id);
    std::vector<std::string> References(SpriteID id,bool clip=false) const;
    SpriteSheetDefinition& Draft() {return m_Draft;}
    const SpriteSheetDefinition& Draft() const {return m_Draft;}
    void Changed() {m_Dirty=true;}
    bool Dirty() const {return m_Dirty;}
    const std::filesystem::path& Reference() const {return m_Reference;}
    Ref<ProjectAssets> Assets() const {return m_Assets;}
private:
    Ref<ProjectAssets> m_Assets;
    SpriteSheetDefinition m_Draft;
    std::filesystem::path m_Reference;
    std::string m_SavedText;
    bool m_Dirty=false;
};
}
