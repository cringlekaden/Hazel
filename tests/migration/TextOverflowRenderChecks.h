// GPU/reference placement uses full known retained strings, not another truncator.
static void TextOverflowRenderChecks() {
    auto font=Hazel::Font::Load("typography/DejaVuSans.ttf");
    for(const auto& source:{u8"e\u0301 e\u0301 ffi long",u8"مرحبا بالعالم Hazel 123",u8"שלום Hazel (123) مرحبا"}) {
        std::vector<Renderer2D::TextLine> lines;
        auto layout=Renderer2D::LayoutText(source,font,2.8f,false,0,lines,0,Text::Overflow::Ellipsis,1);
        Check(layout->Omitted && lines.size()==1 && layout->Lines[0].Ellipsized,"Rendered fixture not constrained");
        std::vector<Renderer2D::TextGlyph> glyphs;Renderer2D::AppendTextLineGlyphs(lines.front(),font,glyphs);
        Check(!glyphs.empty(),"Final marker generated no glyph geometry");
        for(auto alignment:{0.f,.5f,1.f}) {
            const float x=(2.8f-layout->Width)*alignment;
            auto transform=Transform(-.8f+x*.45f,-.2f,.45f);
            Begin();Renderer2D::DrawTextGlyphs(glyphs,font,transform,{.7f,.8f,1,.6f},431);End();
            Check(Pixels(431)>5,"Actual overflow glyphs/marker not pickable");
            std::vector<uint8_t> actual(128*128*4);glReadBuffer(GL_COLOR_ATTACHMENT0);glReadPixels(0,0,128,128,GL_RGBA,GL_UNSIGNED_BYTE,actual.data());
            // Expected retained source is independently rematerialized from the
            // published original source boundary plus marker; force paragraph
            // direction so neutral final punctuation keeps its paragraph base.
            auto end=layout->Boundaries[layout->Lines[0].End].Byte;
            Text::Options options;options.EmSize=1/font->GetCPUFace()->HeightEm();options.BaseDirection=layout->Paragraphs.front().Level&1?Text::Direction::RTL:Text::Direction::LTR;
            auto expected=Text::Service::Main().Arrange(std::string(source).substr(0,end)+u8"…",font->GetCPUFace(),options);
            Check(expected->Glyphs.size()==layout->Glyphs.size(),"Final marker was independently appended instead of jointly shaped");
            for(size_t i=0;i<expected->Glyphs.size();++i)Check(expected->Glyphs[i].ID==layout->Glyphs[i].ID && expected->Glyphs[i].X==layout->Glyphs[i].X && expected->Glyphs[i].OffsetX==layout->Glyphs[i].OffsetX,"Final bidi/contextual positions differ from complete retained string");
            auto pages=font->PreparePages(*expected);for(auto& glyph:glyphs)Check(bool(glyph.Page),"Healthy marker used unavailable page");
            // Same geometry through painter runs must honor a later clip without
            // applying it retroactively to the first translucent text draw.
            Begin();Renderer2D::EndScene();Renderer2D::BeginOrderedScene(Camera(glm::mat4(1)),glm::mat4(1));
            Renderer2D::DrawTextGlyphs(glyphs,font,transform,{.7f,.8f,1,.6f},431);
            Renderer2D::SetPrimitiveClip({0,0,0,0});Renderer2D::DrawTextGlyphs(glyphs,font,transform,{1,0,0,1},432);End();
            std::vector<uint8_t> ordered(actual.size());glReadBuffer(GL_COLOR_ATTACHMENT0);glReadPixels(0,0,128,128,GL_RGBA,GL_UNSIGNED_BYTE,ordered.data());
            Check(actual==ordered && Pixels(432)==0,"Later empty clip changed previously queued ellipsis paint");
        }
    }
    std::cout<<"PASS: rendered accents/ligatures/Arabic/mixed bidi ellipsis, physical alignment, contextual positions, translucent painter order/clips/picking\n";
}
