#include "Hazel/Core/Log.h"
#include "Renderer2DParity.h"
#include "Renderer3DParity.h"
#include "MultiscatteringQualification.h"
#include "LocalLightingQualification.h"
#include "DirectionalShadowQualification.h"
#include "SpotShadowQualification.h"
#include "XeGTAOQualification.h"
#include "TemporalAAQualification.h"
#include "TemporalEdgeQualification.h"
#include "Hazel/Renderer/Drawing/MeshGeometry.h"
#include "Hazel/Renderer/Resources/UniformBuffer.h"
#include "Renderer2DLines.h"
#include "Hazel/Core/Resources.h"
#include "Hazel/Renderer/RenderCommand.h"
#include "Hazel/Scene/Scene.h"
#include "Hazel/Scene/RuntimeSession.h"
#include "Hazel/Project/Project.h"
#include "Hazel/Scene/Entity.h"
#include "Hazel/Renderer/Scene/SceneRenderer.h"
#include "Hazel/Renderer/Drawing/Renderer2D.h"
#include "Platform/macOS/Metal/MetalRendererAPI.h"
#include "Hazel/Renderer/Resources/Framebuffer.h"
#include "Hazel/Renderer/Resources/ComputePipeline.h"
#include <cstring>
#include "Platform/macOS/Metal/MetalFramebuffer.h"
#include "Platform/macOS/Metal/MetalTexture.h"
#include "Platform/macOS/Metal/MetalShader.h"
#include <glm/gtc/matrix_transform.hpp>
#include <chrono>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <limits>
#include <thread>

