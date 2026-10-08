#pragma once
#include "SpriteSheet.h"
#include <unordered_map>
namespace Hazel {
struct ResolvedSprite {
    Ref<Texture2D> Texture;
    std::array<glm::vec2,4> UV{{{0,0},{1,0},{1,1},{0,1}}};
    std::array<glm::vec2,4> Corners=SpriteCorners({.5f,.5f});
    float TilingFactor=1;
};
struct SpriteResolution { Ref<const ResolvedSprite> Data; std::string Error; };
struct ResolvedClip {
    Ref<const SpriteSheetDefinition> Sheet; SpriteID ID=0;
    std::vector<Ref<const ResolvedSprite>> Frames;
    const SpriteClip& Definition() const { return Sheet->Clip(ID); }
};
class ProjectAssets {
public:
    explicit ProjectAssets(std::filesystem::path root):m_Root(std::filesystem::weakly_canonical(root)) {}
    const std::filesystem::path& Root() const {return m_Root;}
    uint64_t Epoch() const {return m_Epoch;}
    Ref<const SpriteSheetDefinition> Sheet(const std::filesystem::path& reference);
    Ref<Texture2D> Texture(const std::filesystem::path& reference,const TextureSpecification& specification,bool owned=true);
    Ref<const ResolvedSprite> Resolve(const SpriteSource& source);
    Ref<const ResolvedClip> Clip(const AnimationReference& reference);
    void Reload(const std::filesystem::path& reference);
    void Refresh(); // explicit freshness boundary before Play; no per-frame file checks
    void ImportTexture(const std::filesystem::path& source,const std::filesystem::path& destination);
private:
    std::filesystem::path m_Root; uint64_t m_Epoch=1;
    struct SheetEntry { Ref<const SpriteSheetDefinition> Data; std::string Error; };
    std::unordered_map<std::string,SheetEntry> m_Sheets;
    std::unordered_map<std::string,Ref<Texture2D>> m_Textures;
    std::unordered_map<std::string,Ref<const ResolvedSprite>> m_Sprites;
    std::unordered_map<std::string,Ref<const ResolvedClip>> m_Clips;
};
// CPU-only closure validation shared by export and editor reference scans.
struct SpriteAssetUse { std::filesystem::path File; std::string Location; SpriteReference Sprite; AnimationReference Animation; };
std::vector<SpriteAssetUse> FindSpriteAssetUses(const std::filesystem::path& root,const std::vector<std::filesystem::path>& files);
std::vector<std::filesystem::path> AuditSpriteAssets(const std::filesystem::path& root,const std::vector<std::filesystem::path>& files);
}
