// CPU-only service integration plus independent Unicode 17 conformance data.
#include "Hazel/Text/TextLayout.h"
#include "Hazel/Utils/UTF8.h"
#include <hb.h>
#include <hb-ot.h>
#include <graphemebreak.h>
#include <linebreak.h>
#include <SheenBidi/SheenBidi.h>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <type_traits>

using namespace Hazel;
using namespace Hazel::Text;
static_assert(!std::is_copy_constructible<Layout>::value,"Resource-accounted layouts must be shared, never value-copied");
static void Check(bool value,const char* message) { if(!value)throw std::runtime_error(message); }
template<class F> static void Reject(F function,const char* message) {
    bool rejected=false;try{function();}catch(const std::exception&){rejected=true;}Check(rejected,message);
}
static std::vector<uint8_t> Read(const std::filesystem::path& path) {
    std::ifstream input(path,std::ios::binary|std::ios::ate);Check(bool(input),"Missing typography fixture");
    auto n=input.tellg();Check(n>0 && n<=4*1024*1024,"Invalid fixture size");
    std::vector<uint8_t> bytes(size_t(n),0);input.seekg(0);input.read(reinterpret_cast<char*>(bytes.data()),n);Check(bool(input),"Fixture read failed");return bytes;
}
static std::vector<uint32_t> Numbers(const std::string& text,int base=10) {
    std::istringstream input(text);std::string token;std::vector<uint32_t> result;
    while(input>>token)result.push_back(uint32_t(std::stoul(token,nullptr,base)));
    return result;
}
static void BreakConformance(const std::filesystem::path& file,bool grapheme) {
    std::ifstream input(file);Check(bool(input),"Missing Unicode break tests");std::string line;size_t cases=0;
    while(std::getline(input,line)) {
        line=line.substr(0,line.find('#'));std::istringstream tokens(line);std::string token;
        std::vector<bool> expected;std::vector<uint32_t> cps;
        while(tokens>>token) {
            if(token==u8"÷" || token==u8"×")expected.push_back(token==u8"÷");
            else cps.push_back(uint32_t(std::stoul(token,nullptr,16)));
        }
        if(cps.empty())continue;
        Check(expected.size()==cps.size()+1,"Invalid conformance format");std::vector<char> actual(cps.size());
        if(grapheme)set_graphemebreaks_utf32(cps.data(),cps.size(),nullptr,actual.data());
        else set_linebreaks_utf32(cps.data(),cps.size(),"-strict",actual.data());
        for(size_t i=0;i<cps.size();++i) {
            bool permitted=grapheme?actual[i]==GRAPHEMEBREAK_BREAK:(actual[i]==LINEBREAK_MUSTBREAK || actual[i]==LINEBREAK_ALLOWBREAK);
            // libunibreak leaves non-EOL EOF indeterminate for streaming use.
            // A complete text's EOF is a boundary, as required by UAX14 LB3.
            if(!grapheme && i+1==cps.size() && actual[i]==LINEBREAK_INDETERMINATE)permitted=true;
            if(permitted!=expected[i+1])throw std::runtime_error(file.filename().string()+" case "+std::to_string(cases)+" boundary "+std::to_string(i+1));
        }
        ++cases;
    }
    Check(cases>500,"Conformance fixture unexpectedly incomplete");std::cout<<"PASS: "<<file.filename()<<" "<<cases<<" cases\n";
}
static void BidiConformance(const std::filesystem::path& file) {
    std::ifstream input(file);Check(bool(input),"Missing bidi conformance");std::string line;size_t cases=0;
    while(std::getline(input,line)) {
        if(line.empty() || line[0]=='#')continue;
        std::vector<std::string> fields;std::istringstream values(line);std::string part;
        while(std::getline(values,part,';'))fields.push_back(part);
        Check(fields.size()==5,"Invalid bidi conformance format");const auto cps=Numbers(fields[0],16);
        SBCodepointSequence sequence{SBStringEncodingUTF32,cps.data(),cps.size()};
        auto algorithm=SBAlgorithmCreate(&sequence);Check(algorithm,"Bidi algorithm failed");
        const auto base=std::stoi(fields[1]);auto paragraph=SBAlgorithmCreateParagraph(algorithm,0,cps.size(),base==2?SBLevelDefaultLTR:base);
        Check(paragraph,"Bidi paragraph failed");Check(SBParagraphGetBaseLevel(paragraph)==std::stoi(fields[2]),"Bidi paragraph level mismatch");
        auto visual=SBParagraphCreateLine(paragraph,0,cps.size());Check(visual,"Bidi line failed");
        auto runs=SBLineGetRunsPtr(visual);std::vector<uint8_t> levels(cps.size());
        for(size_t r=0;r<SBLineGetRunCount(visual);++r)
            std::fill(levels.begin()+runs[r].offset,levels.begin()+runs[r].offset+runs[r].length,runs[r].level);
        std::istringstream levelInput(fields[3]);std::string token;std::vector<bool> retained;
        while(levelInput>>token) {
            const auto i=retained.size();retained.push_back(token!="x");
            if(token!="x" && (i>=cps.size() || levels[i]!=std::stoi(token)))
                throw std::runtime_error("Bidi case "+std::to_string(cases)+" scalar "+std::to_string(i)+" actual "+std::to_string(levels[i])+" expected "+token+"; "+fields[0]);
        }
        Check(retained.size()==cps.size(),"Invalid bidi level test");
        std::vector<uint32_t> indices;
        for(size_t r=0;r<SBLineGetRunCount(visual);++r) {
            const auto run=runs[r];
            for(size_t j=0;j<run.length;++j) {
                const auto i=uint32_t((run.level&1)?run.offset+run.length-1-j:run.offset+j);
                if(retained[i])indices.push_back(i);
            }
        }
        Check(indices==Numbers(fields[4]),"Bidi visual source order mismatch");
        SBLineRelease(visual);SBParagraphRelease(paragraph);SBAlgorithmRelease(algorithm);++cases;
    }
    Check(cases>50000,"Bidi conformance unexpectedly incomplete");std::cout<<"PASS: BidiCharacterTest "<<cases<<" cases\n";
}
static std::vector<uint32_t> IDs(const Ref<const Layout>& layout) {
    std::vector<uint32_t> result;for(const auto& g:layout->Glyphs)result.push_back(g.ID);return result;
}
#include "TextOverflowChecks.h"
static void ServiceChecks(const std::filesystem::path& root) {
    const auto bytes=Read(root/"DejaVuSans.ttf");Service service;auto face=service.CreateFace(bytes,"DejaVuSans-2.37-pinned");
    // Independent reference IDs come from cmap entries for explicit presentation
    // forms, not the service's own shaping or another copy of its layout loop.
    auto blob=hb_blob_create(reinterpret_cast<const char*>(bytes.data()),unsigned(bytes.size()),HB_MEMORY_MODE_READONLY,nullptr,nullptr);
    auto reference=hb_face_create(blob,0);hb_blob_destroy(blob);
    auto cmap=hb_font_create(reference);hb_ot_font_set_funcs(cmap);
    auto nominal=[&](uint32_t cp){hb_codepoint_t glyph=0;Check(hb_font_get_nominal_glyph(cmap,cp,&glyph),"Reference cmap missing");return glyph;};
    {
        auto ffi=service.Arrange("ffi",face);Check(ffi->Glyphs.size()==1 && ffi->Glyphs[0].ID==nominal(0xfb03),"ffi ligature reference mismatch");
        Check(ffi->Glyphs[0].ClusterBegin==0 && ffi->Glyphs[0].ClusterEnd==3,"Ligature source range lost");
        auto composed=service.Arrange(u8"é",face),decomposed=service.Arrange(u8"e\u0301",face);
        Check(IDs(composed)==IDs(decomposed) && std::abs(composed->Width-decomposed->Width)<1e-6f,"Canonical accent normalization mismatch");
        Check(!decomposed->Boundaries[1].Grapheme && decomposed->Glyphs[0].ClusterEnd==2,"Combining boundary/cluster mismatch");
        auto multi=service.Arrange(u8"q\u0301",face);Check(multi->Glyphs.size()==2,"Expected multiple glyphs in combining cluster");
        for(const auto& g:multi->Glyphs)Check(g.ClusterBegin==0 && g.ClusterEnd==2,"Multiple-glyph cluster split");
        auto joined=service.Arrange(u8"بب",face);
        Check(IDs(joined)==std::vector<uint32_t>{nominal(0xfe90),nominal(0xfe91)},"Arabic final/initial joining reference mismatch");
        Check(joined->Glyphs[0].ClusterBegin==1 && joined->Glyphs[1].ClusterBegin==0,"Arabic visual/logical order mismatch");
        Check(joined->Paragraphs[0].Level==1,"Automatic Arabic paragraph direction mismatch");
        Options rtl;rtl.BaseDirection=Direction::RTL;
        Check(IDs(service.Arrange("(",face,rtl))==std::vector<uint32_t>{nominal(')')},"RTL punctuation mirroring mismatch");
        auto ligature=service.Arrange(u8"لا",face);Check(IDs(ligature)==std::vector<uint32_t>{nominal(0xfefb)},"Arabic lam-alef reference mismatch");
        auto mixed=service.Arrange(u8"abc אבג 12",face);std::vector<uint32_t> order;
        for(const auto& g:mixed->Glyphs)order.push_back(g.ClusterBegin);
        Check(order==std::vector<uint32_t>{0,1,2,3,8,9,7,6,5,4},"Mixed Hebrew/Latin/numbers independent visual order mismatch");
        auto mapping=service.Arrange(u8"\ufeffAé😀",face);
        Check(mapping->Boundaries[0].Byte==3 && mapping->Boundaries[0].UTF16==1 &&
            mapping->Boundaries[3].Byte==10 && mapping->Boundaries[3].UTF16==5,"UTF8/UTF16 source boundary mismatch");
        auto empty=service.Arrange("",face);Check(empty->Glyphs.empty() && empty->Lines.size()==1 && empty->Height>0,"Empty caret line lost");
        Options scaled;scaled.EmSize=2;
        auto twice=service.Arrange(u8"é",face,scaled);Check(twice->Width==composed->Width*2 && twice->Height==composed->Height*2,"Em-unit scaling mismatch");
        auto paragraphs=service.Arrange("a\r\nb\n",face);Check(paragraphs->Paragraphs.size()==2 && paragraphs->Lines.size()==3,"CRLF/trailing empty line mismatch");
        auto tab=service.Arrange("\t",face),spaces=service.Arrange("    ",face);Check(tab->Width==spaces->Width,"Fixed four-space tab contract mismatch");
        auto nb=service.Arrange(u8"a\u00a0b",face);Check(!nb->Boundaries[1].Line && !nb->Boundaries[2].Line,"NBSP incorrectly permits a line break");
    }
    Options narrow;narrow.Width=service.Arrange("ab ",face)->Width;
    auto wrapped=service.Arrange("ab cd",face,narrow);Check(wrapped->Lines.size()==2 && wrapped->Lines[0].End==3,"Greedy permitted line break mismatch");
    for(const auto& line:wrapped->Lines)Check(line.Width<=*narrow.Width && !line.Overflow,"Wrapped width constraint violated");
    {
        Options zero;zero.Width=0;auto result=service.Arrange(u8"e\u0301b",face,zero);
        Check(result->Lines.size()==2 && result->Lines[0].End==2 && result->Lines[0].Overflow,"Zero width split a grapheme or lost overflow");
        // Emergency line boundaries must reshape Arabic forms, not slice a
        // paragraph shaped as an unbroken joining run.
        auto one=service.Arrange(u8"ب",face);Options size;size.Width=one->Width+0.001f;
        auto lines=service.Arrange(u8"بب",face,size);
        Check(lines->Lines.size()==2 && lines->Glyphs.size()==2,"Arabic emergency line fixture did not wrap");
        for(const auto& glyph:lines->Glyphs)Check(glyph.ID==nominal(0x628),"Arabic line boundary retained cross-line joining");
    }
    // Representative authored corpus: every line must shape with complete
    // coverage, and width-constrained output is deterministic, not a rendering claim.
    std::ifstream corpus(root/"corpus.tsv");Check(bool(corpus),"Missing tested corpus");std::string line;size_t cases=0;
    while(std::getline(corpus,line)) {
        if(line.empty() || line[0]=='#')continue;
        auto tab=line.find('\t');Check(tab!=std::string::npos,"Bad corpus row");
        Options options;options.Language=line.substr(0,tab);options.Width=12;
        auto a=service.Arrange(line.substr(tab+1),face,options),b=service.Arrange(line.substr(tab+1),face,options);
        Check(IDs(a)==IDs(b) && a->Width==b->Width && a->Lines.size()==b->Lines.size(),"Nondeterministic corpus glyphs/line layout");
        for(size_t i=0;i<a->Glyphs.size();++i) {
            const auto& x=a->Glyphs[i];const auto& y=b->Glyphs[i];
            Check(x.X==y.X && x.Y==y.Y && x.OffsetX==y.OffsetX && x.OffsetY==y.OffsetY && x.ClusterBegin==y.ClusterBegin && x.ClusterEnd==y.ClusterEnd,"Nondeterministic positioned corpus");
        }
        for(const auto& l:a->Lines)Check(a->Boundaries[l.Begin].Grapheme && a->Boundaries[l.End].Grapheme,"Corpus line split a grapheme");
        ++cases;
    }
    Check(cases>=10,"Corpus incomplete");
    auto longWord=service.Arrange(std::string(4096,'W'),face,narrow);Check(longWord->Lines.size()>100,"Long word emergency wrapping missing");
    for(const auto& bad:std::vector<std::string>{"\xc0\xaf","\xed\xa0\x80","\xf4\x90\x80\x80","\xe2\x82",std::string("a\0b",3)})
        Reject([&]{service.Arrange(bad,face);},"Invalid scalar input accepted");
    Reject([&]{service.Arrange(std::string(65537,'a'),face);},"Input cap ignored");
    Reject([&]{service.Arrange(std::string(16385,'a'),face);},"Scalar cap ignored");
    Reject([&]{service.Arrange(u8"漢",face);},"Untested script accepted");
    // Arabic alef with attached fatha is absent in this pinned face. Its known
    // script is supported, but its missing glyph must reject.
    hb_codepoint_t absent=0;Check(!hb_font_get_nominal_glyph(cmap,0x0870,&absent),"Missing-coverage reference unexpectedly present");
    bool missing=false;
    try{service.Arrange(u8"\u0870",face);}catch(const std::exception& error){missing=std::string(error.what()).find("Missing font coverage")!=std::string::npos;}
    Check(missing,"Absent coverage reported as successful shaping or confused with unsupported script");
    Reject([&]{service.Arrange(u8"a\u00adb",face);},"Unimplemented discretionary hyphen silently accepted");
    Reject([&]{service.CreateFace({1,2,3},"corrupt");},"Corrupt CPU face accepted");
    Reject([&]{service.CreateFace(std::vector<uint8_t>(4*1024*1024+1),"large");},"Font input cap ignored");
    Options invalid;invalid.Width=-1;Reject([&]{service.Arrange("a",face,invalid);},"Negative width accepted");
    invalid={};invalid.Language="xx-private";Reject([&]{service.Arrange("a",face,invalid);},"Unbounded language intern admitted");
    bool wrongThread=false;std::thread worker([&]{try{service.Arrange("a",face);}catch(const std::exception&){wrongThread=true;}});worker.join();Check(wrongThread,"Owner-thread contract not enforced");
    {
        Limits limits;limits.Faces=1;limits.Results=1;Service small(limits);auto pin=small.CreateFace(bytes,"pinned");
        Reject([&]{small.CreateFace(bytes,"new");},"Live CPU face cap ignored");
        auto result=small.Arrange("pin",pin);pin.reset();Check(small.Usage().Faces==1,"Layout failed to pin face");
        Reject([&]{small.Arrange("another",result->Font);},"Live result cap ignored");
        result.reset();Check(small.Usage().Faces==0 && small.Usage().Results==0 && small.Usage().FaceBytes==0 && small.Usage().ResultBytes==0,"CPU resource retirement leaked");
        auto recovered=small.CreateFace(bytes,"recovered");Check(recovered!=nullptr,"Admission did not recover");
    }
    for(const auto kind:{0,1,2,3,4,5,6}) {
        Limits limits;
        if(kind==0)limits.ShapeScalars=1;
        if(kind==1)limits.Glyphs=1;
        if(kind==2)limits.Runs=1;
        if(kind==3)limits.Lines=1;
        if(kind==4)limits.BidiBytes=1;
        if(kind==5)limits.ResultBytes=1;
        if(kind==6)limits.ShapeCalls=1;
        Service small(limits);Options options;options.Width=0;
        Reject([&]{small.Arrange(u8"a אב",face,options);},"Configured CPU work/output limit ignored");
        Check(small.Usage().Results==0 && small.Usage().ResultBytes==0,"Rejected layout published partial result");
    }
    Limits bytesLimit;bytesLimit.FaceBytes=1;Service tiny(bytesLimit);Reject([&]{tiny.CreateFace(bytes,"blocked");},"CPU face-byte cap ignored");
    Check(service.Arrange("recovery",face)!=nullptr,"Dependency allocator/work failure poisoned subsequent layout");
    Ref<const Layout> survivor;
    {Service temporary;survivor=temporary.Arrange("retained",temporary.CreateFace(bytes,"retired owner"));}
    Check(survivor->Font && !survivor->Glyphs.empty(),"Result invalid after service retirement");survivor.reset();
    OverflowChecks(service,face,cmap);
    hb_font_destroy(cmap);hb_face_destroy(reference);
    std::cout<<"PASS: CPU typography corpus "<<cases<<", analytic cmap/cluster/bidi/line references, strict source maps, limits and retirement\n";
}
#include "TextEditingChecks.h"
#include "TextEmphasisChecks.h"
int main(int argc,char** argv) {
    try {
        const auto root=argc>1?std::filesystem::u8path(argv[1]):std::filesystem::path("typography");
        BreakConformance(root/"GraphemeBreakTest.txt",true);BreakConformance(root/"LineBreakTest.txt",false);
        BidiConformance(root/"BidiCharacterTest.txt");ServiceChecks(root);TextEditingChecks(root);TextEmphasisChecks(root);return 0;
    } catch(const std::exception& error) {std::cerr<<"FAIL: "<<error.what()<<'\n';return 1;}
}
