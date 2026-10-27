#include "hzpch.h"
#include "TextSource.h"
#include "Hazel/Utils/UTF8.h"
#include <graphemebreak.h>
#include <linebreak.h>
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace Hazel::Text
{
namespace {void Check(bool ok,const char* message){if(!ok)throw std::invalid_argument(message);}}
SourceMap SourceMap::Analyze(std::string_view text)
{
    Check(text.size()<=65536,"Display text exceeds 65536 UTF-8 bytes");
    SourceMap out;std::vector<size_t> offsets;UTF8::Decode(text,out.Scalars,&offsets);
    Check(out.Scalars.size()<=16384,"Display text exceeds 16384 scalars");
    const auto n=out.Scalars.size();out.Boundaries.resize(n+1);
    uint32_t utf16=text.substr(0,3)=="\xef\xbb\xbf"?1:0;
    for(size_t i=0;i<=n;++i){out.Boundaries[i].Byte=uint32_t(offsets[i]);out.Boundaries[i].UTF16=utf16;if(i<n)utf16+=out.Scalars[i]>0xffff?2:1;}
    std::vector<char> graphemes(n),lines(n);
    if(n){set_graphemebreaks_utf32(out.Scalars.data(),n,nullptr,graphemes.data());set_linebreaks_utf32(out.Scalars.data(),n,"-strict",lines.data());}
    out.Boundaries[0].Grapheme=true;
    for(size_t i=1;i<=n;++i){auto& b=out.Boundaries[i];b.Grapheme=graphemes[i-1]==GRAPHEMEBREAK_BREAK;b.Mandatory=lines[i-1]==LINEBREAK_MUSTBREAK;b.Line=b.Grapheme&&(b.Mandatory||lines[i-1]==LINEBREAK_ALLOWBREAK);}
    out.Boundaries[n].Grapheme=true;out.Boundaries[n].Line=true;return out;
}
uint32_t SourceMap::Scalar(uint32_t offset,SourceUnit unit,bool grapheme) const
{
    Check(unsigned(unit)<=unsigned(SourceUnit::Grapheme),"Invalid source range units");
    uint32_t scalar=UINT32_MAX;
    if(unit==SourceUnit::Scalar)scalar=offset;
    else if(unit==SourceUnit::Grapheme){uint32_t index=0;for(uint32_t i=0;i<Boundaries.size();++i)if(Boundaries[i].Grapheme){if(index++==offset){scalar=i;break;}}}
    else {auto it=std::lower_bound(Boundaries.begin(),Boundaries.end(),offset,[&](const auto& b,uint32_t value){return (unit==SourceUnit::UTF8Byte?b.Byte:b.UTF16)<value;});if(it!=Boundaries.end()&&(unit==SourceUnit::UTF8Byte?it->Byte:it->UTF16)==offset)scalar=uint32_t(it-Boundaries.begin());}
    Check(scalar<Boundaries.size(),"Range endpoint is not a source boundary (including BOM/surrogate rules)");
    Check(!grapheme||Boundaries[scalar].Grapheme,"Range endpoint splits an extended grapheme");return scalar;
}
uint32_t SourceMap::Offset(uint32_t scalar,SourceUnit unit) const
{
    Check(scalar<Boundaries.size(),"Scalar source offset is out of range");
    if(unit==SourceUnit::Scalar)return scalar;
    if(unit==SourceUnit::UTF8Byte)return Boundaries[scalar].Byte;
    if(unit==SourceUnit::UTF16)return Boundaries[scalar].UTF16;
    Check(unit==SourceUnit::Grapheme&&Boundaries[scalar].Grapheme,"Offset is not a grapheme boundary");
    uint32_t result=0;for(uint32_t i=0;i<scalar;++i)result+=Boundaries[i].Grapheme;return result;
}
uint32_t SourceMap::Graphemes() const {return Offset(uint32_t(Scalars.size()),SourceUnit::Grapheme);}
void ValidateSpanFields(const std::vector<Span>& spans)
{
    Check(spans.size()<=MaxSpans,"Display emphasis exceeds 128 spans");uint32_t end=0;
    for(const auto& s:spans){Check(s.Begin<s.End&&s.Begin>=end,"Emphasis must be sorted, nonempty and nonoverlapping");end=s.End;
        Check(!s.Face||unsigned(*s.Face)<4,"Invalid emphasis face style");
        if(s.Color)for(int i=0;i<4;++i)Check(std::isfinite((*s.Color)[i])&&(*s.Color)[i]>=0&&(*s.Color)[i]<=1,"Emphasis colors require finite 0..1 channels");}
}
std::vector<Span> NormalizeSpans(const SourceMap& source,const std::vector<Span>& spans)
{
    ValidateSpanFields(spans);std::vector<Span> out;out.reserve(spans.size());
    for(auto s:spans){source.Scalar(s.Begin,SourceUnit::Scalar);source.Scalar(s.End,SourceUnit::Scalar);if(!s.Face&&!s.Color)continue;
        if(!out.empty()&&out.back().End==s.Begin&&out.back().Face==s.Face&&out.back().Color==s.Color)out.back().End=s.End;else out.push_back(s);}
    return out;
}
std::vector<Span> NormalizeSpans(std::string_view text,const std::vector<Span>& spans){return NormalizeSpans(SourceMap::Analyze(text),spans);}
const Span* SpanAt(const std::vector<Span>& spans,uint32_t scalar)
{
    auto it=std::upper_bound(spans.begin(),spans.end(),scalar,[](uint32_t p,const Span& s){return p<s.Begin;});
    return it!=spans.begin()&&scalar<(--it)->End?&*it:nullptr;
}
SourceEdit RemapSpans(std::string_view before,std::string_view after,const std::vector<Span>& spans)
{
    auto a=SourceMap::Analyze(before),b=SourceMap::Analyze(after);auto normalized=NormalizeSpans(a,spans);SourceEdit result;
    uint32_t prefix=0;while(prefix<a.Scalars.size()&&prefix<b.Scalars.size()&&a.Scalars[prefix]==b.Scalars[prefix])++prefix;
    while(prefix&&(!a.Boundaries[prefix].Grapheme||!b.Boundaries[prefix].Grapheme))--prefix;
    uint32_t ae=uint32_t(a.Scalars.size()),be=uint32_t(b.Scalars.size());
    while(ae>prefix&&be>prefix&&a.Scalars[ae-1]==b.Scalars[be-1]){--ae;--be;}
    while(ae<a.Scalars.size()&&(!a.Boundaries[ae].Grapheme||!b.Boundaries[be].Grapheme)){++ae;++be;}
    for(auto s:normalized){if(s.End<=prefix)result.Spans.push_back(s);else if(s.Begin>=ae){auto delta=int64_t(be)-ae;s.Begin=uint32_t(s.Begin+delta);s.End=uint32_t(s.End+delta);result.Shifted+=delta!=0;result.Spans.push_back(s);}else ++result.Removed;}
    result.Spans=NormalizeSpans(b,result.Spans);return result;
}
}
