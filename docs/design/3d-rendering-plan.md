# First 3D/PBR/IBL vertical slice

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

This audit changed no engine code, dependencies or settings, installed nothing
and ran no desktop automation. Findings below come from the merged source;
**proposals and numerical budgets are recommendations, not implemented features**.
External implementation reports are identified as such and were not reproduced.

## Intended endpoint and scope

Complete one path: import a static glTF model, create/assign a metallic/roughness
material, place and manipulate it in Hazelnut, view it through a perspective
camera with a directional light and HDR environment/IBL, save/reopen, then export
and run the same scene in Nutella. Include a small managed behavior and existing
screen-space UI. PBR and IBL belong to this first milestone; direct-light-only
rendering is an intermediate checkpoint, not the finished milestone.

Recommend **ordinary forward rendering**, one directional light without shadows,
opaque/alpha-mask materials, static geometry and offline CPU environment baking.
Use the existing OpenGL 4.1 backend, Premake and Python command dispatcher. Do not
replace GLFW, Mono, resource ownership, the scene graph or the editor command
system. No Blinn-Phong stage is necessary.

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
| [Serialization](../../Hazel/src/Hazel/Scene/SceneSerializer.h), [schema](../../Hazel/src/Hazel/Core/DocumentSchema.cpp), [prefabs](../../Hazel/src/Hazel/Scene/Prefab.h) | Strict keys/version rejection, candidate loads, metadata-only mode, exact authored comparisons; detached subtrees and UUID remapping | Current scenes write version 2. Add deliberate version-3 support for 3D fields and new prefabs, preserving old inputs. Extend every component inventory/copy/snapshot/validation path, not just YAML writing. Prefabs have no linked override/reimport propagation |
| [Document history](../../Hazelnut/src/Authoring/DocumentHistory.h), [StructuralCommand](../../Hazelnut/src/Authoring/StructuralCommand.cpp), [FileDocument](../../Hazel/src/Hazel/Core/FileDocument.h) | Bounded value-only commands, identity/version guards, exact dirty checks, save conflicts/recovery and atomic publication | Add authored mesh/material-slot/light/settings coverage. History must never own GPU resources or include derived cache/version handles. Material/environment files need their own document ownership |
| [Editor viewport](../../Hazelnut/src/EditorLayer.cpp), [hierarchy panel](../../Hazelnut/src/Panels/SceneHierarchyPanel.cpp), [Content Browser](../../Hazelnut/src/Panels/ContentBrowserPanel.cpp) | Perspective gizmos, existing scene/prefab drops, root-relative payloads, logical-to-framebuffer conversion and integer entity picking | Add mesh import/assignment and typed material/environment editing. Reuse transactions, operation availability, native tool/process discovery and progress/cancellation; no generic editor rewrite |
| [RuntimeSession](../../Hazel/src/Hazel/Scene/RuntimeSession.cpp), [Nutella](../../Nutella/src/NutellaApp.cpp), [application teardown](../../Hazel/src/Hazel/Core/Application.cpp) | Candidate scene validation, Play copies, failed-transition recovery, lifecycle/input/storage isolation; project assets released while GL context is alive | Extend validation to complete 3D asset closure before Play/transition publication. Nutella currently renders through the session directly to the active target; it needs the same HDR view pipeline as Hazelnut |
| [Managed entities](../../Hazel-ScriptCore/Source/Hazel/Scene/Entity.cs), [ScriptGlue](../../Hazel/src/Hazel/Scripting/ScriptGlue.cpp) | `Entity.LocalTranslation`, `LocalRotation` (radians), `LocalScale`, `WorldMatrix`, parenting and prefab instantiation already support 3D values | A spinning/moving mesh demo needs no scripting rewrite. The `TransformComponent` wrapper itself exposes only translation; do not mistake that for absence of local rotation on Entity. Camera wrapper lacks perspective FOV access; add only if a demonstrated script needs it. New exposed component wrappers require explicit registration/ABI tests |
| [Runtime UI](../../Hazel/src/Hazel/UI/RuntimeUI.cpp), [scene UI](../../Hazel/src/Hazel/Scene/SceneUI.cpp) | Ordered paint, independent logical/pixel metrics, clipping, text coverage, input scopes, depth-disabled UI and state restoration | Move screen UI after world tone mapping and before final encoding for 3D scenes; preserve clipping, entity IDs, text alpha and exposure independence |
| [Native audit](../../tools/SpriteAssetAudit/main.cpp), [closure code](../../Hazel/src/Hazel/Assets/ProjectAssets.cpp), [packaging](../../scripts/internal/packaging.py), [Mac bundling](../../scripts/macos/packaging.py) | Native metadata-only dependency validation, Python inventory/managed/native closure, complete eligible Assets copying, resource layout and signing | Extend the existing native audit to mesh/material/environment/artifact dependencies. Its historical name need not force a second independent auditor. Python must not learn a second glTF/material parser |

