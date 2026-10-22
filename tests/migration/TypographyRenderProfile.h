#include "Hazel/Renderer/GPUTimer.h"
#include <sstream>
#include <iomanip>
// Optional, same maintained font/corpus as the correctness fixtures. No waits,
// presentation, input, source detection or scene layout are timed here.
static void TypographyRenderProfile() {
    using Clock=std::chrono::steady_clock;
    auto milliseconds=[](auto a,auto b){return std::chrono::duration<double,std::milli>(b-a).count();};
    auto summary=[](std::vector<double> values){if(values.empty())return std::string("unavailable");std::sort(values.begin(),values.end());std::ostringstream out;out<<std::fixed<<std::setprecision(4)<<values[values.size()/2]<<'/'<<values[values.size()*95/100];return out.str();};
    target->Unbind();FramebufferSpecification spec;spec.Width=1280;spec.Height=720;
    spec.Attachments={FramebufferTextureFormat::RGBA8,FramebufferTextureFormat::RED_INTEGER,FramebufferTextureFormat::Depth};target=Framebuffer::Create(spec);target->Bind();glDisable(GL_DEPTH_TEST);
    auto start=Clock::now();auto font=Hazel::Font::Load("typography/DejaVuSans.ttf");auto loaded=Clock::now();
    std::ifstream corpus("typography/corpus.tsv");Check(bool(corpus),"Typography profile corpus missing");std::string source,row;
    while(std::getline(corpus,row))if(!row.empty() && row[0]!='#'){if(!source.empty())source+='\n';source+=row.substr(row.find('\t')+1);}
    auto shaping=Clock::now();auto layout=font->Layout(source,-1,false);auto shaped=Clock::now();
    std::vector<Renderer2D::TextGlyph> glyphs;Renderer2D::AppendTextGlyphs(source,font,{},glyphs);auto prepared=Clock::now();
    Check(layout->MissingGlyphs==0 && font->PageError().empty(),"Typography profile failed preparation");
    auto resources=Hazel::Font::Resources();
    std::cout<<"TYPOGRAPHY_COLD face_ms="<<milliseconds(start,loaded)<<" shape_ms="<<milliseconds(shaping,shaped)<<" pages_geometry_ms="<<milliseconds(shaped,prepared)<<" glyph_quads="<<glyphs.size()<<" pages="<<font->PageCount()<<" atlas_bytes="<<font->GetAtlasBytes()<<'\n';
    auto camera=Camera(glm::ortho(0.f,1280.f,0.f,720.f));auto transform=Transform(24,680,24);
    for(bool rebuild:{false,true}) {
        std::vector<double> prep,submit,gpu;uint64_t collected=0;
        auto timer=GPUTimer::Create();Renderer2D::Statistics stats;
        for(uint64_t frame=0;frame<140;++frame) {
            target->Bind();RenderCommand::SetClearColor({.04f,.06f,.1f,1});RenderCommand::Clear();
            Renderer2D::ResetStats();Renderer2D::BeginOrderedScene(camera,glm::mat4(1));
            auto before=Clock::now();if(rebuild){glyphs.clear();Renderer2D::AppendTextGlyphs(source,font,{},glyphs);}auto ready=Clock::now();
            timer->Begin(frame);Renderer2D::DrawTextGlyphs(glyphs,font,transform,{1,1,1,1},421);Renderer2D::EndScene();timer->End();auto done=Clock::now();
            if(frame>=20){prep.push_back(milliseconds(before,ready));submit.push_back(milliseconds(ready,done));stats=Renderer2D::GetStats();}
            auto result=timer->Latest();if(result.Available && result.Submission>=20 && result.Submission>collected){gpu.push_back(result.Milliseconds);collected=result.Submission;}
            glFlush(); // enqueue, never wait for a sample
        }
        std::cout<<"TYPOGRAPHY_WARM "<<(rebuild?"world-reprepare":"cached-paint")<<" prep_ms="<<summary(prep)<<" submit_ms="<<summary(submit)<<" gpu_interval_ms="<<summary(gpu)<<" gpu_samples="<<gpu.size()<<" draws="<<stats.DrawCalls<<" quads="<<stats.QuadCount<<'\n';
    }
    if(const auto* file=std::getenv("HAZEL_TYPOGRAPHY_CAPTURE")) {
        std::vector<uint8_t> rgb(1280*720*3);glReadBuffer(GL_COLOR_ATTACHMENT0);glPixelStorei(GL_PACK_ALIGNMENT,1);glReadPixels(0,0,1280,720,GL_RGB,GL_UNSIGNED_BYTE,rgb.data());
        std::ofstream capture(std::filesystem::u8path(file),std::ios::binary);capture<<"P6\n1280 720\n255\n";
        for(int y=719;y>=0;--y)capture.write(reinterpret_cast<const char*>(rgb.data()+y*1280*3),1280*3);
    }
    auto after=Hazel::Font::Resources();Check(after.LayoutBuilds==resources.LayoutBuilds && after.Generations==resources.Generations,"Warm text reshaped/regenerated pages");
    std::cout<<"PASS: typography cold/warm profile, zero warm shaping/page generation, asynchronous GPU collection\n";
}
