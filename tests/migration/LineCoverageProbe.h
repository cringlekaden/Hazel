#pragma once
#include "../macos/Renderer2DLines.h"
#include <fstream>
#include <filesystem>

static void LineCoverageProbe(const std::filesystem::path& directory)
{
    Check(!std::filesystem::exists(directory),"Use a fresh line-probe directory");
    std::filesystem::create_directories(directory);
    const auto cases=Renderer2DLineCases();
    std::ofstream metadata(directory/"cases.json");WriteRenderer2DLineCases(metadata,cases);
    Check(bool(metadata),"Cannot save line metadata");
    for(size_t index=0;index<cases.size();++index) {
        const auto& c=cases[index];const auto extent=c.Extent;
        FramebufferSpecification spec;spec.Width=spec.Height=extent;
        spec.Attachments={FramebufferTextureFormat::RGBA32F,FramebufferTextureFormat::RED_INTEGER,FramebufferTextureFormat::Depth};
        auto framebuffer=Framebuffer::Create(spec);framebuffer->Bind();
        RenderCommand::SetClearColor(glm::vec4(0));RenderCommand::Clear();framebuffer->ClearAttachment(1,-1);
        DrawRenderer2DLineCase(c);
        std::vector<glm::vec4> color(size_t(extent)*extent);std::vector<int32_t> ids(color.size());std::vector<float> depth(color.size());
        glReadBuffer(GL_COLOR_ATTACHMENT0);glReadPixels(0,0,extent,extent,GL_RGBA,GL_FLOAT,color.data());
        glReadBuffer(GL_COLOR_ATTACHMENT1);glReadPixels(0,0,extent,extent,GL_RED_INTEGER,GL_INT,ids.data());
        glReadPixels(0,0,extent,extent,GL_DEPTH_COMPONENT,GL_FLOAT,depth.data());
        Check(glGetError()==GL_NO_ERROR,"Line-probe GL error");
        std::ofstream raw(directory/("line-"+std::to_string(index)+".bin"),std::ios::binary);
        for(size_t i=0;i<color.size();++i) {
            raw.write(reinterpret_cast<const char*>(&color[i]),16);raw.write(reinterpret_cast<const char*>(&ids[i]),4);
            raw.write(reinterpret_cast<const char*>(&depth[i]),4);
        }
        Check(bool(raw),"Cannot save line probe");framebuffer->Unbind();
    }
    std::cout<<"PASS deterministic line capture: "<<cases.size()<<" cases; independent oracle must run separately\n";
}