namespace {
void Check(bool value, const std::string& message) { if (!value) throw std::runtime_error(message); }
template<class F> void Reject(const char* name, F&& action)
{
    try { action(); } catch (const std::exception& error) { std::cout << "PASS rejected " << name << ": " << error.what() << '\n'; return; }
    throw std::runtime_error(std::string("Expected rejection: ") + name);
}
Hazel::TextureSpecification Specification(bool linear = false)
{
    Hazel::TextureSpecification specification;
    specification.Width = specification.Height = 2;
    specification.GenerateMips = false;
    specification.MinFilter = specification.MagFilter = linear ? Hazel::TextureFilter::Linear : Hazel::TextureFilter::Nearest;
    specification.WrapS = specification.WrapT = Hazel::TextureWrap::ClampToEdge;
    return specification;
}
Hazel::FramebufferSpecification TargetSpecification(uint32_t width = 32, uint32_t height = 32)
{
    Hazel::FramebufferSpecification specification;
    specification.Width = width; specification.Height = height;
    specification.Attachments = {Hazel::FramebufferTextureFormat::RGBA32F,
        Hazel::FramebufferTextureFormat::RED_INTEGER, Hazel::FramebufferTextureFormat::DEPTH32F};
    return specification;
}
void Begin(const Hazel::Ref<Hazel::Framebuffer>& framebuffer, uint32_t width, uint32_t height, glm::vec4 clear = glm::vec4(0))
{
    if (framebuffer->GetSpecification().Width != width || framebuffer->GetSpecification().Height != height)
        framebuffer->Resize(width, height);
    framebuffer->Bind(); framebuffer->Clear(clear);
}
Hazel::Ref<const Hazel::FramebufferReadback> Submit(const Hazel::Ref<Hazel::Framebuffer>& framebuffer)
{
    framebuffer->Unbind(); return framebuffer->Submit();
}
glm::mat4 Transform(float x, float z, float width = 1.5f, float height = 3.f)
{
    return glm::translate(glm::mat4(1), {x, 0, z}) * glm::scale(glm::mat4(1), {width, height, 1});
}
void Wait(Hazel::Framebuffer& framebuffer, const std::shared_ptr<const Hazel::FramebufferReadback>& result)
{
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (!result->Completed)
    {
        framebuffer.Poll();
        if (std::chrono::steady_clock::now() >= deadline) throw std::runtime_error("Quad completion timeout");
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    Check(result->Error.empty(), result->Error);
}
void Pixel(const Hazel::FramebufferReadback& result, size_t x, size_t y, glm::vec4 color, int32_t id, float depth)
{
    const auto i = y * result.Width + x;
    Check(result.Entity[i] == id, "Signed ID/orientation mismatch at " + std::to_string(x) + "," + std::to_string(y));
    for (int c = 0; c < 4; ++c) Check(std::abs(result.Color[i][c] - color[c]) <= 1e-6f, "Color/UV/sampling/version mismatch at " + std::to_string(x) + "," + std::to_string(y) + " channel=" + std::to_string(c) + " actual=" + std::to_string(result.Color[i][c]) + " expected=" + std::to_string(color[c]));
    Check(std::abs(result.Depth[i] - depth) <= 1e-6f, "Depth/camera/version mismatch");
}
void Save(const Hazel::FramebufferReadback& result, const std::filesystem::path& path)
{
    std::ofstream output(path, std::ios::binary);
    for (size_t i = 0; i < result.Color.size(); ++i)
    {
        output.write(reinterpret_cast<const char*>(&result.Color[i]), 16);
        output.write(reinterpret_cast<const char*>(&result.Entity[i]), 4);
        output.write(reinterpret_cast<const char*>(&result.Depth[i]), 4);
    }
    Check(bool(output), "Cannot save quad readback");
}
}
namespace {
void ComputeDependencies(Hazel::MetalRendererAPI& backend, const std::filesystem::path& output)
{
    using namespace Hazel;
    auto pipeline = ComputePipeline::Create(output / "ColorDependency.metallib", "ColorDependency");
    Reject("missing compute entry", [&] { ComputePipeline::Create(output / "ColorDependency.metallib", "MissingEntry"); });
    auto source = Framebuffer::Create(TargetSpecification(31, 19));
    auto computed = Framebuffer::Create(TargetSpecification(31, 19));
    Reject("compute without producer", [&] { source->DispatchColorTo(computed, pipeline, std::vector<uint8_t>(16)); });
    auto gate = backend.GateQueueForTesting();
    std::array<Ref<const FramebufferReadback>,3> results;
    std::array<std::weak_ptr<const MetalFramebufferAttachments>,3> generations;
    for (size_t frame = 0; frame < 3; ++frame) {
        Begin(source, 31, 19, {.125f,.25f,.5f,1});
        source->ClearAttachment(1, -2000000003);
        Renderer2D::BeginScene(glm::mat4(1));
        Renderer2D::DrawQuad(Transform(0,0,1,1), glm::vec4(.25f,.375f,.625f,1), 2000000003);
        Renderer2D::EndScene(); source->Unbind();
        glm::vec4 parameters(frame == 0 ? 4.f : 1.f, float(frame+1)/64,0,0);
        std::vector<uint8_t> bytes(sizeof(parameters)); std::memcpy(bytes.data(), &parameters, bytes.size());
        Reject("in-place compute", [&] { source->DispatchColorTo(source,pipeline,bytes); });
        Reject("compute parameter ABI size", [&] { source->DispatchColorTo(computed,pipeline,std::vector<uint8_t>(8)); });
        source->DispatchColorTo(computed,pipeline,bytes);
        std::fill(bytes.begin(),bytes.end(),0xff); // CPU mutation must not change captured parameters
        if (frame == 1) {
            auto display = Framebuffer::Create(TargetSpecification(31,19));
            Begin(display,31,19); computed->CompositeWorldTo(display,0);
            results[frame] = Submit(display);
            generations[frame] = std::dynamic_pointer_cast<MetalFramebuffer>(display)->Snapshot();
        } else {
            results[frame] = computed->Submit();
            generations[frame] = std::dynamic_pointer_cast<MetalFramebuffer>(computed)->Snapshot();
        }
        Check(bool(results[frame]) && !results[frame]->Completed, "Compute dependency was not retained behind queue gate");
    }
    std::weak_ptr<ComputePipeline> pipelineOwner = pipeline;
    source.reset(); computed.reset(); pipeline.reset();
    Check(!pipelineOwner.expired(), "Pending compute pipeline released early");
    for (const auto& generation : generations) Check(!generation.expired(), "Pending compute generation released early");
    Check(backend.Observe()->Snapshot().InFlight == 3, "Compute dependencies did not use all three production slots");
    gate();
    auto poller = Framebuffer::Create(TargetSpecification(1,1));
    for (size_t frame = 0; frame < 3; ++frame) {
        Wait(*poller,results[frame]);
        const auto& result = *results[frame];
        for (uint32_t y=0; y<19; ++y) for (uint32_t x=0; x<31; ++x) {
            const auto pixel = size_t(y)*31+x;
            // Independent pixel-center coverage oracle at an odd extent.
            const bool covered = x >= 8 && x <= 22 && y >= 5 && y <= 13;
            Check(result.Entity[pixel] == (covered ? 2000000003 : -2000000003), "Compute changed signed ID or coverage");
            glm::vec4 expected = covered ? glm::vec4(.25f,.375f,.625f,1) : glm::vec4(.125f,.25f,.5f,1);
            if (frame == 0) expected *= glm::vec4(4,4,4,1); // HDR must survive the native storage write/readback.
            expected += glm::vec4(glm::vec3(float(frame+1)/64),0);
            if (frame == 1) expected -= glm::vec4(.04f,.04f,.04f,0); // independent low-range Neutral branch
            Pixel(result,x,y,expected,result.Entity[pixel],covered ? .5f : 1.f);
        }
        Save(result, output / ("compute-"+std::to_string(frame)+".bin"));
    }
    Check(pipelineOwner.expired(), "Completed compute pipeline did not retire");
    for (const auto& generation : generations) Check(generation.expired(), "Completed compute generation did not retire");
    Check(backend.Observe()->Snapshot().Errors == 0, "Native compute validation failed");
    std::cout << "PASS production render-compute-readback and render-compute-render-readback: three gated slots, immutable constants, signed IDs/depth, odd extent, pending destruction and retirement\n";
}
void TemporalLifetime(Hazel::MetalRendererAPI& backend, bool shadows=false) {
    using namespace Hazel;
    auto target=Framebuffer::Create(TargetSpecification());SceneRenderView view;
    SceneSubmission frame;frame.Presentation.SceneReferred=true;frame.Presentation.TemporalAA=true;
    frame.Presentation.Shadows.Enabled=shadows;
    if(shadows)frame.Light=SubmittedDirectional{glm::vec3(0,0,1),glm::vec3(1)};
    auto project=Project::LoadCandidate(std::filesystem::absolute("examples/RenderingReference/RenderingReference-Direct-PerspectivePBR.hproj"));
    frame.Assets=CreateRef<ProjectAssets>(project->GetAssetRoot());auto mesh=frame.Assets->Mesh("Native/Direct_v1/UnitCube.hmesh");
    Check(bool(mesh.Accepted),mesh.Error);
    auto material=CreateRef<MaterialDefinition>();material->Model=MaterialModel::Unlit;material->BaseColor={.1f,.2f,.3f,1};
    SubmittedMesh draw;draw.Geometry=mesh.Accepted;draw.Materials={material};draw.Identity=1;draw.Picking=-2000000003;
    frame.Meshes={draw};frame.Camera.Orthographic=true;frame.Camera.Position={0,0,3};
    frame.Camera.ViewProjection=glm::ortho(-1.f,1.f,-1.f,1.f,.1f,10.f)*glm::lookAt(frame.Camera.Position,glm::vec3(0),glm::vec3(0,1,0));
    // Compare complete moving diagonal-edge results with serialized submissions.
    // This catches history phase/read-write hazards hidden by flat interiors.
    auto pose=[&](unsigned index){
        material->BaseColor={.1f+float(index)*.2f,.2f,.3f,1};
        frame.Meshes[0].SetWorld(glm::translate(glm::mat4(1),{float(index)*.01f,0,0})*
            glm::rotate(glm::mat4(1),.37f,{0,0,1})*glm::scale(glm::mat4(1),{1.3f,.6f,.1f}));
        if(shadows)frame.ShadowCasters=frame.Meshes;
    };
    auto referenceTarget=Framebuffer::Create(TargetSpecification());SceneRenderView referenceView;
    std::array<Ref<const FramebufferReadback>,3> expectedFrames;
    pose(0);Begin(referenceTarget,32,32);SceneRenderer::Render(frame,referenceView,referenceTarget,[]{},[]{});
    auto initial=Submit(referenceTarget);Wait(*referenceTarget,initial);
    for(unsigned i=0;i<3;++i){pose(i);Begin(referenceTarget,32,32);SceneRenderer::Render(frame,referenceView,referenceTarget,[]{},[]{});expectedFrames[i]=Submit(referenceTarget);Wait(*referenceTarget,expectedFrames[i]);}
    referenceView.Reset();referenceTarget.reset();pose(0);
    // Warm history first, then gate three pending successors.
    Begin(target,32,32);SceneRenderer::Render(frame,view,target,[]{},[]{});auto warmup=Submit(target);Check(bool(warmup),"Temporal warmup busy");Wait(*target,warmup);
    std::array<Ref<const FramebufferReadback>,3> results;
    struct Release {std::function<void()> Call;~Release(){if(Call)Call();}} gate{backend.GateQueueForTesting()};
    unsigned motionFrame=0;
    for(auto& result:results) {
        pose(motionFrame++);
        Begin(target,32,32);SceneRenderer::Render(frame,view,target,[]{},[]{});
        result=Submit(target);Check(bool(result),"Temporal dependency unexpectedly busy");
        Check(view.Temporal.Submitted->load(),"Temporal commit did not publish history");
    }
    Begin(target,32,32);SceneRenderer::Render(frame,view,target,[]{},[]{});
    Check(!Submit(target)&&!view.Temporal.Submitted->load(),"Busy temporal packet published history");
    target->Cancel();view.Reset();target.reset();
    Check(backend.Observe()->Snapshot().InFlight==3,"Temporal history did not retain three slots");
    gate.Call();gate.Call={};
    auto poller=Framebuffer::Create(TargetSpecification(1,1));
    for(unsigned i=0;i<results.size();++i) {
        Wait(*poller,results[i]);const auto& image=*results[i];const size_t pixel=16*32+16;
        const auto& serial=*expectedFrames[i];
        Check(image.Entity==serial.Entity && image.Depth==serial.Depth,"Pending edge history changed IDs/depth");
        for(size_t p=0;p<image.Color.size();++p)Check(glm::length(image.Color[p]-serial.Color[p])<1e-6f,"Three pending edge histories differ from serialized phase/order");
        auto expected=glm::vec3(OracleNeutral({.1+double(i)*.2,.2,.3}));
        Check(image.Entity[pixel]==-2000000003&&glm::length(glm::vec3(image.Color[pixel])-expected)<1e-4f,"Queue-ordered TAA scratch/history overwrote an earlier frame");
    }
    Check(backend.Observe()->Snapshot().Errors==0,"Temporal pending retirement failed");
    std::cout<<"PASS temporal three-slot dependencies and serialized diagonal-edge equivalence, busy cancellation and view destruction before completion\n";
}

// Lit moving shadows make atlas read-after-write errors visible. The older
// lifetime probes intentionally use unlit materials and cannot establish this.
void ShadowAtlasLifetime(Hazel::MetalRendererAPI& backend, bool temporal=false, bool spot=false) {
    using namespace Hazel;
    auto project=Project::LoadCandidate(std::filesystem::absolute("examples/RenderingReference/RenderingReference.hproj"));
    SceneSubmission frame;frame.Assets=CreateRef<ProjectAssets>(project->GetAssetRoot());
    auto mesh=frame.Assets->Mesh("Native/Direct_v1/UnitCube.hmesh");Check(bool(mesh.Accepted),mesh.Error);
    auto material=CreateRef<MaterialDefinition>();material->BaseColor={.5f,.5f,.5f,1};material->Roughness=.8f;
    SubmittedMesh ground;ground.Geometry=mesh.Accepted;ground.Materials={material};ground.Identity=11;ground.Picking=11;
    ground.SetWorld(glm::translate(glm::mat4(1),{0,-.05f,0})*glm::scale(glm::mat4(1),{5,.1f,5}));
    SubmittedMesh caster=ground;caster.Identity=12;caster.Picking=12;
    frame.Presentation.SceneReferred=true;frame.Presentation.Shadows.Enabled=true;frame.Presentation.Shadows.Distance=20;
    frame.Presentation.TemporalAA=temporal;frame.Debug=temporal?SurfaceView::Shaded:SurfaceView::Direct;frame.LinearLightingInspection=!temporal;
    frame.Light=SubmittedDirectional{glm::normalize(glm::vec3(1,1,0)),glm::vec3(2)};
    frame.Camera.Orthographic=true;frame.Camera.Position={0,10,0};frame.Camera.ToCamera={0,1,0};
    frame.Camera.ViewProjection=glm::ortho(-2.f,2.f,-2.f,2.f,.1f,20.f)*glm::lookAt(frame.Camera.Position,glm::vec3(0),glm::vec3(0,0,-1));
    if (spot) {
        frame.Presentation.Shadows.Enabled = false;
        frame.Light.reset();
        SubmittedLocalLight light;
        light.Direction = glm::normalize(glm::vec3(-1, -1, 0));
        light.Position = {3, 3, 0};
        light.Incident = glm::vec3(16);
        light.InnerCos = std::cos(.4f);
        light.OuterCos = std::cos(.75f);
        light.CastShadows = true;
        frame.LocalLights = {light};
    }
    auto target=Framebuffer::Create(TargetSpecification(64,64));SceneRenderView view;
    auto render=[&](unsigned index) {
        if (spot) {
            frame.LocalLights[0].ShadowResolution = index == 2 ? 256 : 1024;
            frame.LocalLights[0].Position.z = float(index) * .2f;
            frame.LocalLights[0].Direction = glm::normalize(-frame.LocalLights[0].Position);
        }
        frame.Presentation.Shadows.Resolution=index==2?512:2048;
        caster.SetWorld(glm::translate(glm::mat4(1),{1+float(index)*.4f,1,0})*glm::scale(glm::mat4(1),glm::vec3(.6f)));
        frame.Meshes=frame.ShadowCasters={ground,caster};
        Begin(target,64,64);SceneRenderer::Render(frame,view,target,[]{},[]{});return Submit(target);
    };
    std::array<Ref<const FramebufferReadback>,3> expected,queued;
    for(unsigned i=0;i<3;++i){expected[i]=render(i);Check(bool(expected[i]),"Serialized shadow atlas busy");Wait(*target,expected[i]);}
    double changed=0;for(size_t p=0;p<expected[0]->Color.size();++p)if(expected[0]->Entity[p]==11 && expected[1]->Entity[p]==11)
        changed=std::max(changed,double(glm::length(expected[0]->Color[p]-expected[1]->Color[p])));
    Check(changed>.1,"Atlas lifetime fixture did not move visible lit shadows");
    view.Reset();
    std::weak_ptr<const MetalFramebufferAttachments> first,last;
    struct Release {std::function<void()> Call;~Release(){if(Call)Call();}} gate{backend.GateQueueForTesting()};
    for(unsigned i=0;i<3;++i) {
        queued[i]=render(i);Check(bool(queued[i]),"Queued shadow atlas busy");
        auto snapshot=std::static_pointer_cast<MetalFramebuffer>(spot ? view.SpotShadowDepth : view.ShadowDepth)->Snapshot();
        if(i==0)first=snapshot;
        if(i==1)Check(first.lock()==snapshot,"Private Ultra atlas storage was multiplied for queued frames");
        if(i==2)last=snapshot;
    }
    Check(!render(3),"Fourth gated shadow atlas submission was not busy");target->Cancel();view.Reset();
    Check(!first.expired() && !last.expired(),"Queued shadow atlases retired before completion");
    gate.Call();gate.Call={};
    for(unsigned i=0;i<3;++i) {
        Wait(*target,queued[i]);Check(queued[i]->Entity==expected[i]->Entity && queued[i]->Depth==expected[i]->Depth,"Queued shadow camera coverage changed");
        for(size_t p=0;p<queued[i]->Color.size();++p)Check(glm::length(queued[i]->Color[p]-expected[i]->Color[p])<1e-6f,"Queue-ordered atlas overwrote an earlier lit shadow");
    }
    Check(first.expired() && last.expired(),"Completed private shadow atlas generation retained");
    std::cout<<"PASS lit moving-shadow serialized/three-slot equivalence, shared atlas storage, resolution replacement, busy cancellation and view destruction/retirement spot="<<spot<<" TAA="<<temporal<<'\n';
}

void MeshLifetime(Hazel::MetalRendererAPI& backend, bool ambientOcclusion=false, bool shadows=false)
{
    using namespace Hazel;
    auto wideSpec=Specification();wideSpec.Width=8192;wideSpec.Height=1;
    auto wide=Texture2D::Create(wideSpec);std::vector<uint32_t> widePixels(8192,0xffffffff);
    wide->SetData(widePixels.data(),uint32_t(widePixels.size()*4));
    Check(std::dynamic_pointer_cast<MetalTexture2D>(wide)->Snapshot()->Texture->width()==8192,"Production sampled-texture extent was restricted to the old proof limit");
    auto project=Project::LoadCandidate(std::filesystem::absolute(
        "examples/RenderingReference/RenderingReference-Direct-PerspectivePBR.hproj"));
    auto assets=CreateRef<ProjectAssets>(project->GetAssetRoot());
    auto mesh=assets->Mesh("Native/Direct_v1/UnitCube.hmesh");Check(bool(mesh.Accepted),mesh.Error);
    std::weak_ptr<MeshGeometry> generation=assets->RealizeMesh(mesh.Accepted);
    auto material=CreateRef<MaterialDefinition>();material->Model=MaterialModel::Unlit;
    auto second=CreateRef<MaterialDefinition>(*material);second->BaseColor={0,0,1,1};
    SceneSubmission frame;frame.Assets=assets;frame.Camera.Orthographic=true;
    frame.Presentation.AmbientOcclusion.Enabled=ambientOcclusion;
    frame.Presentation.Shadows.Enabled=shadows;
    if(shadows)frame.Light=SubmittedDirectional{glm::vec3(0,0,1),glm::vec3(1)};
    frame.Camera.Position={0,0,3};frame.Camera.ViewProjection=glm::ortho(-1.f,1.f,-1.f,1.f,-1.f,1.f);
    SubmittedMesh draw;draw.Geometry=mesh.Accepted;draw.Materials={material};draw.Picking=-2000000003;
    draw.SetWorld(glm::translate(glm::mat4(1),{-.5f,0,0})*glm::scale(glm::mat4(1),{.5f,1,1}));
    frame.Meshes={draw};draw.Materials={second};draw.Picking=2000000003;
    draw.SetWorld(glm::translate(glm::mat4(1),{.5f,0,0})*glm::scale(glm::mat4(1),{.5f,1,1}));frame.Meshes.push_back(draw);
    if(shadows)frame.ShadowCasters=frame.Meshes;
    auto target=Framebuffer::Create(TargetSpecification());SceneRenderView view;
    std::array<Ref<const FramebufferReadback>,3> results;
    struct QueueRelease { std::function<void()> Release; ~QueueRelease(){if(Release)Release();} } gate{backend.GateQueueForTesting()};
    for(size_t i=0;i<results.size();++i) {
        frame.LocalLights.resize(32*(i+1));
        for(size_t l=0;l<frame.LocalLights.size();++l){frame.LocalLights[l].Identity=l+1;frame.LocalLights[l].Incident=glm::vec3(float(i+1));}
        material->BaseColor={i==0?1.f:0.f,i==1?1.f:0.f,i==2?1.f:0.f,1};
        Begin(target,32,32);
        const auto before=RenderCommand::GetDrawSubmissionCount();
        SceneRenderer::Render(frame,view,target,[]{},[]{});
        Check(RenderCommand::GetDrawSubmissionCount()==before+2+(ambientOcclusion?2:0)+(shadows?8:0),"Indexed/prepass draw submissions were counted incorrectly");
        results[i]=Submit(target);Check(bool(results[i]),"Gated mesh submit unexpectedly busy");
    }
    frame.Meshes.clear();frame.Assets.reset();assets.reset();project.reset();mesh.Accepted.reset();
    Check(!generation.expired(),"Pending mesh generation escaped cache admission accounting");
    if(ambientOcclusion || shadows)view.Reset(); // Pending frames retain AO generations after view destruction.
    Renderer3D::Shutdown(); // Native draws own all shader/texture/uniform versions already captured.
    Check(backend.Observe()->Snapshot().InFlight==3,"Three mesh frames were not held by completion gate");
    gate.Release();gate.Release={};
    for(size_t i=0;i<results.size();++i) {
        Wait(*target,results[i]);
        Pixel(*results[i],8,16,{i==0?1.f:0.f,i==1?1.f:0.f,i==2?1.f:0.f,1},-2000000003,.25f);
        Pixel(*results[i],24,16,{0,0,1,1},2000000003,.25f);
    }
    Check(generation.expired(),"Completed mesh cache owner did not retire");
    std::cout<<"PASS gated mesh/material/object versions, three pending frames, cache-owner retention and retirement\n";
}
}
int main(int argc, char** argv)
{
    try
    {
        Check(argc >= 3 && argc <= 5, "Usage: MetalRenderer2DQuadSmoke resource-root evidence-directory [texture-slots] [runtime-frames]");
        const auto output = std::filesystem::absolute(std::filesystem::u8path(argv[2]));
        std::filesystem::create_directories(output);
        auto pool = NS::TransferPtr(NS::AutoreleasePool::alloc()->init());
        Hazel::Log::Init();
        const auto root = std::filesystem::absolute(std::filesystem::u8path(argv[1]));
        Hazel::Resources::Configure({root, root, output / "user-data"});
        Hazel::RenderCommand::Configure(Hazel::RendererAPI::API::Metal);
        const bool offscreenBenchmark = argc == 4 && std::string(argv[3]) == "--xegtao-offscreen-benchmark";
        const bool temporalBenchmark = argc == 4 && std::string(argv[3]) == "--taa-offscreen-benchmark";
        const bool edges = argc == 4 && std::string(argv[3]) == "--taa-edges";
        const bool temporal = argc == 4 && std::string(argv[3]) == "--taa";
        const bool ao = argc == 4 && std::string(argv[3]) == "--xegtao";
        const bool compute = argc == 4 && std::string(argv[3]) == "--compute";
        const bool shadowTemporal = argc == 4 && std::string(argv[3]) == "--shadow-temporal-lifetime";
        const bool atlasTemporal = argc == 4 && std::string(argv[3]) == "--shadow-atlas-taa-lifetime";
        const bool atlasLifetime = argc == 4 && std::string(argv[3]) == "--shadow-atlas-lifetime";
        const bool shadows = argc == 4 && std::string(argv[3]) == "--shadows";
        const bool spotLifetime = argc == 4 && std::string(argv[3]) == "--spot-shadow-lifetime";
        const bool spotTemporalLifetime = argc == 4 && std::string(argv[3]) == "--spot-shadow-taa-lifetime";
        const bool spotShadows = argc == 4 && std::string(argv[3]) == "--spot-shadows";
        const bool localLights = argc == 4 && std::string(argv[3]) == "--local-lights";
        const bool multiscattering = atlasTemporal || atlasLifetime || shadowTemporal || shadows || spotShadows || spotLifetime || spotTemporalLifetime || localLights || (argc == 4 && std::string(argv[3]) == "--ggx-ms");
        const bool renderer3D = argc == 4 && std::string(argv[3]) == "--renderer3d-parity";
        Hazel::RendererSettings settings; settings.TextureSlots = argc >= 4 && !multiscattering && !renderer3D && !compute && !ao && !temporal && !edges && !temporalBenchmark && !offscreenBenchmark ? uint32_t(std::stoul(argv[3])) : 4;
        Check(settings.TextureSlots == 2 || settings.TextureSlots == 4,"Fixture supports two or four texture slots");
        const auto runtimeFrames=argc==5?std::stoul(argv[4]):80UL;
        Check(runtimeFrames>=80 && runtimeFrames<=6000,"Runtime frame count must be 80..6000");
        const uint32_t firstDraws=settings.TextureSlots == 2 ? 3 : 1;
        Hazel::RenderCommand::Init(settings);
        auto& backend = static_cast<Hazel::MetalRendererAPI&>(Hazel::RenderCommand::Backend());
        auto observer = backend.Observe();
        auto framebuffer = Hazel::Framebuffer::Create(TargetSpecification());
        Check(bool(std::dynamic_pointer_cast<Hazel::MetalFramebuffer>(framebuffer)), "Factory did not return a production Metal framebuffer");
        Hazel::Renderer2D::Init();
        if(ao || temporal || edges || temporalBenchmark || offscreenBenchmark) {
            const auto check=edges?TemporalEdgeQualification:temporalBenchmark?TemporalAAOffscreenBenchmark:temporal?TemporalAAQualification:offscreenBenchmark?XeGTAOOffscreenBenchmark:XeGTAOQualification;
            check(output,[&](const auto& target) {
                auto result=Submit(target);Check(bool(result),"AO submit busy");Wait(*target,result);return result;
            });
            if(temporal || edges)TemporalLifetime(backend);
            else MeshLifetime(backend,true);
            framebuffer.reset();Hazel::Renderer2D::Shutdown();Hazel::RenderCommand::Shutdown();
            Check(observer->Snapshot().Errors==0 && observer->Snapshot().ResourceBytes==0 && observer->Snapshot().AttachmentGenerations==0,"AO retirement failed");
            std::cout << "PASS focused native scene qualification and retirement\n";return 0;
        }
        if (compute) {
            ComputeDependencies(backend, output);
            framebuffer.reset(); Hazel::Renderer2D::Shutdown(); Hazel::RenderCommand::Shutdown();
            const auto state = observer->Snapshot();
            Check(state.Submitted == 3 && state.Completed == 3 && state.ResourceBytes == 0 && state.Errors == 0 && state.AttachmentGenerations == 0, "Compute shutdown did not retire production resources");
            return 0;
        }
        if (multiscattering) {
            if(atlasLifetime || atlasTemporal)ShadowAtlasLifetime(backend,atlasTemporal);
            else if (spotLifetime || spotTemporalLifetime) ShadowAtlasLifetime(backend, spotTemporalLifetime, true);
            else if(shadowTemporal)TemporalLifetime(backend,true);
            else {
            (spotShadows?SpotShadowQualification:shadows?DirectionalShadowQualification:localLights?LocalLightingQualification:MultiscatteringQualification)(output, [&](const auto& target) {
                auto result=Submit(target);Check(bool(result),"GGX submit busy");Wait(*target,result);return result;
            });
            if (!spotShadows) MeshLifetime(backend,true,shadows);
            }
            framebuffer.reset();Hazel::Renderer2D::Shutdown();Hazel::RenderCommand::Shutdown();
            Check(observer->Snapshot().Errors==0 && observer->Snapshot().ResourceBytes==0 && observer->Snapshot().AttachmentGenerations==0,"GGX retirement failed");
            if (spotShadows || spotLifetime || spotTemporalLifetime) std::cout << "SPOT_RESOURCES peak=" << observer->Snapshot().PeakResourceBytes
                << " attachments=" << observer->Snapshot().PeakAttachmentBytes << " final=" << observer->Snapshot().ResourceBytes << '\n';
            return 0;
        }
        if (renderer3D) {
            Renderer3DParity(output, [&](const auto& target) {
                auto result = Submit(target); Check(bool(result), "3D submit busy"); Wait(*target, result); return result;
            });
            MeshLifetime(backend);
            framebuffer.reset();
            auto stoppedShader=backend.LoadShader("ForwardPBR");
            Hazel::Renderer2D::Shutdown(); Hazel::RenderCommand::Shutdown();
            Reject("shader binding after shutdown",[&]{stoppedShader->Bind();});stoppedShader.reset();
            Reject("vertex factory after shutdown",[]{Hazel::VertexBuffer::Create(64);});
            uint32_t index=0;
            Reject("index factory after shutdown",[&]{Hazel::IndexBuffer::Create(&index,1);});
            Reject("uniform factory after shutdown",[]{Hazel::UniformBuffer::Create(16,2);});
            Hazel::LightingImage cube;cube.Kind=Hazel::LightingImageKind::Background;cube.Side=1;cube.Mips={std::vector<float>(18,0)};
            Reject("cube factory after shutdown",[&]{Hazel::TextureCube::Create(cube);});
            Check(observer->Snapshot().Errors == 0 && observer->Snapshot().ResourceBytes == 0 && observer->Snapshot().AttachmentGenerations == 0, "3D retirement failed");
            std::cout << "PASS production Renderer3D captures, completion and retirement\n"; return 0;
        }
        Reject("duplicate quad initialization", [] { Hazel::Renderer2D::Init(); });
        Reject("unsupported attachment extent", [&] { Hazel::Framebuffer::Create(TargetSpecification(4097, 32)); });
        auto badProfile = TargetSpecification(); badProfile.Samples = 2;
        Reject("multisample framebuffer", [&] { Hazel::Framebuffer::Create(badProfile); });
        badProfile = TargetSpecification(); badProfile.SwapChainTarget = true;
        Reject("swapchain framebuffer", [&] { Hazel::Framebuffer::Create(badProfile); });
        badProfile = TargetSpecification(); badProfile.Attachments.Attachments[2].TextureFormat = Hazel::FramebufferTextureFormat::DEPTH24STENCIL8;
        Reject("unqualified depth format", [&] { Hazel::Framebuffer::Create(badProfile); });
        badProfile = TargetSpecification(); badProfile.Attachments.Attachments[0].TextureFormat = Hazel::FramebufferTextureFormat::RGBA16F;
        Reject("unqualified color format", [&] { Hazel::Framebuffer::Create(badProfile); });
        Reject("scene without target", [] { Hazel::Renderer2D::BeginScene(glm::mat4(1)); });
        auto texture = Hazel::Texture2D::Create(Specification());
        const std::array<uint32_t, 4> imageA{0xff0000ff, 0xff00ff00, 0xffff0000, 0xffffffff};
        const std::array<uint32_t, 4> imageB{0xffffff00, 0xffff00ff, 0xff00ffff, 0xff000000};
        const std::array<glm::vec4, 4> colorsA{{{1,0,0,1}, {0,1,0,1}, {0,0,1,1}, {1,1,1,1}}};
        const std::array<glm::vec4, 4> colorsB{{{0,1,1,1}, {1,0,1,1}, {1,1,0,1}, {0,0,0,1}}};
        auto transparent = imageA; transparent[0] = 0x000000ff;
        texture->SetData(transparent.data(), sizeof(transparent));
        texture->SetData(imageA.data(), sizeof(imageA));
        auto native = std::dynamic_pointer_cast<Hazel::MetalTexture2D>(texture);
        std::weak_ptr<const Hazel::MetalTextureVersion> oldA = native->Snapshot();
        const auto projection = glm::ortho(-2.f, 2.f, -2.f, 2.f, -1.f, 1.f);
        const auto cameraA = projection * glm::translate(glm::mat4(1), {-.5f, 0, 0});
        const auto cameraB = projection * glm::translate(glm::mat4(1), {.5f, 0, 0});
        const glm::vec4 tintA{2,.5f,.25f,1}, tintB{.5f,1,2,1}, clear{.125f,.25f,.5f,1};
        Begin(framebuffer, 32, 32, clear);
        Hazel::Renderer2D::BeginScene(cameraA);
        Hazel::Renderer2D::DrawQuad(Transform(-.5f, .5f), texture, 1, tintA, -123456789);
        Hazel::Renderer2D::EndScene();
        texture->SetData(imageB.data(), sizeof(imageB));
        std::weak_ptr<const Hazel::MetalTextureVersion> oldB = native->Snapshot();
        Hazel::Renderer2D::BeginScene(cameraB);
        Hazel::Renderer2D::DrawQuad(Transform(.5f, .5f), texture, 1, tintB, 123456789);
        Hazel::Renderer2D::EndScene();
        Hazel::Renderer2D::BeginScene(cameraA);
        Hazel::Renderer2D::DrawQuad(Transform(-.5f, -.5f), texture, 1, glm::vec4(1), 42); // Farther, rejected by depth.
        Hazel::Renderer2D::DrawQuad(Transform(-.5f, 2.f), texture, 1, glm::vec4(1), 43); // Before near plane, clipped.
        Hazel::Renderer2D::EndScene();
        texture->SetData(imageA.data(), sizeof(imageA)); // Must not replace either captured version.
        Check(!oldA.expired() && !oldB.expired(), "Captured texture versions retired before submission");
        auto numeric = Submit(framebuffer);
        Wait(*framebuffer, numeric);
        for (size_t y = 0; y < 32; ++y) for (size_t x = 0; x < 32; ++x)
        {
            if (y >= 4 && y < 28 && ((x >= 2 && x < 14) || (x >= 18 && x < 30)))
            {
                const bool left = x < 14;
                const auto local = x - (left ? 2 : 18);
                const size_t col = local < 6 ? 0 : 1, row = y < 16 ? 1 : 0;
                Pixel(*numeric, x, y, (left ? colorsA : colorsB)[row * 2 + col] * (left ? tintA : tintB), left ? -123456789 : 123456789, .25f);
            }
            else Pixel(*numeric, x, y, clear, -1, 1);
        }
        Check(framebuffer->ReadPixel(1, 3, 5) == -123456789 && framebuffer->ReadDepthPixel(3, 5) == .25f &&
            framebuffer->ReadColorPixel(0, 0, 0) == clear, "Factory virtual readback mismatch");
        Reject("integer attachment as color", [&] { framebuffer->ReadColorPixel(1, 0, 0); });
        Reject("negative readback coordinate", [&] { framebuffer->ReadPixel(1, -1, 0); });
        Reject("out-of-range readback coordinate", [&] { framebuffer->ReadDepthPixel(32, 0); });
        Reject("GL renderer ID", [&] { framebuffer->GetColorAttachmentRendererID(); });
        Reject("output encoding", [&] { framebuffer->EncodeOutput(); });
        Reject("compositing", [&] { framebuffer->CompositeWorldTo({}, 1); });
        Reject("outline", [&] { framebuffer->DrawEntityOutlineTo({}, 1, 1, glm::vec4(1)); });
        Check(oldA.expired() && oldB.expired(), "Completed draw versions were not retired");
        Save(*numeric, output / "nearest-camera-versions.bin");
        std::cout << "PASS nearest/camera/version/depth: 1024 pixels, two cameras and texture versions, exact signed IDs, HDR, far/near rejection\n";

        // Empty flush/end must not manufacture draws. Empty submission proves attachment clears.
        Hazel::Renderer2D::ResetStats();
        Hazel::Renderer2D::Flush();
        Begin(framebuffer, 32, 32, clear);
        Hazel::Renderer2D::BeginScene(glm::mat4(1));
        Hazel::Renderer2D::Flush(); Hazel::Renderer2D::EndScene();
        auto empty = Submit(framebuffer); Wait(*framebuffer, empty);
        for (size_t y = 0; y < 32; ++y) for (size_t x = 0; x < 32; ++x) Pixel(*empty, x, y, clear, -1, 1);
        Check(Hazel::Renderer2D::GetStats().DrawCalls == 0 && Hazel::Renderer2D::GetStats().UploadedBytes == 0,
            "Empty flush changed draw/upload statistics");

        auto second = Hazel::Texture2D::Create(Specification()); second->SetData(imageB.data(), sizeof(imageB));
        Begin(framebuffer, 32, 32);
        Hazel::Renderer2D::BeginOrderedScene(Hazel::Camera{}, glm::mat4(1));
        Reject("nested scene", [] { Hazel::Renderer2D::BeginScene(glm::mat4(1)); });
        auto invalid = glm::mat4(1); invalid[0][0] = std::numeric_limits<float>::quiet_NaN();
        Hazel::Renderer2D::DrawQuad(invalid, texture);
        Reject("invalid chunk flush", [] { Hazel::Renderer2D::Flush(); });
        Hazel::Renderer2D::DrawQuad(Transform(0,.9f,4,4),glm::vec4(1,0,1,1),876);
        Hazel::Renderer2D::DrawCircle(invalid,glm::vec4(1),1,.005f,877);
        Reject("atomic mixed chunk failure", [] { Hazel::Renderer2D::Flush(); });
        Hazel::Renderer2D::ResetStats();
        Hazel::Renderer2D::DrawQuad(Transform(-.5f, 0, 1, 1), texture, 1, glm::vec4(1), -2000000001);
        texture->SetData(imageB.data(), sizeof(imageB));
        Hazel::Renderer2D::DrawQuad(Transform(.5f, 0, 1, 1), texture, 1, glm::vec4(1), 2000000001);
        texture->SetData(imageA.data(), sizeof(imageA)); // both pending versions must survive until flush/completion
        // Equal-depth overlap: first submitted quad wins with LESS. Texture slot one is reused.
        Hazel::Renderer2D::DrawQuad(Transform(-.5f, 0, 1, 1), texture, 1, glm::vec4(.5f,.5f,.5f,1), 99);
        Hazel::Renderer2D::Flush(); Hazel::Renderer2D::Flush();
        auto stats = Hazel::Renderer2D::GetStats();
        Check(stats.QuadCount == 3 && stats.GeometryCount == 3 && stats.DrawCalls == firstDraws && stats.UploadedBytes == 768 &&
            stats.Flushes[size_t(Hazel::Renderer2D::FlushReason::External)] == 1,
            "Shared quad run did not coalesce, consume once, or account correctly");
        // A second run in the same scene must keep the earlier immutable vertex/camera/descriptor data.
        second->SetData(imageA.data(), sizeof(imageA)); // earlier right-hand run must still sample B
        Hazel::Renderer2D::DrawQuad(Transform(-.5f, .5f, 1, 1), second, 1, glm::vec4(1), 98);
        Hazel::Renderer2D::EndScene();
        stats = Hazel::Renderer2D::GetStats();
        Check(stats.QuadCount == 4 && stats.DrawCalls == firstDraws + 1 && stats.UploadedBytes == 1024 &&
            stats.Flushes[size_t(Hazel::Renderer2D::FlushReason::End)] == 1, "Second shared flush statistics mismatch");
        auto ordered = Submit(framebuffer); Wait(*framebuffer, ordered);
        for (size_t y = 0; y < 32; ++y) for (size_t x = 0; x < 32; ++x)
        {
            if (y >= 8 && y < 24)
            {
                const bool left = x < 16;
                const size_t col = (x % 16) < 8 ? 0 : 1, row = y < 16 ? 1 : 0;
                Pixel(*ordered, x, y, (left ? colorsA : colorsB)[row * 2 + col], left ? -2000000001 : 2000000001, .5f);
            }
            else Pixel(*ordered, x, y, glm::vec4(0), -1, 1);
        }
        Save(*ordered, output / "ordered-run.bin");
        std::cout << "PASS shared ordered runs: four quads, " << stats.DrawCalls << " draws/flushes, 1024 vertex bytes, equal-depth ordering; empty clears and rejected chunk recovery\n";
        // CPU shutdown discards unflushed work; reinitialization needs no font service.
        Begin(framebuffer, 32, 32);
        Hazel::Renderer2D::BeginScene(glm::mat4(1));
        Hazel::Renderer2D::DrawQuad(glm::mat4(1), texture);
        Hazel::Renderer2D::Shutdown(); framebuffer->Cancel();
        Reject("draw after shutdown", [&] { Hazel::Renderer2D::DrawQuad(glm::mat4(1), texture); });
        Hazel::Renderer2D::Init();

        // Full bounded chunk: automatic capacity flush uses all 32 vertices / 48 indices.
        Hazel::Renderer2D::ResetStats();
        Begin(framebuffer, 32, 32);
        Hazel::Renderer2D::BeginScene(glm::mat4(1));
        for (int q = 0; q < 8; ++q)
            Hazel::Renderer2D::DrawQuad(Transform(-.875f + q * .25f, 0, .25f, 1), texture, 1, glm::vec4(1), q % 2 ? -100-q : 100+q);
        for (int q = 8; q < 20001; ++q)
            Hazel::Renderer2D::DrawQuad(Transform(1000, 0, 1, 1), texture);
        Hazel::Renderer2D::EndScene();
        const auto capacityStats = Hazel::Renderer2D::GetStats();
        Check(capacityStats.DrawCalls == 2 && capacityStats.QuadCount == 20001 && capacityStats.UploadedBytes == 20001 * 256 &&
            capacityStats.Flushes[size_t(Hazel::Renderer2D::FlushReason::Capacity)] == 1, "Capacity flush/recovery statistics mismatch");
        auto capacity = Submit(framebuffer); Wait(*framebuffer, capacity);
        for (size_t y = 0; y < 32; ++y) for (size_t x = 0; x < 32; ++x)
        {
            if (y >= 8 && y < 24)
            {
                const int q = int(x / 4);
                Pixel(*capacity, x, y, colorsA[(y < 16 ? 2 : 0) + (x % 4 >= 2 ? 1 : 0)], q % 2 ? -100-q : 100+q, .5f);
            }
            else Pixel(*capacity, x, y, glm::vec4(0), -1, 1);
        }
        Save(*capacity, output / "capacity-run.bin");
        std::cout << "PASS full capacity run: 20001 quads, two draws, capacity flush preserves visible reference\n";

        // Nonconstant 2x2 texture with linear filtering: CPU bilinear oracle at pixel centers.
        auto linearSpec = Specification(true); linearSpec.Format = Hazel::ImageFormat::RGBA32F;
        auto linear = Hazel::Texture2D::Create(linearSpec); linear->SetData(colorsA.data(), sizeof(colorsA));
        Begin(framebuffer, 32, 32);
        Hazel::Renderer2D::BeginScene(glm::mat4(1));
        Hazel::Renderer2D::DrawQuad(Transform(0, 0, 1, 1), linear, 1, glm::vec4(1), -987654321);
        Hazel::Renderer2D::EndScene(); auto filtered = Submit(framebuffer); Wait(*framebuffer, filtered);
        Save(*filtered, output / "linear.bin");
        for (size_t y = 8; y < 24; ++y) for (size_t x = 8; x < 24; ++x)
        {
            const float u = (float(x) + .5f - 8) / 16, v = 1 - (float(y) + .5f - 8) / 16;
            const float a = std::clamp(u * 2 - .5f, 0.f, 1.f), b = std::clamp(v * 2 - .5f, 0.f, 1.f);
            const auto expected = glm::mix(glm::mix(colorsA[0], colorsA[1], a), glm::mix(colorsA[2], colorsA[3], a), b);
            Pixel(*filtered, x, y, expected, -987654321, .5f);
        }
        Save(*filtered, output / "linear.bin");
        std::cout << "PASS linear sampling: 256 pixels, clamp bilinear oracle\n";
        auto repeatSpec = Specification(); repeatSpec.WrapS = repeatSpec.WrapT = Hazel::TextureWrap::Repeat;
        auto repeat = Hazel::Texture2D::Create(repeatSpec); repeat->SetData(imageA.data(), sizeof(imageA));
        Begin(framebuffer, 32, 32);
        Hazel::Renderer2D::BeginScene(glm::mat4(1));
        Hazel::Renderer2D::DrawQuad(Transform(0, 0, 1, 1), repeat, 2, glm::vec4(1), -135791113);
        Hazel::Renderer2D::EndScene(); auto tiled = Submit(framebuffer); Wait(*framebuffer, tiled);
        for (size_t y = 8; y < 24; ++y) for (size_t x = 8; x < 24; ++x)
        {
            const float u = (float(x) + .5f - 8) / 8, v = 2 - (float(y) + .5f - 8) / 8;
            const size_t col = (u - std::floor(u)) < .5f ? 0 : 1, row = (v - std::floor(v)) < .5f ? 0 : 1;
            Pixel(*tiled, x, y, colorsA[row * 2 + col], -135791113, .5f);
        }
        Save(*tiled, output / "repeat.bin");
        std::cout << "PASS repeat sampling: 256 pixels, tiling factor two and repeat addressing\n";
        for (int frame = 0; frame < 12; ++frame)
        {
            Begin(framebuffer, 32, 32);
            Hazel::Renderer2D::BeginScene(glm::mat4(1));
            Hazel::Renderer2D::DrawQuad(Transform(0, 0, frame % 2 ? -1.f : 1.f, 1), texture, 1, glm::vec4(1), 1000 + frame);
            Hazel::Renderer2D::EndScene(); auto result = Submit(framebuffer); Wait(*framebuffer, result);
            Pixel(*result, 12, 12, colorsA[frame % 2 ? 3 : 2], 1000 + frame, .5f);
            Pixel(*result, 0, 0, glm::vec4(0), -1, 1);
        }
        Save(*numeric, output / "nearest-after-reuse.bin");
        // Production Scene -> SceneRenderer -> Renderer2D, using the same framebuffer factory.
        // Large asymmetric target crosses the old 32-pixel scalar-row limit.
        {
            auto scene = Hazel::CreateRef<Hazel::Scene>();
            scene->OnViewportResize(96, 64);
            auto camera = scene->CreateEntity("Integrated camera");
            camera.AddComponent<Hazel::CameraComponent>().Camera.SetOrthographic(2, -1, 1);
            auto entity = scene->CreateEntity("Integrated quad");
            auto& sprite = entity.AddComponent<Hazel::SpriteRendererComponent>();
            sprite.Source = Hazel::TextureSpriteSource{{}, 1, texture};
            texture->SetData(imageA.data(), sizeof(imageA));
            auto display = Hazel::Framebuffer::Create(TargetSpecification(96, 64));
            Hazel::SceneRenderView view;
            scene->OnRuntimeStart();
            auto renderScene = [&] {
                display->Bind(); display->Clear({.125f,.25f,.5f,1});
                scene->OnUpdateRuntime(0, &view, display);

                auto result = Submit(display); Wait(*display, result); return result;
            };
            auto first = renderScene();
            Check(first->Entity[32 * 96 + 48] == int(entity), "Integrated Scene lost signed entity identity");
            Check(first->Depth[32 * 96 + 48] == .5f, "Integrated Scene depth differs");
            camera.GetComponent<Hazel::TransformComponent>().Translation.x = .5f;
            texture->SetData(imageB.data(), sizeof(imageB));
            auto changed = renderScene();
            Check(first->Color != changed->Color && first->Entity != changed->Entity,
                "Camera/texture changes failed to reach Scene submission");
            scene->Presentation().SceneReferred = true;
            scene->Presentation().Exposure = 1;
            auto hdr = renderScene();
            const auto center = 32 * 96 + 32;
            Check(hdr->Entity[center] == int(entity) && hdr->Depth[center] == changed->Depth[center],
                "World presentation must preserve exact IDs and world depth");
            // At this coordinate image B is yellow; exposure doubles linear radiance.
            auto neutral = [](glm::vec3 value) {
                float minimum = std::min(value.x, std::min(value.y, value.z));
                value -= minimum < .08f ? minimum - 6.25f * minimum * minimum : .04f;
                float maximum = std::max(value.x, std::max(value.y, value.z));
                if (maximum < .76f) return value;
                float compressed = 1 - .0576f / (maximum - .52f);
                return glm::mix(value * (compressed / maximum), glm::vec3(compressed), 1 - 1 / (1 + .15f * (maximum - compressed)));
            };
            const auto expected = neutral(glm::vec3(changed->Color[center]) * 2.f);
            for (int c = 0; c < 3; ++c) Check(std::abs(hdr->Color[center][c] - expected[c]) <= 1e-6f, "Integrated Neutral/exposure conversion differs");
            entity.AddComponent<Hazel::MeshRendererComponent>();
            scene->ValidateRendering(); // Empty optional mesh follows the shared scene contract.
            scene->OnRuntimeStop();
            std::cout << "PASS production Scene: camera/texture changes, 96x64, signed IDs, depth, Neutral exposure, shared optional mesh admission\n";
        }
        std::vector<Hazel::FramebufferReadback> metalParity;
        auto parityTarget = Hazel::Framebuffer::Create(TargetSpecification(128,128));
        for (unsigned phase = 0; phase < 4; ++phase) {
            Begin(parityTarget,128,128);
            Hazel::Renderer2D::ResetStats(); DrawRenderer2DParity(phase);
            auto result = Submit(parityTarget); Wait(*parityTarget,result);
            Save(*result,output / ("mixed-metal-" + std::to_string(phase) + ".bin"));
            metalParity.push_back(*result);
            if (phase < 3) {
            Pixel(*result,4,4,{.05f,.1f,.15f,.25f},-2147483647,1);
            Pixel(*result,16,20,{.525f,.05f,.075f,.625f},2147483647,1);
            Pixel(*result,17,19,{.2625f,.025f,.5375f,.8125f},2147483647,1);
            Pixel(*result,18,46,{.525f,.05f,.075f,.625f},2147483647,1);
            Pixel(*result,68,42,{.2625f,.525f,.0375f,.8125f},-2000000001,1);
            Pixel(*result,82,42,{.025f,.55f,.075f,.625f},-2000000001,1);
            } else Pixel(*result,64,64,{.25f,0,.5f,.75f},-2000000001,1);
            const auto measured = Hazel::Renderer2D::GetStats();
            Check(phase == 3 || measured.PrimitiveTransitions >= 4, "Mixed primitive order did not reach native runs");
            if (phase == 2) Check(measured.Flushes[size_t(Hazel::Renderer2D::FlushReason::Textures)] >= 2,
                "Reduced texture slots did not flush");
        }
        {
            const auto cases=Renderer2DLineCases();
            std::ofstream metadata(output/"line-cases.json");WriteRenderer2DLineCases(metadata,cases);
            Check(bool(metadata),"Cannot save native line metadata");
            for(size_t index=0;index<cases.size();++index) {
                const auto& c=cases[index];Begin(parityTarget,c.Extent,c.Extent);
                DrawRenderer2DLineCase(c);
                auto result=Submit(parityTarget);Wait(*parityTarget,result);
                Save(*result,output/("line-"+std::to_string(index)+".bin"));
            }
            std::cout<<"PASS native deterministic line captures: "<<cases.size()<<" (oracle is separate)\n";
        }
        parityTarget.reset();
        {
            auto project=Hazel::Project::LoadCandidate(std::filesystem::absolute("examples/Sandbox/Sandbox-Renderer2D.hproj"));
            Hazel::RuntimeSession session;
            session.Start(project,project->LoadScene(project->GetConfig().StartScene));
            Hazel::SceneRenderView runtimeView;
            std::ofstream timings(output/"runtime-cpu.csv");
            timings<<"width,height,frame,runtime_ms,submit_ms,draws,batches,geometry,upload_bytes,attachment_allocations,attachment_reuses,attachment_bytes,buffer_allocations,buffer_reuses,retained_buffer_bytes,serial\n";
            for(const auto extent : {glm::uvec2(1280,720),glm::uvec2(1920,1080)}) {
                auto display=Hazel::Framebuffer::Create(TargetSpecification(extent.x,extent.y));
                session.Resize(extent.x,extent.y);session.ResizeFramebuffer(extent.x,extent.y);
                session.SetRenderTarget(display);session.SetInput({-1,-1},false);
                for(unsigned frame=0;frame<runtimeFrames;++frame) {
                    display->Bind();display->Clear(session.GetScene()->Presentation().Background);
                    Hazel::Renderer2D::ResetStats();
                    const auto start=std::chrono::steady_clock::now();
                    // Headless fixture supplies device samples; real Application/Session::Update is qualified by MetalM1Smoke.
                    session.RouteUIInput(Hazel::GameUI::DeviceSample{},1.f/60,false);
                    session.GetScene()->OnUpdateRuntime(Hazel::Timestep(1.f/60),&runtimeView,display);
                    const auto encoded=std::chrono::steady_clock::now();
                    auto result=Submit(display);
                    const auto submitted=std::chrono::steady_clock::now();
                    Check(bool(result),"Serial runtime offscreen submission unexpectedly busy");
                    Wait(*display,result);
                    const auto counts=Hazel::Renderer2D::GetStats();const auto resources=observer->Snapshot();
                    uint32_t batches=0;for(auto count:counts.Flushes)batches+=count;
                    timings<<extent.x<<','<<extent.y<<','<<frame<<','
                        <<std::chrono::duration<double,std::milli>(encoded-start).count()<<','
                        <<std::chrono::duration<double,std::milli>(submitted-encoded).count()<<','
                        <<counts.DrawCalls<<','<<batches<<','<<counts.GeometryCount<<','<<counts.UploadedBytes<<','
                        <<resources.AttachmentAllocations<<','<<resources.AttachmentReuses<<','<<resources.AttachmentBytes<<','
                        <<resources.BufferAllocations<<','<<resources.BufferReuses<<','<<resources.ResourceBytes<<','<<result->Serial<<'\n';
                    if(frame+1==runtimeFrames)Save(*result,output/("runtime-"+std::to_string(extent.x)+"x"+std::to_string(extent.y)+".bin"));
                }
                FocusRenderer2DRuntimeUI(session);
                display->Bind();display->Clear(session.GetScene()->Presentation().Background);
                session.GetScene()->OnUpdateRuntime(Hazel::Timestep(0),&runtimeView,display);
                auto focused=Submit(display);Wait(*display,focused);
                Save(*focused,output/("runtime-focus-"+std::to_string(extent.x)+"x"+std::to_string(extent.y)+".bin"));
                session.CancelUIGestures();session.SetInput({-1,-1},false);
                session.SetRenderTarget({});
            }
            session.Stop();
        }

        {
            Hazel::SceneSubmission submission;
            submission.Presentation.SceneReferred=true;
            Hazel::SceneRenderView view;
            auto target=Hazel::Framebuffer::Create(TargetSpecification());
            target->Bind();target->Clear(glm::vec4(0));
            Reject("failed production pass",[&] {
                Hazel::SceneRenderer::Render(submission,view,target,[&] {
                    Hazel::Renderer2D::BeginScene(glm::mat4(1));
                    auto invalid=glm::mat4(1);invalid[0][0]=std::numeric_limits<float>::quiet_NaN();
                    Hazel::Renderer2D::DrawCircle(invalid,glm::vec4(1));
                    Hazel::Renderer2D::EndScene();
                },[] {});
            });
            Begin(target,32,32);
            Hazel::Renderer2D::BeginScene(glm::mat4(1));
            Hazel::Renderer2D::DrawQuad(glm::mat4(1),glm::vec4(1),-2147483647);
            Hazel::Renderer2D::EndScene();auto recovered=Submit(target);Wait(*target,recovered);
            Pixel(*recovered,16,16,glm::vec4(1),-2147483647,.5f);
        }
        texture->SetData(imageA.data(), sizeof(imageA));
        auto lifecycleGate = backend.GateQueueForTesting();
        std::array<std::weak_ptr<const Hazel::MetalTextureVersion>, 3> lifecycleVersions;
        lifecycleVersions[0] = native->Snapshot();
        auto resized = Hazel::Framebuffer::Create(TargetSpecification(32, 16));
        auto metalResized = std::dynamic_pointer_cast<Hazel::MetalFramebuffer>(resized);
        std::weak_ptr<const Hazel::MetalFramebufferAttachments> generationA = metalResized->Snapshot();
        Begin(resized, 32, 16, glm::vec4(.25f,.5f,2,1));
        resized->ClearAttachment(1, -2000000003);
        Reject("ambient offscreen clear", [] { Hazel::RenderCommand::Clear(); });
        Reject("ambient offscreen clear color", [] { Hazel::RenderCommand::SetClearColor(glm::vec4(1)); });
        Reject("ambient offscreen viewport", [] { Hazel::RenderCommand::SetViewport(0, 0, 1, 1); });
        Reject("resize during captured pass", [&] { resized->Resize(16, 32); });
        Reject("another framebuffer bind", [&] { framebuffer->Bind(); });
        Hazel::Renderer2D::BeginScene(glm::mat4(1));
        Reject("clear during active scene", [&] { resized->Clear(glm::vec4(1)); });
        Reject("unbind during active scene", [&] { resized->Unbind(); });
        Reject("cancel during active scene", [&] { resized->Cancel(); });
        Hazel::Renderer2D::DrawQuad(Transform(0, 0, 1, 1), texture, 1, glm::vec4(1), -2147483647);
        Hazel::Renderer2D::EndScene();
        Reject("clear after captured draws", [&] { resized->Clear(glm::vec4(1)); });
        resized->Unbind();
        framebuffer->Bind();
        Reject("prior generation read after bind", [&] { framebuffer->ReadPixel(1, 0, 0); });
        framebuffer->Unbind(); // two resource-owned CPU packets can coexist
        auto resizedA = resized->Submit(); framebuffer->Cancel();
        Reject("pending framebuffer read", [&] { resized->ReadPixel(1, 0, 0); });
        resized->Resize(16, 32); // gate remains closed: no global wait can be used here
        Check(!generationA.expired(), "Resize released a submitted attachment generation");
        Begin(resized, 16, 32, glm::vec4(2,.5f,.25f,1));
        resized->ClearAttachment(1, 2000000003);
        std::weak_ptr<const Hazel::MetalFramebufferAttachments> generationB = metalResized->Snapshot();
        texture->SetData(imageB.data(), sizeof(imageB));
        lifecycleVersions[1] = native->Snapshot();
        Hazel::Renderer2D::BeginScene(glm::mat4(1));
        Hazel::Renderer2D::DrawQuad(Transform(0, 0, 1, 1), texture, 1, glm::vec4(1), 2147483647);
        Hazel::Renderer2D::EndScene(); auto resizedB = Submit(resized);
        // A submitted generation is replaced on Bind; cancel this new unsubmitted pass.
        resized->Bind(); resized->Cancel();
        resized->Resize(8, 8); resized->Cancel();
        Reject("invalid resize", [&] { resized->Resize(0, 8); });
        Check(resized->GetSpecification().Width == 8 && resized->GetSpecification().Height == 8,
            "Failed resize changed the published specification");
        metalResized.reset(); resized.reset(); // destroy framebuffer with both old generations pending
        auto destroyed = Hazel::Framebuffer::Create(TargetSpecification(8, 8));
        auto metalDestroyed = std::dynamic_pointer_cast<Hazel::MetalFramebuffer>(destroyed);
        std::weak_ptr<const Hazel::MetalFramebufferAttachments> generationC = metalDestroyed->Snapshot();
        Begin(destroyed, 8, 8, glm::vec4(.125f,.25f,.5f,1));
        destroyed->ClearAttachment(1, std::numeric_limits<int32_t>::min());
        auto transient = Hazel::Texture2D::Create(Specification()); transient->SetData(imageA.data(), sizeof(imageA));
        lifecycleVersions[2] = std::dynamic_pointer_cast<Hazel::MetalTexture2D>(transient)->Snapshot();
        Hazel::Renderer2D::BeginScene(glm::mat4(1));
        Hazel::Renderer2D::DrawQuad(Transform(0, 0, 1, 1), transient, 1, glm::vec4(1), 2002);
        Hazel::Renderer2D::EndScene();
        auto destroyedResult = Submit(destroyed); transient.reset();
        metalDestroyed.reset(); destroyed.reset();
        Check(!generationA.expired() && !generationB.expired() && !generationC.expired(),
            "Framebuffer replacement/destruction released pending attachments");
        auto cancelled = Hazel::Framebuffer::Create(TargetSpecification(8, 8));
        std::weak_ptr<const Hazel::MetalFramebufferAttachments> cancelledGeneration =
            std::dynamic_pointer_cast<Hazel::MetalFramebuffer>(cancelled)->Snapshot();
        cancelled->Bind(); cancelled->Unbind();
        Check(!cancelled->Submit(), "Busy allocator unexpectedly reused by framebuffer");
        cancelled->Cancel(); cancelled.reset();
        Check(cancelledGeneration.expired(), "Cancelled unsubmitted attachments leaked");
        auto retry = Hazel::Framebuffer::Create(TargetSpecification(8, 8));
        Begin(retry, 8, 8, glm::vec4(.5f,.25f,.125f,1)); retry->ClearAttachment(1, -1900000001);
        retry->Unbind();
        Check(!retry->Submit(), "Expected a retained busy retry packet");
        Check(!resizedA->Completed && !resizedB->Completed && !destroyedResult->Completed,
            "Queue gate did not cover framebuffer lifecycle mutations");
        texture->SetData(imageA.data(), sizeof(imageA)); // release the mutable owner's B version
        for (const auto& version : lifecycleVersions) Check(!version.expired(), "Pending texture version released before completion");
        Check(observer->Snapshot().InFlight == 3, "Failed to gate all three M1 allocator slots");
        lifecycleGate();
        Wait(*framebuffer, resizedA); Wait(*framebuffer, resizedB); Wait(*framebuffer, destroyedResult);
        auto retried = retry->Submit();
        Check(bool(retried) && !retried->Completed, "Busy framebuffer packet could not be retried unchanged");
        std::weak_ptr<const Hazel::MetalFramebufferAttachments> retryGeneration =
            std::dynamic_pointer_cast<Hazel::MetalFramebuffer>(retry)->Snapshot();
        retry.reset();
        Check(!retryGeneration.expired(), "Destroying retried framebuffer released unretired attachments");
        auto shader = backend.LoadShader("Renderer2D_Quad"); std::weak_ptr<Hazel::Shader> shaderLifetime = shader; shader.reset();
        Hazel::Renderer2D::Shutdown();
        Hazel::RenderCommand::Shutdown();
        Check(retried->Completed && retryGeneration.expired(), "Shutdown failed to retire retried framebuffer");
        for (const auto& version : lifecycleVersions) Check(version.expired(), "Shutdown did not retire pending texture versions");
        Check(shaderLifetime.expired(), "Shader outlived completed backend cache shutdown");
        Reject("framebuffer factory after shutdown", [&] { Hazel::Framebuffer::Create(TargetSpecification()); });
        Reject("framebuffer resize after shutdown", [&] { framebuffer->Resize(8, 8); });
        Reject("framebuffer bind after shutdown", [&] { framebuffer->Bind(); });
        for (uint32_t y = 0; y < 8; ++y) for (uint32_t x = 0; x < 8; ++x)
            Pixel(*retried, x, y, glm::vec4(.5f,.25f,.125f,1), -1900000001, 1);
        Save(*retried, output / "busy-retry.bin");
        for (const auto& result : {resizedA, resizedB})
            for (uint32_t y = 0; y < result->Height; ++y) for (uint32_t x = 0; x < result->Width; ++x)
            {
                const bool first = result == resizedA;
                if (x >= result->Width / 4 && x < result->Width * 3 / 4 &&
                    y >= result->Height / 4 && y < result->Height * 3 / 4)
                    Pixel(*result, x, y, (first ? colorsA : colorsB)[(y < result->Height / 2 ? 2 : 0) + (x >= result->Width / 2 ? 1 : 0)],
                        first ? -2147483647 : 2147483647, .5f);
                else Pixel(*result, x, y, first ? glm::vec4(.25f,.5f,2,1) : glm::vec4(2,.5f,.25f,1),
                    first ? -2000000003 : 2000000003, 1);
            }
        for (uint32_t y = 0; y < 8; ++y) for (uint32_t x = 0; x < 8; ++x)
        {
            if (x >= 2 && x < 6 && y >= 2 && y < 6)
                Pixel(*destroyedResult, x, y, colorsA[(y < 4 ? 2 : 0) + (x >= 4 ? 1 : 0)], 2002, .5f);
            else Pixel(*destroyedResult, x, y, glm::vec4(.125f,.25f,.5f,1), std::numeric_limits<int32_t>::min(), 1);
        }
        Check(generationA.expired() && generationB.expired() && generationC.expired(),
            "Completed framebuffer generations did not retire");
        Save(*resizedA, output / "resize-a.bin"); Save(*resizedB, output / "resize-b.bin");
        Save(*destroyedResult, output / "destroyed-clear.bin");
        std::cout << "PASS factory framebuffer generations: gated resize/replacement/destruction/cancel, busy retry packet, asymmetric extents and signed clears\n";

        const auto final = observer->Snapshot();
        Check(final.BufferReuses > 100 && final.Errors == 0 && final.InFlight == 0 && final.Submitted == 32 + Renderer2DLineCases().size() + 2 * runtimeFrames && final.Completed == final.Submitted && final.PendingAtShutdown == 1 &&
            final.SlotReuses >= 22 && final.ResourceBytes == 0 && final.PeakResourceBytes <= 320 * 1024 * 1024 && final.PeakAllocatorBytes <= 16 * 1024 * 1024,
            "Frame retirement/reuse/accounting failed");
        std::ofstream metrics(output / "metrics.json");
        metrics << "{\"submitted\":" << final.Submitted << ",\"completed\":" << final.Completed << ",\"slotReuses\":" << final.SlotReuses
            << ",\"orderedQuads\":" << stats.QuadCount << ",\"orderedDraws\":" << stats.DrawCalls << ",\"orderedVertexBytes\":" << stats.UploadedBytes
            << ",\"peakInFlight\":" << final.PeakInFlight << ",\"pendingShutdown\":" << final.PendingAtShutdown << ",\"frameBytes\":" << final.ResourceBytes
            << ",\"peakFrameBytes\":" << final.PeakResourceBytes << ",\"bufferAllocations\":" << final.BufferAllocations << ",\"bufferReuses\":" << final.BufferReuses
            << ",\"peakAllocatorBytes\":" << final.PeakAllocatorBytes << ",\"errors\":" << final.Errors << "}\n";
        std::cout << "PASS: initialized shared Renderer2D quad flush, completed frames including independent line cases and authored world/UI and failure recovery, 12 successive submissions, one unretired shutdown and three gated lifecycle submissions, busy-slot rejection and exact CPU readback\n";
        return 0;
    }
    catch (const std::exception& error) { std::cerr << "FAIL: " << error.what() << '\n'; return 1; }
}
