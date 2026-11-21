#pragma once
#include "Hazel/Assets/ProjectAssets.h"
#include "Hazel/Core/FileSystem.h"
#include "Hazel/Renderer/Scene/SceneRenderer.h"
#include "Hazel/Renderer/Drawing/Renderer3D.h"
#include "Hazel/Scene/RuntimeSession.h"
#include "Hazel/Project/Project.h"
#include "Hazel/Scene/Entity.h"
#include "../migration/DirectBRDFReference.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/constants.hpp>
#include <filesystem>
#include <fstream>
#include <functional>
#include <string>
#include <limits>
#include <stdexcept>

// Both native backends render the same saved production scenes, accepted asset
// generations and authored camera. The caller owns only completion/readback.
inline void Renderer3DParity(const std::filesystem::path& output,
    const std::function<Hazel::Ref<const Hazel::FramebufferReadback>(const Hazel::Ref<Hazel::Framebuffer>&)>& capture)
{
    using namespace Hazel;
    std::filesystem::create_directories(output);
    const auto selected=FileSystem::GetEnvironmentPath("HAZEL_RENDERING_REFERENCE_PROJECT");
    const auto projectRoot=selected.empty()?std::filesystem::absolute("examples/RenderingReference"):selected.parent_path();
    const bool metal = RendererAPI::GetAPI() == RendererAPI::API::Metal;
    FramebufferSpecification spec;
    spec.Width = 320; spec.Height = 180;
    spec.Attachments = {FramebufferTextureFormat::RGBA32F, FramebufferTextureFormat::RED_INTEGER,
        metal ? FramebufferTextureFormat::DEPTH32F : FramebufferTextureFormat::Depth};
    Ref<Framebuffer> display;
    const auto save = [&](const std::string& name, const Ref<const FramebufferReadback>& result) {
        if (!result || !result->Completed || !result->Error.empty()) throw std::runtime_error("Incomplete 3D capture");
        std::ofstream file(output / (name + ".bin"), std::ios::binary);
        for (size_t i = 0; i < result->Color.size(); ++i) {
            file.write(reinterpret_cast<const char*>(&result->Color[i]), 16);
            file.write(reinterpret_cast<const char*>(&result->Entity[i]), 4);
            file.write(reinterpret_cast<const char*>(&result->Depth[i]), 4);
        }
        if (!file) throw std::runtime_error("Cannot save 3D parity capture");
    };
    // Independent fixed-camera cube tests use the same production factories,
    // SceneRenderer and direct FP64 reference as the accepted OpenGL suite.
    {
        auto project = Project::LoadCandidate((projectRoot/"RenderingReference-Direct-PerspectivePBR.hproj"));
        auto assets = CreateRef<ProjectAssets>(project->GetAssetRoot());
        auto mesh = assets->Mesh("Native/Direct_v1/UnitCube.hmesh");
        if (!mesh.Accepted) throw std::runtime_error(mesh.Error);
        display = Framebuffer::Create(spec);
        SceneSubmission frame;
        frame.Assets = assets; frame.Presentation.SceneReferred = true;
        frame.Debug = SurfaceView::Direct; frame.LinearLightingInspection = true;
        frame.Camera.Orthographic = true; frame.Camera.Position = {0,0,3}; frame.Camera.ToCamera = {0,0,1};
        frame.Camera.ViewProjection = glm::ortho(-1.f,1.f,-1.f,1.f,.1f,10.f)*
            glm::lookAt(glm::vec3(0,0,3),glm::vec3(0),glm::vec3(0,1,0));
        const auto light = glm::normalize(glm::vec3(.3f,.4f,1));
        frame.Light = SubmittedDirectional{light,glm::vec3(2)};
        auto material = CreateRef<MaterialDefinition>(); material->BaseColor = {.18f,.4f,.7f,1};
        SubmittedMesh draw; draw.Geometry = mesh.Accepted; draw.Materials = {material}; draw.Picking = -2000000003;
        frame.Meshes = {draw};
        SceneRenderView view;
        const auto render = [&] {
            display->Bind();
            if (metal) display->Clear({0,0,0,1});
            else { RenderCommand::SetClearColor({0,0,0,1}); RenderCommand::Clear(); }
            display->ClearAttachment(1,-1);
            SceneRenderer::Render(frame,view,display,[]{},[]{});
            return capture(display);
        };
        for (float metallic : {0.f,.5f,1.f}) for (float roughness : {0.f,.1f,.25f,.5f,.75f,1.f}) {
            material->Metallic=metallic;material->Roughness=roughness;
            auto result=render(); const size_t pixel=size_t(spec.Height/2)*spec.Width+spec.Width/2;
            const auto expected=OracleBRDF(glm::dvec3(material->BaseColor),metallic,roughness,{0,0,1},{0,0,1},glm::dvec3(light))*2.;
            for (int c=0;c<3;++c) if (!std::isfinite(result->Color[pixel][c]) || std::abs(result->Color[pixel][c]-expected[c])>.002)
                throw std::runtime_error("Native direct FP64 oracle failed");
            if (result->Entity[pixel]!=draw.Picking || std::abs(result->Depth[pixel]-(2.5-.1)/9.9)>1e-6)
                throw std::runtime_error("Native cube signed ID/analytic depth failed");
        }
        material->Model=MaterialModel::Unlit; frame.Debug=SurfaceView::BaseColor; frame.LinearLightingInspection=false;
        for (const auto scale : {glm::vec3(1),glm::vec3(-1,1,1),glm::vec3(1,.5f,2)}) {
            frame.Meshes[0].SetWorld(glm::scale(glm::mat4(1),scale));
            auto result=render();const size_t pixel=size_t(spec.Height/2)*spec.Width+spec.Width/2;
            if(result->Entity[pixel]!=draw.Picking)throw std::runtime_error("Mirrored/nonuniform cube missing");
            for(int c=0;c<3;++c)if(std::abs(result->Color[pixel][c]-material->BaseColor[c])>1e-6)
                throw std::runtime_error("Immutable unlit material/winding failed");
        }
        for (float z : {4.f,-11.f}) {
            frame.Meshes[0].SetWorld(glm::translate(glm::mat4(1),{0,0,z}));auto result=render();
            for(auto id:result->Entity)if(id!=-1)throw std::runtime_error("Near/far clipping failed");
        }
        frame.Meshes[0].SetWorld(glm::mat4(1)); material->Alpha=MaterialAlpha::Mask;material->BaseColor.a=.1f;
        auto masked=render();for(auto id:masked->Entity)if(id!=-1)throw std::runtime_error("Masked geometry wrote IDs");
        material->BaseColor.a=1;material->Alpha=MaterialAlpha::Opaque;material->Model=MaterialModel::MetallicRoughness;
        frame.Debug=SurfaceView::Direct;frame.LinearLightingInspection=true;
        material->Metallic=1;material->Roughness=0;frame.Light=SubmittedDirectional{{0,0,1},glm::vec3(1000)};
        auto hdr=render();const size_t pixel=size_t(spec.Height/2)*spec.Width+spec.Width/2;
        if(!std::isfinite(hdr->Color[pixel].b)||hdr->Color[pixel].b<=65504)throw std::runtime_error("Native FP32 HDR highlight lost");
        save("independent-hdr",hdr);
        auto environment = CreateRef<PreparedEnvironment>(ReadPreparedEnvironment(project->GetAssetRoot(),
            "Reference/IBL_v1/Constant.henv"));
        for(auto& mip:environment->Background.Mips) {
            const size_t facePixels=mip.size()/18;
            for(size_t face=0;face<6;++face)for(size_t p=0;p<facePixels;++p)for(size_t c=0;c<3;++c)
                mip[(face*facePixels+p)*3+c]=.125f+.03125f*face+.00390625f*c;
        }
        auto& levels=environment->Specular;
        levels.Side=32;levels.EncodingVersion=2;levels.Mips.clear();
        for(unsigned side=32,level=0;side>=4;side/=2,++level) {
            auto& pixels=levels.Mips.emplace_back(size_t(side)*side*18);
            for(unsigned face=0;face<6;++face)for(size_t pixel=0;pixel<size_t(side)*side;++pixel)
                for(unsigned c=0;c<3;++c)pixels[(size_t(face)*side*side+pixel)*3+c]=.25f*(level+1)+.03125f*face+.00390625f*c;
        }
        for(auto& mip:environment->Irradiance.Mips)for(auto& value:mip)value=float(glm::pi<double>()*.25);
        frame.Environment=environment;frame.Presentation.Environment.Lighting=true;
        frame.Debug=SurfaceView::PrefilteredIBL;frame.Light.reset();
        for(unsigned face=0;face<6;++face) {
            const glm::vec3 directions[]={{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}};
            const auto d=directions[face];const auto up=std::abs(d.y)>.9f?glm::vec3(0,0,1):glm::vec3(0,1,0);
            frame.Camera.Position=d*3.f;frame.Camera.ToCamera=d;
            frame.Camera.ViewProjection=glm::ortho(-1.f,1.f,-1.f,1.f,.1f,10.f)*glm::lookAt(d*3.f,glm::vec3(0),up);
            for(float lod:{.135f,.25f,1.f,1.5f,2.f}) {
                material->Roughness=lod/3;
                auto result=render();
                for(unsigned c=0;c<3;++c) {
                    const float expected=.25f*(std::max(.045f,lod/3)*3+1)+.03125f*face+.00390625f*c;
                    if(std::abs(result->Color[pixel][c]-expected)>.0001f)throw std::runtime_error("Prepared face/fractional LOD oracle failed");
                }
            }
            material->Roughness=1;auto tail=render();
            for(unsigned c=0;c<3;++c)if(std::abs(tail->Color[pixel][c]-.25f)>.00001f)
                throw std::runtime_error("Roughness-1 irradiance reconstruction failed");
        }

        frame.Camera.Position={0,0,3};frame.Camera.ToCamera={0,0,1};
        frame.Camera.ViewProjection=glm::ortho(-1.f,1.f,-1.f,1.f,.1f,10.f)*glm::lookAt(glm::vec3(0,0,3),glm::vec3(0),glm::vec3(0,1,0));
        frame.Debug=SurfaceView::PrefilteredIBL;material->Roughness=.5f;
        const glm::vec3 seams[]={{1,0,1},{-1,0,1},{0,1,1},{0,-1,1}};
        const float faceMean[]={2,2.5f,3,3.5f};
        for(size_t i=0;i<4;++i) {
            const auto r=glm::normalize(seams[i]);frame.Camera.ToCamera={-r.x,-r.y,r.z};
            auto seam=render();
            for(unsigned c=0;c<3;++c)if(std::abs(seam->Color[pixel][c]-(.625f+.03125f*faceMean[i]+.00390625f*c))>.0001f)
                throw std::runtime_error("Independent cube edge filtering oracle failed");
        }
        // GL 4.1 section 3.8.10 permits implementation-dependent fourth texels
        // at corners; only equal-value preservation is mandatory. A distinct
        // synthetic generation tests that contract without fitting driver weights.
        auto corner=CreateRef<PreparedEnvironment>(*environment);
        corner->Generation=environment->Generation==std::numeric_limits<uint64_t>::max()?1:environment->Generation+1;
        for(size_t level=0;level<corner->Specular.Mips.size();++level) {
            auto& pixels=corner->Specular.Mips[level];
            for(size_t p=0;p<pixels.size()/3;++p)for(size_t c=0;c<3;++c)
                pixels[p*3+c]=.25f*float(level+1)+.00390625f*float(c);
        }
        frame.Environment=corner;
        for(const auto direction : {glm::vec3(1,1,1),glm::vec3(1,-1,1),glm::vec3(-1,1,1),glm::vec3(-1,-1,1)}) {
            const auto r=glm::normalize(direction);frame.Camera.ToCamera={-r.x,-r.y,r.z};
            auto result=render();
            for(unsigned c=0;c<3;++c)if(std::abs(result->Color[pixel][c]-(.625f+.00390625f*c))>.0001f)
                throw std::runtime_error("Independent cube corner constant-preservation oracle failed");
        }
        frame.Environment=environment;
        frame.Camera.ToCamera={0,0,1};
        frame.Debug=SurfaceView::DiffuseIBL;material->Metallic=0;
        auto diffuse=render();
        for(unsigned c=0;c<3;++c)if(std::abs(diffuse->Color[pixel][c]-.25f*.96f*material->BaseColor[c])>.00001f)
            throw std::runtime_error("Constant irradiance/Fview diffuse oracle failed");
        material->Metallic=1;auto conductor=render();
        for(unsigned c=0;c<3;++c)if(std::abs(conductor->Color[pixel][c])>.000001f)
            throw std::runtime_error("Conductor diffuse must be zero");
        frame.Debug=SurfaceView::PrefilteredIBL;material->Roughness=.5f;
        frame.Presentation.Environment.Yaw=glm::half_pi<float>();
        auto yaw=render();
        for(unsigned c=0;c<3;++c)if(std::abs(yaw->Color[pixel][c]-(.25f*2.5f+.03125f+.00390625f*c))>.0001f)
            throw std::runtime_error("Inverse environment yaw orientation failed");

        frame.Debug=SurfaceView::DiffuseIBL;material->Metallic=0;frame.Presentation.Environment.Gain=2;
        auto gain=render();
        for(unsigned c=0;c<3;++c)if(std::abs(gain->Color[pixel][c]-.5f*.96f*material->BaseColor[c])>.00001f)
            throw std::runtime_error("Environment linear gain failed");
        frame.Environment.reset();frame.Debug=SurfaceView::Shaded;frame.LinearLightingInspection=false;
        material->Model=MaterialModel::Unlit;
        spec.Attachments.Attachments[0].TextureFormat=metal?FramebufferTextureFormat::RGBA32F:FramebufferTextureFormat::RGBA16F;
        display=Framebuffer::Create(spec);
        for(float ev:{-2.f,0.f,2.f}) {
            frame.Presentation.Exposure=ev;auto result=render();
            const auto expected=OracleNeutral(glm::dvec3(material->BaseColor)*double(std::exp2(ev)));
            for(unsigned c=0;c<3;++c)if(std::abs(result->Color[pixel][c]-expected[c])>.001)
                throw std::runtime_error("Mesh exposure/Neutral independent oracle failed");
            if(result->Entity[pixel]!=draw.Picking)throw std::runtime_error("Mesh presentation changed signed IDs");
        }
        frame.Meshes.clear();frame.Environment=environment;frame.Presentation.Exposure=0;frame.Presentation.Environment.Gain=1;
        for(const auto yawFace : {glm::vec2(0,5),glm::vec2(glm::half_pi<float>(),0)}) {
            frame.Presentation.Environment.Yaw=yawFace.x;auto sky=render();
            glm::dvec3 radiance;
            for(unsigned c=0;c<3;++c)radiance[c]=.125+.03125*yawFace.y+.00390625*c;
            const auto expected=OracleNeutral(radiance);
            for(unsigned c=0;c<3;++c)if(std::abs(sky->Color[pixel][c]-expected[c])>.001)
                throw std::runtime_error("Independent sky orientation/yaw oracle failed");
            if(sky->Entity[pixel]!=-1 || sky->Depth[pixel]!=1)throw std::runtime_error("Sky wrote depth or IDs");
        }
        spec.Attachments.Attachments[0].TextureFormat=FramebufferTextureFormat::RGBA32F;

    }
    for (const std::string name : {"Direct-PerspectiveUnlit", "Direct-PerspectivePBR", "Direct-OrthographicPBR",
        "Direct-SurfaceConventions", "IBL-DirectOnly", "IBL-IBLOnly", "IBL-Combined", "IBL-Photo", "IBL-AxesYaw90", "IBLQuality-Constant", "IBLQuality-TinyEmitter", "IBLQuality-Photo"})
    {
        const auto path = projectRoot/("RenderingReference-" + name + ".hproj");
        if (!std::filesystem::exists(path)) throw std::runtime_error("Missing production 3D fixture: " + path.string());
        auto project = Project::LoadCandidate(path);
        auto requests = project->GetConfig().Rendering.value_or(RuntimeRendererRequests{});
        requests.TextureSlots = RenderCommand::GetSettings().TextureSlots; // Private candidate, identical four-slot policy.
        project->GetConfig().Rendering = requests;
        RuntimeSession session;
        session.Start(project, project->LoadScene(project->GetConfig().StartScene));
        session.Resize(spec.Width, spec.Height); session.ResizeFramebuffer(spec.Width, spec.Height);
        spec.Attachments.Attachments[0].TextureFormat = !metal && session.GetScene()->Presentation().SceneReferred ?
            FramebufferTextureFormat::RGBA16F : FramebufferTextureFormat::RGBA32F;
        display = Framebuffer::Create(spec);
        session.SetRenderTarget(display); session.SetInput({-1,-1}, false);
        SceneRenderView view;
        display->Bind();
        if (metal) display->Clear(session.GetScene()->Presentation().Background);
        else { RenderCommand::SetClearColor(session.GetScene()->Presentation().Background); RenderCommand::Clear(); }
        display->ClearAttachment(1, -1);
        session.GetScene()->OnUpdateRuntime(Timestep(0), &view, display);
        if (!session.GetScene()->RenderError().empty()) throw std::runtime_error(session.GetScene()->RenderError());
        save(name, capture(display));
        session.SetRenderTarget({});
        const auto scene = session.GetScene();
        auto cameraEntity = scene->GetPrimaryCameraEntity();
        if (!cameraEntity) throw std::runtime_error("3D fixture has no primary camera");
        const auto& camera = cameraEntity.GetComponent<CameraComponent>().Camera;
        const auto world = scene->GetWorldTransform(cameraEntity);
        const SceneCameraView cameraView{camera.GetProjection()*glm::inverse(world), glm::vec3(world[3]),
            glm::normalize(glm::vec3(world*glm::vec4(0,0,1,0))),
            camera.GetProjectionType()==SceneCamera::ProjectionType::Orthographic};
        auto frame = SceneRenderer::Collect(*scene, cameraView);
        frame.LinearLightingInspection = true;
        spec.Attachments.Attachments[0].TextureFormat = FramebufferTextureFormat::RGBA32F;
        display = Framebuffer::Create(spec);
        for (unsigned mode = unsigned(SurfaceView::Direct); mode <= unsigned(SurfaceView::NormalHorizon); ++mode) {
            frame.Debug = SurfaceView(mode);
            display->Bind();
            if (metal) display->Clear({0,0,0,1});
            else { RenderCommand::SetClearColor({0,0,0,1}); RenderCommand::Clear(); }
            display->ClearAttachment(1,-1);
            SceneRenderer::Render(frame, view, display, [] {}, [] {});
            save(name+"-linear-"+std::to_string(mode), capture(display));
        }
        if (name == "IBLQuality-Photo") for (const auto extent : {glm::uvec2(1280,720),glm::uvec2(1920,1080)}) {
            spec.Width=extent.x;spec.Height=extent.y;
            spec.Attachments.Attachments[0].TextureFormat=metal?FramebufferTextureFormat::RGBA32F:FramebufferTextureFormat::RGBA16F;
            display=Framebuffer::Create(spec);
            session.Resize(extent.x,extent.y);session.ResizeFramebuffer(extent.x,extent.y);session.SetRenderTarget(display);
            display->Bind();
            if(metal)display->Clear(scene->Presentation().Background);
            else {RenderCommand::SetClearColor(scene->Presentation().Background);RenderCommand::Clear();}
            display->ClearAttachment(1,-1);scene->OnUpdateRuntime(Timestep(0),&view,display);
            if(!scene->RenderError().empty())throw std::runtime_error(scene->RenderError());
            save(name+"-"+std::to_string(extent.x)+"x"+std::to_string(extent.y),capture(display));
            session.SetRenderTarget({});
        }
        session.Stop();
    }
    Renderer3D::Shutdown();
}
