# Directional PCSS — bounded implementation and qualification

Owner authorization: current directional CSM appearance, tuned blending and Ultra
resolution are accepted for continued development. The rejected CSM cohesion
experiments are closed, preserved separately and are not production inputs.

Starting HEAD d80bb5e9a3fcfd687b1a995980806c91748e1b8d on feature/3d-foundations.
Preexisting dirty files are inventoried in build/pcss-v1/before/status.txt,
including a newline-only shadow-project edit. They are preserved. **Decision:
INCOMPLETE; retain production PCF.** The optimized prototypes are archived, and
production C++/GLSL, editor controls and shader artifacts were restored. No PCSS
commit/push or owner visual acceptance is claimed.

The owner deferred PCSS after reviewing these performance results. The
measurements apply to the tested implementations and hardware. The next
production work uses perspective [spotlight shadow maps](spotlight-shadows.md).

## Algorithm and ownership

Reuse SceneRenderer, four D32F atlas tiles, conservative casters, stable sphere
fitting/snapping, canonical GLSL/offline Metal translation, and existing completed
GPU resource ownership. PCF remains the default and its math/fitting is unchanged.
Directional light owns angular half-radius in radians (default .00465, about
.266 degrees); resolution/quality never changes it. The principal experimental PCSS tier used:
8 raw blocker locations (centre plus seven Vogel points) and 32 Vogel hardware
comparison locations. Empty searches inspect four native bilinear texels before
returning lit. No frame-random patterns. The original16/16 proposal and all
failed screens remain archived; counts were not a fixed architectural requirement.

Canonical orthographic depth is z=(hi-dot(L,P))/span, standard Z. Decode blocker
separation along light travel as (receiverZ-blockerZ)*span, not a perspective
depth ratio. For a uniform angular disk of half-radius alpha, estimate physical
penumbra radius r=max(separation,0)*tan(alpha), capped by a configured world radius.
The radius tends to zero at contact; bilinear raster antialiasing retains its
finite native footprint. Reconstruct each map's separation independently, never
average incompatible normalized depths across maps. Blend final visibility using
the established weights. Both maps use one physical search bound and geometric
receiver offset scale for a given receiver. Their blocker estimates remain separate
and must be measured before considering any radius-combination rule.

Search uses a shared conservative caster light-height bound and the same world
radius cap as filtering. Clear depth1 is never a blocker. Blocker classification uses the
actual geometric receiver plane at each fetched texel centre, with explicit depth
bias; physical separation excludes comparison/normal bias. Filtering uses the
native-plane envelope described below, retaining exact per-texel correction near
contact.
Padding includes the full configured world support plus native bilinear/snap/
normal-offset margins; collect all casters intersecting that expanded projection.
Depth fitting reserves the full world disk plus native bilinear support at the
clamped grazing slopes (8 per axis), including the zero-angle limit. PCF fitting
retains its accepted bounds unchanged. PCSS affects incident directional RGB
visibility only; BRDF, energy accounting, IBL, local lights, AO and TAA stay outside it.

