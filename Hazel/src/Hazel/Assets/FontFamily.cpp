#include "hzpch.h"
#include "FontFamily.h"
#include "Hazel/Project/Project.h"
#include "Hazel/Core/DocumentSchema.h"
#include "Hazel/Core/Resources.h"
#include "Hazel/Utils/UTF8.h"
#include <yaml-cpp/yaml.h>
#include <fstream>
#include <set>
#include <cctype>
namespace Hazel {
namespace {
void Path(const std::filesystem::path& path,bool face=false) {
    if(path.native().size()>1024)throw std::runtime_error("Font dependency path exceeds 1024 units");
    auto text=path.generic_u8string();
    if(text.empty() || text.size()>1024 || text.find_first_of(":\\\r\n<>\"|?*")!=std::string::npos || text.find('\0')!=std::string::npos || path.has_root_path())
        throw std::runtime_error("Font dependency must be a portable, nonempty asset-relative path");
    UTF8::Validate(text);
    for(unsigned char c:text)if(c<32 || c==127)throw std::runtime_error("Font path contains a control character");
    for(auto& part:path) {
        if(part==".." || part==".")throw std::runtime_error("Font dependency contains a relative traversal");
        auto name=part.generic_u8string();if(name.empty() || name.back()=='.' || name.back()==' ')throw std::runtime_error("Font path has a nonportable trailing dot/space");
        auto stem=part.stem().generic_u8string();for(auto& c:stem)c=char(std::toupper(static_cast<unsigned char>(c)));
        if(stem=="CON" || stem=="PRN" || stem=="AUX" || stem=="NUL" || (stem.size()==4 && (stem.substr(0,3)=="COM" || stem.substr(0,3)=="LPT") && stem[3]>='1' && stem[3]<='9'))throw std::runtime_error("Font path uses a reserved Windows device name");
    }
    if(face && path.extension()!=".ttf" && path.extension()!=".otf")throw std::runtime_error("Face must select a TTF/OTF");
}
void Corpus(const std::string& text) {
    if(text.size()>8192)throw std::runtime_error("Font coverage corpus exceeds 8192 UTF-8 bytes");
    std::vector<uint32_t> scalars;UTF8::Decode(text,scalars);
    std::set<uint32_t> unique(scalars.begin(),scalars.end());
    if(unique.size()>4096)throw std::runtime_error("Font coverage exceeds 4096 unique scalars");
}
}
const char* FontStyleName(FontStyle style) {
    static constexpr const char* names[]={"Regular","Bold","Italic","Bold Italic"};
    if(unsigned(style)>=4)throw std::invalid_argument("Invalid font style");
    return names[unsigned(style)];
}
void FontSelection::Validate() const {
    (void)FontStyleName(Style);
    if(!Family.empty()){Path(Family);if(Family.extension()!=".hfont")throw std::invalid_argument("Text needs a project .hfont family (or empty for Open Sans)");}
}
void FontFamilyDefinition::Validate() const {
    if(Name.empty() || Name.size()>128)throw std::runtime_error("Family name needs 1–128 UTF-8 bytes");
    UTF8::Validate(Name);Corpus(Required);Corpus(Sample);
    if(Faces[0].File.empty())throw std::runtime_error("Regular face is required");
    for(size_t i=0;i<4;++i){const auto& face=Faces[i];
        if(face.File.empty()){if(!face.Notices.empty())throw std::runtime_error("Notice without face: "+std::string(FontStyleName(FontStyle(i))));continue;}
        Path(face.File,true);
        if(face.Notices.empty() || face.Notices.size()>8)throw std::runtime_error("Each provided face needs 1–8 associated notice files: "+std::string(FontStyleName(FontStyle(i))));
        std::set<std::filesystem::path> seen;
        for(auto& notice:face.Notices){Path(notice);if(notice==face.File)throw std::runtime_error("Face cannot be its own associated notice");if(!seen.insert(notice).second)throw std::runtime_error("Duplicate face notice: "+notice.generic_u8string());}
    }
}
FontStyle FontFamilyDefinition::Select(FontStyle requested) const {
    (void)FontStyleName(requested);return Faces[unsigned(requested)].File.empty()?FontStyle::Regular:requested;
}
FontFamilyDefinition ReadFontFamilyText(const std::string& text) {
    if(text.size()>65536)throw std::runtime_error("Font family exceeds 64 KiB");
    auto root=YAML::Load(text);DocumentSchema::Structure(root);DocumentSchema::Keys(root,{"FontFamily"},"root");
    auto n=root["FontFamily"];DocumentSchema::Keys(n,{"Version","Name","Faces","Required","Sample"},"FontFamily");
    if(!n["Version"].IsScalar() || n["Version"].as<int>()!=1)throw std::runtime_error("Unsupported FontFamily.Version (expected 1)");
    FontFamilyDefinition out;if(!n["Name"].IsScalar())throw std::runtime_error("FontFamily.Name must be text");out.Name=n["Name"].as<std::string>();
    auto faces=n["Faces"];DocumentSchema::Keys(faces,{"Regular","Bold","Italic","Bold Italic"},"FontFamily.Faces");
    for(unsigned i=0;i<4;++i)if(auto face=faces[FontStyleName(FontStyle(i))]) {
        DocumentSchema::Keys(face,{"File","Notices"},FontStyleName(FontStyle(i)));
        if(!face["File"].IsScalar())throw std::runtime_error(std::string(FontStyleName(FontStyle(i)))+".File must select a face");
        out.Faces[i].File=std::filesystem::u8path(face["File"].as<std::string>());
        auto notices=face["Notices"];if(!notices.IsSequence() || notices.size()>8)throw std::runtime_error(std::string(FontStyleName(FontStyle(i)))+".Notices must contain at most eight paths");
        for(auto notice:notices)out.Faces[i].Notices.push_back(std::filesystem::u8path(notice.as<std::string>()));
    }
    if(n["Required"])out.Required=n["Required"].as<std::string>();
    if(n["Sample"])out.Sample=n["Sample"].as<std::string>();
    out.Validate();return out;
}
FontFamilyDefinition ReadFontFamily(const std::filesystem::path& path) {
    std::ifstream file(path,std::ios::binary|std::ios::ate);auto size=file?file.tellg():std::streampos(-1);
    if(size<0 || size>65536)throw std::runtime_error("Missing font family or descriptor exceeds 64 KiB: "+path.generic_u8string());
    std::string text(size_t(size),'\0');file.seekg(0);
    if(!text.empty() && !file.read(text.data(),std::streamsize(text.size())))throw std::runtime_error("Cannot read family: "+path.generic_u8string());
    try{return ReadFontFamilyText(text);}catch(const std::exception& error){throw std::runtime_error(path.generic_u8string()+": "+error.what());}
}
std::string WriteFontFamilyText(const FontFamilyDefinition& def) {
    def.Validate();YAML::Emitter out;
    out<<YAML::BeginMap<<YAML::Key<<"FontFamily"<<YAML::Value<<YAML::BeginMap<<YAML::Key<<"Version"<<YAML::Value<<1;
    out<<YAML::Key<<"Name"<<YAML::Value<<def.Name<<YAML::Key<<"Faces"<<YAML::Value<<YAML::BeginMap;
    for(unsigned i=0;i<4;++i)if(!def.Faces[i].File.empty()) {
        out<<YAML::Key<<FontStyleName(FontStyle(i))<<YAML::Value<<YAML::BeginMap<<YAML::Key<<"File"<<YAML::Value<<def.Faces[i].File.generic_u8string();
        out<<YAML::Key<<"Notices"<<YAML::Value<<YAML::BeginSeq;for(auto& path:def.Faces[i].Notices)out<<path.generic_u8string();out<<YAML::EndSeq<<YAML::EndMap;
    }
    out<<YAML::EndMap<<YAML::Key<<"Required"<<YAML::Value<<def.Required<<YAML::Key<<"Sample"<<YAML::Value<<def.Sample<<YAML::EndMap<<YAML::EndMap;
    std::string text=out.c_str();if(text.size()>65536)throw std::runtime_error("Encoded font family exceeds 64 KiB; reduce paths/corpora");return text;
}
std::vector<FamilyCoverage> InspectFontFamily(const std::filesystem::path& root,const FontFamilyDefinition& def) {
    def.Validate();std::vector<FamilyCoverage> result;
    for(unsigned i=0;i<4;++i)if(!def.Faces[i].File.empty()) {
        try {
            auto file=Project::ResolveOwnedAsset(root,def.Faces[i].File);
            for(auto& notice:def.Faces[i].Notices)if(!std::filesystem::is_regular_file(Project::ResolveOwnedAsset(root,notice)))throw std::runtime_error("Missing notice: "+notice.generic_u8string());
            result.push_back({FontStyle(i),Font::InspectCoverage(file,def.Required),Font::InspectCoverage(file,def.Sample)});
        }catch(const std::exception& error){throw std::runtime_error(std::string(FontStyleName(FontStyle(i)))+" ["+def.Faces[i].File.generic_u8string()+"]: "+error.what());}
    }
    return result;
}
std::vector<std::filesystem::path> AuditFontFamily(const std::filesystem::path& root,const std::filesystem::path& reference) {
    FontSelection{reference}.Validate();
    if(reference.empty())return {};
    auto def=ReadFontFamily(Project::ResolveOwnedAsset(root,reference));
    std::vector<FamilyCoverage> coverage;
    try{coverage=InspectFontFamily(root,def);}catch(const std::exception& error){throw std::runtime_error(reference.generic_u8string()+": "+error.what());}
    std::set<std::filesystem::path> files{reference};
    for(auto& face:def.Faces)if(!face.File.empty()) {
        try{Font::ValidateFile(Project::ResolveOwnedAsset(root,face.File));}catch(const std::exception& error){throw std::runtime_error(reference.generic_u8string()+" face "+face.File.generic_u8string()+": "+error.what());}
        files.insert(face.File);for(auto& notice:face.Notices)files.insert(notice);
    }
    for(auto& check:coverage)if(!check.Required.Satisfied())throw std::runtime_error(reference.generic_u8string()+" / "+FontStyleName(check.Style)+": required coverage unmet: "+check.Required.Summary());
    return {files.begin(),files.end()};
}
const FontFamilyDefinition& DefaultFontFamily() {
    static const FontFamilyDefinition family=[] {
        FontFamilyDefinition f;f.Name="Open Sans (engine)";
        const char* suffix[]={"Regular","Bold","Italic","BoldItalic"};
        for(unsigned i=0;i<4;++i){f.Faces[i].File=std::string("fonts/opensans/OpenSans-")+suffix[i]+".ttf";f.Faces[i].Notices={"fonts/opensans/LICENSE.txt"};}
        return f;
    }();return family;
}
std::filesystem::path DefaultFontFace(FontStyle requested) {return Resources::Resolve(DefaultFontFamily().Faces[unsigned(DefaultFontFamily().Select(requested))].File);}
void AuditTextEmphasis(const std::filesystem::path& root,const FontSelection& selection,const std::string& source,const std::vector<Text::Span>& input) {
    selection.Validate();auto spans=Text::NormalizeSpans(source,input);if(spans.empty())return;
    const auto family=selection.Family.empty()?DefaultFontFamily():ReadFontFamily(Project::ResolveOwnedAsset(root,selection.Family));
    Text::Service service;Text::FaceSet faces{};std::array<Ref<const Text::Face>,4> accepted{};
    auto face=[&](FontStyle style){auto actual=family.Select(style);auto& result=accepted[unsigned(actual)];if(result)return result;
        auto path=selection.Family.empty()?DefaultFontFace(actual):Project::ResolveOwnedAsset(root,family.Faces[unsigned(actual)].File);
        std::ifstream file(path,std::ios::binary|std::ios::ate);auto size=file?file.tellg():std::streampos(-1);
        if(size<=0||uint64_t(size)>FontLimits{}.InputBytes)throw std::invalid_argument("Emphasis face missing or exceeds 4 MiB: "+path.generic_u8string());
        std::vector<uint8_t> bytes(size_t(size),0);file.seekg(0);if(!file.read(reinterpret_cast<char*>(bytes.data()),std::streamsize(bytes.size())))throw std::invalid_argument("Cannot read emphasis face: "+path.generic_u8string());
        result=service.CreateFace(bytes,path.generic_u8string());return result;};
    faces[0]=face(selection.Style);std::vector<Text::FaceRange> ranges;
    for(const auto& s:spans)if(s.Face){auto index=uint8_t(unsigned(*s.Face)+1);faces[index]=face(*s.Face);
        if(!ranges.empty()&&ranges.back().End==s.Begin&&ranges.back().FaceIndex==index)ranges.back().End=s.End;else ranges.push_back({s.Begin,s.End,index});}
    auto result=service.Arrange(source,faces[0],{},faces,ranges);Text::ValidateEmphasis(*result,spans);
}

}
