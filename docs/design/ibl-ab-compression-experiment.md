# Bounded A/B angular compression experiment

Pre-fit declaration, 2026-11-13; starting HEAD `7f2885073373874fdd5b2986f6ee5aac65293dc8`.
This is an offline rejection/qualification experiment, not a production change.

The candidate projects the separate transfer kernels K(1-Fc) and K Fc into
orthonormal real spherical harmonics in the local frame Z=effective Ns,
X=normalize(V-Ns*(Ns.V)), Y=Z cross X. At normal incidence use a deterministic
orthogonal X. Isotropy removes sine modes in this frame. Rank means 8, 16 or 32
retained cosine/m=0 modes per kernel, ordered by degree then m=0..degree;
the last band may be partial. These are linear angular subspaces, not SVD ranks
or claims about every possible rank-32 representation. Maximum degree is 7.
Rotation uses complete world-space SH bands (16/36/64 RGB coefficients), so a
partial local band never implies a partial world band. No radiance clipping,
positivity clamp, metallic fitting or energy renormalization is permitted.

Fit only the authored GGX/correlated-Smith/Schlick transfer, r floor .045,
alpha=r*r, with deterministic FP64 NDF-CDF integration. Store FP32 A/B tables
at N.V={.0005,.002,.01,.02,.05,.1,.2,.35,.5,.7,.85,1} and
r={.045,.05,.1,.2,.35,.45,.6,.75,1}. Interpolate four neighbors explicitly in
FP32 in N.V/r; evaluate controls at exact nodes and midpoint nodes, plus direct
coefficient projections to isolate interpolation error. Lighting projections
and rotation evaluation use FP64 as an optimistic screening bound; count FP32
runtime storage. Fit has no Studio, Axes, TinyEmitter, surface or material data.
Axes and TinyEmitter are held-out lighting. Actual/orbit normals are captured
effective normals, all directions in environment coordinates. Gate by Ng.V;
integrate the full shading incoming hemisphere, without Ng.L truncation.

Before fitting: **per-environment ceiling 1 MiB; shared runtime ceiling 1 MiB;
peak process RSS 512 MiB; each heavy child <=900 seconds, one at a time**.
No unlimited orientation/view/roughness texture grid is allowed. At rank 32:
64 RGB FP32 environment coefficients=768 bytes; two 12*9*32 FP32 tables=27,648
bytes; 21 axis FP32 nodes=84 bytes. Harmonics recurrence/rotation is analytic,
no hidden sampled angular bank or source HDR at runtime. Existing background /
diffuse resources are unchanged and additional to this specular representation.
Offline source decode <=64 MiB, <=2048*1024 RGB source pixels; rows <=1024,
chunks <=65536 angular points, max fit order 512. Disk evidence ceiling 64 MiB
excluding the existing capture and isolated Python environment.

Frozen gates: floor-relative RGB RMS <=.02, max <=.05, denominator
max(reference,.02); channels below .02 also absolute RMS <=.0005 and max <=.002.
Independent reference convergence max <=.005 for r<.06 and <=.001 otherwise.
Unconverged required rows block promotion. Retain exact worst inputs, group
errors and convergence flags; no threshold changes after candidates are seen.
First refine TinyEmitter sharp rows 9/15/18 with localized FP64 source quadrature
and an independently checked constant-field term. Then assess every required
row, including stored nodes, view/roughness midpoints, metals/dielectrics/mixed,
grazing/sharp/intermediate/r=1, rotations/seams, 150 actual pixels, 80 orbit
pixels and positive-view saved sweep rows. Negative-view rows are domain controls.

Only a fully converged accuracy/storage pass authorizes a test-only GPU step.
Count all <=64 reconstruction fetches (A/B, coefficients, four-node interpolation
and rotation), ALU/bandwidth, then measure full native M4 GL4.1 1280x720 AA-off
frames: median <=16.67 ms, p95 <=20 ms. A dispatch proxy cannot qualify it.
Failure means no GPU/product/format changes and an unresolved visual gate.

Reproduction tooling and measured results will be recorded below after execution.

## Executed result: no-go

**Reject ranks 8, 16 and 32 of this harmonic representation. No production or GPU
candidate ships. The visual gate remains open; Stage 5 has not begun.** This is a
rejection of these fixed linear angular spaces, not of all low-rank or nonlinear
view-dependent transfer representations.

