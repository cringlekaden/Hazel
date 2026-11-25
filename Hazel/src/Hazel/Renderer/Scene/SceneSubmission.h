#pragma once
#include "Hazel/Assets/ImportedAsset.h"
#include "Hazel/Assets/Environment.h"
#include "Hazel/Scene/SceneDefaults.h"
#include <optional>
#include <cstdint>
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
// Accepted semantic CPU inputs, independent of the forward backend and importers.
struct SceneSubmission
{
    SceneCameraView Camera;
    uint64_t SceneIdentity = 0;
    glm::mat4 PreviousViewProjection{1};
    unsigned TemporalPhase = 0; // Zero preserves fixed AO noise; 1..64 temporal sequence.
    ScenePresentation Presentation;
    unsigned AmbientOcclusionView = 0; // 0 shaded, 1 visibility, 2 raw, 3 edges, 4 view depth.
    SurfaceView Debug = SurfaceView::Shaded;
    bool LinearLightingInspection = false; // Scoped FP32 diagnostic readback, not authored.
    Ref<ProjectAssets> Assets;
    std::vector<SubmittedMesh> Meshes;
    std::optional<SubmittedDirectional> Light;
    Ref<const PreparedEnvironment> Environment;
};
} // namespace Hazel
