#pragma once
#include "Hazel/Assets/ImportedAsset.h"
#include "Hazel/Assets/Environment.h"
#include "Hazel/Scene/SceneDefaults.h"
#include <optional>
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
    BaseNormal
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
    bool LinearLightingInspection = false; // Scoped FP32 diagnostic readback, not authored.
    Ref<ProjectAssets> Assets;
    std::vector<SubmittedMesh> Meshes;
    std::optional<SubmittedDirectional> Light;
    Ref<const PreparedEnvironment> Environment;
};
} // namespace Hazel
