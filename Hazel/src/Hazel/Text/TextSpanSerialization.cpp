#include "hzpch.h"
#include "TextSpanSerialization.h"
#include "Hazel/Core/DocumentSchema.h"
namespace Hazel::Text
{
std::vector<Span> ReadSpans(const YAML::Node& node,std::string_view source)
{
    if(!node)return {};
    DocumentSchema::Keys(node,{"Version","Spans"},"Text Emphasis");
    if(node["Version"].as<unsigned>()!=1)throw std::invalid_argument("Unsupported text Emphasis.Version");
    auto entries=node["Spans"];
    if(!entries.IsSequence()||entries.size()>MaxSpans)throw std::invalid_argument("Text Emphasis requires at most 128 spans");
    std::vector<Span> spans;spans.reserve(entries.size());
    for(auto entry:entries){
        DocumentSchema::Keys(entry,{"Begin","End","Face","Color"},"Text emphasis span (scalar ranges)");
        Span s;s.Begin=entry["Begin"].as<uint32_t>();s.End=entry["End"].as<uint32_t>();
        if(entry["Face"]){auto face=entry["Face"].as<unsigned>();if(face>3)throw std::invalid_argument("Invalid emphasis face");s.Face=FontStyle(face);}
        if(auto color=entry["Color"]){if(!color.IsSequence()||color.size()!=4)throw std::invalid_argument("Emphasis color requires RGBA");glm::vec4 value;for(int i=0;i<4;++i)value[i]=color[i].as<float>();s.Color=value;}
        spans.push_back(s);
    }
    auto normalized=NormalizeSpans(source,spans);
    if(normalized!=spans)throw std::invalid_argument("Text emphasis must be canonical; remove empty attributes and merge adjacent equal ranges");
    return spans;
}
void WriteSpans(YAML::Emitter& out,std::string_view source,const std::vector<Span>& spans)
{
    auto normalized=NormalizeSpans(source,spans);
    if(normalized!=spans)throw std::invalid_argument("Normalize native display emphasis with SetDisplayText before saving");
    out<<YAML::BeginMap<<YAML::Key<<"Version"<<YAML::Value<<1<<YAML::Key<<"Spans"<<YAML::Value<<YAML::BeginSeq;
    for(auto& s:normalized){out<<YAML::BeginMap<<YAML::Key<<"Begin"<<YAML::Value<<s.Begin<<YAML::Key<<"End"<<YAML::Value<<s.End;
        if(s.Face)out<<YAML::Key<<"Face"<<YAML::Value<<unsigned(*s.Face);
        if(s.Color){out<<YAML::Key<<"Color"<<YAML::Value<<YAML::Flow<<YAML::BeginSeq;for(int i=0;i<4;++i)out<<(*s.Color)[i];out<<YAML::EndSeq;}
        out<<YAML::EndMap;
    }
    out<<YAML::EndSeq<<YAML::EndMap;
}
}
