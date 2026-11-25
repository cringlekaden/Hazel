#pragma once
#include "SceneSubmission.h"
#include "Hazel/Renderer/Resources/Framebuffer.h"
#include <functional>
#include <array>
#include <cstdint>
#include <memory>
#include <atomic>
namespace Hazel
{
class Scene;
struct AmbientOcclusionTiming {
    // GPU completion intervals: depth prep, prefilter, main, denoise, forward/world.
    std::array<double,5> Milliseconds{};
    uint64_t WorkingBytes=0;
    bool Valid=false;
};
struct TemporalTiming {
    double ResolveMilliseconds=0, ShadingMilliseconds=0, MotionMilliseconds=0;
    uint64_t HistoryBytes=0, Allocations=0, Reuses=0;
    bool Valid=false;
};
struct TemporalViewState {
    SceneSubmission Previous;
    std::shared_ptr<std::atomic<bool>> Submitted;
    uint32_t Width=0, Height=0, Phase=0;
    glm::vec2 Jitter{0};
    glm::mat4 JitteredViewProjection{1};
};
class SceneRenderView
{
  public:
    SceneRenderView() = default;
    SceneRenderView(const SceneRenderView&) = delete;
    SceneRenderView& operator=(const SceneRenderView&) = delete;
    SceneRenderView(SceneRenderView&&) noexcept = default;
    SceneRenderView& operator=(SceneRenderView&&) noexcept = default;
    std::shared_ptr<AmbientOcclusionTiming> AmbientTiming=std::make_shared<AmbientOcclusionTiming>();
    bool WarnedUnsupportedAO=false, WarnedUnsupportedTAA=false;
    unsigned TemporalDebug=0; // 0 resolved, 1 motion, 2 jitter, 3 rejection, 4 contribution, 5 raw HDR.
    TemporalViewState Temporal;
    std::weak_ptr<void> NativeDevice; // Detect native backend recreation without retaining it.
    std::shared_ptr<TemporalTiming> TemporalMetrics=std::make_shared<TemporalTiming>();
    Ref<Framebuffer> Jittered, Motion;
    unsigned AmbientOcclusionView = 0; // Transient, never authored.
    SurfaceView Debug = SurfaceView::Shaded; // Private view inspection; never scene/history.
    Ref<Framebuffer> AmbientDepth; // Per-view masked mesh depth preparation.
    Ref<Framebuffer> Radiance; // Optional per-view world target, never authored/history.
    void Reset()
    {
        Radiance.reset();
        Jittered.reset();Motion.reset();Temporal={};
        TemporalMetrics=std::make_shared<TemporalTiming>();
        WarnedUnsupportedTAA=false;
        AmbientDepth.reset();
        AmbientTiming=std::make_shared<AmbientOcclusionTiming>();
        WarnedUnsupportedAO=false;
    }
};
class SceneRenderer
{
  public:
    static void ValidateMaterial(const MaterialDefinition &material,
                                 const ScenePresentation &presentation);
    static SceneSubmission Collect(Scene &scene, const SceneCameraView &camera, glm::uvec2 physicalExtent = glm::uvec2(0));
    static void Render(const SceneSubmission &frame, SceneRenderView &view,
                       const Ref<Framebuffer> &display, const std::function<void()> &world2D,
                       const std::function<void()> &screenUI);
};
} // namespace Hazel
