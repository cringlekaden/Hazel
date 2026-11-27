#pragma once
#include "Renderer3DParity.h"
#include "Hazel/Renderer/Drawing/Renderer2D.h"
#include <chrono>
inline void TemporalAAQualification(const std::filesystem::path& output,
    const std::function<Hazel::Ref<const Hazel::FramebufferReadback>(const Hazel::Ref<Hazel::Framebuffer>&)>& capture)
{
    using namespace Hazel;
    const auto selected=FileSystem::GetEnvironmentPath("HAZEL_RENDERING_REFERENCE_PROJECT");
    const auto projectRoot=selected.empty()?std::filesystem::absolute("examples/RenderingReference"):selected.parent_path();
    auto project=Project::LoadCandidate(projectRoot/"RenderingReference-Direct-PerspectivePBR.hproj");
    auto assets=CreateRef<ProjectAssets>(project->GetAssetRoot());
    auto mesh=assets->Mesh("Native/Direct_v1/UnitCube.hmesh");if(!mesh.Accepted)throw std::runtime_error(mesh.Error);
    FramebufferSpecification spec;spec.Width=320;spec.Height=180;
    spec.Attachments={FramebufferTextureFormat::RGBA32F,FramebufferTextureFormat::RED_INTEGER,FramebufferTextureFormat::DEPTH32F};
    auto target=Framebuffer::Create(spec);
    SceneSubmission frame;frame.Assets=assets;frame.SceneIdentity=99;frame.Presentation.SceneReferred=true;
    frame.Presentation.Background={.02f,.02f,.02f,1};
    auto floorMaterial=CreateRef<MaterialDefinition>();floorMaterial->BaseColor={.45f,.45f,.45f,1};floorMaterial->Model=MaterialModel::Unlit;
    auto boxMaterial=CreateRef<MaterialDefinition>();boxMaterial->BaseColor={3,.1f,.01f,1};boxMaterial->Model=MaterialModel::Unlit;
    SubmittedMesh floor;floor.Geometry=mesh.Accepted;floor.Materials={floorMaterial};floor.Picking=-2000000003;floor.Identity=1;
    floor.SetWorld(glm::scale(glm::mat4(1),{10,10,.1f}));
    SubmittedMesh box=floor;box.Materials={boxMaterial};box.Picking=123456789;box.Identity=2;
    frame.Meshes={floor,box};
    SceneRenderView view,referenceView,independentView;
    std::function<void()> world2D=[]{ };
    auto render=[&](SceneRenderView& renderView,bool enabled) {
        frame.Presentation.TemporalAA=enabled;
        target->Bind();target->Clear(frame.Presentation.Background);
        SceneRenderer::Render(frame,renderView,target,world2D,[]{});
        auto image=capture(target);
        if(!image||!image->Completed||!image->Error.empty())throw std::runtime_error("Temporal capture incomplete");
        for(auto c:image->Color)for(int i=0;i<4;++i)if(!std::isfinite(c[i]))throw std::runtime_error("Nonfinite temporal output");
        return image;
    };
    auto save=[&](const std::string& name,const auto& result) {
        std::ofstream file(output/(name+".bin"),std::ios::binary);
        for(size_t i=0;i<result->Color.size();++i) {
            file.write(reinterpret_cast<const char*>(&result->Color[i]),16);
            file.write(reinterpret_cast<const char*>(&result->Entity[i]),4);
            file.write(reinterpret_cast<const char*>(&result->Depth[i]),4);
        }
    };
    std::ofstream velocityMetrics(output/"velocity.csv");velocityMetrics<<"projection,pixels,max_error_pixels,max_depth_error,max_velocity_pixels\n";
    for(bool orthographic:{false,true}) {
        SceneSubmission probe=frame;probe.Meshes={box};
        auto& draw=probe.Meshes[0];draw.SetWorld(glm::rotate(glm::translate(glm::mat4(1),{.2f,0,.2f}),.2f,glm::vec3(0,1,0)));
        draw.PreviousWorld=glm::rotate(glm::translate(glm::mat4(1),{-.1f,0,.3f}),-.1f,glm::vec3(0,1,0));draw.PreviousValid=true;
        probe.Camera.Orthographic=orthographic;probe.Camera.Position={.3f,.1f,4};
        auto projection=orthographic?glm::ortho(-2.f,2.f,-1.125f,1.125f,.1f,20.f):glm::perspective(glm::radians(45.f),320.f/180.f,.1f,20.f);
        probe.Camera.ViewProjection=projection*glm::lookAt(probe.Camera.Position,glm::vec3(0),glm::vec3(0,1,0));
        probe.PreviousViewProjection=projection*glm::lookAt(glm::vec3(0,0,4),glm::vec3(0),glm::vec3(0,1,0));
        target->Bind();target->Clear(glm::vec4(0));
        {RenderCommand::StateScope scope;auto state=RenderCommand::GetRasterState();state.ColorMask=state.PickingMask=glm::bvec4(true);RenderCommand::SetRasterState(state);
        Renderer3D::Render(probe,false,false,true);}
        auto image=capture(target);auto inverseVP=glm::inverse(probe.Camera.ViewProjection),inverseWorld=glm::inverse(draw.World);
        float maxError=0,maxDepth=0,maxVelocity=0;size_t count=0;
        for(unsigned y=0;y<180;++y)for(unsigned x=0;x<320;++x) {
            size_t p=size_t(y)*320+x;if(image->Entity[p]!=draw.Picking)continue;
            glm::vec2 uv=(glm::vec2(x,y)+.5f)/glm::vec2(320,180);
            auto world=inverseVP*glm::vec4(uv*2.f-1.f,image->Depth[p]*2-1,1);world/=world.w;
            auto previous=probe.PreviousViewProjection*draw.PreviousWorld*inverseWorld*world;
            auto expected=glm::vec3(previous)/previous.w*.5f+.5f;
            auto error=glm::abs(glm::vec2(image->Color[p])-glm::vec2(expected))*glm::vec2(320,180);
            maxError=std::max(maxError,std::max(error.x,error.y));maxDepth=std::max(maxDepth,std::abs(image->Color[p].z-expected.z));
            maxVelocity=std::max(maxVelocity,glm::length((uv-glm::vec2(expected))*glm::vec2(320,180)));++count;
            if(image->Color[p].w!=1)throw std::runtime_error("Rigid motion validity was lost");
        }
        if(count<100 || maxError>.01f || maxDepth>1e-5f || maxVelocity<1)throw std::runtime_error("Camera/object raster velocity differs from independent depth reprojection");
        velocityMetrics<<(orthographic?"ortho":"perspective")<<','<<count<<','<<maxError<<','<<maxDepth<<','<<maxVelocity<<'\n';
        save(orthographic?"velocity-ortho":"velocity-perspective",image);
    }
    std::ofstream metrics(output/"temporal.csv");metrics<<"projection,frame,resolve_ms,allocations,reuses,history_bytes,interior_error\n";
    for(bool ortho:{false,true}) {
        view.Reset();referenceView.Reset();
        frame.Camera.Orthographic=ortho;
        for(unsigned index=0;index<80;++index) {
            // Warmup/static, independent rigid motion, then camera motion, then
            // a cut and exposed floor. Actual scene matrices change every frame.
            float x=index<16?0:index<48?float(index-16)*.018f:index<64?.576f:0;
            float eyeX=index<48?0:index<64?float(index-48)*.012f:index<72?1.2f:0;
            frame.Camera.Position={eyeX,0,4};frame.Camera.ToCamera={0,0,1};
            auto projection=ortho?glm::ortho(-2.f,2.f,-1.125f,1.125f,.1f,20.f):glm::perspective(glm::radians(45.f),320.f/180.f,.1f,20.f);
            frame.Camera.ViewProjection=projection*glm::lookAt(frame.Camera.Position,glm::vec3(eyeX,0,0),glm::vec3(0,1,0));
            frame.Meshes[1].SetWorld(glm::scale(glm::rotate(glm::translate(glm::mat4(1),{x,0,.3f}),.12f,glm::vec3(0,0,1)),{.2f,.8f,.5f}));
            auto off=render(referenceView,false),on=render(view,true);
            double error=0;size_t interiors=0;
            for(unsigned y=2;y<178;++y)for(unsigned xPixel=2;xPixel<318;++xPixel) {
                size_t p=size_t(y)*320+xPixel;
                if(on->Entity[p]!=off->Entity[p]||on->Depth[p]!=off->Depth[p])throw std::runtime_error("TAA changed authoritative signed ID/depth");
                bool interior=true;for(int yy=-2;yy<=2;++yy)for(int xx=-2;xx<=2;++xx)
                    interior&=off->Entity[size_t(int(y)+yy)*320+unsigned(int(xPixel)+xx)]==off->Entity[p];
                if(interior) {error+=glm::length(glm::vec3(on->Color[p]-off->Color[p]));++interiors;}
            }
            error/=double(std::max<size_t>(interiors,1));
            if(error>.015)throw std::runtime_error("Temporal rigid-motion interior has persistent color trail");
            const auto& timing=*view.TemporalMetrics;
            if(!timing.Valid)throw std::runtime_error("Temporal GPU resolve timing missing");
            metrics<<(ortho?"ortho":"perspective")<<','<<index<<','<<timing.ResolveMilliseconds<<','<<timing.Allocations<<','<<timing.Reuses<<','<<timing.HistoryBytes<<','<<error<<'\n';
            if(index%4==0||index>=64)save(std::string(ortho?"ortho":"perspective")+"-"+std::to_string(index),on);
            if(index==15) {
                view.TemporalDebug=4;auto contribution=render(view,true);view.TemporalDebug=0;
                size_t accepted=0;for(auto color:contribution->Color)if(color.r>.25f)++accepted;
                if(accepted<1000)throw std::runtime_error("Temporal history was never accepted");
                save(std::string(ortho?"ortho":"perspective")+"-contribution",contribution);
            }
            if(index==32) {
                const auto phase=view.Temporal.Phase;auto isolated=render(independentView,true);
                if(independentView.Temporal.Phase!=1 || view.Temporal.Phase!=phase)throw std::runtime_error("Temporal view histories are shared");
                (void)isolated;
            }
        }
        auto off=render(referenceView,false),repeat=render(referenceView,false);
        if(off->Color!=repeat->Color||off->Entity!=repeat->Entity||off->Depth!=repeat->Depth)throw std::runtime_error("TAA disabled is nondeterministic");
        // A CPU-only cancelled frame must not become the next frame's history.
        frame.Presentation.TemporalAA=true;target->Bind();target->Clear(glm::vec4(0));
        SceneRenderer::Render(frame,view,target,[]{},[]{});target->Cancel();
        render(view,true);
        if(view.Temporal.Phase!=1)throw std::runtime_error("Cancelled temporal frame advanced history");
    }
    {
        // FP32 HDR is not limited by the compressed clipping coordinates.
        frame.Camera.Orthographic=false;frame.Camera.Position={0,0,4};
        frame.Camera.ViewProjection=glm::perspective(glm::radians(45.f),320.f/180.f,.1f,20.f)*glm::lookAt(frame.Camera.Position,glm::vec3(0),glm::vec3(0,1,0));
        frame.Presentation.Exposure.CompensationStops=-16;boxMaterial->BaseColor={1000000,200000,30000,1};
        frame.Meshes[1].SetWorld(glm::scale(glm::mat4(1),glm::vec3(1)));
        view.Reset();referenceView.Reset();
        auto off=render(referenceView,false);
        for(unsigned index=0;index<4;++index) {
            auto on=render(view,true);size_t p=90*320+160;
            if(glm::length(on->Color[p]-off->Color[p])>1e-3f)throw std::runtime_error("TAA clipping coordinates capped high HDR radiance");
        }
        boxMaterial->BaseColor={120000,24000,3600,1};
        for(unsigned index=0;index<3;++index)render(view,true);
        boxMaterial->BaseColor={100000,20000,3000,1};
        auto changedOff=render(referenceView,false),changedOn=render(view,true);
        if(glm::length(changedOn->Color[90*320+160]-changedOff->Color[90*320+160])>1e-3f)
            throw std::runtime_error("TAA clipping epsilon retained an obsolete high-HDR highlight");
        boxMaterial->BaseColor={3,.1f,.01f,1};frame.Presentation.Exposure.CompensationStops=0;
    }
    {
        auto demo=Project::LoadCandidate(projectRoot/"RenderingReference-XeGTAO.hproj");
        auto requests=demo->GetRendererRequests();requests.TextureSlots=RenderCommand::GetSettings().TextureSlots;demo->GetConfig().Rendering=requests;
        RuntimeSession session;session.Start(demo,demo->LoadScene(demo->GetConfig().StartScene));
        session.Resize(320,180);session.ResizeFramebuffer(320,180);
        auto& scene=*session.GetScene();auto camera=scene.GetEntityByUUID(1),occluder=scene.GetEntityByUUID(13);
        const auto originalCamera=camera.GetComponent<TransformComponent>(),originalObject=occluder.GetComponent<TransformComponent>();
        const int label=int(scene.GetEntityByUUID(71)),backing=int(scene.GetEntityByUUID(72));
        SceneRenderView temporalView,plainView;
        std::ofstream study(output/"ao-motion.csv");study<<"quality,frame,resolve_ms,history_bytes\n";
        auto draw=[&](SceneRenderView& renderView,bool enabled) {
            scene.Presentation().TemporalAA=enabled;
            target->Bind();target->Clear(glm::vec4(0));scene.OnUpdateRuntime(Timestep(0),&renderView,target);
            if(!scene.RenderError().empty())throw std::runtime_error(scene.RenderError());
            return capture(target);
        };
        for(int quality=-1;quality<4;++quality) {
            temporalView.Reset();plainView.Reset();
            scene.Presentation().AmbientOcclusion.Enabled=quality>=0;
            if(quality>=0)scene.Presentation().AmbientOcclusion.Quality=unsigned(quality);
            for(unsigned index=0;index<64;++index) {
                auto currentCamera=originalCamera,currentObject=originalObject;
                currentCamera.Translation.x=index<16?0:float(index-16)*.007f;
                if(index>=32)currentObject.Translation.x+=.35f*std::sin(float(index-32)*.12f);
                scene.SetLocalTransform(camera,currentCamera);scene.SetLocalTransform(occluder,currentObject);
                auto plain=draw(plainView,false),resolved=draw(temporalView,true);
                if(plain->Entity!=resolved->Entity || plain->Depth!=resolved->Depth)throw std::runtime_error("Authored temporal AO changed signed IDs/depth");
                unsigned uiPixels=0;
                for(size_t p=0;p<plain->Color.size();++p) {
                    for(int component=0;component<4;++component)if(!std::isfinite(resolved->Color[p][component]))throw std::runtime_error("Nonfinite temporal AO study");
                    if(plain->Entity[p]==label || plain->Entity[p]==backing) {
                        ++uiPixels;if(plain->Color[p]!=resolved->Color[p])throw std::runtime_error("Temporal AA changed independent screen UI");
                    }
                }
                if(uiPixels<20)throw std::runtime_error("Temporal UI fixture has no coverage");
                study<<quality<<','<<index<<','<<temporalView.TemporalMetrics->ResolveMilliseconds<<','<<temporalView.TemporalMetrics->HistoryBytes<<'\n';
                if(index>=16 && index%4==0) {
                    auto name="study-q"+std::to_string(quality)+"-"+std::to_string(index);
                    save(name+"-off",plain);save(name+"-on",resolved);
                }
            }
        }
        session.Stop();temporalView.Reset();plainView.Reset();
    }
    {
        // Private generated qualification texture; never modify authored or frozen assets.
        auto textureRoot=output/"texture-assets";std::filesystem::create_directories(textureRoot);
        std::ofstream texture(textureRoot/"Fine.tga",std::ios::binary);
        unsigned char header[18]{};header[2]=2;header[12]=128;header[14]=64;header[16]=32;header[17]=8;
        texture.write(reinterpret_cast<const char*>(header),18);
        for(unsigned y=0;y<64;++y)for(unsigned x=0;x<128;++x) {
            unsigned char value=((x/2+y/2)%2)?144:112;
            unsigned char pixel[4]={value,value,value,static_cast<unsigned char>((x/8)%2?255:0)};
            texture.write(reinterpret_cast<const char*>(pixel),4);
        }
        texture.close();
        auto originalAssets=frame.Assets;frame.Assets=CreateRef<ProjectAssets>(textureRoot);
        auto detail=CreateRef<MaterialDefinition>();detail->Model=MaterialModel::Unlit;detail->Textures[0].Image="Fine.tga";
        detail->Textures[0].Min=detail->Textures[0].Mag=TextureFilter::Nearest;
        frame.Meshes.resize(1);frame.Meshes[0].Materials={detail};frame.Meshes[0].Identity=1;
        frame.Presentation.AmbientOcclusion.Enabled=false;frame.Camera.Orthographic=false;
        SceneRenderView detailView,plainView;
        for(unsigned kind=0;kind<3;++kind) {
            detailView.Reset();plainView.Reset();
            detail->Alpha=kind==1?MaterialAlpha::Mask:MaterialAlpha::Opaque;
            detail->Model=kind==2?MaterialModel::MetallicRoughness:MaterialModel::Unlit;
            detail->Metallic=kind==2?1.f:0.f;detail->Roughness=.06f;
            detail->Textures[0].Image=kind==2?"":"Fine.tga";
            frame.Environment=kind==2?CreateRef<PreparedEnvironment>(ReadPreparedEnvironment(project->GetAssetRoot(),"Reference/IBL_v1/Photo.henv")):nullptr;
            for(unsigned index=0;index<32;++index) {
                frame.Camera.Position={float(index)*.002f,0,4};frame.Camera.ToCamera={0,0,1};
                frame.Camera.ViewProjection=glm::perspective(glm::radians(45.f),320.f/180.f,.1f,20.f)*
                    glm::lookAt(frame.Camera.Position,frame.Camera.Position+glm::vec3(0,0,-1),glm::vec3(0,1,0));
                frame.Meshes[0].SetWorld(glm::scale(glm::rotate(glm::translate(glm::mat4(1),{.003f*std::sin(float(index)*.3f),0,0}),.1f,glm::vec3(0,0,1)),{3,1.5f,.1f}));
                auto off=render(plainView,false),on=render(detailView,true);
                if(off->Entity!=on->Entity || off->Depth!=on->Depth)throw std::runtime_error("Textured/masked/specular TAA changed authoritative IDs/depth");
                if(index%4==0) {auto name="detail-"+std::to_string(kind)+"-"+std::to_string(index);save(name+"-off",off);save(name+"-on",on);}
            }
            if(kind==0) {
                Ref<const FramebufferReadback> converged;
                for(unsigned settle=0;settle<48;++settle)converged=render(detailView,true);
                auto plain=render(plainView,false);
                auto largeSpec=spec;largeSpec.Width=1280;largeSpec.Height=720;
                auto large=Framebuffer::Create(largeSpec);SceneRenderView supersampleView;auto highFrame=frame;highFrame.Presentation.TemporalAA=false;
                large->Bind();large->Clear(frame.Presentation.Background);SceneRenderer::Render(highFrame,supersampleView,large,[]{},[]{});
                auto high=capture(large);auto reference=CreateRef<FramebufferReadback>();reference->Width=320;reference->Height=180;
                reference->Color.resize(320*180);reference->Entity=plain->Entity;reference->Depth=plain->Depth;
                for(unsigned y=0;y<180;++y)for(unsigned x=0;x<320;++x) {
                    glm::vec4 sum(0);for(unsigned yy=0;yy<4;++yy)for(unsigned xx=0;xx<4;++xx)sum+=high->Color[size_t(y*4+yy)*1280+x*4+xx];
                    reference->Color[size_t(y)*320+x]=sum/16.f;
                }
                double errorTAA=0,errorOff=0,sumTAA=0,sumRef=0,sumSqTAA=0,sumSqRef=0;size_t count=0;
                for(unsigned y=3;y<177;++y)for(unsigned x=3;x<317;++x) {
                    size_t p=size_t(y)*320+x;if(plain->Entity[p]!=frame.Meshes[0].Picking)continue;
                    bool interior=true;for(int yy=-3;yy<=3;++yy)for(int xx=-3;xx<=3;++xx)interior&=plain->Entity[size_t(int(y)+yy)*320+unsigned(int(x)+xx)]==plain->Entity[p];
                    if(!interior)continue;
                    double a=converged->Color[p].r,b=plain->Color[p].r,r=reference->Color[p].r;
                    errorTAA+=(a-r)*(a-r);errorOff+=(b-r)*(b-r);sumTAA+=a;sumRef+=r;sumSqTAA+=a*a;sumSqRef+=r*r;++count;
                }
                double rmsTAA=std::sqrt(errorTAA/double(count)),rmsOff=std::sqrt(errorOff/double(count));
                double contrastTAA=std::sqrt(std::max(0.,sumSqTAA/double(count)-std::pow(sumTAA/double(count),2)));
                double contrastRef=std::sqrt(std::max(0.,sumSqRef/double(count)-std::pow(sumRef/double(count),2)));
                std::ofstream quality(output/"detail-reference.csv");quality<<"pixels,taa_rmse,off_rmse,taa_contrast,reference_contrast\n"<<count<<','<<rmsTAA<<','<<rmsOff<<','<<contrastTAA<<','<<contrastRef<<'\n';
                save("detail-static-taa",converged);save("detail-static-off",plain);save("detail-static-4x-reference",reference);
                if(count<100 || rmsTAA>=rmsOff || contrastTAA<.85*contrastRef || contrastTAA>1.2*contrastRef)
                    throw std::runtime_error("TAA detail reconstruction failed independent 4x supersampled error/contrast bounds");
                supersampleView.Reset();large.reset();
            }
        }
        frame.Assets=originalAssets;frame.Environment.reset();detailView.Reset();plainView.Reset();
    }
    {
        auto meshes=frame.Meshes;const auto background=frame.Presentation.Background;
        frame.Meshes.clear();frame.Presentation.Background={.05f,.12f,.2f,.35f};
        world2D=[]{
            Renderer2D::BeginScene(glm::mat4(1));
            Renderer2D::DrawQuad(glm::scale(glm::mat4(1),{.8f,.8f,1}),{.2f,.5f,.1f,.4f},-1234);
            Renderer2D::EndScene();
        };
        auto off=render(referenceView,false);
        for(unsigned index=0;index<4;++index) {
            auto on=render(view,true);
            if(on->Entity!=off->Entity || on->Depth!=off->Depth)throw std::runtime_error("Temporal world-2D identity/depth changed");
            for(size_t p=0;p<on->Color.size();++p)
                if(glm::length(on->Color[p]-off->Color[p])>1e-5f)throw std::runtime_error("Temporal resolve changed fractional background/world-2D alpha compositing");
        }
        if(std::abs(off->Color[0].a-.35f)>1e-5f || std::abs(off->Color[90*320+160].a-.61f)>1e-5f)
            throw std::runtime_error("Temporal world-2D alpha fixture lacks fractional coverage");
        world2D=[]{ };frame.Meshes=std::move(meshes);frame.Presentation.Background=background;
    }
    for(auto extent:{glm::uvec2(1,1),glm::uvec2(321,181),glm::uvec2(320,180)}) {
        target->Resize(extent.x,extent.y);render(view,true);
        if(view.Temporal.Phase!=1)throw std::runtime_error("Resize did not reset temporal history");
    }
    view.Reset();referenceView.Reset();independentView.Reset();
}
