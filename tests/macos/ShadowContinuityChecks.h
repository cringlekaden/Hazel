#pragma once
#include "Renderer3DParity.h"
#include <iomanip>
#include <cmath>
#include <fstream>
#include <stdexcept>

inline void ShadowContinuityChecks(const std::filesystem::path& output,
    const std::function<Hazel::Ref<const Hazel::FramebufferReadback>(const Hazel::Ref<Hazel::Framebuffer>&)>& capture)
{
    using namespace Hazel;std::filesystem::create_directories(output);
    auto project=Project::LoadCandidate(FileSystem::GetEnvironmentPath("HAZEL_RENDERING_REFERENCE_PROJECT"));
    auto assets=CreateRef<ProjectAssets>(project->GetAssetRoot());auto mesh=assets->Mesh("Native/Direct_v1/UnitCube.hmesh");
    if(!mesh.Accepted)throw std::runtime_error(mesh.Error);
    auto material=CreateRef<MaterialDefinition>();material->BaseColor={.5f,.5f,.5f,1};material->Roughness=.8f;
    FramebufferSpecification spec;spec.Width=1024;spec.Height=512;
    spec.Attachments={FramebufferTextureFormat::RGBA32F,FramebufferTextureFormat::RED_INTEGER,
        RendererAPI::GetAPI()==RendererAPI::API::Metal?FramebufferTextureFormat::DEPTH32F:FramebufferTextureFormat::Depth};
    auto target=Framebuffer::Create(spec);SceneRenderView view;
    SceneSubmission frame;frame.Assets=assets;frame.Presentation.SceneReferred=true;
    frame.Presentation.Shadows.Enabled=true;frame.Presentation.Shadows.Distance=80;
    frame.Camera.Position={8,10,18};frame.Camera.ToCamera=glm::normalize(frame.Camera.Position-glm::vec3(0,0,-20));
    frame.Camera.ViewProjection=glm::perspective(glm::radians(60.f),2.f,.1f,130.f)*
        glm::lookAt(frame.Camera.Position,glm::vec3(0,0,-20),glm::vec3(0,1,0));
    SubmittedMesh ground;ground.Geometry=mesh.Accepted;ground.Materials={material};ground.Picking=11;ground.Identity=11;
    SubmittedMesh wall=ground;wall.Picking=12;wall.Identity=12;
    auto save=[&](const std::string& name,const auto& pixels) {
        std::ofstream file(output/(name+".bin"),std::ios::binary);
        for(size_t p=0;p<pixels->Color.size();++p){file.write(reinterpret_cast<const char*>(&pixels->Color[p]),16);file.write(reinterpret_cast<const char*>(&pixels->Entity[p]),4);file.write(reinterpret_cast<const char*>(&pixels->Depth[p]),4);}
    };
    auto render=[&](unsigned mode,SurfaceView surface=SurfaceView::Direct) {
        frame.ShadowView=mode;frame.Debug=surface;frame.LinearLightingInspection=true;
        target->Bind();target->ClearEditor({0,0,0,1});target->ClearAttachment(1,-1);
        SceneRenderer::Render(frame,view,target,[]{},[]{});auto image=capture(target);
        if(!image || !image->Completed || !image->Error.empty())throw std::runtime_error("CSM continuity capture failed");
        return image;
    };
    std::ofstream metadata(output/"cascades.csv");metadata<<std::setprecision(12)<<"case,resolution,slope,lightY,pose,distance,cascade,split,texel,depthSpan,normalBias,depthBias\n";
    std::ofstream projections(output/"projections.csv");projections<<std::setprecision(12)<<"case,cascade";
    for(unsigned i=0;i<16;++i)projections<<",m"<<i;
    projections<<'\n';
    unsigned number=0;
    for(unsigned resolution:{512u,1024u,2048u})for(float lightY:{1.f,.2f})for(float slope:{0.f,.3f})for(unsigned pose:{0u,1u}) {
        frame.Presentation.Shadows.Resolution=resolution;
        frame.Presentation.Shadows.Distance=pose?40.f:80.f;
        const auto rotation=glm::rotate(glm::mat4(1),slope,glm::vec3(0,0,1));
        frame.Camera.Position=glm::vec3(rotation*glm::vec4(pose?glm::vec3(8,2,8):glm::vec3(8,10,18),1));
        const glm::vec3 aim(rotation*glm::vec4(0,0,-20,1));frame.Camera.ToCamera=glm::normalize(frame.Camera.Position-aim);
        frame.Camera.ViewProjection=glm::perspective(glm::radians(60.f),2.f,.1f,130.f)*glm::lookAt(frame.Camera.Position,aim,glm::vec3(rotation*glm::vec4(0,1,0,0)));
        ground.SetWorld(rotation*glm::translate(glm::mat4(1),{0,-.05f,-35})*glm::scale(glm::mat4(1),{100,.1f,180}));
        wall.SetWorld(rotation*glm::translate(glm::mat4(1),{0,2,-35})*glm::scale(glm::mat4(1),{.2f,4,180}));
        frame.Meshes=frame.ShadowCasters={ground,wall};frame.Light=SubmittedDirectional{glm::normalize(glm::vec3(-.6f,lightY,.2f)),glm::vec3(2)};
        const auto fit=FitDirectionalShadows(frame);
        for(unsigned c=0;c<4;++c) {
            metadata<<number<<','<<resolution<<','<<slope<<','<<lightY<<','<<pose<<','<<frame.Presentation.Shadows.Distance<<','<<c<<','<<fit.Splits[c]<<','<<fit.TexelDepth[c].x<<','<<1/fit.TexelDepth[c].y<<','<<frame.Presentation.Shadows.NormalBias<<','<<frame.Presentation.Shadows.DepthBias<<std::endl;
            projections<<number<<','<<c;
            for(unsigned column=0;column<4;++column)for(unsigned row=0;row<4;++row)projections<<','<<fit.VP[c][column][row];
            projections<<'\n';
        }
        const auto prefix="boundary-"+std::to_string(number++);
        save(prefix+"-position",render(0,SurfaceView::WorldPosition));
        for(unsigned mode:{1u,2u,3u,4u,5u,6u,7u,8u,9u})save(prefix+"-"+std::to_string(mode),render(mode));
        save(prefix+"-unbiased",render(10));
    }
    std::cout<<"CSM continuity diagnostic captures="<<number<<" (metrics evaluated independently by runner)\n";
    auto study=project->LoadScene("Scenes/DirectionalShadows_v1.hazel");study->OnViewportResize(1280,720);
    study->Presentation().Exposure.CompensationStops=0;study->Presentation().Shadows.Resolution=2048;
    auto cameraEntity=study->GetPrimaryCameraEntity();
    if(!cameraEntity)throw std::runtime_error("CSM continuity study camera missing");
    const auto cameraWorld=study->GetWorldTransform(cameraEntity);
    const auto projection=cameraEntity.GetComponent<CameraComponent>().Camera.GetProjection();
    const SceneCameraView camera{projection*glm::inverse(cameraWorld),glm::vec3(cameraWorld[3]),
        glm::normalize(glm::vec3(cameraWorld*glm::vec4(0,0,1,0))),false};
    const auto base=SceneRenderer::Collect(*study,camera,{1280,720});target->Resize(1280,720);
    for(unsigned state=0;state<12;++state) {
        view.Reset();Ref<const FramebufferReadback> image;
        const unsigned motion=state>=8?state-4:state;
        for(unsigned step=0;step<16;++step) {
            frame=base;frame.Presentation.AmbientOcclusion.Enabled=state==1 || (state>=3 && state<8);
            frame.Presentation.TemporalAA=state>=2 && state<8;
            if(motion==4 || motion==5) {
                auto world=cameraWorld;
                if(motion==4)world[3].x+=float(step)*.02f;
                else world=world*glm::rotate(glm::mat4(1),float(step)*.001f,glm::vec3(0,1,0));
                frame.Camera={projection*glm::inverse(world),glm::vec3(world[3]),
                    glm::normalize(glm::vec3(world*glm::vec4(0,0,1,0))),false};
            }
            if(motion==6 && frame.Light)frame.Light->ToLight=glm::vec3(glm::rotate(glm::mat4(1),float(step)*.003f,glm::vec3(0,1,0))*glm::vec4(base.Light->ToLight,0));
            if(motion==7 && !frame.ShadowCasters.empty()) {
                auto& caster=frame.ShadowCasters.back();auto moved=caster.World;moved[3].x+=float(step)*.02f;
                caster.SetWorld(moved);
                for(auto& draw:frame.Meshes)if(draw.Identity==caster.Identity)draw.SetWorld(moved);
            }
            frame.Debug=SurfaceView::Shaded;target->Bind();target->ClearEditor(frame.Presentation.Background);target->ClearAttachment(1,-1);
            SceneRenderer::Render(frame,view,target,[]{},[]{});image=capture(target);
            if(!image || !image->Completed || !image->Error.empty())throw std::runtime_error("CSM combined study capture failed");
            for(const auto& color:image->Color)if(!std::isfinite(glm::length(color)))throw std::runtime_error("Nonfinite CSM combined study");
            if(step==0 || step==7)save("study-"+std::to_string(state)+"-step-"+std::to_string(step),image);
        }
        save("study-"+std::to_string(state),image);
    }
    view.Reset();
}
