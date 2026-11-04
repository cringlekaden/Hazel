#pragma once
#include "Hazel/Renderer/Color.h"
#include "Hazel/Assets/ProjectAssets.h"
// Independent constants are IEC sRGB vectors, not values computed by the
// production helpers. GPU float error <= 6e-4 accounts for RGBA16F quantization;
// final RGBA8 channels permit one code value of rounding.
static void CanonicalColorChecks() {
    using namespace Hazel;
    Check(std::abs(ColorSpace::FromSRGB(.5f)-.21404114048223255)<1e-7,"sRGB .5 transfer vector");
    Check(std::abs(ColorSpace::ToSRGB(.5f)-.7353569830524495)<1e-7,"Linear .5 transfer vector");
    Check(std::abs(ColorSpace::FromSRGB(.04045f)-.00313080495356037)<1e-8,"sRGB lower segment");
    GLint defaultDraw=0;glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&defaultDraw);glBindFramebuffer(GL_DRAW_FRAMEBUFFER,0);
    GLint windowEncoding=0;glGetFramebufferAttachmentParameteriv(GL_DRAW_FRAMEBUFFER,GL_BACK_LEFT,GL_FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING,&windowEncoding);glBindFramebuffer(GL_DRAW_FRAMEBUFFER,defaultDraw);
    std::cout<<"Actual window attachment encoding: "<<(windowEncoding==GL_SRGB?"sRGB":"linear")<<"; final shader encodes with FRAMEBUFFER_SRGB disabled\n";
    const auto converted=ColorSpace::FromSRGB(glm::vec4(.5f,.5f,.5f,.5f));Check(converted.a==.5f,"Color transfer changed alpha");
    FramebufferSpecification spec;spec.Width=spec.Height=32;spec.Attachments={FramebufferTextureFormat::RGBA16F,FramebufferTextureFormat::RED_INTEGER};
    auto linear=Framebuffer::Create(spec);spec.Attachments={FramebufferTextureFormat::RGBA8};auto output=Framebuffer::Create(spec);
    const auto prior=RenderCommand::GetRasterState();const auto priorBlend=glIsEnabledi(GL_BLEND,0);const auto priorSRGB=glIsEnabled(GL_FRAMEBUFFER_SRGB);
    TextureSpecification ts;ts.GenerateMips=false;ts.MinFilter=ts.MagFilter=TextureFilter::Linear;
    const unsigned char middle[]{128,128,128,128};
    auto draw=[&](const Ref<Texture2D>& texture) {
        linear->Bind();glDisable(GL_DEPTH_TEST);glDisablei(GL_BLEND,0);linear->ClearAttachment(1,-1);
        RenderCommand::SetClearColor({0,0,0,0});RenderCommand::Clear();
        Renderer2D::BeginScene(Camera(glm::mat4(1)),glm::mat4(1));Renderer2D::DrawQuad(glm::mat4(1),texture,1,glm::vec4(1),123);Renderer2D::EndScene();
    };
    for(bool color:{true,false}) {
        ts.Encoding=color?TextureEncoding::SRGB:TextureEncoding::Linear;auto texture=Texture2D::Create(ts);texture->SetData(middle,sizeof(middle));draw(texture);
        float pixel[4]{};glReadBuffer(GL_COLOR_ATTACHMENT0);glReadPixels(16,16,1,1,GL_RGBA,GL_FLOAT,pixel);
        const float expected=color?.21586050011389926f:128.f/255;
        Check(std::abs(pixel[0]-expected)<.0006f && std::abs(pixel[3]-128.f/255)<.0006f,"GPU color/data/alpha interpretation");
        Check(linear->ReadPixel(1,16,16)==123,"Linear target lost entity ID");
        glEnable(GL_FRAMEBUFFER_SRGB);linear->EncodeOutput(output);Check(glIsEnabled(GL_FRAMEBUFFER_SRGB),"Output failed to restore framebuffer encoding state");
        output->Bind();unsigned char encoded[4]{};glReadPixels(16,16,1,1,GL_RGBA,GL_UNSIGNED_BYTE,encoded);
        Check(std::abs(int(encoded[0])-(color?128:188))<=1 && std::abs(int(encoded[3])-128)<=1,"Final output did not encode exactly once or changed alpha");
        output->Unbind();linear->EncodeOutput();glReadBuffer(GL_BACK);glReadPixels(16,16,1,1,GL_RGBA,GL_UNSIGNED_BYTE,encoded);
        Check(std::abs(int(encoded[0])-(color?128:188))<=1,"Player window output did not encode exactly once");
    }
    // Linear filtering and mip generation happen after color decode. Alpha/data
    // retain their numerical meaning, including half coverage.
    ts.Width=ts.Height=2;ts.Encoding=TextureEncoding::SRGB;ts.GenerateMips=true;ts.MinFilter=TextureFilter::LinearMipmapLinear;
    const unsigned char checker[]{0,0,0,128,255,255,255,128,255,255,255,128,0,0,0,128};auto filtered=Texture2D::Create(ts);filtered->SetData(checker,sizeof(checker));
    filtered->Bind();unsigned char mip[4]{};glGetTexImage(GL_TEXTURE_2D,1,GL_RGBA,GL_UNSIGNED_BYTE,mip);
    Check(std::abs(int(mip[0])-188)<=1 && mip[3]==128,"Color mip was averaged in encoded space or transferred alpha");
    linear->Bind();glDisable(GL_DEPTH_TEST);glDisablei(GL_BLEND,0);RenderCommand::SetClearColor({0,0,0,0});RenderCommand::Clear();
    ResolvedSprite fixed;fixed.Texture=filtered;for(auto& uv:fixed.UV)uv={.5f,.5f};
    Renderer2D::BeginScene(Camera(glm::mat4(1)),glm::mat4(1));Renderer2D::DrawSprite(glm::mat4(1),fixed,glm::vec4(1),124);Renderer2D::EndScene();
    float sample[4]{};glReadBuffer(GL_COLOR_ATTACHMENT0);glReadPixels(16,16,1,1,GL_RGBA,GL_FLOAT,sample);Check(std::abs(sample[0]-.5f)<.0006f,"Color filter did not interpolate linear samples");
    glEnablei(GL_BLEND,0);glBlendFuncSeparatei(0,GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA,GL_ONE,GL_ONE_MINUS_SRC_ALPHA);
    RenderCommand::SetClearColor({0,0,0,0});RenderCommand::Clear();
    Renderer2D::BeginScene(Camera(glm::mat4(1)),glm::mat4(1));Renderer2D::DrawQuad(glm::mat4(1),{1,0,0,.5f},125);Renderer2D::DrawQuad(glm::mat4(1),{0,0,1,.5f},126);Renderer2D::EndScene();
    glReadPixels(16,16,1,1,GL_RGBA,GL_FLOAT,sample);Check(std::abs(sample[0]-.25f)<.0006f&&std::abs(sample[2]-.5f)<.0006f&&std::abs(sample[3]-.75f)<.0006f,"Linear RGB/coverage source-over vector");
    linear->Unbind();RenderCommand::SetRasterState(prior);RenderCommand::SetViewport(prior.Viewport.x,prior.Viewport.y,prior.Viewport.z,prior.Viewport.w);
    if(priorBlend)glEnablei(GL_BLEND,0);else glDisablei(GL_BLEND,0);if(priorSRGB)glEnable(GL_FRAMEBUFFER_SRGB);else glDisable(GL_FRAMEBUFFER_SRGB);
    std::cout<<"PASS: canonical color CPU vectors, GPU color/data/alpha, linear filtering/mips, alpha overlap, single final encoding and state restoration\n";
}
