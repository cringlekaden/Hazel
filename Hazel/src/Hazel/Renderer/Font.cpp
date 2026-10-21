#include "hzpch.h"
#include "Font.h"
#include "Hazel/Core/Resources.h"
#include "Hazel/Renderer/RenderCommand.h"
#include "MSDFData.h"
#include <fstream>
#include <algorithm>
#include <cmath>
#include <list>
#include <thread>
#include <stdexcept>
#undef INFINITE
#include "msdf-atlas-gen.h"

namespace Hazel {
namespace {
    struct AtlasRecord {std::weak_ptr<Texture2D> Texture;uint64_t Bytes;};
    struct CacheEntry {std::string Key;Ref<Font> Data;};
    FontLimits limits;
    FontResourceStats stats;
    std::thread::id owner;
    std::list<CacheEntry> cache; // front is MRU; bounded and deliberately small
    std::vector<AtlasRecord> atlases;
    void CheckThread() {
        if(owner!=std::this_thread::get_id())throw std::logic_error("Font resources require the renderer thread/context");
    }
    void Prune() {
        atlases.erase(std::remove_if(atlases.begin(),atlases.end(),[](const auto& a){return a.Texture.expired();}),atlases.end());
    }
    uint64_t LiveBytes() {uint64_t bytes=0;for(auto& a:atlases)bytes+=a.Bytes;return bytes;}
    uint64_t CachedBytes() {uint64_t bytes=0;for(auto& e:cache)bytes+=e.Data->GetAtlasBytes();return bytes;}
    void Evict() {cache.pop_back();++stats.Evictions;Prune();}
    struct FontHandles {
        msdfgen::FreetypeHandle* Library=msdfgen::initializeFreetype();
        msdfgen::FontHandle* Face=nullptr;
        ~FontHandles(){if(Face)msdfgen::destroyFont(Face);if(Library)msdfgen::deinitializeFreetype(Library);}
    };
    void ValidateMetrics(const msdfgen::FontMetrics& m) {
        if(!std::isfinite(m.emSize)||m.emSize<=0||!std::isfinite(m.ascenderY)||!std::isfinite(m.descenderY)||
           m.ascenderY<=m.descenderY||!std::isfinite(m.lineHeight)||m.lineHeight<=0)
            throw std::runtime_error("Invalid font metrics");
    }
    constexpr const char* Parameters="latin1-msdf-v1-em40-range2-miter1-padding0-inktrap3-rgb8";
}
struct Font::Input {
    std::filesystem::path Path;
    std::vector<uint8_t> Bytes;
    std::string Key;
    explicit Input(const std::filesystem::path& path) {
        Path=std::filesystem::weakly_canonical(path);
        // Read size from the opened stream; a growing/replaced file cannot allocate past the cap.
        std::ifstream file(Path,std::ios::binary|std::ios::ate);
        auto size=file?file.tellg():std::streampos(-1);
        if(size<=0)throw std::runtime_error("Cannot read nonempty font: "+Path.generic_u8string());
        if(uint64_t(size)>limits.InputBytes)
            throw std::runtime_error("Font input exceeds "+std::to_string(limits.InputBytes)+" bytes: "+Path.generic_u8string());
        Bytes.resize(size_t(size));file.seekg(0);
        if(!file.read(reinterpret_cast<char*>(Bytes.data()),std::streamsize(Bytes.size())))
            throw std::runtime_error("Cannot read font: "+Path.generic_u8string());
        // Content identity (two deterministic 64-bit streams), canonical path and fixed
        // generation parameters; no dependence on coarse filesystem timestamps.
        uint64_t hash=14695981039346656037ull,second=0x9e3779b97f4a7c15ull;
        for(auto byte:Bytes){hash=(hash^byte)*1099511628211ull;second=(second^byte)*0x100000001b3ull+(second>>32);}
        Key=Path.generic_u8string()+"|"+Parameters+"|"+std::to_string(Bytes.size())+"|"+std::to_string(hash)+"|"+std::to_string(second);
    }
};
Ref<Font> Font::s_DefaultFont;
void Font::Initialize(const FontLimits& requested) {
    if(owner!=std::thread::id{})CheckThread();
    Prune();
    if(!atlases.empty()||stats.LiveFonts||!cache.empty()||s_DefaultFont)throw std::logic_error("Release font owners before initializing font resources");
    if(!requested.InputBytes||requested.InputBytes>4*1024*1024||!requested.AtlasDimension||requested.AtlasDimension>1024||
       !requested.AtlasBytes||requested.AtlasBytes>3*1024*1024||!requested.LiveAtlases||requested.LiveAtlases>24||
       !requested.LiveAtlasBytes||requested.LiveAtlasBytes>24*1024*1024||!requested.GeneratorBytes||requested.GeneratorBytes>32*1024*1024||
       !requested.CacheEntries||requested.CacheEntries>8||requested.CacheBytes<requested.AtlasBytes||requested.CacheBytes>12*1024*1024)
        throw std::invalid_argument("Invalid font resource limits");
    owner=std::this_thread::get_id();limits=requested;stats={};
}
namespace {
Scope<MSDFData> PlanFont(const std::vector<uint8_t>& encoded,const std::filesystem::path& path,
                         uint32_t maximumDimension,int& w,int& hgt) {
    FontHandles h;
    if(!h.Library || !(h.Face=msdfgen::loadFontData(h.Library,encoded.data(),int(encoded.size()))))
        throw std::runtime_error("Invalid font: "+path.generic_u8string());
    auto data=CreateScope<MSDFData>();
    data->FontGeometry=msdf_atlas::FontGeometry(&data->Glyphs);
    msdf_atlas::Charset charset;for(uint32_t c=0x20;c<=0xff;++c)charset.add(c);
    const auto count=data->FontGeometry.loadCharset(h.Face,1,charset);
    if(count<=0 || !data->FontGeometry.getGlyph('?') || !data->FontGeometry.getGlyph(' '))
        throw std::runtime_error("Font needs space and fallback glyphs: "+path.generic_u8string());
    ValidateMetrics(data->FontGeometry.getMetrics());
    // Font outlines are parsed by the pinned dependencies before this bounded-work check.
    size_t edges=0;for(auto& glyph:data->Glyphs)for(auto& contour:glyph.getShape().contours)edges+=contour.edges.size();
    if(edges>32768)throw std::runtime_error("Font Latin-1 outlines exceed 32768 edges: "+path.generic_u8string());
    for(auto& glyph:data->Glyphs) {
        if(!std::isfinite(glyph.getAdvance()))throw std::runtime_error("Invalid font advance");
        if(glyph.isWhitespace())continue;
        const auto bounds=glyph.getShape().getBounds();const auto scale=40*data->FontGeometry.getGeometryScale();
        const double extent=std::max(bounds.r-bounds.l,bounds.t-bounds.b)*scale+16;
        if(!std::isfinite(bounds.l)||!std::isfinite(bounds.r)||!std::isfinite(bounds.t)||!std::isfinite(bounds.b)||
            !std::isfinite(extent)||extent>limits.AtlasDimension)
            throw std::runtime_error("Font glyph exceeds atlas dimension limit");
    }
    msdf_atlas::TightAtlasPacker packer;
    packer.setPixelRange(2);packer.setMiterLimit(1);packer.setPadding(0);packer.setScale(40);
    if(packer.pack(data->Glyphs.data(),int(data->Glyphs.size()))!=0)throw std::runtime_error("Cannot pack font atlas");
    packer.getDimensions(w,hgt);
    if(w<=0||hgt<=0||uint32_t(w)>limits.AtlasDimension||uint32_t(hgt)>limits.AtlasDimension||
        uint32_t(w)>maximumDimension||uint32_t(hgt)>maximumDimension)
        throw std::runtime_error("Font atlas exceeds dimension limit: "+path.generic_u8string());
    const uint64_t bytes=uint64_t(w)*uint64_t(hgt)*3;
    if(bytes>limits.AtlasBytes)throw std::runtime_error("Font atlas exceeds per-resource bytes: "+path.generic_u8string());
    uint64_t maxArea=0;
    for(auto& glyph:data->Glyphs){int l,b,gw,gh;glyph.getBoxRect(l,b,gw,gh);
        if(gw<0||gh<0||gw>w||gh>hgt)throw std::runtime_error("Invalid font glyph box");
        maxArea=std::max(maxArea,uint64_t(gw)*uint64_t(gh));}
    // Pinned generator: RGB8 atlas + two float-RGB workspaces and two error buffers.
    if(bytes+maxArea*26+encoded.size()>limits.GeneratorBytes)throw std::runtime_error("Font generator scratch limit exceeded");
    return data;
}
}
void Font::ValidateFile(const std::filesystem::path& path) {
    Input input(path);int w=0,h=0;
    (void)PlanFont(input.Bytes,input.Path,limits.AtlasDimension,w,h);
}
Font::Font(const std::filesystem::path& path) {CheckThread();Input input(path);Generate(input);}
Font::Font(Input& input) {Generate(input);}
void Font::Generate(Input& input) {
    CheckThread();Prune();int w=0,hgt=0;
    auto data=PlanFont(input.Bytes,input.Path,RenderCommand::GetCapabilities().MaxTextureSize,w,hgt);
    const uint64_t bytes=uint64_t(w)*uint64_t(hgt)*3;
    while(!cache.empty() && (LiveBytes()+bytes>limits.LiveAtlasBytes||atlases.size()>=limits.LiveAtlases))Evict();
    if(LiveBytes()+bytes>limits.LiveAtlasBytes||atlases.size()>=limits.LiveAtlases)
        throw std::runtime_error("Font admission blocked by live/pinned atlases; release owners or retry after Stop: "+input.Path.generic_u8string());
    for(auto& glyph:data->Glyphs)glyph.edgeColoring(msdfgen::edgeColoringInkTrap,3,0);
    msdf_atlas::GeneratorAttributes attributes;attributes.config.overlapSupport=true;attributes.scanlinePass=true;
    msdf_atlas::ImmediateAtlasGenerator<float,3,msdf_atlas::msdfGenerator,msdf_atlas::BitmapAtlasStorage<uint8_t,3>> generator(w,hgt);
    generator.setAttributes(attributes);generator.setThreadCount(2);
    generator.generate(data->Glyphs.data(),int(data->Glyphs.size()));
    msdfgen::BitmapConstRef<uint8_t,3> bitmap=generator.atlasStorage();
    TextureSpecification spec;spec.Width=w;spec.Height=hgt;spec.Format=ImageFormat::RGB8;
    spec.GenerateMips=false;spec.MinFilter=TextureFilter::Linear;
    auto atlas=Texture2D::Create(spec);atlas->SetData(bitmap.pixels,uint32_t(bytes));
    atlases.push_back({atlas,bytes}); // publish only the completed resource
    m_Path=input.Path;m_IdentityKey=input.Key;m_Data=std::move(data);m_AtlasTexture=std::move(atlas);m_AtlasBytes=bytes;m_Accounted=true;
    ++stats.LiveFonts;++stats.Generations;
}
Font::~Font() {
    if(m_Accounted){HZ_CORE_ASSERT(owner==std::this_thread::get_id(),"Font owners must retire on the renderer thread before context teardown");--stats.LiveFonts;}
}
Ref<Font> Font::Load(const std::filesystem::path& path,const Ref<Font>& accepted) {
    CheckThread();Input input(path);
    for(auto it=cache.begin();it!=cache.end();++it)if(it->Key==input.Key){auto font=it->Data;cache.splice(cache.begin(),cache,it);++stats.Hits;return font;}
    auto retain=[](const Ref<Font>& font) {
        if(!limits.CacheEntries||font->GetAtlasBytes()>limits.CacheBytes)return;
        for(auto it=cache.begin();it!=cache.end();) {
            if(it->Data->GetPath()==font->GetPath())it=cache.erase(it);else ++it;
        }
        while(!cache.empty()&&(cache.size()>=limits.CacheEntries||CachedBytes()+font->GetAtlasBytes()>limits.CacheBytes))Evict();
        cache.push_front({font->m_IdentityKey,font});
    };
    Ref<Font> font;
    try{font=Ref<Font>(new Font(input));}
    catch(...){if(accepted)retain(accepted);throw;}
    retain(font);return font;
}
Ref<Font> Font::GetDefault() {
    CheckThread();
    if(!s_DefaultFont)s_DefaultFont=CreateRef<Font>(Hazel::Resources::Resolve("fonts/opensans/OpenSans-Regular.ttf"));
    return s_DefaultFont;
}
void Font::ReleaseRoot(const std::filesystem::path& root){
    CheckThread();for(auto it=cache.begin();it!=cache.end();){auto relative=it->Data->GetPath().lexically_relative(root);
        if(!relative.empty()&&*relative.begin()!="..")it=cache.erase(it);else ++it;}Prune();
}
void Font::Trim(){CheckThread();while(!cache.empty())Evict();}
FontResourceStats Font::Resources(){
    CheckThread();Prune();auto result=stats;result.Limits=limits;
    result.CacheEntries=uint32_t(cache.size());result.CacheBytes=CachedBytes();
    result.LiveAtlases=uint32_t(atlases.size());result.LiveAtlasBytes=LiveBytes();
    for(auto& atlas:atlases){bool reusableOnly=false;
        for(auto& entry:cache)if(entry.Data->GetAtlasTexture()==atlas.Texture.lock()) {
            reusableOnly=entry.Data.use_count()==1 && atlas.Texture.use_count()==1;break;
        }
        if(!reusableOnly){++result.PinnedAtlases;result.PinnedBytes+=atlas.Bytes;}
    }
    return result;
}
void Font::Shutdown(){
    if(owner==std::thread::id{})return;
    CheckThread();Trim();s_DefaultFont.reset();Prune();
    if(!atlases.empty()||stats.LiveFonts)HZ_CORE_ERROR("Font shutdown: {} live fonts / {} atlases still owned; release resource Refs before Application teardown",stats.LiveFonts,atlases.size());
}
}
