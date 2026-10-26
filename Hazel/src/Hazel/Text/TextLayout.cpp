#include "hzpch.h"
#include "TextLayout.h"
#include "Hazel/Utils/UTF8.h"
#include <hb.h>
#include <hb-ot.h>
#include <graphemebreak.h>
#include <linebreak.h>
#include <SheenBidi/SheenBidi.h>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <limits>
#include <mutex>
#include <stdexcept>

namespace Hazel::Text {
struct Budget { Resources Live; Limits Cap; std::thread::id Owner; };
namespace {
void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
void Admit(bool condition,const char* message) {
    if(!condition)throw ResourcePressure(message);
}
template<class T, void(*Release)(T)> struct Handle {
    T Value;
    explicit Handle(T value): Value(value) { Require(value!=nullptr,"Typography dependency allocation/analysis failed"); }
    ~Handle() { Release(Value); }
    Handle(const Handle&)=delete;
    operator T() const { return Value; }
};
using Buffer=Handle<hb_buffer_t*,hb_buffer_destroy>;
using Plan=Handle<hb_shape_plan_t*,hb_shape_plan_destroy>;
using Algorithm=Handle<SBAlgorithmRef,SBAlgorithmRelease>;
using BidiParagraph=Handle<SBParagraphRef,SBParagraphRelease>;
using BidiLine=Handle<SBLineRef,SBLineRelease>;
using Scripts=Handle<SBScriptLocatorRef,SBScriptLocatorRelease>;

// Sheen's stable algorithm API uses a process default allocator. Serialize its
// entire use and scope a checked allocator, restoring the previous default only
// after every dependent object has died. Never install a per-document global.
std::mutex bidiMutex;
struct BidiMemory {
    struct alignas(std::max_align_t) Header { size_t Bytes; };
    uint64_t Used=0, Limit;
    std::unique_lock<std::mutex> Lock{bidiMutex};
    SBAllocatorRef Allocator, Previous;
    static void* Allocate(SBUInteger n, void* context) {
        auto& m=*static_cast<BidiMemory*>(context);
        if(n>m.Limit-m.Used || n>SIZE_MAX-sizeof(Header)) return nullptr;
        auto p=static_cast<Header*>(std::malloc(size_t(n)+sizeof(Header)));
        if(!p)return nullptr;
        p->Bytes=size_t(n);m.Used+=n;return p+1;
    }
    static void Free(void* p, void* context) {
        if(!p)return;
        auto h=static_cast<Header*>(p)-1;
        static_cast<BidiMemory*>(context)->Used-=h->Bytes;std::free(h);
    }
    static void* Resize(void* p,SBUInteger n,void* context) {
        if(!p)return Allocate(n,context);
        auto h=static_cast<Header*>(p)-1;auto old=h->Bytes;
        auto& m=*static_cast<BidiMemory*>(context);
        if(n>m.Limit-(m.Used-old) || n>SIZE_MAX-sizeof(Header))return nullptr;
        auto next=static_cast<Header*>(std::realloc(h,size_t(n)+sizeof(Header)));
        if(!next)return nullptr;
        m.Used=m.Used-old+n;next->Bytes=size_t(n);return next+1;
    }
    explicit BidiMemory(uint64_t bytes):Limit(bytes) {
        Previous=SBAllocatorGetDefault();
        SBAllocatorProtocol protocol{};protocol.allocateBlock=Allocate;
        protocol.deallocateBlock=Free;protocol.reallocateBlock=Resize;
        Allocator=SBAllocatorCreate(&protocol,this);
        Require(Allocator!=nullptr,"Cannot allocate bidi allocator");
        SBAllocatorSetDefault(Allocator);
    }
    ~BidiMemory() {
        SBAllocatorSetDefault(Previous);SBAllocatorRelease(Allocator);
    }
};
uint64_t Bytes(const Layout& l) {
    return sizeof(Layout)+l.Source.capacity()+l.Scalars.capacity()*sizeof(uint32_t)+
        l.Boundaries.capacity()*sizeof(SourceBoundary)+l.Paragraphs.capacity()*sizeof(Paragraph)+
        l.Lines.capacity()*sizeof(Line)+l.Runs.capacity()*sizeof(Run)+l.Glyphs.capacity()*sizeof(Glyph);
}
bool SupportedScript(uint32_t tag) {
    return tag==HB_TAG('L','a','t','n') || tag==HB_TAG('G','r','e','k') || tag==HB_TAG('C','y','r','l') ||
        tag==HB_TAG('A','r','a','b') || tag==HB_TAG('H','e','b','r') || tag==HB_TAG('Z','y','y','y') ||
        tag==HB_TAG('Z','i','n','h');
}
}
struct Face::Data {
    std::vector<uint8_t> Bytes;
    hb_face_t* Face=nullptr;
    hb_font_t* Font=nullptr;
    bool StaticOutline=true;
    ~Data() { if(Font)hb_font_destroy(Font);if(Face)hb_face_destroy(Face); }
};
Face::Face()=default;
Face::~Face() {
    if(m_Budget) { --m_Budget->Live.Faces; m_Budget->Live.FaceBytes-=m_Data->Bytes.capacity()+m_Identity.capacity()+sizeof(Face)+sizeof(Data); }
}
uint32_t Face::UnitsPerEm() const { return hb_face_get_upem(m_Data->Face); }
uint32_t Face::GlyphCount() const { return hb_face_get_glyph_count(m_Data->Face); }
const std::vector<uint8_t>& Face::Bytes() const { return m_Data->Bytes; }
float Face::HeightEm() const { hb_font_extents_t e{};Require(hb_font_get_h_extents(m_Data->Font,&e),"Font has no horizontal extents");return float(e.ascender-e.descender)/UnitsPerEm(); }
float Face::AscenderEm() const { hb_font_extents_t e{};Require(hb_font_get_h_extents(m_Data->Font,&e),"Font has no horizontal extents");return float(e.ascender)/UnitsPerEm(); }
float Face::LineHeightEm() const { hb_font_extents_t e{};Require(hb_font_get_h_extents(m_Data->Font,&e),"Font has no horizontal extents");return float(e.ascender-e.descender+e.line_gap)/UnitsPerEm(); }
bool Face::HasScalar(uint32_t scalar) const { hb_codepoint_t id=0;return hb_font_get_nominal_glyph(m_Data->Font,scalar,&id) && id; }

bool Face::HasInk(uint32_t id) const { hb_glyph_extents_t e{};return !hb_font_get_glyph_extents(m_Data->Font,id,&e) || e.width!=0 || e.height!=0; }
Layout::~Layout() { if(m_Budget) { --m_Budget->Live.Results; m_Budget->Live.ResultBytes-=m_Bytes; } }
Service::Service(const Limits& cap):m_Limits(cap),m_Budget(CreateRef<Budget>()),m_Owner(std::this_thread::get_id()) {
    const Limits defaults;
    Require(cap.InputBytes && cap.InputBytes<=defaults.InputBytes && cap.Scalars && cap.Scalars<=defaults.Scalars &&
        cap.Runs && cap.Runs<=defaults.Runs && cap.Glyphs && cap.Glyphs<=defaults.Glyphs && cap.Lines && cap.Lines<=defaults.Lines &&
        cap.ShapeScalars && cap.ShapeScalars<=defaults.ShapeScalars && cap.ShapeCalls && cap.ShapeCalls<=defaults.ShapeCalls &&
        cap.Faces && cap.Faces<=defaults.Faces && cap.Results && cap.Results<=defaults.Results &&
        cap.FaceBytes && cap.FaceBytes<=defaults.FaceBytes && cap.ResultBytes && cap.ResultBytes<=defaults.ResultBytes &&
        cap.BidiBytes && cap.BidiBytes<=defaults.BidiBytes,"Invalid CPU typography limits");
    m_Budget->Cap=cap;m_Budget->Owner=m_Owner;
}
Service& Service::Main() { static Service service;service.CheckThread();return service; }
void Service::CheckThread() const { Require(m_Owner==std::this_thread::get_id(),"Typography service requires its owning thread"); }
Resources Service::Usage() const { CheckThread();return m_Budget->Live; }
Ref<const Face> Service::CreateFace(const std::vector<uint8_t>& bytes,std::string identity) {
    CheckThread();
    Require(!bytes.empty() && bytes.size()<=4*1024*1024,"CPU font input must be 1 byte to 4 MiB");
    Require(!identity.empty() && identity.size()<=4096,"CPU font identity must be 1 to 4096 bytes");
    auto& live=m_Budget->Live;
    Admit(live.Faces<m_Limits.Faces,"CPU font face count exhausted; release inactive owners");
    const auto estimate=bytes.size()+identity.size()+sizeof(Face)+sizeof(Face::Data);
    Admit(estimate<=m_Limits.FaceBytes-live.FaceBytes,"CPU font input retention exhausted; release inactive owners");
    Ref<Face> face(new Face);face->m_Data=CreateScope<Face::Data>();face->m_Identity=std::move(identity);
    auto& data=*face->m_Data;data.Bytes=bytes;
    auto blob=hb_blob_create(reinterpret_cast<const char*>(data.Bytes.data()),unsigned(data.Bytes.size()),HB_MEMORY_MODE_READONLY,nullptr,nullptr);
    data.Face=hb_face_create(blob,0);hb_blob_destroy(blob);
    Require(data.Face && hb_face_get_glyph_count(data.Face)>0,"Invalid/empty OpenType face");
    for(const auto tag:{HB_TAG('f','v','a','r'),HB_TAG('C','O','L','R'),HB_TAG('C','B','D','T'),HB_TAG('s','b','i','x'),HB_TAG('S','V','G',' ')}) {
        auto table=hb_face_reference_table(data.Face,tag);auto size=hb_blob_get_length(table);hb_blob_destroy(table);
        if(size)data.StaticOutline=false;
    }
    Require(hb_face_get_upem(data.Face)>=16 && hb_face_get_upem(data.Face)<=16384,"Invalid font units per em");
    data.Font=hb_font_create(data.Face);Require(data.Font!=hb_font_get_empty(),"Cannot allocate shaping font");
    hb_ot_font_set_funcs(data.Font);hb_font_set_scale(data.Font,int(face->UnitsPerEm()),int(face->UnitsPerEm()));
    hb_face_make_immutable(data.Face);hb_font_make_immutable(data.Font);
    const auto retained=data.Bytes.capacity()+face->m_Identity.capacity()+sizeof(Face)+sizeof(Face::Data);
    Admit(retained<=m_Limits.FaceBytes-live.FaceBytes,"CPU font retained capacity exceeds limit");
    face->m_Budget=m_Budget;++live.Faces;live.FaceBytes+=retained;return face;
}

Ref<Layout> Service::Build(std::string_view source,const Ref<const Face>& face,const Options& options,uint32_t& calls,uint32_t& workScalars) {
    CheckThread();
    Require(face && face->m_Budget->Owner==m_Owner,"Text layout needs an accepted CPU face on the same owner thread");
    Require(face->m_Data->StaticOutline,"CPU typography supports static outline faces; variable/color tables are unsupported");
    Require(source.size()<=m_Limits.InputBytes,"Text exceeds CPU layout input byte limit");
    Require(std::isfinite(options.EmSize) && options.EmSize>0 && options.EmSize<=512,"Invalid text em size");
    Require(!options.Width || (std::isfinite(*options.Width) && *options.Width>=0 && *options.Width<=1e7f),"Invalid text available width");
    Require(options.BaseDirection==Direction::Auto || options.BaseDirection==Direction::LTR || options.BaseDirection==Direction::RTL,"Invalid text direction");
    bool known=false;
    for(const auto* language:{"und","en","fr","de","el","ru","ar","he"})known|=options.Language==language;
    Require(known,"CPU foundation language must be und/en/fr/de/el/ru/ar/he");

    auto result=CreateRef<Layout>();result->Source=std::string(source);result->Font=face;result->EmSize=options.EmSize;
    std::vector<size_t> offsets;UTF8::Decode(source,result->Scalars,&offsets);
    auto& out=*result;const auto count=uint32_t(out.Scalars.size());
    Require(count<=m_Limits.Scalars,"Text exceeds CPU layout scalar limit");
    out.Boundaries.resize(size_t(count)+1);
    uint32_t utf16=source.size()>=3 && source.substr(0,3)=="\xef\xbb\xbf"?1:0;
    for(uint32_t i=0;i<=count;++i) {
        out.Boundaries[i].Byte=uint32_t(offsets[i]);out.Boundaries[i].UTF16=utf16;
        if(i<count)utf16+=out.Scalars[i]>0xffff?2:1;
    }
    std::vector<char> graphemes(count),breaks(count);
    if(count) {
        set_graphemebreaks_utf32(out.Scalars.data(),count,nullptr,graphemes.data());
        set_linebreaks_utf32(out.Scalars.data(),count,"-strict",breaks.data());
    }
    out.Boundaries[0].Grapheme=true;
    for(uint32_t i=1;i<=count;++i) {
        auto& b=out.Boundaries[i];b.Grapheme=graphemes[i-1]==GRAPHEMEBREAK_BREAK;
        b.Mandatory=breaks[i-1]==LINEBREAK_MUSTBREAK;
        b.Line=b.Grapheme && (b.Mandatory || breaks[i-1]==LINEBREAK_ALLOWBREAK);
    }
    out.Boundaries[count].Grapheme=true;out.Boundaries[count].Line=true;
    for(auto cp:out.Scalars) {
        Require(cp!=0xad,"Discretionary soft hyphen layout is not supported in this foundation");
        Require(cp>=0x20 || cp=='\r' || cp=='\n' || cp=='\t',"Unsupported text control in CPU layout");
    }
    hb_font_extents_t extents{};
    Require(hb_font_get_h_extents(face->m_Data->Font,&extents),"Font has no horizontal extents");
    const float scale=options.EmSize/face->UnitsPerEm();
    Require(std::isfinite(options.Tracking) && std::abs(options.Tracking)<=512 && std::isfinite(options.LineSpacing) && std::abs(options.LineSpacing)<=512,"Invalid text spacing");
    const float lineHeight=(extents.ascender-extents.descender+extents.line_gap)*scale+options.LineSpacing;
    Require(std::isfinite(lineHeight) && lineHeight>0,"Invalid font line height");
    const auto language=hb_language_from_string(options.Language.c_str(),int(options.Language.size()));
    BidiMemory memory(m_Limits.BidiBytes);
    SBCodepointSequence sequence{SBStringEncodingUTF32,out.Scalars.data(),count};
    // Sheen rejects zero-length sequences. Empty text still has one caret line.
    if(!count) {
        out.Lines.push_back({0,0,0,0,0,extents.ascender*scale});out.Height=lineHeight;
    } else {
        Algorithm algorithm(SBAlgorithmCreate(&sequence));
        Scripts locator(SBScriptLocatorCreate());SBScriptLocatorLoadCodepoints(locator,&sequence);
        struct Script { uint32_t Begin,End,Tag; };
        std::vector<Script> scripts;
        while(SBScriptLocatorMoveNext(locator)) {
            auto a=SBScriptLocatorGetAgent(locator);auto tag=SBScriptGetUnicodeTag(a->script);
            Require(SupportedScript(tag),"Text script is outside the tested Latin/Greek/Cyrillic/Arabic/Hebrew foundation");
            Require(scripts.size()<m_Limits.Runs,"Text script run limit exceeded");
            scripts.push_back({uint32_t(a->offset),uint32_t(a->offset+a->length),tag});
        }
        struct Prepared { std::vector<Run> Runs;std::vector<Glyph> Glyphs;float Width=0; };
        auto shape=[&](SBParagraphRef paragraph,uint32_t begin,uint32_t end) {
            Prepared p;
            if(begin==end)return p;
            Require(end-begin<=m_Limits.ShapeScalars-workScalars,"Text contextual line-shaping scalar work limit exceeded");
            out.ShapeScalars+=end-begin;workScalars+=end-begin;
            BidiLine line(SBParagraphCreateLine(paragraph,begin,end-begin));
            const auto runs=SBLineGetRunsPtr(line);const auto runCount=SBLineGetRunCount(line);
            Require(runCount<=m_Limits.Runs,"Text bidi run limit exceeded");
            std::vector<Script> parts;
            for(size_t r=0;r<runCount;++r) {
                const auto br=runs[r];parts.clear();
                const auto first=std::lower_bound(scripts.begin(),scripts.end(),uint32_t(br.offset),
                    [](const Script& s,uint32_t offset){return s.End<=offset;});
                for(auto it=first;it!=scripts.end() && it->Begin<br.offset+br.length;++it) {
                    const auto& s=*it;
                    auto a=std::max(uint32_t(br.offset),s.Begin),b=std::min(uint32_t(br.offset+br.length),s.End);
                    if(a<b)parts.push_back({a,b,s.Tag});
                }
                if(br.level&1)std::reverse(parts.begin(),parts.end());
                for(const auto& s:parts) {
                    Require(p.Runs.size()<m_Limits.Runs,"Text shaped run limit exceeded");
                    Require(calls<m_Limits.ShapeCalls,"Text dependency shaping-call work limit exceeded");
                    ++out.ShapeCalls;++calls;
                    Require(out.Boundaries[s.Begin].Grapheme && out.Boundaries[s.End].Grapheme,"Script/direction boundary interrupts a grapheme; unsupported run segmentation");
                    Buffer buffer(hb_buffer_create());
                    hb_buffer_set_direction(buffer,(br.level&1)?HB_DIRECTION_RTL:HB_DIRECTION_LTR);
                    hb_buffer_set_script(buffer,hb_script_from_iso15924_tag(s.Tag));hb_buffer_set_language(buffer,language);
                    hb_buffer_set_cluster_level(buffer,HB_BUFFER_CLUSTER_LEVEL_MONOTONE_GRAPHEMES);
                    hb_buffer_set_flags(buffer,hb_buffer_flags_t((s.Begin==begin?HB_BUFFER_FLAG_BOT:0)|(s.End==end?HB_BUFFER_FLAG_EOT:0)));
                    // Context is THIS chosen line, not the original paragraph. This
                    // preserves joining across script/bidi boundaries, but never
                    // joins through a soft line break. Clusters remain global.
                    hb_buffer_add_codepoints(buffer,out.Scalars.data()+begin,int(end-begin),s.Begin-begin,int(s.End-s.Begin));
                    Require(hb_buffer_allocation_successful(buffer),"Cannot allocate shaping input");
                    auto info=hb_buffer_get_glyph_infos(buffer,nullptr);
                    for(uint32_t i=0;i<s.End-s.Begin;++i) {
                        info[i].cluster+=begin;
                        if(info[i].codepoint=='\t')info[i].codepoint=' ';
                        else if(info[i].codepoint==0x2028)info[i].codepoint=0x200b;
                    }
                    hb_segment_properties_t properties{};hb_buffer_get_segment_properties(buffer,&properties);
                    const char* shapers[]={"ot",nullptr};
                    // No cached shape plans: bounded caller results own all retained
                    // layout, rather than an unbounded font/language plan cache.
                    Plan plan(hb_shape_plan_create(face->m_Data->Face,&properties,nullptr,0,shapers));
                    Require(hb_shape_plan_execute(plan,face->m_Data->Font,buffer,nullptr,0) && hb_buffer_allocation_successful(buffer),"OpenType shaping failed or exceeded dependency limits");
                    unsigned n=0;info=hb_buffer_get_glyph_infos(buffer,&n);auto pos=hb_buffer_get_glyph_positions(buffer,nullptr);
                    Require(n<=m_Limits.Glyphs-p.Glyphs.size(),"Text glyph limit exceeded");
                    Run run{s.Begin,s.End,uint32_t(p.Glyphs.size()),n,s.Tag,uint8_t(br.level),p.Width,0};
                    std::vector<uint32_t> clusters;clusters.reserve(n+1);
                    for(unsigned i=0;i<n;++i)clusters.push_back(info[i].cluster);
                    clusters.push_back(s.End);std::sort(clusters.begin(),clusters.end());
                    clusters.erase(std::unique(clusters.begin(),clusters.end()),clusters.end());
                    for(unsigned i=0;i<n;++i) {
                        Require(info[i].cluster>=s.Begin && info[i].cluster<s.End,"Invalid shaped source cluster");
                        if(!info[i].codepoint && !options.PermitNotdef)throw std::runtime_error("Missing font coverage in cluster starting at scalar "+std::to_string(info[i].cluster)+"; no cross-family fallback in CPU foundation");
                        const auto next=*std::upper_bound(clusters.begin(),clusters.end(),info[i].cluster);
                        Glyph glyph{info[i].codepoint,info[i].cluster,next,p.Width+run.Width,0,
                            pos[i].x_advance*scale,-pos[i].y_advance*scale,pos[i].x_offset*scale,-pos[i].y_offset*scale};
                        if(out.Scalars[info[i].cluster]=='\t')glyph.AdvanceX*=4; // fixed four-space advance, not tab stops
                        Require(std::isfinite(glyph.AdvanceX) && glyph.AdvanceX>=0,"Unsupported negative/nonfinite horizontal glyph advance");
                        // Tracking never separates the glyphs of one HB cluster. Joining
                        // Arabic runs accept tracking only on whitespace, retaining joins.
                        const bool last=i+1==n || info[i+1].cluster!=info[i].cluster;
                        if(last && (s.Tag!=HB_TAG('A','r','a','b') || out.Scalars[info[i].cluster]==' '))
                            glyph.AdvanceX=std::max(0.f,glyph.AdvanceX+options.Tracking);
                        run.Width+=glyph.AdvanceX;p.Glyphs.push_back(glyph);
                    }
                    p.Width+=run.Width;p.Runs.push_back(run);
                }
            }
            return p;
        };
        auto publish=[&](uint32_t begin,uint32_t end,Prepared&& p,bool emergency) {
            Require(out.Lines.size()<m_Limits.Lines && p.Runs.size()<=m_Limits.Runs-out.Runs.size() &&
                p.Glyphs.size()<=m_Limits.Glyphs-out.Glyphs.size(),"Text output line/run/glyph limit exceeded");
            const float baseline=out.Lines.size()*lineHeight+extents.ascender*scale;
            out.Lines.push_back({begin,end,uint32_t(out.Runs.size()),uint32_t(p.Runs.size()),p.Width,baseline,
                options.Width && p.Width>*options.Width,emergency});
            for(auto run:p.Runs) { run.FirstGlyph+=uint32_t(out.Glyphs.size());out.Runs.push_back(run); }
            for(auto glyph:p.Glyphs) { glyph.Y=baseline;out.Glyphs.push_back(glyph); }
            out.Width=std::max(out.Width,p.Width);
        };
        const SBLevel base=options.BaseDirection==Direction::Auto?SBLevelDefaultLTR:(options.BaseDirection==Direction::RTL?1:0);
        for(uint32_t paragraphBegin=0;paragraphBegin<count;) {
            SBUInteger length=0,separator=0;
            SBAlgorithmGetParagraphBoundary(algorithm,paragraphBegin,count-paragraphBegin,&length,&separator);
            Require(length && length<=count-paragraphBegin,"Invalid bidi paragraph boundary");
            const auto paragraphEnd=paragraphBegin+uint32_t(length),textEnd=paragraphEnd-uint32_t(separator);
            BidiParagraph paragraph(SBAlgorithmCreateParagraph(algorithm,paragraphBegin,length,base));
            out.Paragraphs.push_back({paragraphBegin,textEnd,paragraphEnd,uint8_t(SBParagraphGetBaseLevel(paragraph))});
            if(paragraphBegin==textEnd)publish(textEnd,textEnd,{},false);
            uint32_t emergencyEnd=paragraphBegin;
            for(uint32_t start=paragraphBegin;start<textEnd;) {
                uint32_t hardEnd=textEnd;
                for(auto i=start+1;i<textEnd;++i)if(out.Boundaries[i].Mandatory){hardEnd=i;break;}
                uint32_t chosen=hardEnd;Prepared accepted;
                bool emergency=false,haveFit=false;
                if(!options.Width || !options.Wrap)accepted=shape(paragraph,start,hardEnd);
                else if(emergencyEnd>start) {
                    const auto end=std::min(hardEnd,emergencyEnd);
                    for(auto j=start+1;j<=end;++j) {
                        if(!out.Boundaries[j].Grapheme)continue;
                        auto piece=shape(paragraph,start,j);
                        if(piece.Width<=*options.Width || !haveFit) { chosen=j;accepted=std::move(piece);haveFit=true; }
                        else break;
                        if(accepted.Width>*options.Width)break;
                    }
                    emergency=chosen<end;
                } else {
                    // Greedy permitted boundaries with freshly shaped line
                    // context; stop at the first overflowing candidate. Bounded
                    // emergency wrapping only when the first word cannot fit.
                    for(auto i=start+1;i<=hardEnd;++i) {
                        if(i<hardEnd && !out.Boundaries[i].Line)continue;
                        auto candidate=shape(paragraph,start,i);
                        if(candidate.Width<=*options.Width) { chosen=i;accepted=std::move(candidate);haveFit=true; }
                        else {
                            if(!haveFit) {
                                candidate={}; // release full-word geometry before emergency trials
                                emergencyEnd=i;
                                for(auto j=start+1;j<=i;++j) {
                                    if(!out.Boundaries[j].Grapheme)continue;
                                    auto piece=shape(paragraph,start,j);
                                    if(piece.Width<=*options.Width || !haveFit) { chosen=j;accepted=std::move(piece);haveFit=true; }
                                    else break;
                                    if(accepted.Width>*options.Width)break;
                                }
                                emergency=chosen<i;
                            }
                            break;
                        }
                    }
                }
                Require(chosen>start,"Text line arrangement made no progress");
                publish(start,chosen,std::move(accepted),emergency);start=chosen;
            }
            // A trailing paragraph separator starts a final empty caret line.
            if(paragraphEnd==count && separator)publish(count,count,{},false);
            paragraphBegin=paragraphEnd;
        }
        out.Height=out.Lines.size()*lineHeight;
    }
    return result;
}
Ref<const Layout> Service::Arrange(std::string_view source,const Ref<const Face>& face,const Options& options) {
    CheckThread();
    Require(!options.Height || (std::isfinite(*options.Height) && *options.Height>=0 && *options.Height<=1e7f),"Invalid text available height");
    Require(options.OverflowMode==Overflow::Clip || options.OverflowMode==Overflow::Ellipsis,"Invalid text overflow mode");
    Require(options.MaxLines<=m_Limits.Lines,"Text MaxLines exceeds layout line limit");
    Admit(m_Budget->Live.Results<m_Limits.Results,"CPU layout result count exhausted; release inactive results");
    uint32_t calls=0,workScalars=0;
    auto result=Build(source,face,options,calls,workScalars);auto& out=*result;
    out.OriginalLines=uint32_t(out.Lines.size());
    const float lineHeight=face->LineHeightEm()*options.EmSize+options.LineSpacing;
    const float boxHeight=face->HeightEm()*options.EmSize;
    Require(std::isfinite(boxHeight) && boxHeight>0,"Invalid font-height text box");
    size_t keep=out.Lines.size();
    if(options.MaxLines)keep=std::min(keep,size_t(options.MaxLines));
    if(options.OverflowMode==Overflow::Ellipsis && options.Height) {
        size_t fit=0;
        // Normalize/scale/arrange each round floats. Admit a four-ULP edge
        // tolerance so Content's own font-height box does not lose its last line.
        // Exact zero remains empty; width/ellipsis candidates still fit strictly.
        float edge=*options.Height;
        if(edge>0)for(int i=0;i<4;++i)edge=std::nextafter(edge,std::numeric_limits<float>::infinity());
        if(edge>=boxHeight) {
            const double additional=(double(edge)-boxHeight)/lineHeight;
            fit=additional>=double(out.Lines.size())?out.Lines.size():1+size_t(std::floor(additional));
        }
        keep=std::min(keep,fit);
    }
    // Only published geometry requests pages. The authored source and all its
    // original scalar/byte/UTF-16/grapheme boundaries remain intact.
    auto trim=[&](size_t n) {
        out.Lines.resize(n);
        size_t runs=n?out.Lines.back().FirstRun+out.Lines.back().RunCount:0;
        out.Runs.resize(runs);
        out.Glyphs.resize(runs?out.Runs.back().FirstGlyph+out.Runs.back().GlyphCount:0);
    };
    const bool omittedLines=keep<out.Lines.size() && !out.Scalars.empty();out.Omitted=omittedLines;trim(keep);
    if(!keep && omittedLines && options.OverflowMode==Overflow::Ellipsis)out.MarkerDoesNotFit=true;
    for(size_t index=0;index<out.Lines.size();++index) {
        auto& line=out.Lines[index];
        const bool omittedTail=index+1==keep && omittedLines;
        const bool tooWide=options.Width && line.Width>*options.Width;
        if(options.OverflowMode!=Overflow::Ellipsis || (!omittedTail && !tooWide))continue;
        out.Omitted=true;
        // Arrange the complete retained logical line plus U+2026 afresh. This
        // reruns bidi, scripts and contextual shaping rather than splicing quads.
        Options candidateOptions=options;candidateOptions.Width.reset();candidateOptions.Height.reset();
        candidateOptions.MaxLines=0;candidateOptions.OverflowMode=Overflow::Clip;
        for(const auto& paragraph:out.Paragraphs)if(line.Begin>=paragraph.Begin && line.Begin<=paragraph.End) {
            candidateOptions.BaseDirection=paragraph.Level&1?Direction::RTL:Direction::LTR;break;
        }
        const float available=options.Width.value_or(1e7f);
        Ref<Layout> marker;
        try{marker=Build(u8"…",face,candidateOptions,calls,workScalars);}
        catch(const std::exception& error){throw std::runtime_error(std::string("Ellipsis U+2026 marker: ")+error.what());}
        const bool markerFits=marker->Width<=available;
        Ref<Layout> accepted;uint32_t retained=line.Begin;
        if(markerFits) {
            std::vector<uint32_t> ends;
            uint32_t markerEnd=line.End;
            while(markerEnd>line.Begin && out.Scalars[markerEnd-1]==0x2028)--markerEnd;
            // A width-only marker must replace at least one authored grapheme.
            if(!omittedTail && markerEnd==line.End && markerEnd>line.Begin) {
                --markerEnd;while(markerEnd>line.Begin && !out.Boundaries[markerEnd].Grapheme)--markerEnd;
            }
            for(uint32_t end=line.Begin;end<=markerEnd;++end)if(out.Boundaries[end].Grapheme)ends.push_back(end);
            std::vector<uint32_t> candidates;
            if(ends.size()<=128)candidates=ends;
            else {
                out.ConservativeTruncation=true;
                // Predict a neighborhood using existing logical cluster advances;
                // it is a hint only. Every accepted candidate is independently
                // reshaped and measured; no monotonic-prefix assumption is made.
                std::vector<float> advance(line.End-line.Begin+1,0);
                for(uint32_t r=line.FirstRun;r<line.FirstRun+line.RunCount;++r)
                    for(uint32_t g=out.Runs[r].FirstGlyph;g<out.Runs[r].FirstGlyph+out.Runs[r].GlyphCount;++g)
                        advance[out.Glyphs[g].ClusterBegin-line.Begin]+=out.Glyphs[g].AdvanceX;
                float width=0;uint32_t estimate=line.Begin;
                for(uint32_t i=line.Begin;i<line.End;++i){width+=advance[i-line.Begin];if(width+marker->Width<=available)estimate=i+1;}
                auto at=size_t(std::lower_bound(ends.begin(),ends.end(),estimate)-ends.begin());
                size_t first=at>48?at-48:0,last=std::min(ends.size(),at+49);
                candidates.insert(candidates.end(),ends.begin()+first,ends.begin()+last);
                for(size_t n=ends.size()-1;n;n/=2)candidates.push_back(ends[n]);
                candidates.push_back(line.Begin);candidates.push_back(markerEnd);
                std::sort(candidates.begin(),candidates.end());candidates.erase(std::unique(candidates.begin(),candidates.end()),candidates.end());
            }
            Require(candidates.size()<=128,"Ellipsis candidate work limit exceeded");
            for(auto it=candidates.rbegin();it!=candidates.rend();++it) {
                if(*it==line.Begin){accepted=std::move(marker);retained=line.Begin;break;}
                const auto first=out.Boundaries[line.Begin].Byte,last=out.Boundaries[*it].Byte;
                auto candidateSource=out.Source.substr(first,last-first)+u8"…";
                // The public decoder ignores one document-leading BOM. A FEFF
                // retained at a line start is instead an interior format scalar;
                // protect it with an ignored synthetic BOM in this private trial.
                if(candidateSource.compare(0,3,"\xef\xbb\xbf")==0)candidateSource.insert(0,"\xef\xbb\xbf");
                if(candidateSource.size()>m_Limits.InputBytes || *it-line.Begin>=m_Limits.Scalars)continue;
                auto candidate=Build(candidateSource,face,candidateOptions,calls,workScalars);
                if(candidate->Width<=available){accepted=std::move(candidate);retained=*it;break;}
            }
        }
        // Rebuild compact output below: no discarded tail pins atlas pages.
        const auto oldRuns=line.RunCount;
        std::vector<Run> replacementRuns;std::vector<Glyph> replacementGlyphs;
        if(accepted) {
            replacementRuns=std::move(accepted->Runs);replacementGlyphs=std::move(accepted->Glyphs);
            const uint32_t prefix=retained-line.Begin;
            for(auto& run:replacementRuns){run.ContainsGenerated=run.End>prefix;run.Begin=line.Begin+std::min(run.Begin,prefix);run.End=line.Begin+std::min(run.End,prefix);}
            for(auto& glyph:replacementGlyphs){
                glyph.Y=line.Baseline;
                if(glyph.ClusterBegin>=prefix){glyph.Generated=true;glyph.ClusterBegin=glyph.ClusterEnd=UINT32_MAX;}
                else {Require(glyph.ClusterEnd<=prefix,"Ellipsis marker merged with an authored cluster");glyph.ClusterBegin+=line.Begin;glyph.ClusterEnd+=line.Begin;}
            }
            line.Width=accepted->Width;
        } else {line.Width=0;out.MarkerDoesNotFit=true;}
        const uint32_t firstGlyph=line.FirstRun<out.Runs.size()?out.Runs[line.FirstRun].FirstGlyph:uint32_t(out.Glyphs.size());
        const uint32_t endGlyph=oldRuns?out.Runs[line.FirstRun+oldRuns-1].FirstGlyph+out.Runs[line.FirstRun+oldRuns-1].GlyphCount:firstGlyph;
        const int64_t glyphDelta=int64_t(replacementGlyphs.size())-(endGlyph-firstGlyph),runDelta=int64_t(replacementRuns.size())-oldRuns;
        for(auto& run:replacementRuns)run.FirstGlyph+=firstGlyph;
        out.Glyphs.erase(out.Glyphs.begin()+firstGlyph,out.Glyphs.begin()+endGlyph);
        out.Glyphs.insert(out.Glyphs.begin()+firstGlyph,replacementGlyphs.begin(),replacementGlyphs.end());
        out.Runs.erase(out.Runs.begin()+line.FirstRun,out.Runs.begin()+line.FirstRun+oldRuns);
        out.Runs.insert(out.Runs.begin()+line.FirstRun,replacementRuns.begin(),replacementRuns.end());
        for(size_t r=line.FirstRun+replacementRuns.size();r<out.Runs.size();++r)out.Runs[r].FirstGlyph=uint32_t(int64_t(out.Runs[r].FirstGlyph)+glyphDelta);
        for(size_t l=index+1;l<out.Lines.size();++l)out.Lines[l].FirstRun=uint32_t(int64_t(out.Lines[l].FirstRun)+runDelta);
        line.RunCount=uint32_t(replacementRuns.size());line.End=retained;line.Ellipsized=true;line.Overflow=false;
        Require(out.Runs.size()<=m_Limits.Runs && out.Glyphs.size()<=m_Limits.Glyphs,"Ellipsis output limit exceeded");
    }
    out.Width=0;for(const auto& line:out.Lines)out.Width=std::max(out.Width,line.Width);
    out.Height=out.Lines.size()*lineHeight;
    if(out.Omitted) {
        // Keep source maps, not unused shaped tail capacity, in retained results.
        auto compact=[](auto& values){using Values=std::decay_t<decltype(values)>;if(values.capacity()>std::max(size_t(64),values.size()*2))Values(values.begin(),values.end()).swap(values);};
        compact(out.Glyphs);compact(out.Runs);compact(out.Lines);
    }
    out.ShapeCalls=calls;out.ShapeScalars=workScalars;
    for(const auto& glyph:out.Glyphs){out.MissingGlyphs+=glyph.ID==0;out.MissingMarkerGlyphs+=glyph.Generated && glyph.ID==0;}
    const auto bytes=Bytes(out);
    Admit(bytes<=m_Limits.ResultBytes-m_Budget->Live.ResultBytes,"CPU text result retention exhausted; release inactive results");
    out.m_Bytes=bytes;out.m_Budget=m_Budget;++m_Budget->Live.Results;m_Budget->Live.ResultBytes+=bytes;
    return result;
}
}
