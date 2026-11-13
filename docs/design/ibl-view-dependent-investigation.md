# View-dependent IBL investigation

The subsequent [bounded A/B compression experiment](ibl-ab-compression-experiment.md)
executed ranks 8/16/32, converged the required reference rows and rejected the
tested harmonic representation. No production/GPU promotion or visual-gate
closure follows from that result.

For the next bounded offline A/B compression experiment, start with the
[repo-only handoff](ibl-ab-compression-handoff.md). It records the verified Git
state, separate agent work, reproduction inputs and accuracy-first stop rules.

Starting renderer: `1bf4f50`, including `bb0d43d`'s silhouette repair.
Diagnostic tooling checkpoint: `93cd059`.
**The visual gate remains open. No production IBL model change ships.** The
implemented full-BRDF experiment corrects the measured lighting error at high
sample counts, but fails the M4 runtime gate. Stage 5 has not begun.

## Target and gates

These tolerances were recorded before candidate evaluation and were not relaxed.
Compare isolated RGB specular radiance before exposure/presentation against the
independent FP64 authored GGX/correlated-Smith/Schlick integral. Use the captured
**effective** shading normal, unchanged material/AO and matching horizon policy.
The native AA-off silhouette regression is a separate mandatory gate.

For actual SurfaceDetail samples, positive-grazing sweeps and held-out lighting,
require channelwise floor-relative RMS <= 2%, maximum <= 5%, with denominator
`max(reference, .02)`. For channels below .02 also require absolute RMS <= .0005,
maximum <= .002. Report roughness/material groups separately. Systematic dark
patches absent from the reference, temporal instability, sharp/intermediate
regressions or loss of legitimate grazing Fresnel cannot qualify.

Require reference convergence substantially below these limits: target .5%
floor-relative maximum for sharp samples, .1% for broad lobes. Unconverged rows
cannot establish acceptance. Source-cell subdivision and independent microfacet
quadrature are separate convergence tests, not additional production samples.

The runtime gate is a native 1280×720 no-AA M4 frame, GPU median <= 16.67 ms,
p95 <= 20 ms after warmup, excluding compilation/readback. The experiment times
integration over captured coverage, not a complete scene frame. Its input textures
also have overhead; these timings are a proxy, not a mathematical lower bound.
Passing the proxy would still require shared-scene timing and host qualification.
No reduced resolution, temporal accumulation, HDR clamp, exposure or material
change may obtain the result.

### Match the horizon convention

Production gates view visibility by Ng·V and resolves the shading normal against
geometry. Its split-sum IBL integrates the **shading** hemisphere, without a
per-incoming-ray Ng·L truncation. The primary model-isolation reference and
candidate use that same convention. Separately labelled `horizon-` outputs add
Ng·L>0 and compare only with the oracle's geometric-horizon integral.

The initial horizon-control comparison is retained in the raw evidence; it must
not be substituted for the convention-matched model error. At the dark patch,
full/horizon red references are .23962647/.23946379. Across 24 copper samples,
changing just that convention changes reference RGB RMS by .00051875, versus
.06775653 production model error. For wood the convention difference is larger
at some pixels (.00310637 RMS); report it separately. The controlled grazing
sweep uses Ng=Ns, so the two conventions agree exactly. No normal policy or error
threshold changed during this audit.

## Fresh baseline and experiment

Conditions: Apple M4, OpenGL 4.1 Metal–91.7; SurfaceDetail 1280×720, AA off,
EV 0, yaw .349999994, gain 1, direct light disabled. Original Studio Small 09
HDR SHA-256 `e7cfda5f4e98e623db12b8bfd0184e048488e4855d9c83e2751fb44a32e80c45`,
accepted High generation `d030bea247f14dd3`, original materials/normal maps/AO.
No authored showcase view, environment or frozen case changed.

The fresh capture contains 13 controller-orbit views; five have FP32 isolated
terms, effective normals, geometric normals, positions, parameters and IDs.
It also includes constant, no-normal-map, untextured dielectric/conductor and
384-row domain controls. The measured actual-pixel corpus has 24 stratified
interior samples per sharp/r20/r45/r75/copper/wood group plus six named samples.
Historical `d0fbac0` numbers are context, not this comparison's baseline.

