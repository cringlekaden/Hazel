# Renderer organization and IBL quality pass

Implemented on `feature/3d-foundations` in the canonical Hazel checkout.
Stage 5 has not begun.

Pre-tuning criteria (2026-11-06, ddeb1ef/bb1fdf7 baseline)

All errors are linear HDR, same base RGB (.18,.4,.7), metallic 0/.5/1,
geometric normals, AO=1, no direct key, fixed camera/EV0/yaw0. Test Photo,
constant C and a one-pixel 10,000-radiance emitter, plus yaw/moving-view offsets.
Separate diffuse/specular/combined output and inspect every prepared level.

Constant piC/C <=1e-5. No diffuse contribution for metal=1 (GPU <=1e-6).
At actual convolved levels r>=.25: independent source quadrature must converge
within .2% peak; target RMS <=1%, 99th percentile <=3%, maximum <=5% of the
reference peak, separately per level/direction set. Energy <=1% relative.
For sharper levels measure integration convergence, peak/location and half-height
lobe width; target <=one source-cell/one output-texel angular sampling error.
Measure off-lobe spikes, not only averages: no error >5% peak or isolated second
maximum above the converged lobe. Compare rotation-adjacent samples against the
same independent integral. Cube face/edge samples use a common direction oracle.
Fractional GPU LOD must interpolate distinctive known levels within FP32 .0001;
CPU artifact -> GPU face/level readback exact FP32. Exposure/UI/picking and direct
paths retain their existing bounds. No tone-curve/exposure/material tuning.

If a proposed remedy misses these targets, investigate and report it; do not
silently relax them. Quantify roughness-level/spatial representation error and
split-sum model error independently from estimator error.

## Baseline evidence

At `ddeb1ef`, Photo diffuse had 7.77% RMS / 27.67% maximum error relative to
the sampled reference peak; roughness-1 specular had 9.08% RMS / 15.50% maximum.
The original float tiny-emitter prototype had 49.7% RMS / 183.2% maximum error;
the exact archived RGBE/default-profile reproduction below was worse.
These are bake-data errors before GPU sampling or tone mapping. Independent
FP64 source-cell quadrature uses the bilinear radiance field and exact cell solid
angles; convergence and roughness-level results are reported separately below.

The organization-only checkpoint is `bb1fdf7`, with portable includes corrected
in `32d5ecd`. Existing focused 2D/direct/IBL checks passed before and after the
split. [Follow a frame](../follow-a-frame.md) describes the current entry points.

Canonical CI 42318229171 passed Linux and hosted macOS. Windows passed Debug/Release
software and Release GL4.1 (18 native executables per profile), then hit the
unchanged 480-second Debug gameplay timeout. Its last log reached MainMenu at
01:19:51 UTC, after ObservatoryDawn at 01:19:42; no failing assertion preceded
the timeout. This is separate duration debt, not a Stage-4 renderer failure.

## Implemented repair and ownership

`SceneRenderer` now coordinates targets and frame order; `Renderer3D` owns forward
mesh/material/light bindings and the ordered background pass. Renderer2D retains
sprite/circle/text batching. Renderer has shallow Cameras, Drawing, Scene,
Resources and Text groups; backend factories/state remain at the existing
resource/OpenGL boundary. The offline environment producer is grouped separately.
The actual entry points and shutdown order are in [Follow a frame](../follow-a-frame.md).

Both diffuse and specular previously used finite direction-only sampling against
the unfiltered panorama. Small, high-radiance features were hit or missed as the
sample frame changed over a sphere. Diffuse error is independent of roughness;
it becomes more conspicuous on low-metallic rough surfaces as sharp specular
highlights stop dominating. Metals have zero diffuse, but their specular bake
also had large high-roughness estimator errors. This explains both reported
patterns without attributing all splotches to the specular prefilter.

The new producer builds a solid-angle-weighted luminance proposal over the
original bilinear source. A positive neighboring-cell proposal covers its support;
it changes probabilities, not source pixels. Global deterministic radiance samples
produce coherent diffuse irradiance. Balanced radiance/GGX MIS captures small
emitters while retaining narrow GGX lobes. Specular normalization uses the exact
kernel integral. Sample budgets grow per roughness level. Nothing clips radiance,
lowers exposure or pre-blurs the source to hide spikes.