The fit and all 758 reference rows were executed on Apple M4. The capture is the
retained post-silhouette SurfaceDetail capture: its shader SHA-256 matches the
unchanged current `ForwardPBR.glsl` (`84573b53…f94a4`). Extraction uses the YAML
UUID mapping, effective mode-2 normals and recorded eye/yaw, not historical
runtime handles. There are 150 actual samples, 80 copper/wood samples over five
poses, 384 saved sweep rows and 144 new controls. The new controls cover stored
and midpoint N.V/r pairs, two orientations/tangent rolls including a longitude-
seam direction and cube-corner direction, metallic 0/.5/1, sharp floor/.0475,
intermediate and r=1. Of these, **694 are positive-view primary rows**; 64 are
negative-view controls and contribute zero, rather than diluting the table below.
No Ng.L truncation is introduced. All three unchanged lighting fixtures were
screened; Axes/TinyEmitter never enter fitting.

### Independent reference and estimator checks

TinyEmitter sharp rows 9/15/18 were refined **before fitting** at localized source
orders 16/32/64/128. The maximum 64→128 floor-relative changes were respectively
3.973e-7, 1.010e-7 and 3.747e-7 (fractions, not percentages). Their final RGBs and
exact inputs are retained in [the evidence directory](evidence/ibl-ab-compression/).
The former NDF-2048 values differ from these by up to .8010% floor-relative.
They were not reused for acceptance.

All final reference rows were regenerated through the native FP64 oracle, without
using historical reference result files. Studio/Axes broad rows use independent
source subdivisions 2/4, with bounded 8/16 refinement; sharp rows use NDF
1024/2048, with targeted 2048/4096 refinement. Three Axes broad grazing controls
also require NDF2048/4096 convergence AND agreement with source subdivision 16;
maximum differences are .02730% and .01343% respectively. No row was accepted by
selecting a favorable unconverged method. TinyEmitter uses split bilinear-tent
source integration plus the independent constant-field furnace integral. The
new oracle clamps normal-incidence cosine roundoff and rejects nonfinite output;
a new normal-incidence regression exercises that case. Earlier diagnostic files
with nonfinite values are retained locally and excluded from all committed metrics.

| Fixture | Maximum sharp uncertainty | Maximum broad uncertainty | Unconverged required rows |
| --- | ---: | ---: | ---: |
| Studio | .484623% | .027544% | 0 |
| Axes | .078628% | .097773% | 0 |
| TinyEmitter | .002359% | .020012% | 0 |

The independent reference self-checks pass, including localized/full-source
emitter agreement. A separate candidate constant-field check against native
NDF4096 gives maximum floor-relative difference .0000195743 (fraction), and
analytic SH rotation reconstructs transformed harmonics within 1.95e-15.

Kernel tables use order 512; order-256 controls retain the same rejection.
[Fit convergence](evidence/ibl-ab-compression/fit-convergence.json) quantifies
candidate output changes per fixture/rank. Lighting SH projection was refined
from midpoint integration to generic Gauss orders 3/6 split at every bilinear knot
and clamped polar boundary, for **all fixtures with identical settings**. This
refines the numerical integral, not the basis, nodes, lighting or gates. The final
maximum output change is 7.60e-9 floor-relative. No rejected source projection,
nonfinite reference or unconverged row supplies a pass.

### Accuracy

The following is pooled RGB floor-relative RMS / maximum over the 694 primary
rows. The ledger additionally retains each channel's RMS, near-black channel
RMS, every material/roughness/view group, actual/orbit groups and exact worst
inputs. Every group must meet the unchanged gates; pooled RMS alone cannot pass.

| Lighting | Rank 8 RMS / max | Rank 16 RMS / max | Rank 32 RMS / max |
| --- | ---: | ---: | ---: |
| Studio | 731.510% / 6748.665% | 923.669% / 10754.966% | 878.722% / 15027.707% |
| Axes (held out) | 2.920% / 28.574% | 2.908% / 27.532% | 2.889% / 26.533% |
| TinyEmitter (held out) | 1330.605% / 24897.471% | 1591.805% / 32498.031% | 1686.227% / 21131.420% |

Near-black absolute RMS / maximum, against limits .0005 / .002:

