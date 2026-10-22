#include "FontFamilyFixture.h"
#include "Hazel/Assets/ProjectAssets.h"
// Independent numeric reference: RGB8 MSDF distance, straight-alpha inputs,
// linear RGBA8 attachment (no sRGB conversion, dithering or multisampling).
static void TextCompositingChecks(bool reproduce = false) {
    std::filesystem::copy_file("assets/fonts/opensans/OpenSans-Regular.ttf","assets/analytic-private.ttf",std::filesystem::copy_options::overwrite_existing);
    auto font=CreateRef<Hazel::Font>("assets/analytic-private.ttf");
    auto analyticPages=font->PreparePages(*font->Layout("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789",-1,false));
    auto atlas=analyticPages.front()->Texture;
    std::vector<unsigned char> distance(size_t(atlas->GetWidth())*atlas->GetHeight()*3,128);
    atlas->SetData(distance.data(),uint32_t(distance.size()));
    // Small quad / full-atlas UV makes screenPxRange clamp to exactly 1.
    Check(atlas->GetWidth()>=64 && atlas->GetHeight()>=64,"Analytic atlas assumption");
    std::vector<Renderer2D::TextGlyph> glyphs{{{-.25f,-.25f},{.25f,.25f},{0,0},{1,1},analyticPages.front()}};
    const bool dither=glIsEnabled(GL_DITHER);glDisable(GL_DITHER);
    const bool srgb=glIsEnabled(GL_FRAMEBUFFER_SRGB);glDisable(GL_FRAMEBUFFER_SRGB);
    auto over=[](glm::vec4 dst,glm::vec4 tint,float coverage) {
        float a=tint.a*coverage;
        return glm::vec4(glm::vec3(tint)*a+glm::vec3(dst)*(1-a),a+dst.a*(1-a));
    };
    auto quantize=[](glm::vec4 value){return glm::round(value*255.f)/255.f;};
    for(bool ordered:{false,true})for(auto background:{glm::vec4(0,0,0,0),glm::vec4(1),glm::vec4(.2f,.4f,.7f,1),glm::vec4(.06f,.12f,.21f,.3f)})
        for(float authored:{0.f,.4f,1.f})for(unsigned char sample:{0,128,255}) {
            std::fill(distance.begin(),distance.end(),sample);atlas->SetData(distance.data(),uint32_t(distance.size()));
            Begin();Renderer2D::EndScene();RenderCommand::SetClearColor(background);RenderCommand::Clear();target->ClearAttachment(1,-1);
            if(ordered)Renderer2D::BeginOrderedScene(Camera(glm::mat4(1)),glm::mat4(1));
            else Renderer2D::BeginScene(Camera(glm::mat4(1)),glm::mat4(1));
            const glm::vec4 tint(.8f,.3f,.6f,authored);
            Renderer2D::DrawTextGlyphs(glyphs,font,glm::mat4(1),tint,301);End();
            auto expected=over(quantize(background),tint,sample/255.f);auto actual=Color();
            for(int c=0;c<4;++c)if(std::abs(int(actual[c])-int(std::round(expected[c]*255)))>2) {
                std::cout<<"D8 reference: ordered="<<ordered<<" distance="<<int(sample)<<" opacity="<<authored<<" channel="<<c<<" expected="<<expected[c]*255<<" actual="<<int(actual[c])<<'\n';
                if(reproduce) {
                    std::cout<<"REPRODUCED: text RGB coverage applied twice\n";
                    if(dither)glEnable(GL_DITHER);
                    if(srgb)glEnable(GL_FRAMEBUFFER_SRGB);
                    return;
                }
                throw std::runtime_error("Analytic text source-over RGB / stored-alpha reference mismatch");
            }
            Check(ID()==(sample&&authored?301:-1),"Zero text coverage/opacity wrote picking");
        }
    // An ordinary DrawString uses the same source convention; its glyph shape
    // is intentionally replaced by a uniform signed distance, not a screenshot.
    std::fill(distance.begin(),distance.end(),128);atlas->SetData(distance.data(),uint32_t(distance.size()));
    std::vector<Renderer2D::TextGlyph> world;Renderer2D::AppendTextGlyphs("M",font,{},world);
    Check(world.size()==1,"World analytic glyph missing");
    const auto& g=world.front();
    auto pixels=(g.Max-g.Min)*8.f,uv=g.UVMax-g.UVMin;
    Check(glm::dot(glm::vec2(2.f/atlas->GetWidth(),2.f/atlas->GetHeight()),pixels/uv)*.5f<=1,"World coverage reference needs minimum screen range");
    Begin();const glm::vec4 tint(.2f,.8f,.4f,.5f);
    Renderer2D::DrawString("M",font,Transform(-.1f,-.1f,.125f),{tint,0,0},302);End();
    auto center=(g.Min+g.Max)*.5f*.125f+glm::vec2(-.1f);
    auto color=Color(int((center.x+1)*64),int((center.y+1)*64));
    auto expected=over({},tint,128/255.f);
    for(int c=0;c<4;++c)Check(std::abs(int(color[c])-int(std::round(expected[c]*255)))<=2,"World DrawString analytic compositing mismatch");
    // Overlap must use the quantized destination from the preceding draw.
    Begin();Renderer2D::EndScene();Renderer2D::BeginOrderedScene(Camera(glm::mat4(1)),glm::mat4(1));
    const glm::vec4 bottom(.8f,.2f,.4f,.4f),top(.1f,.7f,.3f,.6f);
    Renderer2D::DrawTextGlyphs(glyphs,font,glm::mat4(1),bottom,303);
    Renderer2D::DrawTextGlyphs(glyphs,font,glm::mat4(1),top,304);End();
    expected=over(quantize(over({},bottom,128/255.f)),top,128/255.f);color=Color();
    for(int c=0;c<4;++c)Check(std::abs(int(color[c])-int(std::round(expected[c]*255)))<=2,"Overlapping text analytic mismatch");
    Check(ID()==304,"Overlapping text picking order changed");
    // A sprite after text must recover its existing stored-alpha convention.
    Begin();Renderer2D::EndScene();Renderer2D::BeginOrderedScene(Camera(glm::mat4(1)),glm::mat4(1));
    Renderer2D::DrawTextGlyphs(glyphs,font,glm::mat4(1),bottom,306);
    Renderer2D::DrawQuad(glm::mat4(1),top,307);End();
    auto textDestination=quantize(over({},bottom,128/255.f));
    expected=glm::vec4(glm::vec3(top)*top.a+glm::vec3(textDestination)*(1-top.a),top.a*top.a+textDestination.a*(1-top.a));color=Color();
    for(int c=0;c<4;++c)Check(std::abs(int(color[c])-int(std::round(expected[c]*255)))<=2,"Text compositing changed later sprite semantics");
    // Capture/restore arbitrary target-0 alpha factors/equation, leave target 1
    // and blend enable untouched (integer picking ignores blending).
    glBlendFuncSeparatei(0,GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA,GL_ZERO,GL_ONE);
    glBlendEquationSeparatei(0,GL_FUNC_ADD,GL_FUNC_REVERSE_SUBTRACT);
    glBlendFuncSeparatei(1,GL_ONE,GL_ZERO,GL_ONE,GL_ZERO);
    Begin();Renderer2D::DrawTextGlyphs(glyphs,font,glm::mat4(1),tint,308);End();
    GLint source=0,destination=0,equation=0,other=0;
    glGetIntegeri_v(GL_BLEND_SRC_ALPHA,0,&source);glGetIntegeri_v(GL_BLEND_DST_ALPHA,0,&destination);
    glGetIntegeri_v(GL_BLEND_EQUATION_ALPHA,0,&equation);glGetIntegeri_v(GL_BLEND_SRC_RGB,1,&other);
    Check(source==GL_ZERO&&destination==GL_ONE&&equation==GL_FUNC_REVERSE_SUBTRACT&&other==GL_ONE&&glIsEnabled(GL_BLEND),"Text leaked indexed blend state");
    glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);glBlendEquation(GL_FUNC_ADD);
    glDisable(GL_BLEND);Begin();Renderer2D::DrawTextGlyphs(glyphs,font,glm::mat4(1),tint,309);End();
    Check(!glIsEnabled(GL_BLEND),"Text submission enabled host-disabled blending");glEnable(GL_BLEND);
    // Same numeric reference through the real scene-owned runtime UI path.
    {
        auto scene=CreateRef<Scene>();auto assets=CreateRef<ProjectAssets>(std::filesystem::current_path()/"assets");scene->SetAssets(assets);
        std::filesystem::copy_file(assets->Root()/"fonts/opensans/OpenSans-Regular.ttf",assets->Root()/"analytic-face.ttf",std::filesystem::copy_options::overwrite_existing);
        auto uiFont=assets->FontAsset("analytic-face.ttf");auto uiPages=uiFont->PreparePages(*uiFont->Layout("M",-1,false));auto uiAtlas=uiPages.front()->Texture;
        std::vector<unsigned char> uniform(size_t(uiAtlas->GetWidth())*uiAtlas->GetHeight()*3,128);uiAtlas->SetData(uniform.data(),uint32_t(uniform.size()));
        scene->OnViewportResize(128,128);auto canvas=scene->CreateEntity("Analytic canvas");auto& canvasData=canvas.AddComponent<UICanvasComponent>();canvasData.ReferenceSize={128,128};canvasData.SafeInsets={};
        auto label=scene->CreateEntity("Analytic label");scene->Reparent(label,canvas,TransformPolicy::KeepLocal);
        label.AddComponent<UIRectComponent>().FixedSize={64,64};auto& text=label.AddComponent<TextComponent>();text.TextString="M";text.Color=tint;
        auto& style=label.AddComponent<UITextStyleComponent>();style.Size=8;WriteTestFamily(assets->Root(),"test.hfont","analytic-face.ttf");label.GetComponent<TextComponent>().Font.Family="test.hfont";
        Begin();Renderer2D::EndScene();EditorCamera camera(45,1,.1f,1000);scene->OnUpdateEditor(0,camera);
        std::vector<int> ids(128*128);glReadBuffer(GL_COLOR_ATTACHMENT1);glReadPixels(0,0,128,128,GL_RED_INTEGER,GL_INT,ids.data());
        auto found=std::find(ids.begin(),ids.end(),int(uint32_t(label)));Check(found!=ids.end(),"Runtime analytic text did not pick");
        auto index=size_t(found-ids.begin());color=Color(int(index%128),int(index/128));expected=over({},tint,128/255.f);
        for(int c=0;c<4;++c)Check(std::abs(int(color[c])-int(std::round(expected[c]*255)))<=2,"Scene UI analytic compositing mismatch");
    }
    // Ordinary window framebuffer: RGB reference only, since its alpha bits are
    // platform-selected and the window compositor is outside this renderer test.
    target->Unbind();RenderCommand::SetViewport(0,0,128,128);glReadBuffer(GL_BACK);
    RenderCommand::SetClearColor({0,0,0,0});RenderCommand::Clear();
    Renderer2D::BeginScene(Camera(glm::mat4(1)),glm::mat4(1));Renderer2D::DrawTextGlyphs(glyphs,font,glm::mat4(1),tint,305);End();
    glReadPixels(64,64,1,1,GL_RGBA,GL_UNSIGNED_BYTE,color.data());expected=over({},tint,128/255.f);
    for(int c=0;c<3;++c)Check(std::abs(int(color[c])-int(std::round(expected[c]*255)))<=2,"Window framebuffer text compositing mismatch");
    target->Bind();
    if(reproduce)throw std::runtime_error("D8 reproduction unexpectedly passed");
    if(dither)glEnable(GL_DITHER);
    if(srgb)glEnable(GL_FRAMEBUFFER_SRGB);
}
