#pragma once
#include "Renderer3DParity.h"
#include <chrono>
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
    {
        auto depthSpec=spec;depthSpec.Width=depthSpec.Height=8;depthSpec.Attachments={FramebufferTextureFormat::DEPTH32F};
        auto depthOnly=Framebuffer::Create(depthSpec);
        const auto reject=[&](const std::function<void()>& operation) {
            bool rejected=false;try{operation();}catch(const std::logic_error&){rejected=true;}
            if(!rejected)throw std::runtime_error("Depth-only target accepted a color/picking operation");
        };
        depthOnly->Bind();depthOnly->Clear(glm::vec4(0));
        reject([&]{depthOnly->ClearAttachment(1,-1);});
        depthOnly->Unbind();reject([&]{depthOnly->Submit();});
        reject([&]{depthOnly->GetEditorTexture();});reject([&]{depthOnly->EncodeOutput();});
        depthOnly->Cancel();
    }
    SceneSubmission frame;frame.Assets=assets;frame.Debug=SurfaceView::Direct;
    frame.Presentation.AmbientOcclusion.Enabled=true;
    frame.AmbientOcclusionView=1;
    auto material=CreateRef<MaterialDefinition>();material->BaseColor={.5f,.5f,.5f,1};
    SubmittedMesh floor;floor.Geometry=mesh.Accepted;floor.Materials={material};floor.Picking=-2000000003;
    floor.SetWorld(glm::scale(glm::mat4(1),{10,10,.1f}));frame.Meshes={floor};
    SceneRenderView view;
    bool planeFailure=false;
    std::ofstream metrics(output/"geometry-metrics.csv");
    metrics<<"projection,quality,min_plane,min_contact,max_contact,working_bytes\n";
    auto render=[&](int clearEntity=-1) {
        display->Bind();display->Clear({0,0,0,1});display->ClearAttachment(1,clearEntity);
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
            auto contact=render(),repeat=render();float minimum=1,maximum=0;
            if(!view.AmbientTiming->Valid)throw std::runtime_error("XeGTAO GPU timing unavailable");
            for(size_t p=0;p<contact->Color.size();++p) {
                float ao=contact->Color[p].r;
                if(!std::isfinite(ao)||ao<0||ao>1.001f)throw std::runtime_error("XeGTAO visibility is nonfinite or unbounded");
                if(contact->Entity[p]==floor.Picking){minimum=std::min(minimum,ao);maximum=std::max(maximum,ao);}
                if(contact->Color[p]!=repeat->Color[p])throw std::runtime_error("XeGTAO static frame is not deterministic");
            }
            if(minimum>.9f)throw std::runtime_error("XeGTAO contact fixture has no occlusion");
            metrics<<(ortho?"orthographic":"perspective")<<','<<quality<<',';
            float minPlane=1;for(unsigned y=20;y<160;++y)for(unsigned x=20;x<300;++x)minPlane=std::min(minPlane,plane->Color[size_t(y)*320+x].r);
            metrics<<minPlane<<','<<minimum<<','<<maximum<<','<<view.AmbientTiming->WorkingBytes<<'\n';
            save(std::string(ortho?"ortho":"perspective")+"-q"+std::to_string(quality),contact);
        }
        frame.AmbientOcclusionView=0;frame.Light=SubmittedDirectional{{0,0,1},glm::vec3(2)};
        auto on=render();frame.Presentation.AmbientOcclusion.Enabled=false;auto off=render();
        for(size_t p=0;p<on->Color.size();++p)
            if(on->Color[p]!=off->Color[p] || on->Entity[p]!=off->Entity[p] || on->Depth[p]!=off->Depth[p])
                throw std::runtime_error("XeGTAO changed direct light, exact signed IDs or depth");
        frame.AmbientOcclusionView=1;frame.Presentation.AmbientOcclusion.Enabled=true;
        material->Alpha=MaterialAlpha::Mask;material->BaseColor.a=.1f;
        auto masked=render();for(auto id:masked->Entity)if(id!=-1)throw std::runtime_error("XeGTAO masked geometry wrote IDs");
        material->BaseColor.a=1;
        material->Textures[0].Image="Reference/Direct_v1/Surfaces/AsymmetricMask.tga";
        frame.AmbientOcclusionView=0;frame.Presentation.AmbientOcclusion.Enabled=true;auto partialOn=render(-77777);
        frame.Presentation.AmbientOcclusion.Enabled=false;auto partialOff=render(-77777);
        if(std::find(partialOn->Entity.begin(),partialOn->Entity.end(),-77777)==partialOn->Entity.end())
            throw std::runtime_error("Partial-mask fixture has no caller-cleared background coverage");
        save("partial-mask-on",partialOn);save("partial-mask-off",partialOff);
        if(partialOn->Color!=partialOff->Color || partialOn->Entity!=partialOff->Entity || partialOn->Depth!=partialOff->Depth)
            throw std::runtime_error("XeGTAO changed partial alpha-mask color, depth or exact IDs");
        frame.Presentation.AmbientOcclusion.Enabled=true;frame.AmbientOcclusionView=1;auto partialInspection=render(-77777);
        if(partialInspection->Entity!=partialOff->Entity || partialInspection->Depth!=partialOff->Depth)
            throw std::runtime_error("XeGTAO inspection changed masked picking or depth clears");
        frame.AmbientOcclusionView=0;
        material->Alpha=MaterialAlpha::Opaque;material->Textures[0].Image.clear();
        material->DoubleSided=true;frame.Meshes[1].SetWorld(glm::scale(box.World,{-1.f,1.f,1.f}));
        frame.Presentation.AmbientOcclusion.Enabled=true;auto sidedOn=render();
        frame.Presentation.AmbientOcclusion.Enabled=false;auto sidedOff=render();
        if(sidedOn->Color!=sidedOff->Color || sidedOn->Entity!=sidedOff->Entity || sidedOn->Depth!=sidedOff->Depth)
            throw std::runtime_error("XeGTAO changed double-sided mirrored geometry");
        material->DoubleSided=false;frame.AmbientOcclusionView=1;frame.Presentation.AmbientOcclusion.Enabled=true;
        frame.Meshes.resize(1);auto clipBaseline=render();
        frame.Meshes.push_back(box);
        for(float z:{5.f,-30.f}) {
            frame.Meshes[1].SetWorld(glm::translate(glm::mat4(1),{0,0,z}));auto clipped=render();
            if(clipped->Color!=clipBaseline->Color || clipped->Entity!=clipBaseline->Entity || clipped->Depth!=clipBaseline->Depth)
                throw std::runtime_error("Clipped geometry affected XeGTAO visibility or final depth/IDs");
        }
        frame.Meshes[1]=box;
        frame.Presentation.AmbientOcclusion.Quality=2;
        const auto stable=render();
        float motionMin=1,motionMax=0;
        std::ofstream motionMetrics(output/(ortho?"motion-orthographic.csv":"motion-perspective.csv"));
        motionMetrics<<"frame,world_probe_visibility\n";
        auto original=frame.Camera;
        for(unsigned i=0;i<24;++i) {
            const float angle=float(i)*.025f;
            frame.Camera.Position={std::sin(angle)*.6f,std::cos(angle)*.2f,4.f+.2f*std::sin(angle)};
            frame.Camera.ToCamera=glm::normalize(frame.Camera.Position);
            frame.Camera.ViewProjection=projection*glm::lookAt(frame.Camera.Position,glm::vec3(0),glm::vec3(0,1,0));
            auto motion=render();
            for(const auto& color:motion->Color)if(!std::isfinite(color.r)||color.r<0||color.r>1.001f)throw std::runtime_error("XeGTAO moving geometry/camera visibility invalid");
            auto clip=frame.Camera.ViewProjection*glm::vec4(.37f,0,.05f,1);auto uv=glm::vec2(clip)/clip.w*.5f+.5f;
            int x=int(uv.x*320),y=int(uv.y*180);float probe=0;
            if(x<1||x>=319||y<1||y>=179)throw std::runtime_error("XeGTAO world stability probe escaped view");
            for(int yy=y-1;yy<=y+1;++yy)for(int xx=x-1;xx<=x+1;++xx) {
                auto p=size_t(yy)*320+size_t(xx);
                if(motion->Entity[p]!=floor.Picking)throw std::runtime_error("XeGTAO world stability probe is not on the floor");
                probe+=motion->Color[p].r/9;
            }
            motionMin=std::min(motionMin,probe);motionMax=std::max(motionMax,probe);
            motionMetrics<<i<<','<<probe<<'\n';
        }
        if(motionMax-motionMin>.15f)throw std::runtime_error("XeGTAO reprojected contact probe varies by more than .15 during camera motion");
        for(unsigned i=0;i<24;++i) {
            frame.Meshes[1].SetWorld(glm::scale(glm::translate(glm::mat4(1),{.2f*std::sin(float(i)*.1f),0,.2f}),{.5f,.5f,.3f}));
            auto moving=render();
            for(const auto& color:moving->Color)if(!std::isfinite(color.r)||color.r<0||color.r>1.001f)throw std::runtime_error("XeGTAO moving object visibility invalid");
        }
        frame.Camera=original;frame.Meshes[1]=box;
        auto restored=render();if(restored->Color!=stable->Color)throw std::runtime_error("XeGTAO camera restoration differs");
        frame.AmbientOcclusionView=0;
        frame.Environment=CreateRef<PreparedEnvironment>(ReadPreparedEnvironment(project->GetAssetRoot(),"Reference/IBL_v1/Constant.henv"));
        frame.Light.reset();
        for(auto inspection:{SurfaceView::SpecularIBL,SurfaceView::DiffuseIBL}) {
            frame.Debug=inspection;frame.Presentation.AmbientOcclusion.Enabled=true;auto lit=render();
            frame.Presentation.AmbientOcclusion.Enabled=false;auto plain=render();
            unsigned darkened=0;
            for(size_t p=0;p<lit->Color.size();++p) {
                if(lit->Entity[p]!=plain->Entity[p]||lit->Depth[p]!=plain->Depth[p])throw std::runtime_error("XeGTAO IBL changed depth/IDs");
                if(inspection==SurfaceView::SpecularIBL && lit->Color[p]!=plain->Color[p])throw std::runtime_error("Diffuse AO changed specular IBL");
                if(inspection==SurfaceView::DiffuseIBL) {
                    if(lit->Color[p].r>plain->Color[p].r+1e-6f)throw std::runtime_error("XeGTAO adds diffuse energy");
                    if(lit->Color[p].r<plain->Color[p].r-1e-4f)++darkened;
                }
            }
            if(inspection==SurfaceView::DiffuseIBL && darkened<10)throw std::runtime_error("XeGTAO did not affect diffuse IBL contacts");
            if(inspection==SurfaceView::DiffuseIBL) {
                frame.Presentation.AmbientOcclusion.Enabled=true;frame.AmbientOcclusionView=1;auto visibility=render();frame.AmbientOcclusionView=0;
                // This existing constant image has red=128/255 and is sampled as linear AO data.
                material->Textures[3].Image="Reference/Direct_v1/Surfaces/NormalPlusY.tga";
                auto mapped=render();
                for(size_t p=0;p<mapped->Color.size();++p)if(mapped->Entity[p]!=-1)
                    for(unsigned c=0;c<3;++c) {
                        const auto expected=plain->Color[p][c]*std::min(128.f/255.f,visibility->Color[p].r);
                        if(std::abs(mapped->Color[p][c]-expected)>1e-5f)throw std::runtime_error("Material and screen-space AO do not combine by minimum visibility");
                    }
                material->Textures[3].Image.clear();
            }
            save(std::string(ortho?"ortho":"perspective")+(inspection==SurfaceView::DiffuseIBL?"-diffuse-on":"-specular-on"),lit);
            save(std::string(ortho?"ortho":"perspective")+(inspection==SurfaceView::DiffuseIBL?"-diffuse-off":"-specular-off"),plain);
        }
        for(const auto environment:{"Reference/IBL_v1/Photo.henv","Reference/IBLQuality_v2/TinyEmitter.henv"}) {
            frame.Environment=CreateRef<PreparedEnvironment>(ReadPreparedEnvironment(project->GetAssetRoot(),environment));
            material->Metallic=1;material->BaseColor={.06f,.055f,.05f,1};frame.Debug=SurfaceView::SpecularIBL;
            for(float roughness:{.2f,.8f}) {
                material->Roughness=roughness;frame.Presentation.AmbientOcclusion.Enabled=true;auto metalOn=render();
                frame.Presentation.AmbientOcclusion.Enabled=false;auto metalOff=render();
                if(metalOn->Color!=metalOff->Color)throw std::runtime_error("Diffuse GTAO changed dark-metal specular IBL");
            }
        }
        material->Metallic=0;material->Roughness=.5f;material->BaseColor=glm::vec4(.5f,.5f,.5f,1);
        frame.Environment.reset();frame.Light.reset();frame.Debug=SurfaceView::Shaded;frame.Presentation.SceneReferred=true;
        material->Emissive={4,1,.25f};frame.Presentation.AmbientOcclusion.Enabled=true;auto emissiveOn=render();
        frame.Presentation.AmbientOcclusion.Enabled=false;auto emissiveOff=render();
        if(emissiveOn->Color!=emissiveOff->Color)throw std::runtime_error("XeGTAO changed emission or its tone mapping");
        material->Emissive=glm::vec3(0);frame.Presentation.SceneReferred=false;
        frame.Debug=SurfaceView::Direct;frame.Light.reset();
        frame.AmbientOcclusionView=1;frame.Presentation.AmbientOcclusion.Enabled=true;
    }
    auto geometry=frame.Meshes;frame.Meshes.clear();render();
    if(view.AmbientTiming->Valid)throw std::runtime_error("Empty geometry retained stale AO GPU timing");
    frame.Meshes=std::move(geometry);
    auto validCamera=frame.Camera;frame.Camera.ViewProjection=glm::mat4(0);
    bool rejected=false;try{render();}catch(const std::runtime_error&){rejected=true;}
    if(!rejected)throw std::runtime_error("XeGTAO accepted singular camera projection");
    frame.Camera=validCamera;
    for(auto extent:{glm::uvec2(317,179),glm::uvec2(1,17),glm::uvec2(17,1),glm::uvec2(1,1),glm::uvec2(640,360),glm::uvec2(320,180)}) {
        display->Resize(extent.x,extent.y);auto resized=render();
        if(resized->Width!=extent.x||resized->Height!=extent.y)throw std::runtime_error("XeGTAO resize extent differs");
        for(const auto& color:resized->Color)if(!std::isfinite(color.r)||color.r<0||color.r>1.001f)throw std::runtime_error("XeGTAO resize visibility invalid");
    }
    view.Reset();display.reset();
    {
        auto demo=Project::LoadCandidate(std::filesystem::absolute("examples/RenderingReference/RenderingReference-XeGTAO.hproj"));
        auto requests=demo->GetRendererRequests();requests.TextureSlots=RenderCommand::GetSettings().TextureSlots;demo->GetConfig().Rendering=requests;
        RuntimeSession session;session.Start(demo,demo->LoadScene(demo->GetConfig().StartScene));
        spec.Width=640;spec.Height=360;display=Framebuffer::Create(spec);
        session.Resize(640,360);session.ResizeFramebuffer(640,360);
        SceneRenderView demoView;
        Ref<const FramebufferReadback> demoBaseline;
        for(unsigned mode=0;mode<3;++mode) {
            session.GetScene()->Presentation().AmbientOcclusion.Enabled=mode!=0;
            demoView.AmbientOcclusionView=mode==2?1:0;
            display->Bind();display->Clear({0,0,0,1});display->ClearAttachment(1,-1);
            session.GetScene()->OnUpdateRuntime(Timestep(0),&demoView,display);
            if(!session.GetScene()->RenderError().empty())throw std::runtime_error(session.GetScene()->RenderError());
            auto image=capture(display);
            if(mode==0)demoBaseline=image;
            else {
                unsigned uiPixels=0;
                const int label=int(session.GetScene()->GetEntityByUUID(71)),backing=int(session.GetScene()->GetEntityByUUID(72));
                for(size_t p=0;p<image->Color.size();++p)if(demoBaseline->Entity[p]==label || demoBaseline->Entity[p]==backing) {
                    ++uiPixels;
                    if(image->Color[p]!=demoBaseline->Color[p] || image->Entity[p]!=demoBaseline->Entity[p] || image->Depth[p]!=demoBaseline->Depth[p])
                        throw std::runtime_error("XeGTAO changed screen UI color, picking or depth");
                }
                if(uiPixels<100)throw std::runtime_error("XeGTAO screen UI fixture has no coverage");
            }
            save(mode==0?"demo-off":mode==1?"demo-on":"demo-visibility",image);
        }
        session.Stop();demoView.Reset();display.reset();
    }
    Renderer3D::Shutdown();
    if(planeFailure)throw std::runtime_error("XeGTAO isolated plane .97 visibility gate failed (other cases completed)");
}

