#include "hzpch.h"
#include "SpriteSheetDocument.h"
#include "Hazel/Project/Project.h"
#include "Hazel/Core/FileDocument.h"
namespace Hazel {
void SpriteSheetDocument::Open(const std::filesystem::path& reference) {
    FileDocument file;file.Open(Project::ResolveOwnedAsset(m_Assets->Root(),reference));
    bool defaults=false;auto candidate=ReadSpriteSheetText(file.Original(),&defaults);file.Check();
    if(defaults)file.PreserveOriginal();
    m_Draft=std::move(candidate);m_Reference=reference;m_File=std::move(file);m_Dirty=false;
}
void SpriteSheetDocument::Create(const std::filesystem::path& texture,const std::filesystem::path& destination) {
    SpriteSheetDefinition candidate;candidate.Texture=texture;
    auto image=Texture2D::ReadImage(Project::ResolveOwnedAsset(m_Assets->Root(),texture),candidate.Sampling.Format);
    candidate.Sampling.Width=image.Width;candidate.Sampling.Height=image.Height;
    SaveSpriteSheet(m_Assets->Root(),destination,candidate,WriteMode::CreateNew);
    FileDocument file;file.Open(Project::ResolveOwnedAsset(m_Assets->Root(),destination));
    if(file.Original()!=WriteSpriteSheetText(candidate))throw std::runtime_error("Created sheet changed externally; previous draft retained. Open the created file explicitly.");
    m_Draft=std::move(candidate);m_Reference=destination;m_File=std::move(file);m_Dirty=false;m_Assets->Reload(destination);
}
void SpriteSheetDocument::Save(const std::filesystem::path& recoveryRoot) {
    m_File.Check();
    const auto text=ValidateSpriteSheetSave(m_Assets->Root(),m_Reference,m_Draft);
    m_File.Save(text,recoveryRoot);
    m_Dirty=false;m_Assets->Reload(m_Reference);
}
void SpriteSheetDocument::Discard() {Open(m_Reference);}
void SpriteSheetDocument::SelectTexture(const std::filesystem::path& texture) {
    auto image=Texture2D::ReadImage(Project::ResolveOwnedAsset(m_Assets->Root(),texture),m_Draft.Sampling.Format);
    m_Draft.Texture=texture;m_Draft.Sampling.Width=image.Width;m_Draft.Sampling.Height=image.Height;Changed();
}
void SpriteSheetDocument::DeleteRegion(SpriteID id) {
    m_Draft.Region(id);m_Draft.RetiredRegionIDs.push_back(id);
    m_Draft.Regions.erase(std::remove_if(m_Draft.Regions.begin(),m_Draft.Regions.end(),[id](auto& r){return r.ID==id;}),m_Draft.Regions.end());Changed();
}
void SpriteSheetDocument::DeleteClip(SpriteID id) {
    m_Draft.Clip(id);m_Draft.RetiredClipIDs.push_back(id);
    m_Draft.Clips.erase(std::remove_if(m_Draft.Clips.begin(),m_Draft.Clips.end(),[id](auto& c){return c.ID==id;}),m_Draft.Clips.end());Changed();
}
const SpriteClip& SpriteSheetDocument::PreviewClip(SpriteID id) const {
    const auto& clip=m_Draft.Clip(id);SpritePlayback::Duration(clip);
    for(auto frame:clip.Frames){const auto& region=m_Draft.Region(frame.Region);SpriteUV(region.Rect,m_Draft.Sampling.Width,m_Draft.Sampling.Height);SpriteCorners(region.Pivot);}
    return clip;
}
std::vector<std::string> SpriteSheetDocument::References(SpriteID id,bool clip) const {
    std::vector<std::filesystem::path> files;
    for(auto& entry:std::filesystem::recursive_directory_iterator(m_Assets->Root())) {
        if(entry.is_regular_file() && (entry.path().extension()==".hazel" || entry.path().extension()==".hprefab")) files.push_back(entry.path().lexically_relative(m_Assets->Root()));
    }
    std::vector<std::string> uses;
    for(auto& use:FindSpriteAssetUses(m_Assets->Root(),files)) {
        const auto& sheet=clip?use.Animation.Sheet:use.Sprite.Sheet;
        auto usedID=clip?use.Animation.Clip:use.Sprite.Region;
        if(usedID==id && Project::ResolveOwnedAsset(m_Assets->Root(),sheet)==Project::ResolveOwnedAsset(m_Assets->Root(),m_Reference)) uses.push_back(use.Location);
    }
    if(!clip) for(auto& c:m_Draft.Clips) for(auto& f:c.Frames) if(f.Region==id) {uses.push_back("Clip '"+c.Name+"'");break;}
    return uses;
}
}