| Lighting | Rank 8 | Rank 16 | Rank 32 |
| --- | ---: | ---: | ---: |
| Studio | .237261 / 1.349733 | .281170 / 1.172706 | .236552 / 1.440385 |
| Axes | .001027 / .005715 | .001016 / .005506 | .000980 / .005307 |
| TinyEmitter | .326306 / 4.979494 | .397640 / 6.499606 | .419677 / 4.226284 |

Angular truncation spreads narrow HDR energy and produces signed ringing. It is
not fixed by table interpolation or by keeping only the rough copper improvement.
No negative values were clamped or HDR peaks hidden. Actual Studio copper at rank
32 still fails: **2.054% RMS / 5.524% maximum**. Its orbit group alone passes
(1.410% / 3.652%), while wood orbit fails (122.952% / 529.671%). A passing subset
cannot qualify the representation.

For rank 32, stored-node / midpoint / directly projected midpoint RMS and max:

| Lighting | Stored nodes | Interpolated midpoints | Direct midpoint coefficients |
| --- | ---: | ---: | ---: |
| Studio | 1403.180% / 7329.192% | 783.281% / 3074.130% | 807.376% / 3221.440% |
| Axes | 1.259% / 10.417% | 3.586% / 17.312% | 1.236% / 10.792% |
| TinyEmitter | 1605.877% / 5705.640% | 1532.345% / 6572.250% | 1661.706% / 8826.157% |

The rank-32 interpolation-only absolute maxima are .105241 (Studio), .029276
(Axes), .450782 (TinyEmitter). Direct coefficients still fail, independently
establishing angular representation error. Node interpolation-only differences
are <=1.16e-7 (FP32 table storage). Ranks 8/16 have the same retained controls in
[groups.csv](evidence/ibl-ab-compression/groups.csv) and
[metrics.json](evidence/ibl-ab-compression/metrics.json).

Rank 32 is the best rejected result by the **worst primary error across lighting**,
although rank 8 has lower pooled RMS on some fixtures. Its worst row is evidence
row 47, actual sharp sphere UUID 110 at top-down pixel (168,322): TinyEmitter
reference RGB (.013626470148,.00910333979949,.00767648433314), candidate
(4.23991043910,2.83255646130,2.38859607069). Its reference uncertainty is
.000309%. Studio has the same rank-32 worst row: reference red .055238162255,
candidate 8.35626741925, uncertainty .078233%. Axes rank-32 worst is row 42,
pixel (66,351): reference red .00824602082737, candidate .0135526953435,
uncertainty .011955%. The full 15 input values and worst rows for every rank,
group and fixture are in the committed JSON; no bright-peak normalization is used.

### Storage, memory and cost

| Rank | Per-environment FP32 SH | Shared FP32 A/B + axes | Illustrative explicit reconstruction fetches |
| --- | ---: | ---: | ---: |
| 8 | 192 B | 6,996 B | 32 |
| 16 | 432 B | 13,908 B | 68 |
| 32 | 768 B | 27,732 B | 128 |

All storage fits the predeclared 1 MiB + 1 MiB ceilings. This counts complete
world bands, separate A/B tables and all view/roughness nodes. The conservative
texture layout reads one RGB world coefficient per complete-band mode, plus four
explicit interpolation neighbors for each packed vec4 A/B coefficient. It counts
all accesses, including table interpolation. It is not an optimized lower bound:
uniform packing or analytic coefficient evaluation could alter it. Rotation has
no stored orientation grid; one direct rank-32 band rotation uses 312 matrix
weights with RGB dot products, plus harmonic/rotation generation and the final
A/B reconstruction. No GPU ALU/bandwidth or frame-time claim is inferred from
these counts. Rank 8's count alone does not authorize a GPU step.

Order-512 fitting took **20.489 s**, peak RSS **100.3 MiB**. Final screening took
**13.805 s**, peak RSS **116.2 MiB**. Lighting projection times (decode plus both
Gauss orders, not a production bake) were Studio **7.559 s**, Axes **.623 s**,
TinyEmitter **.625 s**. Fresh references plus selected refinements/regeneration
took **247.723 s**, maximum native child RSS **22.2 MiB**, controller RSS **88.3
MiB**. These are offline CPU preparation/verification costs, not runtime timings.
The largest measured process stays below 512 MiB. The committed numerical bundle
is about 1.3 MiB; ignored intermediate evidence stays below 64 MiB, excluding the
isolated Python environment and pre-existing capture. One heavy job ran at a time;
none approached the 900-second ceiling. Earlier failed diagnostic runs and the
order-256 control are additional verification cost, not hidden production cost.

