#pragma once
#include "Hazel/Renderer/Resources/EnvironmentResources.h"
// Distinct values in every face/level prove explicit upload and real sampled
// fractional LOD, independently of a plausible-looking filtered environment.
static LightingImage IBLMipChecks()
{
    RenderCommand::StateScope scope;
    LightingImage image;
    image.Kind=LightingImageKind::Specular;
    image.Side=32;
    image.EncodingVersion=2;
    for(unsigned side=32,level=0;side>=4;side/=2,++level)
    {
        auto &m=image.Mips.emplace_back(size_t(side)*side*18);
        for(unsigned f=0;f<6;++f)for(unsigned i=0;i<side*side;++i)
            for(unsigned c=0;c<3;++c)m[(size_t(f)*side*side+i)*3+c]=.25f*(level+1)+.03125f*f+.00390625f*c;
    }
    auto portable=EncodeLightingImage(image);
    auto decoded=DecodeLightingImage(portable);
    auto cube=TextureCube::Create(decoded);
    cube->Bind(6);
    unsigned side=decoded.Side;
    for(unsigned level=0;level<decoded.Mips.size();++level,side/=2)
        for(unsigned f=0;f<6;++f)
        {
            std::vector<float> gpu(size_t(side)*side*3);
            glGetTexImage(GL_TEXTURE_CUBE_MAP_POSITIVE_X+f,level,GL_RGB,GL_FLOAT,gpu.data());
            auto at=decoded.Mips[level].begin()+size_t(f)*side*side*3;
            Check(std::equal(gpu.begin(),gpu.end(),at),"CPU artifact -> actual GPU cube face/mip changed");
        }
    auto program=Shader::Create("ExplicitIBLLevels",R"(#version 410 core
void main(){vec2 p=vec2((gl_VertexID<<1)&2,gl_VertexID&2);gl_Position=vec4(p*2-1,0,1);})",R"(#version 410 core
layout(location=0)out vec4 color;uniform samplerCube levels;uniform vec3 direction;uniform float lod;
void main(){color=vec4(textureLod(levels,direction,lod).rgb,1);})");
    auto triangle=VertexArray::Create();uint32_t indices[]={0,1,2};triangle->SetIndexBuffer(IndexBuffer::Create(indices,3));
    FramebufferSpecification s;s.Width=s.Height=4;s.Attachments={FramebufferTextureFormat::RGBA32F};auto target=Framebuffer::Create(s);target->Bind();
    auto state=RenderCommand::GetRasterState();state.DepthTest=state.Blend=state.SRGB=state.ClipEnabled=false;state.Cull=CullFace::None;state.ColorMask=glm::bvec4(true);RenderCommand::SetRasterState(state);
    program->Bind();program->SetInt("levels",6);
    for(unsigned f=0;f<6;++f)for(float lod:{0.f,.25f,1.f,1.5f,2.75f,3.f})
    {
        program->SetFloat3("direction",glm::vec3(CubeDirection(f,.5,.5)));program->SetFloat("lod",lod);
        RenderCommand::DrawIndexed(triangle,3);
        auto pixel=target->ReadColorPixel(0,2,2);
        for(unsigned c=0;c<3;++c)Check(std::abs(pixel[c]-(.25f*(lod+1)+.03125f*f+.00390625f*c))<.0001f,
            "Explicit integer/fractional cubemap LOD sampled the wrong level");
    }
    program->SetFloat3("direction",{0,0,1});program->SetFloat("lod",.135f);
    RenderCommand::DrawIndexed(triangle,3);
    auto quantized=target->ReadColorPixel(0,2,2);
    std::cout<<"Hardware fractional LOD .135: sampled red "<<quantized.r
        <<"; ideal FP32 "<<(.25*1.135+.03125*4)<<" (PBR separately verifies explicit FP32 level interpolation)\n";
    target->Unbind();
    Check(glGetError()==GL_NO_ERROR,"Explicit mip readback OpenGL error");
    std::cout<<"PASS: portable tail-four specular data, all actual GPU faces/mips and distinct integer/fractional textureLod levels\n";
    return decoded;
}