## Dependency decisions

No dependency was acquired or built during this audit. Existing GLFW, Glad,
GLM, YAML, ImGui/ImGuizmo, typography, Mono, audio and pinned shader tools remain.
The recommendations below need focused implementation-time qualification.

| Candidate | Evidence, tradeoff and recommendation |
| --- | --- |
| **TinyGLTF 3.0.1** | Preferred importer candidate: tag resolves to `0d9ce1a9561688c6c6e7471430d15a50a0b15f1a`. Upstream's [v3 README](https://github.com/syoyo/tinygltf/blob/v3.0.1/README.md) describes a C11-first API, arena ownership, structured errors and validation/security fixtures. Inspecting that pin confirms `tiny_gltf_v3.c`, `tiny_gltf_v3.h`, `tinygltf_json_c.h`, byte-based `tg3_parse_glb` and optional filesystem/image I/O. Use the C translation unit through Premake, own byte I/O, and disable optional image/writer/coroutine paths. [MIT license](https://github.com/syoyo/tinygltf/blob/v3.0.1/LICENSE); preserve its bundled numeric-parser/serializer notices too. Native arm64/MSVC/GCC compilation and malformed-input behavior are **unproven here**, so stage 1 gates adoption |
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

## Asset ownership, import and reload

Proposed names below are new files, not existing facilities. Extend
`Hazel/src/Hazel/Assets` with typed Mesh/Material/Environment codecs/resolutions;
put producer-only GLTFImport/EnvironmentBake code in a small import module linked
by `HazelProject`. Keep the engine/runtime loader independent of parser internals.
Add native `HazelProject` import/prepare operations and canonical Python dispatch;
the editor uses its existing native ToolRunner/process/cancellation boundary with
a validated complete executable path, not shell commands or embedded Python.

1. Preserve the source `.glb` under project ownership. A per-import manifest records
   source hash, options, parser/tangent versions, source-index mappings, generated
   files and original notices. This is a manifest for one import, not an asset DB.
2. Produce `.hmesh` references plus a versioned binary geometry artifact, initial
   `.hmat` documents, extracted images and a detached `.hprefab` for the selected
   static node tree. The prefab owns node TRS; do not invent a parallel runtime
   model hierarchy. Repeated nodes share mesh resources. Scene instances remain
   detached under the existing prefab contract.
3. Use root-relative paths as current project identity. Within each mesh, persist
   material-slot IDs independently of display names. glTF names/array indices are
   not persistent authored identity. Reimport reconciles the stored map; ambiguous
   primitive/material reorder or removed slots must report a conflict rather than
   silently assigning another material. Preserve existing slot overrides.
4. Geometry is mesh-local. A first vertex layout is position/normal/tangent vec4/
   UV0/color vec4 (64 packed bytes); explicitly serialize fields, never dump GLM
   struct ABI. Expand indices to uint32, rebase per-primitive indices within the
   uploaded mesh buffer and retain indexed ranges/material slots/local bounds.
   Existing `DrawIndexed(count, firstIndex)` suffices; a base-vertex or indirect
   submission API is not necessary. Validate bounds and arithmetic before reads.
5. Generate flat normals with corner splits when missing. Honor supplied tangents;
   otherwise generate Mikk tangents against the normal map's supported UV set,
   then split/reindex. Reject unusable required normal-map UVs with a useful error.
   Require finite attributes, valid normals/tangents and non-singular rendering
   transforms. Define handling of degenerate triangles explicitly; never emit NaNs.
