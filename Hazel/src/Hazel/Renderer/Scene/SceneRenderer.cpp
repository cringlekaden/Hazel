#include "hzpch.h"
#include "SceneRenderer.h"
#include "Hazel/Renderer/Drawing/Renderer3D.h"
#include "Hazel/Renderer/RenderCommand.h"
namespace Hazel
{
void SceneRenderer::Render(const SceneSubmission &frame, SceneRenderView &view,
                           const Ref<Framebuffer> &display, const std::function<void()> &world2D,
                           const std::function<void()> &screenUI)
{
    if (unsigned(frame.Debug) > 8)
        throw std::runtime_error("Unsupported surface inspection");
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
