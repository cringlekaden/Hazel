# Directional shadows: first CSM foundation

Baseline: `feature/3d-foundations`, `c36fd1a15e2774a3e0d6bdf15f7fe38163b5104f`.
The owner accepted that revision's local-light terminator correction. Preserve
its direct BRDF, authored-surface visibility and independent indirect lighting.

Status: cohesive first foundation implemented; **production milestone INCOMPLETE**.

## Audit and selected design

`Scene::DrawView` → `SceneRenderer::Collect/Render` → `Renderer3D::Render` is the
single scene/material submission path. Collection currently drops off-frustum
meshes. CSM must retain conservative casters in the same collection, independently
of camera visibility. Mesh bounds, immutable assets, mirrored winding, material
MASK and double-sided state are reusable. World 2D remains outside mesh shadows.

`SceneRenderView` owns per-view targets; Metal framebuffer generations and draw
uniform snapshots survive GPU completion. DepthPBR already writes masked depth
without color/IDs. Metal supports depth32F preparation; OpenGL's DEPTH32F enum
is not yet implemented by OpenGLFramebuffer. Both require an explicit depth
producer-to-lighting dependency. Extend the existing packet ownership pattern,
not scene traversal or a render graph. Camera ViewProjection is authoritative,
including direct test submissions which omit separate View/Projection matrices.

Four cascades in one 2×2 atlas avoid array/layer rendering and sampler-array
indexing differences on OpenGL 4.1. Each cascade uses an explicit viewport and
depth-only draw. Initial per-cascade resolutions: 256, 512, 1024; atlas dimensions
remain within Metal's existing framebuffer limits. No backend default changes.

Practical logarithmic/uniform perspective splits, linear orthographic splits,
and configurable shadow distance. Reconstruct finite frustum corners from the
unjittered authoritative VP and physical camera forward. Fit each overlapping
slice with a bounding sphere rather than a tight light-space box: this spends
resolution to keep scale independent of camera rotation. Quantize radius, snap
light-space XY to texels, reserve filter/snap/bias margins. Fit depth conservatively
to receiver and overlapping caster bounds; never camera-cull casters. A fixed
light basis may change at its pole-selection threshold under moving lights;
temporal invariance is promised only for fixed light/projection/settings.

Canonical GLSL owns depth lookup and deterministic 3×3 bilinear PCF (16 shared depth fetches). Atlas texelFetch
prevents hardware comparison/filter differences and adjacent-cascade leakage.
Shared projections use RH OpenGL [-1,1] clip depth, standard Z, Less, clear 1.
Metal's existing SPIRV-Cross clip-depth/Y fixups also apply to shadow producers;
sampling explicitly converts canonical NDC to [0,1]. No reversed Z conversion.

Bias is receiver-side, in world/texel units, derived from the actual geometric
plane rather than mapped shading normals. Bounded normal offset and slope/depth
allowance must be tested on contacts and grazing surfaces. Shadow visibility
multiplies directional incident RGB only; leave GGX/Kulla–Conty, local lights,
emission, AO and IBL unchanged. Smooth shading still follows actual caster
triangles, so geometric shadow terminators require explicit visual qualification.

Authored opt-in settings use existing ScenePresentation serialization/editor
draft/apply mechanisms. Changing settings invalidates TAA through presentation
equality. No PCSS, contact hardening, spot/point shadows or update scheduler.

## Acceptance gates declared before implementation

- CPU/FP64: split anchors/order, finite matrices, inverse-VP slice coverage,
  conservative off-camera casters, light directions including poles, scale and
  texel-grid invariance under camera translation/rotation; invalid inputs fail.
- GPU: independent analytic blocker/receiver visibility, exact unchanged camera
  depth/IDs, deterministic captures, contact/acne/detachment, near/far transitions,
  cascade blending continuity and distance fade. Test opaque/MASK, smooth/hard,
  mirrored/nonuniform transforms, off-frustum and moving casters/lights/cameras.
- Native Metal/OpenGL matched captures and bounded parity; API/GPU validation,
  two views, queued frames, resize/reconfiguration/cancel/recreation/retirement.
- Unmodified frozen GGX/IBL, accepted local-light terminator, AO/TAA regressions;
  Debug/Release warnings, editor/player and packaged artifact coverage.
- Performance: matched off/on generation versus forward sampling where counters
  permit; warmup 5, at least 20 samples, median/p95 at 720p/1080p, 512/1024 maps.
  Distinguish GPU completion intervals from serialized readback/frame pacing.
- Owner visual acceptance remains separate. Unavailable gates are NOT RUN or
  BLOCKED; never infer a production PASS from one image or backend.

