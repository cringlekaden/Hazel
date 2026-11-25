#include "hzpch.h"
#include "SceneRenderer.h"
#include "Hazel/Renderer/Drawing/Renderer3D.h"
#include "Hazel/Renderer/Drawing/Renderer2D.h"
#include "Hazel/Renderer/RenderCommand.h"
#include <map>
#include <cmath>
#include <limits>
#ifdef HZ_PLATFORM_MACOS
#include "Platform/macOS/Metal/MetalFramebuffer.h"
#include "Platform/macOS/Metal/MetalRendererAPI.h"
#endif
namespace Hazel
{
namespace {
float Halton(unsigned index,unsigned base) {
    float value=0,fraction=1;
    while(index){fraction/=float(base);value+=fraction*float(index%base);index/=base;}return value;
}
bool ValidCamera(const glm::mat4& matrix) {
    for(unsigned c=0;c<4;++c)for(unsigned r=0;r<4;++r)if(!std::isfinite(matrix[c][r]))return false;
    return std::isfinite(glm::determinant(matrix))&&std::abs(glm::determinant(matrix))>1e-12f;
}
bool CameraCut(const SceneSubmission& frame,const SceneCameraView& previous) {
    const auto& current=frame.Camera;
    if(current.Orthographic!=previous.Orthographic)return true;
    // Near-plane displacement exaggerates ordinary camera motion, especially
    // with a small near clip or a long orthographic depth range. Compare rays
    // at submitted mesh depth instead; disocclusion remains a per-pixel test.
    const auto forward=-glm::normalize(current.ToCamera);
    float referenceDepth=std::numeric_limits<float>::max(),fallbackDepth=referenceDepth;
    for(const auto& draw:frame.Meshes)if(draw.Geometry) {
        const auto& bounds=draw.Geometry->Geometry.Bounds;
        auto centre=glm::vec3(draw.World*glm::vec4(bounds.Min*.5f+bounds.Max*.5f,1));
        float depth=glm::dot(centre-current.Position,forward);
        if(std::isfinite(depth)&&depth>0)referenceDepth=std::min(referenceDepth,depth);
        else {
            float radius=glm::length(glm::vec3(draw.World*glm::vec4(bounds.Max*.5f-bounds.Min*.5f,0)));
            if(std::isfinite(radius)&&radius>0)fallbackDepth=std::min(fallbackDepth,radius);
        }
    }
    if(referenceDepth==std::numeric_limits<float>::max())referenceDepth=fallbackDepth==referenceDepth?1.f:fallbackDepth;
    const auto inverse=glm::inverse(current.ViewProjection);
    auto nearPoint=inverse*glm::vec4(0,0,-1,1);
    if(!std::isfinite(nearPoint.w)||nearPoint.w==0)return true;
    float nearDepth=glm::dot(glm::vec3(nearPoint)/nearPoint.w-current.Position,forward);
    if(std::isfinite(nearDepth))referenceDepth=std::max(referenceDepth,nearDepth);
    for(float x:{-.8f,.8f})for(float y:{-.8f,.8f}) {
        auto sample=inverse*glm::vec4(x,y,current.Orthographic?-1.f:0.f,1);
        if(!std::isfinite(sample.w)||sample.w==0)return true;
        auto ray=glm::vec3(sample)/sample.w-current.Position;
        float depth=glm::dot(ray,forward);
        if(!std::isfinite(depth)||(!current.Orthographic&&depth<=0))return true;
        auto point=current.Orthographic?current.Position+ray+forward*(referenceDepth-depth):current.Position+ray*(referenceDepth/depth);
        auto p=previous.ViewProjection*glm::vec4(point,1);
        if(!std::isfinite(p.w)||p.w<=0||glm::length(glm::vec2(p)/p.w-glm::vec2(x,y))>.5f)return true;
    }
    return false;
}
}

void SceneRenderer::Render(const SceneSubmission &frame, SceneRenderView &view,
                           const Ref<Framebuffer> &display, const std::function<void()> &world2D,
                           const std::function<void()> &screenUI)
{
#ifdef HZ_PLATFORM_MACOS
    if(RendererAPI::GetAPI()==RendererAPI::API::Metal) {
        auto device=static_cast<MetalRendererAPI&>(RenderCommand::Backend()).Device();
        if(view.NativeDevice.lock()!=device){view.Reset();view.NativeDevice=device;}
    } else if(!view.NativeDevice.expired()){view.Reset();view.NativeDevice.reset();}
#endif
    const bool temporal=frame.Presentation.TemporalAA && RendererAPI::GetAPI()==RendererAPI::API::Metal &&
        frame.Presentation.SceneReferred && frame.Debug==SurfaceView::Shaded && frame.AmbientOcclusionView==0;
    if(!temporal) {if(view.Temporal.Submitted)view.Radiance.reset();view.Temporal={};view.Jittered.reset();view.Motion.reset();view.TemporalMetrics->Valid=false;}
    if(frame.Presentation.TemporalAA && RendererAPI::GetAPI()!=RendererAPI::API::Metal && !view.WarnedUnsupportedTAA) {
        HZ_CORE_WARN("TAA request inactive: full-resolution temporal resolve requires Metal 4");view.WarnedUnsupportedTAA=true;
    }
    SceneSubmission sample,motion;
    glm::vec2 jitter(0);
    bool historyValid=false;
    std::shared_ptr<std::atomic<bool>> submitted;
    if(temporal) {
        sample=frame;motion=frame;
        if(!display || !ValidCamera(frame.Camera.ViewProjection) ||
            !std::isfinite(glm::dot(frame.Camera.Position,frame.Camera.Position)) ||
            !std::isfinite(glm::dot(frame.Camera.ToCamera,frame.Camera.ToCamera)) || glm::length(frame.Camera.ToCamera)<.5f) {view.Temporal={};throw std::runtime_error("TAA requires a finite invertible camera and explicit view target");}
        const auto& extent=display->GetSpecification();auto& state=view.Temporal;
        historyValid=state.Submitted && state.Submitted->load() && state.Width==extent.Width && state.Height==extent.Height &&
            state.Previous.SceneIdentity==frame.SceneIdentity && state.Previous.Assets==frame.Assets &&
            state.Previous.Presentation==frame.Presentation && !CameraCut(frame,state.Previous.Camera);
        if(!historyValid)state.Phase=0;
        jitter={Halton(state.Phase%64+1,2)-.5f,Halton(state.Phase%64+1,3)-.5f};
        glm::mat4 offset(1);offset[3][0]=2*jitter.x/float(extent.Width);offset[3][1]=2*jitter.y/float(extent.Height);
        sample.Camera.ViewProjection=offset*frame.Camera.ViewProjection;
        sample.TemporalPhase=state.Phase%64+1;
        motion.PreviousViewProjection=historyValid?state.Previous.Camera.ViewProjection:frame.Camera.ViewProjection;
        std::map<std::pair<uint64_t,int>,const SubmittedMesh*> previous;
        if(historyValid)for(const auto& old:state.Previous.Meshes)if(old.Identity)previous.emplace(std::make_pair(old.Identity,old.Picking),&old);
        for(auto& draw:motion.Meshes) {
            draw.PreviousValid=false;
            auto old=previous.find({draw.Identity,draw.Picking});
            if(old!=previous.end() && draw.Geometry==old->second->Geometry && draw.Materials==old->second->Materials) {
                draw.PreviousWorld=old->second->World;draw.PreviousValid=true;
            }
        }
        submitted=std::make_shared<std::atomic<bool>>(false);
    }
    if(frame.AmbientOcclusionView>4)throw std::runtime_error("Unsupported ambient-occlusion inspection");
    if(!frame.Presentation.AmbientOcclusion.Enabled || frame.Meshes.empty())view.AmbientTiming->Valid=false;
    if (unsigned(frame.Debug) > unsigned(SurfaceView::NormalHorizon))
        throw std::runtime_error("Unsupported surface inspection");
    if (frame.LinearLightingInspection && (frame.Debug < SurfaceView::Direct || frame.Debug > SurfaceView::NormalHorizon))
        throw std::runtime_error("Linear lighting inspection requires a lighting-term view and a float target");
    if (frame.LinearLightingInspection && (!display || display->GetSpecification().Attachments.Attachments.empty() ||
        display->GetSpecification().Attachments.Attachments[0].TextureFormat != FramebufferTextureFormat::RGBA32F))
        throw std::runtime_error("Linear lighting inspection requires an explicit FP32 color target");
    if (RendererAPI::GetAPI() == RendererAPI::API::Metal)
    {
        if (!display)
            throw std::runtime_error("Metal scene requires an explicit view target");
        RenderCommand::StateScope scope;
        auto raster = RenderCommand::GetRasterState();
        raster.DepthTest = raster.DepthWrite = raster.Blend = true;
        raster.Compare = DepthCompare::Less;
        raster.ColorMask = raster.PickingMask = glm::bvec4(true);
        raster.ClipEnabled = false; raster.Cull = CullFace::None; raster.Clockwise = false;
        RenderCommand::SetRasterState(raster);
        try
        {
            const bool ao=frame.Presentation.AmbientOcclusion.Enabled && !frame.Meshes.empty();
            if(ao) {
                auto size=display->GetSpecification();
                size.Attachments={FramebufferTextureFormat::DEPTH32F};
                if(!view.AmbientDepth)view.AmbientDepth=Framebuffer::Create(size);
                else if(view.AmbientDepth->GetSpecification().Width!=size.Width || view.AmbientDepth->GetSpecification().Height!=size.Height)
                    view.AmbientDepth->Resize(size.Width,size.Height);
                display->Unbind();view.AmbientDepth->Bind();view.AmbientDepth->Clear(glm::vec4(0));
                Renderer3D::Render(temporal?sample:frame,false,true);view.AmbientDepth->Unbind();
            } else view.AmbientDepth.reset();
            auto prepareAO=[&](const Ref<Framebuffer>& target) {
#ifdef HZ_PLATFORM_MACOS
                if(ao) std::static_pointer_cast<MetalFramebuffer>(target)->PrepareAmbientOcclusion(view.AmbientDepth,temporal?sample.Camera:frame.Camera,frame.Presentation.AmbientOcclusion,frame.AmbientOcclusionView,view.AmbientTiming,sample.TemporalPhase);
#else
                (void)target;
#endif
            };
            if (frame.Presentation.SceneReferred && frame.Debug == SurfaceView::Shaded && (!ao || frame.AmbientOcclusionView==0))
            {
                auto size = display->GetSpecification();
                if (!view.Radiance) view.Radiance = Framebuffer::Create(size);
                else if (view.Radiance->GetSpecification().Width != size.Width || view.Radiance->GetSpecification().Height != size.Height)
                    view.Radiance->Resize(size.Width, size.Height);
                display->Cancel();
                if(temporal) {
                    auto ensure=[&](Ref<Framebuffer>& target) {
                        if(!target)target=Framebuffer::Create(size);
                        else if(target->GetSpecification().Width!=size.Width || target->GetSpecification().Height!=size.Height)target->Resize(size.Width,size.Height);
                    };
                    ensure(view.Jittered);ensure(view.Motion);
#ifdef HZ_PLATFORM_MACOS
                    std::static_pointer_cast<MetalFramebuffer>(view.Jittered)->SetTemporalProducer();
                    std::static_pointer_cast<MetalFramebuffer>(view.Motion)->SetTemporalProducer();
#endif
                    view.Jittered->Bind();view.Jittered->Clear(frame.Presentation.Background);
                    prepareAO(view.Jittered);Renderer3D::Render(sample,true);view.Jittered->Unbind();
                    view.Motion->Bind();view.Motion->Clear(glm::vec4(0));Renderer3D::Render(motion,false,false,true);view.Motion->Unbind();
                    view.Radiance->Bind();
#ifdef HZ_PLATFORM_MACOS
                    glm::mat4 direction(1);direction[3]=glm::vec4(-frame.Camera.Position,0);
                    auto skyMotion=motion.PreviousViewProjection*direction*glm::inverse(frame.Camera.ViewProjection);
                    const unsigned skyMode=frame.Camera.Orthographic?
                        (historyValid && frame.Camera.ToCamera==view.Temporal.Previous.Camera.ToCamera?2u:0u):1u;
                    std::static_pointer_cast<MetalFramebuffer>(view.Radiance)->ResolveTemporal(view.Jittered,view.Motion,jitter,historyValid,view.TemporalDebug,skyMotion,skyMode,submitted,view.TemporalMetrics);
#endif
                } else {
                    view.Radiance->Bind();view.Radiance->Clear(frame.Presentation.Background);
                    prepareAO(view.Radiance);Renderer3D::Render(frame,true);
                }
                auto worldState = raster;
                RenderCommand::SetRasterState(worldState);
                world2D();
                view.Radiance->Unbind();
                display->Bind();
                view.Radiance->CompositeWorldTo(display, frame.Presentation.Exposure);
            }
            else {
                view.Radiance.reset();
                if(ao) {
#ifdef HZ_PLATFORM_MACOS
                    auto target=std::static_pointer_cast<MetalFramebuffer>(display);
                    if(frame.AmbientOcclusionView)target->Resume(glm::vec4(frame.AmbientOcclusionView==3?0.f:1.f));
                    else target->Resume();
#endif
                }
                prepareAO(display);
                Renderer3D::Render(frame, false);
                RenderCommand::SetRasterState(raster);
                world2D();
            }
            screenUI();
            if(temporal) {
                auto& state=view.Temporal;state.Previous=frame;state.Submitted=submitted;
                state.Jitter=jitter;state.JitteredViewProjection=sample.Camera.ViewProjection;
                state.Width=display->GetSpecification().Width;state.Height=display->GetSpecification().Height;++state.Phase;
            }
        }
        catch (...)
        {
            view.Temporal={};
            Renderer2D::CancelScene();
            if(view.Jittered)view.Jittered->Cancel();
            if(view.Motion)view.Motion->Cancel();
            if (view.AmbientDepth) view.AmbientDepth->Cancel();
            if (view.Radiance) view.Radiance->Cancel();
            display->Cancel();
            throw;
        }
        return;
    }
    if(frame.Presentation.AmbientOcclusion.Enabled && !view.WarnedUnsupportedAO) {
        HZ_CORE_WARN("XeGTAO request inactive: OpenGL has no ambient-occlusion compute implementation");
        view.WarnedUnsupportedAO=true;
    }
    RenderCommand::StateScope scope;
    auto raster = RenderCommand::GetRasterState();
    raster.ClipEnabled = false;
    raster.SRGB = false;
    raster.SeamlessCubes = true;
    raster.DepthTest = true;
    raster.DepthWrite = true;
    raster.Compare = DepthCompare::Less;
    raster.Cull = CullFace::None;
    raster.Clockwise = false;
    raster.Blend = true;
    raster.ColorMask = raster.PickingMask = glm::bvec4(true);
    RenderCommand::SetRasterState(raster);
    struct WorldBinding
    {
        Ref<Framebuffer> Pending;
        ~WorldBinding()
        {
            if (Pending)
                Pending->Unbind();
        }
        void Finish()
        {
            if (Pending)
            {
                Pending->Unbind();
                Pending.reset();
            }
        }
    } worldBinding;
    const bool hdr = frame.Presentation.SceneReferred && frame.Debug == SurfaceView::Shaded;
    if (hdr)
    {
        if (!display)
            throw std::runtime_error(
                "Scene-referred rendering requires an explicit host view target");
        auto size = display->GetSpecification();
        if (!view.Radiance || view.Radiance->GetSpecification().Width != size.Width ||
            view.Radiance->GetSpecification().Height != size.Height)
        {
            size.Samples = 1;
            size.Attachments = {FramebufferTextureFormat::RGBA32F,
                                FramebufferTextureFormat::RED_INTEGER,
                                FramebufferTextureFormat::Depth};
            view.Radiance = Framebuffer::Create(size);
        }
        view.Radiance->Bind();
        worldBinding.Pending = view.Radiance;
        RenderCommand::SetClearColor(frame.Presentation.Background);
        RenderCommand::Clear();
        view.Radiance->ClearAttachment(1, -1);
    }
    else
        view.Radiance.reset();
    Renderer3D::Render(frame, hdr);
    raster = RenderCommand::GetRasterState();
    raster.Cull = CullFace::None;
    raster.Clockwise = false;
    raster.Blend = true;
    RenderCommand::SetRasterState(raster);
    world2D();
    if (hdr)
    {
        worldBinding.Finish();
        display->Bind();
        view.Radiance->CompositeWorldTo(display, frame.Presentation.Exposure);
    }
    screenUI();
}
} // namespace Hazel