A tested alternative used solid-angle-weighted source mips and PDF/footprint LOD,
as described in [Filament's IBL notes](https://google.github.io/filament/main/filament.html).
At 1024 samples it reduced Photo diffuse RMS error to .83%, but tiny-emitter
diffuse remained 7.87% and its specular levels 2.30–7.07%; Photo roughest specular
was 2.72%. That candidate missed the criteria. The production sampler evaluates
the original source and uses radiance-aware MIS instead. Increasing samples alone
and source footprint filtering are not asserted equivalent to this remedy.

M4 GL4.1 also quantized a requested fractional mip LOD .135 to .125 in a distinct
level fixture. ForwardPBR now interpolates two integer `textureLod` samples in
FP32. Both the resource probe and actual PBR floor/roughness mapping are checked;
the .0001 gate remains unchanged. Explicit uploads are byte-checked on every face
and level, before and after rendering. No later pass generates or overwrites the
specular chain.

Production specular faces increase from 128 to 512 and stop at an explicit 4×4
angular tail rather than 1×1 faces. This reduces spatial representation error at
high roughness. The portable v2 specular encoding declares that complete chain;
known v1 data remains readable for frozen evidence through the same renderer.
The minimum custom 4×4 base-face profile retains a full v1 chain so it still
has distinct roughness endpoints. Unknown future versions/recipes reject.

Accepted metadata v2 records `Hazel.EnvPrepare_v2.SolidAngleBalancedMIS.BilinearSource.Tail4`.
Cache identity includes it; native producer feature contract 2 rejects stale tools.
The panel identifies earlier accepted preparation revisions and offers explicit
Prepare. It never rewrites frozen assets automatically. BRDF, roughness floor,
visibility, LUT axes/channels and single-scattering semantics are unchanged;
the shared LUT retains its independent cache identity. There is one renderer and
one current baker, with no selectable historical path.

## Executed numerical evidence

Errors below are linear HDR relative to each sampled reference peak, evaluated
at stored cube texel centers. New resolution changes the center grid; controlled
GPU before/after captures additionally hold camera/materials/normals/AO fixed.
Independent source quadrature uses two/four subdivisions per source cell for the
1k Photo and four/eight for the maintained finite-emitter check. It does not call
production proposal, sampling or convolution helpers.

| Case / term | Baseline RMS / max | Production RMS / max |
| --- | --- | --- |
| Photo diffuse | 7.77% / 27.67% | .133% / .340% |
| Photo specular r=1 | 9.08% / 15.50% | .0198% / .0541% |
| Tiny-emitter diffuse | 98.0% / 407.1% | .0194% / .0464% |
| Tiny-emitter specular r=1 | 58.8% / 99.4% | .00113% / .00376% |

The source quadrature, all levels, p99/max errors and sampled bias are retained
under ignored `build/ibl-quality-evidence`. The tiny baseline table used a
standalone reconstruction of the old estimator with the exact current RGBE source
and original 128-specular/32-irradiance default profile. Baseline Photo uses the
original accepted data. Prototype/FIS logs are separately identified.
The maintained finite-emitter test at r=.25/.5/.75/1 passed the fixed bounds:
maximum outlier .492%, quadrature delta .0314%, full-sphere convolved flux error
.249%. The production 256×128 RGBE emitter's full-cube flux error was below .05%
for every convolved level, with sharp-endpoint resampling error .132%.

Actual M4 GPU lobe probes at all seven convolved production levels measured
maximum peak error 1.78%, half-width deviation .414 degrees and off-lobe error
4.09%. Source/output angular sampling bounds and the 5% spike bound passed.
These include spatial cube interpolation error; they are distinct from estimator
error at stored texels. Constant piC/C, axis/yaw/face/seam, LUT, furnace, direct,
AO, signed/mapped normals, exposure/UI, depth/picking and state-restoration checks
retain their existing gates. The controlled raw FP32 checks verify zero diffuse
on metallic=1, separate terms summing to combined output, four yaw steps and
unchanged authored scenes.

The viewed Photo pairs show removal of low-metallic diffuse grain, preserved
sharp HDR highlights and smooth broad rough-metal reflections. Captures use the
same black inspection surround and EV 0; the saved diagnostic also includes the
normal environment background/UI. The old IBL_v1 bytes/cameras/exposure remain
frozen. `IBLQuality_v2` is additive; it does not replace Core_v1/Import_v1/Direct_v1.

## Preparation choices and costs

The native environment panel exposes Preview, Production, High and the actual
custom dimensions/budgets through existing document history. Names are presets,
not a second serialized setting. No preparation runs during native New Project
or player startup.

| Choice | Background / specular / irradiance | Base MIS samples per proposal |
| --- | --- | --- |
| Preview | 128 / 128 / 32 | 1024 |
| Production (new environment default) | 256 / 512 / 64 | 4096 |
| High | 512 / 512 / 64 | 16384 |

Specular budgets double per level up to 65536; diffuse uses max(16384,4×base).
The constant source has an exact analytic fast path. Requests remain bounded;
quality names do not alter runtime BRDF or invent an optional backend.

On this M4/Apple Clang Release build, Photo Production took **119.09 seconds**,
**134.11 MiB** peak RSS and **28.91 MiB** portable resources including the shared
LUT. Tiny Production took 119.91 seconds / 126 MiB. Showcase High took **450.83
seconds**, **174.22 MiB**, **42.41 MiB**; measured CPU time was 449.76 user + 1.02
system seconds while other checkpoint work could run. This deliberately spends
offline time/memory on quality. It is not a promise for other CPUs. Higher
roughness budgets and source-aware integration, rather than an ordinary generated
mipmap chain, produce every convolved level. High reduces Photo diffuse RMS/max
from Production's .133%/.340% to .0223%/.0545% against the same source integral;
its roughest specular level shares the 65,536-sample cap and is unchanged. The
extra cost principally improves diffuse and sharper/intermediate convolutions,
not a different BRDF or exposure.

## Open the diagnostic and showcase

From `/Volumes/External/kadencringle/Projects/Hazel`:

```sh
python3 scripts/hazel.py run Hazelnut --config Release --project examples/RenderingReference/RenderingReference-IBLQuality-Photo.hproj
python3 scripts/hazel.py run Nutella --config Release --project examples/RenderingReference/RenderingReference-IBLQuality-Photo.hproj
python3 scripts/hazel.py run Hazelnut --config Release --project examples/RenderingShowcase/RenderingShowcase.hproj
python3 scripts/hazel.py run Nutella --config Release --project examples/RenderingShowcase/RenderingShowcase.hproj
```

Use Play in Hazelnut for the fixed authored camera; Edit navigation remains
private. Adjacent diagnostic Constant/TinyEmitter selectors and showcase
RoughnessSweep/SurfaceDetail selectors are named saved views. See the
[showcase guide](../../examples/RenderingShowcase/README.md) for what to inspect
and exact licensed material/HDR provenance. Its art direction never changes the
diagnostic controls. No shadows, bloom, local reflections, dynamic skies, Metal
or alternate rendering path was added.

## Checkpoint qualification

Apple M4 OpenGL **4.1 Metal - 91.7**, matching Apple Clang Debug/Release builds:
all **18 native regression executables passed in each configuration**. Release's
first full campaign passed 17 and exposed an editor test still expecting the old
16-to-1 specular chain. The corrected assertion checks the exact declared v2
16-to-4 chain, preparation recipe and clean authored history; its focused rerun
passed. Debug's subsequent full campaign passed all 18 sequentially. No graphics
check was counted from a skip, and no numeric gate or timeout was relaxed.

The complete suites retain 2D/color/text, direct lighting, exposure-independent
UI, picking, native authoring/history/save/reopen/Play/Stop, failed/cancelled
preparation and accepted-resource lifetime checks. Runtime checks cover all three
saved showcase views and the controlled diagnostic rotations. Logs, captures,
independent integrations and costs live under ignored `build/ibl-quality-evidence`.
They are local investigation artifacts, not a new screenshot or test framework.

Actual Release Hazelnut and Nutella launched a disposable source-free showcase
from the canonical directory; Nutella opened Gallery, RoughnessSweep and
SurfaceDetail. Every launch rendered 60 frames and completed normal teardown,
using private profiles/saves and no project script module. The player symbol
inventory contains no environment baker, TinyGLTF or MikkTSpace producer symbols.

The first package attempt correctly rejected a missing detached-prefab dependency
in the new showcase copy helper. The helper now copies the native import closure,
including its prefabs; existing runtime/package validation was not weakened.
Focused Debug/Release runtime checks validate that closure and the saved views.
A Release content-only v2 showcase package then passed outside the checkout in
a path containing spaces/Unicode. The existing sandbox denied the source checkout
and SDK/system-Mono fallbacks. No HDR, GLB or project DLL was present: accepted
lighting and native dependency closure sufficed. Nutella rendered 60 frames,
closed normally, and retained its code signature and native loaded closure.
Corrupting the private candidate specular artifact was rejected by its hash check;
the accepted ZIP and checksum remained byte-identical. Archive SHA-256:
`d1ef925a96d05dba8861fc53764ea2a124f16f9dbe3138f962e03874b13bd4d2`.

Canonical [run 42343499034](https://github.com/cringlekaden/Hazel/actions/runs/42343499034)
found a Linux test compile failure: Xlib's global `None` macro expanded the new
probes' `CullFace::None`. `a75969a` isolates that macro immediately after the
Xlib include; no renderer behavior or gate changed. The showcase closure repair
is `1a3681d`, qualified by focused Debug/Release RuntimeSessionSmoke reruns.
Replacement [run 42344369137](https://github.com/cringlekaden/Hazel/actions/runs/42344369137)
covers both repairs and is **pending** on Linux, Windows and hosted macOS at this
record's close. The earlier run's Windows/macOS jobs were also still running.
No complete current Windows/Linux qualification is claimed. The separately
confirmed historical Windows gameplay timeout is described above.

All 261 inventoried old content/reference/Finder files remain byte-for-byte
unchanged; the existing Reference README is intentionally extended. LastLightkeeper
and the original owner recovery branch remain intact. Vendor pins, personal data
and local Xcode preferences were not committed. There is one canonical worktree.
Stage 5's broader mixed-scene/runtime/export campaign has not begun.

## Model and representation limits

Estimator error is not the entire lighting error. A separate FP64 Photo check at
NoV=1, roughness=1 and +Z reflection measures **6.29%** difference between the
ideal split-sum reconstruction and full single-scattering BRDF integration.
Sampling the stored 4×4 tail adds **3.50%** spatial interpolation error there;
the combined difference is **9.56%** before tone mapping. This does not contradict
the much smaller estimator error at stored texel centers. The lobe probes above
bound spatial peak/shape error separately rather than folding it into a bake
average or loosening its gate.

Tiny intense emitters expose the split-sum model's grazing/reflection-hemisphere
boundaries and finite cube interpolation. These can remain visible even when the
Monte Carlo splotching is gone. Continuous roughness between the finite stored
levels is also an approximation. This pass does not establish a complete
high-frequency/grazing reference-image bound or eliminate subpixel background/
normal-map aliasing. It preserves the approved single-scattering recipe; rough
metal energy loss is not hidden with ambient gain or altered exposure. Temporal
antialiasing, multiple scattering and a different lighting representation require
their own measured changes, not a claim that this checkpoint has AAA parity.

## Review checkpoints

- `bb1fdf7`, `32d5ecd`: organization-only split and portable includes.
- `9a3af2d`: preparation recipe, quality policies, explicit mips and numerical/GPU probes.
- `228b555`, `d02cce7`: role-based ForwardPBR naming and public Renderer3D entry point.
- `a971c28`: licensed showcase, saved views and accepted-resource lifetime checks.
- `f4b512d`: exact v2 native editor assertion.
- `1a3681d`, `a75969a`: showcase package closure and Linux Xlib test boundary repairs.

The canonical workspace remains `/Volumes/External/kadencringle/Projects/Hazel/Hazel.xcworkspace`.
No master merge or release was made.
