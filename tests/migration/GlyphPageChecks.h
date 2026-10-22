#include "Hazel/Text/TextLayout.h"
#include "Hazel/Assets/ProjectAssets.h"
#include "FontFamilyFixture.h"
#include <fstream>
#include <chrono>
static void GlyphPageChecks() {
    auto font=Hazel::Font::Load("typography/DejaVuSans.ttf");
    std::ifstream corpus("typography/corpus.tsv");Check(bool(corpus),"Rendered corpus missing");
    std::string row;size_t count=0;double cold=0;
    while(std::getline(corpus,row)) {
        if(row.empty() || row[0]=='#')continue;
        auto split=row.find('\t');auto text=row.substr(split+1);
        auto layout=font->Layout(text,4,true);Check(layout->MissingGlyphs==0,"Pinned corpus has missing face glyphs");
        Text::Options referenceOptions;referenceOptions.EmSize=1/font->GetCPUFace()->HeightEm();referenceOptions.Width=4;referenceOptions.Language=row.substr(0,split);
        auto reference=Text::Service::Main().Arrange(text,font->GetCPUFace(),referenceOptions);
        Check(reference->Glyphs.size()==layout->Glyphs.size() && std::abs(reference->Width-layout->Width)<1e-6f,"Renderer units/default language disagree with corpus reference");
        for(size_t i=0;i<layout->Glyphs.size();++i)Check(reference->Glyphs[i].ID==layout->Glyphs[i].ID && reference->Glyphs[i].X==layout->Glyphs[i].X && reference->Glyphs[i].OffsetY==layout->Glyphs[i].OffsetY,"Corpus shaping differs through renderer adapter");
        std::vector<Renderer2D::TextLine> lines;Renderer2D::LayoutText(text,font,4,true,0,lines);
        Check(!lines.empty() && lines[0].Result==layout,"Measurement and paint selected different CPU results");
        std::vector<Renderer2D::TextGlyph> glyphs;
        auto start=std::chrono::steady_clock::now();
        for(auto& line:lines)Renderer2D::AppendTextLineGlyphs(line,font,glyphs,{0,layout->Lines.front().Baseline-layout->Lines[line.Index].Baseline});
        cold+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
        Check(!glyphs.empty() && font->PageError().empty(),"Corpus glyph pages unavailable");
        for(auto& glyph:glyphs)Check(glyph.Page && glyph.Page->Face==layout->Font && glyph.Min.x<=glyph.Max.x && glyph.Min.y<=glyph.Max.y &&
            glyph.UVMin.x>=0 && glyph.UVMin.y>=0 && glyph.UVMax.x<=1 && glyph.UVMax.y<=1,"Invalid immutable page/UV/ink bounds");
        Begin();Renderer2D::EndScene();Renderer2D::BeginOrderedScene(Camera(glm::mat4(1)),glm::mat4(1));
        Renderer2D::DrawTextGlyphs(glyphs,font,Transform(-.9f,.6f,.4f),{.6f,.8f,1,.6f},401);End();
        Check(Pixels(401)>0,"Rendered corpus produced no picking/coverage");
        ++count;
    }
    Check(count==12,"Rendered corpus incomplete");
    // Independent CPU positioning: shaped ffi is one glyph and decomposed
    // accent matches composed ink. This cannot pass with cmap-codepoint quads.
    std::vector<Renderer2D::TextGlyph> ligature,composed,decomposed;
    Renderer2D::AppendTextGlyphs("ffi",font,{},ligature);Renderer2D::AppendTextGlyphs(u8"é",font,{},composed);Renderer2D::AppendTextGlyphs(u8"é",font,{},decomposed);
    Check(ligature.size()==1 && composed.size()==1 && decomposed.size()==1,"Glyph substitution was replaced by scalar geometry");
    Check(composed[0].Min==decomposed[0].Min && composed[0].Max==decomposed[0].Max && composed[0].UVMin==decomposed[0].UVMin,"Canonical accents split metrics/UVs");
    auto accent=font->Layout(u8"q́",-1,false);std::vector<Renderer2D::TextGlyph> marks;Renderer2D::AppendTextGlyphs(u8"q́",font,{},marks);
    Check(accent->Glyphs.size()==2 && marks.size()==2 && accent->Glyphs[1].AdvanceX==0,"Combining cluster lost zero-advance positioned glyph");
    auto position=accent->Glyphs[1];auto geometry=marks[1].Page->Find(position.ID);
    auto expected=geometry->Min/font->GetCPUFace()->HeightEm()+glm::vec2(position.X+position.OffsetX,-position.OffsetY);
    Check(glm::length(expected-marks[1].Min)<1e-6f,"Shaped offset/baseline not applied to outline bounds");
    // Empty outlines advance but produce no quad, including after RTL shaping.
    std::vector<Renderer2D::TextGlyph> blanks;Renderer2D::AppendTextGlyphs(" \t ",font,{},blanks);Check(blanks.empty(),"Empty-outline glyph produced a quad");
    // Exact accepted bytes must remain usable after source retirement/replacement.
    auto temporary=std::filesystem::current_path()/"assets/paged-face.ttf";
    std::filesystem::copy_file("typography/DejaVuSans.ttf",temporary,std::filesystem::copy_options::overwrite_existing);
    auto old=Hazel::Font::Load(temporary);auto oldLayout=old->Layout(u8"ΩЖمرحبا",-1,false);
    {std::ofstream changed(temporary,std::ios::binary);changed<<"invalid replacement";}
    bool rejected=false;try{Hazel::Font::Load(temporary,old);}catch(const std::exception&){rejected=true;}
    Check(rejected && !old->PreparePages(*oldLayout).empty(),"Failed reload mixed old layout/new outline bytes");
    // More than 128 distinct source glyphs requires immutable pages, not repacking.
    std::string many;
    for(uint32_t cp=0x21;cp<0x100;++cp)if(font->GetCPUFace()->HasScalar(cp) && !(cp>=0x7f&&cp<=0x9f) && cp!=0xad && !(cp>=0x300&&cp<=0x36f)) {
        if(cp<0x80)many+=char(cp);else{many+=char(0xc0|(cp>>6));many+=char(0x80|(cp&63));}
    }
    auto large=font->Layout(many,-1,false);auto pages=font->PreparePages(*large);Check(pages.size()>1 && font->PageError().empty(),"Multiple glyph-ID pages not admitted");
    auto held=pages.front();auto texture=held->Texture;auto beforeUV=held->Glyphs.front().UVMin;
    std::vector<Renderer2D::TextGlyph> overflow;
    Renderer2D::AppendTextGlyphs(many,font,{},overflow);
    for(auto& g:overflow){auto extent=g.Max-g.Min;g.Min={-.8f,-.5f};g.Max=g.Min+extent*.8f;}
    Begin();Renderer2D::EndScene();Renderer2D::BeginOrderedScene(Camera(glm::mat4(1)),glm::mat4(1));
    for(int i=0;i<140;++i)Renderer2D::DrawTextGlyphs(overflow,font,glm::mat4(1),{1,1,1,.1f},402);
    End();Check(Renderer2D::GetStats().GeometryCount>20000 && Renderer2D::GetStats().DrawCalls>1 && Pixels(402)>0,"Page/capacity rollover lost glyphs");
    Hazel::Font::Trim();Check(held->Texture==texture && held->Glyphs.front().UVMin==beforeUV,"Eviction repacked a pinned page");
    // Shared service used by real UI Content, family resolution and painting.
    auto scene=CreateRef<Scene>();auto assets=CreateRef<ProjectAssets>(std::filesystem::current_path()/"assets");scene->SetAssets(assets);
    std::filesystem::copy_file("typography/DejaVuSans.ttf",temporary,std::filesystem::copy_options::overwrite_existing);
    std::filesystem::copy_file("typography/LICENSE-DejaVu.txt",assets->Root()/"paged-NOTICE.txt",std::filesystem::copy_options::overwrite_existing);
    FontFamilyDefinition family;family.Name="Corpus";family.Faces[0]={"paged-face.ttf",{"paged-NOTICE.txt"}};
    std::ofstream(assets->Root()/"paged.hfont")<<WriteFontFamilyText(family);
    scene->OnViewportResize(128,128);auto canvas=scene->CreateEntity("Corpus canvas");auto& c=canvas.AddComponent<UICanvasComponent>();c.ReferenceSize={128,128};c.SafeInsets={};
    auto label=scene->CreateEntity("Corpus label");scene->Reparent(label,canvas,TransformPolicy::KeepLocal);
    auto& rect=label.AddComponent<UIRectComponent>();rect.FixedSize={120,120};auto& text=label.AddComponent<TextComponent>();text.Font.Family="paged.hfont";text.TextString=u8"مرحبا Hazel 123\nשלום café";
    label.AddComponent<UITextStyleComponent>().Size=14;EditorCamera camera(45,1,.1f,1000);
    Begin();Renderer2D::EndScene();scene->OnUpdateEditor(0,camera);Check(Pixels(int(uint32_t(label)))>30,"Scene UI did not paint shaped corpus");
    auto before=Hazel::Font::Resources();Begin();Renderer2D::EndScene();scene->OnUpdateEditor(0,camera);auto after=Hazel::Font::Resources();
    Check(after.LayoutBuilds==before.LayoutBuilds && after.Generations==before.Generations,"Warm UI reshaped or regenerated pages");
    Check(glGetError()==GL_NO_ERROR,"Rendered typography GL error");
    std::cout<<"PASS: 12 rendered corpus rows, shared layout/geometry, ligature/accents/offsets, multiple pages/capacity, accepted bytes after failed reload, real UI and warm reuse; cold page preparation "<<cold<<" ms (CPU, no language-reader certification)\n";
}
static void PagePressurePixels() {
    Hazel::Font::Shutdown();FontLimits limits;limits.LiveAtlases=1;Hazel::Font::Initialize(limits);
    {
        auto first=Hazel::Font::GetDefault();auto pin=first->PreparePages(*first->Layout("A",-1,false)).front();
        auto blocked=Hazel::Font::Load("typography/DejaVuSans.ttf");auto layout=blocked->Layout(u8"مرحبا",-1,false);
        std::vector<Renderer2D::TextGlyph> glyphs;Renderer2D::AppendTextGlyphs(u8"مرحبا",blocked,{},glyphs);
        Check(glyphs.size()==layout->Glyphs.size() && !blocked->PageError().empty(),"Page pressure omitted shaped glyphs");
        for(size_t i=0;i<glyphs.size();++i)Check(!glyphs[i].Page && std::abs(glyphs[i].Min.x-layout->Glyphs[i].X-layout->Glyphs[i].OffsetX)<1e-6f,"Pressure placeholder changed shaped position");
        glyphs.resize(1);glyphs[0].Min={-.5f,-.5f};glyphs[0].Max={.5f,.5f};
        Begin();Renderer2D::DrawTextGlyphs(glyphs,blocked,glm::mat4(1),{1,1,1,1},411);End();
        Check(ID()==411 && Color()[0]>250 && Color()[1]<5 && Color()[2]>250,"Unavailable page placeholder is invisible or unpickable");
        std::vector<Renderer2D::TextLine> clipped;
        auto ellipsis=Renderer2D::LayoutText("a long label that cannot fit",blocked,1,false,0,clipped,0,Text::Overflow::Ellipsis,1);
        glyphs.clear();Renderer2D::AppendTextLineGlyphs(clipped.front(),blocked,glyphs);
        Check(ellipsis->Omitted && !glyphs.empty() && !glyphs.back().Page && ellipsis->Glyphs.back().Generated,"Marker page pressure bypassed normal placeholder/admission");
        auto failures=Hazel::Font::Resources().PageFailures;
        Renderer2D::AppendTextGlyphs(u8"مرحبا",blocked,{},glyphs);
        Check(Hazel::Font::Resources().PageFailures==failures,"Warm page failure retried generation");
    }
    Hazel::Font::Shutdown();Check(Hazel::Font::Resources().LiveAtlases==0,"Pressure page/texture pins leaked");Hazel::Font::Initialize();Hazel::Font::GetDefault();
}