**No GPU prototype was authorized by the offline gate.** No diagnostic dispatch,
full native candidate frame, median/p95 GPU time, candidate motion, candidate
Hazelnut/Nutella parity or source-free candidate package was measured or claimed.
The <=64-fetch and <=16.67/20 ms full-frame gates remain mandatory for any future
qualifying representation.

### Retained checks and reproduction

Release build through existing Premake/Xcode services succeeded. Independent
FP64 reference self-checks passed. Native M4 OpenGL 4.1 Renderer2DSmoke and
RuntimeSessionSmoke both passed through the normal private-project runner,
including 724 AA-off silhouette samples with exact depth/IDs, positive grazing
furnace, fractional LOD, direct/2D/UI/picking, frozen IBL cases, saved views,
editor/player parity and runtime transitions. Core_v1 player startup also rendered
60 frames per orthographic/perspective case. See
[native-release.txt](evidence/ibl-ab-compression/native-release.txt). This session
ran Release only; no new Debug, Windows/Linux, candidate package or native motion
qualification is implied. The new sparse reference checks are retained in the
existing suite.

Build `MigrationRenderer2DSmoke` using the handoff's existing build invocation.
Use a fresh output directory and isolated Python environment (NumPy 2.3.5,
PyYAML 6.0.3); these are offline tool dependencies, not vendor/runtime pin changes.
Reuse a verified matching capture or regenerate one using the
[surface capture procedure](ibl-surface-investigation.md#reproduce-and-inspect).
Then, sequentially:

```sh
# HAZEL_AB_ROOT must be an absolute, fresh private directory.
python tests/experiments/ibl_ab_compression.py references --capture /absolute/capture --output "$HAZEL_AB_ROOT/references"
python tests/experiments/ibl_ab_compression.py fit --order 512 --output "$HAZEL_AB_ROOT/fit"
python tests/experiments/ibl_ab_compression.py screen --references "$HAZEL_AB_ROOT/references" --fit "$HAZEL_AB_ROOT/fit" --order 512 --output "$HAZEL_AB_ROOT/screen"
python tests/experiments/ibl_ab_compression.py bundle --references "$HAZEL_AB_ROOT/references" --fit "$HAZEL_AB_ROOT/fit" --screen "$HAZEL_AB_ROOT/screen" --output "$HAZEL_AB_ROOT/bundle"
```

Reference CLI outputs are now NDF 32–4096 (six columns), source subdivisions
1–16 (fifteen columns), or `--ibl-reference-sparse` order 8–128 (**three primary
RGB columns only**). Sparse mode validates the exact uniform-plus-one-interior-
emitter structure; it supplies no geometric-horizon result. The 512-row, 1 MiB
input and five-billion-evaluation limits, non-overwrite outputs and independent
reference-convergence gates remain. `--ibl-reference-self-check` is CPU-only and
explicitly excludes graphics coverage. Every experiment command reports MEASURED,
with pass/no-go decisions in JSON; it does not substitute for the native runner.

The main checkout remained on `feature/3d-foundations`; the only original dirt
was the two untracked Finder files. Live feature origin initially matched the
verified handoff; Hermes remained at `688e271`, ahead two of its live `2a86b43`
remote with its recorded dirty files. It was inspected read-only and untouched.
No reset, stash, extra worktree, unrelated merge, vendor change, owner-content
edit, bake/cache/format/shader change, Metal, AA, master merge or release occurred.
The recovery ref remains `bfbce8119c25fce2aaf1e95226afe362f7098b32`.

The smallest next experiment is **one reflection-centered analytic sharp A/B lobe
plus rank-8 residual**, initially only on the exact worst Studio/TinyEmitter row
47 and the failed positive-grazing/midpoint controls, with this same oracle,
thresholds and budgets. It tests whether a narrow transported component avoids
global harmonic ringing; it is not authorized for production or a reason to
repeat larger SH ranks. Expand to the complete retained corpus before any GPU
work if that bounded test qualifies. No compromise from this rejected run ships.
