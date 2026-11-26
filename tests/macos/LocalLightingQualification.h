#pragma once
#include "Renderer3DParity.h"
#include "../migration/GeometricNormalReference.h"
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
        frame.Debug=debug;frame.LinearLightingInspection=debug!=SurfaceView::Normal;target->Bind();
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
            if(FileSystem::GetEnvironmentPath("HAZEL_LOCAL_BENCHMARK")=="terminator" &&
                (extent.x!=1920 || !culling || tileSize!=16 ||
                 !((count==32 && sparse) || (count==128 && sparse) || (count==320 && !sparse))))continue;
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
    if (!FileSystem::GetEnvironmentPath("HAZEL_LOCAL_TERMINATOR_DIAGNOSTIC").empty()) {
        // The front face is analytic (+Z). Tilt its shading normals toward a
        // light crossing the geometric horizon while keeping N.V and N.L positive.
        auto tilted = CreateRef<MeshAsset>(*mesh.Accepted);
        ++tilted->Generation;
        for (auto &vertex : tilted->Geometry.Vertices)
            if (vertex.Normal.z > .99f) {
                vertex.Normal = glm::vec3(.5f, 0, std::sqrt(.75f));
                const glm::vec3 tangent(vertex.Tangent);
                vertex.Tangent = glm::vec4(glm::normalize(tangent - vertex.Normal * glm::dot(vertex.Normal, tangent)),
                    vertex.Tangent.w);
            }
        frame.Meshes[0].Geometry = tilted;
        for (float metallic : {0.f, 1.f}) {
            material->Metallic = metallic;
            for (bool spot : {false, true}) {
                SubmittedLocalLight source;
                source.Range = 4;
                source.Incident = glm::vec3(1);
                if (spot) {
                    source.Direction = {-1, 0, 0};
                    source.InnerCos = .9f;
                    source.OuterCos = .8f;
                }
                const auto sample = [&](float height) {
                    source.Position = glm::vec3(centerPosition) + glm::vec3(1, 0, height);
                    frame.LocalLights = {source};
                    const auto lit = render(SurfaceView::Direct);
                    if (lit->Entity[center] != draw.Picking || lit->Depth[center] != positions->Depth[center])
                        throw std::runtime_error("Terminator fixture changed center ID/depth");
                    return lit->Color[center].r;
                };
                const float lit = sample(.1f), grazing = sample(.001f), behind = sample(-.001f);
                std::cout << "LOCAL_TERMINATOR metal=" << metallic << " spot=" << spot
                          << " lit=" << lit << " grazing=" << grazing << " behind=" << behind << std::endl;
                if (!(lit > .001f && grazing >= 0 && grazing < lit * .03f && behind == 0))
                    throw std::runtime_error("Local geometric horizon must approach zero continuously");
            }
        }
        frame.Meshes[0].Geometry = mesh.Accepted;
    }
    if (!FileSystem::GetEnvironmentPath("HAZEL_SMOOTH_TERMINATOR_DIAGNOSTIC").empty()) {
        auto sphere = assets->Mesh("Reference/Direct_v1/Sphere.hmesh");
        if (!sphere.Accepted) throw std::runtime_error(sphere.Error);
        for (unsigned segments : {0u, 16u, 64u}) for (bool perspective : {false, true}) {
        auto geometry = CreateRef<MeshAsset>(*sphere.Accepted);
        geometry->Generation += 10000 + segments * 2 + unsigned(perspective);
        if (segments) {
            auto &data = geometry->Geometry;
            data.Vertices.clear(); data.Indices.clear();
            const unsigned rings = segments / 2;
            for (unsigned row = 0; row <= rings; ++row) for (unsigned column = 0; column <= segments; ++column) {
                const double latitude = -glm::pi<double>() / 2 + glm::pi<double>() * row / rings;
                const double longitude = 2 * glm::pi<double>() * column / segments;
                MeshVertex vertex;
                vertex.Normal = {std::cos(latitude)*std::cos(longitude), std::sin(latitude),
                    std::cos(latitude)*std::sin(longitude)};
                vertex.Position = vertex.Normal * .5f;
                vertex.Tangent = glm::vec4(-std::sin(longitude), 0, std::cos(longitude), 1);
                data.Vertices.push_back(vertex);
            }
            for (unsigned row = 0; row < rings; ++row) for (unsigned column = 0; column < segments; ++column) {
                const unsigned a = row * (segments + 1) + column, b = a + segments + 1;
                if (row) data.Indices.insert(data.Indices.end(), {a, b, a+1});
                if (row + 1 < rings) data.Indices.insert(data.Indices.end(), {a+1, b, b+1});
            }
            data.Primitives.resize(1);
            data.Primitives[0].FirstIndex = 0;
            data.Primitives[0].IndexCount = uint32_t(data.Indices.size());
            data.UpdateBounds();
        }
        frame.Meshes[0].Geometry = geometry;
        frame.Camera.Orthographic = !perspective;
        frame.Camera.Position = {0, 0, 3};
        frame.Camera.ViewProjection = (perspective ? glm::perspective(.9f, 257.f/193.f, .1f, 10.f) :
            glm::ortho(-.8f,.8f,-.6f,.6f,.1f,10.f)) *
            glm::lookAt(frame.Camera.Position, glm::vec3(0), glm::vec3(0,1,0));
        target->Resize(257, 193);
        const auto surfacePositions = render(SurfaceView::WorldPosition);
        const auto faceNormals = render(SurfaceView::GeometricNormal);
        unsigned crossedSamples = 0;
        for (float metallic : {0.f, 1.f}) {
            material->Metallic = metallic;
            for (unsigned step = 0; step < 12; ++step) {
                SubmittedLocalLight source;
                source.Position = {1.5f, -.7f + float(step) * .13f, .1f};
                source.Range = 4;
                source.Incident = glm::vec3(.3f);
                if (step % 2) {
                    source.Direction = glm::normalize(-source.Position);
                    source.InnerCos = std::cos(.5f);
                    source.OuterCos = std::cos(.9f);
                }
                const bool directional = step >= 8;
                frame.LocalLights = directional ? std::vector<SubmittedLocalLight>{} :
                    std::vector<SubmittedLocalLight>{source};
                frame.Light = directional ? std::optional<SubmittedDirectional>(
                    SubmittedDirectional{glm::normalize(source.Position), source.Incident}) : std::nullopt;
                const auto lit = render(SurfaceView::Direct);
                if (metallic == 0 && segments == 0 && !perspective) {
                    std::ofstream image(output / ("terminator-motion-" + std::to_string(step) + ".bin"),
                        std::ios::binary);
                    for (size_t pixel = 0; pixel < lit->Color.size(); ++pixel) {
                        image.write(reinterpret_cast<const char*>(&lit->Color[pixel]), 16);
                        image.write(reinterpret_cast<const char*>(&lit->Entity[pixel]), 4);
                        image.write(reinterpret_cast<const char*>(&lit->Depth[pixel]), 4);
                    }
                }
                for (size_t pixel = 0; pixel < lit->Color.size(); ++pixel) {
                    if (surfacePositions->Entity[pixel] != draw.Picking) continue;
                    const glm::dvec3 position(surfacePositions->Color[pixel]);
                    const auto N = glm::normalize(position);
                    const auto V = perspective ? glm::normalize(glm::dvec3(frame.Camera.Position) - position) :
                        glm::dvec3(0, 0, 1);
                    const double nv = glm::dot(N, V);
                    if (nv < .05) continue;
                    const auto delta = glm::dvec3(source.Position) - position;
                    const double distance = glm::length(delta);
                    const auto L = directional ? glm::normalize(glm::dvec3(source.Position)) : delta / distance;
                    const double nl = glm::dot(N, L);
                    glm::dvec3 expected(0);
                    if (nl > 0) {
                        double attenuation = directional ? 1 : LocalReferenceAttenuation(distance, source.Range);
                        if (!directional && source.OuterCos >= 0)
                            attenuation *= LocalReferenceCone(glm::dot(-L, glm::dvec3(source.Direction)),
                                source.InnerCos, source.OuterCos);
                        expected = ReferenceMultiscatteringBRDF(glm::dvec3(material->BaseColor), metallic, .5,
                            N, V, L, ReferenceSampleRuntimeBRDF(brdf, nv, .5),
                            ReferenceSampleRuntimeBRDF(brdf, nl, .5), average) *
                            glm::dvec3(source.Incident) * attenuation;
                        if (glm::dot(glm::dvec3(faceNormals->Color[pixel]), L) <= 0 &&
                            expected.r > .0001) ++crossedSamples;
                    }
                    if (lit->Entity[pixel] != surfacePositions->Entity[pixel] ||
                        lit->Depth[pixel] != surfacePositions->Depth[pixel])
                        throw std::runtime_error("Smooth horizon changed IDs/depth");
                    for (unsigned c = 0; c < 3; ++c)
                        if (!std::isfinite(lit->Color[pixel][c]) ||
                            std::abs(lit->Color[pixel][c] - expected[c]) > .0002 + .002 * std::abs(expected[c]))
                            throw std::runtime_error("Analytic sphere horizon oracle failed");
                }
            }
        }
        if (crossedSamples < 10) throw std::runtime_error("Smooth horizon did not cross triangle horizons");
        std::cout << "SMOOTH_TERMINATOR PASS analytic sphere, moving point/spot, crossed samples="
                  << crossedSamples << " segments=" << segments << " perspective=" << perspective << std::endl;
        }
        // A single open face distinguishes real backfaces from closed-cube
        // coverage. Split cube normals, material mapping and signed transforms
        // have independent FP64 expectations, rather than image tolerances.
        auto plane = CreateRef<MeshAsset>(*mesh.Accepted);
        plane->Generation += 20000;
        plane->Geometry.Vertices.resize(4);
        plane->Geometry.Indices.resize(6);
        plane->Geometry.Primitives[0].IndexCount = 6;
        plane->Geometry.UpdateBounds();
        target->Resize(65, 49);
        frame.Meshes[0].Geometry = plane;
        material->Metallic = 0;
        unsigned checked = 0;
        double maximumError = 0, maximumNormalError = 0;
        for (bool smooth : {false, true}) for (float mapping : {0.f, 1.f, 8.f})
        for (float mirror : {1.f, -1.f}) for (bool back : {false, true}) {
            const auto authored = smooth ? glm::normalize(glm::dvec3(.3, .4, 1)) : glm::dvec3(0, 0, 1);
            const auto tangent = glm::normalize(glm::dvec3(authored.z, 0, -authored.x));
            ++plane->Generation;
            for (auto &vertex : plane->Geometry.Vertices) {
                vertex.Normal = glm::vec3(authored);
                vertex.Tangent = glm::vec4(glm::vec3(tangent), 1);
                vertex.UV = {.25f, .25f}; // Uniform +Y region, away from the fixture seam.
            }
            material->Textures[2].Image = mapping ? "Reference/Direct_v1/Surfaces/NormalPlusY.tga" : "";
            material->NormalScale = mapping;
            const auto world = glm::rotate(glm::mat4(1), back ? glm::pi<float>() : 0.f, glm::vec3(0,1,0)) *
                glm::scale(glm::mat4(1), glm::vec3(mirror * .7f, 1.3f, .8f));
            frame.Meshes[0].SetWorld(world);
            const auto normalMatrix = glm::transpose(glm::inverse(glm::dmat3(world)));
            auto surface = glm::normalize(normalMatrix * authored);
            auto T = glm::dmat3(world) * tangent;
            T = glm::normalize(T - surface * glm::dot(surface, T));
            const auto B = glm::cross(surface, T) * double(mirror);
            // Existing uniform data-normal fixture is RGB=(128,192,255).
            auto mapped = mapping ? glm::normalize(T * ((2.*128/255-1)*mapping) +
                B * ((2.*192/255-1)*mapping) + surface) : surface;
            if (back) { surface = -surface; mapped = -mapped; }
            for (unsigned camera = 0; camera < 3; ++camera) {
                const auto eye = glm::vec3(.15f * float(camera), -.1f * float(camera), 3);
                frame.Camera = {glm::ortho(-.8f,.8f,-.6f,.6f,.1f,10.f) *
                    glm::lookAt(eye,glm::vec3(0),glm::vec3(0,1,0)), eye, glm::normalize(eye), true};
                material->DoubleSided = false;
                const auto single = render(SurfaceView::Direct);
                if (back && single->Entity[center] != -1)
                    throw std::runtime_error("Single-sided terminator backface was drawn");
                material->DoubleSided = true;
                const auto positionImage = render(SurfaceView::WorldPosition);
                const auto V = glm::normalize(glm::dvec3(eye));
                const auto resolved = GeometricNormalReference(mapped, surface, V);
                auto geometric = glm::normalize(normalMatrix * glm::dvec3(0,0,1));
                if (glm::dot(geometric,V) < 0) geometric = -geometric;
                const auto indirect = GeometricNormalReference(mapped, geometric, V);
                const auto normalImage = render(SurfaceView::Normal);
                const auto actualNormal = glm::dvec3(normalImage->Color[center])*2.-1.;
                const double normalError = glm::length(actualNormal-indirect);
                maximumNormalError = std::max(maximumNormalError,normalError);
                if (normalError > 2e-5 || glm::dot(glm::reflect(-V,actualNormal),geometric) < -2e-5)
                    throw std::runtime_error("Direct terminator changed indirect geometric normal repair");
                for (const auto direction : {glm::dvec3(1,0,.1), glm::dvec3(1,0,-.1), glm::dvec3(0,0,-1)}) {
                    const auto L = glm::normalize(direction);
                    frame.LocalLights.clear();
                    frame.Light = SubmittedDirectional{glm::vec3(L), glm::vec3(.3f)};
                    const auto lit = render(SurfaceView::Direct);
                    const double cosine = glm::dot(surface, L), projected = glm::dot(resolved, L)*glm::dot(surface,resolved);
                    const double g = cosine <= 0 ? 0 : cosine >= projected ? 1 : cosine / projected;
                    glm::dvec3 expected(0);
                    const double nv = glm::dot(resolved,V), nl = glm::dot(resolved,L);
                    if (cosine > 0 && glm::dot(surface,V) > 0 && nv > 0 && nl > 0)
                        expected = ReferenceMultiscatteringBRDF(glm::dvec3(material->BaseColor), 0, .5,
                            resolved,V,L,ReferenceSampleRuntimeBRDF(brdf,nv,.5),
                            ReferenceSampleRuntimeBRDF(brdf,nl,.5),average) * (.3*(g+g*g-g*g*g));
                    if (lit->Entity[center] != draw.Picking || lit->Depth[center] != positionImage->Depth[center])
                        throw std::runtime_error("Mapped/double-sided terminator changed ID/depth");
                    for (unsigned c=0;c<3;++c) {
                        const double error = std::abs(lit->Color[center][c]-expected[c]);
                        maximumError = std::max(maximumError,error);
                        if (!std::isfinite(lit->Color[center][c]) || error > .0002 + .002*std::abs(expected[c]))
                            throw std::runtime_error("Mapped/double-sided terminator FP64 oracle failed smooth=" + std::to_string(smooth) +
                                " mapping=" + std::to_string(mapping) + " mirror=" + std::to_string(mirror) +
                                " back=" + std::to_string(back) + " camera=" + std::to_string(camera) +
                                " lightZ=" + std::to_string(L.z) + " actual=" + std::to_string(lit->Color[center][c]) +
                                " expected=" + std::to_string(expected[c]));
                    }
                    ++checked;
                }
            }
        }
        std::cout << "TERMINATOR_MATERIAL PASS cases=" << checked << " maximumError=" << maximumError << " indirectNormalError=" << maximumNormalError << std::endl;
        material->DoubleSided = false;
        material->NormalScale = 1;
        material->Textures[2].Image.clear();
        frame.Meshes[0].SetWorld(glm::mat4(1));
        frame.Camera.ToCamera = {0,0,1};
        frame.Meshes[0].Geometry = mesh.Accepted;
        frame.Light.reset();
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
