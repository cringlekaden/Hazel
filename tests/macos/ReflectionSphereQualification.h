#pragma once

#include "Renderer3DParity.h"
#include "Hazel/Renderer/RenderCommand.h"
#include "Hazel/Core/FileDocument.h"
#include "Hazel/Renderer/Cameras/NavigationCamera.h"
#include "Hazel/Renderer/Resources/EnvironmentResources.h"
#include "Hazel/Scene/SceneSerializer.h"
#include "Hazel/Renderer/Scene/ReflectionProbeCapture.h"
#include "Hazel/Scene/Scene.h"
#include <chrono>
#include <iomanip>
#include <thread>

namespace ReflectionSphereStudy
{
using namespace Hazel;
constexpr unsigned Width=384, Height=256, Poses=9, CubeSide=128;
constexpr glm::vec3 OrangeCenter{-1.68f,1.56f,1.62f};
constexpr float OrangeRadius=.65f;
constexpr glm::vec3 OffsetCapture{.35f,-.22f,.31f};
constexpr glm::vec3 ProxyHalfSize{3.f,2.8f,3.f};
constexpr const char* SphereMesh="Reference/Direct_v1/Sphere.hmesh";
constexpr const char* OrangeMaterial="ReflectionStudy/Materials/Warm panel 2.hmat";
constexpr const char* ChromeMaterial="ReflectionStudy/Materials/Chrome.hmat";

inline Ref<const FramebufferReadback> Render(const std::filesystem::path& output, const std::string& name,
    const SceneSubmission& frame, SceneRenderView& view, const Ref<Framebuffer>& target,
    const std::function<Ref<const FramebufferReadback>(const Ref<Framebuffer>&)>& capture)
{
    const bool metal=RendererAPI::GetAPI()==RendererAPI::API::Metal;
    target->Bind();
    if (metal) target->Clear({0,0,0,1});
    else {
        RenderCommand::SetClearColor({0,0,0,1});
        RenderCommand::Clear();
    }
    target->ClearAttachment(1,-1);
    SceneRenderer::Render(frame,view,target,[]{},[]{});
    auto result=capture(target);
    if (!result || !result->Completed || !result->Error.empty())
        throw std::runtime_error("Incomplete mirror-sphere readback");
    if (!name.empty()) {
        std::ofstream file(output/(name+".bin"),std::ios::binary);
        for (size_t i=0;i<result->Color.size();++i) {
            const auto color=result->Color[i];
            if (!std::isfinite(color.r) || !std::isfinite(color.g) || !std::isfinite(color.b))
                throw std::runtime_error("Nonfinite mirror-sphere readback");
            file.write(reinterpret_cast<const char*>(&color),16);
            file.write(reinterpret_cast<const char*>(&result->Entity[i]),4);
            file.write(reinterpret_cast<const char*>(&result->Depth[i]),4);
        }
        if (!file) throw std::runtime_error("Could not write mirror-sphere readback");
    }
    return result;
}

inline void RunWorkspace(const std::filesystem::path& output, const Ref<ProjectAssets>& assets,
    const std::filesystem::path& pose,
    const std::function<Ref<const FramebufferReadback>(const Ref<Framebuffer>&)>& capture)
{
    const auto saved=YAML::Load(FileDocument::Read(pose));
    auto scene=CreateRef<Scene>();
    SceneSerializer reader(scene,assets->Root(),false,assets);
    if (!reader.DeserializeText(FileDocument::Read(Project::ResolveOwnedAsset(
            assets->Root(),saved["Scene"].as<std::string>())),false))
        throw std::runtime_error(reader.Report().Error);
    if (saved["Navigation"].as<std::string>()!="Orbit")
        throw std::runtime_error("Sphere screenshot comparison expects the editor orbit camera");
    FramebufferSpecification spec;
    spec.Width=2022;
    spec.Height=748;
    const bool metal=RendererAPI::GetAPI()==RendererAPI::API::Metal;
    spec.Attachments={FramebufferTextureFormat::RGBA32F,FramebufferTextureFormat::RED_INTEGER,
        metal?FramebufferTextureFormat::DEPTH32F:FramebufferTextureFormat::Depth};
    const auto target=Framebuffer::Create(spec);
    SceneRenderView view;
    NavigationCamera navigation(30,1.778f,.1f,1000);
    navigation.SetViewportSize(float(spec.Width),float(spec.Height));
    const auto f=saved["Focus"];
    navigation.RestoreOrbit({f[0].as<float>(),f[1].as<float>(),f[2].as<float>()},
        saved["Pitch"].as<float>(),saved["Yaw"].as<float>(),saved["Distance"].as<float>());
    const SceneCameraView camera{navigation.GetViewProjection(),navigation.GetPosition(),
        -navigation.GetForwardDirection(),false,navigation.GetViewMatrix(),navigation.GetProjection()};
    for (const auto handle : scene->GetAllEntitiesWith<ReflectionProbeComponent>())
        Entity(handle,scene.get()).GetComponent<ReflectionProbeComponent>().Enabled=true;
    const auto authored=SceneRenderer::Collect(*scene,camera,{spec.Width,spec.Height});
    if (authored.ReflectionProbes.empty())
        throw std::runtime_error("Workspace comparison requires an accepted baked probe");
    auto frame=authored;
    frame.Presentation.TemporalAA=false;
    frame.Presentation.AmbientOcclusion.Enabled=false;
    frame.Presentation.ScreenReflections.Enabled=false;
    frame.ReflectionProbes.clear();
    const auto globalOnly=frame;
    auto global=Render(output,"global",frame,view,target,capture);
    auto save=[&](const char* name) {
        const auto image=Render(output,name,frame,view,target,capture);
        if (image->Entity!=global->Entity || image->Depth!=global->Depth)
            throw std::runtime_error("SSR workspace variants changed raster coverage");
        return image;
    };
    frame.Presentation.ScreenReflections.Enabled=true;
    save("ssr");
    frame.ReflectionProbes=authored.ReflectionProbes;
    save("mixed");
    frame.Presentation.ScreenReflections.Enabled=false;
    save("local");
    frame.Debug=SurfaceView::WorldPosition;
    frame.LinearLightingInspection=true;
    const auto positions=save("position");
    frame.Debug=SurfaceView::Normal;
    frame.LinearLightingInspection=false;
    const auto normals=save("normal");
    frame.Debug=SurfaceView::Shaded;
    std::ofstream cameraData(output/"eye.csv");
    auto vector=[](std::ostream& stream,glm::vec3 value) {
        stream << ',' << value.x << ',' << value.y << ',' << value.z;
    };
    cameraData << std::setprecision(10) << "eye";
    vector(cameraData,camera.Position);
    cameraData << '\n';
    std::ofstream objects(output/"geometry.csv");
    objects << std::setprecision(10);
    for (const auto handle : scene->GetAllEntitiesWith<MeshRendererComponent>()) {
        Entity entity(handle,scene.get());
        const auto& renderer=entity.GetComponent<MeshRendererComponent>();
        const auto mesh=assets->Mesh(renderer.Mesh);
        if (!mesh.Accepted)
            throw std::runtime_error(mesh.Error);
        const auto world=scene->GetWorldTransform(entity);
        const auto& bounds=mesh.Accepted->Geometry.Bounds;
        const bool sphere=renderer.Mesh==SphereMesh;
        if (!sphere && renderer.Mesh!="Native/Direct_v1/UnitCube.hmesh")
            throw std::runtime_error("Workspace geometry oracle expects the Reflection Study cubes and spheres");
        for (int i=0;i<3;++i)
            for (int j=0;j<3;++j)
                if (i!=j && std::abs(world[i][j])>1e-6f)
                    throw std::runtime_error("Workspace geometry oracle expects axis-aligned transforms");
        objects << uint64_t(entity.GetUUID()) << ',' << int(handle) << ',' << (sphere?"sphere":"box");
        vector(objects,glm::vec3(world*glm::vec4(bounds.Min,1)));
        vector(objects,glm::vec3(world*glm::vec4(bounds.Max,1)));
        objects << '\n';
    }
    std::ofstream probes(output/"probes.csv");
    probes << std::setprecision(10);
    for (const auto& probe : authored.ReflectionProbes) {
        probes << probe.Identity;
        vector(probes,probe.Position);
        vector(probes,probe.InfluenceHalfSize);
        vector(probes,probe.ProjectionOffset);
        vector(probes,probe.ProjectionHalfSize);
        probes << ',' << probe.BlendDistance << '\n';
    }
    for (auto& probe : frame.ReflectionProbes)
        probe.ProjectionHalfSize=glm::vec3(10000);
    save("unprojected");
    frame=globalOnly;
    frame.Environment.reset();
    save("direct-only");
    frame=globalOnly;
    frame.Light.reset();
    frame.LocalLights.clear();
    save("ibl-only");
    frame.Debug=SurfaceView::SpecularIBL;
    frame.LinearLightingInspection=true;
    const auto ibl=save("ibl-linear");
    if (!frame.Environment)
        throw std::runtime_error("Workspace halo control requires its authored Constant environment");
    const auto found=std::find_if(frame.Meshes.begin(),frame.Meshes.end(),
        [](const SubmittedMesh& draw) {return draw.Identity==106;});
    if (found==frame.Meshes.end() || found->Materials.empty())
        throw std::runtime_error("Workspace halo control requires the warm polished sphere");
    const auto& material=*found->Materials.front();
    if (material.Metallic!=1 || material.Roughness>.045f)
        throw std::runtime_error("Workspace halo control expects the original polished material");

    // Evaluate the established split sum independently in FP64 for the
    // authored uniform environment. No probe, SSR, or point light contributes.
    const auto table=ReadRuntimeBRDF();
    const auto mean=ReferenceAverageRuntimeBRDF(table,.045);
    const auto& specular=frame.Environment->Specular.Mips.front();
    const auto& irr=frame.Environment->Irradiance.Mips.front();
    const glm::dvec3 radiance(specular[0],specular[1],specular[2]);
    const glm::dvec3 irradiance=glm::dvec3(irr[0],irr[1],irr[2])/glm::pi<double>();
    const glm::dvec3 f0(material.BaseColor);
    double maximum=0, rimBlueRed=0, faceBlueRed=0;
    unsigned tested=0, rim=0, face=0;
    for (size_t i=0;i<ibl->Color.size();++i) {
        if (ibl->Entity[i]!=found->Picking)
            continue;
        const auto n=glm::normalize(2.*glm::dvec3(normals->Color[i])-1.);
        const auto v=glm::normalize(glm::dvec3(camera.Position)-glm::dvec3(positions->Color[i]));
        const auto mu=glm::dot(n,v);
        if (mu<.035 || glm::length(glm::vec3(ibl->Color[i]))<.001f)
            continue;
        const auto ab=ReferenceSampleRuntimeBRDF(table,mu,.045);
        const auto energy=ReferenceMultiscatteringEnergy(f0,ab,mean);
        const auto expected=(radiance*(f0*ab.x+ab.y)+irradiance*energy.C*std::max(0.,1-ab.x-ab.y))*
            double(frame.Presentation.Environment.Gain);
        const auto delta=glm::abs(glm::dvec3(ibl->Color[i])-expected);
        maximum=std::max({maximum,delta.x,delta.y,delta.z});
        const auto ratio=ibl->Color[i].b/ibl->Color[i].r;
        if (mu<.12) {rimBlueRed+=ratio;++rim;}
        if (mu>.92) {faceBlueRed+=ratio;++face;}
        ++tested;
    }
    std::ofstream halo(output/"halo.csv");
    if (!rim || !face || tested<500 || maximum>.002)
        throw std::runtime_error("Uniform IBL grazing control did not match the established FP64 reference");
    halo << "samples,maximumError,rimSamples,rimBlueRed,faceSamples,faceBlueRed\n"
         << tested << ',' << maximum << ',' << rim << ',' << rimBlueRed/rim << ',' << face << ','
         << faceBlueRed/face << '\n';
}

inline void Run(const std::filesystem::path& output, const Ref<ProjectAssets>& assets,
    const std::function<Ref<const FramebufferReadback>(const Ref<Framebuffer>&)>& capture, bool bake)
{
    const bool metal=RendererAPI::GetAPI()==RendererAPI::API::Metal;
    const auto mesh=assets->Mesh(SphereMesh);
    if (!mesh.Accepted)
        throw std::runtime_error(mesh.Error);
    // The accepted diagnostic mesh is centered at zero. Match its actual
    // extent to the physical radii used by the independent ideal spheres.
    const auto bounds=mesh.Accepted->Geometry.Bounds;
    if (bounds.Max.x<=0 || glm::length(bounds.Max+bounds.Min)>.02f ||
        glm::length(bounds.Max-glm::vec3(bounds.Max.x))>.02f)
        throw std::runtime_error("Unexpected diagnostic sphere extent");
    auto scene=CreateRef<Scene>();
    scene->SetAssets(assets);
    scene->Presentation().SceneReferred=true;
    scene->Presentation().Background={0,0,0,1};
    auto add=[&](uint64_t id,const char* name,glm::vec3 position,float radius,const char* surface) {
        auto entity=scene->CreateEntityWithUUID(id,name);
        auto& transform=entity.GetComponent<TransformComponent>();
        transform.Translation=position;
        transform.Scale=glm::vec3(radius/bounds.Max.x);
        auto& renderer=entity.AddComponent<MeshRendererComponent>();
        renderer.Mesh=SphereMesh;
        renderer.Materials[14]=surface;
    };
    add(1,"Chrome receiver",glm::vec3(0),1,ChromeMaterial);
    add(2,"Orange emitter",OrangeCenter,OrangeRadius,OrangeMaterial);
    if (bake) {
        for (unsigned index=0;index<2;++index) {
            ReflectionProbeCapture job(scene,index?OffsetCapture:glm::vec3(0),CubeSide);
            while (!job.Advance())
                if (metal) std::this_thread::sleep_for(std::chrono::milliseconds(1));
            FileSystem::WriteNewFile(output/(index?"offset.hlight":"center.hlight"),EncodeLightingImage(job.Image()));
        }
        return;
    }

    auto centered=assets->Environment("SphereStudy/center.henv");
    auto offset=assets->Environment("SphereStudy/offset.henv");
    if (!centered.Accepted || !offset.Accepted)
        throw std::runtime_error("Sphere study requires prepared captures");
    FramebufferSpecification specification;
    specification.Width=Width;
    specification.Height=Height;
    specification.Attachments={FramebufferTextureFormat::RGBA32F,FramebufferTextureFormat::RED_INTEGER,
        metal?FramebufferTextureFormat::DEPTH32F:FramebufferTextureFormat::Depth};
    auto target=Framebuffer::Create(specification);
    SceneRenderView view, temporalView;
    auto render=[&](const std::string& name,const SceneSubmission& frame,SceneRenderView& state) {
        return Render(output,name,frame,state,target,capture);
    };
    const SubmittedReflectionProbe local{1,glm::vec3(0),glm::vec3(1.65f),glm::vec3(0),
        ProxyHalfSize,.25f,centered.Accepted};
    std::ofstream cameraData(output/"cameras.csv");
    cameraData << std::setprecision(10);
    for (unsigned i=0;i<Poses;++i) {
        SceneCameraView camera;
        camera.Position={.16f*(float(i)-4),.16f,5.4f};
        camera.View=glm::lookAt(camera.Position,glm::vec3(-.12,.26,0),glm::vec3(0,1,0));
        camera.ToCamera=glm::vec3(glm::inverse(camera.View)[2]);
        camera.Projection=glm::perspective(glm::radians(47.f),float(Width)/Height,.1f,40.f);
        camera.ViewProjection=camera.Projection*camera.View;
        cameraData << i << ',' << camera.Position.x << ',' << camera.Position.y << ',' << camera.Position.z;
        for (int column=0;column<4;++column)
            for (int row=0;row<4;++row) cameraData << ',' << camera.ViewProjection[column][row];
        cameraData << '\n';
        auto frame=SceneRenderer::Collect(*scene,camera,{Width,Height});
        if (frame.Meshes.size()!=2)
            throw std::runtime_error("Expected the chrome and orange sphere in the camera frustum");
        frame.Presentation.ScreenReflections.Steps=64;
        const auto suffix="-"+std::to_string(i);
        const auto baseline=render("black"+suffix,frame,view);
        const auto save=[&](const char* name,const SceneSubmission& state) {
            const auto current=render(name+suffix,state,view);
            if (current->Entity!=baseline->Entity || current->Depth!=baseline->Depth)
                throw std::runtime_error("Sphere study changed raster coverage");
        };
        frame.Debug=SurfaceView::WorldPosition;
        frame.LinearLightingInspection=true;
        save("position",frame);
        frame.Debug=SurfaceView::Normal;
        frame.LinearLightingInspection=false;
        save("normal",frame);
        frame.Debug=SurfaceView::Shaded;
        frame.Presentation.ScreenReflections.Enabled=true;
        save("ssr",frame);
        frame.ReflectionProbes={local};
        save("mixed",frame);
        frame.Presentation.ScreenReflections.Enabled=false;
        save("box",frame);
        frame.ReflectionProbes[0].ProjectionHalfSize=glm::vec3(10000);
        save("unprojected",frame);
        frame.Presentation.ScreenReflections.Enabled=true;
        save("mixed-unprojected",frame);
        frame.Presentation.ScreenReflections.Enabled=false;
        frame.ReflectionProbes={local};
        frame.ReflectionProbes[0].Position=OffsetCapture;
        frame.ReflectionProbes[0].Environment=offset.Accepted;
        // Hold the box fixed in world space when moving the capture center.
        frame.ReflectionProbes[0].ProjectionOffset=-OffsetCapture;
        save("offset",frame);
        frame.Presentation.ScreenReflections.Enabled=true;
        save("mixed-offset",frame);
        frame.Presentation.ScreenReflections.Enabled=false;
        if (i==Poses/2) {
            auto rough=frame;
            rough.ReflectionProbes={local};
            for (auto& draw : rough.Meshes)
                if (draw.Identity==1) {
                    auto material=CreateRef<MaterialDefinition>(*draw.Materials.at(0));
                    material->Roughness=.28f;
                    draw.Materials={material};
                }
            save("satin",rough);
            frame.ReflectionProbes.clear();
            frame.Environment=centered.Accepted;
            save("global",frame);
        }
        if (metal) {
            frame.Environment.reset();
            frame.ReflectionProbes={local};
            frame.Presentation.ScreenReflections.Enabled=true;
            frame.Presentation.AmbientOcclusion.Enabled=true;
            frame.Presentation.TemporalAA=true;
            if (!i) render("",frame,temporalView);
            render("temporal"+suffix,frame,temporalView);
            if (temporalView.Temporal.Phase!=i+2 || !temporalView.TemporalMetrics->Valid ||
                !temporalView.AmbientTiming->Valid)
                throw std::runtime_error("Mirror sphere lost moving AO/TAA history");
        }
    }
}
}
