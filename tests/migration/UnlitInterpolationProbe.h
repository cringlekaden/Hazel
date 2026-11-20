#pragma once
#include "Hazel/Renderer/Drawing/MeshGeometry.h"
#include "Hazel/Core/FileSystem.h"
#include <iomanip>
#include <utility>
// Diagnostic only: independent GLSL 410 isolates raster interpolation from the
// production material, sampler and lighting path. Never used as a color oracle.
static void UnlitInterpolationProbe(const SceneSubmission& frame, unsigned width, unsigned height)
{
    const bool diagnostic=!FileSystem::GetEnvironmentPath("HAZEL_UNLIT_INTERPOLATION_PROBE").empty();
    if(!diagnostic)return;
    RenderCommand::StateScope state;
    FramebufferSpecification spec;spec.Width=width;spec.Height=height;
    spec.Attachments={FramebufferTextureFormat::RGBA32F,FramebufferTextureFormat::RED_INTEGER,FramebufferTextureFormat::Depth};
    auto probeTarget=Framebuffer::Create(spec);
    for(const char* qualifier : {"smooth", "flat"}) {
        const std::string varying=std::string(qualifier)+" ";
        const std::string vertex="#version 410 core\nlayout(location=0) in vec3 a_Position;layout(location=4) in vec4 a_Color;uniform mat4 u_MVP;"+varying+"out vec4 v_Color;void main(){gl_Position=u_MVP*vec4(a_Position,1);v_Color=a_Color;}";
        const std::string fragment="#version 410 core\n"+varying+"in vec4 v_Color;uniform vec3 u_Base;uniform int u_ID;layout(location=0) out vec4 o_Color;layout(location=1) out int o_ID;void main(){o_Color=vec4(u_Base*v_Color.rgb,1);o_ID=u_ID;}";
        GLuint vs=glCreateShader(GL_VERTEX_SHADER),fs=glCreateShader(GL_FRAGMENT_SHADER),program=glCreateProgram();
        GLint priorProgram=0;glGetIntegerv(GL_CURRENT_PROGRAM,&priorProgram);
        struct Cleanup {GLuint VS,FS,Program;GLint Prior;~Cleanup(){glUseProgram(GLuint(Prior));glDeleteProgram(Program);glDeleteShader(VS);glDeleteShader(FS);}} cleanup{vs,fs,program,priorProgram};
        for(auto entry : {std::pair<GLuint,const std::string&>{vs,vertex},{fs,fragment}}) {
            const char* text=entry.second.c_str();glShaderSource(entry.first,1,&text,nullptr);glCompileShader(entry.first);
            GLint okay=0;glGetShaderiv(entry.first,GL_COMPILE_STATUS,&okay);Check(okay,"Interpolation probe shader compile failed");glAttachShader(program,entry.first);
        }
        glLinkProgram(program);GLint linked=0;glGetProgramiv(program,GL_LINK_STATUS,&linked);Check(linked,"Interpolation probe link failed");
        probeTarget->Bind();RenderCommand::SetClearColor({0,0,0,1});RenderCommand::Clear();probeTarget->ClearAttachment(1,-1);
        GLint bits=0,type=0;glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_FRAMEBUFFER_ATTACHMENT_RED_SIZE,&bits);glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_FRAMEBUFFER_ATTACHMENT_COMPONENT_TYPE,&type);
        Check(bits==32 && type==GL_FLOAT,"Interpolation probe requires actual FP32 output");
        auto raster=RenderCommand::GetRasterState();raster.DepthTest=true;raster.DepthWrite=true;raster.Compare=DepthCompare::Less;raster.Blend=false;raster.Cull=CullFace::None;RenderCommand::SetRasterState(raster);
        glUseProgram(program);
        for(const auto& mesh : frame.Meshes) {
            const auto mvp=frame.Camera.ViewProjection*mesh.World;
            glUniformMatrix4fv(glGetUniformLocation(program,"u_MVP"),1,GL_FALSE,&mvp[0][0]);
            const auto base=mesh.Materials[0]->BaseColor;
            glUniform3fv(glGetUniformLocation(program,"u_Base"),1,&base.x);glUniform1i(glGetUniformLocation(program,"u_ID"),mesh.Picking);
            for(const auto& vertexInput:mesh.Geometry->Geometry.Vertices)
                Check(vertexInput.Color==glm::vec4(1),"Probe requires independently checked white mesh vertex colors");
            auto geometry=frame.Assets->RealizeMesh(mesh.Geometry);geometry->Vertices()->Bind();
            for(const auto& primitive:mesh.Geometry->Geometry.Primitives)
                glDrawElements(GL_TRIANGLES,GLsizei(primitive.IndexCount),GL_UNSIGNED_INT,reinterpret_cast<const void*>(size_t(primitive.FirstIndex)*sizeof(uint32_t)));
        }
        std::vector<glm::vec4> colors(size_t(width)*height);std::vector<int> ids(colors.size());
        glReadBuffer(GL_COLOR_ATTACHMENT0);glReadPixels(0,0,width,height,GL_RGBA,GL_FLOAT,colors.data());
        glReadBuffer(GL_COLOR_ATTACHMENT1);glReadPixels(0,0,width,height,GL_RED_INTEGER,GL_INT,ids.data());
        double maximum=0;size_t pixels=0,failures=0;glm::vec3 first{},expected{};size_t firstIndex=0;
        for(size_t i=0;i<ids.size();++i)for(const auto& mesh:frame.Meshes)if(ids[i]==mesh.Picking) {
            ++pixels;const auto base=glm::vec3(mesh.Materials[0]->BaseColor);
            double error=0;for(int c=0;c<3;++c)error=std::max(error,std::abs(double(colors[i][c])-double(base[c])));
            maximum=std::max(maximum,error);if(error>1e-6){if(!failures){first=colors[i];expected=base;firstIndex=i;}++failures;}
        }
        std::cout<<"UNLIT_INTERPOLATION qualifier="<<qualifier<<" output_bits="<<bits<<" white_vertex_colors=1 sampler=none pixels="<<pixels<<" above_1e-6="<<failures<<" max_abs="<<std::setprecision(12)<<maximum<<" first="<<firstIndex%width<<','<<firstIndex/width<<" actual="<<first.x<<','<<first.y<<','<<first.z<<" expected="<<expected.x<<','<<expected.y<<','<<expected.z<<'\n';
        Check(pixels>100,"Interpolation probe had no meaningful coverage");Check(glGetError()==GL_NO_ERROR,"Interpolation probe GL error");
        if(std::string(qualifier)=="flat")Check(maximum<=1e-6,"Flat interpolation control lost constant-color precision");
        glUseProgram(0);probeTarget->Unbind();
    }
}
