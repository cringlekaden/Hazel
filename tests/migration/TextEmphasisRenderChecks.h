#include <chrono>
#include <set>
// Independent source-over reference plus actual supplied-face geometry/pages.
static void TextEmphasisRenderChecks() {
    std::filesystem::copy_file("assets/fonts/opensans/OpenSans-Regular.ttf","assets/emphasis-private.ttf",std::filesystem::copy_options::overwrite_existing);
    auto font=CreateRef<Hazel::Font>("assets/emphasis-private.ttf");
    auto page=font->PreparePages(*font->Layout("M",-1,false)).front();auto atlas=page->Texture;
    std::vector<uint8_t> samples(size_t(atlas->GetWidth())*atlas->GetHeight()*3,128);atlas->SetData(samples.data(),uint32_t(samples.size()));
    std::vector<Renderer2D::TextGlyph> analytic{{{-.25f,-.25f},{.25f,.25f},{0,0},{1,1},page,font->GetCPUFace(),0}};
    std::vector<Text::Span> spans{{0,1,{},glm::vec4(.9f,.2f,.7f,.6f)}};
    const bool dither=glIsEnabled(GL_DITHER),srgb=glIsEnabled(GL_FRAMEBUFFER_SRGB);glDisable(GL_DITHER);glDisable(GL_FRAMEBUFFER_SRGB);
    for(float root:{0.f,.4f,1.f}) {
        const glm::vec4 background(.2f,.4f,.1f,.3f),interaction(.5f,.8f,.9f,.7f);
        Begin();Renderer2D::EndScene();RenderCommand::SetClearColor(background);RenderCommand::Clear();target->ClearAttachment(1,-1);
        Renderer2D::BeginOrderedScene(Camera(glm::mat4(1)),glm::mat4(1));
        Renderer2D::DrawTextGlyphs(analytic,font,glm::mat4(1),{.1f,.8f,.3f,root},511,spans,interaction);End();
        auto actual=Color();auto destination=glm::round(background*255.f)/255.f;
        const auto rgb=glm::vec3(*spans[0].Color)*glm::vec3(interaction);float alpha=.6f*root*.7f*(128/255.f);
        const glm::vec4 expected(rgb*alpha+glm::vec3(destination)*(1-alpha),alpha+destination.a*(1-alpha));
        for(int c=0;c<4;++c)Check(std::abs(int(actual[c])-int(std::round(expected[c]*255)))<=2,"Inline color/opacity/coverage independent source-over mismatch");
        Check(ID()==(root?511:-1),"Zero root opacity did not hide explicit spans from picking");
    }
    if(dither)glEnable(GL_DITHER);if(srgb)glEnable(GL_FRAMEBUFFER_SRGB);
    FontPalette fonts{};fonts[0]=Hazel::Font::GetDefault();fonts[2]=Hazel::Font::Load("assets/fonts/opensans/OpenSans-Bold.ttf");fonts[3]=Hazel::Font::Load("assets/fonts/opensans/OpenSans-Italic.ttf");
    spans={{0,4,FontStyle::Bold,glm::vec4(1,.2f,.1f,1)},{5,11,FontStyle::Italic,{}}};
    const auto coldStart=std::chrono::steady_clock::now();
    std::vector<Renderer2D::TextLine> lines;auto layout=Renderer2D::LayoutText("Bold italic ordinary",fonts[0],3.5f,true,0,lines,0,Text::Overflow::Ellipsis,2,-1,fonts,spans);
    const auto shapeDone=std::chrono::steady_clock::now();
    std::vector<Renderer2D::TextGlyph> glyphs;for(const auto& line:lines)Renderer2D::AppendTextLineGlyphs(line,fonts[0],glyphs,{},fonts);
    const auto pagesDone=std::chrono::steady_clock::now();
    bool bold=false,italic=false;for(const auto& g:glyphs){Check(g.BaseFace==fonts[0]->GetCPUFace()&&g.Page,"Rich geometry lost immutable root/page pins");bold|=g.Page->Face==fonts[2]->GetCPUFace();italic|=g.Page->Face==fonts[3]->GetCPUFace();}
    Check(bold&&italic,"Actual rich rendering did not prepare both real face pages");
    Begin();Renderer2D::DrawTextGlyphs(glyphs,fonts[0],Transform(-.8f,-.2f,.4f),{1,1,1,.7f},512,spans);End();Check(Pixels(512)>20,"Mixed-face wrapped text is not drawn/pickable");
    const auto richDraws=Renderer2D::GetStats().DrawCalls;
    const auto before=Hazel::Font::Resources();spans[0].Color=glm::vec4(.1f,.9f,.4f,.5f);
    auto reused=Renderer2D::LayoutText("Bold italic ordinary",fonts[0],3.5f,true,0,lines,0,Text::Overflow::Ellipsis,2,-1,fonts,spans);
    Check(reused==layout&&Hazel::Font::Resources().LayoutBuilds==before.LayoutBuilds&&Hazel::Font::Resources().Generations==before.Generations,"Color-only emphasis reshaped or regenerated pages");
    if(std::getenv("HAZEL_EMPHASIS_PROFILE")){
        std::set<const FontPage*> pages;for(const auto& g:glyphs)pages.insert(g.Page.get());
        std::vector<double> samples;for(int i=0;i<140;++i){auto start=std::chrono::steady_clock::now();auto hit=Renderer2D::LayoutText("Bold italic ordinary",fonts[0],3.5f,true,0,lines,0,Text::Overflow::Ellipsis,2,-1,fonts,spans);auto end=std::chrono::steady_clock::now();if(i>=20)samples.push_back(std::chrono::duration<double,std::micro>(end-start).count());Check(hit==layout,"Warm rich shape result changed");}
        std::sort(samples.begin(),samples.end());
        std::cout<<"EMPHASIS_PROFILE cold_layout_ms="<<std::chrono::duration<double,std::milli>(shapeDone-coldStart).count()<<" cold_pages_geometry_ms="<<std::chrono::duration<double,std::milli>(pagesDone-shapeDone).count()<<" runs="<<layout->Runs.size()<<" quads="<<glyphs.size()<<" pages="<<pages.size()<<" draws="<<richDraws<<" warm_lookup_us_median="<<samples[samples.size()/2]<<" p95="<<samples[samples.size()*95/100]<<" new_layouts="<<Hazel::Font::Resources().LayoutBuilds-before.LayoutBuilds<<" new_pages="<<Hazel::Font::Resources().Generations-before.Generations<<'\n';
    }
    TextComponent world;world.TextString="Bold italic ordinary";world.Spans=spans;
    ResolveTextFonts(world,{},world.Spans);
    Begin();Renderer2D::DrawString(world.TextString,Transform(-.8f,-.2f,.4f),world,513);End();Check(Pixels(513)>20,"World component bypassed mixed-face rendering");
    std::cout<<"PASS: rich real-face pages/world draw, wrapping, immutable pins, color-only reuse; independent linear RGBA8 span opacity/coverage and zero-opacity picking\n";
}
