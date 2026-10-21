#include "hzpch.h"
#include "Hazel/Audio/AudioPlayback.h"
#include "ProjectAssets.h"
#include "Hazel/Scene/Components.h"
#include "Hazel/UI/RuntimeUIConfig.h"
#include "Hazel/Scene/SceneSerializer.h"
#include "Hazel/Scene/Prefab.h"
#include "Hazel/Core/DocumentSchema.h"
#include "Hazel/Core/FileDocument.h"
#include "Hazel/Project/Project.h"
#include <yaml-cpp/yaml.h>
#include <set>
#include <sstream>
#include <cmath>
#include <map>
#include <fstream>
namespace Hazel {
namespace {
std::string TextureKey(const std::filesystem::path& path,const TextureSpecification& s) {
    return path.generic_u8string()+"|"+std::to_string(s.Width)+"|"+std::to_string(s.Height)+"|"+std::to_string(static_cast<int>(s.Format))+"|"+TextureFilterName(s.MinFilter)+"|"+TextureFilterName(s.MagFilter)+"|"+TextureWrapName(s.WrapS)+"|"+TextureWrapName(s.WrapT)+(s.GenerateMips?"|mips":"|base");
}
std::string ReferenceKey(const std::filesystem::path& path,SpriteID id) {return Project::NormalizeAssetPath(path).lexically_normal().generic_u8string()+"|"+SpriteIDText(id);}
}
ProjectAssets::~ProjectAssets() {
    // Family document/coverage services are CPU-only. Metadata entries alone
    // must not invoke rendering-thread teardown in native authoring/export tools.
    bool usedFaces=!m_Fonts.empty();const std::weak_ptr<Font> empty;
    for(const auto& family:m_Families)for(const auto& face:family.second.Faces)
        usedFaces |= face.Accepted.owner_before(empty) || empty.owner_before(face.Accepted);
    if(usedFaces)Font::ReleaseRoot(m_Root);
}
void ProjectAssets::ReloadFamily(const std::string& key,FamilyEntry& entry) {
    try {
        auto def=key.empty()?DefaultFontFamily():ReadFontFamily(Project::ResolveOwnedAsset(m_Root,std::filesystem::u8path(key)));
        if(!key.empty())for(unsigned i=0;i<4;++i)if(!def.Faces[i].File.empty()) {
            try {
                Font::ValidateFile(Project::ResolveOwnedAsset(m_Root,def.Faces[i].File));
                for(auto& notice:def.Faces[i].Notices)if(!std::filesystem::is_regular_file(Project::ResolveOwnedAsset(m_Root,notice)))throw std::runtime_error("Missing notice: "+notice.generic_u8string());
            }catch(const std::exception& error){throw std::runtime_error(std::string(FontStyleName(FontStyle(i)))+" ["+def.Faces[i].File.generic_u8string()+"]: "+error.what());}
        }
        entry.Accepted=CreateRef<FontFamilyDefinition>(std::move(def));entry.Error.clear();
        for(auto& face:entry.Faces){face.Loaded=false;face.Error.clear();}
    }catch(const std::exception& error){
        if(entry.Error!=error.what())HZ_CORE_WARN("Font family '{}': {}. Accepted family retained, or engine default; Reload to retry.",key,error.what());
        entry.Error=error.what();
    }
}
ProjectAssets::FamilyEntry* ProjectAssets::FamilyEntryFor(const std::filesystem::path& reference) {
    FontSelection{reference}.Validate();const auto key=reference.generic_u8string();
    auto it=m_Families.find(key);
    if(it==m_Families.end()) {
        if(m_Families.size()>=128){if(!m_FontLimitReported){HZ_CORE_WARN("Project font-family reference limit (128); additional families use engine default");m_FontLimitReported=true;}return nullptr;}
        it=m_Families.emplace(key,FamilyEntry{}).first;ReloadFamily(key,it->second);
    }
    return &it->second;
}
Ref<const FontFamilyDefinition> ProjectAssets::Family(const std::filesystem::path& reference) {
    auto* entry=FamilyEntryFor(reference);return entry?entry->Accepted:nullptr;
}
Ref<Font> ProjectAssets::ResolveFamilyFace(const std::string& key,FamilyEntry& entry,FontStyle requested) {
    if(!entry.Accepted)return Font::GetDefault();
    auto selected=entry.Accepted->Select(requested);auto& face=entry.Faces[unsigned(selected)];
    auto accepted=face.Accepted.lock();
    if(!face.Loaded || (!accepted && face.Error.empty())) {
        face.Loaded=true;
        try {
            auto path=key.empty()?DefaultFontFace(selected):Project::ResolveOwnedAsset(m_Root,entry.Accepted->Faces[unsigned(selected)].File);
            accepted=key.empty() && selected==FontStyle::Regular?Font::GetDefault():Font::Load(path,accepted);
            face.Accepted=accepted;face.Error.clear();
        }catch(const std::exception& error){
            if(face.Error!=error.what())HZ_CORE_WARN("Font family '{}' / {}: {}. Keeping accepted face or engine Regular; Reload to retry.",key,FontStyleName(selected),error.what());
            face.Error=error.what();
        }
    }
    return accepted?accepted:Font::GetDefault();
}
Ref<Font> ProjectAssets::FamilyFont(const FontSelection& selection) {
    selection.Validate();auto fallback=Font::GetDefault(); // reserve safe fallback before custom admission
    auto* entry=FamilyEntryFor(selection.Family);
    return entry?ResolveFamilyFace(selection.Family.generic_u8string(),*entry,selection.Style):fallback;
}
FamilyResolution ProjectAssets::FamilyStatus(const FontSelection& selection) {
    auto actual=FamilyFont(selection);FamilyResolution result;result.Actual=actual;result.Requested=selection.Style;result.Face=actual->GetPath();
    auto* entry=FamilyEntryFor(selection.Family);
    if(!entry){result.DefaultFallback=true;result.Error="Project font-family reference limit (128)";return result;}
    result.Error=entry->Error;
    if(!entry->Accepted){result.DefaultFallback=true;return result;}
    result.Selected=entry->Accepted->Select(selection.Style);result.StyleFallback=result.Selected!=selection.Style;
    const auto& face=entry->Faces[unsigned(result.Selected)];
    if(!face.Error.empty()){if(!result.Error.empty())result.Error+="; ";result.Error+=face.Error;}
    result.DefaultFallback=face.Accepted.expired();return result;
}
Ref<Font> ResolveTextFont(TextComponent& text,const Ref<ProjectAssets>& assets) {
    const auto epoch=assets?assets->Epoch():1;
    const bool sameOwner=!text.FontOwner.owner_before(assets) && !assets.owner_before(text.FontOwner);
    if(!text.ResolvedFont || text.ResolvedSelection!=text.Font || text.FontEpoch!=epoch || !sameOwner) {
        text.Font.Validate();
        text.ResolvedFont=assets?assets->FamilyFont(text.Font):text.Font.Family.empty()?(text.Font.Style==FontStyle::Regular?Font::GetDefault():Font::Load(DefaultFontFace(text.Font.Style),text.ResolvedFont)):Font::GetDefault();
        text.ResolvedSelection=text.Font;text.FontEpoch=epoch;text.FontOwner=assets;
    }
    return text.ResolvedFont;
}
void ProjectAssets::ImportFontFile(const std::filesystem::path& source,const std::filesystem::path& destination,bool face) {
    // Native asset import. Never overwrite an existing file; descriptor assignment remains a draft edit.
    auto path=Project::ResolveOwnedAsset(m_Root,destination);
    if(face){Font::ValidateFile(source);FontSelection probe;auto ext=path.extension();if(ext!=".ttf" && ext!=".otf")throw std::runtime_error("Imported face needs .ttf/.otf extension");}
    std::ifstream in(source,std::ios::binary|std::ios::ate);auto size=in?in.tellg():std::streampos(-1);
    const uint64_t cap=face?4*1024*1024:1024*1024;
    if(size<=0 || uint64_t(size)>cap)throw std::runtime_error("Imported face/notice is empty or exceeds its input limit");
    std::string bytes(size_t(size),'\0');in.seekg(0);if(!in.read(bytes.data(),std::streamsize(bytes.size())))throw std::runtime_error("Cannot read imported dependency");
    std::filesystem::create_directories(path.parent_path());
    FileSystem::WriteFileAtomically(path,[&](auto& out){out.write(bytes.data(),std::streamsize(bytes.size()));},WriteMode::CreateNew);
}
void ProjectAssets::ReloadFont(const std::string& key,FontEntry& entry) {
    try {auto previous=entry.Accepted.lock();auto accepted=Font::Load(Project::ResolveOwnedAsset(m_Root,std::filesystem::u8path(key)),previous);entry.Accepted=accepted;entry.Error.clear();}
    catch(const std::exception& error) {
        const std::string diagnostic=error.what();
        if(entry.Error!=diagnostic)HZ_CORE_WARN("Font '{}': {}. Keeping accepted resource, or shipped default; Refresh assets to retry.",key,diagnostic);
        entry.Error=diagnostic;
    }
}
Ref<Font> ProjectAssets::FontAsset(const std::filesystem::path& reference) {
    auto fallback=Font::GetDefault(); // reserve the safe resource before custom admission
    if(reference.empty())return fallback;
    const auto key=reference.generic_u8string();auto it=m_Fonts.find(key);
    if(it==m_Fonts.end()) {
        if(m_Fonts.size()>=256) {
            if(!m_FontLimitReported){HZ_CORE_WARN("Project font reference limit (256) reached; using shipped default for additional references");m_FontLimitReported=true;}
            return fallback;
        }
        it=m_Fonts.emplace(key,FontEntry{}).first;ReloadFont(key,it->second);
    } else if(it->second.Error.empty() && it->second.Accepted.expired())ReloadFont(key,it->second);
    auto accepted=it->second.Accepted.lock();return accepted?accepted:fallback;
}
FontResolution ProjectAssets::FontStatus(const std::filesystem::path& reference) {
    auto actual=FontAsset(reference);
    auto it=m_Fonts.find(reference.generic_u8string());
    return {actual,it!=m_Fonts.end()?it->second.Error:reference.empty()?std::string{}:"Project font reference limit (256)",
        !reference.empty()&&(it==m_Fonts.end()||it->second.Accepted.expired())};
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
        draw->Texture=Texture(sheet->Texture,sheet->Sampling); draw->UV=SpriteUV(region.Rect,sheet->Sampling.Width,sheet->Sampling.Height);draw->Corners=SpriteCorners(region.Pivot);draw->SourceSize={region.Rect.Width,region.Rect.Height};
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
    if(reference.empty() || reference.extension()==".hfont") {
        auto* entry=FamilyEntryFor(reference);if(entry)ReloadFamily(reference.generic_u8string(),*entry);
        ++m_Epoch;return;
    }
    if(reference.extension()==".ttf"||reference.extension()==".otf"){
        auto it=m_Fonts.find(reference.generic_u8string());
        if(it!=m_Fonts.end())ReloadFont(it->first,it->second);
        else FontAsset(reference);
        // A direct face reload invalidates families using it, including cached failures.
        for(auto& pair:m_Families)ReloadFamily(pair.first,pair.second);
        ++m_Epoch;return;
    }
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
    std::vector<std::filesystem::path> paths;for(auto& p:m_Sheets) paths.push_back(std::filesystem::u8path(p.first).lexically_relative(m_Root));
    for(auto& p:paths) {
        try{Reload(p);}catch(const std::exception& e){HZ_CORE_WARN("Sprite asset refresh: {}",e.what());}
    }
    for(auto& entry:m_Fonts)ReloadFont(entry.first,entry.second);
    for(auto& entry:m_Families)ReloadFamily(entry.first,entry.second);
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
        auto path=Project::ResolveOwnedAsset(root,file);auto doc=YAML::Load(FileDocument::Read(path));
        if(file.extension()==".hprefab")Prefab::Load(root,file,true,ResourceLoading::MetadataOnly);
        else {
            auto scene=CreateRef<Scene>();SceneSerializer loader(scene,root,true);
            if(!loader.DeserializeText(FileDocument::Read(path),false,ResourceLoading::MetadataOnly))throw std::runtime_error(file.generic_u8string()+": "+loader.Report().Error);
        }
        if(!doc.IsMap() || !doc["Scene"] || (doc["Entities"] && !doc["Entities"].IsSequence())) throw std::runtime_error("Malformed scene/prefab: "+file.generic_u8string());
        for(auto entity:doc["Entities"]) {
            const auto location=file.generic_u8string()+" entity "+entity["Entity"].as<std::string>();
            if(auto s=entity["SpriteRendererComponent"]) {
                auto source=ReadSpriteSource(s); if(auto r=std::get_if<SpriteReference>(&source)) result.push_back({file,location,*r,{}});
            }
            if(auto b=entity["UIButtonComponent"]){auto c=ReadUIButtonComponent(b);for(auto& source:c.Images)if(auto r=std::get_if<SpriteReference>(&source))result.push_back({file,location+" UI state",*r,{}});}
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
    std::set<std::filesystem::path> auditedFamilies;
    auto family=[&](const std::filesystem::path& reference) {
        auto owned=Project::ResolveOwnedAsset(root,reference);
        if(auditedFamilies.insert(owned).second)for(auto& dependency:AuditFontFamily(root,reference))dependencies.insert(dependency);
    };
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
    for(auto file:files) {
        if(file.extension()==".hsprites")get(file);
        if(file.extension()==".hfont")family(file);
    }
    for(auto use:FindSpriteAssetUses(root,files)) {
        try { if(use.Sprite.Region) get(use.Sprite.Sheet).Region(use.Sprite.Region);if(use.Animation.Clip) get(use.Animation.Sheet).Clip(use.Animation.Clip); }
        catch(const std::exception& e) {throw std::runtime_error(use.Location+": "+e.what());}
    }
    // Whole texture sources are also validated by the same canonical parser/decoder.
    for(auto file:files) if(file.extension()==".hazel" || file.extension()==".hprefab") {
        auto doc=YAML::Load(FileDocument::Read(Project::ResolveOwnedAsset(root,file)));
        DocumentSchema::Scene(doc,file.extension()==".hprefab");
        for(auto e:doc["Entities"]) {
            if(auto n=e["UINineSliceComponent"]) {
                const auto borders=ReadUINineSliceComponent(n).Borders;
                auto validate=[&](const SpriteSource& source){glm::vec2 size{};
                    if(auto image=std::get_if<SpriteReference>(&source);image&&image->Region){const auto rect=get(image->Sheet).Region(image->Region).Rect;size={rect.Width,rect.Height};}
                    if(auto image=std::get_if<TextureSpriteSource>(&source);image&&!image->Texture.empty()){auto pixels=Texture2D::ReadImage(Project::ResolveOwnedAsset(root,image->Texture));size={pixels.Width,pixels.Height};}
                    if(size.x<=0||size.y<=0||borders.x+borders.z>size.x||borders.y+borders.w>size.y)throw std::runtime_error(file.generic_u8string()+": nine-slice borders exceed source image");
                };
                if(!e["SpriteRendererComponent"])throw std::runtime_error("Nine-slice requires an image");
                validate(ReadSpriteSource(e["SpriteRendererComponent"]));
                if(auto button=e["UIButtonComponent"])for(auto& source:ReadUIButtonComponent(button).Images)if(source.index()!=0)validate(source);
            }
            if(auto n=e["TextComponent"])if(n["FontFamily"] && !n["FontFamily"].as<std::string>().empty()) {
                auto reference=std::filesystem::u8path(n["FontFamily"].as<std::string>());
                try{family(reference);}
                catch(const std::exception& error){throw std::runtime_error(file.generic_u8string()+" entity "+e["Entity"].as<std::string>()+" family "+reference.generic_u8string()+": "+error.what());}
            }
            if(auto n=e["UIButtonComponent"]){for(auto& source:ReadUIButtonComponent(n).Images)if(auto t=std::get_if<TextureSpriteSource>(&source)){if(!t->Texture.empty()){auto path=Project::ResolveOwnedAsset(root,t->Texture);Texture2D::ReadImage(path);dependencies.insert(path.lexically_relative(std::filesystem::weakly_canonical(root)));}}}
        }
        for(auto e:doc["Entities"]) if(auto n=e["AudioSourceComponent"]) {
            auto clip=std::filesystem::u8path(n["Clip"].as<std::string>());
            if(!clip.empty()) {
                auto path=Project::ResolveOwnedAsset(root,clip);AudioPlayback::ValidateClip(path);
                dependencies.insert(path.lexically_relative(std::filesystem::weakly_canonical(root)));
            }
        }
        for(auto e:doc["Entities"]) if(auto n=e["SpriteRendererComponent"]) {
            auto source=ReadSpriteSource(n); if(auto t=std::get_if<TextureSpriteSource>(&source)) {
                auto path=Project::ResolveOwnedAsset(root,t->Texture);Texture2D::ReadImage(path);dependencies.insert(path.lexically_relative(std::filesystem::weakly_canonical(root)));
            }
        }
    }
    return {dependencies.begin(),dependencies.end()};
}
}
