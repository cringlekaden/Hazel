#include "hzpch.h"
#include "SpriteSheet.h"
#include "Hazel/Core/DocumentSchema.h"
#include "Hazel/Core/FileDocument.h"
#include <yaml-cpp/yaml.h>
#include "Hazel/Core/UUID.h"
#include "Hazel/Project/Project.h"
#include <set>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <limits>
namespace Hazel {
namespace {
void Keys(const YAML::Node& n,std::initializer_list<const char*> allowed) {
    if(!n.IsMap()) throw std::runtime_error("Expected metadata map");
    std::set<std::string> seen;
    for(auto kv:n) {
        auto name=kv.first.as<std::string>();
        if(!seen.insert(name).second) throw std::runtime_error("Duplicate metadata key: "+name);
        bool ok=false; for(auto key:allowed) if(name==key) ok=true;
        if(!ok) throw std::runtime_error("Unknown metadata key: "+name);
    }
}
uint32_t Number(const YAML::Node& n) {
    auto v=n.as<int64_t>(); if(v<0 || v>std::numeric_limits<uint32_t>::max()) throw std::runtime_error("Pixel dimensions must be unsigned integers"); return static_cast<uint32_t>(v);
}
void Label(const std::string& s) { if(s.empty() || s.find_first_not_of(" \t\r\n")==std::string::npos || s.front()==' ' || s.back()==' ') throw std::runtime_error("Use a nonempty name without leading/trailing spaces"); }
void ReferencePath(const std::filesystem::path& path) {
    auto p=Project::NormalizeAssetPath(path); auto s=p.generic_u8string();
    if(p.empty() || p.has_root_path() || s.find(':')!=std::string::npos || s.find('\0')!=std::string::npos || s.find_first_of("\r\n")!=std::string::npos) throw std::runtime_error("Use a project-relative asset path: "+s);
    for(auto part:p) if(part=="..") throw std::runtime_error("Parent traversal is not permitted: "+s);
}
void ValidateClip(const SpriteClip& clip) {
    if(clip.Frames.empty()) throw std::runtime_error("Clip '"+clip.Name+"' has no frames");
    double total=0;
    for(auto f:clip.Frames) {
        if(!f.Region || !std::isfinite(f.Duration) || f.Duration<=0) throw std::runtime_error("Clip frames need region IDs and positive finite durations");
        total+=f.Duration;
    }
    if(!std::isfinite(total) || total<=0) throw std::runtime_error("Clip duration overflow");
}
}
std::string SpriteIDText(SpriteID id) { std::ostringstream out; out<<std::hex<<std::setw(16)<<std::setfill('0')<<id; return out.str(); }
SpriteID ParseSpriteID(const std::string& text) {
    if(text.size()!=16 || text.find_first_not_of("0123456789abcdefABCDEF")!=std::string::npos) throw std::runtime_error("Expected 16-digit sprite ID");
    auto id=std::stoull(text,nullptr,16); if(!id) throw std::runtime_error("Sprite ID zero is reserved"); return id;
}
SpriteSheetDefinition::SpriteSheetDefinition() {
    Sampling=TextureSpecification::FileDefaults(); Sampling.Format=ImageFormat::RGBA8;
    Sampling.MinFilter=Sampling.MagFilter=TextureFilter::Nearest; Sampling.WrapS=Sampling.WrapT=TextureWrap::ClampToEdge; Sampling.GenerateMips=false;
}
const SpriteRegion& SpriteSheetDefinition::Region(SpriteID id) const {
    for(const auto& r:Regions) if(r.ID==id) return r;
    throw std::runtime_error("Missing/retired sprite region "+SpriteIDText(id));
}
const SpriteClip& SpriteSheetDefinition::Clip(SpriteID id) const {
    for(const auto& c:Clips) if(c.ID==id) return c;
    throw std::runtime_error("Missing/retired animation clip "+SpriteIDText(id));
}
SpriteID SpriteSheetDefinition::NewID(bool clip) const {
    std::set<SpriteID> used;
    if(clip) { for(auto& c:Clips) used.insert(c.ID); used.insert(RetiredClipIDs.begin(),RetiredClipIDs.end()); }
    else { for(auto& r:Regions) used.insert(r.ID); used.insert(RetiredRegionIDs.begin(),RetiredRegionIDs.end()); }
    SpriteID id; do { id=UUID(); } while(!id || used.count(id)); return id;
}
void SpriteSheetDefinition::Validate(bool playable) const {
    ReferencePath(Texture); Sampling.Validate(true);
    if(Sampling.Format!=ImageFormat::R8 && Sampling.Format!=ImageFormat::RGB8 && Sampling.Format!=ImageFormat::RGBA8)throw std::runtime_error("Sheets require an explicit R8/RGB8/RGBA8 format");
    if(!Sampling.Width || !Sampling.Height) throw std::runtime_error("Sheet requires positive recorded texture dimensions");
    std::set<SpriteID> ids; std::set<std::string> names;
    for(auto id:RetiredRegionIDs) if(!id || !ids.insert(id).second) throw std::runtime_error("Invalid/duplicate retired region ID");
    for(const auto& r:Regions) {
        Label(r.Name);
        if(!r.ID || !ids.insert(r.ID).second || !names.insert(r.Name).second) throw std::runtime_error("Duplicate/retired region ID or name: "+r.Name);
        if(!r.Rect.Width || !r.Rect.Height || uint64_t(r.Rect.X)+r.Rect.Width>Sampling.Width || uint64_t(r.Rect.Y)+r.Rect.Height>Sampling.Height)
            throw std::runtime_error("Region rectangle outside texture: "+r.Name);
        if(!std::isfinite(r.Pivot.x) || !std::isfinite(r.Pivot.y) || r.Pivot.x<0 || r.Pivot.x>1 || r.Pivot.y<0 || r.Pivot.y>1) throw std::runtime_error("Pivot must be finite in [0,1]: "+r.Name);
    }
    ids.clear(); names.clear();
    for(auto id:RetiredClipIDs) if(!id || !ids.insert(id).second) throw std::runtime_error("Invalid/duplicate retired clip ID");
    for(const auto& c:Clips) {
        Label(c.Name);
        if(!c.ID || !ids.insert(c.ID).second || !names.insert(c.Name).second) throw std::runtime_error("Duplicate/retired clip ID or name: "+c.Name);
        for(auto f:c.Frames) if(!f.Region || !std::isfinite(f.Duration) || f.Duration<=0) throw std::runtime_error("Invalid frame in clip: "+c.Name);
        if(!c.Frames.empty()) ValidateClip(c);
        if(playable) { ValidateClip(c); for(auto f:c.Frames) Region(f.Region); }
    }
}
std::array<glm::vec2,4> SpriteUV(const PixelRect& r,uint32_t w,uint32_t h) {
    if(!w || !h || !r.Width || !r.Height || uint64_t(r.X)+r.Width>w || uint64_t(r.Y)+r.Height>h) throw std::runtime_error("Invalid UV rectangle");
    float x0=(r.X+.5f)/w,x1=(r.X+r.Width-.5f)/w,y0=1-(r.Y+r.Height-.5f)/h,y1=1-(r.Y+.5f)/h;
    return {{{x0,y0},{x1,y0},{x1,y1},{x0,y1}}};
}
std::array<glm::vec2,4> SpriteCorners(glm::vec2 p) {
    if(!std::isfinite(p.x) || !std::isfinite(p.y) || p.x<0 || p.x>1 || p.y<0 || p.y>1)throw std::runtime_error("Pivot must be finite in [0,1]");
    return {{{-p.x,p.y-1},{1-p.x,p.y-1},{1-p.x,p.y},{-p.x,p.y}}};
}
std::vector<SpriteRegion> GenerateGridPreview(const SpriteSheetDefinition& sheet,const GridSliceOptions& g) {
    if(!g.CellWidth || !g.CellHeight || uint64_t(g.Left)+g.Right>=sheet.Sampling.Width || uint64_t(g.Top)+g.Bottom>=sheet.Sampling.Height) throw std::runtime_error("Invalid grid cells/margins");
    Label(g.Prefix);
    uint64_t w=sheet.Sampling.Width-g.Left-g.Right,h=sheet.Sampling.Height-g.Top-g.Bottom;
    auto cols=w<g.CellWidth?0:(w+g.SpacingX)/(uint64_t(g.CellWidth)+g.SpacingX);
    auto rows=h<g.CellHeight?0:(h+g.SpacingY)/(uint64_t(g.CellHeight)+g.SpacingY);
    if((g.Columns && g.Columns>cols) || (g.Rows && g.Rows>rows)) throw std::runtime_error("Explicit grid count exceeds full cells");
    if(g.Columns) cols=g.Columns; if(g.Rows) rows=g.Rows;
    if(cols*rows>65536) throw std::runtime_error("Grid exceeds 65536 regions");
    std::set<std::string> names; for(auto r:sheet.Regions) names.insert(r.Name);
    std::vector<SpriteRegion> result;
    for(uint64_t y=0;y<rows;++y) for(uint64_t x=0;x<cols;++x) {
        SpriteRegion r; r.Rect={static_cast<uint32_t>(g.Left+x*(uint64_t(g.CellWidth)+g.SpacingX)),static_cast<uint32_t>(g.Top+y*(uint64_t(g.CellHeight)+g.SpacingY)),g.CellWidth,g.CellHeight};
        std::ostringstream n; n<<g.Prefix<<'_'<<std::setw(3)<<std::setfill('0')<<(uint64_t(g.Start)+result.size()); auto base=n.str(); r.Name=base;
        for(unsigned suffix=1;names.count(r.Name);++suffix) r.Name=base+"_"+std::to_string(suffix);
        names.insert(r.Name); result.push_back(r);
    }
    return result;
}
size_t AddGridRegions(SpriteSheetDefinition& sheet,const std::vector<SpriteRegion>& regions) {
    size_t added=0; for(auto r:regions) {
        if(std::any_of(sheet.Regions.begin(),sheet.Regions.end(),[&](auto& old){return old.Rect==r.Rect;})) continue;
        r.ID=sheet.NewID(); sheet.Regions.push_back(r); ++added;
    } return added;
}
double SpritePlayback::Duration(const SpriteClip& clip) { ValidateClip(clip); double total=0; for(auto f:clip.Frames) total+=f.Duration; return total; }
void SpritePlayback::Scrub(const SpriteClip& clip,double time) {
    const auto total=Duration(clip);
    if(!std::isfinite(time)) throw std::runtime_error("Animation time must be finite");
    Time=std::clamp(time,0.0,total); Frame=clip.Frames.size()-1; double end=0;
    for(size_t i=0;i<clip.Frames.size();++i) { end+=clip.Frames[i].Duration; if(Time<end) {Frame=i;break;} }
}
void SpritePlayback::Advance(const SpriteClip& clip,double dt,double speed) {
    if(!std::isfinite(dt) || dt<0 || !std::isfinite(speed) || speed<0) throw std::runtime_error("Animation timestep/speed must be finite and nonnegative");
    if(!Playing || speed==0 || dt==0) return;
    const auto total=Duration(clip);
    // Reduce before multiplying; bounded work even for huge dt/speed or tiny frame durations.
    double advance;
    if(clip.Loop) {
        int exponent=0; const double fraction=std::frexp(speed,&exponent);
        // Portable even where long double == double (MSVC): no overflowing product.
        advance=dt*fraction;
        if(exponent<0) advance=std::ldexp(advance,exponent);
        advance=std::fmod(advance,total);
        while(exponent-->0) advance=advance>=total-advance ? advance-(total-advance) : advance+advance;
        const double time=std::fmod(Time,total);
        Scrub(clip,time>=total-advance ? time-(total-advance) : time+advance);
    } else {
        if(dt>=(total-Time)/speed) { Scrub(clip,total); Playing=false; Finished=true; }
        else Scrub(clip,Time+dt*speed);
    }
}
SpriteReference ReadSpriteReference(const YAML::Node& n) {
    if(!n || n.IsNull()) return {};
    Keys(n,{"Sheet","RegionID"}); auto path=Project::NormalizeAssetPath(std::filesystem::u8path(n["Sheet"].as<std::string>()));
    ReferencePath(path); if(path.extension()!=".hsprites") throw std::runtime_error("Sprite requires a .hsprites asset");
    return {path,ParseSpriteID(n["RegionID"].as<std::string>())};
}
AnimationReference ReadAnimationReference(const YAML::Node& n) {
    if(!n || n.IsNull()) return {};
    Keys(n,{"Sheet","ClipID"}); auto path=Project::NormalizeAssetPath(std::filesystem::u8path(n["Sheet"].as<std::string>()));
    ReferencePath(path); if(path.extension()!=".hsprites") throw std::runtime_error("Animation requires a .hsprites asset");
    return {path,ParseSpriteID(n["ClipID"].as<std::string>())};
}
void WriteSpriteReference(YAML::Emitter& out,const SpriteReference& r) {
    ReferencePath(r.Sheet);if(!r.Region || r.Sheet.extension()!=".hsprites")throw std::runtime_error("Invalid sprite reference");
    out<<YAML::BeginMap<<YAML::Key<<"Sheet"<<YAML::Value<<r.Sheet.generic_u8string()<<YAML::Key<<"RegionID"<<YAML::Value<<YAML::DoubleQuoted<<SpriteIDText(r.Region)<<YAML::EndMap;
}
void WriteAnimationReference(YAML::Emitter& out,const AnimationReference& r) {
    ReferencePath(r.Sheet);if(!r.Clip || r.Sheet.extension()!=".hsprites")throw std::runtime_error("Invalid animation reference");
    out<<YAML::BeginMap<<YAML::Key<<"Sheet"<<YAML::Value<<r.Sheet.generic_u8string()<<YAML::Key<<"ClipID"<<YAML::Value<<YAML::DoubleQuoted<<SpriteIDText(r.Clip)<<YAML::EndMap;
}
SpriteAnimationSettings ReadSpriteAnimationSettings(const YAML::Node& n) {
    Keys(n,{"DefaultClip","Autoplay","Speed"});SpriteAnimationSettings result;
    result.DefaultClip=ReadAnimationReference(n["DefaultClip"]);
    if(n["Autoplay"])result.Autoplay=n["Autoplay"].as<bool>();
    if(n["Speed"])result.Speed=n["Speed"].as<double>();
    if(!std::isfinite(result.Speed) || result.Speed<0)throw std::runtime_error("Animation speed must be finite and nonnegative");
    return result;
}
void WriteSpriteAnimationSettings(YAML::Emitter& out,const SpriteAnimationSettings& settings) {
    if(!std::isfinite(settings.Speed) || settings.Speed<0)throw std::runtime_error("Animation speed must be finite and nonnegative");
    out<<YAML::BeginMap<<YAML::Key<<"DefaultClip"<<YAML::Value;
    if(settings.DefaultClip.Clip)WriteAnimationReference(out,settings.DefaultClip);else out<<YAML::Null;
    out<<YAML::Key<<"Autoplay"<<YAML::Value<<settings.Autoplay<<YAML::Key<<"Speed"<<YAML::Value<<settings.Speed<<YAML::EndMap;
}
SpriteSource ReadSpriteSource(const YAML::Node& component) {
    if(!component["Source"]) {
        if(component["TexturePath"]) return TextureSpriteSource{Project::NormalizeAssetPath(std::filesystem::u8path(component["TexturePath"].as<std::string>())),component["TilingFactor"]?component["TilingFactor"].as<float>():1,{}};
        return std::monostate{};
    }
    if(component["TexturePath"] || component["TilingFactor"]) throw std::runtime_error("Ambiguous legacy/canonical sprite source");
    auto s=component["Source"]; auto type=s["Type"].as<std::string>();
    if(type=="None") {Keys(s,{"Type"});return std::monostate{};}
    if(type=="Texture") {
        Keys(s,{"Type","Texture","TilingFactor"});
        auto path=Project::NormalizeAssetPath(std::filesystem::u8path(s["Texture"].as<std::string>()));
        float tiling=s["TilingFactor"]?s["TilingFactor"].as<float>():1;
        if(path.empty() || !std::isfinite(tiling) || tiling<0) throw std::runtime_error("Invalid whole-texture source/tiling");
        return TextureSpriteSource{path,tiling,{}};
    }
    if(type=="Region") {
        Keys(s,{"Type","Sheet","RegionID"});
        YAML::Node ref; ref["Sheet"]=s["Sheet"]; ref["RegionID"]=s["RegionID"]; return ReadSpriteReference(ref);
    }
    throw std::runtime_error("Unknown sprite source type: "+type);
}
void WriteSpriteSource(YAML::Emitter& out,const SpriteSource& source) {
    out<<YAML::Key<<"Source"<<YAML::Value<<YAML::BeginMap<<YAML::Key<<"Type"<<YAML::Value;
    if(auto t=std::get_if<TextureSpriteSource>(&source)) {
        if(!std::isfinite(t->TilingFactor) || t->TilingFactor<0)throw std::runtime_error("Invalid sprite tiling");
        if(t->Texture.empty() && t->Resource) throw std::runtime_error("A generated texture has no serializable asset path");
        if(t->Texture.empty())out<<"None";
        else out<<"Texture"<<YAML::Key<<"Texture"<<YAML::Value<<t->Texture.generic_u8string()<<YAML::Key<<"TilingFactor"<<YAML::Value<<t->TilingFactor;
    } else if(auto r=std::get_if<SpriteReference>(&source);r && r->Region) {
        ReferencePath(r->Sheet);if(r->Sheet.extension()!=".hsprites")throw std::runtime_error("Sprite requires a .hsprites asset");
        out<<"Region"<<YAML::Key<<"Sheet"<<YAML::Value<<r->Sheet.generic_u8string()<<YAML::Key<<"RegionID"<<YAML::Value<<YAML::DoubleQuoted<<SpriteIDText(r->Region);
    } else out<<"None";
    out<<YAML::EndMap;
}
SpriteSheetDefinition ReadSpriteSheet(const std::filesystem::path& file) {
    const auto text=FileDocument::Read(file);
    try{return ReadSpriteSheetText(text);}catch(const std::exception& e){throw std::runtime_error(file.generic_u8string()+": "+e.what());}
}
SpriteSheetDefinition ReadSpriteSheetText(const std::string& text, bool* usesDefaults) {
    try {
        auto root=YAML::Load(text); DocumentSchema::Structure(root); Keys(root,{"SpriteSheet"}); auto n=root["SpriteSheet"];
        Keys(n,{"Version","Texture","TextureSize","Sampling","Filter","Regions","RetiredRegionIDs","Clips","RetiredClipIDs"});
        if(n["Version"].as<int>()!=1) throw std::runtime_error("Unsupported SpriteSheet Version (expected 1)");
        SpriteSheetDefinition result; result.Texture=Project::NormalizeAssetPath(std::filesystem::u8path(n["Texture"].as<std::string>()));
        auto size=n["TextureSize"]; if(!size.IsSequence() || size.size()!=2) throw std::runtime_error("TextureSize must have two integers");
        result.Sampling.Width=Number(size[0]); result.Sampling.Height=Number(size[1]);
        if(n["Filter"]) { if(n["Sampling"]) throw std::runtime_error("Use either legacy Filter or Sampling"); result.Sampling.MinFilter=result.Sampling.MagFilter=ParseTextureFilter(n["Filter"].as<std::string>()); }
        if(auto p=n["Sampling"]) {
            Keys(p,{"MinFilter","MagFilter","WrapS","WrapT","GenerateMips","Format"});
            if(p["MinFilter"]) result.Sampling.MinFilter=ParseTextureFilter(p["MinFilter"].as<std::string>());
            if(p["MagFilter"]) result.Sampling.MagFilter=ParseTextureFilter(p["MagFilter"].as<std::string>());
            if(p["WrapS"]) result.Sampling.WrapS=ParseTextureWrap(p["WrapS"].as<std::string>());
            if(p["WrapT"]) result.Sampling.WrapT=ParseTextureWrap(p["WrapT"].as<std::string>());
            if(p["GenerateMips"]) result.Sampling.GenerateMips=p["GenerateMips"].as<bool>();
            if(p["Format"]) {
                auto f=p["Format"].as<std::string>();
                if(f=="RGBA8") result.Sampling.Format=ImageFormat::RGBA8;
                else if(f=="RGB8") result.Sampling.Format=ImageFormat::RGB8;
                else if(f=="R8") result.Sampling.Format=ImageFormat::R8;
                else throw std::runtime_error("Sheet format must be R8/RGB8/RGBA8 linear UNORM");
            }
        }
        if(auto rs=n["Regions"]) {
            if(!rs.IsSequence()) throw std::runtime_error("Regions must be a sequence");
            for(auto r:rs) {
                Keys(r,{"ID","Name","Rect","Pivot"}); SpriteRegion region;
                region.ID=ParseSpriteID(r["ID"].as<std::string>()); region.Name=r["Name"].as<std::string>();
                auto rect=r["Rect"]; if(!rect.IsSequence() || rect.size()!=4) throw std::runtime_error("Rect requires x,y,width,height");
                region.Rect={Number(rect[0]),Number(rect[1]),Number(rect[2]),Number(rect[3])};
                if(auto p=r["Pivot"]) {if(!p.IsSequence() || p.size()!=2) throw std::runtime_error("Pivot requires two values");region.Pivot={p[0].as<float>(),p[1].as<float>()};}
                result.Regions.push_back(region);
            }
        }
        if(auto cs=n["Clips"]) {
            if(!cs.IsSequence()) throw std::runtime_error("Clips must be a sequence");
            for(auto c:cs) {
                Keys(c,{"ID","Name","Loop","Frames"}); SpriteClip clip;
                clip.ID=ParseSpriteID(c["ID"].as<std::string>()); clip.Name=c["Name"].as<std::string>(); if(c["Loop"]) clip.Loop=c["Loop"].as<bool>();
                if(auto fs=c["Frames"]) {
                    if(!fs.IsSequence()) throw std::runtime_error("Frames must be a sequence");
                    for(auto f:fs) {Keys(f,{"RegionID","Duration"});clip.Frames.push_back({ParseSpriteID(f["RegionID"].as<std::string>()),f["Duration"].as<double>()});}
                }
                result.Clips.push_back(clip);
            }
        }
        if(n["RetiredRegionIDs"] && !n["RetiredRegionIDs"].IsSequence())throw std::runtime_error("RetiredRegionIDs must be a sequence");
        if(n["RetiredClipIDs"] && !n["RetiredClipIDs"].IsSequence())throw std::runtime_error("RetiredClipIDs must be a sequence");
        for(auto r:n["RetiredRegionIDs"]) result.RetiredRegionIDs.push_back(ParseSpriteID(r.as<std::string>()));
        for(auto c:n["RetiredClipIDs"]) result.RetiredClipIDs.push_back(ParseSpriteID(c.as<std::string>()));
        result.Validate();
        if(usesDefaults) {
            bool defaults=bool(n["Filter"]) || !n["Sampling"];
            for(const char* key:{"MinFilter","MagFilter","WrapS","WrapT","GenerateMips","Format"})
                defaults=defaults || !n["Sampling"] || !n["Sampling"][key];
            for(const char* key:{"Regions","Clips","RetiredRegionIDs","RetiredClipIDs"})defaults=defaults || !n[key];
            for(auto region:n["Regions"])defaults=defaults || !region["Pivot"];
            for(auto clip:n["Clips"])defaults=defaults || !clip["Loop"] || !clip["Frames"];
            *usesDefaults=defaults;
        }
        return result;
    } catch(const std::exception& e) {throw std::runtime_error(std::string("Sprite-sheet metadata: ")+e.what());}
}
std::string WriteSpriteSheetText(const SpriteSheetDefinition& s) {
    s.Validate(); YAML::Emitter out; out.SetFloatPrecision(9); out.SetDoublePrecision(17);
    out<<YAML::BeginMap<<YAML::Key<<"SpriteSheet"<<YAML::Value<<YAML::BeginMap<<YAML::Key<<"Version"<<YAML::Value<<1;
    out<<YAML::Key<<"Texture"<<YAML::Value<<s.Texture.generic_u8string()<<YAML::Key<<"TextureSize"<<YAML::Value<<YAML::Flow<<YAML::BeginSeq<<s.Sampling.Width<<s.Sampling.Height<<YAML::EndSeq;
    out<<YAML::Key<<"Sampling"<<YAML::Value<<YAML::BeginMap;
    out<<YAML::Key<<"MinFilter"<<YAML::Value<<TextureFilterName(s.Sampling.MinFilter)<<YAML::Key<<"MagFilter"<<YAML::Value<<TextureFilterName(s.Sampling.MagFilter);
    out<<YAML::Key<<"WrapS"<<YAML::Value<<TextureWrapName(s.Sampling.WrapS)<<YAML::Key<<"WrapT"<<YAML::Value<<TextureWrapName(s.Sampling.WrapT)<<YAML::Key<<"GenerateMips"<<YAML::Value<<s.Sampling.GenerateMips;
    out<<YAML::Key<<"Format"<<YAML::Value<<(s.Sampling.Format==ImageFormat::R8?"R8":s.Sampling.Format==ImageFormat::RGB8?"RGB8":"RGBA8")<<YAML::EndMap;
    out<<YAML::Key<<"Regions"<<YAML::Value<<YAML::BeginSeq;
    for(auto r:s.Regions) out<<YAML::BeginMap<<YAML::Key<<"ID"<<YAML::Value<<YAML::DoubleQuoted<<SpriteIDText(r.ID)<<YAML::Key<<"Name"<<YAML::Value<<r.Name<<YAML::Key<<"Rect"<<YAML::Value<<YAML::Flow<<YAML::BeginSeq<<r.Rect.X<<r.Rect.Y<<r.Rect.Width<<r.Rect.Height<<YAML::EndSeq<<YAML::Key<<"Pivot"<<YAML::Value<<YAML::Flow<<YAML::BeginSeq<<r.Pivot.x<<r.Pivot.y<<YAML::EndSeq<<YAML::EndMap;
    out<<YAML::EndSeq<<YAML::Key<<"RetiredRegionIDs"<<YAML::Value<<YAML::Flow<<YAML::BeginSeq;
    for(auto id:s.RetiredRegionIDs) out<<YAML::DoubleQuoted<<SpriteIDText(id);
    out<<YAML::EndSeq<<YAML::Key<<"Clips"<<YAML::Value<<YAML::BeginSeq;
    for(auto c:s.Clips) {
        out<<YAML::BeginMap<<YAML::Key<<"ID"<<YAML::Value<<YAML::DoubleQuoted<<SpriteIDText(c.ID)<<YAML::Key<<"Name"<<YAML::Value<<c.Name<<YAML::Key<<"Loop"<<YAML::Value<<c.Loop<<YAML::Key<<"Frames"<<YAML::Value<<YAML::BeginSeq;
        for(auto f:c.Frames) out<<YAML::BeginMap<<YAML::Key<<"RegionID"<<YAML::Value<<YAML::DoubleQuoted<<SpriteIDText(f.Region)<<YAML::Key<<"Duration"<<YAML::Value<<f.Duration<<YAML::EndMap;
        out<<YAML::EndSeq<<YAML::EndMap;
    }
    out<<YAML::EndSeq<<YAML::Key<<"RetiredClipIDs"<<YAML::Value<<YAML::Flow<<YAML::BeginSeq;
    for(auto id:s.RetiredClipIDs) out<<YAML::DoubleQuoted<<SpriteIDText(id);
    out<<YAML::EndSeq<<YAML::EndMap<<YAML::EndMap;
    if(!out.good()) throw std::runtime_error(out.GetLastError()); return out.c_str();
}
std::string ValidateSpriteSheetSave(const std::filesystem::path& root,const std::filesystem::path& reference,const SpriteSheetDefinition& s) {
    s.Validate(); auto file=Project::ResolveOwnedAsset(root,reference);
    if(file.extension()!=".hsprites") throw std::runtime_error("Sheet must end in .hsprites");
    auto image=Texture2D::ReadImage(Project::ResolveOwnedAsset(root,s.Texture),s.Sampling.Format);
    if(image.Width!=s.Sampling.Width || image.Height!=s.Sampling.Height) throw std::runtime_error("Texture dimensions differ from saved sheet; explicitly accept/revalidate dimensions");
    return WriteSpriteSheetText(s);
}
void SaveSpriteSheet(const std::filesystem::path& root,const std::filesystem::path& reference,const SpriteSheetDefinition& s,WriteMode mode) {
    const auto text=ValidateSpriteSheetSave(root,reference,s);
    const auto file=Project::ResolveOwnedAsset(root,reference);
    std::filesystem::create_directories(file.parent_path());
    FileSystem::WriteFileAtomically(file,[&](auto& out){out<<text;},mode);
}
}