// Isolated pass measurements while a visible desktop is unavailable. Readback
// and synchronous completion are intentional and excluded from application claims.
inline void SceneOffscreenBenchmark(const std::filesystem::path& output,
    const std::function<Hazel::Ref<const Hazel::FramebufferReadback>(const Hazel::Ref<Hazel::Framebuffer>&)>& capture, bool temporalMatrix)
{
    using namespace Hazel;
    using Clock=std::chrono::steady_clock;
    auto project=Project::LoadCandidate(std::filesystem::absolute("examples/RenderingReference/RenderingReference-XeGTAO.hproj"));
    auto requests=project->GetRendererRequests();requests.TextureSlots=4;project->GetConfig().Rendering=requests;
    RuntimeSession session;session.Start(project,project->LoadScene(project->GetConfig().StartScene));
    FramebufferSpecification spec;spec.Attachments={FramebufferTextureFormat::RGBA32F,FramebufferTextureFormat::RED_INTEGER,FramebufferTextureFormat::DEPTH32F};
    SceneRenderView view;Ref<Framebuffer> target;
    std::ofstream csv(output/"offscreen-timings.csv");
    csv<<"serial,quality,width,height,iteration,scene_ms,capture_wait_ms,frame_ms,depth_ms,prefilter_ms,main_ms,denoise_ms,forward_world_ms,working_bytes,taa,resolve_ms,history_bytes,history_allocations,history_reuses,shading_ms,motion_ms\n";
    for(auto extent:{glm::uvec2(1280,720),glm::uvec2(1920,1080)}) {
        spec.Width=extent.x;spec.Height=extent.y;target=Framebuffer::Create(spec);
        session.Resize(extent.x,extent.y);session.ResizeFramebuffer(extent.x,extent.y);
        for(int quality=-1;quality<4;++quality) {
            auto& ao=session.GetScene()->Presentation().AmbientOcclusion;ao.Enabled=quality>=0;if(ao.Enabled)ao.Quality=unsigned(quality);
            for(bool temporal:{false,true}) {
            if(temporal && !temporalMatrix)continue;
            session.GetScene()->Presentation().TemporalAA=temporal;view.Reset();
            for(unsigned iteration=0;iteration<160;++iteration) {
                const auto start=Clock::now();target->Bind();target->Clear({0,0,0,1});target->ClearAttachment(1,-1);
                session.GetScene()->OnUpdateRuntime(Timestep(0),&view,target);
                if(!session.GetScene()->RenderError().empty())throw std::runtime_error(session.GetScene()->RenderError());
                const auto rendered=Clock::now();auto result=capture(target);const auto completed=Clock::now();
                if(!result || !result->Completed || !result->Error.empty())throw std::runtime_error("Offscreen AO benchmark did not complete");
                const auto& timing=*view.AmbientTiming;
                if(ao.Enabled && !timing.Valid)throw std::runtime_error("Offscreen AO benchmark has no GPU counters");
                csv<<result->Serial<<','<<quality<<','<<extent.x<<','<<extent.y<<','<<iteration<<','
                    <<std::chrono::duration<double,std::milli>(rendered-start).count()<<','
                    <<std::chrono::duration<double,std::milli>(completed-rendered).count()<<','
                    <<std::chrono::duration<double,std::milli>(completed-start).count();
                for(auto value:timing.Milliseconds)csv<<','<<(ao.Enabled?value:0);
                const auto& taa=*view.TemporalMetrics;
                if(temporal&&!taa.Valid)throw std::runtime_error("Offscreen TAA benchmark has no GPU counters");
                csv<<','<<(ao.Enabled?timing.WorkingBytes:0)<<','<<temporal<<','<<(temporal?taa.ResolveMilliseconds:0)<<','
                    <<(temporal?taa.HistoryBytes:0)<<','<<(temporal?taa.Allocations:0)<<','<<(temporal?taa.Reuses:0)<<','<<(temporal?taa.ShadingMilliseconds:0)<<','<<(temporal?taa.MotionMilliseconds:0)<<'\n';
            }
            }
        }
    }
    session.Stop();view.Reset();target.reset();Renderer3D::Shutdown();
}
inline void XeGTAOOffscreenBenchmark(const std::filesystem::path& output,
    const std::function<Hazel::Ref<const Hazel::FramebufferReadback>(const Hazel::Ref<Hazel::Framebuffer>&)>& capture)
{SceneOffscreenBenchmark(output,capture,false);}
inline void TemporalAAOffscreenBenchmark(const std::filesystem::path& output,
    const std::function<Hazel::Ref<const Hazel::FramebufferReadback>(const Hazel::Ref<Hazel::Framebuffer>&)>& capture)
{SceneOffscreenBenchmark(output,capture,true);}
