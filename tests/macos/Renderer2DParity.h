#pragma once
// Identical shared Renderer2D workload for native Metal and OpenGL readbacks.
#include "Hazel/Renderer/Drawing/Renderer2D.h"
#include "Hazel/Renderer/RenderCommand.h"
#include "Hazel/Scene/RuntimeSession.h"
#include "Hazel/Scene/Scene.h"
#include <stdexcept>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <array>
#include <cstdint>

inline void DrawRenderer2DParity(unsigned phase)
{
    using namespace Hazel;
    RenderCommand::StateScope scope;
    auto raster = RenderCommand::GetRasterState();
    raster.DepthTest = raster.DepthWrite = false;
    raster.Blend = true;
    raster.ColorMask = raster.PickingMask = glm::bvec4(true);
    raster.ClipEnabled = false;
    RenderCommand::SetRasterState(raster);
    const auto transform = [](float x, float y, float w, float h) {
        return glm::translate(glm::mat4(1), {x,y,0}) * glm::scale(glm::mat4(1), {w,h,1});
    };
    Camera camera(glm::ortho(0.f,128.f,0.f,128.f,-1.f,1.f));
    if (phase == 3) {
        Renderer2D::BeginScene(camera,glm::mat4(1));
        Renderer2D::DrawCircle(transform(64,64,64,64),{0,0,1,.5f},1,.02f,-2000000001);
        Renderer2D::DrawQuad(transform(64,64,32,32),{1,0,0,.5f},2147483647);
        Renderer2D::EndScene();
        return;
    }
    Renderer2D::BeginOrderedScene(camera, glm::mat4(1));
    Renderer2D::DrawQuad(transform(64,64,128,128), {.2f,.4f,.6f,.25f}, -2147483647);
    Renderer2D::DrawQuad(transform(40,40,64,64), {1,0,0,.5f}, 2147483647);
    Renderer2D::DrawCircle(transform(68,42,48,48), {0,1,0,.5f}, 1,.02f,-2000000001);
    Renderer2D::SetPrimitiveClip({17,19,31,27});
    Renderer2D::SetPickingEnabled(false);
    Renderer2D::DrawQuad(transform(32,32,60,60), {0,0,1,.5f}, 71);
    Renderer2D::SetPickingEnabled(true);
    Renderer2D::SetPrimitiveClip({-1,-1,-1,-1});
    Renderer2D::SetLineWidth(1);
    Renderer2D::DrawLine({8.5f,75.5f,0},{119.5f,75.5f,0},{1,.5f,0,.75f},-987654321);
    Renderer2D::DrawLine({8.5f,80.5f,0},{119.5f,96.5f,0},{1,.5f,0,.75f},-987654321);
    Renderer2D::DrawLine({122.5f,7.5f,0},{122.5f,117.5f,0},{1,.5f,0,.75f},-987654321);
    Renderer2D::TextParams text; text.Color={.9f,.7f,.5f,.65f};
    Renderer2D::DrawString(phase ? "Atlas: é Ω Ж ﬁ" : "Hazel 2D", Font::GetDefault(), transform(8,104,12,12), text, -123456789);
    TextureSpecification spec;
    spec.Width=spec.Height=2; spec.GenerateMips=phase==2;
    spec.MinFilter=spec.MagFilter=TextureFilter::Nearest;
    spec.WrapS=TextureWrap::MirroredRepeat;
    auto texture=Texture2D::Create(spec);
    std::array<uint32_t,4> texels{0x800000ff,0x8000ff00,0x80ff0000,0x80ffffff};
    texture->SetData(texels.data(),sizeof(texels));
    Renderer2D::DrawQuad(transform(102,32,24,32),texture,1,{.5f,1,.75f,.5f},-1700000001);
    Renderer2D::EndScene(); // Match GL texture-update ordering; prior Metal fixtures test retained versions.
    Renderer2D::BeginOrderedScene(camera,glm::mat4(1));
    std::swap(texels[0],texels[2]); texture->SetData(texels.data(),sizeof(texels));
    Renderer2D::DrawQuad(transform(102,64,24,24),texture,1,{1,.5f,1,.75f},1700000001);
    if(phase==2) {
        for(unsigned i=0;i<9;++i) {
            auto next=Texture2D::Create(spec);
            texels[0]=0xff000000 | (17*i); next->SetData(texels.data(),sizeof(texels));
            Renderer2D::DrawQuad(transform(5.f+13*i,88,10,8),next,1,glm::vec4(1),-100-int(i));
        }
    }
    Renderer2D::EndScene();
}

// Exercise RuntimeSession's real input router and focus-outline ownership without
// claiming physical device input or presentation. Both backends use this helper.
inline void FocusRenderer2DRuntimeUI(Hazel::RuntimeSession& session)
{
    using namespace Hazel;
    Renderer2D::SetLineWidth(2);
    GameUI::DeviceSample sample;sample.Point={-1,-1};
    session.SetInput(sample.Point,true);
    session.RouteUIInput(sample,1.f/60,true);
    session.RouteUIInput(sample,1.f/60,true);
    sample.Keys[Key::Tab]=true;session.RouteUIInput(sample,1.f/60,true);
    sample.Keys[Key::Tab]=false;session.RouteUIInput(sample,1.f/60,true);
    sample.Keys[Key::Enter]=true;session.RouteUIInput(sample,1.f/60,true);
    auto scene=session.GetScene();GameUI::Action action;
    if(!scene->GetUIDiagnostics().Focus || !scene->ConsumeUIAction(0,action) ||
        action.Entity!=scene->GetUIDiagnostics().Focus || scene->ConsumeUIAction(0,action))
        throw std::runtime_error("Runtime UI Tab/Accept focus/activation was lost or duplicated");
    sample.Keys[Key::Enter]=false;session.RouteUIInput(sample,1.f/60,true);
}