This is the directional specialization of the search/estimate/filter structure in
the [NVIDIA PCSS guide](https://developer.download.nvidia.com/assets/gamedev/docs/PCSS_Integration.pdf),
not its perspective-light penumbra equation. A mean blocker depth is an approximation
for multilayer blockers, and a single raster depth cannot recover missing thin-edge
coverage. Those limitations need measured bounds, not another shadow representation.

## Pre-execution gates

Freeze this section before candidate captures. Preserve all original fixtures and
thresholds, including .02 existing analytic visibility, .003 stable-case RGB
parity, exact stable IDs, 1e-6 depth and .01 split/fade checks. The known strict
full-frame CSM stress failure remains FAIL and is compared separately.

- FP64 decoding/radius: separation error <=8 FP32 depth ULPs times light-depth
  span +1e-5 world; radius error <=tan(alpha) times that bound +1e-5 world. Clear,
  fully lit/blocked, zero-angle/contact and distinct cascade spans are explicit cases.
- Independently implement the fixed quadrature/depth-grid reference; GPU visibility
  must agree within .02 away from independently identified raster ambiguity.
  Do not turn a broad uncertainty interval into a PASS.
- Resolved parallel-plane edges (r>=4 native texels): 50% contour/contact error
  <=one native texel + the explicitly derived receiver displacement. 10–90% width
  within20% of a continuous uniform-disk reference, integrated mean visibility error
  <=.035 and maximum <=.10. These finite-quadrature targets are additional to the
  exact algorithm reference, not replacements for existing frozen tests.
- Independently resolved neighboring maps: blocker separation/radius must agree
  within1% plus the FP32 decoding bound on constant-height planes. No widening of
  cascade blending. Report no-blocker disagreements and subtexel cases separately.
- Static repeat exact. TAA-off moving-camera world-contour p95 drift and maximum
  adjacent step may not exceed matching PCF by more than .25 output pixel; compare
  both translation/rotation. No new holes/lost thin or MASK shadows/components
  relative to matching PCF in held-out geometry; moving lights/casters and AO/TAA
  on/off must stay finite with correctly invalidated setting changes.
- Native Metal/GL4.1 Debug/Release: explicit shader compilation, numerical/radiance
  parity, settings roundtrip/legacy defaults, atlas boundaries/full support,
  completed resource lifetime/replacement/cancellation and no new warnings.
- On the native16-GB M4 at720p/1080p, 1024/2048: 5 warmups +20 measured samples.
  Optional PCSS median forward overhead <=1.5ms and <=75% versus interleaved PCF;
  p95 increase <=2ms. Demanding1080p overlap forward <=6ms. Generation increase
  <=.5ms. Existing2-GiB tracked resource budget includes in-flight retirement;
  no new texture types/storage or larger atlas. Presented Retina measured where
  available; unavailable presentation is NOT RUN, never inferred from offscreen.

A failed discriminating correctness/coverage/cost screen can stop expansion and
leave an isolated prototype/recommendation instead of forcing production adoption.
Full milestone status requires all relevant gates, owner visual acceptance remains
independent, and PCSS does not authorize spotlight/point shadows.

## Initial sampling screen

The first paired Vogel16 pattern decoded physical separation/radius correctly but
failed resolved width (about22–24% too wide). The same16 locations changed to
concentric equal-area4x4 strata; width agrees within4%, but native1024/2048 shows
half-visibility plateaus and the2048 large-radius case exceeds the frozen continuous
CDF limits (.1135 maximum, .0360 mean). The 50% crossing is non-unique over the
plateau; reporting only its midpoint would hide finite-sample banding. Both runs
and the initial shader are preserved in build/pcss-v1. No thresholds changed.
Before increasing filter count, measure actual16+64-fetch ordinary/overlap cost.
An independent360-orientation CPU CDF analysis also records four standard16-point
patterns; orientation-dependent sample gaps are real quadrature limits, not light
span decoding. Do not rotate a pattern merely to pass one authored edge orientation.

## Bounded optimization continuation

The owner requested a source-based optimization investigation before any final
rejection. The original16/16 count is a starting point, not an architectural
requirement. Keep earlier captures/timings and the frozen contract; this continuation
produced the candidate below. It was not adopted after the final cost/quality
screens. Ordinary-path performance improved substantially; the wider exact-contact
fallback remains unresolved.

Pinned research inputs: Filament863e7d72420e99f4efbed8cafa6a343f3407018d and
PKRenderer3e3113b7ecb83c07c91eab7a322aeed6aab0d256. Source snapshots and commit
metadata live under build/pcss-v1/research, not a second Hazel checkout.

Filament's current [soft-shadow shader](https://github.com/google/filament/blob/863e7d72420e99f4efbed8cafa6a343f3407018d/shaders/src/surface_shadowing.fs)
uses bounded, mip-filtered EVSM moment search/filtering; its depth-PCF path uses
hardware comparison filtering and a reduced-location Gaussian reconstruction.
Its moment/mip mechanism cannot be transferred to this depth-only milestone.

PKRenderer's [directional PCSS compute shader](https://github.com/konstatoivanen/PKRenderer/blob/3e3113b7ecb83c07c91eab7a322aeed6aab0d256/PKRenderer/Content/Shaders/CS_ScreenspaceShadow.shader)
runs at quarter pixel count, uses rotated/dithered sampling, wave-wide all-lit/
all-shadowed shortcuts and an upsample pass. Those choices materially change cost
and reconstruction. Do not copy its all-shadow shortcut into Hazel's thin/MASK
path without proving coverage; sampled blocker agreement alone is not a proof.

The generated Hazel MSL retains nested2x2 filter loops. Three exact-math candidates
vectorize the four comparisons, replace four reads with a vector gather, or unroll
the16 locations. Native overlap measurements remain about10.4–10.6ms for vector/
unrolled and16ms for vector gather; these measured implementations do not recover
the budget. Generated MSL and per-candidate raw timings are retained.

## Optimized depth-only candidate

The same D32F atlas is bound raw at10 and with a linear LessEqual comparison sampler
at11. OpenGL4.1 uses a sampler object; Metal retains its sampler with the consuming
frame packet until GPU completion. No extra depth storage, compute pass or mip
hierarchy is introduced. PCF stays the default. The existing atlas owns four1024
maps (16MiB) or four2048 maps (64MiB), independently of angular size.

One hardware bilinear lookup has a single reference depth. It cannot exactly
correct that reference separately at four sloped receiver-plane texels. An
unconditional native slope envelope erased the thin .005-world suspended-contact
shadow and was rejected. The candidate bounds that envelope by the native texel
footprint, and selects exact per-texel comparisons when the nearest sampled
blocker separation is no more than four such envelopes. It never scales slope
bias by the penumbra radius. This remains an explicit approximation on distant
sloped receivers, qualified separately from blocker-distance decoding.

The exact contact path shares four depth comparisons when all finite filter
locations occupy one bilinear cell. Their average weights follow the first and
cross moments of the fixed pattern. For a disk of diameter at most one native
texel, at most3x3 depths are needed; tent weights are integrated before reading
those nine depths. These caches preserve the finite quadrature rather than
blurring or sharpening it. Wider contact footprints retain up to128 raw filter
reads per map; the mixed-depth cost screen must exercise that fallback.

The ordinary soft-edge path has eight raw search lookups and32 hardware comparison
lookups per map, doubled in cascade overlaps. Those are sampling instructions,
not a claim of40 underlying depth-memory reads. Bilinear comparison can consult
four native depths internally. No-blocker refinement adds four raw reads; zero
angle uses one exact bilinear location. Blocker estimates remain independent per
cascade and only final visibility is blended. Reusing one map's estimate was not
selected because coverage and depth-range differences would need a separate
correctness argument.

Hoisting map coordinates, inverse spans and native-plane terms outside the filter
loop materially improved measured M4 cost. Literal unrolling, gathers and four
centre comparison lookups did not. The shaded PCSS program also evaluates shadow
visibility before material/BRDF intermediates and omits inspection branches.
A zero visibility skips directional BRDF work while local/indirect lighting keeps
its existing energy calculation. Actual compiler register occupancy has not been
measured; reduced live state is an inference from source and timing, not a hardware
counter result.

All variants originate in ForwardPBR.glsl. Concrete ForwardPBR_PCSS and
ForwardPBR_PCSSInspect aliases select compile-time flags in shaderc/GL cache inputs
and offline Metal artifacts; there is no duplicate shading model. The PCF variant
retains the accepted direct-light evaluation order and16-read filter. The experimental producer
emitted 14 raster packages from 11 canonical sources. The two extra programs add
code/pipeline storage, not another shadow atlas.

The empty-search screen found a real temporal error: point queries could all miss
while the native bilinear footprint still covered a caster edge. Four weighted
native-cell blocker queries recover that finite coverage before returning lit.
This reduced the tested near-contact contour step from about.77 output pixel to
less than.014 pixel excess over PCF in the translation/rotation screen. It does
not prove that eight locations find every narrow blocker in a wide search disk.
Thin unresolved silhouettes and multilayer blocker means remain measured limits.

## Decision and measured results

**INCOMPLETE. Keep the accepted PCF implementation in production.** The serious
bounded attempt recovered practical cost for ordinary PCSS, but did not demonstrate
one candidate satisfying contact quality, penumbra coverage and the predeclared
cost limits together. This is a result for these Hazel implementations on M4, not
a lower bound on the cost of the PCSS algorithm.

The best broadly referenced candidate was the global-search 8/32 implementation.
At fixed exposure on the authored shadow scene, native **positive presentation**
measurements at 1920x1080 / Ultra2048 gave median whole-frame GPU time 4.53ms PCSS
versus 3.45ms PCF; with TAA, 10.33 versus 9.32ms. AO was disabled. At 1280x720 the
corresponding pairs were 2.18/1.71ms and 4.81/4.32ms. These include presentation
output and optional TAA, not only the forward pass. Each physical extent excludes
30 positive-presentation warmups and retains 656–690 joined samples. Missing/zero
presentation timestamps were excluded. The attached VG272U V3 monitor reported
scale1: this is actual native1x, **not Retina2x**. Offscreen 2560x1440 overlap PCSS
forward time was about6.55ms; that is not proof of native Retina presentation.

The final paired 32-cube typical scene measured 1080p PCSS forward medians
2.79/2.96ms at1024/2048; the surrounding PCF runs ranged2.01–2.08/1.42–2.35ms.
At720p PCSS was1.56/1.68ms versus PCF .95–.97/.90–1.10ms. The wider variance in
2048 PCF is retained, not hidden by a single favorable denominator. The earlier
interleaved batch is also archived. The cost decision does not depend on this
variation: the layered contact stress fails by a large margin.

| Native Metal1080p overlap screen | 1024 median | 2048 median | Outcome |
|---|---:|---:|---|
| Initial scalar16-search /16-bilinear-filter | about10ms | about10ms | Initial implementation cost, not an intrinsic PCSS limit |
| Optimized8/32, uniform near-contact sheet | 5.02ms | 3.66ms | Scoped cost gate met |
| Optimized8/32, near-contact sheet plus upper slatted canopy | 13.72ms | 13.55ms | FAIL6ms demanding-overlap budget |
| Tighter receiver-plane bound, layered Sun angle | 8.98ms | 9.34ms | Improved, still FAIL |
| Tighter bound, layered2-degree half-radius | 13.46ms | 13.46ms | FAIL |
| Fused exact fallback, scalar reads, wide angle | 13.05ms | 13.01ms | Small gain; no adoption |
| Fused exact fallback, gather reads, wide angle | 15.89ms | 15.88ms | Regression; rejected |

All quoted stress rows used5 warmups and20 measured samples, with validation off;
raw per-frame CSVs and p95 values are retained. Atlas generation in the original
layered Sun screen was .070/.335ms, separating the loss from forward sampling.
The exact branch can require128 raw filter reads per map, plus8 search reads
(and up to4 native refinement reads). Two active cascades double those requests.
Normal8/32 filtering instead uses32 hardware comparison instructions; instruction
counts must not be confused with underlying memory transactions.

OpenGL4.1 also completed a positive elapsed-query matrix after query results were
harvested following the existing readback. In the simple1080p overlap case, total
SceneRenderer GPU medians were1.51ms PCF /2.77ms PCSS at1024 and2.72/4.05ms at2048.
These GL times include atlas and forward work; the harness did not split them.
A native GL visible-window PCSS probe returned positive GPU durations, but its
later PCF control was BLOCKED when the GUI console became unavailable. GL window
execution has no presentation timestamp proof. No CPU time is labeled GPU time.

## Correctness evidence and limits

The principal global-search candidate passed32 independent plane cases per native
backend in Debug/Release,36 thin/layered depth-grid cases, and the scoped camera
motion tests. Across Release Metal/OpenGL: maximum FP64 visibility error .000620
against .02; decoded separation error2.71e-5 world units; radius error9.32e-7;
50% contour error below.466 native receiver texel; resolved10–90% width error below
6.92%; direct-radiance/visibility error1.48e-8. Static repeat was exact. Maximum
additional contour drift/adjacent step was below.014 output pixel versus PCF,
with TAA disabled, against the frozen+.25-pixel gate. Full bounds and individual
cases remain in the records; these numbers are not general ray-traced soft-shadow
accuracy claims.

Debug/Release native PCF foundation,735-case GGX/IBL component qualification,
accepted smooth-normal local-light terminator regression, shader corpus/cache/ABI
checks, lit three-slot atlas/TAA lifetimes, resize/replacement/cancellation and
retirement passed at the recorded checkpoint. Release AO and TAA regression
scripts passed. Authored hard/smooth/thin/mirrored/MASK geometry and moving camera,
light and casters were captured with AO/TAA off/on; completion/finite captures
alone do not constitute owner visual acceptance.

The strict CSM full-frame stress gate remains **FAIL**, unchanged. A separate
15-capture TAA/AO-off authored comparison also fails strict full-frame RGB/ID/depth
parity: its static frame has2 RGB failures,2 ID mismatches and1 depth failure in
both PCF and PCSS. Across the15 captures there were **zero additional PCSS RGB
failures on pixels where PCF IDs, depth and RGB met the frozen thresholds**. This
is scoped evidence, not a resolution of the stress gate or proof that all raster
uncertainty is harmless.

The original unconditional hardware slope envelope erased the .005-world thin
contact shadow. Exact native comparisons fixed that error, with cached bilinear
weights reducing a12.46ms screen to5.70ms before the8-search preset. Empty point
searches initially caused a.77-pixel contact-contour step; native footprint
refinement removed it. These are concrete correctness fixes retained in the
prototype, rather than extra blur, darker shadows or wider cascade transitions.

The depth-support audit found missing native bilinear plane support at zero
physical radius: actual near-corner virtual plane intersections reached normalized
depth-.00832. PCSS-only depth padding gained16 world texels (8 slope per axis),
then passed an independent FP64 grazing-plane check at256/1024/2048 and zero/nonzero
angle. No visible leak was claimed from that mathematical audit.

The tighter-search experiment packed a common world caster AABB into32 additional
uniform bytes (448 versus416, both within the same512-byte GPU allocation alignment).
Its receiver-plane distance bound reduced tilted-ground oversampling, but exposed
finite8-location search gaps: isolated resolved-edge visibility error reached
.14365 against the frozen .10 limit, while its independent algorithm-reference
error stayed below.00015. Both native backends reproduced the failure. Four plane
cases failed;48 finite/grazing/layered depth-grid cases agreed with the algorithm
reference. Agreement with an algorithm is not proof of physical penumbra coverage.
This optimization is rejected along with the fused-fallback cost regressions.

Atlas storage stayed16MiB/64MiB at1024/2048, with no extra texture type, compute
pass or hierarchy. Recorded serialized1080p tracked peaks were about256MiB and
retired to zero; three-slot atlas/TAA ownership was independently exercised. Native
presentation runs completed without budget/admission errors. These checks do not
measure total driver/unified-memory pressure or establish long-session production
qualification of the16GB system. The2GiB resource budget was never increased.

## Remaining decision

The unresolved combination is wider mixed-depth filtering with correct per-texel
geometric receiver semantics and bounded cost. Hardware comparison's one-reference
bilinear operation cannot simply replace four different receiver references; the
measured cheap unconditional bias lost thin contacts. Gather instructions did not
recover that cost on this Metal implementation. GLSL4.1 gather reads the base level
without implicit derivatives, as specified in the [Khronos GLSL4.10 specification](https://registry.khronos.org/OpenGL/specs/gl/GLSLangSpec.4.10.pdf); the rejection is measured cost, not an assumed API gap. Sampled all-shadow early-outs were
not adopted as proofs of coverage for thin/MASK geometry. A smaller reconstruction
or an explicitly approximate quality tier would need fresh independent contact,
penumbra, mask and temporal qualification; a moment/hierarchy representation is
a larger, separately scoped decision. No next shadow stage follows this incomplete result.

Full source-free PCSS export/relocation, native2x Retina, Windows/Linux and Intel
GPU PCSS execution remain NOT RUN. The common-bound448 experiment has Release
coverage only; it is not claimed Debug-qualified. No owner PCSS acceptance or full
production PASS is claimed. Production source, generated artifacts and editor
options are restored; only the evidence/tools/design record remain for review.

See the [optimization evidence index](evidence/PCSS_v1/optimization-v2/README.md)
for matching ABI/source patches, raw local paths, captures and reproduction rules.
