#include "hzpch.h"
#include "Font.h"
#include "Hazel/Core/Resources.h"
#include "Hazel/Renderer/RenderCommand.h"
#include "MSDFData.h"
#include "Hazel/Utils/UTF8.h"
#include <map>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <algorithm>
#include <cmath>
#include <thread>
#include <stdexcept>

namespace Hazel {
namespace {
    struct PageRecord {std::weak_ptr<const FontPage> Page;std::weak_ptr<Texture2D> Texture;uint64_t Bytes;};
    struct CacheEntry {std::string Key;Ref<Font> Data;};
    FontLimits limits;
    FontResourceStats stats;
    std::thread::id owner;
    uint64_t retryEpoch=1;
    std::vector<uint32_t> glyphIDs, missingIDs; // serialized owner-thread workspace, <=65536 IDs each
    std::list<CacheEntry> cache;
    std::list<Ref<const FontPage>> pages; // the font resource cache's reusable GPU ownership
    std::vector<PageRecord> livePages; // weak index includes texture-only pending draws
    std::vector<std::weak_ptr<Font>> liveFaces;
    struct SourceRecord {
        std::weak_ptr<const Text::Face> Face;
        std::vector<uint32_t> BaseIDs; // printable ASCII shaped by the same CPU service
        std::weak_ptr<const FontPage> BasePage;
    };
    std::list<SourceRecord> sources; // weak identity index; stable during page eviction
    void CheckThread() {if(owner!=std::this_thread::get_id())throw std::logic_error("Font resources require the renderer thread/context");}
    void Prune() {
        sources.remove_if([](const auto& source){return source.Face.expired();});
        liveFaces.erase(std::remove_if(liveFaces.begin(),liveFaces.end(),[](const auto& f){return f.expired();}),liveFaces.end());
        livePages.erase(std::remove_if(livePages.begin(),livePages.end(),[](const auto& p){return p.Page.expired() && p.Texture.expired();}),livePages.end());
    }
    uint64_t LiveBytes() {uint64_t n=0;for(auto& p:livePages)n+=p.Bytes;return n;}
    uint64_t CachedBytes() {uint64_t n=0;for(auto& p:pages)n+=p->Bytes;return n;}
    void EvictPage() {pages.pop_back();++stats.Evictions;Prune();}
    void EvictFace() {cache.pop_back();++stats.Evictions;Prune();}
    struct FontHandles {
        msdfgen::FreetypeHandle* Library=msdfgen::initializeFreetype();msdfgen::FontHandle* Face=nullptr;
        ~FontHandles(){if(Face)msdfgen::destroyFont(Face);if(Library)msdfgen::deinitializeFreetype(Library);}
        void Open(const std::vector<uint8_t>& bytes) {
            if(!Library || !(Face=msdfgen::loadFontData(Library,bytes.data(),int(bytes.size()))))throw std::runtime_error("Invalid outline font");
        }
    };
    constexpr const char* Parameters="glyph-pages-v1-em40-range2-miter1-padding0-inktrap3-rgb8";
}
struct Font::Input {
    std::filesystem::path Path;std::vector<uint8_t> Bytes;std::string Key;
    explicit Input(const std::filesystem::path& path) {
        Path=std::filesystem::weakly_canonical(path);
        std::ifstream file(Path,std::ios::binary|std::ios::ate);auto size=file?file.tellg():std::streampos(-1);
        if(size<=0)throw std::runtime_error("Cannot read nonempty font: "+Path.generic_u8string());
        if(uint64_t(size)>limits.InputBytes)throw std::runtime_error("Font input exceeds "+std::to_string(limits.InputBytes)+" bytes: "+Path.generic_u8string());
        Bytes.resize(size_t(size));file.seekg(0);
        if(!file.read(reinterpret_cast<char*>(Bytes.data()),std::streamsize(Bytes.size())))throw std::runtime_error("Cannot read font: "+Path.generic_u8string());
        uint64_t hash=14695981039346656037ull,second=0x9e3779b97f4a7c15ull;
        for(auto byte:Bytes){hash=(hash^byte)*1099511628211ull;second=(second^byte)*0x100000001b3ull+(second>>32);}
        Key=Path.generic_u8string()+"|"+Parameters+"|"+std::to_string(Bytes.size())+"|"+std::to_string(hash)+"|"+std::to_string(second);
    }
};
const FontPage::Glyph* FontPage::Find(uint32_t id) const {
    auto it=std::lower_bound(Glyphs.begin(),Glyphs.end(),id,[](const Glyph& g,uint32_t n){return g.ID<n;});
    return it!=Glyphs.end() && it->ID==id?&*it:nullptr;
}
FontPage::~FontPage(){HZ_CORE_ASSERT(owner==std::this_thread::get_id(),"Pages must retire on renderer thread");}
Ref<Font> Font::s_DefaultFont;
void Font::Initialize(const FontLimits& requested) {
    if(owner!=std::thread::id{})CheckThread();
    Prune();
    if(!livePages.empty()||stats.LiveFonts||!cache.empty()||s_DefaultFont)throw std::logic_error("Release font owners before initializing font resources");
    if(!requested.InputBytes||requested.InputBytes>4*1024*1024||!requested.AtlasDimension||requested.AtlasDimension>1024||
       !requested.AtlasBytes||requested.AtlasBytes>3*1024*1024||!requested.LiveAtlases||requested.LiveAtlases>24||
       !requested.LiveAtlasBytes||requested.LiveAtlasBytes>24*1024*1024||!requested.GeneratorBytes||requested.GeneratorBytes>32*1024*1024||
       !requested.CacheEntries||requested.CacheEntries>8||requested.CacheBytes<requested.AtlasBytes||requested.CacheBytes>12*1024*1024)
        throw std::invalid_argument("Invalid font resource limits");
    owner=std::this_thread::get_id();limits=requested;stats={};
}
void Font::Accept(Input& input) {
    CheckThread();FontHandles handles;handles.Open(input.Bytes);
    Ref<const Text::Face> cpu;
    Prune();for(auto& record:sources)if(auto source=record.Face.lock();source && source->Identity()==input.Key){cpu=source;break;}
    for(;!cpu;)try {cpu=Text::Service::Main().CreateFace(input.Bytes,input.Key);break;}
        catch(const Text::ResourcePressure&) {if(!pages.empty())EvictPage();else if(!cache.empty())EvictFace();else throw;}
    // Validate before publishing; the optional packing cohort is not font coverage.
    (void)Text::Service::Main().Arrange("",cpu);
    if(!std::isfinite(cpu->HeightEm())||cpu->HeightEm()<=0)throw std::runtime_error("Invalid font height");
    if(std::none_of(sources.begin(),sources.end(),[&](const auto& record){return record.Face.lock()==cpu;})) {
        SourceRecord record;record.Face=cpu;
        try {
            std::string common;for(char c=32;c<127;++c){common+=c;common+=' ';}
            Text::Options options;options.PermitNotdef=true;
            auto shaped=Text::Service::Main().Arrange(common,cpu,options);
            for(auto& glyph:shaped->Glyphs)if(glyph.ID)record.BaseIDs.push_back(glyph.ID);
            std::sort(record.BaseIDs.begin(),record.BaseIDs.end());record.BaseIDs.erase(std::unique(record.BaseIDs.begin(),record.BaseIDs.end()),record.BaseIDs.end());
            if(record.BaseIDs.size()>128)record.BaseIDs.clear();
        }catch(const std::exception&){record.BaseIDs.clear();} // ordinary ID pages still serve the requested layout
        sources.push_back(std::move(record));
    }
    m_CPUFace=std::move(cpu);m_Path=input.Path;m_IdentityKey=input.Key;m_Accounted=true;++stats.LiveFonts;
}
Font::Font(const std::filesystem::path& path){CheckThread();Input input(path);Accept(input);}
Font::Font(Input& input){Accept(input);}
Font::~Font(){if(m_Accounted){HZ_CORE_ASSERT(owner==std::this_thread::get_id(),"Font owners must retire on renderer thread");--stats.LiveFonts;}}
Ref<const FontPage> Font::GeneratePage(const std::vector<uint32_t>& ids) {
    CheckThread();Prune();
    if(ids.empty() || ids.size()>128)throw std::invalid_argument("Page requires 1 to 128 glyph IDs");
    // Never reopen m_Path: old prepared results must rasterize their accepted bytes.
    FontHandles handles;handles.Open(m_CPUFace->Bytes());MSDFData data;
    data.FontGeometry=msdf_atlas::FontGeometry(&data.Glyphs);
    msdf_atlas::Charset set;for(auto id:ids)set.add(id);
    if(data.FontGeometry.loadGlyphset(handles.Face,1,set,true,false)!=int(ids.size()))throw std::runtime_error("Cannot load requested glyph IDs");
    size_t edges=0;bool ink=false;
    for(auto& g:data.Glyphs) {
        for(auto& c:g.getShape().contours)edges+=c.edges.size();
        if(edges>32768)throw std::runtime_error("Page outlines exceed 32768 edges");
        if(g.isWhitespace())continue;
        ink=true;
        auto b=g.getShape().getBounds();double extent=std::max(b.r-b.l,b.t-b.b)*40*data.FontGeometry.getGeometryScale()+16;
        if(!std::isfinite(b.l)||!std::isfinite(b.r)||!std::isfinite(b.t)||!std::isfinite(b.b)||!std::isfinite(extent)||extent>limits.AtlasDimension)
            throw std::runtime_error("Glyph exceeds page dimension limit");
    }
    int w=0,h=0;uint64_t bytes=0;
    if(ink) {
        msdf_atlas::TightAtlasPacker packer;packer.setPixelRange(2);packer.setMiterLimit(1);packer.setPadding(0);packer.setScale(40);
        if(packer.pack(data.Glyphs.data(),int(data.Glyphs.size()))!=0)throw std::runtime_error("Cannot pack glyph page");
        packer.getDimensions(w,h);auto maximum=std::min(limits.AtlasDimension,RenderCommand::GetCapabilities().MaxTextureSize);
        if(w<=0||h<=0||uint32_t(w)>maximum||uint32_t(h)>maximum)throw std::runtime_error("Glyph page exceeds dimension limit");
        bytes=uint64_t(w)*uint64_t(h)*3;if(bytes>limits.AtlasBytes)throw std::runtime_error("Glyph page exceeds byte limit");
        uint64_t area=0;for(auto& g:data.Glyphs){int l,b,gw,gh;g.getBoxRect(l,b,gw,gh);
            if(gw<0||gh<0||gw>w||gh>h)throw std::runtime_error("Invalid glyph box");
            area=std::max(area,uint64_t(gw)*uint64_t(gh));}
        if(bytes+area*26+m_CPUFace->Bytes().size()>limits.GeneratorBytes)throw std::runtime_error("Glyph generator scratch limit exceeded");
    }
    while(!pages.empty() && (LiveBytes()+bytes>limits.LiveAtlasBytes || livePages.size()>=limits.LiveAtlases))EvictPage();
    if(LiveBytes()+bytes>limits.LiveAtlasBytes || livePages.size()>=limits.LiveAtlases)throw std::runtime_error("Page admission blocked by live/pinned pages; release text owners then Reload family/Trim");
    auto page=CreateRef<FontPage>();page->Face=m_CPUFace;page->Path=m_Path;page->Bytes=bytes;
    if(ink) {
        for(auto& g:data.Glyphs)g.edgeColoring(msdfgen::edgeColoringInkTrap,3,0);
        msdf_atlas::GeneratorAttributes attributes;attributes.config.overlapSupport=true;attributes.scanlinePass=true;
        msdf_atlas::ImmediateAtlasGenerator<float,3,msdf_atlas::msdfGenerator,msdf_atlas::BitmapAtlasStorage<uint8_t,3>> generator(w,h);
        generator.setAttributes(attributes);generator.setThreadCount(2);generator.generate(data.Glyphs.data(),int(data.Glyphs.size()));
        msdfgen::BitmapConstRef<uint8_t,3> bitmap=generator.atlasStorage();
        TextureSpecification spec;spec.Width=w;spec.Height=h;spec.Format=ImageFormat::RGB8;spec.GenerateMips=false;spec.MinFilter=TextureFilter::Linear;
        page->Texture=Texture2D::Create(spec);page->Texture->SetData(bitmap.pixels,uint32_t(bytes));
    }
    for(auto& g:data.Glyphs) {
        double l,b,r,t,al,ab,ar,at;g.getQuadPlaneBounds(l,b,r,t);g.getQuadAtlasBounds(al,ab,ar,at);
        page->Glyphs.push_back({uint32_t(g.getIndex()),{l,b},{r,t},ink?glm::vec2(al/w,ab/h):glm::vec2(0),ink?glm::vec2(ar/w,at/h):glm::vec2(0),g.isWhitespace()});
    }
    std::sort(page->Glyphs.begin(),page->Glyphs.end(),[](auto& a,auto& b){return a.ID<b.ID;});
    livePages.push_back({page,page->Texture,bytes});++stats.Generations;
    while(!pages.empty() && (pages.size()>=limits.LiveAtlases || CachedBytes()+bytes>limits.CacheBytes))EvictPage();
    if(bytes<=limits.CacheBytes)pages.push_front(page);
    return page;
}
std::vector<Ref<const FontPage>> Font::PreparePages(const Text::Layout& layout) {
    CheckThread();if(layout.Font!=m_CPUFace)throw std::invalid_argument("Layout/page accepted face mismatch");
    if(layout.Glyphs.size()>65536)throw std::invalid_argument("Page preparation exceeds 65536 glyphs");
    auto& ids=glyphIDs;auto& missing=missingIDs;ids.clear();missing.clear();ids.reserve(layout.Glyphs.size());
    for(auto& g:layout.Glyphs)ids.push_back(g.ID);
    std::sort(ids.begin(),ids.end());ids.erase(std::unique(ids.begin(),ids.end()),ids.end());
    std::vector<Ref<const FontPage>> result;result.reserve(std::min(size_t(24),livePages.size()));
    Prune();auto source=std::find_if(sources.begin(),sources.end(),[&](const auto& record){return record.Face.lock()==m_CPUFace;});
    if(source==sources.end())throw std::logic_error("Accepted face lost its weak source record");
    if(m_PageFailureEpoch!=retryEpoch)m_PageError.clear();
    auto fail=[&](const std::exception& error){m_PageError=error.what();m_PageFailureEpoch=retryEpoch;++stats.PageFailures;HZ_CORE_WARN("Font '{}': {}. Missing pages paint magenta metric placeholders; Reload to retry.",m_Path.generic_u8string(),m_PageError);};
    bool needsBase=std::any_of(ids.begin(),ids.end(),[&](auto id){return m_CPUFace->HasInk(id) && std::binary_search(source->BaseIDs.begin(),source->BaseIDs.end(),id);});
    Ref<const FontPage> basePage;
    if(needsBase) {
        basePage=source->BasePage.lock();
        if(!basePage && m_PageError.empty())try {basePage=GeneratePage(source->BaseIDs);source->BasePage=basePage;}
            catch(const std::exception& error){fail(error);}
    }
    if(basePage)result.push_back(basePage); // stable priority over overlapping generic buckets
    for(auto id:ids) {
        if(!m_CPUFace->HasInk(id))continue;
        Ref<const FontPage> found;
        if(basePage && basePage->Find(id))found=basePage;
        else for(auto& record:livePages)if(auto p=record.Page.lock();p && p->Face==m_CPUFace && p->Find(id)){found=p;break;}
        if(found){++stats.PageHits;if(std::find(result.begin(),result.end(),found)==result.end())result.push_back(found);
            auto it=std::find(pages.begin(),pages.end(),found);if(it!=pages.end())pages.splice(pages.begin(),pages,it);}
        else missing.push_back(id);
    }
    if(!m_PageError.empty())return result; // explicit freshness retry, never a per-frame generation storm
    // Stable ID buckets avoid a page for each newly typed character. Pages
    // never grow/repack; the same bounded 128-ID bucket is reused thereafter.
    uint32_t previous=UINT32_MAX;
    for(auto id:missing) {
        uint32_t base=(id/128)*128;if(base==previous)continue;previous=base;
        std::vector<uint32_t> bucket;
        for(uint32_t glyph=base;glyph<std::min(base+128,m_CPUFace->GlyphCount());++glyph)bucket.push_back(glyph);
        try {result.push_back(GeneratePage(bucket));}
        catch(const std::exception& error){fail(error);break;}
    }
    return result;
}
Ref<const Text::Layout> Font::Layout(const std::string& text,float width,bool wrap,float tracking,float spacing,Text::Overflow overflow,uint32_t maxLines,float height) {
    CheckThread();if(!std::isfinite(width) || !std::isfinite(height) || (width<0 && width!=-1) || (height<0 && height!=-1))throw std::invalid_argument("Text constraints must be finite, -1 (indefinite) or nonnegative");
    if(!wrap && overflow==Text::Overflow::Clip)width=-1;
    if(overflow==Text::Overflow::Clip)height=-1;
    for(auto it=m_Layouts.begin();it!=m_Layouts.end();++it)if(it->Width==width && it->Wrap==wrap && it->Height==height && it->OverflowMode==overflow && it->MaxLines==maxLines && it->Tracking==tracking && it->Spacing==spacing && it->Result->Source==text){auto result=it->Result;m_Layouts.splice(m_Layouts.begin(),m_Layouts,it);++stats.LayoutHits;return result;}
    Text::Options options;options.EmSize=1/m_CPUFace->HeightEm();options.Tracking=tracking;options.LineSpacing=spacing;options.PermitNotdef=true;options.Wrap=wrap;options.OverflowMode=overflow;options.MaxLines=maxLines;if(width>=0)options.Width=width;if(height>=0)options.Height=height;
    Ref<const Text::Layout> result;
    for(;;)try{result=Text::Service::Main().Arrange(text,m_CPUFace,options);break;}catch(const Text::ResourcePressure&){if(m_Layouts.empty())throw;m_LayoutBytes-=m_Layouts.back().Result->RetainedBytes();m_Layouts.pop_back();}
    ++stats.LayoutBuilds;
    if(result->MissingGlyphs && m_TextWarning.empty()) {
        m_TextWarning="Observed missing source glyphs: .notdef, no cross-family fallback";
        HZ_CORE_WARN("Font '{}': {}",m_Path.generic_u8string(),m_TextWarning);
    }
    const auto bytes=result->RetainedBytes();
    while(!m_Layouts.empty() && (m_Layouts.size()>=128 || m_LayoutBytes+bytes>256*1024)){m_LayoutBytes-=m_Layouts.back().Result->RetainedBytes();m_Layouts.pop_back();}
    if(bytes<=256*1024){m_Layouts.push_front({width,tracking,spacing,height,wrap,overflow,maxLines,result});m_LayoutBytes+=bytes;}return result;
}
Ref<Font> Font::Load(const std::filesystem::path& path,const Ref<Font>& accepted) {
    CheckThread();Input input(path);Prune();
    for(auto it=cache.begin();it!=cache.end();++it)if(it->Key==input.Key){auto font=it->Data;cache.splice(cache.begin(),cache,it);font->m_PageError.clear();++stats.Hits;return font;}
    if(accepted && accepted->m_IdentityKey==input.Key){accepted->m_PageError.clear();++stats.Hits;return accepted;}
    for(auto& weak:liveFaces)if(auto live=weak.lock();live && live->m_IdentityKey==input.Key){live->m_PageError.clear();++stats.Hits;return live;}
    auto font=Ref<Font>(new Font(input));liveFaces.push_back(font);
    for(auto it=cache.begin();it!=cache.end();)if(it->Data->GetPath()==font->GetPath())it=cache.erase(it);else ++it;
    while(cache.size()>=limits.CacheEntries)EvictFace();
    cache.push_front({font->m_IdentityKey,font});return font;
}
Ref<Font> Font::GetDefault(){CheckThread();if(!s_DefaultFont){s_DefaultFont=CreateRef<Font>(Hazel::Resources::Resolve("fonts/opensans/OpenSans-Regular.ttf"));liveFaces.push_back(s_DefaultFont);}return s_DefaultFont;}
uint64_t Font::GetAtlasBytes() const {uint64_t bytes=0;for(auto& r:livePages)if(auto p=r.Page.lock();p && p->Face==m_CPUFace)bytes+=p->Bytes;return bytes;}
uint32_t Font::PageCount() const {uint32_t count=0;for(auto& r:livePages)if(auto p=r.Page.lock();p && p->Face==m_CPUFace)++count;return count;}
void Font::ReleaseRoot(const std::filesystem::path& root){CheckThread();for(auto it=pages.begin();it!=pages.end();) {
    auto relative=(*it)->Path.lexically_relative(root);
    bool remove=!relative.empty() && *relative.begin()!="..";
    if(remove)it=pages.erase(it);else ++it;
}for(auto it=cache.begin();it!=cache.end();){auto relative=it->Data->GetPath().lexically_relative(root);if(!relative.empty()&&*relative.begin()!="..")it=cache.erase(it);else ++it;}Prune();}
void Font::Trim(){CheckThread();++retryEpoch;while(!pages.empty())EvictPage();for(auto& weak:liveFaces)if(auto font=weak.lock()){font->m_PageError.clear();font->m_Layouts.clear();font->m_LayoutBytes=0;}while(!cache.empty())EvictFace();}
FontResourceStats Font::Resources(){CheckThread();Prune();auto result=stats;result.Limits=limits;result.CacheEntries=uint32_t(cache.size());result.CachePages=uint32_t(pages.size());result.CacheBytes=CachedBytes();result.LiveAtlases=uint32_t(livePages.size());result.LiveAtlasBytes=LiveBytes();
    for(auto& record:livePages){bool reusable=false;for(auto& page:pages)if(record.Page.lock()==page){reusable=page.use_count()==1 && page->Texture.use_count()<=1;break;}if(!reusable){++result.PinnedAtlases;result.PinnedBytes+=record.Bytes;}}return result;}
void Font::Shutdown(){if(owner==std::thread::id{})return;CheckThread();Trim();std::vector<uint32_t>().swap(glyphIDs);std::vector<uint32_t>().swap(missingIDs);s_DefaultFont.reset();Prune();if(!livePages.empty()||stats.LiveFonts)HZ_CORE_ERROR("Font shutdown: {} live fonts / {} pages still owned; release GPU Refs before Application teardown",stats.LiveFonts,livePages.size());}
void Font::ValidateFile(const std::filesystem::path& path){Input input(path);FontHandles handles;handles.Open(input.Bytes);Text::Service service;auto face=service.CreateFace(input.Bytes,input.Key);(void)service.Arrange("",face);if(!std::isfinite(face->HeightEm())||face->HeightEm()<=0)throw std::runtime_error("Invalid font height");}
std::string FontCoverage::Summary() const {std::ostringstream out;out<<Scalars<<" checked, "<<MissingSource<<" absent in face, "<<Unsupported<<" outside tested typography; glyph-ID pages prepare on demand (residency/admission not certified)";for(auto& issue:Issues)out<<"; U+"<<std::uppercase<<std::hex<<std::setw(4)<<std::setfill('0')<<issue.Scalar<<std::dec<<(issue.Source?" source-present":" source-missing")<<(issue.Typography?" unsupported":"");if(!Detail.empty())out<<"; "<<Detail;return out.str();}
FontCoverage Font::InspectCoverage(const std::filesystem::path& path,const std::string& corpus) {
    if(corpus.size()>8192)throw std::invalid_argument("Font corpus exceeds 8192 bytes");
    std::vector<uint32_t> decoded;std::vector<size_t> offsets;UTF8::Decode(corpus,decoded,&offsets);
    std::map<uint32_t,size_t> scalars;for(size_t i=0;i<decoded.size();++i)scalars.emplace(decoded[i],i);
    if(scalars.size()>4096)throw std::invalid_argument("Font corpus exceeds 4096 unique scalars");
    Input input(path);FontHandles handles;handles.Open(input.Bytes);Text::Service service;auto face=service.CreateFace(input.Bytes,input.Key);FontCoverage result;
    Text::Options options;options.PermitNotdef=true;uint32_t shapedMissing=0;bool unsupported=false;
    try{shapedMissing=service.Arrange(corpus,face,options)->MissingGlyphs;}
    catch(const Text::ResourcePressure&){throw;}
    catch(const std::exception& error){unsupported=true;result.Detail=error.what();}
    for(auto [scalar,index]:scalars) {
        if(scalar=='\n'||scalar=='\r'||scalar=='\t'||scalar==0xfeff||scalar==0x85||scalar==0x61c||scalar==0x2028||scalar==0x2029||scalar==0x2060 || (scalar>=0x200b&&scalar<=0x200f)||(scalar>=0x202a&&scalar<=0x202e)||(scalar>=0x2066&&scalar<=0x2069))continue;
        ++result.Scalars;bool source=face->HasScalar(scalar),typography=false;
        if(unsupported)try{(void)service.Arrange(corpus.substr(offsets[index],offsets[index+1]-offsets[index]),face,options);}
            catch(const Text::ResourcePressure&){throw;}
            catch(const std::exception&){typography=true;}
        result.MissingSource+=!source;result.Unsupported+=typography;
        if((!source||typography)&&result.Issues.size()<32)result.Issues.push_back({scalar,source,typography});
    }
    if(shapedMissing && !result.MissingSource)result.Detail="Shaping produced .notdef despite nominal cmap coverage";
    return result;
}
}