The experiment evaluates the full view-dependent BRDF with balanced deterministic
radiance/visible-normal sampling. The visible-normal construction follows
[Heitz, JCGT 7(4), corrected 2019](https://jcgt.org/published/0007/04/01/).
A positive neighboring-cell radiance proposal covers bilinear-source support.
The estimator retains the authored correlated Smith visibility and Schlick
Fresnel; it does not substitute the sampling distribution's masking model for
the BRDF. Source radiance is never clipped. A second candidate calibrates the
sampled A/B moments to the existing LUT. Neither candidate changes metallic,
exposure, base color, normals or diffuse lighting.

The production split-sum assumes a V=N convolution lobe and factors the remaining
view-dependent BRDF into environment-independent A/B moments. Accurate angular
prefilter reconstruction and a correct constant-field LUT do not recover the
lost correlation between HDR direction, visibility and Fresnel. Full integration
substantially removes both errors. LUT-moment calibration alone does not close
the accuracy/cost gap, and retains admitted table endpoint error. No metallic-
dependent correction or normal-directed endpoint substitution was adopted.

| Convention-matched linear RGB sample | Production | 8192+1024 plain probe | FP64 reference |
| --- | --- | --- | --- |
| Copper (902,567) | .14421231, .09092057, .07460393 | .23985419, .15269898, .12785342 | .23962647, .15248500, .12763159 |
| Wood (585,267) | .17912939, .17972250, .18600996 | .08286997, .08336969, .08739156 | .08286392, .08334339, .08733684 |
| Wood grazing (612,200) | .45360428, .44757694, .45489490 | .46169657, .45762867, .46910083 | .46265098, .45856352, .47005684 |

The wood rim contains legitimate neutral grazing reflectance. Reducing the
positive-grazing approximation error does not mean eliminating that response.
The silhouette repair is retained unchanged; those repaired pixels are not
counted as evidence of a model improvement.

## Accuracy and cost

[The numerical ledger](evidence/ibl-view-dependent.csv) records RGB RMS/max,
floor-relative RMS/max and near-black absolute errors for both candidates.
For the 24 copper samples, production RMS/max is .06775653/.21868649;
8192+1024 plain integration is .00095453/.00279741. Wood RMS falls from
.01087292 to .00060385. These are fresh post-silhouette measurements.

| Studio actual-pixel group | 256+64 relative RMS/max | 2048+256 relative RMS/max | 8192+1024 relative RMS/max |
| --- | --- | --- | --- |
| Sharp | 1.692% / 5.630% | .681% / 2.536% | .251% / 1.061% |
| r=.2 | 9.113% / 34.431% | 4.835% / 18.464% | .629% / 1.839% |
| r=.45 | 9.035% / 32.094% | 1.920% / 5.984% | .470% / .913% |
| r=.75 | 6.228% / 14.346% | 1.382% / 2.835% | .406% / .799% |
| Copper r=1 | 4.149% / 7.964% | 1.247% / 2.204% | .334% / .636% |
| Wood | 5.738% / 20.751% | 1.312% / 4.518% | .556% / 2.474% |

At N·V=.02, dielectric r=.2/.45/.75 across four directions has production
linear RGB RMS/max .12275634/.31430711; the high-budget probe has
.00039886/.00097343. At N·V=.1 it changes .07998992/.20422955 to
.00030704/.00089108. Valid grazing energy remains present.

Actual M4 GL_TIME_ELAPSED, 246,534 covered mesh pixels in the 1280×720 dispatch,
four warmup draws and 24 timed draws per primary candidate:

| Radiance + BRDF samples | Plain median / p95 ms | LUT-calibrated median / p95 ms |
| --- | --- | --- |
| 256 + 64 | 10.53 / 11.07 | 10.70 / 11.59 |
| 2048 + 256 | 56.14 / 63.82 | 55.02 / 61.52 |
| 8192 + 1024 | 249.13 / 263.07 | 241.90 / 261.27 |

The fast candidate fails accuracy, including near-black/intermediate-roughness
samples. The high-budget candidate improves the tested image but fails runtime
by a large margin. The calibrated candidate is not a solution. This rejects the
measured implementations, not every possible view-dependent representation.

The 256-sample CPU proposal construction measured .00613 s. The 8192-sample
experiment's source/proposal textures occupy 16.25 MiB, including a .25 MiB
sample bank; captured per-pixel input textures are additional diagnostic cost.
Maximum observed experiment child RSS was about 378 MiB, including capture maps
and GL realization. No offline bake ran, and no cooked artifact, recipe, LUT,
cache identity, native dependency or package input changed. These are experiment
costs, not a qualified source-free runtime representation.

## Independent convergence and held-out evidence

The original source-cell FP64 oracle remains independent of production CDF/MIS,
GPU filtering and the candidate. The new NDF-CDF Gauss-Legendre/azimuth tensor
quadrature resolves sharp lobes independently of the candidate's visible-normal
sampler. Constant-field tests check Jacobian/visibility/color/AO against furnace
moments, cross-check the two quadratures and distinguish geometric truncation.

For Studio sharp samples, NDF orders 1024/2048 differ by at most .3183%
floor-relative over actual pixels plus the sweep (.0783% for actual pixels).
Broad actual Studio rows converge well below .1% at source subdivisions 2/4.
The old nonconverged sharp source-cell estimates are not used for acceptance.

Held-out AsymmetricAxes uses the same captured frames and materials: sharp NDF
convergence is .0466% maximum; at 2048+256 the actual-group error is at most
1.0444% floor-relative. TinyEmitter uses its original generated diagnostic HDR:
broad actual rows converge within .0366% at source subdivisions 8/16 and the
8192+1024 broad-group maximum error is .2659%. Its sharp NDF 1024/2048 convergence
still reaches **1.79%**: those sharp rows are explicitly **not qualified**, even
though candidate/reference agreement looks close. No tolerance was loosened.

At five poses over the native 6.9° orbit, eight copper directions per pose have
production RGB RMS .04024–.08986, versus .000508–.001259 in the high-budget probe.
Wood RMS changes .00365–.02227 to .000080–.000984. Reference source 2/4 maximum
channel changes are below .0000012 (copper) and .0000114 (wood).

Matching local crops are in ignored `build/ibl-view-evidence/crops`: left is the
current renderer; right is the **rejected** high-budget experiment. They use GPU
lighting, the same captured geometry/pose and unchanged EV0 Neutral/sRGB
presentation reconstructed in FP64. They are numerical composites, **not**
Nutella screenshots of an accepted fix. Five inspected key poses show the copper
stripe becoming a clean broad reflection; no per-pixel blur or lighting tweak
is applied. They do not establish continuous native candidate motion stability.
The original 13-view native capture remains available separately.

## Reproduce

Launch the unchanged current product from the canonical checkout:

```sh
python3 scripts/hazel.py run Nutella --config Release --project examples/RenderingShowcase/RenderingShowcase-SurfaceDetail.hproj --inspect-camera
python3 scripts/hazel.py run Hazelnut --config Release --project examples/RenderingShowcase/RenderingShowcase-SurfaceDetail.hproj
```

Generate a private capture using the existing `--ibl-surface` procedure in
[the surface record](ibl-surface-investigation.md#reproduce-and-inspect).
Use its captured effective normals/positions/parameters to write bounded oracle
rows; format remains N.xyz, V.xyz, Ng.xyz, base.rgb, roughness, metallic, AO.
`--ibl-reference` accepts source subdivisions 1–16 and writes the existing fifteen
columns. `--ibl-reference-ndf` accepts orders 32–2048 and writes **six columns**:
full specular RGB and geometric-horizon specular RGB. Both reject existing
outputs and retain the five-billion-evaluation bound.

After building Release tests, with normal `HAZEL_RESOURCES`, isolated `HAZEL_DATA`
and the maintained Mono library path, the explicit GPU experiment is:

```sh
bin/Release-macos-arm64/MigrationRenderer2DSmoke/MigrationRenderer2DSmoke --ibl-integration-probe examples/RenderingShowcase/Assets/Environments/Studio.hdr /absolute/private/capture /absolute/new/results 8192 1024 "$PWD/tests/migration/shaders/IBLIntegrationProbe.glsl"
```

Set `HAZEL_IBL_PROBE_STEP` to -6/-3/0/3/6 to select a captured numeric pose.
Outputs include plain/calibrated FP32 radiance, separately labelled incoming-
horizon controls, positive/negative-view sweeps and GPU timings. Horizon-control
timings are single draws, not statistical performance results. The tool reports
**MEASURED**, not a regression/acceptance pass, and does not run the other checks.
It lives only in the regression executable; its shader is outside shipping
resources. Original HDR access in this experiment is not a new runtime contract.
Probe shader SHA-256:
`4ad7c6f20ec7849d095b954188f0f8a9f4724265dcbf068d565660b84f3fdba5`.

## Qualification and next experiment

Actual M4 native **Debug and Release Renderer2DSmoke and RuntimeSessionSmoke
passed**, two sequential selected executables per configuration. The checks
retain AA-off geometric edges, independent furnace/direct PBR, explicit M4
fractional-LOD interpolation, masks/depth/picking, 2D/color/text and exposure-
independent UI, inspection cameras and runtime transitions. New reference
self-checks run in the maintained renderer suite. Core_v1 Nutella startup ran
60 rendered frames per orthographic/perspective case without a script module.
Other regression executables were not run; no skipped graphics check is a pass.

The fresh native surface diagnostic passed shared editor/player coverage and
interior parity (maximum display difference .00244141 inside coverage; whole
image .00537109 includes horizon-sensitive boundary samples). It did not promote
the candidate into either host. No candidate source-free package qualification
is claimed. Shipping package inputs are unchanged; the earlier relocated package
evidence remains historical. No CI polling, master merge, release, IBL rebake,
AA, Metal or Stage 5 work occurred.

**Do not start Stage 5 on this result.** The smallest next experiment is to
compress the *view-dependent BRDF transfer*, keeping separate A/B kernels, before
changing any asset format: test angular ranks 8/16/32 against this fixed corpus
and held-out lighting, then time a <=64-fetch reconstruction on M4. Reject it if
it fails these same maximum/near-black gates; do not substitute a fitted metallic
tint or a normal-directed rough endpoint. The measured integration bottleneck is the 9,216-ray
per-pixel cost, not a need for another general renderer/backend architecture.
The tiny-emitter sharp oracle also needs targeted adaptive refinement before
claiming that additional case. No rejected model enters the product meanwhile.
