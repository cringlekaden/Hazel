# Authored local lights and Metal Forward+

**Terminator correction: PASS scoped native automated qualification. Milestone: INCOMPLETE pending owner visual approval.**
The owner rejected the original `b492560` gallery; the final correction below is awaiting visual acceptance.
Windows/Linux execution and their CI are NOT RUN at this macOS checkpoint.
Starting revision: `3a4647c8f0ad7f2b6cba9dd6534fb491d530796f`,
`feature/3d-foundations`. Qualification ran on the scoped working tree; exact
shader/BRDF hashes and gates are in [the qualification summary](local-lighting-qualification.json).
No shadow resources, depth passes or sampling filters were added.

## Source audit and ownership

- `SceneCollection.cpp` resolves transforms and collects backend-neutral
  `SubmittedLocalLight` values. Transform supplies position and normalized local
  -Z spot travel; authored range stays in world units, independent of scale.
  Submission sorts by entity UUID, with point before spot when both inhabit one entity.
- `Lighting.h` owns validated shared parameters and point/spot definitions.
  `Components.h::AllComponents`, `SceneSerializer.cpp`, `DocumentSchema.cpp` and
  `AuthoredState.cpp` cover persistence, copying, duplication and prefab/history state.
  `UIEditCommand` also registers both types for exact property preview/Undo/Redo,
  cancellation, component retirement and validation. Legacy directional ownership remains.
- `Renderer3D.cpp` packs the shared std140 block. `ForwardPBR.glsl::direct` remains
  the single accepted Multiscattering_v2 BRDF, byte-identical to starting HEAD.
  Local contributions share its authored direct-light horizon, complementary
  diffuse and HDR accumulation. Direct light stays independent of material AO/XeGTAO.
  Emissive, IBL and TAA semantics remain intact.
- Metal records **one immutable CPU light snapshot per scene submission**, shared
  by its draws. Retaining the mutable `UniformBuffer` allocation was rejected by
  independent-view color tests. `MetalDeviceRenderer2D.cpp` prepares/encodes light
  work through existing frame packets, residency and three-slot buffer retirement.
  Editor, player and offscreen paths reset/reuse the counter pool; drain releases it.
  No second scene graph, alternate material model or general render graph exists.

## Point and spot terminator investigation

The owner reported triangular light-to-dark transitions on the authored gallery at
`b49256001bd6d13422fa4c88d0a52a96ac3b5c42`. The native offscreen captures use
the authored primary camera at 1280×720, exposure 0 and static authored light
transforms. Metal and OpenGL reproduce the same lighting. See the
[matched baseline and experimental crop](evidence/LocalLights_v1/terminator/matched-crop.png)
with baseline on the left.

The geometric normal from world-position derivatives is constant on each planar
face, while the shading normal is continuous across smooth edges. The local loop
sets radiance to zero when geometric N·L is nonpositive, even when the accepted
GGX evaluation at shading N·L would return positive radiance. On the first
gallery sphere, the authored warm point key straddles that gate at 26 visible
shared-edge midpoints where the shading normal is light-facing and the view
reflection horizon leaves it unchanged on both faces. One midpoint has shading
N·L 0.05219 and geometric N·L −0.00352 and +0.10771 on the two faces. Point
attenuation is continuous across that edge, and the point source has no cone.

