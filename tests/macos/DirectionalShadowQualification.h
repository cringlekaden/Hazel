#pragma once
#include "Renderer3DParity.h"
#include "ShadowContinuityChecks.h"
#include "Hazel/Scene/SceneDefaults.h"
#include "Hazel/Renderer/Cameras/NavigationCamera.h"
#include "Hazel/Renderer/Drawing/Renderer2D.h"
#include <iomanip>
#include <iostream>
#include <chrono>

inline void ShadowCheck(bool value,const char* message) {if(!value)throw std::runtime_error(message);}
// Independent perspective rays, analytic split equation and ray/box occlusion.
// Does not reuse production inverse-VP fitting or the depth sampling algorithm.
inline void DirectionalShadowCPUChecks()
{
    using namespace Hazel;
    SceneSubmission frame;frame.Presentation.Shadows.Enabled=true;
    frame.Presentation.Shadows.Distance=80;
    frame.Light=SubmittedDirectional{glm::normalize(glm::vec3(.3f,1,.2f)),glm::vec3(1)};
    std::array<std::array<float,4>,4> scales{};
    unsigned checked=0;
    for(float yaw:{0.f,.3f,1.5f,3.f})for(float move:{0.f,.001f,1.f,100.f})for(unsigned res:{256u,512u,1024u,2048u}) {
        auto& camera=frame.Camera;camera.Position={move,2,4};
        const glm::vec3 forward(std::sin(yaw),0,-std::cos(yaw));camera.ToCamera=-forward;
        const auto right=glm::cross(glm::dvec3(forward),glm::dvec3(0,1,0));
        camera.ViewProjection=glm::perspective(glm::radians(60.f),1.5f,.1f,200.f)*
            glm::lookAt(camera.Position,camera.Position+forward,glm::vec3(0,1,0));
        frame.Presentation.Shadows.Resolution=res;
        const auto fit=FitDirectionalShadows(frame);
        double previous=.1;
        for(unsigned c=0;c<4;++c) {
            const auto scale=glm::length(glm::vec3(fit.VP[c][0][0],fit.VP[c][1][0],fit.VP[c][2][0]));
            auto& referenceScale=scales[res==256?0:res==512?1:res==1024?2:3][c];
            if(referenceScale==0)referenceScale=scale;
            ShadowCheck(std::abs(scale/referenceScale-1)<1e-5f,"CSM scale changed under camera rotation");
            const double t=double(c+1)/4,expected=(.1+(80-.1)*t)*.35+.1*std::pow(800.,t)*.65;
            ShadowCheck(std::abs(fit.Splits[c]-expected)<.002,"CSM split differs from FP64 anchor");
            for(double depth:{previous,double(fit.Splits[c])})for(int x:{-1,1})for(int y:{-1,1}) {
                const auto point=glm::dvec3(camera.Position)+glm::dvec3(forward)*depth+
                    right*(double(x)*depth*std::tan(glm::pi<double>()/6)*1.5)+glm::dvec3(0,1,0)*(double(y)*depth*std::tan(glm::pi<double>()/6));
                const auto clip=glm::dmat4(fit.VP[c])*glm::dvec4(point,1);
                ShadowCheck(glm::all(glm::lessThanEqual(glm::abs(glm::dvec3(clip)),glm::dvec3(1.0001))),"CSM lost independent frustum corner");++checked;
            }
            // A world-space point can move by whole texels, never fractional
            // texels, when only the camera translates and the projection is fixed.
            auto translated=frame;translated.Camera.Position.x+=.013f;
            translated.Camera.ViewProjection=glm::perspective(glm::radians(60.f),1.5f,.1f,200.f)*
                glm::lookAt(translated.Camera.Position,translated.Camera.Position+forward,glm::vec3(0,1,0));
            const auto moved=FitDirectionalShadows(translated);
            for(unsigned axis=0;axis<2;++axis) {
                const double shift=double(moved.VP[c][3][axis]-fit.VP[c][3][axis])*res*.5;
                ShadowCheck(std::abs(shift-std::round(shift))<.002,"CSM translated off texel grid");
                ShadowCheck(glm::length(glm::vec3(moved.VP[c][0][axis],moved.VP[c][1][axis],moved.VP[c][2][axis])-
                    glm::vec3(fit.VP[c][0][axis],fit.VP[c][1][axis],fit.VP[c][2][axis]))<1e-6,"CSM scale changed under translation");
            }
            previous=fit.Splits[c];
        }
    }
    auto invalid=frame;invalid.Camera.ViewProjection=glm::mat4(0);
    bool rejected=false;try{FitDirectionalShadows(invalid);}catch(const std::exception&){rejected=true;}
    ShadowCheck(rejected,"Invalid CSM camera accepted");
    for(auto light:{glm::vec3(0,1,0),glm::vec3(0,-1,0),glm::vec3(1,0,0),glm::vec3(0,0,-1)}) {
        frame.Light->ToLight=light;const auto fit=FitDirectionalShadows(frame);
        for(const auto& matrix:fit.VP)ShadowCheck(std::isfinite(glm::determinant(matrix)) && glm::determinant(matrix)!=0,"CSM light-pole basis is singular");
    }
    auto ortho=frame;ortho.Camera.Orthographic=true;ortho.Camera.Position={0,0,3};ortho.Camera.ToCamera={0,0,1};
    ortho.Camera.ViewProjection=glm::ortho(-1.f,1.f,-1.f,1.f,-1.f,1.f)*glm::lookAt(ortho.Camera.Position,glm::vec3(0),glm::vec3(0,1,0));
    const auto orthographic=FitDirectionalShadows(ortho);
    for(unsigned c=0;c<4;++c)ShadowCheck(std::abs(orthographic.Splits[c]-(-1.f+.5f*float(c+1)))<1e-6f,"CSM orthographic negative-near split failed");
    const auto roundtrip=SceneDefaults::ReadPresentation(SceneDefaults::Encode(frame.Presentation));
    ShadowCheck(roundtrip==frame.Presentation,"CSM authoring roundtrip failed");
    std::cout<<"PASS CSM independent FP64 split/corner/grid checks: "<<checked<<" corners\n";
}
inline bool ShadowBoxHit(glm::dvec3 p,glm::dvec3 direction,glm::dvec3 lo,glm::dvec3 hi)
{
    double enter=0,exit=1e30;
    for(unsigned axis=0;axis<3;++axis) {
        if(std::abs(direction[axis])<1e-12){if(p[axis]<lo[axis] || p[axis]>hi[axis])return false;continue;}
        double a=(lo[axis]-p[axis])/direction[axis],b=(hi[axis]-p[axis])/direction[axis];
        enter=std::max(enter,std::min(a,b));exit=std::min(exit,std::max(a,b));
    }
    return exit>=enter && exit>1e-6;
}
inline void DirectionalShadowQualification(const std::filesystem::path& output,
    const std::function<Hazel::Ref<const Hazel::FramebufferReadback>(const Hazel::Ref<Hazel::Framebuffer>&)>& capture)
{
    using namespace Hazel;
    if(!FileSystem::GetEnvironmentPath("HAZEL_SHADOW_CONTINUITY").empty()){ShadowContinuityChecks(output,capture);return;}
    DirectionalShadowCPUChecks();std::filesystem::create_directories(output);
    auto selected=FileSystem::GetEnvironmentPath("HAZEL_RENDERING_REFERENCE_PROJECT");
    auto project=Project::LoadCandidate(selected);auto assets=CreateRef<ProjectAssets>(project->GetAssetRoot());
    auto mesh=assets->Mesh("Native/Direct_v1/UnitCube.hmesh");ShadowCheck(bool(mesh.Accepted),"CSM cube asset missing");
    auto material=CreateRef<MaterialDefinition>();material->Roughness=.8f;material->BaseColor={.6f,.4f,.2f,1};
    SubmittedMesh ground;ground.Geometry=mesh.Accepted;ground.Materials={material};ground.Picking=11;
    ground.SetWorld(glm::translate(glm::mat4(1),{0,-.05f,0})*glm::scale(glm::mat4(1),{30,.1f,30}));
    SubmittedMesh blocker=ground;blocker.Picking=12;
    FramebufferSpecification spec;spec.Width=384;spec.Height=256;
    spec.Attachments={FramebufferTextureFormat::RGBA32F,FramebufferTextureFormat::RED_INTEGER,
        RendererAPI::GetAPI()==RendererAPI::API::Metal?FramebufferTextureFormat::DEPTH32F:FramebufferTextureFormat::Depth};
    auto target=Framebuffer::Create(spec);SceneRenderView view;
    SceneSubmission frame;frame.Assets=assets;frame.Presentation.SceneReferred=true;frame.LocalMetrics=view.LocalMetrics;
    frame.Presentation.Shadows.Distance=80;
    auto render=[&](SurfaceView debug,bool shadows) {
        frame.Debug=debug;frame.LinearLightingInspection=true;frame.Presentation.Shadows.Enabled=shadows;
        target->Bind();if(RendererAPI::GetAPI()==RendererAPI::API::Metal)target->Clear({0,0,0,1});
        else {RenderCommand::SetClearColor({0,0,0,1});RenderCommand::Clear();}
        target->ClearAttachment(1,-1);SceneRenderer::Render(frame,view,target,[]{},[]{});
        auto pixels=capture(target);ShadowCheck(pixels && pixels->Completed && pixels->Error.empty(),"CSM capture incomplete");return pixels;
    };
    if(!FileSystem::GetEnvironmentPath("HAZEL_SHADOW_BENCHMARK").empty()) {
        frame.Camera.Position={0,6,10};frame.Camera.ToCamera=glm::normalize(frame.Camera.Position);
        frame.Light=SubmittedDirectional{glm::normalize(glm::vec3(-.5f,1,.35f)),glm::vec3(2)};
        frame.Meshes={ground};
        for(unsigned i=0;i<32;++i) {
            auto cube=blocker;cube.Picking=100+int(i);
            cube.SetWorld(glm::translate(glm::mat4(1),{float(int(i%8)-4)*1.4f,.5f,float(int(i/8)-2)*1.4f}));
            frame.Meshes.push_back(cube);
        }
        frame.ShadowCasters=frame.Meshes;
        std::ofstream timing(output/"benchmark.csv");timing<<"width,height,resolution,enabled,sample,shadowGPUms,forwardGPUms,serializedFrameMs\n";
        for(auto extent:{glm::uvec2(1280,720),glm::uvec2(1920,1080)})for(unsigned resolution:{512u,1024u,2048u})for(bool enabled:{false,true}) {
            target->Resize(extent.x,extent.y);frame.Presentation.Shadows.Resolution=resolution;
            frame.Camera.ViewProjection=glm::perspective(glm::radians(55.f),float(extent.x)/float(extent.y),.1f,100.f)*
                glm::lookAt(frame.Camera.Position,glm::vec3(0),glm::vec3(0,1,0));
            for(unsigned sample=0;sample<25;++sample) {
                frame.LocalMetrics->Valid=false;
                const auto start=std::chrono::steady_clock::now();render(SurfaceView::Direct,enabled);
                const double whole=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
                if(RendererAPI::GetAPI()==RendererAPI::API::Metal)ShadowCheck(frame.LocalMetrics->Valid && frame.LocalMetrics->ForwardMilliseconds>0 &&
                    (!enabled || frame.LocalMetrics->ShadowMilliseconds>0),"Missing CSM GPU timestamps");
                if(sample>=5)timing<<extent.x<<','<<extent.y<<','<<resolution<<','<<enabled<<','<<sample-5<<','<<frame.LocalMetrics->ShadowMilliseconds<<','<<frame.LocalMetrics->ForwardMilliseconds<<','<<whole<<std::endl;
            }
        }
        std::cout<<"PASS CSM scoped serialized benchmark; timestamps require native Metal\n";return;
    }
    std::ofstream csv(output/"shadows.csv");csv<<"case,resolution,lit,shadowed,maximumInteriorError\n";
    unsigned caseNumber=0;
    for(unsigned res:{256u,512u,1024u,2048u})for(unsigned variant=0;variant<6;++variant) {
        frame.Presentation.Shadows.Resolution=res;
        const bool offscreen=variant==5;
        const auto light=glm::normalize(offscreen?glm::vec3(1,1,0):variant==4?glm::vec3(0,1,0):glm::vec3(-.5f,1,.35f));
        const glm::vec3 centre=light*(8.f/light.y)+glm::vec3(float(variant%2)*.7f,0,0);
        const glm::vec3 scale=offscreen?glm::vec3(4):glm::vec3(6);
        blocker.SetWorld(glm::translate(glm::mat4(1),centre)*glm::scale(glm::mat4(1),scale));
        frame.Light=SubmittedDirectional{light,glm::vec3(2)};
        frame.Meshes=offscreen?std::vector<SubmittedMesh>{ground}:std::vector<SubmittedMesh>{ground,blocker};
        frame.ShadowCasters={ground,blocker};
        frame.Camera.Orthographic=variant>=4;
        const float distance=variant==0?8.f:variant==1?18.f:variant==2?40.f:12.f;
        frame.Camera.Position=offscreen?glm::vec3(0,8,0):glm::vec3(distance*.8f,distance*.6f,distance);
        const glm::vec3 aim(0);frame.Camera.ToCamera=glm::normalize(frame.Camera.Position-aim);
        const auto cameraView=glm::lookAt(frame.Camera.Position,aim,offscreen?glm::vec3(0,0,-1):glm::vec3(0,1,0));
        frame.Camera.ViewProjection=(frame.Camera.Orthographic?glm::ortho(-6.f,6.f,-4.f,4.f,.1f,100.f):glm::perspective(glm::radians(55.f),1.5f,.1f,100.f))*cameraView;
        if(offscreen)ShadowCheck(!ShadowCasterVisible(mesh.Accepted->Geometry.Bounds.Min,mesh.Accepted->Geometry.Bounds.Max,frame.Camera.ViewProjection*blocker.World),"Off-frustum fixture caster is visible");
        auto positions=render(SurfaceView::WorldPosition,false),unshadowed=render(SurfaceView::Direct,false);
        auto shaded=render(SurfaceView::Direct,true),repeat=render(SurfaceView::Direct,true);
        unsigned lit=0,shadow=0;double maximum=0;
        // Exclude a conservative footprint around analytic edges. Interior
        // samples independently classify visibility; images retain edge behavior.
        const double margin=res==256?1.5:res==512?.8:res==1024?.4:.2;
        const glm::dvec3 lo=glm::dvec3(centre-scale*.5f),hi=glm::dvec3(centre+scale*.5f);
        for(size_t i=0;i<positions->Color.size();++i) {
            ShadowCheck(shaded->Entity[i]==unshadowed->Entity[i] && shaded->Depth[i]==unshadowed->Depth[i],"CSM changed authoritative IDs/depth");
            ShadowCheck(shaded->Color[i]==repeat->Color[i],"Static CSM capture is nondeterministic");
            if(positions->Entity[i]!=11 || std::abs(positions->Color[i].y)>.001f)continue;
            const glm::dvec3 p(positions->Color[i]);bool all=true,any=false;
            for(double x:{-margin,margin})for(double z:{-margin,margin}) {
                const bool hit=ShadowBoxHit(p+glm::dvec3(x,1e-5,z),glm::dvec3(light),lo,hi);all &= hit;any |= hit;
            }
            if(any!=all)continue;
            const double expected=all?0:1,ratio=shaded->Color[i].r/unshadowed->Color[i].r;
            maximum=std::max(maximum,std::abs(expected-ratio));all?++shadow:++lit;
        }
        csv<<caseNumber<<','<<res<<','<<lit<<','<<shadow<<','<<maximum<<std::endl;
        ShadowCheck(lit>100 && shadow>10 && maximum<.02,"CSM analytic interior visibility failed");
        std::ofstream file(output/("case-"+std::to_string(caseNumber++)+".bin"),std::ios::binary);
        for(size_t i=0;i<shaded->Color.size();++i){file.write(reinterpret_cast<const char*>(&shaded->Color[i]),16);file.write(reinterpret_cast<const char*>(&shaded->Entity[i]),4);file.write(reinterpret_cast<const char*>(&shaded->Depth[i]),4);}
    }
    // Cross each orthographic split and final fade while the same ground
    // samples retain their screen positions. Compare independent GPU captures.
    frame.Camera.Orthographic=true;frame.Light=SubmittedDirectional{glm::normalize(glm::vec3(1,1,0)),glm::vec3(2)};
    blocker.SetWorld(glm::translate(glm::mat4(1),{4,4,0}));frame.Meshes={ground};frame.ShadowCasters={ground,blocker};
    double boundaryMaximum=0;
    for(float boundary:{20.075f,40.05f,60.025f,80.f}) {
        std::array<Ref<const FramebufferReadback>,2> sides;
        for(unsigned side=0;side<2;++side) {
            frame.Camera.Position={0,boundary+(side?.0001f:-.0001f),0};frame.Camera.ToCamera={0,1,0};
            frame.Camera.ViewProjection=glm::ortho(-3.f,3.f,-2.f,2.f,.1f,100.f)*
                glm::lookAt(frame.Camera.Position,glm::vec3(0),glm::vec3(0,0,-1));
            sides[side]=render(SurfaceView::Direct,true);
        }
        for(size_t i=0;i<sides[0]->Color.size();++i)if(sides[0]->Entity[i]==11 && sides[1]->Entity[i]==11)
            boundaryMaximum=std::max(boundaryMaximum,double(glm::length(glm::vec3(sides[0]->Color[i]-sides[1]->Color[i]))));
    }
    ShadowCheck(boundaryMaximum<.01,"CSM boundary blend/fade is discontinuous");
    std::cout<<"PASS CSM split/fade crossing max RGB vector delta="<<boundaryMaximum<<'\n';
    auto collectionScene=CreateRef<Scene>();collectionScene->SetAssets(assets);
    collectionScene->Presentation()=frame.Presentation;
    collectionScene->CreateEntityWithUUID(1,"Directional").AddComponent<DirectionalLightComponent>();
    auto add=[&](uint64_t id,const glm::vec3& translation,const glm::vec3& scale) {
        auto entity=collectionScene->CreateEntityWithUUID(id,"Caster");
        entity.GetComponent<TransformComponent>().Translation=translation;entity.GetComponent<TransformComponent>().Scale=scale;
        entity.AddComponent<MeshRendererComponent>().Mesh="Native/Direct_v1/UnitCube.hmesh";
    };
    add(2,{0,-.05f,0},{30,.1f,30});add(3,{8,8,0},{2,2,2});
    frame.Camera.Position={0,8,0};frame.Camera.ViewProjection=glm::ortho(-3.f,3.f,-2.f,2.f,.1f,100.f)*
        glm::lookAt(frame.Camera.Position,glm::vec3(0),glm::vec3(0,0,-1));
    const auto collected=SceneRenderer::Collect(*collectionScene,frame.Camera,{384,256});
    ShadowCheck(collected.Meshes.size()==1 && collected.ShadowCasters.size()==2,"Collection discarded off-camera shadow caster");
    // Capture the actual authored fixture through collection, with its masked,
    // smooth and mirrored meshes and IBL. This is an observation, not a golden image.
    auto study=project->LoadScene("Scenes/DirectionalShadows_v1.hazel");study->OnViewportResize(1280,720);
    auto cameraEntity=study->GetPrimaryCameraEntity();ShadowCheck(bool(cameraEntity),"Shadow study camera missing");
    const auto& camera=cameraEntity.GetComponent<CameraComponent>().Camera;
    const auto world=study->GetWorldTransform(cameraEntity);
    const SceneCameraView cameraView{camera.GetProjection()*glm::inverse(world),glm::vec3(world[3]),
        glm::normalize(glm::vec3(world*glm::vec4(0,0,1,0))),false};
    frame=SceneRenderer::Collect(*study,cameraView,{1280,720});frame.LocalMetrics=view.LocalMetrics;
    target->Resize(1280,720);frame.Debug=SurfaceView::Shaded;frame.LinearLightingInspection=false;
    target->Bind();if(RendererAPI::GetAPI()==RendererAPI::API::Metal)target->Clear(frame.Presentation.Background);
    else {RenderCommand::SetClearColor(frame.Presentation.Background);RenderCommand::Clear();}
    target->ClearAttachment(1,-1);SceneRenderer::Render(frame,view,target,[]{},[]{});
    const auto studyImage=capture(target);ShadowCheck(studyImage && studyImage->Completed && studyImage->Error.empty(),"Authored shadow study failed");
    std::ofstream studyFile(output/"study.bin",std::ios::binary);
    for(size_t i=0;i<studyImage->Color.size();++i){studyFile.write(reinterpret_cast<const char*>(&studyImage->Color[i]),16);studyFile.write(reinterpret_cast<const char*>(&studyImage->Entity[i]),4);studyFile.write(reinterpret_cast<const char*>(&studyImage->Depth[i]),4);}
    if(RendererAPI::GetAPI()==RendererAPI::API::Metal) {
        // Exercise the host scene entry point and editor lens, including real
        // world-2D/UI callbacks. Surface the primary render error before overlays
        // can replace it with an unbound-framebuffer exception.
        NavigationCamera editorCamera(30,1280.f/720.f,.1f,1000);
        editorCamera.SetViewportSize(1280,720);
        editorCamera.RestoreOrbit({-20.0864868f,14.6694498f,-25.6105156f},.609317422f,-4.08032942f,10);
        study->Presentation().AmbientOcclusion.Enabled=false;
        study->Presentation().TemporalAA=false;
        for(unsigned setting=0;setting<3;++setting) {
            study->Presentation().AmbientOcclusion.Enabled=setting>=1;
            study->Presentation().TemporalAA=setting==2;
            target->Bind();target->Clear(study->Presentation().Background);target->ClearAttachment(1,-1);
            study->OnUpdateEditor(Timestep(0),editorCamera,&view,target);
            if(!study->RenderError().empty())throw std::runtime_error("Shadow editor settings stage "+std::to_string(setting)+": "+study->RenderError());
            Renderer2D::BeginScene(editorCamera);Renderer2D::EndScene();
            const auto image=capture(target);
            ShadowCheck(image && image->Completed && image->Error.empty(),"Shadow editor capture failed");
        }
        // A failed producer must leave a valid host packet for error UI/overlays.
        study->Presentation().Shadows.Distance=1;
        NavigationCamera rejectedCamera(30,1280.f/720.f,2,1000);
        target->Bind();target->Clear(study->Presentation().Background);
        study->OnUpdateEditor(Timestep(0),rejectedCamera,&view,target);
        ShadowCheck(study->RenderError().find("distance must exceed")!=std::string::npos,"Original shadow failure was lost");
        Renderer2D::BeginScene(rejectedCamera);Renderer2D::EndScene();
        const auto rejectedImage=capture(target);
        ShadowCheck(rejectedImage && rejectedImage->Completed && rejectedImage->Error.empty(),"Editor failure recovery did not submit");
        for(auto id:rejectedImage->Entity)ShadowCheck(id==-1,"Editor failure recovery retained stale picking IDs");
        study->Presentation().Shadows.Distance=100;
        std::cout<<"PASS editor shadow -> AO -> TAA entry/overlay and original-error recovery\n";
        frame.Presentation.TemporalAA=true;frame.Presentation.AmbientOcclusion.Enabled=true;
        const auto original=frame.Camera;
        for(unsigned step=0;step<16;++step) {
            if(step==4)frame.Presentation.Shadows.Resolution=2048;
            if(step==6)frame.Presentation.Shadows.Resolution=512;
            if(step==8)target->Resize(640,360);
            frame.Camera.Position=original.Position+glm::vec3(float(step)*.01f,0,0);
            frame.Camera.ViewProjection=original.ViewProjection*glm::translate(glm::mat4(1),{-float(step)*.01f,0,0});
            frame.Light->ToLight=glm::normalize(glm::vec3(.3f+float(step)*.002f,.8f,.5f));
            if(step==12 && !frame.ShadowCasters.empty()) {
                const auto identity=frame.ShadowCasters.back().Identity;
                auto moved=frame.ShadowCasters.back().World;moved[3].x+=.2f;
                frame.ShadowCasters.back().SetWorld(moved);
                for(auto& draw:frame.Meshes)if(draw.Identity==identity)draw.SetWorld(moved);
            }
            target->Bind();target->Clear(frame.Presentation.Background);target->ClearAttachment(1,-1);
            SceneRenderer::Render(frame,view,target,[]{},[]{});const auto image=capture(target);
            ShadowCheck(image && image->Completed && image->Error.empty(),"Moving shadow/AO/TAA view failed");
            for(const auto& pixel:image->Color)ShadowCheck(std::isfinite(glm::length(pixel)),"Nonfinite combined shadow/AO/TAA output");
        }
        view.Reset();
        std::cout<<"PASS exercised lit shadow/AO/TAA moving camera/light/caster, resize and resolution replacement (visual quality separate)\n";
    }
    std::cout<<"PASS CSM analytic visibility, deterministic off-frustum captures and unchanged IDs/depth: "<<caseNumber<<" cases\n";
}
