#pragma once
#include "Hazel/Renderer/Framebuffer.h"
#include <glad/glad.h>
#include <cstdlib>
// The saved native project is the fixture. No test-only scene or renderer.
static void RenderingReferenceChecks() {
    const char* selected=std::getenv("HAZEL_RENDERING_REFERENCE_PROJECT");
    if(!selected){std::cout<<"NOT EXECUTED: Rendering Reference (no explicit project selector)\n";return;}
    const auto prior=RenderCommand::GetRasterState();GLint draw=0,read=0;glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&draw);glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING,&read);
    for(bool perspective:{false,true}) {
        const auto descriptor=std::filesystem::u8path(selected);auto project=Project::LoadCandidate(descriptor);Check(bool(project),"Rendering Reference project rejected");
        const auto reference=perspective?"Scenes/Core_v1-Perspective.hazel":"Scenes/Core_v1-Orthographic.hazel";
        auto authored=project->LoadScene(reference);const auto before=SceneSerializer(authored).CaptureAuthoredState();
        const auto camera=authored->GetEntityByUUID(1);Check(camera && camera.GetName()==(perspective?"Perspective":"Orthographic") && camera.GetComponent<TransformComponent>().Translation==glm::vec3(0,0,10),"Frozen Core_v1 camera identity/pose changed");
        for(unsigned scale:{1u,2u}) {
            const unsigned width=1280*scale,height=720*scale;
            FramebufferSpecification spec;spec.Width=width;spec.Height=height;spec.Attachments={FramebufferTextureFormat::RGBA16F,FramebufferTextureFormat::RED_INTEGER,FramebufferTextureFormat::Depth};
            auto composite=Framebuffer::Create(spec);spec.Attachments={FramebufferTextureFormat::RGBA8};auto output=Framebuffer::Create(spec);
            RuntimeSession session;session.Resize(1280,720);session.ResizeFramebuffer(width,height);session.Start(project,authored);session.SetInput({-100,-100},false);
            composite->Bind();auto raster=prior;raster.DepthTest=true;raster.ClipEnabled=false;RenderCommand::SetRasterState(raster);
            RenderCommand::SetClearColor(authored->Presentation().Background);RenderCommand::Clear();composite->ClearAttachment(1,-1);session.Update(0);
            // Independent frozen projection: z=10, ortho height=10 or 45-degree FOV.
            const double worldHeight=perspective?20*std::tan(3.14159265358979323846/8):10;
            auto point=[&](double x,double y){return glm::ivec2(int((.5+x/(worldHeight*1280/720))*width),int((.5+y/worldHeight)*height));};
            auto sample=[&](double x,double y){auto p=point(x,y);glm::vec4 v;glReadBuffer(GL_COLOR_ATTACHMENT0);glReadPixels(p.x,p.y,1,1,GL_RGBA,GL_FLOAT,&v.x);return v;};
            const float expected[]{0,.21404114048223255f,.5f,1};
            for(int i=0;i<4;++i){const auto x=-5.5+1.6*i;auto v=sample(x,1.8);Check(std::abs(v.r-expected[i])<.0006f && std::abs(v.g-expected[i])<.0006f && v.a==1,"Core_v1 known linear swatch changed");auto p=point(x,1.8);Check(composite->ReadPixel(1,p.x,p.y)==int(uint32_t(session.GetScene()->GetEntityByUUID(10+i))),"Core_v1 swatch picking/framebuffer scale mismatch");}
            const auto color=sample(2.55,2.15),data=sample(4.65,2.15);
            Check(std::abs(color.g-.21586050011389926f)<.0006f && std::abs(data.g-128.f/255)<.0006f && color.r==0 && data.r==0,"Core_v1 top-left rows/color-data interpretation changed");
            const auto overlap=sample(-3,-1);Check(std::abs(overlap.r-.5f)<.0006f&&std::abs(overlap.g-.25f)<.0006f&&std::abs(overlap.b-.75f)<.0006f&&overlap.a==1,"Core_v1 linear overlapping layers changed");
            auto center=point(.3,-1);Check(composite->ReadPixel(1,center.x,center.y)==-1,"Core_v1 circle hole wrote picking");
            composite->Unbind();composite->EncodeOutput(output);output->Bind();auto p=point(-3,-1);unsigned char encoded[4]{};glReadPixels(p.x,p.y,1,1,GL_RGBA,GL_UNSIGNED_BYTE,encoded);
            Check(std::abs(int(encoded[0])-188)<=1&&std::abs(int(encoded[1])-137)<=1&&std::abs(int(encoded[2])-225)<=1&&encoded[3]==255,"Core_v1 final sRGB output mismatch");output->Unbind();session.Stop();
            Check(SceneSerializer(authored).CaptureAuthoredState()==before,"Rendering Reference Play/Stop changed authored data");
        }
    }
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER,draw);glBindFramebuffer(GL_READ_FRAMEBUFFER,read);RenderCommand::SetRasterState(prior);RenderCommand::SetViewport(prior.Viewport.x,prior.Viewport.y,prior.Viewport.z,prior.Viewport.w);
    std::cout<<"PASS: Rendering Reference Core_v1 orthographic/perspective, linear swatches/color-data/alpha, final output, picking and explicit 2x framebuffer override through normal RuntimeSession\n";
}