6. Stage under an owned temporary with cancellation/progress and input conflict
   checks. Publish immutable generation files under `Assets/Imported/<import>/...`
   and atomically replace the descriptor/manifest last. Reuse leases and atomic
   filesystem services. Never overwrite a user-edited material or prefab on
   reimport: compare accepted bytes, preserve editable files and report source
   changes. Updating geometry does not relink detached scene instances.
7. `ProjectAssets` owns accepted immutable definitions and bounded GPU resolutions;
   scene components store references, not GL names. Failed reimport/reload retains
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

Scene rendering settings own one optional environment reference, environment
yaw/gain, exposure and the 3D-view mode. A directional-light component owns one
light's color/intensity, with direction from its world orientation. Validate at
most one enabled directional light initially. Do not hide competing environment
or exposure owners in Project, editor preferences and camera components.

## Geometry, material and color conventions

The [glTF specification](https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html)
is the import authority. The engine-specific rules below are the proposed single
3D convention; they must be written into fixtures and artifact recipe versions.

| Concern | Convention |
| --- | --- |
| Coordinates | Right-handed, +Y up, meters. Keep glTF mesh coordinates; the asset's +Z front does not change the camera's local -Z viewing direction. GLM column-vector matrices, world = parent world × local TRS; Euler authoring remains radians |
| Projection | Existing OpenGL -1..1 clip-space depth and ordinary depth test; new-scene near .1/far 100 are defaults to verify against bounds. No reverse-Z/clip-control dependency |
| Nodes | Preserve explicit local TRS, including signed scales. For matrix nodes use checked affine signed-TRS reconstruction, not silent shear removal. Existing Keep World/gizmo restrictions remain visible. No physics3D component is implied by a mesh |
| Normal basis | Normalize inverse-transpose world normals; transform and orthogonalize tangents. Bitangent uses tangent.w and world determinant sign. Negative determinant also reverses front-face winding. For double-sided back faces, reverse the final mapped shading normal before lighting; test both flat and mapped backs |
| UV/image rows | glTF UV0 and decoded top-left image rows stay paired; no green-channel inversion. Cooked mesh textures upload those rows directly via explicit pixel data, not the existing file constructor's row flip. Keep legacy sprite upload/UV behavior unchanged. Test a labeled asymmetric checker and tangent mirrors before adding exceptions |
| Lighting space | Linear RGB with sRGB/Rec.709 primaries and D65. Treat supported Radiance `.hdr` input as linear RGB under this documented convention; no LDR gamma conversion or automatic HDR-to-8-bit conversion. EXR/wide-gamut interpretation deferred |
| Base color / emissive | Image RGB is sRGB and must decode before filtering/lighting; alpha is linear. Factors are linear, `COLOR_0` is a linear multiplier. Normal/data maps must never use sRGB storage |
| Metallic/roughness | B = metallic, G = perceptual roughness, multiplied by corresponding factors. Other channels do not change those values. Occlusion is a separate R-channel use, even if it shares the image |
| Normal map | Linear RGB maps to tangent-space XYZ; scale X/Y, then normalize. Use glTF +Y convention. No global DirectX-green-flip setting |
| Occlusion/emission | Occlusion strength attenuates indirect lighting only in this baseline; do not multiply direct light or emissive output by AO. Emissive adds linear radiance; bloom is deferred |
| Alpha | OPAQUE ignores alpha. MASK discards below the authored cutoff before depth/color/entity-ID writes. Support double-sided. BLEND is explicitly rejected initially; future straight-alpha, depth-tested/no-depth-write back-to-front rendering needs one transparent-world ordering policy, not a fake opaque approximation |
| Exposure / display | Artist exposure in stops, `2^EV` multiplies world radiance before tone mapping. One [Khronos PBR Neutral](https://modelviewer.dev/examples/tone-mapping) curve, then screen UI in display-linear space, then exact piecewise sRGB encoding once. No `pow(x,1/2.2)` substitute and no second `GL_FRAMEBUFFER_SRGB` conversion |

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
owns transient framebuffer targets and draw packets. Root Renderer owns shared
shaders/default resources. All GPU creation/destruction remains on the context
thread and precedes window teardown.

Extract the common world submission from Scene's runtime/editor loops. Scene
update, scripts and UI routing stay with Scene/RuntimeSession; render-view input
provides view/projection/camera position, pixel/logical extents and destination.
Both hosts use the same pipeline. Do not expose ImGui, an editor camera type or
OS APIs to material/import code.

| Approach | Assessment for this slice |
| --- | --- |
| Simple forward | Chosen: one light, a few material maps and IBL, low draw counts, straightforward mask/double-sided/picking semantics. CPU bounds/frustum culling is sufficient |
| Forward+ | No demonstrated light-count problem; common compute/SSBO implementations exceed the 4.1 baseline. CPU light lists are possible later but unnecessary now |
| Deferred | More buffers/bandwidth, material encoding, resolve and transparency decisions, especially on older integrated hardware. No present requirement justifies it |

For a 3D-enabled scene, use an explicit sequence, without a render graph:

1. Bind/clear a one-sample HDR world target with depth and R32I entity IDs (-1).
   Draw the environment background without depth writes or entity selection.
2. Submit opaque and masked mesh ranges with complete material/depth/cull state;
   draw existing unlit world sprites/circles/text through the 3D view's defined
   linear-color scope. World depth/picking must agree. General blended mesh/world
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

Keep existing scenes on their current LDR/2D contract by default. The authored
3D view mode selects the new linear HDR pipeline; this is a compatibility boundary
for existing content, not a second PBR implementation. Extend Renderer2D's color
submission scope for 3D views instead of globally changing old sprite/UI colors.
Existing 2D examples must remain visually/functionally unchanged.
In that scope, image resolution requests sRGB color variants and vertex colors
convert once; glyph coverage/MSDF and data textures remain linear. Do not decode
an already hardware-decoded color sample again.

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

Recommend **offline CPU preparation in the native asset tool**, not runtime bake.
It works without a desktop/GPU on all three producer platforms and gives players
bounded startup. GPU fragment-pass baking is 4.1-feasible but adds context/tool
ownership and driver variability; compute-based baking is unnecessary.

One `.henv` definition references linear HDR source and a prepared generation:
background/radiance cube, diffuse irradiance cube, GGX specular mip chain, and a
matching shared BRDF LUT. Proposed first quality profile: background 256-face,
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
- AO attenuates indirect terms only; environment gain applies consistently to
  background/diffuse/specular. Environment rotation is a runtime lookup transform,
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
2. Environment prefilter key: HDR bytes, decode/color convention, cube mapping,
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

## Authoring, runtime and packaging integration

Add typed MeshRenderer/DirectionalLight inspectors and a material document panel;
scene rendering settings select environment, yaw/gain and exposure. Reuse existing
document guards, asset-root payload decoding, progress UI and bounded operations.
Initially material edits apply through explicit Save/Reload; live material graphs,
node editors and material inheritance are unnecessary. Implement useful defaults,
bounds framing and mesh/prefab drop placement, not a new navigation system.

Introduce scene schema version 3 for new 3D data and an explicit compatible prefab
version. Keep version-1/2 loading and legacy 2D encoding behavior; unknown future
data must continue to fail without rewriting the source. Update `AllComponents`,
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

## Independent correctness checks

Use a small maintained fixture set, numerical assertions and a few fixed reference
captures. Do not rely on a moving camera or a plausible-looking helmet. References
must specify source/recipe SHA, camera, light, environment orientation/intensity,
exposure, output encoding and resolution.

| Layer | Independent check and acceptance target |
| --- | --- |
| Geometry/CPU | Interleaved/normalized accessors, uint8/16/32 indices, non-indexed triangles, multiple ranges/materials, node instancing, malformed offsets/counts, cancellation and path confinement. Reject unsupported features without partial publication. Preserve old data on reimport conflicts |
| Transforms/TBN | Golden matrices and reconstruction, non-uniform scale, negative determinant, parent rotation/scale, tangent seams/mirrors, missing normals/tangents and singular inputs. Orthogonality/length within 1e-4 for ordinary fixtures; render winding and double-sided cases against independent geometry |
| Color/data | sRGB .5 decodes to approximately .214041, while data/alpha .5 remain .5; compare GPU samples/readbacks. Black/white color mips average in linear space. Verify no extra gamma, alpha transform or RGBM decode. Quantized tests use specified format error bounds |
| Direct BRDF | A separately written FP64 evaluator integrates/evaluates the documented model; do not call production shader/bake helpers for the expected result. Check reciprocity, finite grazing behavior, nonnegative output, metallic diffuse removal and incident-light scaling. Well-conditioned shader points should agree within roughly 2e-3; fix exact tolerances with explicit precision/error analysis |
| Irradiance/orientation | Constant HDR C gives πC (initial target <=.5% integral error) on every face; asymmetric six-axis/polar patterns test yaw, poles, seams and background/reflection agreement. Mirror material must not hide a rotation mismatch |
| Specular/LUT | Independent high-sample FP64 hemisphere quadrature, not the production Hammersley loop. Test NoV .05/.25/.5/1 and roughness floor/.25/.5/1; initial LUT absolute-error target .01 with convergence evidence. Constant environments stay constant through normalized prefilter mips. Verify mip endpoints, monotonic lobe broadening and A/B/channel/axis order |
| Energy | White-furnace and roughness/metallic grids before tone mapping. No energy creation beyond declared numerical tolerance (initial <=2% ordinary cases); expected single-scattering loss must match the reference. Quantify split-sum/diffuse-Fresnel approximation error separately from implementation error, especially high-frequency HDR/grazing cases |
| Reference images | Render small analytic sphere/plane scenes against independently integrated float reference images, before exposure/tone mapping; inspect bounded regions and numeric errors, not a loose whole-image score. Use matching-model references. Modern Khronos/Filament images can differ through multi-scattering/tone choices and are not automatic pixel oracles |
| Final display/UI | Exposure +1 doubles pre-tone world radiance. Neutral tone output and exact sRGB transfer match CPU vectors; UI brightness, alpha and focus are unchanged by exposure. Existing 2D tutorial/text/style/sprite checks remain positive |
| Actual GPU/picking | Apple GL4.1, accelerated supported Linux and Windows paths; nearest visible entity wins, masked holes select background, UI owns its painted hits, postprocess writes no IDs. Test resized/2x framebuffer coordinates and state restoration after each pass, including errors |
| Authoring/lifecycle | Actual Inspector create/assign/edit, gizmo, undo/redo, prefab duplicate/instantiate, save/reopen, failed reload, Play/Stop, script reload and scene transitions. Positive authoring evidence; no fixture that edits fields behind the production operation counts as full authoring acceptance |
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

These stages form one bounded 3D milestone. Each checkpoint should be reviewable;
do not call the vertical slice finished before stage 6.

| Stage | Purpose, prerequisites and likely files | Approach, acceptance and checkpoint |
| --- | --- | --- |
| 1 — contracts and native import | Requires merged Mac baseline. New Assets Mesh/Material/Environment codecs; producer GLTFImport module; `HazelProject`, Premake/dependency notices; Components/DocumentSchema/SceneSerializer/Scene copy and metadata-only paths | Qualify TinyGLTF 3.0.1 + Mikk under native Apple Clang, MSVC and GCC before committing the producer choice. Test valid GLB and bounded malformed fixtures under sanitizers where available; keep validation on. Then produce real cooked geometry/materials/prefab with portable references, conservative stable slots and atomic cancellation/conflict behavior. CPU codecs and round trips cover every new persisted field. Stop/reassess if the parser gate fails; do not bypass it or start a parser rewrite. Commit: typed assets and working native producer |
| 2 — shared forward PBR view | Requires stage-1 assets. RendererAPI/RenderCommand state; Texture/Framebuffer and OpenGL adapters/capabilities; new SceneRenderer/view; Scene runtime/editor submission; EditorLayer/Nutella output; PBR/sky/tone/output shaders | Real indexed mesh with PBR direct lighting in Hazelnut and Nutella; one-sample HDR, correct matrices/material channels, state restoration and integer/mask picking. Add explicit opt-in 3D scene mode and linear-color UI scope without changing old 2D output. Check actual Apple GL4.1 and portable GL4.1 fixtures; CPU BRDF oracle and resource/pixel tests pass. No placeholder renderer or Phong path. Commit: shared world/view pipeline and direct PBR |
| 3 — HDR/IBL preparation and verification | Requires stage-2 resource formats and documented BRDF recipe. Producer EnvironmentBake, `.henv` codec/resolver, LUT/cube upload, scene settings, native/Python prepare dispatch | Bake one HDR into portable cube/mips/irradiance/LUT using the specified conventions. No runtime bake. Shader consumes the matching recipe; cancellation/stale/missing/corrupt data is explicit. Constant/axis/furnace/LUT/reference-image checks pass on CPU and actual M4; demonstrate direct-only, IBL-only and combined lighting. Commit: complete first PBR+IBL model |
| 4 — complete native authoring/history | Requires stages 1–3. ContentBrowser/SceneHierarchyPanel, Authoring documents/controllers, proposed material/environment panels, scene-settings commands, EditorCamera bounds framing | Import from the editor using native job services, create/duplicate/save/assign material slots, select environment, manipulate and frame model, edit sun/exposure, undo/redo and reopen. Exact dirty state covers all values; failed asset candidates preserve accepted resources with visible diagnostics. Existing prefabs remain detached; no silent overwrite on reimport. Native authoring fixture plus human acceptance when explicitly available. Commit: usable editor vertical slice |
| 5 — runtime/export closure | Requires stage-4 authored content. RuntimeSession validation/transition/render ownership, native asset audit, packaging/inventory/asset-prepare CLI, fixture demo scripts | Same saved scene, materials, environment and existing UI in Nutella. Existing managed local transforms animate a model; repeated Play/Stop/reload and scene switching preserve isolation. Export includes prepared outputs/notices and needs no producer/source/HDR/model at runtime. Test missing dependencies, stale recipes and failed preparation without corrupting accepted content. Commit: standalone 3D project and relocation |
| 6 — qualify and measure | Requires complete slice. Extend native test inventory and CI where suitable; add maintained 3D fixtures/provenance and current guides | Debug/Release on all targets, existing 2D/tutorial/game regressions retained, actual Apple GL4.1 and available older accelerated hardware checks, relocated signed bundles and bounded reference captures. Measure rather than assume performance; record unavailable hardware/manual checks honestly. No 3D support claim from CPU/headless passes alone. Commit: qualified first 3D milestone |

Recommended prompt grouping: **(1)** stage 1 assets/import/contracts;
**(2)** stages 2–3 shared forward PBR/IBL plus independent numerical checks;
**(3)** stages 4–6 editor/runtime/export and final qualification. Keep these on a
3D implementation branch created after review; this plan branch contains no code.

For performance, use existing asynchronous GPUTimer intervals plus CPU submit,
import/bake time, peak memory, draw/triangle counts and texture/target bytes.
Measure cold/warm behavior separately, with resolution and driver recorded.
Proposed initial scene envelope: 1280×720 physical pixels (record actual Retina
framebuffer extent), <=100k visible triangles, <=64 opaque/
masked mesh draws, one sun, modest 512-pixel material maps and the IBL profile
above; target <=128 MiB resident 3D asset data. Aim for <=16 ms world GPU time on
the M4 and <=33 ms on the documented older Intel path, then revise the envelope
from evidence. These are provisional targets, not verified throughput. The current
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
- Deferred: alpha-BLEND/transmission, advanced material extensions, external GLTF
  URIs, compressed textures/meshes, HDR displays/wide gamut, MSAA/TAA/bloom,
  dynamic probes/day-night rebakes, GI, ray tracing, material graphs and automatic
  linked-prefab reimport. No replacement of existing platform adapters or libraries.

## Decisions and remaining uncertainty

No owner decision blocks beginning stage 1. Recommended engineering defaults are
the supported GLB subset, TinyGLTF 3.0.1 qualification first, Mikk tangents, simple
forward, one directional light, single-scattering GGX, CPU-prepared IBL, Neutral
tone mapping and legacy-2D preservation. None requires broad dependency upgrades.

Owner input is useful for the **art direction/reference hero asset** and whether
the provisional older-machine performance envelope is the desired product target.
Default to the licensed diagnostic assets plus a small neutral example, and the
720p/30-fps older-machine envelope until measured. No need to defer implementation
for those preferences; do not manufacture a renderer-backend decision.

Unresolved technical gates are importer compilation/validation on the actual
toolchains, measured bake quality/time and half-float range, quantified IBL
approximation error, and real older-GPU throughput. The stages above give the
smallest experiments and acceptance criteria that resolve them. No new library,
3D render or performance result is claimed by this read-only audit.
