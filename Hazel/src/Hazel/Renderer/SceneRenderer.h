#pragma once
#include "Hazel/Assets/ImportedAsset.h"
#include "Hazel/Renderer/Framebuffer.h"
#include "Hazel/Renderer/Lighting.h"
#include "Hazel/Scene/SceneDefaults.h"
#include <functional>
#include <optional>
namespace Hazel
{
class Scene;
class ProjectAssets;
enum class SurfaceView
{
    Shaded,
    BaseColor,
    Normal,
    Tangent,
    MetalRoughAO,
    Radiance
};
struct SceneCameraView
{
    glm::mat4 ViewProjection{1};
    glm::vec3 Position{}, ToCamera{0, 0, 1};
    bool Orthographic = false;
};
struct SubmittedMesh
{
    Ref<const MeshAsset> Geometry;
    std::vector<Ref<const MaterialDefinition>> Materials;
    glm::mat4 World{1}, Normal{1};
    int Handedness = 1;
    void SetWorld(const glm::mat4 &world);
    int Picking = -1;
};
struct SubmittedDirectional
{
    glm::vec3 ToLight{}, Incident{};
};
// Accepted semantic CPU inputs, independent of the forward backend and importers.
struct SceneSubmission
{
    SceneCameraView Camera;
    ScenePresentation Presentation;
    SurfaceView Debug = SurfaceView::Shaded;
    Ref<ProjectAssets> Assets;
    std::vector<SubmittedMesh> Meshes;
    std::optional<SubmittedDirectional> Light;
};
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
    static void Shutdown();
    static void ValidateMaterial(const MaterialDefinition &material,
                                 const ScenePresentation &presentation);
    static SceneSubmission Collect(Scene &scene, const SceneCameraView &camera);
    static void Render(const SceneSubmission &frame, SceneRenderView &view,
                       const Ref<Framebuffer> &display, const std::function<void()> &world2D,
                       const std::function<void()> &screenUI);
};
} // namespace Hazel
