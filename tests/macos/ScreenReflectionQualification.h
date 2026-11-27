#pragma once

#include "Renderer3DParity.h"
#include "Hazel/Core/FileDocument.h"
#include "Hazel/Renderer/RenderCommand.h"
#include "Hazel/Scene/SceneSerializer.h"
#include <glad/glad.h>
#include <cmath>
#include <iostream>
#include <chrono>
#include <thread>
#ifdef HZ_PLATFORM_MACOS
#include "Platform/macOS/Metal/MetalRendererAPI.h"
#endif

inline void ScreenReflectionQualification(const std::filesystem::path& output,
    const Hazel::Ref<Hazel::Project>& project, const Hazel::Ref<Hazel::ProjectAssets>& assets,
    const std::function<Hazel::Ref<const Hazel::FramebufferReadback>(const Hazel::Ref<Hazel::Framebuffer>&)>& capture)
{
    using namespace Hazel;
    const bool metal=RendererAPI::GetAPI()==RendererAPI::API::Metal;
    const auto scene=CreateRef<Scene>();
    SceneSerializer reader(scene,assets->Root(),false,assets);
    if (!reader.DeserializeText(FileDocument::Read(Project::ResolveOwnedAsset(
            assets->Root(),project->GetConfig().StartScene)),false))
        throw std::runtime_error(reader.Report().Error);
    if (!scene->Presentation().ScreenReflections.Enabled)
        throw std::runtime_error("Expected an authored SSR Gallery");
    if (!(SceneDefaults::ReadPresentation(SceneDefaults::Encode(scene->Presentation()))==scene->Presentation()))
        throw std::runtime_error("SSR settings did not survive presentation serialization");
    SceneRenderView view;
    FramebufferSpecification spec;
    spec.Width=640; spec.Height=360;
    spec.Attachments={FramebufferTextureFormat::RGBA32F,FramebufferTextureFormat::RED_INTEGER,
        metal?FramebufferTextureFormat::DEPTH32F:FramebufferTextureFormat::Depth};
    auto target=Framebuffer::Create(spec);
    auto draw=[&](const SceneSubmission& frame, GLuint query=0) {
        target->Bind();
        if (metal)
            target->Clear({0,0,0,1});
        else {
            RenderCommand::SetClearColor({0,0,0,1});
            RenderCommand::Clear();
        }
        target->ClearAttachment(1,-1);
        if (query) glBeginQuery(GL_TIME_ELAPSED,query);
        SceneRenderer::Render(frame,view,target,[]{},[]{});
        if (query) glEndQuery(GL_TIME_ELAPSED);
    };
    auto render=[&](const SceneSubmission& frame, GLuint query=0) {
        draw(frame,query);
        auto result=capture(target);
        if (!result || !result->Completed || !result->Error.empty())
            throw std::runtime_error("Incomplete SSR readback");
        for (const auto& pixel : result->Color)
            if (!std::isfinite(pixel.r) || !std::isfinite(pixel.g) || !std::isfinite(pixel.b))
                throw std::runtime_error("Nonfinite SSR output");
        return result;
    };
    auto camera=[](glm::vec3 eye, glm::vec3 focus, bool orthographic) {
        SceneCameraView lens;
        lens.Orthographic=orthographic;
        lens.Position=eye;
        lens.View=glm::lookAt(eye,focus,glm::vec3(0,1,0));
        lens.ToCamera=glm::vec3(glm::inverse(lens.View)[2]);
        lens.Projection=orthographic ? glm::ortho(-6.f,6.f,-3.375f,3.375f,.1f,50.f) :
            glm::perspective(glm::radians(50.f),640.f/360.f,.1f,50.f);
        lens.ViewProjection=lens.Projection*lens.View;
        return lens;
    };
    auto gallery=[&](float x,glm::uvec2 extent) {
        return SceneRenderer::Collect(*scene,camera({x,3.35f,9.3f},{.38f*x,1.18f,-.6f},false),extent);
    };
    if (!FileSystem::GetEnvironmentPath("HAZEL_SSR_BENCHMARK").empty()) {
        struct TimingQuery {
            GLuint ID=0;
            ~TimingQuery() { if (ID) glDeleteQueries(1,&ID); }
        } query;
        if (!metal) {
            GLint bits=0;
            glGetQueryiv(GL_TIME_ELAPSED,GL_QUERY_COUNTER_BITS,&bits);
            if (!bits) throw std::runtime_error("OpenGL has no GPU elapsed query");
            glGenQueries(1,&query.ID);
        }
        for (const auto& extent : {glm::uvec2(1280,720),glm::uvec2(1920,1080),glm::uvec2(2560,1440)}) {
            target->Resize(extent.x,extent.y);
            auto frame=gallery(0,extent);
            std::array<SceneRenderView,2> states;
            for (int sample=-4; sample<15; ++sample)
                for (unsigned i=0;i<2;++i) {
                    const unsigned enabled=(i+unsigned(sample+4))%2;
                    frame.Presentation.ScreenReflections.Enabled=enabled!=0;
                    std::swap(view,states[enabled]);
                    const auto result=render(frame,query.ID);
                    std::swap(view,states[enabled]);
                    GLuint64 ns=0;
                    if (query.ID) glGetQueryObjectui64v(query.ID,GL_QUERY_RESULT,&ns);
                    if (sample>=0) {
                        if (query.ID && (!ns || glGetError()!=GL_NO_ERROR))
                            throw std::runtime_error("OpenGL SSR GPU timing failed");
                        std::cout << "SSR_TIMING " << extent.x << ' ' << extent.y << ' ' << enabled
                                  << ' ' << sample << ' ' << result->Serial << ' ' << ns << '\n';
                    }
                }
        }
        return;
    }
    auto save=[&](const std::string& name,const Ref<const FramebufferReadback>& result) {
        std::ofstream file(output/(name+".bin"),std::ios::binary);
        for (size_t i=0;i<result->Color.size();++i) {
            file.write(reinterpret_cast<const char*>(&result->Color[i]),16);
            file.write(reinterpret_cast<const char*>(&result->Entity[i]),4);
            file.write(reinterpret_cast<const char*>(&result->Depth[i]),4);
        }
        if (!file) throw std::runtime_error("Could not write SSR readback");
    };
    auto coverage=[](const auto& a,const auto& b) {
        if (a->Entity!=b->Entity || a->Depth!=b->Depth)
            throw std::runtime_error("SSR changed ID/depth coverage");
    };
    // An exact plane/rectangle intersection in world space. An emissive,
    // unlit wall makes the reference independent of the PBR and IBL models.
    auto cube=assets->Mesh("Native/Direct_v1/UnitCube.hmesh");
    if (!cube.Accepted) throw std::runtime_error(cube.Error);
    auto floor=CreateRef<MaterialDefinition>();
    floor->BaseColor={.83f,.83f,.83f,1}; floor->Metallic=1; floor->Roughness=.045f;
    auto wall=CreateRef<MaterialDefinition>();
    wall->Model=MaterialModel::Unlit; wall->BaseColor={.54f,.11f,.09f,1};
    auto box=[&](int id,const auto& material,glm::vec3 center,glm::vec3 size) {
        SubmittedMesh draw;
        draw.Geometry=cube.Accepted; draw.Materials={material}; draw.Picking=id; draw.Identity=unsigned(id);
        draw.SetWorld(glm::translate(glm::mat4(1),center)*glm::scale(glm::mat4(1),size));
        return draw;
    };
    target->Resize(320,180);
    for (bool ortho : {false,true}) {
        SceneSubmission frame;
        frame.Assets=assets; frame.Presentation.SceneReferred=true;
        frame.Camera=camera({0,3.2f,6},{0,.9f,-2},ortho);
        frame.Meshes={box(1,floor,{0,-.1f,0},{12,.2f,18}),
                      box(2,wall,{0,1.2f,-3.02f},{3,2.4f,.04f})};
        const auto off=render(frame);
        frame.Presentation.ScreenReflections.Enabled=true;
        frame.Presentation.ScreenReflections.Steps=80;
        const auto on=render(frame);
        coverage(on,off);
        const auto repeated=render(frame);
        if (on->Color!=repeated->Color)
            throw std::runtime_error("SSR changed repeated AA-off frames");
        save(ortho?"ortho-reference":"perspective-reference",on);
        const glm::dmat4 vp(frame.Camera.ViewProjection), inverse=glm::inverse(vp);
        size_t expected=0, found=0, missedWall=0, falseHit=0;
        const glm::dvec3 normal(0,1,0);
        for (unsigned y=0;y<on->Height;++y) for (unsigned x=0;x<on->Width;++x) {
            const size_t i=size_t(y)*on->Width+x;
            if (on->Entity[i]!=1) continue;
            const glm::dvec2 uv((x+.5)/on->Width,(y+.5)/on->Height);
            auto unproject=[&](double depth) {
                const auto p=inverse*glm::dvec4(uv*2.-1.,depth,1);
                return glm::dvec3(p)/p.w;
            };
            const auto near=unproject(-1), far=unproject(1);
            const auto direction=glm::normalize(far-near);
            const auto point=near-direction*(near.y/direction.y);
            const auto ray=glm::reflect(direction,normal);
            const double t=(-3-point.z)/ray.z;
            if (t<=0 || t>16 || ray.y<.12 || ray.z>-.15) continue;
            const auto hit=point+ray*t;
            const auto p=vp*glm::dvec4(hit,1);
            if (std::abs(p.x/p.w)>.78 || std::abs(p.y/p.w)>.78) continue;
            const bool reflected=on->Color[i].r-off->Color[i].r>.025f;
            if (std::abs(hit.x)<1.34 && hit.y>.15 && hit.y<2.24) {
                ++expected;
                found+=reflected;
            }
            if (std::abs(hit.x)>1.68 || hit.y>2.58) {
                ++missedWall;
                falseHit+=reflected;
            }
        }
        std::cout << "SSR_ORACLE ortho=" << ortho << " expected=" << expected << " found=" << found
                  << " missedWall=" << missedWall << " falseHit=" << falseHit << '\n';
        if (expected<100 || missedWall<100 || found<expected*.90 || falseHit>missedWall*.005)
            throw std::runtime_error("SSR disagrees with the FP64 wall intersection");
        auto shortRay=frame;
        shortRay.Presentation.ScreenReflections.Distance=.1f;
        const auto fallback=render(shortRay);
        coverage(fallback,off);
        if (fallback->Color!=off->Color)
            throw std::runtime_error("SSR distance limit did not retain exact fallback");
        if (ortho) {
            // Orthographic rays in this view have no X component. Extending
            // the wall across the viewport isolates screen-edge confidence.
            frame.Meshes.back()=box(2,wall,{0,1.2f,-3.02f},{20,2.4f,.04f});
            const auto edge=render(frame);
            size_t rows=0;
            double maxEdgeRatio=0;
            for (unsigned y=0;y<edge->Height;++y) {
                const size_t row=size_t(y)*edge->Width;
                const size_t left=row+6, mid=row+edge->Width/2, right=row+edge->Width-7;
                if (edge->Entity[left]!=1 || edge->Entity[mid]!=1 || edge->Entity[right]!=1 ||
                    edge->Color[mid].r<.15f) continue;
                ++rows;
                maxEdgeRatio=std::max(maxEdgeRatio,double(std::max(edge->Color[left].r,edge->Color[right].r)/edge->Color[mid].r));
            }
            std::cout << "SSR_EDGE rows=" << rows << " maxEdgeRatio=" << maxEdgeRatio << '\n';
            if (rows<12 || maxEdgeRatio>.2)
                throw std::runtime_error("SSR did not fade a visible reflection near the screen boundary");
        }
        if (metal && !ortho) {
            frame.Presentation.TemporalAA=true;
            for (unsigned sample=0;sample<8;++sample)
                static_cast<void>(render(frame));
            const auto before=render(frame);
            frame.Meshes.pop_back();
            const auto obscured=render(frame);
            double stale=0;
            size_t compared=0;
            for (size_t i=0;i<before->Color.size();++i)
                if (before->Entity[i]==1 && obscured->Entity[i]==1 && before->Color[i].r>.15f) {
                    stale=std::max(stale,double(obscured->Color[i].r));
                    ++compared;
                }
            std::cout << "SSR_DISOCCLUSION compared=" << compared << " maxStaleRed=" << stale << '\n';
            if (compared<100 || stale>.003)
                throw std::runtime_error("TAA retained a vanished reflection on a stationary receiver");
        }
    }
    target->Resize(640,360);
    SceneRenderView control, temporalView;
    size_t changed=0, preserved=0, probeSensitive=0;
    for (unsigned pose=0;pose<17;++pose) {
        auto frame=gallery((float(pose)/16-.5f)*9,{640,360});
        auto offFrame=frame;
        offFrame.Presentation.ScreenReflections.Enabled=false;
        std::swap(view,control);
        const auto off=render(offFrame);
        std::swap(view,control);
        const auto on=render(frame);
        coverage(on,off);
        save("off-"+std::to_string(pose),off);
        save("on-"+std::to_string(pose),on);
        if (pose==8) {
            frame.ReflectionProbes.clear();
            const auto global=render(frame);
            coverage(on,global);
            save("global-8",global);
            for (size_t i=0;i<on->Color.size();++i) {
                if (on->Entity[i]<0) continue;
                if (glm::length(glm::vec3(on->Color[i]-off->Color[i]))>.025f) ++changed;
                if (on->Color[i]==off->Color[i]) ++preserved;
                if (glm::length(glm::vec3(on->Color[i]-global->Color[i]))>.025f) ++probeSensitive;
            }
        }
        if (metal) {
            frame=gallery((float(pose)/16-.5f)*9,{640,360});
            frame.Presentation.TemporalAA=true;
            frame.Presentation.AmbientOcclusion.Enabled=true;
            std::swap(view,temporalView);
            if (!pose) static_cast<void>(render(frame));
            const auto temporal=render(frame);
            if (view.Temporal.Phase!=pose+2 || !view.TemporalMetrics->Valid || !view.AmbientTiming->Valid)
                throw std::runtime_error("SSR did not coexist with native AO/TAA");
            save("temporal-"+std::to_string(pose),temporal);
            std::swap(view,temporalView);
        }
    }
    if (changed<1200 || preserved<1000 || probeSensitive<1200)
        throw std::runtime_error("Gallery did not exercise both screen hits and retained local probes");
    std::cout << "SSR_COMPOSITE changed=" << changed << " preserved=" << preserved
              << " probeSensitive=" << probeSensitive << '\n';
#ifdef HZ_PLATFORM_MACOS
    if (metal) {
        auto& backend=static_cast<MetalRendererAPI&>(RenderCommand::Backend());
        target->Resize(320,180);
        std::array<Ref<const FramebufferReadback>,3> serial, queued;
        for (unsigned i=0;i<serial.size();++i)
            serial[i]=render(gallery(float(i)-1,{320,180}));
        auto unblock=backend.GateQueueForTesting();
        try {
            for (unsigned i=0;i<queued.size();++i) {
                draw(gallery(float(i)-1,{320,180}));
                target->Unbind();
                queued[i]=target->Submit();
                if (!queued[i] || queued[i]->Completed)
                    throw std::runtime_error("SSR lifetime did not cover a gated submission");
            }
            view.Reset();
            target->Resize(257,149);
            if (backend.Observe()->Snapshot().InFlight!=queued.size())
                throw std::runtime_error("SSR lifetime did not use all three queue slots");
        } catch (...) {
            unblock();
            throw;
        }
        unblock();
        for (unsigned i=0;i<queued.size();++i) {
            for (unsigned tries=0;!queued[i]->Completed && tries<2500;++tries) {
                target->Poll();
                if (!queued[i]->Completed) std::this_thread::sleep_for(std::chrono::milliseconds(2));
            }
            if (!queued[i]->Completed || !queued[i]->Error.empty())
                throw std::runtime_error("Queued SSR readback failed to complete");
            coverage(queued[i],serial[i]);
            if (queued[i]->Color!=serial[i]->Color)
                throw std::runtime_error("Queued SSR sampled a different framebuffer generation");
        }
        std::cout << "SSR_LIFETIME queue=3 reset=pass resize=pass\n";
    }
#endif
    std::cout << "SSR_GALLERY poses=17 ID/depth=pass deterministic=pass resize=pass"
              << (metal?" AO/TAA=pass":"") << '\n';
}
