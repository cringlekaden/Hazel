#pragma once
#include "IBLQualityMath.h"
#include "Hazel/Renderer/Resources/EnvironmentResources.h"
static void IBLQualityLobeChecks()
{
    RenderCommand::StateScope scope;
    const auto selector=FileSystem::GetEnvironmentPath("HAZEL_RENDERING_REFERENCE_PROJECT");
    Check(!selector.empty(),"Quality lobe requires Rendering Reference");
    auto project=Project::LoadCandidate(selector);auto root=project->GetAssetRoot();
    auto prepared=ReadPreparedEnvironment(root,"Reference/IBLQuality_v2/TinyEmitter.henv");
    auto source=DecodeRadianceHDR(ReadAssetBytes(root/"Reference/IBLQuality_v2/TinyEmitter.hdr",64*1024*1024));
    IBLSourceQuadrature reference(source,4);
    auto cube=TextureCube::Create(prepared.Specular);cube->Bind(6);
    auto shader=Shader::Create("QualityLobeProbe",R"(#version 410 core
void main(){vec2 p=vec2((gl_VertexID<<1)&2,gl_VertexID&2);gl_Position=vec4(p*2-1,0,1);})",R"(#version 410 core
layout(location=0)out vec4 color;uniform samplerCube image;uniform vec3 direction;uniform float level;
void main(){color=vec4(textureLod(image,direction,level).rgb,1);})");
    auto triangle=VertexArray::Create();uint32_t indices[]={0,1,2};triangle->SetIndexBuffer(IndexBuffer::Create(indices,3));
    FramebufferSpecification s;s.Width=s.Height=1;s.Attachments={FramebufferTextureFormat::RGBA32F};auto target=Framebuffer::Create(s);target->Bind();
    auto raster=RenderCommand::GetRasterState();raster.DepthTest=raster.DepthWrite=raster.Blend=raster.SRGB=raster.ClipEnabled=false;raster.SeamlessCubes=true;raster.Cull=CullFace::None;raster.ColorMask=glm::bvec4(true);RenderCommand::SetRasterState(raster);
    shader->Bind();shader->SetInt("image",6);
    const double pi=std::acos(-1.),theta=pi*50.5/128,phi=2*pi*(139.5/256-.5);
    glm::dvec3 centre(std::sin(theta)*std::cos(phi),std::cos(theta),std::sin(theta)*std::sin(phi));
    auto tangent=glm::normalize(glm::cross(glm::dvec3(0,1,0),centre));
    auto direction=[&](double angle){return centre*std::cos(angle)+tangent*std::sin(angle);};
    auto gpu=[&](double angle){shader->SetFloat3("direction",glm::vec3(direction(angle)));RenderCommand::DrawIndexed(triangle,3);return double(target->ReadColorPixel(0,0,0).r);};
    auto halfWidth=[&](auto evaluate,double peak){double low=0,high=pi/2,half=(peak+source.RGB[0])*.5;for(int i=0;i<18;++i){double middle=(low+high)*.5;if(evaluate(middle)>half)low=middle;else high=middle;}return(low+high)*.5;};
    double maximumPeakError=0,maximumWidthDegrees=0,maximumOffLobe=0;
    for(unsigned level=1;level<prepared.Specular.Mips.size();++level)
    {
        double r=double(level)/(prepared.Specular.Mips.size()-1);shader->SetFloat("level",float(level));
        auto cpu=[&](double angle){return reference.Integrate(direction(angle),r,false).r;};
        double peak=cpu(0),sampledPeak=gpu(0),cpuWidth=halfWidth(cpu,peak),gpuWidth=halfWidth(gpu,sampledPeak);
        double peakError=std::abs(sampledPeak-peak)/peak,widthError=std::abs(cpuWidth-gpuWidth);
        unsigned side=prepared.Specular.Side>>level;
        double angularCell=std::max(2*pi/source.Width,2*std::atan(1./side));
        Check(peakError<.05,"Finite HDR emitter peak lost or amplified on actual GPU level");
        Check(widthError<angularCell,"Specular lobe broadened beyond source/output angular sampling");
        for(double multiple:{.5,1.,1.5,2.})
        {
            double angle=std::min(pi,cpuWidth*multiple),error=std::abs(gpu(angle)-cpu(angle))/peak;
            Check(error<.05,"Actual GPU specular off-lobe spike or shape error");maximumOffLobe=std::max(maximumOffLobe,error);
        }
        maximumPeakError=std::max(maximumPeakError,peakError);maximumWidthDegrees=std::max(maximumWidthDegrees,widthError*180/pi);
        std::cout<<"IBL GPU lobe r="<<r<<" peak error="<<peakError<<" FWHM reference="<<cpuWidth*360/pi<<" GPU="<<gpuWidth*360/pi<<" degrees\n";
    }
    target->Unbind();Check(glGetError()==GL_NO_ERROR,"IBL lobe GPU error");
    std::cout<<"PASS: actual GPU tiny-emitter level peaks/half-width/shape; max peak "<<maximumPeakError<<" half-width delta "<<maximumWidthDegrees<<" degrees off-lobe "<<maximumOffLobe<<'\n';
}
