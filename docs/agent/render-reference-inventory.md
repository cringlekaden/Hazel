# Rendering Reference Inventory and Implementation Roadmap

**Date:** 2026-11-08  
**Branch:** `agent/render-reference` (worktree on `agent/render-reference`)  
**Engine Version:** Hazel fork (based on TheCherno's public Hazel, commit `a4c18a7` baseline)  
**Target Platform:** Apple Silicon macOS 27.0+, OpenGL 4.1 core

---

## Executive Summary

The Hazel engine has **partial 3D/PBR/IBL infrastructure** with demonstrated functionality but significant gaps preventing a complete, production-ready rendering reference scene. The existing codebase includes:

- **Implemented:** Forward PBR shading, IBL (image-based lighting) with GGX prefiltered mips and BRDF LUT, basic mesh/material system, scene graph with ECS components, camera system (perspective/orthographic), and a working 2D renderer.
- **Partial/Incomplete:** Mesh import (stage-2 only, no glTF), material system (V1, no editor integration), environment preparation (offline baking exists but requires manual invocation).
- **Absent:** Native mesh authoring, material editor panels, scene environment inspector, mesh import pipeline, lighting inspector, runtime session 3D support in Nutella.

This document details the architecture, API surfaces, build/test infrastructure, and a phased roadmap to create a deterministic reference scene for regression testing.

---

## 1. 3D Rendering Architecture

### 1.1 Renderer Pipeline

The rendering pipeline is organized in a layered fashion:

```
Host (EditorLayer/Nutella)
  └─ RuntimeSession/Scene
      └─ SceneCollection (CPU asset resolution, transform resolution)
          └─ SceneSubmission (typed CPU data, no GL IDs)
              └─ Renderer3D::Render()
                  ├─ Environment background (sky shader)
                  └─ Mesh forward pass (ForwardPBR.glsl)
                      ├─ Opaque/masked meshes with depth/ID writes
                      └─ Debug/inspection modes (optional)
```

Key components:
- **SceneSubmission**: CPU-only submission containing camera (VP matrix, position, orthographic flag), light (direction, incident radiance), environment reference, and mesh instances (geometry ref, world transform, normal transform, material slots, picking ID, handedness).
- **Renderer3D**: Static mesh forward renderer consuming SceneSubmission. Handles environment background draw, material-texture bindings, indexed draw dispatch. Validates GPU capability requirements (8 texture slots, 5 vertex attributes, 7 uniform bindings, 144-byte uniform blocks).
- **ForwardPBR.glsl**: Single-scattering GGX model with Schlick Fresnel, Lambert diffuse, correlated Smith visibility. Supports diagnostic inspection modes via debug flags.
- **EnvironmentResources**: GPU-realized cubemaps (background, irradiance, specular) + BRDF LUT (2D texture). Prepared offline via EnvironmentBake tool.

### 1.2 Shader Pipeline

The shader compilation pipeline is:
1. Source `.glsl` → `shaderc` validation/reflection
2. SPIR-V generation via `shaderc`
3. SPIR-V → GLSL 410 via `SPIRV-Cross`
4. Runtime shader cache keyed by recipe/version

Available shaders in `Hazel/Resources/shaders/`:
- `Renderer2D_Quad.glsl`, `Renderer2D_Line.glsl`, `Renderer2D_Circle.glsl`, `Renderer2D_Text.glsl` (2D drawing)
- `ForwardPBR.glsl` (3D PBR forward pass)
- `EntityOutline.glsl`, `WorldPresentation.glsl`, `DisplayOutput.glsl`
- `EnvironmentSky.glsl` (background skybox)

---

## 2. PBR and Image-Based Lighting Status

### 2.1 PBR Implementation Status

| Feature | Status | Evidence/Notes |
|---------|--------|----------------|
| Metallic/Roughness shading | ✅ Implemented | ForwardPBR.glsl uses GGX/Trowbridge-Reitz, Schlick Fresnel, Lambert diffuse. `F0 = mix(0.04, baseColor, metallic)`. Roughness floor 0.045, alpha = r². |
| Direct lighting | ✅ Implemented | Single directional light with normalized world-local -Z travel direction. Incident radiance in `ViewData::Incident`. |
| Specular IBL | ✅ Implemented | GGX/NoL convolution with V=N, unblurred mip 0, perceptual roughness k/(levels-1). Mip LOD interpolation in FP32. |
| Diffuse IBL | ✅ Implemented | Irradiance cubemap with `/π` division once. |
| BRDF LUT | ✅ Implemented | 128² RG32F LUT, 4096 samples, x=NoV, y=roughness, channels A/B reconstruct `F0*A+B`. |
| AO | ✅ Implemented | Occlusion strength attenuates indirect terms only (diffuse/specular IBL). |
| Tone mapping | ✅ Implemented | Khronos PBR Neutral curve, fixed EV 0 exposure. |
| Rough metals darkening | ✅ Expected | Single-scattering model energy loss is correct behavior. |

### 2.2 IBL Implementation Status

| Feature | Status | Evidence/Notes |
|---------|--------|----------------|
| Environment source decoding | ✅ Implemented | Radiance HDR via `stb_image`, linear float RGB, 64 MiB/8192×4096 limits. |
| Offline bake (EnvironmentBake) | ✅ Implemented | Native tool: `HazelProject prepare-environment PROJECT.hproj ASSET.henv`. Produces `.hlight` artifacts (cubemaps, LUT). |
| Prepared environment storage | ✅ Implemented | `.henv` asset with source reference, preparation profile, yaw/gain. `.himport` descriptor for accepted generation. |
| Runtime IBL consumption | ✅ Implemented | `Renderer3D::Render()` binds irradiance/specular/BRDF textures. Environment rotation via yaw lookup. |
| LUT/recipe validation | ✅ Partially implemented | CPU checks pass; shader-side validation exists. |
| Multi-scattering | ❌ Absent | Single-scattering only. |
| Local reflection probes | ❌ Absent | Global IBL only. |
| Dynamic environment updates | ❌ Absent | Manual bake/prepare required. |
| EXR support | ❌ Absent | Radiance HDR only. |

### 2.3 Environment Bake Evidence

From Stage 4 documentation (`docs/design/3d-environment-stage4.md`):
- **Default profile**: 256 background faces, 128 specular faces (8 mips), 32 irradiance faces, 1024 convolution samples
- **Memory**: ~44.89 MiB peak resident, ~6.2 MiB accepted artifacts
- **Bake time**: 1.332 s cold, 0.934 s cached (M4)
- **Accuracy**: LUT max error 0.00435928, photographic prefilter error 0.017962
- **Validation**: CPU furnace integration, analytic axis/lobe endpoint checks pass

---

## 3. Scene, Material, Mesh, Asset, Camera, Lighting APIs

### 3.1 Scene Graph (Components.h)

**Core components:**
```cpp
struct TransformComponent {
    glm::vec3 Translation, Rotation, Scale;  // Radians
    glm::mat4 GetTransform() const;          // T*R*S
};

struct IDComponent { UUID ID; };
struct TagComponent { std::string Tag; };
```

**Rendering components:**
```cpp
struct DirectionalLightComponent : DirectionalLight {
    // DirectionalLight: ToLight (normalized -Z), Incident (radiance)
};

struct MeshRendererComponent : MeshBinding {
    // MeshBinding: Mesh (asset path), Materials (slot paths)
    // No stage-2 drawing; authored references only
};

struct CameraComponent {
    SceneCamera Camera;
    bool Primary = true;
    bool FixedAspectRatio = false;
};
```

**Scene operations:**
- `Scene::GetAllEntitiesWith<Component>()`
- `Entity::AddComponent<T>()`, `Entity::GetComponent<T>()`
- `Scene::SetWorldTransform(entity, transform)`, `Scene::ValidatePhysics()`
- `Scene::CopyForScene()` (for copy/prefab)
- `Scene::Serialize()`, `Scene::Deserialize()`

### 3.2 Material System (Material.h)

**Material models:**
```cpp
enum class MaterialModel { MetallicRoughness, Unlit };
enum class MaterialAlpha { Opaque, Mask };
enum class MaterialTexture { BaseColor, MetallicRoughness, Normal, Occlusion, Emissive };
```

**MaterialDefinition:**
```cpp
struct MaterialDefinition {
    std::string Name;
    MaterialModel Model;
    glm::vec4 BaseColor;         // Linear RGB / straight alpha
    glm::vec3 Emissive;          // Linear nonnegative
    float Metallic, Roughness, NormalScale, OcclusionStrength, AlphaCutoff;
    MaterialAlpha Alpha;
    bool DoubleSided;
    std::array<MaterialImage, 5> Textures;  // Image path, Min/Mag filters, Wrap
};
```

**File I/O:**
- `MaterialDefinition ReadMaterial(const std::filesystem::path &file)`
- `std::string WriteMaterialText(const MaterialDefinition &data)`
- `MaterialDefinition Validate()` throws on invalid values

**Texture encoding:**
- BaseColor, Emissive: SRGB (decode before filtering/shading)
- MetallicRoughness, Normal, Occlusion: Linear
- `MaterialTextureEncoding(MaterialTexture slot)` returns encoding

### 3.3 Mesh System (ImportedAsset.h)

```cpp
struct MeshBinding {
    std::filesystem::path Mesh;       // .hmesh asset path
    std::vector<std::filesystem::path> Materials;  // .hmat paths per slot
    bool operator==(const MeshBinding &b) const;
};
```

**Geometry format:**
- Vertex layout: Position, Normal, Tangent (vec4), UV0, Color (vec4) — ~64 bytes/vertex
- Indices: uint32, material-scoped
- Local bounds: per-primitive

**Mesh assets:**
- `.hmesh`: geometry definition
- `.hgeom`: generated geometry artifact (per generation)
- `.himport`: import manifest with source key mapping

### 3.4 Asset System (ProjectAssets.h)

```cpp
class ProjectAssets {
    Ref<Texture2D> Texture(const Image &image, const TextureSpecification &spec);
    Ref<MeshGeometry> RealizeMesh(const MeshBinding &binding);
    Ref<EnvironmentResources> RealizeEnvironment(const EnvironmentReference &ref);
    void Refresh();  // Epoch-based reload
};
```

**Asset roots:**
- CPU: `ProjectAssets` owns accepted generations
- GPU: Renderer caches keyed by generation/usage
- No universal asset database; per-project root ownership

### 3.5 Camera System

```cpp
class Camera {
    glm::mat4 m_Projection;
    const glm::mat4& GetProjection() const;
};

class OrthographicCamera : public Camera {
    float m_Left, m_Right, m_Bottom, m_Top;
    float m_ZNear, m_ZFar;
    // View matrix derived from logical/pixel extents
};

class PerspectiveCamera : public Camera {
    float m_FOV, m_Aspect;
    float m_ZNear, m_ZFar;
};
```

**SceneCamera (component):**
```cpp
struct SceneCamera {
    glm::mat4 ViewProjection;
    glm::vec3 Position;
    bool Orthographic;
    glm::vec4 ToCamera;
};
```

### 3.6 Lighting System

```cpp
class DirectionalLight {
    glm::vec3 ToLight;      // Normalized direction (world-local -Z)
    glm::vec3 Incident;     // Incident radiance (linear RGB)
    bool Enabled;
};
```

---

## 4. Example Project Structure and Authoring Workflows

### 4.1 Existing Examples

| Example | Description | 3D Support |
|---------|-------------|------------|
| `Sandbox` | UI tutorial (5 pages) | ❌ 2D only |
| `LastLightkeeper` | Island adventure | ❌ 2D only |
| `MeadowRun` | Garden game with physics | ❌ 2D only |
| `Skybound` | Scrolling flight | ❌ 2D only |
| `SceneTransitions` | Scene transitions demo | ❌ 2D only |
| `RenderingReference` | **3D diagnostic project** | ✅ Partial (see below) |
| `RenderingShowcase` | Art direction demo | ⚠️ Uses IBL, but not reference-quality |

### 4.2 RenderingReference Structure

```
examples/RenderingReference/
├── RenderingReference.hproj                    # Main project
├── RenderingReference-Perspective.hproj        # Perspective camera
├── RenderingReference-Direct-PerspectivePBR.hproj  # Direct PBR
├── RenderingReference-Direct-OrthographicPBR.hproj  # Ortho PBR
├── RenderingReference-IBL-Combined.hproj       # IBL + direct
├── RenderingReference-IBL-Photo.hproj          # IBL with HDR
├── RenderingReference-IBL-DirectOnly.hproj     # Direct only
├── RenderingReference-IBL-IBLOnly.hproj        # IBL only
├── RenderingReference-IBL-AxesYaw90.hproj      # IBL yaw 90°
├── RenderingReference-IBLQuality-Photo.hproj   # IBL quality test
├── Assets/
│   ├── Scenes/
│   │   ├── Core_v1-*.hazel                    # 2D reference
│   │   ├── Direct_v1-*.hazel                  # Direct PBR cases
│   │   └── IBL_v1/IBLQuality_v2-*.hazel       # IBL cases
│   ├── Prepared/                              # Offline-baked environments
│   │   └── BRDF/                              # LUT artifacts
│   ├── Native/                                # Native mesh assets
│   │   └── Direct_v1/                         # .hmesh, .hmat, .himport
│   └── Reference/                             # Reference descriptors
│       ├── Core_v1.json                       # Camera/scene metadata
│       ├── Direct_v1/                          # Materials, outputs.json
│       └── IBL_v1/                             # conditions.yaml
└── README.md                                  # Project documentation
```

**Authoring workflow:**
1. Create project via `hazel.py new-project` (currently 2D only)
2. Manually author scenes in Hazelnut (`.hazel` files)
3. Bake environments via `HazelProject prepare-environment`
4. Import GLB via `HazelProject import-glb` (stage-2, not in examples)

### 4.3 New Project Creation

```bash
python3 scripts/hazel.py new-project --name "MyProject" \
    --identifier "myproject" \
    --destination "/path/to/MyProject" \
    --build-scripts
```

Current templates:
- **2D starter**: Orthographic camera, sprites, text, screen UI
- **3D starter**: Not yet implemented (requires Stage 1 completion)

---

## 5. macOS Build, Test, and Application Launch Commands

### 5.1 Build Commands

```bash
# Generate Xcode workspace
python3 scripts/hazel.py generate --tests

# Build Debug configuration (Hazelnut, Nutella, SpriteAssetAudit)
python3 scripts/hazel.py build --config Debug --tests

# Build Release configuration
python3 scripts/hazel.py build --config Release --tests

# Build specific targets
python3 scripts/hazel.py build --config Debug --targets Hazelnut Nutella
```

### 5.2 Test Commands

```bash
# Run full test suite (native OpenGL on macOS)
python3 scripts/hazel.py test --config Debug --profile native

# Run with GL4.1 forcing (for regression verification)
python3 scripts/hazel.py test --config Release --profile gl41

# CPU-only tests (no graphics)
python3 scripts/hazel.py test --config Debug --profile native --suite cpu

# Run specific test
python3 scripts/hazel.py test --config Debug --only Renderer2DSmoke

# Run migration tests (authoring/regressions)
python3 scripts/hazel.py test --config Release --profile gl41 --only MigrationSceneSmoke
```

**Available test suites:**
- `all` (default): Full suite including graphics/editor
- `cpu`: Display-free subset

**Test profiles:**
- `native`: Real Apple OpenGL driver (macOS only)
- `gl41`: Force GL 4.1 / Mesa on Linux
- `software`: llvmpipe on Linux/Windows

### 5.3 Application Launch

```bash
# Launch Hazelnut (editor)
python3 scripts/hazel.py run Hazelnut \
    --config Debug \
    --project examples/RenderingReference/RenderingReference.hproj

# Launch Nutella (runtime)
python3 scripts/hazel.py run Nutella \
    --config Release \
    --project examples/RenderingReference/RenderingReference-Perspective.hproj

# Launch with specific project (Nutella)
python3 scripts/hazel.py run Nutella --config Release --project path/to/MyProject.hproj
```

### 5.4 Environment Bake (Offline)

```bash
# Prepare environment from source to accepted generation
./bin/Debug-macos-arm64/HazelProject/HazelProject \
    prepare-environment examples/RenderingReference/RenderingReference.hproj \
    Assets/Prepared/IBL_v1/StudioSmall09.henv

# Cancel preparation
./bin/Debug-macos-arm64/HazelProject/HazelProject \
    cancel-environment examples/RenderingReference/RenderingReference.hproj
```

### 5.5 GL Info

```bash
# Query OpenGL capabilities
glxinfo -B | grep -E "OpenGL|GL_VERSION"
```

---

## 6. GPU, Shader, Pixel, and Rendering Regression Test Infrastructure

### 6.1 Test Infrastructure

Tests are organized in `tests/migration/`:
- `RendererFeaturesSmoke.cpp` — Basic renderer functionality
- `IBLSurfaceReference.h/cpp` — IBL mathematical correctness
- `IBLQualityRenderingChecks.h/cpp` — IBL quality validation
- `IBLReferenceMath.h/cpp` — Analytic IBL reference
- `EnvironmentBakeChecks.h/cpp` — Bake validation
- `UIStyleSceneChecks.h/cpp` — UI rendering
- `ImportCookingChecks.h/cpp` — Mesh import validation
- `RenderingShowcaseChecks.h/cpp` — Showcase rendering

### 6.2 Existing Regression Tests

**2D/Renderer tests:**
- `Renderer2DSmoke`
- `Renderer2DCircleSmoke`
- `Renderer2DTextSmoke`
- `Renderer2DSpriteSmoke`
- `Renderer2DLineSmoke`

**IBL tests:**
- `IBLSurfaceReference` — FP32 surface isolation
- `IBLQualityRenderingChecks` — Runtime IBL validation
- `IBLQualityLobeChecks` — Prefilter lobe validation
- `IBLReferenceMath` — Analytic reference integration
- `IBLReferenceChecks` — Full reference validation
- `IBLSurfaceReferenceCLI` — CLI-based validation

**Authoring tests:**
- `MigrationSceneSmoke` / `MigrationSceneGPUSmoke`
- `MigrationEditorSmoke`
- `MigrationRuntimeSessionSmoke`
- `MigrationGameUIModelSmoke`
- `MigrationTypographySmoke`
- `MigrationFontSmoke`
- `ImportCookingChecks`

### 6.3 Evidence from Existing Tests

**Stage 4 qualification (from `docs/design/3d-environment-stage4.md`):**
- **CPU tests**: 18/18 Release + 18/18 Debug on M4 OpenGL 4.1
- **GPU tests**: M4 OpenGL 4.1, 18/18 Release + 18/18 Debug
- **Package tests**: Content-only Nutella with source/producer resources denied

**IBL quality metrics:**
- LUT max error: 0.00435928
- Photographic prefilter error: 0.017962 (budget: 0.02)
- Split-sum/diffuse approximation: 0.0647609 (separately reported)
- Fview diffuse furnace deficit at grazing: 0.719418 (expected)

---

## 7. Deterministic Reference Image Capture

### 7.1 Fixed Camera and Output

The RenderingReference project documents:
- **Resolution**: 1280×720 logical pixels, physical Retina extent separately recorded
- **Camera**: Named authored cameras (Perspective, Orthographic, SurfaceDetail)
- **Exposure**: EV 0, Neutral tone mapping, no auto-exposure
- **Output**: RGBA8 encoding, no postprocess

### 7.2 Scene Hashing and Metadata

Reference scenes record:
- Entity/camera UUIDs
- Authored matrices/material/light/environment values
- Source/accepted generation/recipe identities
- Fixed clock/seed
- Logical/pixel extents, output formats/encoding
- Commit/configuration, backend/device/driver, effective capabilities

### 7.3 Capture Commands

```bash
# Build Release
python3 scripts/hazel.py build --config Release --tests

# Launch Hazelnut, select scene, capture via screenshot or console output
python3 scripts/hazel.py run Hazelnut \
    --config Release \
    --project examples/RenderingReference/RenderingReference-IBL-Photo.hproj

# Screenshot (manual via editor, or console-based)
# Editor captures via ImGui viewport or dedicated capture tool
```

### 7.4 Reference Image Locations

- **Direct_v1**: `examples/RenderingReference/Assets/Reference/Direct_v1/outputs.json`
- **IBL_v1**: `examples/RenderingReference/Assets/Reference/IBL_v1/conditions.yaml`
- **IBLQuality_v2**: Not yet documented

---

## 8. Missing Functionality Blocking Reference Scene

### 8.1 Critical Gaps

| Missing Feature | Impact | Priority |
|-----------------|--------|----------|
| **Native mesh authoring** | Cannot author 3D geometry without import | High |
| **Material editor panel** | No UI for material parameters | High |
| **Scene environment inspector** | Cannot edit yaw/gain/environment in scene | High |
| **Lighting inspector** | Cannot adjust light intensity/direction | Medium |
| **Mesh import pipeline** | Cannot import glTF models | High |
| **Runtime Session 3D support** | Nutella lacks HDR view pipeline | High |
| **Scene-referred presentation settings** | No scene-level exposure/tone mapping control | Medium |
| **Orthographic PBR** | Only perspective camera tested for PBR | Low |

### 8.2 Partially Implemented Features

| Partial Feature | Status | Notes |
|-----------------|--------|-------|
| Mesh system | ✅ Core API exists | No editor integration, no import |
| Material system | ✅ Core API exists | No editor integration |
| Environment system | ✅ Offline bake exists | Manual tool invocation required |
| Camera system | ✅ Perspective/Orthographic | No editor integration for runtime camera |
| Forward PBR | ✅ Implemented | Works, but no inspector |
| IBL | ✅ Implemented | Manual bake required |

---

## 9. Implementation Roadmap

### Milestone 1: Settings, Canonical Color, and Permanent 2D Reference

**Objective:** Establish sound settings/color foundation with fully usable 2D rendering reference.

**Prerequisites:** None (merged Mac baseline)

**Deliverables:**
- ProjectConfig/serializers with new-scene defaults
- Native ProjectCreation/templates for 2D/3D starters
- AuthoringPanel/RenderingSettings integration
- EditorState/camera/gizmo integration
- Scene/DocumentSchema/history updates
- RendererPolicy, Texture/Framebuffer/Renderer2D/UI shader updates
- OpenGL adapters for HDR/float targets
- Canonical linear color schema (display-referred unlit path)
- Core_v1 rendering reference with fixed metadata

**Acceptance Criteria:**
- CPU round trips for settings/color
- Color/UV/picking tests pass
- Positive UI authoring
- Reviewed example comparisons
- Commit: sound settings/color foundation with fully usable 2D

**Files to create/modify:**
- `Hazel/src/Hazel/Project/ProjectConfig.h/cpp` (new-scene defaults)
- `Hazelnut/src/Authoring/AuthoringPanel.cpp` (preset cards)
- `Hazelnut/src/Authoring/RenderingSettings.h/cpp` (scene presentation)
- `docs/agent/render-reference-inventory.md` (this document)

---

### Milestone 2: Format-Neutral Import and Native Asset Authoring

**Objective:** Qualify mesh import pipeline with usable native authoring.

**Prerequisites:** Stage 1 schema/color

**Deliverables:**
- New imported scene/image DTOs (`ImportedSceneData`)
- Common validation/cook/publication service
- Private GLB adapter (TinyGLTF 3.0.1 + MikkTSpace)
- `HazelProject` import CLI integration
- Premake/notices for dependencies
- Components/Scene copy/metadata updates
- ContentBrowser material document/slot Inspector
- Detached prefab generation

**Acceptance Criteria:**
- TinyGLTF compiles on Apple Clang/MSVC/GCC
- Valid and bounded malformed fixtures/sanitizers
- Canonical geometry/materials/detached prefab
- Stable source-key mappings
- Atomic reimport/conflict/cancellation
- Parser-free runtime loading
- Native import/progress UI
- Material create/duplicate/edit/save/assign
- Metadata inspection before rendering
- Commit: independent import pipeline with usable native authoring

**Files to create/modify:**
- `Hazel/src/Hazel/Assets/ImportedSceneData.h/cpp`
- `Hazel/src/Hazel/Assets/GlbAdapter.h/cpp`
- `tools/HazelImport/GLBImporter/main.cpp`
- `Hazel/src/Hazel/Project/HazelProject.cpp` (import CLI)
- `Hazelnut/src/Panels/MaterialPanel.h/cpp` (Inspector)

---

### Milestone 3: Shared Submission and Direct PBR

**Objective:** Real static indexed meshes, direct PBR, scene-referred HDR/exposure/tone mapping.

**Prerequisites:** Stage 2 assets

**Deliverables:**
- SceneRenderer/view and common inputs
- Scene runtime/editor submission
- RuntimeSession view ownership
- Typed RendererAPI state/resources OpenGL adapters
- Mesh/material/light/camera inspectors
- EditorLayer/Nutella output integration
- ForwardPBR shader completion
- Bounds framing, sun/camera/scene controls
- Dirty/history/save/reopen for 3D
- Working direct-lit 3D preset

**Acceptance Criteria:**
- Both hosts consume same saved content
- Orthographic PBR/perspective unlit reference cases work
- CPU BRDF oracle passes
- Actual Apple/GL4.1 checks pass
- Commit: usable direct-PBR editor/runtime checkpoint

**Files to create/modify:**
- `Hazel/src/Hazel/Renderer/Scene/SceneRenderer.h/cpp` (view ownership)
- `Hazel/src/Hazel/Scene/Scene.cpp` (runtime/editor submission)
- `Nutella/src/NutellaApp.cpp` (HDR view pipeline)
- `Hazelnut/src/Panels/SceneHierarchyPanel.h/cpp` (3D gizmos)

---

### Milestone 4: Environment Ownership and Offline HDR/IBL

**Objective:** Complete first PBR+IBL and native authoring.

**Prerequisites:** Stage 3 resources/BRDF recipe

**Deliverables:**
- `.henv` source/generation codecs
- Image normalization service
- Native EnvironmentBake and prepare dispatch
- Cube/LUT upload
- Scene environment settings
- Document/job UI
- IBL inspector (yaw/gain)
- Neutral prepared 3D starter
- DirectOnly/IBLOnly/Combined/Photo reference cases

**Acceptance Criteria:**
- Native select/prepare/progress/cancel/history
- Clear stale/missing diagnostics
- No runtime bake
- Full native project creation (no SDK/bake/GPU)
- Commit: complete first PBR+IBL and native authoring

**Files to create/modify:**
- `Hazel/src/Hazel/Assets/Environment.h/cpp` (codec)
- `tools/HazelImport/Environment/EnvironmentBake.h/cpp` (dispatch)
- `Hazelnut/src/Panels/EnvironmentPanel.h/cpp` (Inspector)

---

### Milestone 5: Runtime, Transitions, and Export Closure

**Objective:** Same saved scenes, assets, screen UI in Nutella.

**Prerequisites:** Stage 4 content

**Deliverables:**
- RuntimeSession readiness/transition/lifetime
- Native metadata audit
- Packaging/inventory/prepare CLI
- Fixture scripts
- RenderingReference package
- Mixed-content export example

**Acceptance Criteria:**
- Nutella renders HDR/PBR scenes
- One project transitions between 2D/HDR/PBR
- Repeated Play/Stop/reload/project switch/teardown
- Export RenderingReference
- Source/producer-denied relocation
- Dependency failures/notices/checksums/signatures
- Commit: standalone first slice and reference-package closure

---

### Milestone 6: Qualify, Measure, and Document

**Objective:** Qualified permanent 2D plus first complete 3D milestone.

**Prerequisites:** Complete slice

**Deliverables:**
- Debug/Release on all targets
- All five 2D examples/tutorials retain positive acceptance
- Actual Apple GL4.1, available older accelerated hardware
- Native authoring/history/lifecycle
- Relocated packages
- Reference metadata/captures
- Quality/effective settings documentation

**Acceptance Criteria:**
- CI passes
- All tests green
- Reference captures verified
- Commit: qualified permanent 2D plus first complete 3D milestone

---

## 10. Important Implementation Decisions

### 10.1 Forward Rendering Only

The first slice uses simple forward rendering:
- One directional light (no shadows)
- Opaque/alpha-mask materials
- Static geometry
- Offline CPU environment baking
- No compute, SSBO, image load/store, bindless textures

### 10.2 Single-Scattering PBR

- Isotropic GGX/Trowbridge-Reitz
- Height-correlated Smith visibility
- Schlick Fresnel
- Lambert diffuse
- No multi-scattering, no ambient occlusion on direct light

### 10.3 Canonical Color Contract

- **Linear RGB** for all authored values (material factors, script colors, UI)
- sRGB textures decode before filtering/shading
- Display-referred unlit bypasses exposure/tone mapping
- Scene-referred applies exposure + Neutral tone mapping
- UI composites in display-linear space

### 10.4 Offline Environment Baking

- CPU-only bake in native tool
- No runtime baking
- Prepared environments are portable artifacts
- Source HDR files are not runtime dependencies

### 10.5 OpenGL 4.1 Baseline

- No Vulkan, Metal, or other backends
- GLSL 410 shaders
- Standard OpenGL state management
- 1280×720 logical resolution

---

## 11. Build and Test Commands (Summary)

```bash
# Initial setup
python3 scripts/hazel.py setup --tests

# Build
python3 scripts/hazel.py build --config Debug --tests
python3 scripts/hazel.py build --config Release --tests

# Test (full suite)
python3 scripts/hazel.py test --config Debug --profile native
python3 scripts/hazel.py test --config Release --profile gl41

# Test (CPU only)
python3 scripts/hazel.py test --config Debug --profile native --suite cpu

# Test (specific test)
python3 scripts/hazel.py test --config Debug --only IBLReferenceChecks

# Launch editor
python3 scripts/hazel.py run Hazelnut --config Debug --project examples/RenderingReference/RenderingReference.hproj

# Launch runtime
python3 scripts/hazel.py run Nutella --config Release --project examples/RenderingReference/RenderingReference-Perspective.hproj

# Environment bake
./bin/Debug-macos-arm64/HazelProject/HazelProject prepare-environment \
    examples/RenderingReference/RenderingReference.hproj \
    Assets/Prepared/IBL_v1/StudioSmall09.henv
```

---

## 12. Known Limitations

- No Metal/Vulkan backends (OpenGL 4.1 only)
- No shadow mapping
- No animated geometry/skinning
- No local reflection probes
- No dynamic environment updates
- No EXR support (Radiance HDR only)
- No transparent mesh/sprite interleaving
- No controller/physical input qualification (Mac)
- No clipboard/mixed-display qualification (Mac)
- No public signing/notarization

---

## 13. Recommended Next Task

**Task:** Implement Milestone 1 — Settings, Canonical Color, and Permanent 2D Reference.

This establishes the foundational color schema and 2D rendering reference before tackling 3D geometry and lighting. The 2D path is lighter and can be verified quickly, providing a stable baseline for subsequent 3D milestones.

**Actions:**
1. Extend `ProjectConfig` with new-scene defaults (projection, presentation, editor view)
2. Implement 2D/3D starter presets in `AuthoringPanel`
3. Update `DocumentSchema` for linear color semantics
4. Create Core_v1 rendering reference scenes with fixed metadata
5. Verify color round trips (sRGB ↔ linear)
6. Build and run existing 2D tests
7. Document all changes in commit

---

## References

- **3D Rendering Plan:** `docs/design/3d-rendering-plan.md`
- **Stage 4 Environment:** `docs/design/3d-environment-stage4.md`
- **IBL Quality Pass:** `docs/design/ibl-quality-pass.md`
- **Follow a Frame:** `docs/follow-a-frame.md`
- **Mac Port:** `docs/design/macos-port.md`
- **Platform Dependencies:** `docs/platforms-and-dependencies.md`

---

*End of document.*
