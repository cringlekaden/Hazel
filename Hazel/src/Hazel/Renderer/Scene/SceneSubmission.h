#pragma once
#include "Hazel/Assets/ImportedAsset.h"
#include "Hazel/Assets/Environment.h"
#include "Hazel/Scene/SceneDefaults.h"
#include <optional>
#include <cstdint>
#include <memory>
namespace Hazel
{
class ProjectAssets;
enum class SurfaceView
{
    Shaded,
    BaseColor,
    Normal,
    Tangent,
    MetalRoughAO,
    Radiance,
    Direct,
    DiffuseIBL,
    SpecularIBL,
    // Transient FP32 probes for numeric surface inspection. These are not
    // artistic settings or persisted editor inspection choices.
    PrefilteredIBL,
    BRDFLUT,
    SurfaceParameters,
    WorldPosition,
    GeometricNormal,
    BaseNormal,
    NormalHorizon // Raw shading N.V, geometric N.V, effective shading N.V.
};
struct SceneCameraView
{
    glm::mat4 ViewProjection{1};
    glm::vec3 Position{}, ToCamera{0, 0, 1};
    bool Orthographic = false;
    glm::mat4 View{1}, Projection{1}; // Unjittered source matrices; VP is authoritative for direct submissions.
};
struct SubmittedMesh
{
    Ref<const MeshAsset> Geometry;
    std::vector<Ref<const MaterialDefinition>> Materials;
    glm::mat4 World{1}, Normal{1};
    int Handedness = 1;
    void SetWorld(const glm::mat4 &world);
    int Picking = -1;
    uint64_t Identity = 0; // Stable entity UUID; zero rejects object history.
    glm::mat4 PreviousWorld{1};
    bool PreviousValid = false;
};
struct SubmittedDirectional
{
    glm::vec3 ToLight{}, Incident{};
};
struct LocalLightingMetrics
{
    double CollectionMilliseconds=0, UploadMilliseconds=0, CullMilliseconds=0, ForwardMilliseconds=0, ShadowMilliseconds=0;
    uint64_t LightBytes=0, TileBytes=0, Candidates=0, OverflowTiles=0;
    unsigned Count=0, Tiles=0;
    bool Valid=false;
};
struct SubmittedLocalLight
{
    uint64_t Identity = 0;
    glm::vec3 Position{}, Incident{}, Direction{0,0,-1};
    float Range = 10, InnerCos = 1, OuterCos = -1; // OuterCos=-1 denotes a point.
};
// Accepted semantic CPU inputs, independent of the forward backend and importers.
struct SceneSubmission
{
    SceneCameraView Camera;
    uint64_t SceneIdentity = 0;
    glm::mat4 PreviousViewProjection{1};
    unsigned TemporalPhase = 0; // Zero preserves fixed AO noise; 1..64 temporal sequence.
    ScenePresentation Presentation;
    unsigned AmbientOcclusionView = 0; // 0 shaded, 1 visibility, 2 raw, 3 edges, 4 view depth.
    unsigned LocalLightTileSize = 16; // Qualification compares 16/32-pixel tiles.
    bool ForceLocalLightCulling = false; // Qualification of the small-light crossover.
    bool LocalLightCulling = true; // Transient full-loop comparison/profiling control.
    unsigned ShadowView = 0; // Transient CSM inspection; never authored.
    unsigned LocalLightingView = 0; // 1 local radiance, 2 tile occupancy, 3 overflow.
    SurfaceView Debug = SurfaceView::Shaded;
    bool LinearLightingInspection = false; // Scoped FP32 diagnostic readback, not authored.
    Ref<ProjectAssets> Assets;
    std::vector<SubmittedMesh> Meshes;
    std::vector<SubmittedMesh> ShadowCasters; // Same collection; never camera-frustum culled.
    DirectionalShadowData Shadows;
    std::optional<SubmittedDirectional> Light;
    std::vector<SubmittedLocalLight> LocalLights;
    double LightCollectionMilliseconds=0;
    std::shared_ptr<LocalLightingMetrics> LocalMetrics;
    Ref<const PreparedEnvironment> Environment;
};
} // namespace Hazel
