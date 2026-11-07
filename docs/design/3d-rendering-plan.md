# First-class 2D and 3D: import, rendering and environment contracts

Audit date: 2026-11-03. Exact audited master:
**`a4c18a7bf5e7dc73508bda308157dcea729ba06a`**
([source tree](https://github.com/cringlekaden/Hazel/tree/a4c18a7bf5e7dc73508bda308157dcea729ba06a)).
This is the normal merge of the qualified Mac port. Product code matches
`631af3b839409f5bc892e65b32c3fdf7d5caae74`; later integration edits are docs only.
[CI 41705072658](https://github.com/cringlekaden/Hazel/actions/runs/41705072658)
passed full Linux/Windows qualification and Mac Debug/Release CPU/service/managed
checks. Local M4 evidence covers actual Apple OpenGL, editor/runtime interaction,
relocated products, all five games, Mono/native closure and ad-hoc signatures.
See [the Mac record](macos-port.md). Clipboard, physical Retina/mixed-display and
controller acceptance remain unexecuted; Developer ID/notarization remain undone.
These are not claims of 3D correctness or performance.

The automatic [post-merge run 41720163554](https://github.com/cringlekaden/Hazel/actions/runs/41720163554)
subsequently passed Linux/Mac but timed out in the Windows Debug native game
fixture at its existing 480-second bound. Its log reached ObservatoryDawn at
22:22:34, five seconds before termination, with no `FAIL` assertion; the qualified
run completed the same fixture in 5m43s with the same Mesa/LLVM driver. Product
tree comparison against `631af3b` is empty. The slowdown's cause is not established;
this newer run is **not** claimed green. No timeout/assertion was weakened and no
rerun was used to hide it. The existing completed qualification remains the code
baseline; Windows CI duration variability is an explicit follow-up limitation.

This revision supersedes `e61e339`'s proposed permanent historical LDR/color path
and its use of a single 3D-view switch. The audited product baseline is unchanged.
This audit changed no engine code, dependencies or settings, installed nothing
and ran no desktop automation. Findings below come from the merged source;
**proposals and numerical budgets are recommendations, not implemented features**.
External implementation reports are identified as such and were not reproduced.

## Intended endpoint and decision scope

Keep **2D and 3D permanent, first-class capabilities** of the same engine. Complete
one new path: import a static glTF model, create/assign a metallic/roughness
material, place and manipulate it in Hazelnut, view it through a perspective
camera with a directional light and HDR environment/IBL, save/reopen, then export
and run the same scene in Nutella. Include a small managed behavior and existing
screen-space UI. PBR and IBL belong to this first milestone; direct-light-only
rendering is an intermediate checkpoint, not the finished milestone.

Recommend **ordinary forward rendering**, one directional light without shadows,
opaque/alpha-mask materials, static geometry and offline CPU environment baking.
Use the existing OpenGL 4.1 backend, Premake and Python command dispatcher. Do not
replace GLFW, Mono, resource ownership, the scene graph or the editor command
system. No Blinn-Phong stage is necessary. Sound shared contracts precede that
rendering work; producing a model image alone does not complete the milestone.

Throughout this plan, **implement now** means the first implementation milestone,
not this documentation run. **Establish a contract now** means define the semantics
or data/lifetime boundary needed to prevent a later fork; add only fields used by
the first implementation. **Defer** means no UI option, registry, unused buffer or
placeholder subsystem is required yet.

| Implement now | Establish a contract now | Defer until justified |
| --- | --- | --- |
| Native project presets/settings, canonical linear color, lightweight unlit 2D, GLB/static meshes, forward PBR, offline HDR/IBL, native authoring and export | Format-neutral imported data, common scene submission, environment source/generation ownership, capability/request resolution, stable Rendering Reference content | Additional format adapters, Metal/other render paths, dynamic atmosphere/weather/probes, animation, ray tracing and broad frameworks |

Initial import contract: glTF 2.0 **binary `.glb`**, static default-scene nodes,
triangle primitives, embedded PNG/JPEG images, indexed or non-indexed geometry,
multiple primitives/material slots and repeated mesh instances. Decode supported
accessor component types/normalization/strides correctly. Support `TEXCOORD_0` and
optional `COLOR_0`; reject sparse accessors, additional UV requirements, skins,
morph targets, animation, non-triangle primitives and required unsupported
extensions with precise diagnostics. This is an explicitly bounded glTF subset,
not a claim of general glTF conformance. Optional extensions may use a defined
core fallback only with a warning; features essential to the selected material
must not silently disappear. Text `.gltf`, external/network URIs, Draco,
meshopt-compressed buffers, KTX2/Basis, transmission and alpha blending are deferred.
Use the declared default scene, or the sole scene if unambiguous; otherwise
require explicit scene selection. Do not silently merge every scene in a file.
Inventory unsupported source semantics before publication, including authored
cameras/lights, extra attributes and optional material extensions. A documented
core-equivalent fallback may warn; meaningful discarded behavior needs an explicit
recorded user choice or rejection. The first automatic import rejects it. In
particular, an optional unlit-material extension is not permission to silently
turn that surface into PBR; translate it deliberately or diagnose unsupported use.

## Project creation, independent choices and settings ownership

### What the current source actually does

[ProjectCreation](../../Hazel/src/Hazel/Project/ProjectCreation.cpp) is already a
native, GPU/compiler-free producer: it validates template contract version 1,
creates an orthographic camera and Welcome text through CPU document data, writes
script source/build files without compiling, validates the descriptor, and
publishes a new directory exclusively. It must remain that authority.
[ProjectCreationController](../../Hazelnut/src/Authoring/ProjectCreationController.cpp)
opens that content; [readiness](../../Hazelnut/src/Authoring/AuthoringReadiness.h)
already allows script-free Play without an SDK or accepted project assembly.
The [New Project form](../../Hazelnut/src/Authoring/AuthoringPanel.cpp) currently
asks for name, technical identifier and destination; there is no preset choice.
The only bundled templates are the contract, C# stub and Premake recipe; starter
scene content is generated natively rather than copied from a Python script.

[ProjectConfig](../../Hazel/src/Hazel/Project/Project.h) owns startup/assets/scripts,
save namespace, runtime renderer requests and UI bindings. Project Settings stages
validated descriptor publication/reopen, with a separate guarded rendering-request
save. [NewScene](../../Hazelnut/src/EditorLayer.cpp) currently creates an empty
scene; it does not apply a project scene template. Runtime cameras already support
both projection types, while editor rendering/gizmos use the perspective orbit
camera; an orthographic controller also exists but is not a complete planar-editor
mode. [ProjectWorkspace](../../Hazelnut/src/Authoring/EditorState.h) persists orbit
pose, not a 2D/3D navigation choice. These are concrete extensions, not existing UX.

### Presets seed content; they do not restrict a project

Implement New Project with **Name**, **Location**, and two template cards:
**2D — sprites and a flat starter scene** / **3D — a lit model starter scene**.
Default to 2D, matching current creation behavior. Suggest a valid build identifier
from the name, with a deterministic valid fallback for non-identifier names; keep
the validated identifier under Advanced identity settings, editable before Create.
Do not expose backend, shader path, texture slots, IBL resolution or SDK selection
in this form. Preserve destination rejection, Unicode paths and content-first
creation. No assigned script or script compilation is necessary for either starter.

| Default selected by creation preset | 2D preset | 3D preset |
| --- | --- | --- |
| Starter scene/content | Orthographic camera, unlit sprite/world-text dimensional reference, simple screen UI; useful immediately | Perspective camera framing a one-meter native starter mesh, neutral PBR material, directional key and prepared neutral image environment |
| Camera defaults | Orthographic size 10; sensible non-singular position/clipping, e.g. z=10 with near .1/far 100 | Perspective vertical FOV 45°, near .1/far 100, position/orientation derived from starter bounds |
| World presentation | Display-referred, unlit; no lighting/IBL work | Scene-referred HDR with lighting, EV 0 and Neutral tone mapping |
| Editor initial view | Planar pan/zoom, grid and appropriate manipulation plane | Orbit/pan/dolly and bounds framing |
| Shared behavior | Same scene/ECS, assets, linear color contract, runtime UI, history, saves and packaging | Same; no separate 3D project/runtime architecture |

Bundle the small, engine-authored starter mesh/material/image and prepared data
with templates, validated CPU-only. Creation copies/publishes them without invoking
a parser, baker, GPU, Python or scripting SDK. It may generate simple authored DTOs
as today. Validate updated native/template contracts together; never advertise a
working 3D starter before its renderer/assets are ready.

Store **new-scene defaults**, not an authoritative `Is3D` permission bit. A preset
name is descriptive provenance and a reset action over defaults. Project Settings
sections should be **General/startup/storage**, **New scene defaults**, **Runtime
performance/quality**, **UI/input**, and **Advanced renderer requests/diagnostics**.
New scene defaults choose initial projection, world presentation and editor view
seed; they do not own an existing scene's environment or exposure. **File > New
Scene** applies those defaults through the same native scene factory, with a small
explicit preset override when wanted. Changing defaults affects future scenes/
fresh workspaces only. Edit existing scenes/cameras directly; do not silently
rewrite all scenes when the project preset changes or introduce bulk conversion UI.
Runtime performance/quality covers supported budgets and requests, not a competing
project-wide world-presentation or environment override.

These axes must remain independent:

| Choice | Meaning and boundary |
| --- | --- |
| Sprite/world-space 2D | Geometry/authoring convenience, painter ordering and optional planar physics; still world transforms and world depth/visibility |
| Orthographic/perspective | Camera projection, not material, lighting model or project capability. Orthographic PBR and perspective unlit scenes are valid |
| Lit/unlit surface | Material semantics. PBR receives lights/IBL; unlit bypasses lighting, not necessarily world exposure/tone mapping. Sprite/circle/text draws remain first-class unlit surfaces initially; lit-sprite extensions are not forbidden by their project preset |
| World presentation | Display-referred unlit presentation versus scene-referred radiance/exposure/tone mapping; independent of geometry dimension and projection |
| Screen-space runtime UI | Canvas layout/input/ordered painting in display-linear space, independent of the world camera and exposure |
| Editor navigation/tools | Workspace planar/orbit navigation, grid/manipulation plane and preview camera; switching tools does not change authored runtime cameras or materials |

A 2D-created project can add a perspective camera or a PBR mesh. Adding lit content
to a display-referred scene must offer an ordinary guarded scene-setting change
to scene-referred presentation, or give an actionable validation error; never
silently render it unlit. A 3D-created project can use orthographic cameras,
sprites, tile-like content, unlit materials and screen UI. Implementing a tilemap
system is not part of this milestone. Both kinds of scene may live in one project.

### One owner per setting; intent and capability stay separate

Reuse [RendererPolicy](../../Hazel/src/Hazel/Renderer/RendererPolicy.cpp) and
[RendererRequestsSerializer](../../Hazel/src/Hazel/Project/RendererRequestsSerializer.cpp).
Today `RuntimeRendererRequests` contains VSync, texture slots and Auto/GLSL shader
loading. `Resolve` retains requested values, derives effective values/reasons from
actual context caps, limits batching, and selects the available Auto shader path.
`RestartReason` already distinguishes launch-time renderer changes. The editor's
VSync/debug preference and runtime project's VSync are intentionally different
scopes. Do not replace this with dimension checks or new platform-name tables.

| Owner / persistence | Implement now | Resolution, validation and later extensions |
| --- | --- | --- |
| Project `.hproj` | Startup/assets/scripts/save namespace/UI bindings; new-scene defaults; supported portable runtime requests | Defaults seed new content once. Runtime requests are validated intent, never copied device facts. Keep advanced batching/shader requests separate from artistic settings. A future quality-profile choice may select only defined, prepared content profiles |
| Scene `.hazel` / detached prefab where meaningful | Explicit world presentation, environment/background/lighting selection and instance orientation/gain, exposure, directional-light entities | Existing scenes own their values after creation; no hidden project/editor fallback for exposure or sun. Scene settings have ordinary dirty/history/atomic-save coverage. The first scene-referred curve is fixed Neutral, not a menu of unimplemented tone operators. Validate incompatible presentation/content before Play/export |
| Camera / transient render view | Camera projection/FOV/clipping/pose and aspect policy; host supplies logical/pixel extent, camera selection and destination | First slice uses scene exposure. A future explicit camera-shot exposure override may replace that one field before player compensation; do not add it until needed. Preview/capture overrides are scoped and reported, not silently saved to the scene |
| Editor preferences and project workspace | Tool paths, UI scale, editor VSync/debug policy; planar/orbit choice, poses, grid and debug display per workspace | User preferences seed new workspaces; existing workspace values win. Preview diagnostics never become game requests. Camera preview is explicit and uses authored camera values without overwriting editor navigation state |
| Player overrides / game user storage | Existing audio/input overrides stay separate; no new graphics-options menu is required now | Establish a whitelist for later resolution/VSync/available quality profiles and optional additive EV compensation. Those may override only designated presentation fields, never rewrite project/material/environment assets or invent missing bakes |
| Detected backend/device record | Actual-context capabilities, limits, supported formats/paths and submitted versus measured presentation state | Read-only, process/context lifetime; never serialize into projects. Refresh at context replacement, not through repeated OS queries in consumers |
| Preparation profile / accepted asset generation | Requested resource quality and recipe/version/hash metadata | Producer validates and cooks it; renderer validates usable format/limits. Profile/resolution changes that need different data require preparation, not a shader pretending the requested quality exists |

Define a resolved view configuration with value provenance and explanations by
extending the existing pure policy boundary, not adding a service locator:

1. Built-in defaults fill only missing creation choices; the factory writes explicit
   canonical scene/camera values. Project scene defaults are not a runtime override.
2. Resolve authored project runtime requests and scene/camera intent. Apply only
   whitelisted player settings when implemented. A justified explicit camera
   override replaces its scene field; player EV compensation would be additive,
   not a second authored exposure. Editor-only controls apply to Edit, not Nutella.
3. Resolve against active backend/device capabilities. Defined Auto requests or
   documented budget clamping may choose a supported effective result with a
   reason, as today. An unknown request, unavailable required feature, incompatible
   material/presentation or missing prepared variant is an error, not silent success.
4. Report **authored/requested → effective**, source, limitation and apply boundary
   in Project/Scene Settings and diagnostics. Preserve launch restart requirements;
   view/asset changes publish a fully validated replacement at a safe frame boundary.
   Driver-submitted VSync is not measured frame pacing.

The initial backend remains OpenGL. A future backend choice belongs to validated
launch/deployment policy and backend factories; no backend selector is needed now.
No scene, material, project preset or consumer needs `if Mac/Windows/Linux` rendering
policy. Extend typed requests/caps only when there is a real supported setting.

## Confirmed foundations and concrete gaps

| Concern and inspected source | Reuse | Required extension / constraint |
| --- | --- | --- |
| [Components](../../Hazel/src/Hazel/Scene/Components.h), [hierarchy](../../Hazel/src/Hazel/Scene/Hierarchy.cpp) | Local vec3 translation/rotation/scale, GLM quaternion-derived `T*R*S`, UUID identities, ordered parents, full world matrices, bounded graph and lifecycle validation | Add mesh/light components and scene rendering settings. Rendering must use `GetWorldTransform`, not local TRS. Keep current 10,000-entity/256-depth limits |
| `Scene::ExactTRS`, `SetWorldTransform`, `ValidatePhysics` in hierarchy | Existing deliberate shear/reflection rejection for Keep World, finite validation and root-only planar Box2D owners | Do not use the lossy `Math::DecomposeTransform` as an importer validator. Preserve explicit signed glTF local TRS; validate matrix reconstruction. Reflected/sheared world matrices can render, but current world gizmos may reject them. Keep this diagnostic and local-property editing; do not weaken Keep World |
| [SceneCamera](../../Hazel/src/Hazel/Scene/SceneCamera.h), [EditorCamera](../../Hazel/src/Hazel/Renderer/EditorCamera.cpp) | Runtime perspective/orthographic projection; editor perspective orbit/pan/zoom; minimized-size guards; persisted orbit | Add bounds-based frame-selection and sensible new-3D-scene camera defaults. Validate perspective near/far/FOV, invertibility and a unique selected runtime primary; do not rewrite old camera defaults |
| [Scene rendering](../../Hazel/src/Hazel/Scene/Scene.cpp) | Scene update/lifecycle, world-space sprite/circle/text submissions | Runtime `OnUpdateRuntime` and editor `RenderScene` duplicate 2D loops and both call `RenderUI` immediately. Extract the actual view/render boundary needed for HDR; avoid separate editor/player implementations |
| [Renderer](../../Hazel/src/Hazel/Renderer/Renderer.cpp), [buffers](../../Hazel/src/Hazel/Renderer/Buffer.h), [vertex arrays](../../Hazel/src/Hazel/Renderer/VertexArray.h) | Ref/Scope resource lifetime, vertex layouts, uint32 index buffers, indexed ranges, uniforms/UBOs and context-thread shutdown | `Renderer::Submit` is a generic orthographic helper, not a mesh/material/lighting system. Add a small forward world renderer and per-view target owner; no existing mesh or material abstraction was found |
| [Texture contract](../../Hazel/src/Hazel/Renderer/Texture.h), [decoder](../../Hazel/src/Hazel/Renderer/Texture.cpp), [GL upload](../../Hazel/src/Platform/OpenGL/OpenGLTexture.cpp) | CPU decoding, sampling descriptions, byte upload, generated RGBA32F textures | Files decode through `stbi_load_from_memory` into 8-bit bytes; float file loading is explicitly rejected. No sRGB format or cubemap API. File upload flips rows after top-left decode. PBR/HDR must not reuse that path blindly |
| [Framebuffer](../../Hazel/src/Hazel/Renderer/Framebuffer.h), [GL framebuffer](../../Hazel/src/Platform/OpenGL/OpenGLFramebuffer.cpp) | RGBA8, R32I picking, depth/stencil, binding restoration, integer clear/readback and existing MSAA tests | No HDR attachment format, public attachment-copy operation or cubemap-face/mip upload. Existing engine size cap is 8192 in addition to device limits. Begin 3D at one sample; do not assume existing color/integer MSAA limits imply every new format combination works |
| [RendererAPI](../../Hazel/src/Hazel/Renderer/RendererAPI.h), [GL state](../../Hazel/src/Platform/OpenGL/OpenGLRendererAPI.cpp) | Raster/clip/viewport/picking state, indexed draws, explicit text compositing | `RasterState` does not describe depth write/compare, cull/front-face or general blend/output-encoding state. Extend this authority for real passes; do not scatter raw GL fixes in scene/editor code |
| [Capabilities](../../Hazel/src/Hazel/Renderer/RendererCapabilities.h), [GL detection](../../Hazel/src/Platform/OpenGL/OpenGLCapabilities.cpp), [policy](../../Hazel/src/Hazel/Renderer/RendererPolicy.h) | One actual-context authority for texture limits, framebuffer/sample limits, binary shader/debug paths, forward-compatible line width | Add only limits used by new resources: cube size, vertex attributes and UBO size/binding limits. Validate format allocation/completeness. A newer GL version or present function pointer alone does not establish an optional usable feature |
| [Shader implementation](../../Hazel/src/Platform/OpenGL/OpenGLShader.cpp) | Pinned shaderc → Vulkan SPIR-V validation/reflection → SPIRV-Cross → GLSL 410; reflected sampler/UBO binding; source/recipe-keyed validated cache | Add PBR/sky/tone/output shaders within this pipeline. No include dependency tracking exists: keep first shaders self-contained, or hash fully expanded source if shared includes are introduced. Do not create a second shader compiler/cache |
| [ProjectAssets](../../Hazel/src/Hazel/Assets/ProjectAssets.h), [implementation](../../Hazel/src/Hazel/Assets/ProjectAssets.cpp), [styles](../../Hazel/src/Hazel/Assets/ProjectStyles.cpp) | Project-root ownership, normalized references, sampling-aware texture keys, explicit reload/refresh/epochs, accepted immutable font/style versions | Extend with bounded mesh/material/environment resolutions. There is no universal asset database or importer to extend. Mesh import and dependency scanning must be CPU-only; GPU upload is a distinct render-thread step |
| [Serialization](../../Hazel/src/Hazel/Scene/SceneSerializer.h), [schema](../../Hazel/src/Hazel/Core/DocumentSchema.cpp), [prefabs](../../Hazel/src/Hazel/Scene/Prefab.h) | Strict keys/version rejection, candidate loads, metadata-only mode, exact authored comparisons; detached subtrees and UUID remapping | Current scenes write version 2. Move maintained content to the new canonical settings/color schema with reviewed example updates; do not preserve an old gamma renderer. Extend every component inventory/copy/snapshot/validation path, not just YAML writing. Prefabs have no linked override/reimport propagation |
| [Document history](../../Hazelnut/src/Authoring/DocumentHistory.h), [StructuralCommand](../../Hazelnut/src/Authoring/StructuralCommand.cpp), [FileDocument](../../Hazel/src/Hazel/Core/FileDocument.h) | Bounded value-only commands, identity/version guards, exact dirty checks, save conflicts/recovery and atomic publication | Add authored mesh/material-slot/light/settings coverage. History must never own GPU resources or include derived cache/version handles. Material/environment files need their own document ownership |
| [Editor viewport](../../Hazelnut/src/EditorLayer.cpp), [hierarchy panel](../../Hazelnut/src/Panels/SceneHierarchyPanel.cpp), [Content Browser](../../Hazelnut/src/Panels/ContentBrowserPanel.cpp) | Perspective gizmos, existing scene/prefab drops, root-relative payloads, logical-to-framebuffer conversion and integer entity picking | Add mesh import/assignment and typed material/environment editing. Reuse transactions, operation availability, native tool/process discovery and progress/cancellation; no generic editor rewrite |
| [RuntimeSession](../../Hazel/src/Hazel/Scene/RuntimeSession.cpp), [Nutella](../../Nutella/src/NutellaApp.cpp), [application teardown](../../Hazel/src/Hazel/Core/Application.cpp) | Candidate scene validation, Play copies, failed-transition recovery, lifecycle/input/storage isolation; project assets released while GL context is alive | Extend validation to complete 3D asset closure before Play/transition publication. Nutella currently renders through the session directly to the active target; it needs the same HDR view pipeline as Hazelnut |
| [Managed entities](../../Hazel-ScriptCore/Source/Hazel/Scene/Entity.cs), [ScriptGlue](../../Hazel/src/Hazel/Scripting/ScriptGlue.cpp) | `Entity.LocalTranslation`, `LocalRotation` (radians), `LocalScale`, `WorldMatrix`, parenting and prefab instantiation already support 3D values | A spinning/moving mesh demo needs no scripting rewrite. The `TransformComponent` wrapper itself exposes only translation; do not mistake that for absence of local rotation on Entity. Camera wrapper lacks perspective FOV access; add only if a demonstrated script needs it. New exposed component wrappers require explicit registration/ABI tests |
| [Runtime UI](../../Hazel/src/Hazel/UI/RuntimeUI.cpp), [scene UI](../../Hazel/src/Hazel/Scene/SceneUI.cpp) | Ordered paint, independent logical/pixel metrics, clipping, text coverage, input scopes, depth-disabled UI and state restoration | Composite in display-linear space on both world presentation paths; when world tone mapping is needed, UI follows it. Preserve clipping, entity IDs, correct alpha and exposure independence |
| [Native audit](../../tools/SpriteAssetAudit/main.cpp), [closure code](../../Hazel/src/Hazel/Assets/ProjectAssets.cpp), [packaging](../../scripts/internal/packaging.py), [Mac bundling](../../scripts/macos/packaging.py) | Native metadata-only dependency validation, Python inventory/managed/native closure, complete eligible Assets copying, resource layout and signing | Extend the existing native audit to mesh/material/environment/artifact dependencies. Its historical name need not force a second independent auditor. Python must not learn a second glTF/material parser |

## Dependency decisions

No dependency was acquired or built during this audit. Existing GLFW, Glad,
GLM, YAML, ImGui/ImGuizmo, typography, Mono, audio and pinned shader tools remain.
The recommendations below need focused implementation-time qualification.

| Candidate | Evidence, tradeoff and recommendation |
| --- | --- |
| **TinyGLTF 3.0.1** | Preferred importer candidate: tag resolves to `0d9ce1a9561688c6c6e7471430d15a50a0b15f1a`. Upstream's [v3 README](https://github.com/syoyo/tinygltf/blob/v3.0.1/README.md) describes a C11-first API, arena ownership, structured errors and validation/security fixtures. Inspecting that pin confirms `tiny_gltf_v3.c`, `tiny_gltf_v3.h`, `tinygltf_json_c.h`, byte-based `tg3_parse_glb` and optional filesystem/image I/O. Use the C translation unit through Premake, own byte I/O, and disable optional image/writer/coroutine paths. [MIT license](https://github.com/syoyo/tinygltf/blob/v3.0.1/LICENSE); preserve its bundled numeric-parser/serializer notices too. Native arm64/MSVC/GCC compilation and malformed-input behavior are **unproven here**, so stage 2 gates adoption behind the format-neutral boundary |
| cgltf 1.15 | Compact C99/MIT alternative; tag commit `360db1a95480fe102ae9c69b27c5d101167ff5ba` ([source/license](https://github.com/jkuhlmann/cgltf/tree/360db1a95480fe102ae9c69b27c5d101167ff5ba)). However, upstream open [report 299](https://github.com/jkuhlmann/cgltf/issues/299) alleges a validator over-read, and [report 300](https://github.com/jkuhlmann/cgltf/issues/300) alleges a sparse-unpack overwrite. These are reported findings, not locally reproduced. Latest inspected master `85cd62382dfea638278962690cf515023f33ed00` is not evidence those reports are fixed. Do not automatically adopt 1.15 plus an assumption that calling `validate()` makes it safe. Reconsider only with verified upstream fixes and regression coverage; do not start an unbounded local parser fork |
| Assimp | [BSD-style license](https://github.com/assimp/assimp/blob/master/LICENSE), broad formats and a larger integration/notice surface. No first-slice requirement for FBX/OBJ justifies that scope. Do not introduce its build stack or hidden CMake prerequisite. No version chosen |
| **MikkTSpace** | Needed when a supported normal-mapped primitive omits tangents. Pin `3e895b49d05ea07e4c2133156cfa94369e19e409`; compile its small C source with Premake. The [original header](https://github.com/mmikk/MikkTSpace/blob/3e895b49d05ea07e4c2133156cfa94369e19e409/mikktspace.h) carries a zlib-style permissive notice and warns that results are per corner: split/reindex vertices rather than overwriting shared vertices across tangent seams. Producer-only; no new player dylib |
| Existing stb_image | Already compiled in `Hazel/vendor/stb_image/stb_image.cpp`; pinned header exposes HDR detection and float memory decoding. Add a bounded `.hdr` float decode service and embedded PNG/JPEG decoding using the existing implementation/mutex. Do not compile another stb implementation through the importer. No new image library; OpenEXR and texture compression deferred |
| Existing shaderc/Cross and native frameworks | Build/integration changes only. GLSL 410 supports the proposed draws, cubemaps, explicit LOD, UBOs and float targets. No Vulkan SDK, compute library, Metal framework or replacement window library is needed |
| Reference implementations | [Khronos BRDF](https://github.com/KhronosGroup/glTF-Sample-Renderer/blob/962002468332e65ef06f95258837e7b60f799309/source/Renderer/shaders/brdf.glsl), [IBL](https://github.com/KhronosGroup/glTF-Sample-Renderer/blob/962002468332e65ef06f95258837e7b60f799309/source/Renderer/shaders/ibl.glsl), [filtering](https://github.com/KhronosGroup/glTF-Sample-Renderer/blob/962002468332e65ef06f95258837e7b60f799309/source/shaders/ibl_filtering.frag) and [Filament's rendering notes](https://google.github.io/filament/main/filament.html) are design/check references, not engine dependencies. Inspect per-file notices before copying code: filtering source includes separately attributed sampling code. Keep Hazel's bake, LUT and runtime recipe explicit rather than borrowing mismatched pieces |

Acquisition later should pin source and checksums/notices in the existing
dependency ledger and Premake integration, with unchanged upstream files and
maintained wrappers. These are source builds for all three targets, not downloaded
Intel libraries. Producer requirements are the existing compiler/Premake/Python
workflow; consumers need only cooked runtime assets and the existing runtime
dependencies. Do not publish an SDK or require a system importer installation.

## Format-independent import foundation, ownership and reload

Proposed names below are new files, not existing facilities. Extend
`Hazel/src/Hazel/Assets` with typed Mesh/Material/Environment codecs/resolutions;
put producer-only format adapters and common cooking in a small import module
linked by `HazelProject`. Keep the engine/runtime loader independent of parser
internals. A GLB file is the first source, not the engine's mesh/material schema.
Add native `HazelProject` import/prepare operations and canonical Python dispatch;
the editor uses its existing native ToolRunner/process/cancellation boundary with
a validated complete executable path, not shell commands or embedded Python.
Provide the matching producer helper through the editor/development installation
layout and report import/prepare capability separately from scripting-SDK readiness.
Do not make selecting a C# SDK a prerequisite for native creation or asset authoring.
The helper links the parser/cooker; the engine/player runtime does not.

### Necessary producer stages

```mermaid
flowchart LR
    Source[Source bytes and interpretation] --> Parse[Format-private parser]
    Parse --> Convert[Source semantic conversion]
    Convert --> Imported[Engine imported mesh material node data]
    Imported --> Cook[Common validation and cooking]
    Cook --> Publish[Staged publication and stable reimport map]
    Publish --> Accepted[Accepted portable runtime assets]
    Accepted --> Load[ProjectAssets CPU resolution]
    Load --> GPU[Renderer resource cache and context-thread upload]
```

**Establish now:** use owned, versioned engine data between semantic conversion
and common validation/cooking. Call it `ImportedSceneData` (proposed name): only
the first slice's mesh primitives, attributes/index streams, material definitions,
decoded/referenced image semantics, nodes/instances, local transforms/bounds and
source-key mapping. Parser arena pointers/indices must be converted or copied
before the parser is freed. TinyGLTF structs, extension objects, error enums and
buffer-view layouts never become engine/runtime APIs or cooked file formats.
Source-format-specific options/diagnostics stay in their adapter; common diagnostics
name source location plus the engine semantic that cannot be represented.

Use concrete adapter entry points returning this DTO and one common cook service;
do not implement discovery/registration of arbitrary import plug-ins. A second
format adapter can later call the same service without changing runtime loaders.
No universal property bag or mandatory empty animation/skeleton fields are needed.

| Boundary | Preserve or normalize |
| --- | --- |
| Parse | Read bounded bytes/metadata without GPU, active scene or uncontrolled external I/O. A parse success is not semantic validation or a published asset |
| Source semantics | Convert declared units, axis/handedness, node matrices, winding and normal/tangent convention into engine meters/right-handed/+Y-up. Record the conversion in provenance. Require explicit interpretation when source information is absent; do not guess from a filename or silently import a centimeters model at meters scale |
| Mesh/node DTO | Keep local geometry separate from node instances and transforms; multiple nodes may share a mesh. Preserve source keys scoped to one import, node/mesh relationships, primitive/material slots, UV-set identity and bounds. Source IDs are evidence for stable mapping, not engine UUIDs |
| Materials/images | Translate source meaning into engine Unlit/MetallicRoughness surface data and color/data texture uses. Repack/convert channels deliberately, preserve samplers and document unsupported material semantics. No forward shader bindings, GL names or importer material objects |
| Validate/cook | Check finite values/counts/indices/graph/bounds, generate required normals/tangents, split seams, normalize image/UV conventions, encode portable data and compute dependency/recipe identities. Same validators for every adapter |
| Publish/reimport | Reconcile persistent engine slot/output mappings against source keys and content evidence, check conflicts, stage and atomically publish a whole accepted generation. Format changes do not bypass these rules |
| Runtime/GPU | Read canonical typed assets only; no source parser, source SDK or cooking on player startup. ProjectAssets owns accepted identity/definitions; its renderer-facing cache owns GPU realizations keyed by accepted generation and texture usage. CPU documents/DTOs have no GL identities/bindings |

**Future adapters, not now:** text glTF can add confined dependency resolution;
FBX can supply its declared axes/units/instance identities; OBJ can provide geometry
and mapped material semantics with explicit import interpretation for missing
metadata. Each produces the same static DTO; choose its parser/license/build
strategy when implementing it. A source with richer material or animation data
must report unsupported semantics, not flatten them silently to a plausible model.
Any later user-selected mesh-only/static-pose import must be explicit and recorded.
Future skeleton/animation products extend this boundary with real typed data and
stable source-node mappings when needed, without putting foreign skeleton objects
or unused GPU skinning streams into today's static mesh format.

### Accepted assets and reimport

1. Preserve the source `.glb` under project ownership. A per-import manifest records
   source hash, options, parser/tangent versions, source-index mappings, generated
   files and original notices. This is a manifest for one import, not an asset DB.
2. Common cooking produces `.hmesh` references plus a versioned geometry artifact, initial
   `.hmat` documents, extracted images and a detached `.hprefab` for the selected
   static node tree. The prefab owns node TRS; do not invent a parallel runtime
   model hierarchy. Repeated nodes share mesh resources. Scene instances remain
   detached under the existing prefab contract.
3. Use root-relative paths as current project identity. Within each mesh, persist
   material-slot IDs independently of display names. glTF names/array indices are
   not persistent authored identity. The adapter supplies source keys but the
   publication layer owns persistent mappings. Reimport reconciles the stored map; ambiguous
   primitive/material reorder or removed slots must report a conflict rather than
   silently assigning another material. Preserve existing slot overrides.
4. Geometry is mesh-local. A first vertex layout is position/normal/tangent vec4/
   UV0/color vec4 (64 packed bytes); explicitly serialize fields, never dump GLM
   struct ABI. Expand indices to uint32, rebase per-primitive indices within the
   uploaded mesh buffer and retain indexed ranges/material slots/local bounds.
   Existing `DrawIndexed(count, firstIndex)` suffices; a base-vertex or indirect
   submission API is not necessary. Validate bounds and arithmetic before reads.
5. Generate flat normals with corner splits when missing, ignoring supplied
   tangents in that case as glTF requires. Honor valid supplied tangents when
   normals exist; otherwise generate Mikk tangents against the normal map's supported UV set,
   then split/reindex. Reject unusable required normal-map UVs with a useful error.
   Require finite attributes, valid normals/tangents and non-singular rendering
   transforms. Define handling of degenerate triangles explicitly; never emit NaNs.
6. Stage under an owned temporary with cancellation/progress and input conflict
   checks. Publish immutable generation files under `Assets/Imported/<import>/...`
   and atomically replace the descriptor/manifest last. Reuse leases and atomic
   filesystem services. Never overwrite a user-edited material or prefab on
   reimport: compare accepted bytes, preserve editable files and report source
   changes. Updating geometry does not relink detached scene instances.
7. `ProjectAssets` remains the accepted identity/CPU-definition authority and
   project lifetime owner. Keep GPU realizations in its renderer-facing cache,
   separate from CPU validation/loading. Do not create a competing asset database.
   Scene components store references, not GL names. Failed reimport/reload retains
   already accepted/pinned resources for viewing and reports the candidate error.
   Play/export reject unresolved or incompatible required generations. Reload and
   epoch advancement remain explicit; no per-frame filesystem scans.

Use endian/versioned headers, lengths, checksums and checked offsets for cooked
geometry and cube data. Record bounds and primitive counts; avoid compression or
an extensible universal container in this milestone. Suggested initial producer
bounds: 64 MiB source GLB, one million total vertices, three million indices,
bounded node/material/image counts and 512 MiB total decoded working data. Check
image dimensions before allocation; reject rather than truncate. These are proposed
admission limits to validate against selected fixtures, not existing promises.

Materials have one typed schema: base-color factor/map, metallic/roughness
factor/map, normal scale/map, occlusion strength/map, emissive factor/map, alpha
mode/cutoff, double-sided and sampling. A material asset is shared immutable data;
a draw's material instance binds its accepted version and per-draw object data.
Initially, entity overrides replace whole slot material references. Unique scalar
values use a duplicated `.hmat`; do not add inheritance or arbitrary property bags.
Imported defaults follow glTF; a newly authored default may deliberately be a
neutral dielectric (`metallic=0`, `roughness=.5`) rather than glTF's omitted values.
The same surface contract also has an Unlit model; PBR parameters do not imply
forward-pass or G-buffer layout. Existing sprites/circles/text can keep concise
component APIs that submit Unlit semantics; every UI control need not acquire a
mesh/material asset just to share a color model.

### Image and radiance import are adapters too

Use an engine `ImportedImageData` contract: dimensions/channels, pixel representation,
row convention, color-space/alpha interpretation, usage and provenance. PNG/JPEG
and Radiance HDR use the existing decoder first; do not bake a `.hdr` filepath
into the IBL convolution API. Image normalization produces linear radiance/canonical
image data, then the common environment preparer consumes it. Generated/captured
radiance can later enter after decoding without pretending to be an image file.

For future EXR, select channels and normalize data/display windows, row orientation,
alpha association and declared color primaries before common cooking. The
[OpenEXR technical introduction](https://openexr.com/en/latest/TechnicalIntroduction.html)
documents named channels, separate windows and associated-alpha conventions;
an EXR file is not automatically equivalent to a top-left RGB Radiance image.
Do not assume a filename proves transfer function/primaries or silently drop deep,
multipart or unsupported channels. Add a decoder/dependency only with an actual
format requirement; none is acquired now. Other image/environment adapters share
the same engine image/radiance contracts and explicit interpretation diagnostics.

## Geometry, material and color conventions

The [glTF specification](https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html)
is the source-format authority. The engine rules below apply across importers and
both 2D and 3D; write them into fixtures and artifact recipe versions.

| Concern | Convention |
| --- | --- |
| Coordinates | Right-handed, +Y up, meters. Keep glTF mesh coordinates; the asset's +Z front does not change the camera's local -Z viewing direction. GLM column-vector matrices, world = parent world × local TRS; Euler authoring remains radians |
| Projection | Existing OpenGL -1..1 clip-space depth and ordinary depth test; new-scene near .1/far 100 are defaults to verify against bounds. No reverse-Z/clip-control dependency |
| Nodes | Preserve explicit local TRS, including signed scales. For matrix nodes use checked affine signed-TRS reconstruction, not silent shear removal. Existing Keep World/gizmo restrictions remain visible. No physics3D component is implied by a mesh |
| Normal basis | Normalize inverse-transpose world normals; transform and orthogonalize tangents. Bitangent uses tangent.w and world determinant sign. Negative determinant also reverses front-face winding. For double-sided back faces, reverse the final mapped shading normal before lighting; test both flat and mapped backs |
| UV/image rows | Canonical ordinary images use top-left rows; glTF UV0 stays paired with those rows, without a green-channel inversion. Consolidate the existing file-upload flip and sprite UV assumptions at the image/upload boundary and deliberately update affected consumers/tests. Do not add a permanent old-sprite interpretation. Generated glyph/MSDF atlases retain an explicit data/UV contract. Test labeled asymmetric images and tangent mirrors |
| Lighting space | Linear RGB with sRGB/Rec.709 primaries and D65. Treat supported Radiance `.hdr` input as linear RGB under this documented convention; no LDR gamma conversion or automatic HDR-to-8-bit conversion. EXR/wide-gamut interpretation deferred |
| Base color / emissive | Image RGB is sRGB and must decode before filtering/lighting; alpha is linear. Factors are linear, `COLOR_0` is a linear multiplier. Normal/data maps must never use sRGB storage |
| Metallic/roughness | B = metallic, G = perceptual roughness, multiplied by corresponding factors. Other channels do not change those values. Occlusion is a separate R-channel use, even if it shares the image |
| Normal map | Linear RGB maps to tangent-space XYZ; scale X/Y, then normalize. Use glTF +Y convention. No global DirectX-green-flip setting |
| Occlusion/emission | Occlusion strength attenuates indirect lighting only in this baseline; do not multiply direct light or emissive output by AO. Emissive adds linear radiance; bloom is deferred |
| Alpha | OPAQUE ignores alpha. MASK discards below the authored cutoff before depth/color/entity-ID writes. Support double-sided. BLEND is explicitly rejected initially; future straight-alpha, depth-tested/no-depth-write back-to-front rendering needs one transparent-world ordering policy, not a fake opaque approximation |
| Exposure / display | Scene-referred world radiance uses artist exposure in stops (`2^EV`) and one [Khronos PBR Neutral](https://modelviewer.dev/examples/tone-mapping) curve. Display-referred unlit world content bypasses those two operations. Both compose UI in display-linear space and encode sRGB exactly once; no `pow(x,1/2.2)` substitute or second `GL_FRAMEBUFFER_SRGB` conversion |

Channel/factor defaults come from the official [metallic/roughness schema](https://github.com/KhronosGroup/glTF/blob/main/specification/2.0/schema/material.pbrMetallicRoughness.schema.json),
[material alpha/double-sided/emissive schema](https://github.com/KhronosGroup/glTF/blob/main/specification/2.0/schema/material.schema.json)
and [normal-scale schema](https://github.com/KhronosGroup/glTF/blob/main/specification/2.0/schema/material.normalTextureInfo.schema.json).
Map glTF samplers explicitly; use a documented linear/trilinear default when
unspecified, without inventing anisotropy requirements.

Extend `TextureSpecification` with an explicit color-space/usage contract, and its
cache identity accordingly. Add sRGB color storage and float formats at the
OpenGL resource boundary. The same image used as base color and linear data must
not alias one incorrectly encoded GPU texture. Keep sampler state in its identity.
Inspectors should convert between display color selection and stored linear
factors deliberately; scalar roughness/metallic/alpha never undergo that conversion.

### One color model; two useful world-presentation paths

**Implement now:** authored RGB color values, including sprite/text colors,
material factors, UI styles and scripted colors, are **linear Rec.709/sRGB-primary
RGB**; alpha is linear coverage/opacity. Color pickers and familiar hex/sRGB input
display or accept encoded values and convert exactly once at that user boundary.
Label APIs/helpers so a script can deliberately convert an sRGB literal rather
than guess. Factors imported from glTF are already linear. Color textures decode
sRGB before filtering/shading; normal, metallic/roughness, AO, masks and glyph/MSDF
data never do. Keep file interpretation, authored values and GPU sampling distinct.
Use exact [sRGB conversion math](https://www.w3.org/TR/css-color-4/#color-conversion-code)
and independent vectors, not a 2.2 approximation or a CSS asset/schema dependency.

World presentation is an authored scene setting, not a hardware capability or
project dimension flag:

| Presentation | World processing | Common output boundary |
| --- | --- | --- |
| Display-referred unlit | Submit sprites/circles/text or unlit meshes into a linear composite target with world depth/ID semantics. No light evaluation, IBL, separate radiance target or tone-map pass. Values describe intended display-linear appearance | Ordered screen UI composites in display-linear space; encode final output once |
| Scene-referred | Lit **and unlit** world surfaces produce linear radiance into HDR. Apply scene exposure and Neutral tone mapping into the display-linear composite target | Same UI and final encoding. Unlit world objects bypass lights, not exposure/tone mapping; UI bypasses both |

An orthographic scene can use PBR/HDR, and a perspective scene can use the
lightweight unlit path. A display-referred scene with lit content requires an
explicit scene-setting change or validation error. Do not silently disable lights
or infer presentation from whether a mesh happens to be present this frame.
The lightweight path may use a float linear composite target initially; avoiding
3D lighting/IBL and extra world/tone passes is the required saving. A measured
hardware-sRGB optimization can come later under the same output contract.
An unlit RGB value of 1 means display white in display-referred presentation;
in scene-referred presentation it is unit relative radiance and may roll off
under exposure/Neutral. Explain this in the scene/material tools rather than
quietly promoting world sprites into exposure-independent screen UI. Solid
background colors follow their scene's world presentation too.

Keep existing ordered sprite/world-alpha behavior where supported, but blend RGB
in linear space. Distinguish this from the first mesh importer's explicit rejection
of BLEND; general mesh/sprite transparent interleaving is a separate future feature.
UI uses its existing ordered clipping/hit-testing contract with linear RGB blending.
Final editor viewport textures contain encoded output and ImGui must display them
without another decode/encode; any future display-color management belongs at that
output boundary. HDR monitors, wide gamut and premultiplied asset conventions are
not implicit additions to this milestone.

### Deliberate example update, not a permanent historical renderer

The repository examples are the owner projects in scope. Establish the canonical
schema and update their scenes/prefabs/styles, template defaults, scripted color
literals and expected fixtures together. Where current values were chosen as
display/sRGB values, convert their RGB deliberately to linear and leave alpha,
geometry and non-color vectors alone. Tag/document the current canonical schema
version; unsupported historical color semantics receive an actionable diagnostic,
not a second runtime gamma resolver. Existing structural readers may remain where
unambiguous, but must not choose an alternate color pipeline. No migration UI,
automatic source rewrite on load or hypothetical external-project compatibility
framework is required. A bounded developer conversion/re-authoring operation is
acceptable when reviewed field by field.

Before changes, record fixed scene/camera/resolution captures and behavior from
the qualified baseline. Review intended after-images against numeric color tests:
correct linear blending can change translucent overlaps/edges even after authored
RGB conversion. Preserve readability, palette intent and gameplay, rather than
reintroducing gamma-space blending to force every old pixel to match. Document
intentional differences and obtain focused visual acceptance; never silently
replace image baselines because a test failed.

| Existing example | Required appearance and functional review |
| --- | --- |
| MeadowRun | Pixel-art palette, layered scenery/parallax, sprite orientation, alpha edges and HUD contrast |
| Skybound | World/background contrast, platform/cloud layers, camera framing, text and gameplay visibility |
| LastLightkeeper | Night/day and beam/gate readability, authored/scripted tints, menu/settings styles and saves; operate on disposable copies preserving owner edits |
| SceneTransitions | Menu/level text and sprite contrast, exposure-independent UI and repeated transitions |
| Sandbox UI Tutorial | All five tutorial pages, font/MSDF edges, styles, focus, clipping and positive authoring tests |

All five remain supported 2D reference examples. The new Rendering Reference adds
controlled mixed-content checks; it does not replace their functional acceptance.

### BRDF and direct lighting

Use isotropic GGX/Trowbridge-Reitz, height-correlated Smith visibility and Schlick
Fresnel, with Lambert diffuse and Fresnel/metallic energy partition. Fix one
versioned `BRDFRecipe` used by runtime shading and IBL generation:

- `F0 = mix(.04, baseColor, metallic)`; no dielectric F0 slider initially.
- Authored perceptual roughness stays [0,1]; proposed numerical evaluation floor
  `.045`, then microfacet alpha = evaluated roughness squared. Record the floor
  in the bake/LUT contract; do not square roughness twice or apply a different
  direct-light roughness remap.
- Direct light uses `D * V * F`, where V already contains `G/(4 NoL NoV)`.
  Do not divide by that denominator again. Add diffuse `baseColor/π` partitioned
  by `(1-metallic)*(1-F)` and multiply incident light by `max(NoL,0)` once.
- Evaluate in FP32, handle grazing/zero-length cases explicitly and test finite
  outputs. One directional light has renderer-relative incident RGB intensity;
  its travel direction is normalized world-local -Z, and surface-to-light L is
  the opposite. Do not label intensity calibrated lux without a photometric
  conversion and evidence.
- Begin with a documented **single-scattering** model. Rough-metal energy loss
  is expected in that model, not something to hide with ambient multipliers.
  Multi-scattering is deferred; adding it later changes BRDF/LUT recipes together.

The Khronos BRDF reference above is useful for term definitions. Its current IBL
implementation includes additional Fresnel/multi-scattering treatment, so copying
its LUT reconstruction into a different single-scattering pipeline is not valid.

## Shared rendering path and OpenGL 4.1 boundary

Add `SceneRenderer`/`SceneRenderView` under `Hazel/Renderer` (proposed names), with
per-view target lifetime owned by the editor viewport or RuntimeSession. Scene
owns authored settings/entities; ProjectAssets owns accepted resources; the view
owns transient framebuffer targets and frame submissions. Root Renderer owns shared
shaders/default resources. All GPU creation/destruction remains on the context
thread and precedes window teardown.

Extract the common world submission from Scene's runtime/editor loops. Scene
update, scripts and UI routing stay with Scene/RuntimeSession; render-view input
provides view/projection/camera position, pixel/logical extents and destination.
Both hosts use the same pipeline. Do not expose ImGui, an editor camera type or
OS APIs to material/import code.

### Necessary shared inputs, without a universal renderer framework

**Establish now:** a concrete frame submission contains camera/view parameters,
mesh instances (accepted geometry reference, world/normal transforms, bounds,
material-slot references and picking identity), semantic light records and the
accepted environment generation/background state. Preserve world-2D ordered
commands and runtime UI's ordered paint/layout data; do not flatten every text
glyph/UI control into an imported-mesh representation. CPU scene, imported asset
and material data contain no GL object IDs, texture units, shader bindings,
G-buffer fields or forward-specific lighting lists.

The first forward renderer consumes those inputs. Later Forward+ may derive
light lists; deferred may derive G-buffer passes and encodings. Those are consumers
of common surface/instance/light semantics, not new authored material or scene
formats. Their transparency/quality constraints must be resolved explicitly if
implemented, not offered as nonfunctional renderer choices now. Metal can map
typed resource descriptions, state and pass inputs to its own backend without
creating a separate material/import architecture. Confine existing raw texture-ID
interop to the backend/editor display adapter, not new CPU asset records.

ProjectAssets remains the one accepted identity/definition authority. Add private
renderer-facing GPU realizations keyed by generation/usage; avoid a second asset
database or wholesale rewrite of existing font/sprite caches. Resource descriptions
must cover actual formats, sampling, attachment usage and state coherently; extend
them for operations used here, not every prospective API feature.

For future ray tracing, keep cooked triangle positions/indices, vertex normals/UVs,
material assignments, instance transforms/bounds/identity and accepted-generation
links recoverable. CPU geometry can be reloaded from portable assets; permanent
CPU residency is not required. No acceleration structures, ray APIs, unused buffer
flags or allocation infrastructure now. Screen UI is not implicitly ray-traced.

| Approach | Assessment for this slice |
| --- | --- |
| Simple forward | Chosen: one light, a few material maps and IBL, low draw counts, straightforward mask/double-sided/picking semantics. CPU bounds/frustum culling is sufficient |
| Forward+ | No demonstrated light-count problem; common compute/SSBO implementations exceed the 4.1 baseline. CPU light lists are possible later but unnecessary now |
| Deferred | More buffers/bandwidth, material encoding, resolve and transparency decisions, especially on older integrated hardware. No present requirement justifies it |

For scene-referred presentation, use this explicit sequence without a render graph:

1. Bind/clear a one-sample HDR world target with depth and R32I entity IDs (-1).
   Draw the environment background without depth writes or entity selection.
2. Submit opaque and masked mesh ranges with complete material/depth/cull state;
   draw unlit world sprites/circles/text as linear world content, with exposure
   semantics defined above. World depth/picking must agree. General blended mesh/world
   transparency interleaving is outside this slice.
3. Tone-map world HDR into a display-linear target. Preserve picking separately:
   a small typed integer-attachment copy operation with nearest/no averaging is
   enough to carry world IDs into the UI target; do not sample IDs through tone
   mapping. No generalized attachment graph is needed.
4. Draw existing ordered screen UI over that target, depth disabled, with its
   colors/images interpreted into display-linear space and its existing alpha/
   clipping/entity-picking behavior preserved. Exposure does not affect UI.
5. Encode once into the editor's RGBA8 viewport output or the player's output.
   Bind/restore framebuffer, viewport, write/compare/cull/blend/clip/encoding state
   at pass boundaries. Postprocess/sky draws must not overwrite useful entity IDs.

Give editor-only overlays an explicit position in the view's pass sequence and
disable their entity-ID writes. ImGui displays the final encoded viewport, never
the raw HDR texture. Picking reads the completed world/UI ID target, without
applying tone mapping, filtering or UUID serialization to GPU entity handles.

For display-referred presentation, draw unlit world content directly into the
display-linear composite target, preserve depth/IDs, then execute UI/output steps
4–5. Allocate no separate HDR world target or IBL resources solely because the
engine supports 3D. Renderer2D and UI use the same canonical color/sampling contract
in both paths. Do not decode an already hardware-decoded color sample again.

Start with RGBA16F world/display-linear targets and RGBA8 final output; add a float
readback/reference path. Test range/finite behavior of imported HDR and bright
highlights, including storage conversion; explicitly reject/report unrepresentable
prepared data. Do not silently clamp broken NaNs into plausible images. Consider
RGBA32F only where measured range/error warrants its memory cost. No HDR monitor
output, MSAA, bloom, automatic exposure or color-grading stack is required.

At the resource boundary add cubemap face/mip upload, appropriate float/LUT
formats, complete mip-chain validation and seamless cube sampling. Keep GL enums,
bind-based allocation, framebuffer status checks and state restoration inside
`Platform/OpenGL`. New public descriptions use typed formats/states and Ref-owned
resources. A narrow `VertexBuffer` byte-data overload may be appropriate; there
is no need to replace the buffer system or introduce indirect draws.

A fixed material layout needs at most eight fragment samplers: base color,
metallic/roughness, normal, occlusion, emissive, diffuse cube, specular cube and
BRDF LUT. Background/postprocess reuse bindings in separate passes. This fits the
observed M4 limit of 16 and the baseline's intended device budget; validate actual
caps. Use small std140 UBOs with explicit offsets/padded vectors and static
layout checks. The existing reflection/binding pipeline handles GLSL 410 bindings.
No compute, SSBO, image load/store, bindless textures, DSA, GL shader-SPIR-V or
420pack assumptions are allowed in the baseline. Use the [OpenGL 4.1 core specification](https://github.com/KhronosGroup/OpenGL-Registry/blob/main/specs/gl/glspec41.core.pdf)
as the API contract. Validate on actual Apple GL4.1;
Linux forced-4.1/software evidence remains a distinct layer.

Bind complete default textures for every active material sampler, including cube
samplers when no environment is authored; reuse the Mac port's lesson about
validating unused active samplers. Omitted optional maps use documented neutral
values, while explicitly broken references fail readiness. An absent normal map
uses the geometric normal directly, rather than introducing 8-bit neutral-map
rounding into every surface. This is not a fallback for a failed required asset.

## Environment preparation, IBL and cache identity

### Authored source, preparation policy and accepted resources are distinct

**Establish now:** environment ownership is not synonymous with one HDR file and
some cubemaps. Use the following concrete boundaries; only the image-source and
offline-prepared implementations are needed in this milestone.

| Layer | First implementation | Future extension point, not implementation now |
| --- | --- | --- |
| Authored scene environment settings | Background description (solid color or image environment), lighting-environment reference, orientation and gain. Background/lighting link to the same source by default; any separation is explicit. Scene exposure remains in world presentation | A scene can later select a procedural source or local-probe configuration without making exposure a hidden property of its generator |
| Source description `.henv` | Image reference with explicit radiance interpretation and preparation profile; not a filepath inside every lighting API | Typed procedural-atmosphere/capture source descriptions when those exist; no generic plug-in registry or mandatory unused atmosphere fields |
| Source evaluation / normalized radiance | Decode/normalize the supported Radiance HDR into canonical radiance data; background can use its source representation | Procedural or captured radiance can feed the same preparation boundary. A procedural background need not itself be a baked cube |
| Preparation/update policy | Explicit offline preparation, bounded progress/cancellation, recipe/content identity; no runtime baking | Later asynchronous generation, cadence and allowed visual latency are separate policies, not source-format parser responsibilities |
| Accepted lighting generation | Immutable diffuse/specular/LUT resources and metadata, accepted and pinned as one compatible generation | New sources can publish compatible replacements; local probes can own further bounded generations without replacing global environment ownership |

The scene owns its environment instance and directional-light entities; the `.henv`
asset owns source/preparation intent. Project defaults only seed these choices.
The preparer owns candidates, ProjectAssets owns accepted generations and the
renderer owns their GPU realizations. The render view consumes references; it does
not open an HDR file or become another environment authority.

Recommend **offline CPU preparation in the native asset tool**, not runtime bake.
It works without a desktop/GPU on all three producer platforms and gives players
bounded startup. GPU fragment-pass baking is 4.1-feasible but adds context/tool
ownership and driver variability; compute-based baking is unnecessary.

The first image source produces background/radiance data, diffuse irradiance,
GGX specular mip chain and a matching shared BRDF LUT, associated through an
accepted generation descriptor. Proposed first quality profile: background 256-face,
specular 128-face with the complete eight-level chain, irradiance 32-face and
128² RG16F LUT; fixed, recorded sample counts (start at 1024 for production).
These are quality/performance defaults to qualify, not measured results.

- Diffuse stores **irradiance E**, the cosine-weighted hemispherical integral.
  Its shader applies `/π` exactly once with diffuse color/energy partition. A
  constant environment C must produce E = πC, not C or π²C.
- Specular uses a normalized GGX importance-sampled prefilter with V=N at each
  output direction and NoL-weighted accepted samples. Mip 0 is a special unblurred
  source endpoint; mip k>0 uses perceptual roughness k/(levels-1), evaluated with
  the BRDF's floor/alpha mapping. Runtime lookup uses the evaluated perceptual
  roughness. Test interpolation near the floor separately. Ordinary generated
  box-filter mipmaps are not a specular prefilter.
- LUT coordinates are `(NoV, perceptual roughness)`; channels A/B reconstruct
  single-scattering specular as `prefiltered(R, r*(levels-1)) * (F0*A + B)`.
  The initial IBL diffuse partition is `(1-metallic)*(1-Fview)`, where
  `Fview=F0+(1-F0)*(1-NoV)^5`; quantify its deviation from full integration.
  Do not quietly combine this with a different roughness-Fresnel or energy-
  compensation recipe. Integrate the same height-correlated visibility for A/B;
  do not substitute a differently remapped Schlick-G shortcut. Roughness, LUT
  axes, sample PDF and visibility conventions must be tested together.
  The LUT is linear data with clamp-to-edge, linear filtering and no generated
  mip chain; the specular cube uses only its explicitly prepared mip levels.
- AO attenuates indirect terms only; linked environment gain applies consistently
  to background/diffuse/specular. Any independent background-art gain is explicit,
  never a hidden lighting multiplier. Environment rotation is a runtime lookup transform,
  not a reason to rebuild the bake. Exposure/tone mapping are also runtime-only.

Orientation contract: input HDR rows are top-down;
`u = .5 + atan2(z,x)/(2π)`, `v = acos(clamp(y,-1,1))/π`.
Yaw lookup is `R_y(-yaw) * worldDirection`, shared by sky and both lighting paths.
For cube face texel coordinates a=2s-1, b=2t-1, normalize these directions:

| Face order | Direction |
| --- | --- |
| +X, -X | `(1,-b,-a)`, `(-1,-b,a)` |
| +Y, -Y | `(a,1,b)`, `(a,-1,-b)` |
| +Z, -Z | `(a,-b,1)`, `(-a,-b,-1)` |

Store the face order/row convention in the artifact schema and test edge/corner
continuity. This table, not a hand-applied image flip, governs CPU bake and GL
upload. An asymmetric axis-colored environment must agree in the background,
mirror reflection and diffuse illumination, including yaw +90°.

There are three distinct identities:

1. Mesh import key: source bytes, import settings, parser/Mikk/decoder identities,
   geometry format and coordinate/tangent/row conventions.
2. Environment prefilter key: source content/evaluation identity, interpretation,
   decode/color convention, cube mapping,
   resolutions/formats/sample counts, sampling/PDF and prefilter algorithm version.
3. BRDF LUT key: BRDF/visibility/roughness recipe, integrator version, resolution,
   samples and format, independent of the HDR source.

Record producer/compiler/FP mode/architecture separately from target/resource
format; include the producer fingerprint in cache identity where floating-point
output is not promised bit-identical across producers. Hash actual artifact bytes
and verify them. Reuse canonical build identity rather than re-deriving host names
in each tool. Any shader-side BRDF change that affects LUT interpretation must
invalidate/require a matching recipe; source shader caching alone cannot do that.

Derived working caches belong under application/tool cache roots. Accepted,
portable runtime artifacts belong to the prepared project under owned paths and
must be included in export. A missing/stale bake is a preparation diagnostic, not
an excuse to run an unbounded bake at player startup. Preserve the last accepted
generation through failed/cancelled preparation and publish atomically.

Keep producer and runtime dependencies typed: the authoring `.henv`/import manifest
describes source inputs and preparation intent; its accepted portable descriptor
references the required cooked outputs and compatible semantic/recipe identity.
Source hashes/revisions remain provenance, not runtime paths that must still exist.
Native closure/readiness distinguishes **producer input required to rebuild** from
**prepared dependency required to run**. Runtime validates the accepted descriptor
and artifacts without opening original GLB/HDR files. Authoring changes that need
preparation cannot masquerade as a current generation merely because old outputs
exist; failed candidates leave accepted viewing state intact with a visible error.

### Future changes use these boundaries, not extra systems in this slice

- **Atmosphere and celestial objects:** a later atmosphere source evaluates
  radiance from typed parameters. A time-of-day controller owns one clock/state
  and updates source state and explicitly bound sun/moon light entities by stable
  identity; it does not create competing hidden lights. The current HDR image may
  already contain a sun; its authored directional light is deliberate additional
  illumination, not an automatic brightest-pixel extraction. A later procedural
  model must state which celestial energy belongs in direct lighting versus IBL
  to avoid double-counting, while a visible disk can remain background content.
  Stars are source/background content unless a later feature gives them another
  lighting role. Multiple direct celestial lights require a real supported light
  policy; do not silently ignore a moon under the one-light baseline.
- **Time/weather/volumetrics:** source changes can request new prepared lighting
  independently from background evaluation. Weather parameters are not a bag of
  fields added to every environment asset now. Participating-media/volumetric
  effects would be explicit per-view passes with depth and lifetime requirements,
  rather than arbitrary modifications to the IBL cube loader.
- **Local reflection probes:** future probe components own bounds/capture pose
  and a source/prepared generation. They can reuse radiance preparation; selection,
  blending and parallax policy belong to future lighting submission/evaluation.
  No probe arrays, capture scheduler or resolver is required for the global case.
- **Asynchronous updates:** snapshot source revision, recipe and project lifetime;
  prepare/upload a candidate and accept only the still-current compatible result
  at a safe boundary. Retain the previous complete accepted generation on failure,
  cancellation or stale completion; never mix new diffuse with old specular/LUT.
  Retire resources after all frame references release, before context teardown.
  If background eventually advances faster than lighting, its bounded-lag policy
  and both effective revisions must be explicit. Reference captures freeze one
  coherent state. No async runtime scheduler is implemented for offline-first IBL.

## Authoring, runtime and packaging integration

Add typed MeshRenderer/DirectionalLight inspectors and a material document panel;
scene rendering settings select presentation, environment, yaw/gain and exposure.
Project Settings expose new-scene defaults and effective runtime requests; camera
and workspace tools expose projection/navigation independently. Reuse existing
document guards, asset-root payload decoding, progress UI and bounded operations.
Initially material edits apply through explicit Save/Reload; live material graphs,
node editors and material inheritance are unnecessary. Implement useful defaults,
bounds framing and mesh/prefab drop placement. Complete planar/orbit tool selection
using existing controllers; avoid an unrelated navigation rewrite. Native exposure
and history for each feature are part of its checkpoint, not an end-stage backlog.

Version the canonical scene/prefab/project/material/style contracts for their real
new fields and color semantics, starting from current scene/prefab version 2.
Do not reserve a schema version here independently of the final field changes.
Update repository content deliberately as described above; structural old-document
support must not introduce a historical gamma path. Unknown future data continues
to fail without rewriting the source. Update `AllComponents`,
copy/duplicate/instantiate, metadata-only decoding, exact dirty snapshots, structural
commands and root scene-setting history together. No transient mesh caches,
resolved materials, light buffers or bake progress enter serialized state/history.
Use existing FileDocument conflicts and document-owned undo for `.hmat`/`.henv`.

`RuntimeSession::Validate` must preflight mesh ranges, assigned slots, textures,
environment generations and rendering settings before publishing Play or a scene
transition. Preserve accepted runtime state when a candidate fails, as with current
managed/lifecycle behavior. Test repeated Play/Stop, simulation, project switching
and shutdown with resources pinned. The demo can rotate a mesh through existing
`Entity.LocalRotation` and request a scene transition; no Mono rewrite or general
managed material API is needed. If new managed wrappers are exposed, add explicit
ScriptGlue registration, type/ABI validation and stale-entity/domain tests.

Extend native metadata-only closure validation for new documents/artifacts and
carry its dependency inventory into existing Python packaging. Do not duplicate
asset semantics in Python. Runtime must consume prepared geometry/textures/IBL,
without original model/HDR, importer, source SDK, Python or an editor cwd. The
current packager copies all eligible Assets, so producer source bytes may initially
travel incidentally; that does not make them runtime dependencies. Prove loading
with those source paths unavailable in disposable extracted copies. Cooked-only
pruning is later work, not a new global asset-database requirement.

Use existing InstallationLayout/Resources/UserData/Save roots. Include shaders,
all material maps, meshes, environment mips/LUT, managed assemblies, notices and
recipe/checksum metadata. Mac binary modifications/signing stay inside the
canonical bundler, inside-out before final archive verification. Do not conflate
source-SDK authoring readiness with runtime asset readiness. Public signing and
notarization remain a separate authorization/distribution milestone.

## Permanent Rendering Reference project

**Implement early:** maintain `examples/RenderingReference` as ordinary native,
content-first project data. Stage 1 makes its 2D/color/camera core runnable; later
stages add real geometry and lighting cases as those features work. Do not mark a
planned case as passed or present a blank scene as PBR evidence. The reference
remains supported through future rendering changes. A separate Rendering Showcase
can pursue art direction; it must not continually restage the correctness fixture.

Use an immutable **Core_v1** case definition and additive, named/versioned cases.
Never move its cameras, change its light/environment/exposure or tune material
values to make a new renderer look better. An intentional convention/correctness
revision gets documented numerical evidence, reviewed expected results and a new
case version; retain old case metadata for comparisons. New shadow/effect cases
extend the suite rather than changing baseline illumination.

| Stable content | Purpose and fixed controls |
| --- | --- |
| Named Perspective, Orthographic, SurfaceDetail and UI cameras | Authored transforms/projection/clipping, frozen scene time/seed and named active-camera selection; 1280×720 physical-pixel reference output with separately recorded logical UI extent. Editor camera preview uses these cameras, not a hand-positioned orbit view |
| Unit/coordinate references | One-meter plane/cube, axis labels, neutral gray/white/black patches and dimensional markers. Check scale, handedness, clipping and declared linear values |
| Roughness/metallic sphere grids | Fixed materials: roughness 0 (evaluated at the documented floor), .1, .25, .5, .75, 1 and metallic 0, .5, 1; identical geometry, normals and positions across runs |
| Normal/tangent cases | Mapped versus geometric detail, UV seams, mirrored UVs, negative and nonuniform transforms, double-sided backs, masked holes; independent winding/TBN expectations |
| DirectOnly, IBLOnly and Combined saved cases | Explicit sun direction/intensity, environment enablement, gain and yaw; EV 0, Neutral tone mapping and no auto-exposure. Do not hide lights in test-only renderer code |
| Constant, AsymmetricAxes and Photo environments | Analytic constant radiance, directional labels/highlights and one pinned modest photographic HDR. Fixed hashes/profiles expose normalization, orientation and realistic high-frequency approximation error |
| 2D/unlit/UI coexistence | Orthographic and perspective sprites/world text, unlit material samples, alpha overlaps, data/color textures and screen UI. Paired display-referred and scene-referred cases document their intentionally different world-exposure behavior |

Scene/case manifests record stable entity/camera IDs, all referenced content and
hashes, authored matrices/material/light/environment values, source and accepted
generation/recipe identities, fixed clock/seed, logical/pixel extents, output
formats/encoding and expected case version. A capture result additionally records
commit/configuration, backend/device/driver, detected/effective capabilities and
any intentional capture override. These are test metadata, not serialized device
capabilities in project settings. Pixel comparisons use defined tolerances/regions;
producer hashes need not imply bit-identical floating-point output on every CPU.

Use normal import, document save/load, SceneRenderer and RuntimeSession. Editor,
Nutella and exported packages consume **the same reference content**, with startup
scene/case selection explicit and exported prepared closure verified without
source/producer access. Reuse the existing regression executable/fixture boundaries;
no hidden alternate renderer or scene-loading bypass. Engine-authored synthetic
assets carry provenance; third-party cases retain licenses, original bytes/hashes,
attribution and recorded transformations. The sources below are candidates until
acquired and admitted during implementation, not current fixture dependencies.

Numerical assertions and independent float reference images remain required.
The maintained project makes differences reproducible and inspectable; an
attractive reference scene alone cannot establish correct energy or color math.

## Independent correctness checks

Use a small maintained fixture set, numerical assertions and a few fixed reference
captures. Do not rely on a moving camera or a plausible-looking helmet. References
must specify source/recipe SHA, camera, light, environment orientation/intensity,
exposure, output encoding and resolution.

| Layer | Independent check and acceptance target |
| --- | --- |
| Geometry/CPU | Interleaved/normalized accessors, uint8/16/32 indices, non-indexed triangles, multiple ranges/materials, node instancing, malformed offsets/counts, cancellation and path confinement. Reject unsupported features without partial publication. Preserve old data on reimport conflicts |
| Transforms/TBN | Golden matrices and reconstruction, non-uniform scale, negative determinant, parent rotation/scale, tangent seams/mirrors, missing normals/tangents and singular inputs. Orthogonality/length within 1e-4 for ordinary fixtures; render winding and double-sided cases against independent geometry |
| Color/data | sRGB .5 decodes to approximately .214041, while data/alpha .5 remain .5; compare GPU samples/readbacks. Black/white color mips average in linear space. Picker round trips, scripted colors, sprite UVs and UI styles use the same contract. Verify no extra gamma, alpha transform or RGBM decode. Quantized tests use specified format error bounds |
| Direct BRDF | A separately written FP64 evaluator integrates/evaluates the documented model; do not call production shader/bake helpers for the expected result. Check reciprocity, finite grazing behavior, nonnegative output, metallic diffuse removal and incident-light scaling. Well-conditioned shader points should agree within roughly 2e-3; fix exact tolerances with explicit precision/error analysis |
| Irradiance/orientation | Constant HDR C gives πC (initial target <=.5% integral error) on every face; asymmetric six-axis/polar patterns test yaw, poles, seams and background/reflection agreement. Mirror material must not hide a rotation mismatch |
| Specular/LUT | Independent high-sample FP64 hemisphere quadrature, not the production Hammersley loop. Test NoV .05/.25/.5/1 and roughness floor/.25/.5/1; initial LUT absolute-error target .01 with convergence evidence. Constant environments stay constant through normalized prefilter mips. Verify mip endpoints, monotonic lobe broadening and A/B/channel/axis order |
| Energy | White-furnace and roughness/metallic grids before tone mapping. No energy creation beyond declared numerical tolerance (initial <=2% ordinary cases); expected single-scattering loss must match the reference. Quantify split-sum/diffuse-Fresnel approximation error separately from implementation error, especially high-frequency HDR/grazing cases |
| Reference images | Render small analytic sphere/plane scenes against independently integrated float reference images, before exposure/tone mapping; inspect bounded regions and numeric errors, not a loose whole-image score. Use matching-model references. Modern Khronos/Filament images can differ through multi-scattering/tone choices and are not automatic pixel oracles |
| Final display/UI | Exposure +1 doubles pre-tone scene-referred world radiance, including unlit world content; display-referred world bypasses exposure/tone. Neutral tone output and exact sRGB transfer match CPU vectors. Screen UI brightness, alpha and focus are unchanged by world exposure. Existing 2D tutorial/text/style/sprite checks and deliberately reviewed example appearance remain positive |
| Actual GPU/picking | Apple GL4.1, accelerated supported Linux and Windows paths; nearest visible entity wins, masked holes select background, UI owns its painted hits, postprocess writes no IDs. Test resized/2x framebuffer coordinates and state restoration after each pass, including errors |
| Authoring/lifecycle | Actual Inspector create/assign/edit, gizmo, undo/redo, prefab duplicate/instantiate, save/reopen, failed reload, Play/Stop, script reload and scene transitions. Positive authoring evidence; no fixture that edits fields behind the production operation counts as full authoring acceptance |
| Presets/settings | SDK-free native 2D/3D creation, template/canonical schema validation, New Scene defaults, later settings edits, planar/orbit workspace persistence and orthographic PBR/perspective unlit cases. Invalid required settings reject candidates; requested/effective provenance and restart boundaries remain visible. Scene/project/player/editor owners do not overwrite each other |
| Packages | Debug/Release builds and maintained suites; relocated 3D Nutella and Hazelnut with source/producer resources denied, valid closure/notices/checksums and unchanged ad-hoc signatures. Existing five 2D games remain qualified. Headless or unexecuted checks are not runtime passes |

The stated numeric targets are starting engineering criteria, not measurements.
Set reproducible sample counts/seeds and analytic bounds before accepting captures.
Do not add retries or widen thresholds simply to turn a discrepancy green.
Provide focused debug views for base color (linear/display), normals/tangents,
metallic/roughness/AO, direct/diffuse-IBL/specular-IBL, cube face/mip, LUT and HDR
before tone mapping. These expose common channel, π, roughness and orientation
mistakes; they do not replace the assertions.

### Reference assets and redistribution

Use engine-generated planes/cubes/spheres and synthetic constant/axis/highlight
environments first. Add a small pinned subset of Khronos assets from inspected
repository revision `8f492390ddc7a2ae221a1369ef560589cf8c70fb`:

- [NormalTangentTest](https://github.com/KhronosGroup/glTF-Sample-Assets/blob/8f492390ddc7a2ae221a1369ef560589cf8c70fb/Models/NormalTangentTest/README.md): AGI/Ed Mackey, CC0; compares mapped normals to actual geometry.
- [NormalTangentMirrorTest](https://github.com/KhronosGroup/glTF-Sample-Assets/blob/8f492390ddc7a2ae221a1369ef560589cf8c70fb/Models/NormalTangentMirrorTest/README.md): AGI/Ed Mackey, CC BY 4.0; supplied tangent/mirror checks.
- [MetalRoughSpheres](https://github.com/KhronosGroup/glTF-Sample-Assets/blob/8f492390ddc7a2ae221a1369ef560589cf8c70fb/Models/MetalRoughSpheres/README.md): AGI/Ed Mackey, CC BY 4.0; texture-channel/roughness grid.
- [Studio Small 09](https://polyhaven.com/a/studio_small_09), Sergej Majboroda: use a small Radiance HDR asset under [Poly Haven's asset CC0 license](https://polyhaven.com/license), not its website preview renders. Save exact download URL, byte hash and original notice at acquisition.

Verify each selected GLB uses the supported subset before admitting it; an
unsupported fixture must not become a silently counted pass. Record exact bytes,
authors/licenses and transformations in maintained fixture provenance. Do not
assume every Khronos model is CC0: for example, the inspected
[DamagedHelmet notice](https://github.com/KhronosGroup/glTF-Sample-Assets/blob/8f492390ddc7a2ae221a1369ef560589cf8c70fb/Models/DamagedHelmet/README.md)
also lists earlier CC BY-NC material. It is not the default redistributable example.
No external models/HDR binaries were downloaded by this audit.

## Ordered implementation checkpoints

These stages form one bounded first 3D milestone in an engine that keeps 2D
first-class. Contracts land with their first real consumer; future extension
points do not require future systems. Each checkpoint is runnable and reviewable;
do not call the full vertical slice finished before stage 6. Preserve Windows,
Linux and native macOS, OpenGL 4.1, Premake/vendor isolation, history, lifecycle
and packaging throughout.

| Stage | Purpose, prerequisites and likely files | Approach, acceptance and checkpoint |
| --- | --- | --- |
| 1 — settings, canonical color and permanent 2D reference | Requires merged Mac baseline. ProjectConfig/serializers, native ProjectCreation/templates and NewScene factory; AuthoringPanel/RenderingSettings, EditorState/camera/gizmo integration; Scene/DocumentSchema/history; RendererPolicy, Texture/Framebuffer/Renderer2D/UI shaders and OpenGL adapters | Establish project/scene/view ownership and resolved-value provenance. Implement canonical color and the lightweight display-referred path; update all five examples/styles/script colors through reviewed conversion or reauthoring. Complete planar/orbit tools and native defaults/settings with history. Start RenderingReference Core_v1 with runnable color/2D/camera cases and fixed metadata. Define preset DTOs now, expose only working starter choices; no fake 3D starter. CPU round trips, color/UV/picking tests, positive UI authoring and reviewed example comparisons pass on actual supported GPUs. Commit: sound settings/color foundation with fully usable 2D |
| 2 — format-neutral import and native asset authoring | Requires stage-1 schema/color. New imported scene/image DTOs, common validation/cook/publication and runtime Mesh/Material codecs; private GLB adapter; `HazelProject`, Premake/notices; Components/Scene copy/metadata; ContentBrowser and material document/slot Inspector | Qualify TinyGLTF 3.0.1 + Mikk on Apple Clang/MSVC/GCC, valid and bounded malformed fixtures/sanitizers where available. Produce canonical geometry/materials/detached prefab with stable mappings, atomic reimport/conflict/cancellation and parser-free runtime loading. Native import/progress, material create/duplicate/edit/save and assignment/history work now, with metadata inspection before rendering. Extend reference geometry/TBN fixtures and provenance. Parser gate failure stops that adapter choice, not validation. Commit: independent import pipeline with usable native authoring |
| 3 — shared submission and direct PBR | Requires stage-2 assets. SceneRenderer/view and common inputs; Scene runtime/editor submission, RuntimeSession view ownership; typed RendererAPI state/resources and OpenGL adapters; mesh/material/light/camera inspectors, EditorLayer/Nutella output and shaders | Real static indexed meshes, direct PBR, scene-referred HDR/exposure/tone mapping, coherent world-2D/UI coexistence, mask/depth/Retina picking and state restoration. Complete bounds framing, sun/camera/scene controls, dirty/history/save/reopen and working direct-lit 3D preset; document the still-pending IBL portion. Both hosts consume the same saved content and common inputs; orthographic PBR/perspective unlit reference cases work. CPU BRDF oracle and actual Apple/portable GL4.1 checks pass. Commit: usable direct-PBR editor/runtime checkpoint, not completed 3D milestone |
| 4 — environment ownership and offline HDR/IBL | Requires stage-3 resources/BRDF recipe. `.henv` source/generation codecs, image normalization, native EnvironmentBake and prepare dispatch, cube/LUT upload, scene environment settings and document/job UI | Separate source intent, preparation policy and accepted lighting generation. Native select/prepare/progress/cancel/history with clear stale/missing diagnostics; no runtime bake. Freeze the shared BRDF/LUT recipe and pass constant/axis/furnace/LUT/independent-image checks on CPU and M4. Finish neutral prepared 3D starter, DirectOnly/IBLOnly/Combined and Photo reference cases. Full native project creation still needs no SDK, parser, bake or GPU. Future dynamic-source/update contracts add no scheduler now. Commit: complete first PBR+IBL and native authoring |
| 5 — runtime, transitions and export closure | Requires stage-4 content. RuntimeSession readiness/transition/lifetime, native metadata audit, packaging/inventory/prepare CLI and fixture scripts | Same saved scenes, assets and screen UI in Nutella; one project transitions between lightweight 2D and HDR/PBR scenes. Existing managed transforms manipulate a mesh without a scripting rewrite. Repeated Play/Stop/reload/project switch/teardown retain accepted generations correctly. Export RenderingReference plus a small mixed-content example; source/producer-denied relocation, dependency failures, notices/checksums/signatures pass. Commit: standalone first slice and reference-package closure |
| 6 — qualify, measure and document | Requires complete slice. Maintained test inventory/CI where suitable, reference metadata/captures and current guides | Debug/Release on all supported targets; all five 2D examples/tutorials retain positive acceptance after reviewed color updates. Actual Apple GL4.1, available older accelerated hardware, native authoring/history/lifecycle and relocated packages pass. Record quality/effective settings and unavailable hardware honestly. Headless/CPU tests are not rendering passes. Commit: qualified permanent 2D plus first complete 3D milestone |

Recommended prompt grouping: **(1)** stage 1 settings/color/2D reference;
**(2)** stage 2 import and native asset authoring;
**(3)** stages 3–4 common rendering/direct PBR/IBL with native controls and independent
checks (split at the direct-lit checkpoint if needed);
**(4)** stages 5–6 runtime/export and qualification. Keep these on a
3D implementation branch created after review; this plan branch contains no code.

For performance, use existing asynchronous GPUTimer intervals plus CPU submit,
import/bake time, peak memory, draw/triangle counts and texture/target bytes.
Measure cold/warm behavior separately, with resolution and driver recorded.
Separate **baseline supported functionality**, **optional device capabilities** and
**prepared quality/performance profiles**. The baseline is the documented 4.1
feature set plus this slice's semantics, not one computer's triangle budget.
Optional paths require capability-driven effective-value reporting; quality profiles
change defined resource/work budgets without silently removing required lighting
or substituting missing bakes. Offer another profile only when prepared and tested.

Proposed initial measurement envelope: 1280×720 physical pixels (record actual Retina
framebuffer extent), <=100k visible triangles, <=64 opaque/
masked mesh draws, one sun, modest 512-pixel material maps and the IBL profile
above; target <=128 MiB resident 3D asset data. Aim for <=16 ms world GPU time on
the M4 and <=33 ms on the documented older Intel path, then revise the envelope
from evidence. These are provisional profiling targets, not feature admission
limits or verified throughput. Grandpa's limits must not dictate every future
feature: measure the baseline and define explicit quality/optional requirements
when adding features rather than weakening semantics globally. The current
record documents HD4000 Linux GL4.2/forced-4.1, not a measured 3D capability of
“Grandpa.” Software CI cannot supply that hardware performance acceptance.

CI can run CPU import/codec/bake/closure checks on existing jobs and controlled
GL4.1 numerical/picking tests on Linux/Windows test drivers. Hosted Mac keeps its
build/CPU/service boundary unless the runner actually supplies suitable GPU access.
Local M4 checks supply Apple rendering evidence. No new desktop-control service,
gameplay bot, broad screenshot campaign or self-hosted runner is required.

## Future boundaries and non-goals

- **Metal:** keep CPU assets/material semantics/view inputs/pass resource ownership
  independent of GL enums, resource IDs and binding syntax. Explicit target formats,
  samplers and pass state are useful real contracts. A future backend maps cube
  orientation/output conventions at its boundary. No render graph, universal shader
  language or speculative backend registry now.
- **Shadows:** a later directional depth pass can use the same world transforms,
  bounds and light owner. Cascades, contact shadows and shadow filtering are not
  required for this slice.
- **Animation/skinning:** retain source mesh/node mappings and versioned geometry
  layouts; later add skeleton/animation assets and vertex streams. Do not reserve
  mandatory unused joint buffers or implement skinning now.
- **Physics:** render transforms/bounds do not create collision ownership. Keep
  Box2D's planar/root restrictions. 3D physics and collider generation are deferred.
- **More lights/larger scenes:** start with CPU bounds culling, immutable resources
  and explicit draw lists. Profile light count, CPU cost and bandwidth before
  choosing Forward+, deferred, instancing or spatial acceleration structures.
- **Environment evolution:** the source/policy/generation boundary accommodates
  atmosphere, celestial/time state, weather, local probes and asynchronous updates
  without implementing them in the image/offline milestone. Keep one direct-light
  authority and explicit radiance/energy partition.
- **Ray tracing:** retain recoverable geometry/material/instance information, not
  speculative acceleration structures or API-specific scene data.
- Deferred: mesh alpha-BLEND/transmission, advanced material extensions, external GLTF
  URIs, compressed textures/meshes, HDR displays/wide gamut, MSAA/TAA/bloom,
  dynamic probes/day-night rebakes, GI, ray tracing, material graphs and automatic
  linked-prefab reimport. No replacement of existing platform adapters or libraries.

## Decisions and remaining uncertainty

No owner decision blocks beginning stage 1. Recommended defaults are first-class
2D/3D creation presets, one linear color contract with lightweight unlit and
scene-referred presentation, format-neutral import/cook boundaries, the supported
GLB subset, TinyGLTF 3.0.1 qualification first, Mikk tangents, simple forward,
one directional light, single-scattering GGX, offline-prepared IBL and Neutral tone
mapping. No permanent historical gamma resolver or broad dependency upgrade.

Owner review is useful for the **intended appearance of deliberately updated
examples**, a separate showcase's art direction and later shipping performance
profiles. Default to preserving palette/readability/gameplay intent under correct
color math, licensed diagnostic reference content plus a neutral starter, and the
720p/30-fps older-machine measurement envelope. These preferences do not block
contract work or require a renderer/backend decision now. Freeze reference core
lighting/cameras independently of showcase preferences.

Unresolved technical gates are importer compilation/validation on the actual
toolchains, measured bake quality/time and half-float range, quantified IBL
approximation error, and real older-GPU throughput. The stages above give the
smallest experiments and acceptance criteria that resolve them. No new library,
3D render or performance result is claimed by this read-only audit.

## Stage 1 implementation status

Implementation is isolated on `feature/3d-foundations` from the audited merged
master. The approved design above remains the roadmap. Stage 1 adds native
new-scene defaults/factory, scene-owned display-referred background, private
Planar/Orbit navigation, canonical linear RGB and a single final output encoding.
Only the working 2D starter is exposed. Stage 1 does not implement
scene-referred/PBR/IBL or import; stage-2 progress is recorded below. Qualification evidence and any remaining limitations
are recorded in [the stage-1 implementation record](3d-foundations-stage1.md); proposals above
must not be read as implemented product capabilities.

## Stage 2 implementation status

Stage 2 implements content-only packaging, pinned producer-only GLB/Mikk
dependencies, format-independent CPU cooking/publication, native asset codecs and
Hazelnut material/import/prefab authoring. Local Debug/Release and relocated
package qualification passed; canonical CI and remaining Windows limits are
tracked in [the stage-2 record](3d-import-stage2.md). The subsequent direct-rendering
checkpoint is recorded below. Stage 2 did not implement rendering or IBL; Metal
and additional import formats remain outside these checkpoints.


## Stage 3 implementation status

The original Hazel checkout is canonical. Stage 3 adds shared scene/view inputs,
accepted indexed static meshes, direct single-scattering GGX, one directional key,
HDR exposure/fixed Neutral presentation and independent screen UI. Native creation
has prepared direct PBR and 2D starters; native light/material/camera/scene tools,
history and bounds framing share existing ownership. Additive Direct_v1 cases and
independent CPU/GPU checks are recorded in [the Stage 3 record](3d-direct-stage3.md).
Qualification status there distinguishes actual M4/CI results from pending work.

## Stage 4 implementation status

Stage 4 adds bounded native HDR preparation, portable accepted lighting generations,
GGX-prefiltered cubemaps and a shared BRDF LUT, common editor/player IBL and
environment background, native environment documents/history/jobs and a bundled
complete PBR starter. Additive IBL_v1 reference cases preserve prior frozen bytes.
Actual numerical, M4, native authoring/package and CI evidence is tracked in
[the environment record](3d-environment-stage4.md). Stage 5 has not begun.
