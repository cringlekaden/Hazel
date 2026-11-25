#pragma once
#include "Renderer3DParity.h"
#include "Hazel/Scene/SceneSerializer.h"
#include <iomanip>
#include <algorithm>
#include <chrono>

// Independent double-precision incident-light oracle; BRDF oracle is the accepted
// independent Multiscattering_v2 integral/table reference, shared with its frozen gate.
inline double LocalReferenceAttenuation(double distance, double range) {
    if(distance>=range || distance==0)return 0;
    const double q=distance/range;
    const double window=1-q*q*q*q;
    const double s=distance*distance,denominator=s+.0001;
    return window*window*s*s/(denominator*denominator*denominator);
}
inline double LocalReferenceCone(double cosine,double inner,double outer) {
    if(cosine<=outer)return 0;if(cosine>=inner)return 1;
    const double x=(cosine-outer)/(inner-outer);return 3*x*x-2*x*x*x;
}
inline void LocalLightingQualification(const std::filesystem::path &output,
    const std::function<Hazel::Ref<const Hazel::FramebufferReadback>(const Hazel::Ref<Hazel::Framebuffer>&)> &capture)
{
    using namespace Hazel;
    std::filesystem::create_directories(output);
    auto selected=FileSystem::GetEnvironmentPath("HAZEL_RENDERING_REFERENCE_PROJECT");
    if(selected.empty())throw std::runtime_error("Local lights require explicit reference project");
    auto project=Project::LoadCandidate(selected);auto assets=CreateRef<ProjectAssets>(project->GetAssetRoot());
    auto mesh=assets->Mesh("Native/Direct_v1/UnitCube.hmesh");if(!mesh.Accepted)throw std::runtime_error(mesh.Error);
    const auto brdf=ReadRuntimeBRDF();
    const auto average=ReferenceAverageRuntimeBRDF(brdf,.5);
    const auto viewAB=ReferenceSampleRuntimeBRDF(brdf,1,.5);
    if(LocalReferenceAttenuation(0,2)!=0 || LocalReferenceAttenuation(2,2)!=0 ||
        LocalReferenceAttenuation(2-1e-7,2)>1e-12 || LocalReferenceAttenuation(1e-8,2)>1e-16 ||
        LocalReferenceCone(.5,.8,.5)!=0 || LocalReferenceCone(.8,.8,.5)!=1 ||
        std::abs(LocalReferenceCone(.65,.8,.5)-.5)>1e-12)
        throw std::runtime_error("FP64 local-light analytic anchors failed");
    FramebufferSpecification spec;spec.Width=65;spec.Height=49;
    spec.Attachments={FramebufferTextureFormat::RGBA32F,FramebufferTextureFormat::RED_INTEGER,
        RendererAPI::GetAPI()==RendererAPI::API::Metal?FramebufferTextureFormat::DEPTH32F:FramebufferTextureFormat::Depth};
    auto target=Framebuffer::Create(spec);
    SceneSubmission frame;frame.Assets=assets;frame.Presentation.SceneReferred=true;
    frame.Camera.Orthographic=true;frame.Camera.Position={0,0,3};
    frame.Camera.ViewProjection=glm::ortho(-.8f,.8f,-.6f,.6f,.1f,10.f)*glm::lookAt(glm::vec3(0,0,3),glm::vec3(0),glm::vec3(0,1,0));
    auto material=CreateRef<MaterialDefinition>();material->BaseColor={.7f,.3f,.1f,1};material->Roughness=.5f;
    SubmittedMesh draw;draw.Geometry=mesh.Accepted;draw.Materials={material};draw.Picking=4242;frame.Meshes={draw};
    SceneRenderView view;frame.LocalMetrics=view.LocalMetrics;
    auto render=[&](SurfaceView debug) {
        frame.Debug=debug;frame.LinearLightingInspection=true;target->Bind();
        if(RendererAPI::GetAPI()==RendererAPI::API::Metal)target->Clear({0,0,0,1});
        else {RenderCommand::SetClearColor({0,0,0,1});RenderCommand::Clear();}
        target->ClearAttachment(1,-1);SceneRenderer::Render(frame,view,target,[]{},[]{});
        auto result=capture(target);
        if(!result || !result->Completed || !result->Error.empty())throw std::runtime_error("Local lighting capture failed");
        return result;
    };
    if(!FileSystem::GetEnvironmentPath("HAZEL_LOCAL_BENCHMARK").empty()) {
        std::ofstream timing(output/"benchmark.csv");
        timing<<std::setprecision(12)<<"width,height,count,sparse,culling,tileSize,sample,collectionCPUms,encodeCPUms,lightPrepCPUms,serializedFrameMs,cullGPUms,forwardGPUms,lightBytes,tileBytes,candidates,overflowTiles\n";
        for(auto extent:{glm::uvec2(1280,720),glm::uvec2(1920,1080)})for(unsigned count:{0u,1u,8u,32u,128u,256u,320u})
          for(bool sparse:{false,true})for(bool culling:{false,true})for(unsigned tileSize:{16u,32u}) {
            if(!culling && tileSize==32)continue;
            if(FileSystem::GetEnvironmentPath("HAZEL_LOCAL_BENCHMARK")=="zero" && count)continue;
            target->Resize(extent.x,extent.y);
            auto scene=CreateRef<Scene>();scene->Presentation().SceneReferred=true;
            for(unsigned i=0;i<count;++i) {
                auto e=scene->CreateEntityWithUUID(i+1,"Benchmark light");PointLight *light=nullptr;
                if(i%2){auto &spot=e.AddComponent<SpotLightComponent>();spot.InnerCone=.3f;spot.OuterCone=.8f;light=&spot;}
                else light=&e.AddComponent<PointLightComponent>();
                light->Intensity=.25f;light->Range=sparse?1.5f:4;
                e.GetComponent<TransformComponent>().Translation={float(int(i%17)-8)*(sparse?.6f:.04f),float(int(i%13)-6)*(sparse?.6f:.04f),1.5f};
                if(i==0)e.GetComponent<TransformComponent>().Translation={0,0,1.5f};
            }
            frame.LocalLightCulling=culling;frame.ForceLocalLightCulling=culling;frame.LocalLightTileSize=tileSize;
            for(unsigned sample=0;sample<25;++sample) {
                const auto start=std::chrono::steady_clock::now();
                auto collected=SceneRenderer::Collect(*scene,frame.Camera,extent);
                frame.LocalLights=std::move(collected.LocalLights);frame.LightCollectionMilliseconds=collected.LightCollectionMilliseconds;
                frame.Debug=SurfaceView::Direct;frame.LinearLightingInspection=true;target->Bind();
                if(RendererAPI::GetAPI()==RendererAPI::API::Metal)target->Clear({0,0,0,1});
                else {RenderCommand::SetClearColor({0,0,0,1});RenderCommand::Clear();}
                target->ClearAttachment(1,-1);
                const auto encodeStart=std::chrono::steady_clock::now();
                SceneRenderer::Render(frame,view,target,[]{},[]{});
                const double encode=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-encodeStart).count();
                auto result=capture(target);
                if(!result || !result->Completed || !result->Error.empty())throw std::runtime_error("Local benchmark capture failed");
                const double whole=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
                if(sample>=5){const auto &m=*frame.LocalMetrics;
                    timing<<extent.x<<','<<extent.y<<','<<count<<','<<sparse<<','<<culling<<','<<tileSize<<','<<sample-5<<','<<collected.LightCollectionMilliseconds<<','<<encode<<','<<m.UploadMilliseconds<<','<<whole<<','<<m.CullMilliseconds<<','<<m.ForwardMilliseconds<<','<<m.LightBytes<<','<<m.TileBytes<<','<<m.Candidates<<','<<m.OverflowTiles<<std::endl;
                }
            }
        }
        std::cout<<"LOCAL_LIGHTS benchmark completed (serialized capture; excludes presentation)\n";return;
    }
    const auto positions=render(SurfaceView::WorldPosition);
    SubmittedLocalLight invalidCone;invalidCone.OuterCos=-.5f;frame.LocalLights={invalidCone};
    bool invalidRejected=false;try{render(SurfaceView::Direct);}catch(const std::exception& error){invalidRejected=std::string(error.what()).find("Invalid submitted spotlight")!=std::string::npos;}
    if(!invalidRejected)throw std::runtime_error("Invalid submitted cone silently became a point light");
    frame.LocalLights.clear();
    std::ofstream csv(output/"local-lights.csv");csv<<std::setprecision(12)<<"count,sparse,metal,maximumError,orderError,samples,candidates,overflowTiles\n";
    for(unsigned count:{0u,1u,8u,9u,32u,64u,65u,128u,256u,320u})for(bool sparse:{false,true})for(float metal:{0.f,1.f}) {
        material->Metallic=metal;frame.LocalLights.clear();
        for(unsigned i=0;i<count;++i) {
            SubmittedLocalLight l;l.Identity=i+1;
            l.Position={float(int(i%7)-3)*.23f,float(int(i%11)-5)*.16f,.8f+float(i%5)*.3f};
            if(sparse)l.Position.x+=float(int(i%5)-2)*1.5f;
            l.Range=sparse?.75f:4.f;l.Incident={.1f+float(i%3)*.1f,.2f,.15f};
            if(i%2){l.InnerCos=std::cos(.3f);l.OuterCos=std::cos(.8f);}
            if(count==1 && sparse){l.Position={0,0,1};l.Range=2;l.InnerCos=std::cos(.3f);l.OuterCos=std::cos(.8f);}
            frame.LocalLights.push_back(l);
        }
        frame.LocalLightCulling=true;
        auto result=render(SurfaceView::Direct);double maximum=0,order=0;unsigned samples=0;
        const auto tileMetrics=*frame.LocalMetrics;
        if(RendererAPI::GetAPI()==RendererAPI::API::Metal && count>64 && !sparse &&
            (!tileMetrics.Valid || tileMetrics.OverflowTiles==0))throw std::runtime_error("Dense local-light test did not exercise tile overflow");
        for(size_t pixel=0;pixel<positions->Color.size();++pixel) {
            if(positions->Entity[pixel]!=draw.Picking)continue;
            if(result->Entity[pixel]!=draw.Picking || std::abs(result->Depth[pixel]-positions->Depth[pixel])>1e-7)
                throw std::runtime_error("Local lights changed IDs/depth");
            const glm::dvec3 p(positions->Color[pixel]);glm::dvec3 expected(0);
            for(const auto &l:frame.LocalLights) {
                const auto delta=glm::dvec3(l.Position)-p;const auto distance=glm::length(delta);
                if(distance==0 || distance>=l.Range)continue;
                const auto L=delta/distance;if(L.z<=0)continue;
                double attenuation=LocalReferenceAttenuation(distance,l.Range);
                if(l.OuterCos>=0)attenuation*=LocalReferenceCone(glm::dot(-L,glm::dvec3(l.Direction)),l.InnerCos,l.OuterCos);
                if(attenuation==0)continue;
                expected+=ReferenceMultiscatteringBRDF(glm::dvec3(material->BaseColor),metal,.5,{0,0,1},{0,0,1},L,viewAB,ReferenceSampleRuntimeBRDF(brdf,L.z,.5),average)*glm::dvec3(l.Incident)*attenuation;
            }
            for(unsigned c=0;c<3;++c) {
                const double error=std::abs(result->Color[pixel][c]-expected[c]);maximum=std::max(maximum,error);
                if(!std::isfinite(result->Color[pixel][c]) || error>.003+2e-5*std::abs(expected[c]))
                    throw std::runtime_error("Local FP64 oracle count="+std::to_string(count)+" sparse="+std::to_string(sparse)+" pixel="+std::to_string(pixel)+" error="+std::to_string(error));
            }
            ++samples;
        }
        std::reverse(frame.LocalLights.begin(),frame.LocalLights.end());frame.LocalLightCulling=false;const auto reverse=render(SurfaceView::Direct);
        for(size_t pixel=0;pixel<result->Color.size();++pixel)for(unsigned c=0;c<3;++c){
            const double error=std::abs(result->Color[pixel][c]-reverse->Color[pixel][c]);order=std::max(order,error);
            if(error>2e-5*std::max(1.f,std::abs(result->Color[pixel][c])))throw std::runtime_error("Light order invariance failed");
        }
        if(samples<100)throw std::runtime_error("Local oracle did not cover geometry");
        csv<<count<<','<<sparse<<','<<metal<<','<<maximum<<','<<order<<','<<samples<<','<<tileMetrics.Candidates<<','<<tileMetrics.OverflowTiles<<std::endl;
    }
    const size_t center=24*65+32;const glm::dvec3 centerPosition(positions->Color[center]);
    for(float distance:{0.f,.000001f,.001f,.01f,.02f,.1f,1.f,1.99999f,2.f,2.00001f}) {
        SubmittedLocalLight source;source.Position=glm::vec3(centerPosition)+glm::vec3(0,0,distance);
        source.Range=2;source.Incident={.2f,.1f,.3f};frame.LocalLights={source};
        const auto result=render(SurfaceView::Direct);
        const double actualDistance=glm::length(glm::dvec3(source.Position)-centerPosition);
        const auto response=ReferenceMultiscatteringBRDF(glm::dvec3(material->BaseColor),material->Metallic,.5,
            {0,0,1},{0,0,1},{0,0,1},viewAB,viewAB,average);
        const auto expected=response*glm::dvec3(source.Incident)*LocalReferenceAttenuation(actualDistance,2);
        for(unsigned c=0;c<3;++c)if(!std::isfinite(result->Color[center][c]) ||
            std::abs(result->Color[center][c]-expected[c])>.003+2e-5*std::abs(expected[c]))
            throw std::runtime_error("Local near-source/range FP64 oracle failed at "+std::to_string(distance));
    }
    // Matched full-loop comparisons isolate culling through projection changes,
    // camera cuts, near intersections, object/light motion, tile edges and resize.
    for(unsigned step=0;step<30;++step) {
        const glm::uvec2 sizes[]={{1,1},{17,31},{65,49},{128,72},{257,145}};
        const auto size=sizes[step%5];target->Resize(size.x,size.y);
        frame.Camera.Orthographic=step%3==0;
        frame.Camera.Position={.25f*std::sin(float(step)),.15f,.8f+float(step%7)*.4f};
        const auto cameraView=glm::lookAt(frame.Camera.Position,glm::vec3(0),glm::vec3(0,1,0));
        frame.Camera.ViewProjection=(frame.Camera.Orthographic?glm::ortho(-1.f,1.f,-1.f,1.f,.1f,10.f):
            glm::perspective(.9f,float(size.x)/float(size.y),step%2?.1f:.4f,10.f))*cameraView;
        frame.Meshes[0].SetWorld(glm::rotate(glm::mat4(1),float(step)*.13f,glm::vec3(0,1,0)));
        frame.LocalLights.resize(step%2?320:32);
        for(size_t i=0;i<frame.LocalLights.size();++i){auto &l=frame.LocalLights[i];
            l={};l.Identity=i+1;l.Position={float(int(i%7)-3)*.4f+std::sin(float(step)*.2f),float(int(i%11)-5)*.2f,float(int(i%5)-2)*.7f};
            l.Range=i%3?.7f:4;l.Incident=glm::vec3(.1f);
            if(i%2){l.InnerCos=std::cos(.3f);l.OuterCos=std::cos(.8f);}
        }
        frame.LocalLightTileSize=step%2?16:32;frame.LocalLightCulling=true;
        const auto culled=render(SurfaceView::Direct);frame.LocalLightCulling=false;
        const auto complete=render(SurfaceView::Direct);
        for(size_t pixel=0;pixel<culled->Color.size();++pixel){
            if(culled->Entity[pixel]!=complete->Entity[pixel] || culled->Depth[pixel]!=complete->Depth[pixel])
                throw std::runtime_error("Culling motion/resize changed IDs/depth");
            for(unsigned c=0;c<3;++c)if(!std::isfinite(culled->Color[pixel][c]) ||
                std::abs(culled->Color[pixel][c]-complete->Color[pixel][c])>2e-5*std::max(1.f,std::abs(complete->Color[pixel][c])))
                throw std::runtime_error("Culling motion/resize/frustum mismatch step="+std::to_string(step));
        }
    }
    // New showcase output is separate from every historical frozen image.
    auto showcase=CreateRef<Scene>();
    if(!SceneSerializer(showcase,project->GetAssetRoot(),false,assets).Deserialize(
        (project->GetAssetRoot()/"Scenes/LocalLights_v1.hazel").string()))throw std::runtime_error("Showcase load failed");
    showcase->OnViewportResize(1280,720);auto camera=showcase->GetPrimaryCameraEntity();
    const auto world=showcase->GetWorldTransform(camera);
    SceneCameraView authored;authored.View=glm::inverse(world);authored.Projection=camera.GetComponent<CameraComponent>().Camera.GetProjection();
    authored.ViewProjection=authored.Projection*authored.View;authored.Position=glm::vec3(world[3]);
    authored.ToCamera=glm::normalize(glm::vec3(world[2]));authored.Orthographic=false;
    frame=SceneRenderer::Collect(*showcase,authored,{1280,720});frame.LocalMetrics=view.LocalMetrics;
    target->Resize(1280,720);frame.Debug=SurfaceView::Shaded;frame.LinearLightingInspection=false;
    target->Bind();if(RendererAPI::GetAPI()==RendererAPI::API::Metal)target->Clear({0,0,0,1});
    else {RenderCommand::SetClearColor({0,0,0,1});RenderCommand::Clear();}target->ClearAttachment(1,-1);
    SceneRenderer::Render(frame,view,target,[]{},[]{});const auto gallery=capture(target);
    if(!gallery || !gallery->Completed || !gallery->Error.empty())throw std::runtime_error("Showcase capture failed");
    std::ofstream image(output/"showcase.bin",std::ios::binary);
    for(size_t pixel=0;pixel<gallery->Color.size();++pixel){
        for(unsigned c=0;c<4;++c)if(!std::isfinite(gallery->Color[pixel][c]))throw std::runtime_error("Nonfinite showcase");
        image.write(reinterpret_cast<const char*>(&gallery->Color[pixel]),16);
        image.write(reinterpret_cast<const char*>(&gallery->Entity[pixel]),4);
        image.write(reinterpret_cast<const char*>(&gallery->Depth[pixel]),4);
    }
    std::cout<<"LOCAL_LIGHTS PASS FP64 accumulation/order/IDs/depth, 0..320 sparse/dense point/spot, odd viewport\n";
}
