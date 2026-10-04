#include "hzpch.h"
#include "ProjectAssets.h"
#include "Hazel/Project/Project.h"
#include <yaml-cpp/yaml.h>
#include <set>
#include <sstream>
#include <cmath>
#include <map>
namespace Hazel {
namespace {
std::string TextureKey(const std::filesystem::path& path,const TextureSpecification& s) {
    return path.generic_u8string()+"|"+std::to_string(s.Width)+"|"+std::to_string(s.Height)+"|"+std::to_string(static_cast<int>(s.Format))+"|"+TextureFilterName(s.MinFilter)+"|"+TextureFilterName(s.MagFilter)+"|"+TextureWrapName(s.WrapS)+"|"+TextureWrapName(s.WrapT)+(s.GenerateMips?"|mips":"|base");
}
std::string ReferenceKey(const std::filesystem::path& path,SpriteID id) {return Project::NormalizeAssetPath(path).lexically_normal().generic_u8string()+"|"+SpriteIDText(id);}
}
Ref<const SpriteSheetDefinition> ProjectAssets::Sheet(const std::filesystem::path& reference) {
    auto path=Project::ResolveOwnedAsset(m_Root,reference); if(path.extension()!=".hsprites") throw std::runtime_error("Expected .hsprites asset");
    auto key=path.generic_u8string(); auto it=m_Sheets.find(key);
    if(it!=m_Sheets.end()) {if(!it->second.Error.empty()) throw std::runtime_error(it->second.Error);return it->second.Data;}
    try { auto s=CreateRef<SpriteSheetDefinition>(ReadSpriteSheet(path)); m_Sheets.emplace(key,SheetEntry{s,{}}); return s; }
    catch(const std::exception& e) {m_Sheets[key].Error=e.what();throw;}
}
Ref<Texture2D> ProjectAssets::Texture(const std::filesystem::path& reference,const TextureSpecification& spec,bool owned) {
    auto path=owned?Project::ResolveOwnedAsset(m_Root,reference):Project::ResolveAssetPath(m_Root,reference);
    path=std::filesystem::weakly_canonical(path); auto key=TextureKey(path,spec); auto it=m_Textures.find(key);
    if(it!=m_Textures.end()) return it->second;
    auto texture=Texture2D::Create(path.generic_u8string(),spec); m_Textures.emplace(key,texture);return texture;
}
Ref<const ResolvedSprite> ProjectAssets::Resolve(const SpriteSource& source) {
    auto draw=CreateRef<ResolvedSprite>();
    if(auto t=std::get_if<TextureSpriteSource>(&source)) {
        if(!std::isfinite(t->TilingFactor) || t->TilingFactor<0) throw std::runtime_error("Invalid sprite tiling");
        if(t->Texture.empty() && !t->Resource)return draw;
        draw->Texture=t->Resource?t->Resource:Texture(t->Texture,TextureSpecification::FileDefaults(),false); draw->TilingFactor=t->TilingFactor; return draw;
    }
    if(auto r=std::get_if<SpriteReference>(&source)) {
        if(!r->Region && r->Sheet.empty())return draw;
        auto key=ReferenceKey(r->Sheet,r->Region);auto it=m_Sprites.find(key);if(it!=m_Sprites.end()) return it->second;
        auto sheet=Sheet(r->Sheet);auto region=sheet->Region(r->Region);
        draw->Texture=Texture(sheet->Texture,sheet->Sampling); draw->UV=SpriteUV(region.Rect,sheet->Sampling.Width,sheet->Sampling.Height);draw->Corners=SpriteCorners(region.Pivot);
        m_Sprites.emplace(key,draw);
    }
    return draw;
}
Ref<const ResolvedClip> ProjectAssets::Clip(const AnimationReference& reference) {
    auto key=ReferenceKey(reference.Sheet,reference.Clip);auto it=m_Clips.find(key);if(it!=m_Clips.end()) return it->second;
    auto sheet=Sheet(reference.Sheet);const auto& definition=sheet->Clip(reference.Clip);SpritePlayback::Duration(definition);
    auto result=CreateRef<ResolvedClip>();result->Sheet=sheet;result->ID=reference.Clip;
    for(auto frame:definition.Frames) result->Frames.push_back(Resolve(SpriteReference{reference.Sheet,frame.Region}));
    m_Clips.emplace(key,result);return result;
}
void ProjectAssets::Reload(const std::filesystem::path& reference) {
    auto path=Project::ResolveOwnedAsset(m_Root,reference);const auto key=path.generic_u8string();
    // Drop derived resolutions and image resources only at this deliberate authoring boundary.
    ++m_Epoch;m_Sprites.clear();m_Clips.clear();m_Textures.clear();
    try {
        auto candidate=CreateRef<SpriteSheetDefinition>(ReadSpriteSheet(path));
        auto image=Texture2D::ReadImage(Project::ResolveOwnedAsset(m_Root,candidate->Texture),candidate->Sampling.Format);
        if(image.Width!=candidate->Sampling.Width || image.Height!=candidate->Sampling.Height) throw std::runtime_error("Texture dimensions changed: "+candidate->Texture.generic_u8string());
        m_Sheets[key]={candidate,{}};
    } catch(const std::exception& e) {m_Sheets[key].Error=e.what();throw;}
}
void ProjectAssets::Refresh() {
    std::vector<std::filesystem::path> paths;for(auto& p:m_Sheets) paths.push_back(std::filesystem::path(p.first).lexically_relative(m_Root));
    for(auto& p:paths) {
        try{Reload(p);}catch(const std::exception& e){HZ_CORE_WARN("Sprite asset refresh: {}",e.what());}
    }
    ++m_Epoch;m_Textures.clear();m_Sprites.clear();m_Clips.clear();
}
void ProjectAssets::ImportTexture(const std::filesystem::path& source,const std::filesystem::path& destination) {
    Texture2D::ReadImage(source); // Validate through the shared engine decoder before copying.
    auto path=Project::ResolveOwnedAsset(m_Root,destination);
    ScopedBuffer bytes(FileSystem::ReadFileBinary(source));if(!bytes)throw std::runtime_error("Cannot read imported texture");
    std::filesystem::create_directories(path.parent_path());
    FileSystem::WriteFileAtomically(path,[&](std::ostream& out){out.write(reinterpret_cast<const char*>(bytes.Data()),static_cast<std::streamsize>(bytes.Size()));},WriteMode::CreateNew);
}
std::vector<SpriteAssetUse> FindSpriteAssetUses(const std::filesystem::path& root,const std::vector<std::filesystem::path>& files) {
    std::vector<SpriteAssetUse> result;
    for(auto file:files) {
        if(file.extension()!=".hazel" && file.extension()!=".hprefab") continue;
        auto path=Project::ResolveOwnedAsset(root,file);auto doc=YAML::LoadFile(path.u8string());
        if(!doc.IsMap() || !doc["Scene"] || (doc["Entities"] && !doc["Entities"].IsSequence())) throw std::runtime_error("Malformed scene/prefab: "+file.generic_u8string());
        for(auto entity:doc["Entities"]) {
            const auto location=file.generic_u8string()+" entity "+entity["Entity"].as<std::string>();
            if(auto s=entity["SpriteRendererComponent"]) {
                auto source=ReadSpriteSource(s); if(auto r=std::get_if<SpriteReference>(&source)) result.push_back({file,location,*r,{}});
            }
            if(auto a=entity["SpriteAnimationComponent"]) {
                if(!entity["SpriteRendererComponent"])throw std::runtime_error(location+": Animation requires Sprite Renderer");
                result.push_back({file,location+" animation",{},ReadSpriteAnimationSettings(a).DefaultClip});
            }
            for(auto f:entity["ScriptComponent"]["ScriptFields"]) {
                auto type=f["Type"].as<std::string>();auto at=location+" field "+f["Name"].as<std::string>();
                if(type=="Sprite" && f["Data"]) result.push_back({file,at,ReadSpriteReference(f["Data"]),{}});
                if(type=="SpriteAnimation" && f["Data"]) result.push_back({file,at,{},ReadAnimationReference(f["Data"])});
            }
        }
    } return result;
}
std::vector<std::filesystem::path> AuditSpriteAssets(const std::filesystem::path& root,const std::vector<std::filesystem::path>& files) {
    std::map<std::string,SpriteSheetDefinition> sheets;std::set<std::filesystem::path> dependencies;
    auto get=[&](const std::filesystem::path& ref)->const SpriteSheetDefinition& {
        auto path=Project::ResolveOwnedAsset(root,ref);auto key=path.generic_u8string();auto it=sheets.find(key);
        if(it==sheets.end()) {
            auto s=ReadSpriteSheet(path);s.Validate(true);
            auto image=Texture2D::ReadImage(Project::ResolveOwnedAsset(root,s.Texture),s.Sampling.Format);
            if(image.Width!=s.Sampling.Width || image.Height!=s.Sampling.Height) throw std::runtime_error("Sheet texture dimensions changed: "+ref.generic_u8string());
            dependencies.insert(path.lexically_relative(std::filesystem::weakly_canonical(root)));dependencies.insert(s.Texture);
            it=sheets.emplace(key,std::move(s)).first;
        } return it->second;
    };
    for(auto file:files) if(file.extension()==".hsprites") get(file);
    for(auto use:FindSpriteAssetUses(root,files)) {
        try { if(use.Sprite.Region) get(use.Sprite.Sheet).Region(use.Sprite.Region);if(use.Animation.Clip) get(use.Animation.Sheet).Clip(use.Animation.Clip); }
        catch(const std::exception& e) {throw std::runtime_error(use.Location+": "+e.what());}
    }
    // Whole texture sources are also validated by the same canonical parser/decoder.
    for(auto file:files) if(file.extension()==".hazel" || file.extension()==".hprefab") {
        auto doc=YAML::LoadFile(Project::ResolveOwnedAsset(root,file).u8string());
        for(auto e:doc["Entities"]) if(auto n=e["SpriteRendererComponent"]) {
            auto source=ReadSpriteSource(n); if(auto t=std::get_if<TextureSpriteSource>(&source)) {
                auto path=Project::ResolveOwnedAsset(root,t->Texture);Texture2D::ReadImage(path);dependencies.insert(path.lexically_relative(std::filesystem::weakly_canonical(root)));
            }
        }
    }
    return {dependencies.begin(),dependencies.end()};
}
}
