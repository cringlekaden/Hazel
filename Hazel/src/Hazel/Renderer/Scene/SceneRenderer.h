#pragma once
#include "SceneSubmission.h"
#include "Hazel/Renderer/Resources/Framebuffer.h"
#include <functional>
#include <array>
namespace Hazel
{
class Scene;
struct AmbientOcclusionTiming {
    // GPU completion intervals: depth prep, prefilter, main, denoise, forward/world.
    std::array<double,5> Milliseconds{};
    uint64_t WorkingBytes=0;
    bool Valid=false;
};
class SceneRenderView
{
  public:
    std::shared_ptr<AmbientOcclusionTiming> AmbientTiming=std::make_shared<AmbientOcclusionTiming>();
    bool WarnedUnsupportedAO=false;
    unsigned AmbientOcclusionView = 0; // Transient, never authored.
    SurfaceView Debug = SurfaceView::Shaded; // Private view inspection; never scene/history.
    Ref<Framebuffer> AmbientDepth; // Per-view masked mesh depth preparation.
    Ref<Framebuffer> Radiance; // Optional per-view world target, never authored/history.
    void Reset()
    {
        Radiance.reset();
        AmbientDepth.reset();
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
