// Included by TypographySmoke; independent source-boundary and cmap expectations.
static void OverflowChecks(Service& service,const Ref<const Face>& face,hb_font_t* cmap) {
    auto width=[&](uint32_t cp){hb_codepoint_t id=0;Check(hb_font_get_nominal_glyph(cmap,cp,&id),"Overflow reference cmap missing");return float(hb_font_get_glyph_h_advance(cmap,id))/face->UnitsPerEm();};
    Options options;options.Wrap=false;options.OverflowMode=Overflow::Ellipsis;
    options.Width=width('a')+width(0x2026)+.00001f;
    const std::string original="aaaaaa";
    auto result=service.Arrange(original,face,options);
    Check(result->Omitted && result->Lines.size()==1 && result->Lines[0].End==1 && result->Lines[0].Ellipsized,"Expected one retained a plus shaped marker");
    Check(result->Source==original && result->Scalars.size()==6 && result->Boundaries.back().Byte==6,"Overflow rewrote source/maps");
    Check(result->Glyphs.size()==2 && !result->Glyphs[0].Generated && result->Glyphs[0].ClusterBegin==0 && result->Glyphs[1].Generated && result->Glyphs[1].ClusterBegin==UINT32_MAX && result->Glyphs[1].ClusterEnd==UINT32_MAX,"Generated marker falsely references authored text");
    Check(std::abs(result->Width-width('a')-width(0x2026))<.0001f,"Analytic glyph advances disagree with final width");
    options.Width=width('a')+width(0x2026)-.00001f;
    Check(service.Arrange(original,face,options)->Lines[0].End==0,"Fit boundary failed immediately below a plus marker");
    options.Width=width(0x2026);auto marker=service.Arrange(original,face,options);
    Check(marker->Glyphs.size()==1 && marker->Glyphs[0].Generated,"Marker-only box did not fit");
    options.Width=width(0x2026)-.0001f;auto tiny=service.Arrange(original,face,options);
    Check(tiny->Omitted && tiny->MarkerDoesNotFit && tiny->Glyphs.empty() && tiny->Width==0,"Too-small box overflowed or silently succeeded");
    options.Width=100;auto fits=service.Arrange("short",face,options);
    Check(!fits->Omitted && !fits->Lines[0].Ellipsized,"Fitting text acquired a marker");
    auto empty=service.Arrange("",face,options);Check(!empty->Omitted && empty->Lines.size()==1 && empty->Glyphs.empty(),"Empty input marked omitted");
    options.MaxLines=1;auto newline=service.Arrange("a\n",face,options);
    Check(newline->Omitted && newline->OriginalLines==2 && newline->Lines[0].End==1 && newline->Glyphs.back().Generated,"Omitted trailing empty line lacks marker");
    options.OverflowMode=Overflow::Clip;auto clip=service.Arrange("a\nb\n",face,options);
    Check(clip->Lines.size()==1 && clip->Omitted && clip->Glyphs.size()==1 && !clip->Glyphs[0].Generated,"Clip MaxLines failed");
    options.MaxLines=0;options.Height=0;
    Check(service.Arrange("a\nb",face,options)->Lines.size()==2,"Clip height incorrectly removes pixel-clipped lines");
    options.OverflowMode=Overflow::Ellipsis;
    auto noLine=service.Arrange("a",face,options);Check(noLine->Omitted && noLine->MarkerDoesNotFit && noLine->Lines.empty(),"Zero height retained a line");
    options.Height=face->HeightEm();options.LineSpacing=.25f;
    auto vertical=service.Arrange("a\nb",face,options);Check(vertical->Lines.size()==1 && vertical->Glyphs.back().Generated,"Font-height box/line spacing contract failed");
    options.Height=face->HeightEm()+face->LineHeightEm()+options.LineSpacing;
    Check(service.Arrange("a\nb",face,options)->Lines.size()==2,"Exact second full-line boundary rejected");
    options={};options.OverflowMode=Overflow::Ellipsis;options.MaxLines=1;options.Width=2.5f;
    auto wrapped=service.Arrange("one two three four five",face,options);
    Check(wrapped->Omitted && wrapped->Lines.size()==1 && wrapped->Width<=*options.Width && wrapped->Glyphs.back().Generated,"Wrapped MaxLines/width omitted marker");
    options.Wrap=false;options.Width=1.7f;
    for(const auto& text:{u8"e\u0301e\u0301e\u0301e\u0301",u8"q\u0301q\u0301q\u0301",u8"éééé",u8"ffi ffi ffi",u8"مرحبا بالعالم",u8"שלום Hazel (123) مرحبا"}) {
        auto layout=service.Arrange(text,face,options);
        Check(layout->Omitted && layout->Width<=*options.Width,"Corpus ellipsis did not fit");
        Check(layout->Boundaries[layout->Lines[0].End].Grapheme,"Truncation split combining grapheme");
        for(const auto& glyph:layout->Glyphs)if(!glyph.Generated)Check(glyph.ClusterEnd<=layout->Lines[0].End && layout->Boundaries[glyph.ClusterBegin].Grapheme,"Retained cluster map escaped source prefix");
        auto repeat=service.Arrange(text,face,options);Check(IDs(layout)==IDs(repeat) && layout->Width==repeat->Width,"Overflow nondeterministic");
    }
    options.Width=width(0xe9)+width(0x2026)+.00001f;
    auto mapped=service.Arrange(u8"\ufeffe\u0301aaaaaaa",face,options);
    Check(mapped->Lines[0].End==2 && mapped->Boundaries[0].Byte==3 && mapped->Boundaries[0].UTF16==1 && mapped->Boundaries[2].UTF16==3 && mapped->Source==u8"\ufeffe\u0301aaaaaaa","Ellipsis lost original BOM/UTF-16 combining source maps");
    // A monochrome source scalar is used only to test surrogate mapping; no
    // color-emoji typography support is inferred.
    options.Width=width(0x1f600)+width(0x2026)+.00001f;
    mapped=service.Arrange(u8"\U0001f600aaaaaaa",face,options);
    Check(mapped->Lines[0].End==1 && mapped->Boundaries[1].UTF16==2 && mapped->Boundaries[1].Byte==4,"Retained supplementary scalar lost UTF-16 boundary");
    options.MaxLines=2;options.Width=1.5f;
    auto interior=service.Arrange(u8"a\n\ufeffbbbbbbbb",face,options);
    Check(interior->Lines.size()==2 && interior->Source==u8"a\n\ufeffbbbbbbbb" && std::any_of(interior->Glyphs.begin(),interior->Glyphs.end(),[](const auto& g){return g.Generated;}),"Interior FEFF at line start corrupted generated/source mapping");
    options.MaxLines=1;
    options.Width=3;auto rtl=service.Arrange(u8"שלום שלום שלום",face,options);
    auto generated=std::find_if(rtl->Glyphs.begin(),rtl->Glyphs.end(),[](const auto& glyph){return glyph.Generated;});
    Check(generated!=rtl->Glyphs.end() && generated->X==0,"RTL marker is not at visual trailing left edge");
    options.Width=2.5f;auto arabic=service.Arrange(u8"بببببببب",face,options);
    // Retained Arabic must end in a final (or isolated), never medial form.
    hb_codepoint_t final=0;Check(hb_font_get_nominal_glyph(cmap,0xfe90,&final),"Missing Arabic final reference");
    if(arabic->Lines[0].End>1)Check(std::any_of(arabic->Glyphs.begin(),arabic->Glyphs.end(),[&](const auto& g){return !g.Generated && g.ID==final;}),"Arabic retained boundary did not contextually reshape to final form");
    auto longText=service.Arrange(std::string(4096,'a'),face,options);
    Check(longText->ConservativeTruncation && longText->Width<=*options.Width && longText->Glyphs.size()<10 && longText->ShapeCalls<140,"Long text search is unbounded or discards fitting prefix");
    options.Width=0;Check(service.Arrange("a",face,options)->Glyphs.empty(),"Zero width painted marker");
    Limits limits;limits.ShapeCalls=1;Service limited(limits);
    Reject([&]{limited.Arrange("long label",face,options);},"Overflow work cap ignored");
    Check(limited.Usage().Results==0,"Overflow failure published partial result");
    options={};options.MaxLines=4097;Reject([&]{service.Arrange("a",face,options);},"Invalid MaxLines accepted");
    options={};options.Height=-1;Reject([&]{service.Arrange("a",face,options);},"Invalid height accepted");
    // Private input with an intentionally minimal cmap (only authored 'a').
    // Outlines/metrics remain the pinned face; no new font is redistributed.
    auto missingBytes=face->Bytes();
    auto be16=[&](size_t i){return uint32_t(missingBytes[i])*256+missingBytes[i+1];};
    auto be32=[&](size_t i){return (be16(i)<<16)|be16(i+2);};
    auto put16=[&](size_t i,uint32_t n){missingBytes[i]=uint8_t(n>>8);missingBytes[i+1]=uint8_t(n);};
    auto put32=[&](size_t i,uint32_t n){put16(i,n>>16);put16(i+2,n);};
    size_t offset=0;
    for(uint32_t i=0;i<be16(4);++i){size_t at=12+16*i;if(be32(at)==0x636d6170){offset=be32(at+8);break;}}
    Check(offset && offset+44<missingBytes.size(),"Fixture cmap directory not found");
    hb_codepoint_t a=0;Check(hb_font_get_nominal_glyph(cmap,'a',&a),"Fixture a missing");
    put16(offset,0);put16(offset+2,1);put16(offset+4,3);put16(offset+6,1);put32(offset+8,12);
    const auto t=offset+12;
    const uint16_t table[]={4,32,0,4,4,1,0,0x61,0xffff,0,0x61,0xffff,uint16_t(a-0x61),1,0,0};
    for(size_t i=0;i<std::size(table);++i)put16(t+2*i,table[i]);
    auto missingFace=service.CreateFace(missingBytes,"test-only missing ellipsis cmap");
    Check(missingFace->HasScalar('a') && !missingFace->HasScalar(0x2026),"Missing marker fixture incorrectly mapped");
    options={};options.Wrap=false;options.Width=1;options.OverflowMode=Overflow::Ellipsis;
    Reject([&]{service.Arrange("aaaaaa",missingFace,options);},"Missing marker silently omitted in strict layout");
    options.PermitNotdef=true;auto replacement=service.Arrange("aaaaaa",missingFace,options);
    Check(replacement->Omitted && replacement->MissingGlyphs==1 && std::any_of(replacement->Glyphs.begin(),replacement->Glyphs.end(),[](const auto& g){return g.Generated && g.ID==0;}),"Permitted missing marker not represented as generated .notdef");
    std::cout<<"PASS: shaped Clip/Ellipsis/MaxLines, analytic fit boundaries, generated source maps, graphemes, Arabic/bidi, finite height and bounded search\n";
}