An experimental local-light [Chiang visibility factor](https://www.yiningkarlli.com/projects/shadowterminator/shadow_terminator_v1_1.pdf)
multiplied incident radiance by `g * (1 + g * (1 - g))`, with
`g = min(1, geometricNL / (shadingNL * dot(Ng, N)))` for positive denominators.
The geometric backface test and the existing GGX energy equations were retained.
This smooths the approach to the light horizon *within* a face. At the measured
edge above, the lit face still gets full weight because its geometric cosine
exceeds the shading cosine, and its neighbor still gets zero. The candidate
therefore fails the shared-edge continuity requirement. It was tested in an
isolated resource directory and is excluded from production.

The opt-in maintained diagnostic renders a plane with analytic +Z geometry and
normals tilted 30° toward a grazing source. It requires exact center IDs/depth,
zero radiance behind the plane, and grazing radiance below 3% of the reference
lit sample. The baseline fails on both native backends. The experiment passes
for point and spot sources and for a dielectric and a rough metal, but the
plane test cannot certify continuity across two faces.

| 1080p, Metal tiled 16 | Baseline forward median | Experimental forward median |
| --- | ---: | ---: |
| 32 sparse lights | 1.714 ms | 1.708 ms |
| 128 sparse lights | 3.869 ms | 3.999 ms |
| 320 dense lights | 138.818 ms | 144.014 ms |

Each timing uses 20 samples and five warmups in one matched offscreen run per
version. The experimental gallery preserves exact Metal IDs/depth versus
baseline. Experimental Metal/OpenGL parity has zero ID mismatches, max depth
delta 1.79e-7 and max RGB delta 9.00e-6. The full-loop/tiled and FP64 local-light
checks pass for the experiment on both backends. At that checkpoint, no correction was qualified.
The wider moving-light, tessellation, normal-map, double-sided, AO/TAA,
projection, editor/player and source-free export matrix was not rerun for the
rejected candidate. Frozen references were preserved.

Reproduce the baseline diagnostic with `python3 scripts/macos/local_lighting.py
--config Debug --backend metal --terminator --output NEW_EVIDENCE_DIRECTORY`
and repeat with `--backend opengl`. Local logs, full gallery captures, candidate
GLSL and its generated Metal package, the edge scan, and timing CSVs are under
`build/qualification/terminator-*/`. See the
[qualification evidence](evidence/LocalLights_v1/terminator/results.json) for
sample values and gate status.

## Continuous authored-surface horizon experiment

The subsequent candidate distinguishes the flat derivative normal, the
unperturbed authored vertex normal, and the mapped material normal.
Existing vertex normals supply the surface contract. Equal endpoint normals
on a shared edge define a continuous smooth surface, while split normals
retain intentionally hard edges. No mesh format or normal-generation policy
changes are required. The fragment stage normalizes the interpolated authored
normal and flips it with the material normal for explicitly two-sided backs.

Directional, point and spot lights use that continuous surface horizon.
Mapped normals resolve their reflection horizon against this surface, and a
Chiang visibility term accounts for material-normal deviation from it.
With an unmapped surface the visibility factor is identity. The accepted
Kulla–Conty equations and LUT are unchanged. The BRDF source block SHA256 is
`9443aa3a21defd0c3d5242b442dcc72aa48802ffef26e95c383586e4123e9ac6`,
identical to the baseline. Flat surfaces with aligned vertex normals retain
their original horizon and energy behavior.

This is a smooth-surface lighting approximation on the existing raster mesh.
It intentionally permits illumination below an individual triangle's light
horizon when the authored smooth surface faces the light. Raster culling,
winding and signed transforms still determine coverage. The model rejects
light below the authored surface horizon and rejects direct evaluation when
that surface faces away from the camera. Mesh silhouettes, positions, depth,
picking and occlusion geometry remain polygonal. Authored normals with large
deviations describe a correspondingly larger approximation and require
author review. The gallery sphere's maximum face/vertex-normal deviation is
5.2958°, bounding this extension to a negative flat-face cosine of 0.09230.
Edge midpoint radius is at least 0.497866 for the intended radius 0.5.

The independent analytic reference checks all original 26 shared edges.
Its radial-sphere cosine error and shared-edge limiting error are zero.
It also verifies matching limits after nonuniform and mirrored normal
transforms. A 2,001-step moving-direction sweep has a maximum cosine step
of 0.003081. This reference proves the cosine model and its continuity,
and does not replace GPU qualification of the full BRDF.

The maintained native sphere oracle independently derives radial normals from
world positions, evaluates the FP64 multiscattering reference, and checks
moving point/spot and directional direct radiance for dielectrics and rough
metals. It includes 3,276 positive-radiance samples below flat face horizons
per backend. Baseline Metal/OpenGL fail this oracle. The candidate passes
on both backends, including exact IDs/depth. An experimental signed/nonuniform mapped-normal expectation evaluated the
mapped normal against the authored surface. That expectation is retained in
the rejected candidate patch. The maintained baseline expectation and all
historical references are preserved.

- [Matched gallery crop](evidence/LocalLights_v1/terminator/continuous/matched-crop.png)
  shows baseline on the left and the continuous candidate on the right.
- [Moving-light contact sheet](evidence/LocalLights_v1/terminator/continuous/moving-light.png)
  retains twelve direct-component fixture frames in row-major order.
- [Analytic edge evidence](evidence/LocalLights_v1/terminator/continuous/reference.json),
  [native local-light results](evidence/LocalLights_v1/terminator/continuous/native-local.json),
  and [GGX results](evidence/LocalLights_v1/terminator/continuous/ggx.json)
  retain the measured scoped results.

Native Debug GGX numerical/components/parity/retirement, focused XeGTAO and
focused TAA suites pass. Debug/Release qualification targets compiled during the experiment.
The final native local-light fixture passes FP64 accumulation, culling/order,
signed IDs/depth and gallery parity with max RGB delta 9.08e-6.
The full requested controlled tessellation, perspective/camera-motion,
double-sided, export/relocation and editor/player matrix is still unqualified.
The candidate is **rejected as a production change**. The maintained
Renderer2DSmoke fails in IBLSurfaceRenderingChecks with "Shading normal differs
from independent geometric-horizon constraint". Its extreme-normal fixtures
require view/reflection repair against actual raster geometry. Replacing that
normal globally also changes IBL, and a smooth-sphere direct-light oracle does
not prove the preserved IBL contract. The production GLSL, Metal artifact and
mapped-normal regression expectation were restored to baseline after retaining
the candidate patch and native resources. The new opt-in sphere diagnostic
remains a failing baseline regression. Overall status is **INCOMPLETE**.

The measured 1080p forward medians for baseline/candidate are 1.714/1.748 ms
at 32 sparse lights, 3.869/4.067 ms at 128 sparse lights, and
138.818/147.142 ms at 320 dense lights. These use the same benchmark cases,
five warmups and twenty samples. The maintained benchmark renders an unmapped cube with aligned face normals;
these workloads do not add smooth-horizon illumination. They measure added
shader work and implementation effects without isolating the visibility helper. See [timing summary](evidence/LocalLights_v1/terminator/continuous/performance-summary.json).
Public command and packaging documentation remains deferred while the
candidate is unqualified. The rejected patch is retained in
[the experiment record](evidence/LocalLights_v1/terminator/continuous/rejected-global-horizon.patch).
At the end of that rejected experiment, production shading remained at baseline. The subsequent separated correction is recorded below.

Reproduce with `python3 scripts/macos/terminator_reference.py` and
`python3 scripts/macos/local_lighting.py --config Debug --smooth-terminator
--output NEW_EVIDENCE_DIRECTORY`. The old `--terminator` test retains its
historical strict flat-face interpretation for the rejected experiment.
It is incompatible with intentionally treating its tilted authored normals
as a smooth surface. Raw logs and matched motion frames remain in
`build/qualification/terminator-continuous*/` and
`build/qualification/terminator-smooth*/`.

## Separated direct-light and indirect reflection horizons

The final correction keeps three normals with distinct responsibilities. The
actual raster normal remains the derivative cross product; winding, culling,
positions, depth and picking are unchanged. The normalized, unperturbed authored
vertex normal defines the continuous direct-light surface. The mapped material
normal evaluates the same canonical GGX/Kulla–Conty BRDF after view-horizon repair
against that authored surface. Split vertex normals preserve hard edges; a
flat-shaded mesh retains its per-face horizon. Two-sided backs reverse both
authored and mapped normals together. Signed inverse-transpose normals and
signed tangent handedness retain the established transform contract.

For direct lights, reject an authored surface facing away from the view or
light. Evaluate the single existing `direct()` function on the direct normal,
with its matching view-dependent energy table. For normal maps only, multiply
by the existing experiment's bounded Chiang visibility polynomial:
`G = g + g² - g³`, where `g = min(1, Ns·L / ((N·L)(Ns·N)))` when
`Ns·L > 0` and the projected cosine exceeds it; otherwise use one, or zero
below the authored horizon. This branch formulation never divides by a
nonpositive denominator. `G` is in [0,1], approaches zero at the authored horizon,
and cannot amplify a given BRDF evaluation. Unmapped normals have `G=1`
exactly and bypass both material-normal repair and per-light visibility work.
All directional, point and spot sources use this policy, after the unchanged
range and cone attenuation. No second BRDF, texture/LUT generation, mesh-normal
generation, culling policy or submission path was introduced.

Indirect lighting separately calls the original
`resolveShadingNormal(N, geometricNormal, V)` and computes the original IBL
energy from that normal only when IBL is enabled. The uniform enable guard
keeps the LUT sample in uniform control flow, before the existing geometric
view gate; direct-only draws avoid an unused second energy-table evaluation.
The repair function, texture lookups, geometric view gate, reflection direction,
AO and multiscattering equations are unchanged. The
`Normal` / `NormalHorizon` diagnostics continue to expose this indirect repair.

### Reproduced integration failure and ablation

The rejected candidate changed the *input to the shared normal repair* globally,
so its result also drove IBL reflection and the view-dependent energy lookup.
This is sufficient to reproduce the historical extreme-normal regression:
changing only that repair's horizon from actual geometry to the authored normal
fails `IBLSurfaceRenderingChecks` with its original expectations. The direct
light-facing gates, visibility factor and direct GGX equations need not change
for this failure to occur. Baseline and the separated correction pass that same
unchanged suite. `scripts/macos/terminator_ablation.py` stages independent resource
directories and records shader hashes, commands and expected failure outcomes.

For the independent analytic case `N=normalize(-.1,0,1)`, `Ng=(0,0,1)` and
`V=normalize(1,0,.02)`, the global authored horizon leaves `N` unchanged because
its own view cosine is negative. This produces `N·V=-0.07958706` and
`reflect(-V,N)·Ng=-0.17838017`. The geometric repair produces
`N=(-0.00999850,0,0.99995001)`, `N·V=0.00999850` and reflection cosine zero.
The direct/indirect split preserves that result without using a per-triangle
normal to cut smooth direct radiance.

### Physical scope and limitations

Shared-edge continuity and exact zero illumination behind each planar triangle
cannot both hold for the 26 measured edges: the same continuous point and
normal straddle opposite face classifications. The chosen approximation lights
the authored smooth surface while rasterizing its polygonal approximation.
It admits light below an individual face only where the authored surface is
light-facing. It never overrides raster backface coverage. The original gallery
sphere has maximum face/vertex deviation 5.29579°, giving a geometric extension
bound of 0.0922975 in cosine. Large authored deviations permit larger extensions;
there is no hidden angle clamp or assumption that arbitrary normals describe a
sphere. Mesh authors retain responsibility for their smooth/flat normal intent.
Silhouettes and future shadow occlusion still follow actual geometry.

The normal-map visibility term is an empirical masking approximation. It can
reduce mapped-normal grazing energy, and is not a new reciprocal microfacet
model or a claim of exact light transport. The canonical reciprocal
multiscattering BRDF and RuntimeBRDF_v2 remain unchanged. Unit visibility on
unmapped surfaces preserves their accepted energy behavior relative to the
authored smooth surface; geometrically admitted energy necessarily differs from
the rejected per-face gate. No screen-space filtering conceals this difference.

### Qualification and review artifacts

The final working-tree shader SHA256 is
`46e975ee068a74ba4f8968006f544d64ddbbec0929b05bafe8a0c68e53ebf48d`.
Debug/Release native Metal and OpenGL pass the smooth-sphere and mapped-material
oracles, GGX components/furnaces, AO/TAA, 135-capture production parity per
configuration, four detached consumers on one display at native 1×/2×, pending
shutdown, 600 independently lit temporal-view frames per configuration, and
actual Hazelnut/Nutella gallery startup/teardown on both backends. Signed gallery
exports pass source/SDK/compiler-isolated light diagnostics, missing-library
rejection, and relocated Metal/OpenGL editor/player execution from a Unicode/
space path. These are local native checks, not hosted Windows/Linux CI or a
visual acceptance claim.

All 26 original edges pass 156 independent FP64 BRDF-limit cases (three
roughnesses, dielectric/metal), with zero limiting/reference radiance difference.
The native sphere oracle covers the original mesh plus 16/64-segment spheres,
orthographic/perspective cameras and moving directional/point/spot sources;
it checks radiance against the independent radial-sphere reference at
`N·V >= .05`, with tolerance `.0002 + .002*abs(expected)`. Original-mesh
orthographic cases include 3,276 illuminated samples below flat-face horizons
per backend. The additional material fixture checks 216 direct-radiance cases
and 72 indirect-normal readbacks per backend/configuration: flat/smooth authored
normals, map scales 0/1/8, signed nonuniform transforms, culled/two-sided backs,
and three camera positions. Maximum material radiance error is 6.18e-8;
maximum repaired-normal error is 1.99e-7. Native local accumulation error is
at most 1.03e-5. Gallery backend parity has zero ID mismatches, max depth delta
1.79e-7, and max display RGB delta 9.08e-6. Before/after Metal gallery IDs/depth
are exact; the IBL energy scheduling optimization changes no gallery pixels.

GGX component errors remain at most 1.24e-5 against the independent table model
and 1.00e-4 against the independent continuous model. Perfect-reflector GPU
furnaces return approximately 0.999999 (rough), 0.999997 (roughness floor), and
0.99963–0.99965 (grazing `N·V=.01`), within the unchanged 0.003 bound. RuntimeBRDF_v2
and historical reference bytes remain unchanged.

The default HEAD full-suite command still reproduces the **pre-existing**
`Frozen IBLQuality_v2 bytes changed` failure. Sixteen Debug executables passed
before it. The established immutable fixture revision
`86c581c13fd5c944c2a06d720314a753d7607d8f` passes the remaining Debug checks and
all 18 Release executables. After the final energy scheduling change, all four
affected graphics suites were rerun in both configurations; unchanged CPU/2D
gates were retained. No frozen hash or asset was reset. No new first-party or
shader warnings were identified; existing unused-parameter/missing-initializer
and Xcode script-phase warnings remain. Windows/Linux execution and current-fix
hosted CI are **NOT RUN**. The owner's two XeGTAO example edits are preserved and
excluded from the correction.

The [paired performance record](evidence/LocalLights_v1/terminator/separated/performance.json)
uses Release native Metal, 1920×1080, tiled 16, identical unmapped cube and light
workloads, AO/TAA and GPU validation off, five warmups and 20 samples per case,
three alternating baseline/current runs (60 measured samples per version/case).
These are GPU forward-pass intervals in serialized offscreen captures, not
presented frame pacing. No OpenGL pass-timing or fixed frame-budget claim is made.

| Workload | Matched `b492560` median | Correction median | Change |
| --- | ---: | ---: | ---: |
| 32 sparse | 1.892 ms | 2.020 ms | +6.7% |
| 128 sparse | 3.882 ms | 4.198 ms | +8.1% |
| 320 dense | 139.185 ms | 147.160 ms | +5.7% |

The accepted historical baseline medians were 1.714/3.869/138.818 ms; the current
paired baseline above controls for run conditions. This is a measured cost,
not a speedup. The benchmark's aligned flat normals add no new illumination.
Investigation removed unmapped per-light visibility work and moved unused IBL
energy behind its uniform enable condition. Merely moving IBL work after the
light loop did not establish a useful dense-case gain. The retained guard reduced
a single matched probe from 2.106/4.369/153.350 to 2.009/4.218/149.497 ms. Remaining
normal/visibility-path cost is recorded rather than hidden; raw samples and p95
are retained. Owner visual approval remains the final milestone gate. No shadow
implementation, merge or release was performed.

The [separated correction record](evidence/LocalLights_v1/terminator/separated/qualification.json)
contains final gate status, shader identities, numerical errors, timings and
remaining gaps. It supersedes neither the historical rejected-candidate results
nor the owner's visual acceptance gate.

- [Matched gallery crop](evidence/LocalLights_v1/terminator/separated/matched-crop.png):
  identical native Metal camera, exposure and static lights; baseline left,
  separated correction right; nearest-neighbor enlargement.
- [Full current gallery](evidence/LocalLights_v1/terminator/separated/gallery.png).
- [Moving-light contact sheet](evidence/LocalLights_v1/terminator/separated/moving-light.png)
  and [animation](evidence/LocalLights_v1/terminator/separated/moving-light.gif):
  direct component, point/spot followed by directional frames, fixed exposure.
  Separate [point](evidence/LocalLights_v1/terminator/separated/moving-point.gif),
  [spot](evidence/LocalLights_v1/terminator/separated/moving-spot.gif) and
  [directional](evidence/LocalLights_v1/terminator/separated/moving-directional.gif)
  loops avoid switching source types within a clip.

Reproduce the analytic 26-edge cosine and complete FP64 radiance limits with
`python3 scripts/macos/terminator_reference.py`. Run the native low/high
tessellation, perspective, material mapping, signed transform, backface and
camera-motion cases with `python3 scripts/macos/local_lighting.py --config Debug
--smooth-terminator --output NEW_DIRECTORY`, then repeat Release. The historical
`--terminator` option remains a diagnostic of the rejected strict per-face
policy, not a passing acceptance requirement for an intentionally tilted smooth
surface. Matched focused timings use `--benchmark --benchmark-focus --backend
metal`, with `--resources` selecting isolated baseline resources.

## Mathematics and limits

RGB is linear Rec.709. Intensity is renderer-relative incident radiance at unit
distance in the inverse-square far field, before range/cone factors; **not candela
or lumens**. With `s=distance²`, `R=range`, attenuation is
`max(1-(s/R²)²,0)² * (s/(s+0.0001))² / (s+0.0001)`.
The smooth centimetre-scale source core tends continuously to zero at coincidence,
where direction is undefined. This regularized point approximation is not an area
emitter. Cone attenuation is cubic smoothstep between outer/inner cosines;
files store radians, UI half-angle controls display degrees. Zero cone influence
skips BRDF work without changing the lighting model.

Both backends admit **320 enabled local lights**, plus the existing single
directional key. The 15,376-byte block fits GL 4.1's 16 KiB guarantee. Excess
collection/submission fails explicitly; nothing is truncated. Disabled components
validate but consume no submitted slot. OpenGL uses the same bounded GLSL loop.

Metal uses sphere-versus-tile-frustum rejection, including near/far planes and an
outward FP32 margin. The spot sphere is conservative. Actual viewport/jittered VP
and physical extent drive culling. Avoiding depth reduction removes AO/TAA/prepass
dependencies and depth-discontinuity holes. One thread builds each ordered list:
17 uint4 texels/tile hold count and 64 indices; **count >64 evaluates every light**.
No atomics or allocation order can reorder a tile's list. Changing sizes retains
old in-flight packets; new lists use the new extent.

Measured 16/32-pixel tiles favor 16 for the common 32-light sparse case (1.660 vs
2.171 ms forward, 0.050 vs 0.036 ms cull at 1080p); 32 lowers culling cost for
large workloads but has coarser candidate coverage. Default stays 16. At eight
sparse lights the full loop measured 1.458 ms versus forced tiled 2.087 ms plus
0.042 ms culling, supporting the <=8 bypass. These are workload-specific choices.

Payload is 15.02 KiB per light block. At 1080p, 16-pixel lists require 2,219,520
bytes (2.12 MiB) per slot/view; three slots reuse storage. The existing allocator
may retain capacity up to four times a smaller request, so switching to 32-pixel
tiles need not immediately reduce retained bytes. Stats report retained buffer
capacity; whole-device accounting includes attachments and pending resources.

## Qualification evidence

| Gate | Result |
| --- | --- |
| Full MacOSQualification Debug/Release builds; direct-include ABI check | PASS; no new first-party warnings identified |
| Maintained native regressions | PASS 18/18 collectively Debug; 18/18 Release |
| Independent FP64 local accumulation, attenuation/cones, order | PASS both builds, native Metal/OpenGL; max sweep error ~1.1e-5 |
| Zero/one/few/320, single spot, near-source/HDR and range boundary | PASS; no invalid outputs |
| Tile edges, offscreen/near plane, projection/camera cuts, moving light/mesh, resize | PASS matched full-loop comparisons; 16/32 tiles |
| Dense overflow | PASS all 20 odd-viewport tiles overflow with 320 lights; FP64/full-loop contributions retained |
| Metal/OpenGL gallery | PASS exact IDs, max depth 1.79e-7, max RGB 9.08e-6 |
| Two independently lit TAA views | PASS 600 frames/build, real drawable colors, 320x180→640x360, stable warmed resources, positive presentations and retirement |
| Frozen GGX/XeGTAO/TAA | PASS Debug/Release, references unchanged |
| Hazelnut/Nutella gallery | PASS Debug/Release × Metal/OpenGL, managed light motion and teardown |
| Export/relocation | PASS signed inventory; source/SDK/MSL/compiler denied; isolated Metal/GL products; missing culling library rejects |
| Windows/Linux and owner visual approval | NOT RUN / PENDING respectively |

The original HEAD full-suite attempt hit the **pre-existing** frozen M0R3 material
hash mismatch documented in [TAA qualification](taa-qualification.md). Its bytes
match HEAD; they were not changed here. RuntimeSessionSmoke/EditorSmoke Debug and
full Release use the established immutable example revision
`86c581c13fd5c944c2a06d720314a753d7607d8f`. The failed HEAD log is retained locally.
No reference hashes/images were reset. Current-working-tree gallery outputs are
qualified separately. Existing deprecated-copy/unused/C++20-capture and Xcode
script-phase warnings remain; no broad suppressions were introduced.

[Numeric CSVs and versioned summaries](evidence/LocalLights_v1/) retain the measured
results. Full raw logs remain local in `build/qualification/local-lighting/` and
`/tmp/hazel-local-*.log`. Earlier fragment-stage timing and the initial mutable
snapshot experiment are superseded, not acceptance evidence.

## Performance

Apple M4 native Metal, AO/TAA off. Matched offscreen workloads alternate point/spot
lights, preserve one contributing central light in sparse cases, use five warmup
and twenty measured frames. GPU scopes bound culling and forward/world execution;
CPU light preparation overlaps recording and includes deferred native resources.
Scopes are not additive. Serialized frame timings also include completion/readback.
The separate native pacing study uses point lights, 120 positive presentations per
physical extent, thirty warmup frames, and excludes zero/missing presentation times.
VSync off is a request, not a compositor guarantee. Full median/p95 CPU, GPU and
memory data for both 720p/1080p are in the linked JSON/CSV records.

1080p medians in ms; presentation columns are median/p95. Tiled entries below nine
lights are forced diagnostic comparisons; production bypasses compute there.

| Lights | Sparse forward full / tiled16 | Cull16 | Dense tiled16 forward | Native sparse presentation | Native dense presentation |
| ---: | ---: | ---: | ---: | ---: | ---: |
| 0 | 0.802 / 0.915 | 0.036 | 0.842 | 2.299 / 5.360 | 2.314 / 6.871 |
| 1 | 1.854 / 1.615 | 0.040 | 1.606 | 2.420 / 9.412 | 3.146 / 8.081 |
| 8 | 1.458 / 2.087 | 0.042 | 3.762 | 3.161 / 8.807 | 4.190 / 5.899 |
| 32 | 1.885 / 1.660 | 0.050 | 13.749 | 3.147 / 9.170 | 11.626 / 20.585 |
| 128 | 4.987 / 3.879 | 0.176 | 53.321 | 4.602 / 11.135 | 41.899 / 55.918 |
| 256 | 8.609 / 6.274 | 0.342 | 109.155 | 6.418 / 12.200 | 83.560 / 94.702 |
| 320 | 10.373 / 7.457 | 0.425 | 136.090 | 7.599 / 16.501 | 105.843 / 122.767 |

Dense 320-light overlap is expensive: a correct unshadowed full contribution loop
cannot eliminate contributing lights. Overflow deliberately preserves quality at
higher cost. This is a bounded foundation, not a claim of arbitrary light counts
or 60 FPS under extreme overlap. A shader-only zero-light comparison against
starting HEAD measured 0.824 vs 0.813 ms at 1080p (0.332 vs 0.329 ms at 720p),
within measurement noise; it is not a whole-HEAD CPU comparison.

## Reproduction and next boundary

Build using the tracked Premake workflow; build scripts for the gallery's motion:

```sh
python3 scripts/hazel.py build --config Debug --tests
python3 scripts/hazel.py build --config Release --tests
python3 scripts/hazel.py script-build --config Debug examples/RenderingReference/RenderingReference-LocalLights.hproj
python3 scripts/macos/local_lighting.py --config Debug --output NEW_EVIDENCE_DIRECTORY
python3 scripts/macos/local_lighting.py --config Release --benchmark --backend metal --output NEW_PERFORMANCE_DIRECTORY
python3 scripts/macos/local_lighting_presented.py --output NEW_PRESENTATION_DIRECTORY
python3 scripts/hazel.py package --app all --project examples/RenderingReference/RenderingReference-LocalLights.hproj --output NEW_ARCHIVE_DIRECTORY
python3 scripts/macos/local_lighting_relocation.py --archives NEW_ARCHIVE_DIRECTORY --output NEW_RELOCATION_DIRECTORY
```

[Gallery authoring/launch instructions](../../examples/RenderingReference/README.md#locallights_v1-authored-point-and-spot-gallery)
cover creation, fine intensity/range drags, half angles, selectable influence
gizmos and transient radiance/occupancy/overflow inspection. Owner visual approval
remains separate from all automated results.

Future shadows should key resources by scene-instance identity + entity UUID +
light type; append the directional entity UUID to its existing optional record
when needed. Directional CSM, perspective spot maps and omnidirectional point
representations supply scalar visibility [0,1] multiplying that light's direct
incident RGB before the unchanged BRDF. PCF/PCSS share this visibility contract.
Explicit per-light bias, resolution, softness, quality and update policy belong
to the next milestone. No shadow allocation, depth rendering, filters or speculative
scheduler were implemented here.
