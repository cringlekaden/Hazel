#pragma once
// Shared workloads only. The independent FP64 coverage/depth/ID oracle lives in
// scripts/macos/metal_m3_lines.py and does not call production geometry code.
#include "Hazel/Renderer/Drawing/Renderer2D.h"
#include "Hazel/Renderer/RenderCommand.h"
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <ostream>
#include <stdexcept>
#include <vector>

struct Renderer2DLineSegment
{
    glm::vec3 A, B;
    glm::vec4 Color{1};
    int32_t ID = -2000000001;
    bool Picking = true;
    float Width = 0; // zero inherits the case width
};
struct Renderer2DLineCase
{
    const char* Name;
    uint32_t Extent = 64;
    float Width = 1;
    glm::mat4 Camera{1};
    glm::ivec4 Clip{-1};
    bool Depth = false, Rectangle = false, Capacity = false;
    std::vector<Renderer2DLineSegment> Segments;
};
inline std::vector<Renderer2DLineCase> Renderer2DLineCases()
{
    std::vector<Renderer2DLineCase> cases;
    const auto ortho=glm::ortho(0.f,64.f,0.f,64.f,-1.f,1.f);
    const auto add=[&](const char* name,glm::vec3 a,glm::vec3 b,float width=1,float alpha=1) {
        Renderer2DLineCase c;c.Name=name;c.Camera=ortho;c.Width=width;
        c.Segments.push_back({a,b,{1,1,1,alpha},-2000000001-int32_t(cases.size())});
        cases.push_back(c);
    };
    for(float phase:{0.f,.125f,.5f,.875f}) for(float angle:{0.f,15.f,30.f,45.f,75.f,90.f,135.f,180.f}) {
        const float radians=glm::radians(angle);
        const glm::vec3 center(32+phase,32+phase,0),delta(20*std::cos(radians),20*std::sin(radians),0);
        add("angle-phase",center-delta,center+delta);
    }
    for(bool reverse:{false,true}) {
        const auto pair=[&](const char* name,glm::vec3 a,glm::vec3 b) {add(name,reverse?b:a,reverse?a:b);};
        pair("endpoint",{8.25f,16.75f,0},{53.5f,45.125f,0});
        pair("short",{31.25f,31.5f,0},{31.875f,31.75f,0});
        pair("degenerate",{32,32,0},{32,32,0});
    }
    for(float width:{.5f,1.f,2.f,4.f,16.f}) add("width",{8.25f,12.25f,0},{55.5f,48.5f,0},width);
    add("dpi-2x",{8.25f,12.25f,0},{55.5f,48.5f,0});cases.back().Extent=128;
    add("fragment-clip",{4,8,0},{60,52,0},4);cases.back().Clip={17,19,31,27};
    add("viewport-clip",{-8,12,0},{72,52,0},4);
    add("outside-wide",{-.25f,4,0},{-.25f,60,0},4);
    add("alpha",{8.25f,12.25f,0},{55.5f,48.5f,0},4,.375f);
    add("transparent",{8.25f,12.25f,0},{55.5f,48.5f,0},4,0);
    for(bool passive:{false,true}) {
        add(passive?"passive-overlap":"overlap",{8.25f,12.25f,0},{55.5f,48.5f,0},4,.375f);
        auto segment=cases.back().Segments.front();segment.Color={.25f,.5f,1,.5f};segment.ID=2147483647;segment.Picking=!passive;
        cases.back().Segments.push_back(segment);
    }
    add("intersection",{8,32,0},{56,32,0},4,.5f);
    cases.back().Segments.push_back({{32,8,0},{32,56,0},{1,0,0,.5f},2147483647});
    add("rectangle",{16,16,0},{48,16,0},4,.5f);
    auto& rectangle=cases.back();rectangle.Rectangle=true;
    const auto id=rectangle.Segments.front().ID;
    rectangle.Segments.push_back({{48,16,0},{48,48,0},{1,1,1,.5f},id});
    rectangle.Segments.push_back({{48,48,0},{16,48,0},{1,1,1,.5f},id});
    rectangle.Segments.push_back({{16,48,0},{16,16,0},{1,1,1,.5f},id});
    const auto perspective=glm::perspective(glm::radians(60.f),1.f,.1f,10.f);
    add("perspective",{-.5f,-.3f,-1},{.6f,.4f,-2},4);cases.back().Camera=perspective;cases.back().Depth=true;
    add("near-clip",{-.03f,.02f,-.05f},{.5f,.3f,-2},4);cases.back().Camera=perspective;cases.back().Depth=true;
    add("behind-near",{-.03f,.02f,-.05f},{.03f,.02f,-.08f},4);cases.back().Camera=perspective;cases.back().Depth=true;
    add("far-clip",{-.5f,-.3f,-1},{.6f,.4f,-20},4);cases.back().Camera=perspective;cases.back().Depth=true;
    add("depth-order",{8,32,.5f},{56,32,-.5f},4,.5f);cases.back().Depth=true;
    cases.back().Segments.push_back({{32,8,0},{32,56,0},{0,1,0,.5f},2147483647});
    add("variable-width-run",{8,16,0},{56,16,0},.5f);
    cases.back().Segments.push_back({{8,32,0},{56,32,0},{1,1,1,1},2147483647,true,2});
    cases.back().Segments.push_back({{8,48,0},{56,48,0},{1,1,1,1},-2147483647,true,8});
    add("capacity",{8.25f,12.25f,0},{55.5f,48.5f,0},4);cases.back().Capacity=true;
    add("raster-boundary",{8,32,0},{41.0001f,32,0});
    add("raster-boundary-reversed",{41.0001f,32,0},{8,32,0});
    return cases;
}
inline void DrawRenderer2DLineCase(const Renderer2DLineCase& c)
{
    using namespace Hazel;
    RenderCommand::StateScope scope;
    auto raster=RenderCommand::GetRasterState();
    raster.DepthTest=raster.DepthWrite=c.Depth;raster.Compare=DepthCompare::Less;
    raster.Blend=true;raster.ColorMask=raster.PickingMask=glm::bvec4(true);raster.ClipEnabled=false;
    RenderCommand::SetRasterState(raster);
    Renderer2D::ResetStats();
    Renderer2D::BeginOrderedScene(Camera(c.Camera),glm::mat4(1));
    Renderer2D::SetLineWidth(c.Width);Renderer2D::SetPrimitiveClip(c.Clip);
    if(c.Capacity) for(unsigned i=0;i<40000;++i)
        Renderer2D::DrawLine({1000,1000,0},{1001,1001,0},{1,1,1,1},42);
    if(c.Rectangle) Renderer2D::DrawRect({32,32,0},{32,32},c.Segments.front().Color,c.Segments.front().ID);
    else for(const auto& segment:c.Segments) {
        Renderer2D::SetLineWidth(segment.Width>0?segment.Width:c.Width);
        Renderer2D::SetPickingEnabled(segment.Picking);
        Renderer2D::DrawLine(segment.A,segment.B,segment.Color,segment.ID);
    }
    Renderer2D::EndScene();
    if(c.Capacity) {
        const auto stats=Renderer2D::GetStats();
        if(stats.DrawCalls!=2 || stats.GeometryCount!=40001 ||
            stats.Flushes[size_t(Renderer2D::FlushReason::Capacity)]!=1)
            throw std::runtime_error("Expanded line capacity/statistics regression");
    }
}
inline void WriteRenderer2DLineCases(std::ostream& out,const std::vector<Renderer2DLineCase>& cases)
{
    const auto vec=[&](glm::vec4 v){out<<'['<<v.x<<','<<v.y<<','<<v.z<<','<<v.w<<']';};
    out<<std::setprecision(10)<<"{\"contract\":\"pixel-area butt segments v1\",\"cases\":[\n";
    for(size_t i=0;i<cases.size();++i) {
        const auto& c=cases[i];
        out<<(i?",\n":"")<<"{\"name\":\""<<c.Name<<"\",\"file\":\"line-"<<i<<".bin\",\"extent\":"<<c.Extent<<",\"width\":"<<c.Width
            <<",\"depth\":"<<(c.Depth?"true":"false")<<",\"clip\":";vec(glm::vec4(c.Clip));out<<",\"segments\":[";
        for(size_t j=0;j<c.Segments.size();++j) {
            const auto& s=c.Segments[j];out<<(j?",":"")<<"{\"a\":";vec(c.Camera*glm::vec4(s.A,1));out<<",\"b\":";vec(c.Camera*glm::vec4(s.B,1));
            out<<",\"width\":"<<(s.Width>0?s.Width:c.Width)<<",\"color\":";vec(s.Color);out<<",\"id\":"<<s.ID<<",\"picking\":"<<(s.Picking?"true":"false")<<'}';
        }
        out<<"]}";
    }
    out<<"\n]}\n";
}
