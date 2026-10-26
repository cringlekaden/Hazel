#pragma once
#include "SpriteSheet.h"
#include "FontFamily.h"
#include "UIStyle.h"
#include "Hazel/Renderer/Font.h"
#include <unordered_map>
#include <atomic>
namespace Hazel {
struct ResolvedSprite {
    Ref<Texture2D> Texture;
    std::array<glm::vec2,4> UV{{{0,0},{1,0},{1,1},{0,1}}};
    std::array<glm::vec2,4> Corners=SpriteCorners({.5f,.5f});
    float TilingFactor=1;
    glm::vec2 SourceSize{}; // authored region pixels, independent of half-texel sample UVs
    glm::vec2 PixelSize() const {return SourceSize.x>0&&SourceSize.y>0?SourceSize:Texture?glm::vec2(Texture->GetWidth(),Texture->GetHeight()):glm::vec2(0);}

};
struct SpriteResolution { Ref<const ResolvedSprite> Data; std::string Error; };
struct ResolvedClip {
    Ref<const SpriteSheetDefinition> Sheet; SpriteID ID=0;
    std::vector<Ref<const ResolvedSprite>> Frames;
    const SpriteClip& Definition() const { return Sheet->Clip(ID); }
};
struct FontResolution { Ref<Font> Actual; std::string Error; bool Fallback=false; };
struct TextComponent;
struct FamilyResolution {
    Ref<Font> Actual;
    FontStyle Requested=FontStyle::Regular, Selected=FontStyle::Regular;
    std::filesystem::path Face;
    std::string Error;
    bool StyleFallback=false, DefaultFallback=false;
};
class ProjectAssets {
public:
    explicit ProjectAssets(std::filesystem::path root):m_Root(std::filesystem::weakly_canonical(root)) {}
    ~ProjectAssets();
    const std::filesystem::path& Root() const {return m_Root;}
    uint64_t Epoch() const {return m_Epoch;}
    struct StyleResolution {Ref<const UIStyleDefinition> Accepted;std::string Error;};
    StyleResolution Style(const std::filesystem::path& reference);
    size_t StyleBytes() const{return m_StyleBytes;}
    size_t StyleCount() const{return m_Styles.size();}
    size_t StyleLiveBytes() const{return m_StyleBudget->Bytes.load();}
    size_t StyleLiveVersions() const{return m_StyleBudget->Versions.load();}
    static constexpr size_t MaxStyleEntries=64, MaxStyleBytes=4*1024*1024, MaxStyleLiveBytes=8*1024*1024, MaxStyleVersions=128;
    Ref<Font> FamilyFont(const FontSelection& selection);
    FamilyResolution FamilyStatus(const FontSelection& selection);
    Ref<const FontFamilyDefinition> Family(const std::filesystem::path& reference);
    void ImportFontFile(const std::filesystem::path& source,const std::filesystem::path& destination,bool face);
    Ref<Font> FontAsset(const std::filesystem::path& reference);
    FontResolution FontStatus(const std::filesystem::path& reference);
    Ref<const SpriteSheetDefinition> Sheet(const std::filesystem::path& reference);
    Ref<Texture2D> Texture(const std::filesystem::path& reference,const TextureSpecification& specification,bool owned=true);
    Ref<const ResolvedSprite> Resolve(const SpriteSource& source);
    Ref<const ResolvedClip> Clip(const AnimationReference& reference);
    void Reload(const std::filesystem::path& reference);
    void Refresh(); // explicit freshness boundary before Play; no per-frame file checks
    void ImportTexture(const std::filesystem::path& source,const std::filesystem::path& destination);
private:
    struct StyleBudget {std::atomic<size_t> Bytes{0},Versions{0};};
    Ref<StyleBudget> m_StyleBudget=CreateRef<StyleBudget>();
    struct StyleEntry {Ref<const UIStyleDefinition> Accepted;std::string Error;size_t Bytes=0;};
    std::unordered_map<std::string,StyleEntry> m_Styles;
    size_t m_StyleBytes=0;
    void ReloadStyle(const std::string& key,StyleEntry& entry);
    std::filesystem::path m_Root; uint64_t m_Epoch=1;
    struct SheetEntry { Ref<const SpriteSheetDefinition> Data; std::string Error; };
    struct FamilyFace {std::weak_ptr<Font> Accepted;std::string Error;bool Loaded=false;};
    struct FamilyEntry {Ref<const FontFamilyDefinition> Accepted;std::array<FamilyFace,4> Faces;std::string Error;};
    std::unordered_map<std::string,FamilyEntry> m_Families;
    FamilyEntry* FamilyEntryFor(const std::filesystem::path& reference);
    void ReloadFamily(const std::string& key,FamilyEntry& entry);
    Ref<Font> ResolveFamilyFace(const std::string& key,FamilyEntry& entry,FontStyle style);
    struct FontEntry {std::weak_ptr<Font> Accepted;std::string Error;};
    std::unordered_map<std::string,FontEntry> m_Fonts;
    bool m_FontLimitReported=false;
    void ReloadFont(const std::string& key, FontEntry& entry);
    std::unordered_map<std::string,SheetEntry> m_Sheets;
    std::unordered_map<std::string,Ref<Texture2D>> m_Textures;
    std::unordered_map<std::string,Ref<const ResolvedSprite>> m_Sprites;
    std::unordered_map<std::string,Ref<const ResolvedClip>> m_Clips;
};
// Shared UI/world face resolution, retaining the accepted face on the active text component.
Ref<Font> ResolveTextFont(TextComponent& text,const Ref<ProjectAssets>& assets);
// CPU-only closure validation shared by export and editor reference scans.
struct SpriteAssetUse { std::filesystem::path File; std::string Location; SpriteReference Sprite; AnimationReference Animation; };
std::vector<SpriteAssetUse> FindSpriteAssetUses(const std::filesystem::path& root,const std::vector<std::filesystem::path>& files);
std::vector<std::filesystem::path> AuditSpriteAssets(const std::filesystem::path& root,const std::vector<std::filesystem::path>& files);
}