Design references: [Microsoft CSM guidance](https://learn.microsoft.com/windows/win32/dxtecharts/cascaded-shadow-maps)
and [sample fitting/snapping implementation](https://github.com/microsoft/DirectX-SDK-Samples/blob/main/C%2B%2B/Direct3D11/CascadedShadowMaps11/CascadedShadowsManager.cpp).
These inform tradeoffs; Hazel's source and independent tests define its contract.

## Implemented foundation and ownership

The implementation uses a 400-byte std140 `DirectionalShadowData` block at
binding 5 during forward rendering; motion uses binding 5 in its separate pass.
Captured per-draw versions prevent aliasing. Texture 10 is the atlas; material/
IBL 0–7, AO 8 and light tiles 9 retain their meanings. OpenGL 4.1 guarantees
sufficient fragment units; actual capability admission now requires eleven.
The tracked `ForwardPBR.hzmetal` is regenerated from canonical GLSL, with reflected
CPU layout checks. No native shading implementation was added.

`SceneCollection.cpp` fills both camera-visible meshes and the conservative
caster list in the same traversal. No shadow visibility cache or directional
UUID is needed yet: the single enabled key's entire atlas is regenerated per
view. `SceneRenderer` fits the unjittered view, records the atlas before AO/forward,
and supplies those matrices unchanged to jittered shading. `Renderer3D` reuses
MASK/material/cull/mirrored-winding behavior for each depth viewport. The camera
pass still owns authoritative signed IDs and depth.

OpenGLFramebuffer now allocates actual GL_DEPTH_COMPONENT32F depth-only storage
and explicitly binds the atlas for lighting. Metal permits bounded viewports on
**depth-only** targets; the existing color-target viewport restriction remains.
The consumer packet moves/retains its `ShadowSource`, recursively prepares and
accounts its allocations, encodes it before lighting, and orders producer stages
before fragment sampling. Reconfiguration, cancellation, view destruction and
backend recreation follow existing framebuffer-generation ownership. No separate
queue submission, readback synchronization or render graph was introduced.

Four cascades use practical splits (default lambda .65, distance 100), with linear
splits for orthographic views including negative near planes. Each next cascade
includes the preceding blend band. The final band fades to unshadowed light.
The sphere radius is rounded upward in 1/16 world-unit increments; XY uses a
world-anchored light basis and texel snapping. Padding reserves filter, snapping
and normal-offset coverage. Depth includes all overlapping caster bounds, even
behind the eye or beyond camera far; this is conservative but can spend precision
on downstream/outlier geometry. Standard finite non-oblique camera planes and
unit physical forward/light directions are validated. Infinite, oblique and
reversed-Z cameras are explicitly unsupported by this slice.

Bias uses a light-facing **geometric** receiver offset, scaled by world texel
width and grazing angle, plus a small texel-scaled depth allowance. PCF taps use
receiver-plane depth correction; slopes are capped at 8 and denominators at .05
to avoid singular grazing offsets. There is no blanket front-face cull or
backend-specific polygon-offset tuning. Actual triangular geometry still casts
shadows on smooth surfaces; the accepted authored-normal BRDF is untouched.

The first nearest-texel PCF experiment exposed four cross-backend edge pixels
(max RGB .0856) and visibly stepped contacts. The final filter is the average
of nine bilinear comparison lookups, combined into a shared 4×4 footprint:
per-axis weights `(1-f, 1, 1, f)/3`. Its 16 explicit depth fetches keep weights
continuous at texel boundaries. This fixed the strict parity gate without
changing its tolerance. Atlas clamping stays inside the selected tile.

## Native checkpoint evidence

Apple M4, macOS 27.3 (26C5047e), Xcode 27.3 (27C5020n). Native Metal 4 with API/GPU
validation and Apple OpenGL 4.1 through canonical shaderc/SPIRV-Cross GLSL 410;
no software-GL result is substituted for native coverage.

[Machine-readable qualification](evidence/DirectionalShadows_v1/qualification.json),
[Debug](evidence/DirectionalShadows_v1/debug-results.json),
[Release](evidence/DirectionalShadows_v1/release-results.json) and
[math preservation](evidence/DirectionalShadows_v1/canonical-math.json) retain
scope and source/artifact hashes. Raw local logs and intermediate failures remain
under `build/csm-evidence/` rather than being represented as passing runs.

- PASS Debug/Release: 1,536 independent FP64 frustum-corner probes per process,
  analytic split anchors, rotation-independent scale, whole-texel translation,
  light poles, orthographic negative near, invalid projection rejection and YAML
  settings roundtrip.
- PASS both configurations/backends: 18 analytic blocker/receiver cases across
  256/512/1024, near/far camera positions, light pole and off-frustum casters;
  approximately 892k interior receiver samples per backend. Maximum visibility
  error .014685 against the predeclared .02 bound. Filter boundaries remain in
  captures and are excluded conservatively from the hard-visibility oracle.
- PASS strict native parity: zero ID mismatches, max depth delta 9.84e-7 against
  1e-6, max RGB delta .000318 against .003. Static captures are deterministic;
  split/final-fade crossings have maximum RGB-vector change 7.3e-6.
- PASS exercised Metal lifetimes: atlas+AO in three gated frames, separate-process
  TAA three-successor queue test, busy cancellation and view destruction before
  completion. Lit combined shadows/AO/TAA also exercise moving camera/light/caster,
  target resize and resolution replacement. Exercised shutdown resources retire
  and native validation assertions remain clean. These are not long-session or
  motion-quality acceptance results.
- PASS Debug/Release: tools, both GPU fixture executables, Hazelnut and Nutella
  build. No new warnings were identified in changed first-party code; existing
  component-copy, unused-parameter and UI warnings remain. A standalone include
  check covers the new portable shadow header on local Clang only.
- Focused AO/TAA and accepted local-light/Forward+ regressions retain their frozen
  thresholds. Exact configuration/iteration attribution is in qualification.json.
  No accepted fixture or reference data was reset.

![Native Metal offscreen CSM study](evidence/DirectionalShadows_v1/study-metal.png)

This is a production-path offscreen readback, not an editor/presentation capture
or owner approval. It includes hard/smooth contacts, mirrored geometry, thin and
masked casters and far blocks. Continuous PCF improves the observed contact
stepping; grazing bias, geometric smooth-shadow terminators, leaks, detachment
and moving-scene quality still require deliberate owner review.

## GPU cost

[Raw samples](evidence/DirectionalShadows_v1/performance.csv) and
[median/p95 summaries](evidence/DirectionalShadows_v1/performance.json): Release,
Apple M4, 32 cubes plus ground, four cascades, warmup 5 and 20 measured samples
per case. API/GPU validation disabled for timing. Each sample waits for offscreen
completion/readback; this is not presented frame pacing or an FPS claim.

| Physical extent | Cascade resolution | Atlas interval ms median / p95 | Forward off → on median ms | Difference of medians ms |
|---|---:|---:|---:|---:|
| 1280×720 | 512 | .0164 / .0304 | .660 → 1.025 | .365 |
| 1280×720 | 1024 | .0839 / .1563 | .651 → 1.079 | .427 |
| 1920×1080 | 512 | .0211 / .0433 | 1.654 → 2.267 | .613 |
| 1920×1080 | 1024 | .0445 / .0696 | 1.395 → 2.135 | .740 |

Atlas timing brackets the depth command interval; forward brackets forward/world
execution and includes the shadow-sampling path. Differences include cache and
shader execution effects, not an isolated per-fetch cost. Short intervals and
run-order/driver variance are visible; do not infer inverse resolution scaling
or a general performance guarantee from this workload. OpenGL GPU pass timing is
NOT MEASURED here. CPU fitting/collection is included only in serialized elapsed
time, not separated as an optimization claim.

Depth storage is 4 MiB at 512 and 16 MiB at 1024 per atlas generation; up to four
bounded completion/idle generations per view can retain 16/64 MiB respectively.
Existing attachment accounting includes them. This excludes view HDR/AO/TAA,
uniform/upload buffers and native allocation overhead.

An initial benchmark read the reset view metrics object and yielded zeros. It
was discarded. Only `performance-qualified/` is reported: it reads completed
frame-owned counters and checks their validity, positive generation when enabled,
and positive forward intervals.

## Reproduction and remaining gates

```sh
python3 scripts/hazel.py build --config Debug --tests --target MetalShaderTools --target MetalRenderer2DQuadSmoke --target MigrationRenderer2DSmoke
python3 scripts/macos/metal_shaders.py --config Debug --output Hazel/Resources/shaders/metal
python3 tools/assets/generate_directional_shadows.py
python3 scripts/macos/directional_shadows.py --config Debug --resources Hazel/Resources --output NEW_DEBUG_EVIDENCE
# Repeat build/qualification with Release.
python3 scripts/macos/directional_shadows.py --config Release --resources Hazel/Resources --backend metal --benchmark --output NEW_PERFORMANCE_EVIDENCE
```

The [study instructions](../../examples/RenderingReference/README.md#directional-shadow-study)
cover editor/player launch and manual movement. Authored settings are opt-in;
existing scenes retain shadows off. Public editor/example/README documentation
is updated; static README link checking passes. CI triggers/badges remain unchanged;
no branch CI, Windows/Linux/Intel/AMD/NVIDIA execution or new packaging/relocation
campaign is claimed. Detached-view stress, long-session presentation, full
interactive editor/player and owner visual acceptance remain NOT RUN.

The cohesive first foundation is implemented and independently testable.
**The production milestone is INCOMPLETE.** It is available for owner inspection,
but automated numerical/lifetime checks and this one observed image do not
establish production visual acceptance. No PCSS, point/spot shadows or next-stage
work is authorized by these results.

## Owner-found editor TAA failure — follow-up to dd85be4

The owner reported enabling XeGTAO and then TAA in the shadow reference editor
crashed with `Bind a qualified framebuffer before Renderer2D::BeginScene`.
The initial authored-camera offscreen fixture missed the actual editor lens,
saved orbit pose, Retina footprint and host error/overlay path.

Three concrete problems were reproduced:

1. Large Retina views could exceed the original M1 proof's 1 GiB aggregate
   storage ceiling. A native 3200×1800 presentation fixture also exceeded it
   with AO alone. Private HDR world intermediates unnecessarily retained
   completion-versioned attachment pools despite having no external readers.
2. After memory admission, jitter phase 6 at the saved off-origin editor pose
   failed XeGTAO's orthogonality check. Subtracting nearby near-plane points in
   FP32 lost accuracy with a 30-degree lens and .1/1000 clip range. This is a
   false rejection of a valid camera, not evidence for relaxing the threshold.
3. SceneRenderer cancelled failed Metal packets; Scene::RenderScene attempted
   fallback UI and Hazelnut then attempted overlays without a bound host packet.
   The secondary BeginScene exception masked the primary rendering error.

The fix reconstructs the AO camera basis and projection constants in FP64 on
CPU, then narrows to the existing FP32 GPU ABI. The orthogonality tolerance and
shader algorithm are unchanged. Independent camera-pose axes now cover all
64 jitter phases at the failing lens/pose/extent.

Private world intermediates are configured before first use and reuse one
attachment generation on the existing single queue. Queue-stage barriers protect
previous consumers before new writes, including the temporal compute path's
existing compute/blit barrier. Direct presentation/readback of private scratch
is rejected. Public view targets and atlas completion ownership stay versioned.
At 3200×1800, avoiding two redundant HDR/ID/depth world generations saves up to
approximately 264 MiB compared with three slots. This follows the existing private
TAA producer contract, rather than permitting arbitrary render-target aliasing.

Memory admission now shares an explicit **2 GiB** resource ceiling between
framebuffer allocation and aggregate device accounting. Idle/in-flight storage,
uploads and editor UI remain counted; the separate 64 MiB allocator bound remains.
Reuse alone does not make the full Retina AO/TAA footprint fit the old proof's
ceiling. The bound is retained, not disabled; larger/multiple views can still
require more memory than admitted and are not universally qualified.

On an editor rendering failure, Scene::RenderScene restores and clears its host
packet before fallback UI and overlays. The original RenderError remains visible;
failed frames clear stale picking IDs. A regression deliberately rejects an
invalid shadow distance and verifies the original error, a subsequent overlay
BeginScene and completed fallback readback.

Focused qualification and native-window results for this follow-up are recorded
in the [follow-up qualification](evidence/DirectionalShadows_v1/editor-taa-fix.json). The new window fixture
uses the actual scene editor entry point, saved orbit pose, 3200×1800 scene target,
ImGui presentation, live shadow → AO → TAA toggles and repeated off/on changes.
It waits for positive presentation evidence rather than treating CPU updates as
presented frames. The original performance table predates this ownership fix;
no new GPU timing claim is made here. Production visual acceptance remains separate.

The follow-up passes native Debug/Release at 3200×1800 with API/GPU validation:
23/44 positive completed presentations, zero validation errors and zero remaining
exercised resources/attachment generations after shutdown. Debug executed 3,666
CPU ticks while waiting for positive presentation; Release executed 60. These
counts are not interchangeable or an FPS benchmark. Tracked active resources
peaked at 1.52/1.65 GiB respectively; all attachment storage peaked at 1.19/1.49
GiB (overlapping accounting scopes, not additive). Focused CSM parity, AO/TAA,
editor-error recovery and queued-output lifetime checks pass in Debug/Release.
Hazelnut and Nutella are rebuilt in both configurations. Windows/Linux and other
GPUs remain unverified; this scoped fix does not close production visual review.

Reproduce the presentation regression from the repository root on an unlocked,
visible native desktop (replace Debug paths/configuration for Release):

```sh
python3 scripts/hazel.py build --config Debug --tests --target MetalM1Smoke
DYLD_LIBRARY_PATH="$PWD/build/dependencies/mono/macos-arm64/sdk/lib" MTL_DEBUG_LAYER=1 MTL_SHADER_VALIDATION=1 bin/Debug-macos-arm64/MetalM1Smoke/MetalM1Smoke build/NEW_SHADOW_EDITOR_EVIDENCE --shadow-editor-settings Hazel/Resources
```
