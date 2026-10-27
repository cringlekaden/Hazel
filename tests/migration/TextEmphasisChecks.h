// CPU expectations use source units, existing cmap references and whole-face
// shaped advances. Color validation is deliberately separate from positioning.
static void TextEmphasisChecks(const std::filesystem::path& root)
{
    auto map=SourceMap::Analyze(u8"\ufeffAe\u0301😀\r\nZ");
    Check(map.Scalar(1,SourceUnit::UTF16)==0 && map.Offset(4,SourceUnit::UTF16)==6,"Emphasis source units lost BOM/surrogate mapping");
    Reject([&]{map.Scalar(5,SourceUnit::UTF16);},"Interior surrogate admitted as emphasis endpoint");
    Reject([&]{map.Scalar(2,SourceUnit::Scalar);},"Combining grapheme split admitted");
    auto spans=NormalizeSpans("abcd",{{0,2,Hazel::FontStyle::Bold,{}},{2,4,Hazel::FontStyle::Bold,{}}});
    Check(spans.size()==1&&spans[0].End==4,"Equivalent adjacent emphasis failed to coalesce");
    Reject([&]{NormalizeSpans("abc",{{0,2,Hazel::FontStyle::Bold,{}},{1,3,{},glm::vec4(1)}});},"Overlapping emphasis admitted");
    auto remap=RemapSpans("One two three","One added two three",{{0,3,Hazel::FontStyle::Bold,{}},{4,7,{},glm::vec4(1,0,0,1)},{8,13,Hazel::FontStyle::Italic,{}}});
    Check(remap.Removed==0&&remap.Shifted==2&&remap.Spans[1].Begin==10,"Source edit lost unaffected emphasis");
    remap=RemapSpans("alpha beta","alXXpha beta",{{0,5,Hazel::FontStyle::Bold,{}},{6,10,Hazel::FontStyle::Italic,{}}});
    Check(remap.Removed==1&&remap.Spans.size()==1&&remap.Spans[0].Begin==8,"Intersected source emphasis was silently retained");
    Service service;auto regular=service.CreateFace(Read(root/"DejaVuSans.ttf"),"emphasis corpus");
    auto latin=service.CreateFace(Read("assets/fonts/opensans/OpenSans-Regular.ttf"),"emphasis regular");
    auto bold=service.CreateFace(Read("assets/fonts/opensans/OpenSans-Bold.ttf"),"emphasis bold");
    auto italic=service.CreateFace(Read("assets/fonts/opensans/OpenSans-Italic.ttf"),"emphasis italic");
    FaceSet faces{};faces[2]=bold;faces[3]=italic;
    auto mixed=service.Arrange("Bold plain",latin,{},faces,{{0,4,2}});
    const auto expected=service.Arrange("Bold",bold)->Width+service.Arrange(" plain",latin)->Width;
    Check(std::abs(mixed->Width-expected)<1e-5f&&mixed->Runs.size()==2&&mixed->Runs[0].FaceIndex==2,"Real mixed-face advances/selection disagree with complete-face references");
    Check(mixed->BoxHeight>=latin->HeightEm()&&mixed->BoxHeight>=bold->HeightEm(),"Mixed-face uniform line box clips a supplied face");
    for(const auto& line:mixed->Lines)for(uint32_t i=line.FirstRun;i<line.FirstRun+line.RunCount;++i)for(uint32_t g=mixed->Runs[i].FirstGlyph;g<mixed->Runs[i].FirstGlyph+mixed->Runs[i].GlyphCount;++g)
        Check(mixed->Glyphs[g].Y==line.Baseline,"Mixed faces lost shared baseline");
    auto ligature=service.Arrange("ffi",regular);
    Reject([&]{ValidateEmphasis(*ligature,{{0,1,{},glm::vec4(1,0,0,1)}});},"Color split a ligature into independently colored glyph fragments");
    ValidateEmphasis(*ligature,{{0,3,{},glm::vec4(1,0,0,1)}});
    Reject([&]{service.Arrange("ffi",regular,{},faces,{{0,1,2}});},"Face boundary split an original ligature");
    auto joined=service.Arrange(u8"بب Hazel 12 שלום",regular);
    ValidateEmphasis(*joined,{{0,2,{},glm::vec4(1,0,0,1)}});
    ValidateEmphasis(*joined,{{0,1,{},glm::vec4(.2f,1,.4f,1)}});
    FaceSet arabicFaces{};arabicFaces[2]=service.CreateFace(Read(root/"DejaVuSans.ttf"),"same pinned Arabic bytes, alternate accepted version");
    auto contextual=service.Arrange(u8"بب Hazel 12 שלום",regular,{},arabicFaces,{{0,1,2}});
    Check(contextual->Glyphs.size()==joined->Glyphs.size()&&contextual->Width==joined->Width,"Face boundary lost paragraph joining context");
    for(size_t i=0;i<joined->Glyphs.size();++i)Check(contextual->Glyphs[i].ID==joined->Glyphs[i].ID&&contextual->Glyphs[i].X==joined->Glyphs[i].X,"Arabic face segmentation lost contextual forms or bidi placement");
    Check(joined->Glyphs[0].ClusterBegin!=UINT32_MAX,"Color emphasis changed bidi source mappings");
    auto combining=service.Arrange(u8"e\u0301 é",regular);
    Reject([&]{ValidateEmphasis(*combining,{{0,1,{},glm::vec4(1)}});},"Color split a decomposed accent");
    Options wrapped;wrapped.Width=service.Arrange("Bold ",latin,{},faces,{{0,4,2}})->Width+.001f;
    auto lines=service.Arrange("Bold plain\nagain",latin,wrapped,faces,{{0,4,2},{11,16,3}});
    Check(lines->Lines.size()>=3&&lines->Boundaries.back().Byte==16,"Rich wrapping/newlines lost original source");
    Options clipped=wrapped;clipped.OverflowMode=Overflow::Ellipsis;clipped.MaxLines=1;
    auto ellipsis=service.Arrange("Bold plain again",latin,clipped,faces,{{0,4,2}});
    Check(ellipsis->Omitted&&ellipsis->Lines.size()==1,"Mixed-face MaxLines/ellipsis failed");
    bool marker=false;for(const auto& g:ellipsis->Glyphs)if(g.Generated){marker=true;Check(g.ClusterBegin==UINT32_MAX&&g.ClusterEnd==UINT32_MAX&&g.PaintSource<ellipsis->Scalars.size(),"Generated rich marker invented source offsets");}
    Check(marker,"Mixed-face ellipsis omitted its generated marker");
    for(int i=0;i<3;++i){auto again=service.Arrange("Bold plain",latin,{},faces,{{0,4,2}});Check(again->Width==mixed->Width&&again->Glyphs.size()==mixed->Glyphs.size(),"Mixed-face repeated arrangement is nondeterministic");}
    std::cout<<"PASS: typed emphasis/source conversions/remapping, mixed-face metrics/baselines, grapheme/ligature boundaries, bidi, wrapping and marker source semantics\n";
}
