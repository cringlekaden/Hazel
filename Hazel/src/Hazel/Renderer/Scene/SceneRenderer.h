#pragma once
#include "SceneSubmission.h"
#include "Hazel/Renderer/Resources/Framebuffer.h"
#include <functional>
namespace Hazel
{
class Scene;
class SceneRenderView
{
  public:
    SurfaceView Debug = SurfaceView::Shaded; // Private view inspection; never scene/history.
    Ref<Framebuffer> Radiance; // Optional per-view world target, never authored/history.
    void Reset()
    {
        Radiance.reset();
    }
};
class SceneRenderer
{
  public:
    static void ValidateMaterial(const MaterialDefinition &material,
                                 const ScenePresentation &presentation);
    static SceneSubmission Collect(Scene &scene, const SceneCameraView &camera);
    static void Render(const SceneSubmission &frame, SceneRenderView &view,
                       const Ref<Framebuffer> &display, const std::function<void()> &world2D,
                       const std::function<void()> &screenUI);
};
} // namespace Hazel
