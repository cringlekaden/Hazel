#pragma once
#include "Renderer3DParity.h"
inline void XeGTAOQualification(const std::filesystem::path& output,
    const std::function<Hazel::Ref<const Hazel::FramebufferReadback>(const Hazel::Ref<Hazel::Framebuffer>&)>& capture)
{
    using namespace Hazel;
    auto project=Project::LoadCandidate(std::filesystem::absolute("examples/RenderingReference/RenderingReference-Direct-PerspectivePBR.hproj"));
    auto assets=CreateRef<ProjectAssets>(project->GetAssetRoot());
    auto mesh=assets->Mesh("Native/Direct_v1/UnitCube.hmesh");
    if(!mesh.Accepted)throw std::runtime_error(mesh.Error);
    FramebufferSpecification spec;spec.Width=320;spec.Height=180;
    spec.Attachments={FramebufferTextureFormat::RGBA32F,FramebufferTextureFormat::RED_INTEGER,FramebufferTextureFormat::DEPTH32F};
    auto display=Framebuffer::Create(spec);
    SceneSubmission frame;frame.Assets=assets;frame.Debug=SurfaceView::Direct;
    frame.Presentation.AmbientOcclusion.Enabled=true;
    frame.AmbientOcclusionView=1;
    auto material=CreateRef<MaterialDefinition>();material->BaseColor={.5f,.5f,.5f,1};
    SubmittedMesh floor;floor.Geometry=mesh.Accepted;floor.Materials={material};floor.Picking=-2000000003;
    floor.SetWorld(glm::scale(glm::mat4(1),{10,10,.1f}));frame.Meshes={floor};
    SceneRenderView view;
    bool planeFailure=false;
    auto render=[&] {
        display->Bind();display->Clear({0,0,0,1});display->ClearAttachment(1,-1);
        SceneRenderer::Render(frame,view,display,[]{},[]{});
        auto result=capture(display);
        if(!result || !result->Completed || !result->Error.empty())throw std::runtime_error("XeGTAO capture incomplete");
        return result;
    };
    auto save=[&](const std::string& name,const auto& result) {
        std::ofstream file(output/(name+".bin"),std::ios::binary);
        for(size_t i=0;i<result->Color.size();++i) {
            file.write(reinterpret_cast<const char*>(&result->Color[i]),16);
            file.write(reinterpret_cast<const char*>(&result->Entity[i]),4);
            file.write(reinterpret_cast<const char*>(&result->Depth[i]),4);
        }
    };
    for(bool ortho:{false,true}) {
        frame.Camera.Position={0,0,4};frame.Camera.ToCamera={0,0,1};frame.Camera.Orthographic=ortho;
        auto projection=ortho?glm::ortho(-2.f,2.f,-1.125f,1.125f,.1f,20.f):glm::perspective(glm::radians(45.f),320.f/180.f,.1f,20.f);
        frame.Camera.ViewProjection=projection*glm::lookAt(frame.Camera.Position,glm::vec3(0),glm::vec3(0,1,0));
        frame.Meshes.resize(1);
        auto plane=render();save(ortho?"plane-ortho":"plane-perspective",plane);
        for(unsigned debug=2;debug<=4;++debug) {frame.AmbientOcclusionView=debug;save("plane-debug-"+std::to_string(debug),render());}
        frame.AmbientOcclusionView=1;
        for(unsigned y=20;y<160;++y)for(unsigned x=20;x<300;++x) {
            size_t p=size_t(y)*320+x;
            if(!std::isfinite(plane->Color[p].r) || plane->Color[p].r<.97f || plane->Color[p].r>1.001f)
                planeFailure=true;
        }
        SubmittedMesh box=floor;box.Picking=123456789;box.SetWorld(glm::scale(glm::translate(glm::mat4(1),{0,0,.2f}),{.5f,.5f,.3f}));frame.Meshes.push_back(box);
        for(unsigned quality=0;quality<4;++quality) {
            frame.Presentation.AmbientOcclusion.Quality=quality;
            auto contact=render(),repeat=render();float minimum=1;
            for(size_t p=0;p<contact->Color.size();++p) {
                float ao=contact->Color[p].r;
                if(!std::isfinite(ao)||ao<0||ao>1.001f)throw std::runtime_error("XeGTAO visibility is nonfinite or unbounded");
                if(contact->Entity[p]==floor.Picking)minimum=std::min(minimum,ao);
                if(contact->Color[p]!=repeat->Color[p])throw std::runtime_error("XeGTAO static frame is not deterministic");
            }
            if(minimum>.9f)throw std::runtime_error("XeGTAO contact fixture has no occlusion");
            save(std::string(ortho?"ortho":"perspective")+"-q"+std::to_string(quality),contact);
        }
        frame.AmbientOcclusionView=0;frame.Light=SubmittedDirectional{{0,0,1},glm::vec3(2)};
        auto on=render();frame.Presentation.AmbientOcclusion.Enabled=false;auto off=render();
        for(size_t p=0;p<on->Color.size();++p)
            if(on->Color[p]!=off->Color[p] || on->Entity[p]!=off->Entity[p] || on->Depth[p]!=off->Depth[p])
                throw std::runtime_error("XeGTAO changed direct light, exact signed IDs or depth");
        frame.AmbientOcclusionView=1;frame.Presentation.AmbientOcclusion.Enabled=true;
    }
    view.Reset();display.reset();Renderer3D::Shutdown();
    if(planeFailure)throw std::runtime_error("XeGTAO isolated plane .97 visibility gate failed (other cases completed)");
}
