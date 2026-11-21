#include "hzpch.h"
#include "SceneRenderer.h"
#include "Hazel/Renderer/Drawing/Renderer3D.h"
#include "Hazel/Renderer/Drawing/Renderer2D.h"
#include "Hazel/Renderer/RenderCommand.h"
#ifdef HZ_PLATFORM_MACOS
#include "Platform/macOS/Metal/MetalFramebuffer.h"
#endif
namespace Hazel
{
void SceneRenderer::Render(const SceneSubmission &frame, SceneRenderView &view,
                           const Ref<Framebuffer> &display, const std::function<void()> &world2D,
                           const std::function<void()> &screenUI)
{
    if(frame.AmbientOcclusionView>4)throw std::runtime_error("Unsupported ambient-occlusion inspection");
    if(!frame.Presentation.AmbientOcclusion.Enabled)view.AmbientTiming->Valid=false;
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
                display->Cancel();view.AmbientDepth->Bind();view.AmbientDepth->Clear(glm::vec4(0));
                Renderer3D::Render(frame,false,true);view.AmbientDepth->Unbind();
            } else view.AmbientDepth.reset();
            auto prepareAO=[&](const Ref<Framebuffer>& target) {
#ifdef HZ_PLATFORM_MACOS
                if(ao) std::static_pointer_cast<MetalFramebuffer>(target)->PrepareAmbientOcclusion(view.AmbientDepth,frame.Camera,frame.Presentation.AmbientOcclusion,frame.AmbientOcclusionView,view.AmbientTiming);
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
                view.Radiance->Bind();
                view.Radiance->Clear(frame.Presentation.Background);
                prepareAO(view.Radiance);
                Renderer3D::Render(frame, true);
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
                    display->Bind();
                    const auto background=frame.AmbientOcclusionView?glm::vec4(frame.AmbientOcclusionView==3?0.f:1.f):frame.Presentation.Background;
                    display->Clear(background);
                }
                prepareAO(display);
                Renderer3D::Render(frame, false);
                RenderCommand::SetRasterState(raster);
                world2D();
            }
            screenUI();
        }
        catch (...)
        {
            Renderer2D::CancelScene();
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
