# Bounded A/B compression handoff — 2026-11-13

Executed follow-up: [bounded angular compression experiment](ibl-ab-compression-experiment.md).
Ranks 8/16/32 were screened with converged references and rejected offline;
no production/GPU candidate ships and the visual gate remains open. The handoff
below retains its original starting-state and authorization record.

This handoff authorizes the **next session** to implement and run the experiment
below. Preparing this document ran no experiment, build, bake or graphics test.
Stage 5 remains separate. Read `CONTRIBUTING.md`, this handoff,
[silhouette-normal-fix.md](silhouette-normal-fix.md),
[ibl-view-dependent-investigation.md](ibl-view-dependent-investigation.md),
[ibl-surface-investigation.md](ibl-surface-investigation.md), then
[ibl-quality-pass.md](ibl-quality-pass.md). The approved architecture remains
[3d-rendering-plan.md](3d-rendering-plan.md); current frame ownership is in
[Follow a frame](../follow-a-frame.md).

## Verified starting state

- Canonical checkout: `/Volumes/External/kadencringle/Projects/Hazel`, branch
  `feature/3d-foundations`. Code/evidence baseline and live origin/tracking ref:
  **`061ae8ed0a2a82534168f2ee10945675c23db953`**, ahead/behind 0/0 at inspection.
  The commit adding this handoff is docs-only; obtain its SHA with
  `git log -1 --format=%H -- docs/design/ibl-ab-compression-handoff.md`.
- Pushed ancestry includes diagnostic `93cd059`, silhouette implementation
  `bb0d43d` and native evidence/correction `1bf4f50`. Canonical local changes at
  inspection were only untracked `.DS_Store` and `examples/.DS_Store`.
- **Separate agent work exists; it is not integrated or qualification evidence.**
  Existing `/Volumes/External/kadencringle/Projects/Hazel-hermes` is on
  `agent/render-reference` at `688e271421bddf809f1a96f16210da4f6f69598a`.
  Its common ancestor with the feature is `1bf4f50`; unique commits, oldest first,
  are `4148d25` (inventory), `2a86b43` (reference gate), `468d204` (gate repair),
  `688e271` (reference scene). Live origin for that branch is `2a86b43`; the last
  two commits are local/unpushed. It also has modified `DocumentSchema.cpp`,
  `examples/UnifiedRenderingShowcase/UnifiedRenderingShowcase.hproj`, modified
  root Finder metadata and untracked `examples/.DS_Store`. Leave that worktree,
  branch and edits untouched; do not merge/cherry-pick them into this experiment.
- Re-inspect both histories/statuses before working: another agent can advance.
  No reset, stash, new worktree, broad clean or unrelated reconciliation. Preserve
  owner content/settings/saves/layouts/Xcode customizations, vendor pins and
  recovery ref `preserve/lastlightkeeper-owner-20261106` at
  `bfbce8119c25fce2aaf1e95226afe362f7098b32`.

## What is fixed, and what is not

Production still uses prepared GGX split-sum specular plus its A/B BRDF LUT.
The terminal normalized r=1 convolution reconstructs E/pi from the dense
irradiance cube **in the reflection direction**; explicit FP32 interpolation of
integer roughness levels preserves the M4 fractional-LOD repair. CPU filtering,
explicit uploaded mips and final encoding are distinct from model accuracy.

`bb0d43d` resolves effective shading normals against visible geometry before BRDF
evaluation. It fixes the black covered silhouette pixels without removing the
guard or adding a fade. The native AA-off regression covers 724 repaired sample
occurrences with exact depth/IDs. Do not revive the older negative-Ns·V cutoff
as a visibility solution. Valid neutral grazing Fresnel must remain.

Residual rough-copper dark shading and positive-grazing dielectric errors are
**view-dependent factorization error**, not solved by either repair. V=N
convolution and environment-independent LUT moments lose directional correlation
between radiance, Fresnel and visibility. At the fresh post-fix copper patch,
red is .14421231 versus .23962647 reference. Correct normals and reference
conventions are essential; `d0fbac0` is historical, not the current baseline.

Do not repeat the rejected campaigns without a specific new reason:

| Approach | Established result |
| --- | --- |
| Normal-directed rough endpoint / dominant-direction correction | Improved copper but worsened dielectric or intermediate roughness; rejected. |
| Accurate full-BRDF radiance/VNDF integration, 8192+1024 rays | Copper RMS .06775653 → .00095453; grazing dielectric RMS .12275634 → .00039886. About **249.13 ms median / 263.07 ms p95**, lighting dispatch only, M4. Not a production solution. |
| 256+64 rays | About **10.53 ms / 11.07 ms**, but misses accuracy (r=.2 max 34.43%, copper max 7.96%). |
| Calibration to existing LUT A/B moments | Does not close accuracy/cost gap; also retains admitted table endpoint error. |

This establishes the need for a cheaper view-dependent representation, not more
global bake samples, a metallic tint, exposure change or a new renderer audit.
The [committed ledger](evidence/ibl-view-dependent.csv) and investigation record
hold grouped errors/costs. Historical M4 Debug/Release Renderer2DSmoke and
RuntimeSessionSmoke passed; this docs session did not rerun them. Those results
do not qualify a compressed candidate, current CI or all Windows/Linux coverage.
Keep historical Windows Debug gameplay-timeout debt separate.

## Entry points and reproducibility

Paths below are relative to the canonical root and were checked against it:

| Owner | Source |
| --- | --- |
| Production lighting and effective normal | `Hazel/Resources/shaders/ForwardPBR.glsl` (`resolveShadingNormal`, `specularLevel`, specular reconstruction) |
| Shared frame / mesh resources and bindings | `Hazel/src/Hazel/Renderer/Scene/SceneRenderer.cpp`, `Hazel/src/Hazel/Renderer/Drawing/Renderer3D.cpp` |
| Offline producer, unchanged for screening | `tools/HazelImport/Environment/EnvironmentBake.cpp`, `RadianceSampler.cpp` in that directory |
| FP64 oracle / quadrature / bounded CPU CLI | `tests/migration/IBLSurfaceReference.h`, `IBLReferenceMath.h`, `IBLSurfaceReferenceCLI.h`; dispatched before graphics startup in `Renderer2DSmoke.cpp` |
| Actual saved-scene capture / sweeps | `tests/migration/IBLSurfaceChecks.h`, invoked by `RuntimeSessionSmoke.cpp --ibl-surface` |
| Rejected GPU experiment, not shipping resources | `tests/migration/IBLIntegrationProbe.h`, `tests/migration/shaders/IBLIntegrationProbe.glsl` |
| Mandatory retained regressions | `GeometricNormalChecks.h`, `IBLSurfaceRenderingChecks.h`, `IBLMipChecks.h`, `IBLRenderingChecks.h` in `tests/migration` |

**Next-session commands, not work performed for this handoff.** From the root,
use a fresh output path and disposable profiles. On this machine full Xcode is
at the path below; re-check it rather than changing personal Xcode selection.

```sh
cd /Volumes/External/kadencringle/Projects/Hazel
export HAZEL_DEVELOPER_DIR=/Volumes/External/Applications/Xcode-beta.app/Contents/Developer
export HAZEL_AB_ROOT="$PWD/build/ibl-ab-resume"
export HAZEL_RESOURCES="$PWD/Hazel/Resources"
export HAZEL_DATA="$PWD/build/ibl-ab-resume-profile"
export HAZEL_SAVE_ROOT="$PWD/build/ibl-ab-resume-saves"
export DYLD_LIBRARY_PATH="$PWD/build/dependencies/mono/macos-arm64/sdk/lib"
# Build only when needed, sequentially, through existing Premake/Xcode services:
python3 - <<'PY'
import sys
sys.path.insert(0, 'scripts')
import hazel as hz
hz.build('Release', tests=True, targets=('MigrationRenderer2DSmoke', 'MigrationRuntimeSessionSmoke'))
PY
python3 scripts/hazel.py run Nutella --config Release --project examples/RenderingShowcase/RenderingShowcase-SurfaceDetail.hproj --inspect-camera
python3 scripts/hazel.py run Hazelnut --config Release --project examples/RenderingShowcase/RenderingShowcase-SurfaceDetail.hproj
```

For a missing capture, run the complete Python capture invocation in
[surface reproduction](ibl-surface-investigation.md#reproduce-and-inspect),
substituting `out = hz.ROOT / 'build/ibl-ab-resume'` and asserting `not out.exists()`
before `snapshot`. It uses the tracked `internal.testing.projects.snapshot`
helper to copy committed content. Required capture environment:
`HAZEL_RENDERING_SHOWCASE_PROJECT` (absolute private hproj),
`HAZEL_IBL_SURFACE_CAPTURE` (absolute new capture directory), `HAZEL_DATA`,
`HAZEL_SAVE_ROOT`, `HAZEL_RESOURCES`, and the Mono library path above. It invokes
`MigrationRuntimeSessionSmoke Core.dll Fixture.dll --ibl-surface`, with the two
DLLs in the matching Release products. There is no existing compression CLI.

This regenerates `capture/capture.yaml`, raw bottom-up RGBA32F/ID32, PPM views,
`sweep-inputs.txt` and `sweep-results.txt` (specular RGB, diffuse RGB, LUT A/B/N·V).
It captures 13 native controller poses, five numeric poses (-6/-3/0/3/6),
constant/untextured/no-normal-map controls and 384 domain rows. It is diagnostic
capture, not the complete runtime regression suite or physical-input testing.

For example, CPU-only reference runs against the generated sweep are:

```sh
bin/Release-macos-arm64/MigrationRenderer2DSmoke/MigrationRenderer2DSmoke --ibl-reference examples/RenderingShowcase/Assets/Environments/Studio.hdr "$HAZEL_AB_ROOT/capture/sweep-inputs.txt" 2 "$HAZEL_AB_ROOT/studio-ref2.txt"
bin/Release-macos-arm64/MigrationRenderer2DSmoke/MigrationRenderer2DSmoke --ibl-reference examples/RenderingShowcase/Assets/Environments/Studio.hdr "$HAZEL_AB_ROOT/capture/sweep-inputs.txt" 4 "$HAZEL_AB_ROOT/studio-ref4.txt"
```

`--ibl-reference` supports subdivisions 1–16 and fifteen output columns: full
specular RGB, geometric-horizon RGB, normalized prefilter RGB, diffuse RGB, E/pi
RGB. `--ibl-reference-ndf SOURCE INPUT ORDER OUTPUT` supports orders 32–2048 and
**six** columns (full specular, geometric-horizon). Both retain existing outputs,
512-row/1-MiB input and five-billion-evaluation bounds; split work explicitly.
For sharp work use NDF 1024/2048 on sharp-only subsets, not the nonconverged
sharp source-cell estimates or an over-budget 384-row NDF-2048 invocation.

### Inputs and evidence locations

Oracle rows are `Ns.xyz V.xyz Ng.xyz base.rgb roughness metallic AO`, directions
normalized in environment coordinates. Use the **effective** N from numeric
Normal mode 2 (`2*RGB-1`), not raw mode 14. V is normalized camera Eye minus mode
12 WorldPosition; Ng is mode 13. Rotate all three by captured yaw:
`(cos(yaw)*x-sin(yaw)*z, y, sin(yaw)*x+cos(yaw)*z)`. Base is mode 1, rough/metal
are mode 11 G/B, AO mode 4 B, isolated specular is mode 8. Mode 15 stores
raw Ns·V, Ng·V, effective Ns·V. Read Eye/yaw/UUID-to-handle mapping from YAML;
never reuse native pixel-trace entity handles as permanent IDs.

To regenerate the 150 actual rows: at step 0, use UUIDs 110/111/112/113/114/121
in that order; choose pixels with all four cardinal neighbors of the same entity,
raw Ns·V>.02 and Ng·V>.001. In bottom-up buffer order take 24 indices per entity
at `floor((q+.5)*count/24)`, q=0..23. Append top-down named pixels (902,567),
(893,525), (916,586), (612,200), (619,194), (585,267), using index
`(height-1-y)*width+x`. Save input rows plus pixel/group/actual-RGB metadata.
The 64-row sharp corpus is actual rows with roughness<.06, followed by positive-
N·V sweep rows with roughness<.06, retaining order. For five-pose orbit comparisons
use eight stratified copper/wood pixels per pose by the same rule.

Primary model isolation gates view visibility with Ng·V and uses the full
**shading** incoming hemisphere, matching production; it adds no Ng·L truncation.
Optional `horizon-` controls add Ng·L>0 and compare only with oracle columns 4–6.
Use columns 1–3 for primary accuracy. Controlled sweep Ng=Ns removes this
convention ambiguity. Match exposure 0, gain 1, yaw .349999994, materials/normal
maps/AO and camera; disable direct light. Do not count silhouette correctness as
a model improvement or compare raw normals on one side with resolved ones on the other.

Lighting fixtures (all tracked, no download/rebake needed):

- `examples/RenderingShowcase/Assets/Environments/Studio.hdr`: original Studio
  Small 09 by Sergej Majboroda / Poly Haven, CC0-1.0; SHA-256
  `e7cfda5f4e98e623db12b8bfd0184e048488e4855d9c83e2751fb44a32e80c45`.
  `Studio.henv` accepts High generation `d030bea247f14dd3`. Material provenance:
  `examples/RenderingShowcase/Assets/Materials/PolyHaven-original-downloads.json`
  and `Assets/NOTICE.yaml` under that project.
- `examples/RenderingReference/Assets/Reference/IBL_v1/Sources/AsymmetricAxes.hdr`:
  original Hazel Apache-2.0 directional diagnostic, described by `AsymmetricAxes.henv`.
- `examples/RenderingReference/Assets/Reference/IBLQuality_v2/TinyEmitter.hdr`:
  original Hazel Apache-2.0 256×128 RGBE diagnostic, .01 surround and one 10000
  pixel at (139,50); provenance in neighboring `TinyEmitter.henv`.

Ignored, machine-local evidence currently exists in `build/ibl-view-evidence`:
`baseline/`, `same256/`, `same2048/`, `same8192/`, `same-axes2048/`,
`same-tiny8192/`, `*-ref*.txt`, `*-sharp-ndf*.txt`, `sharp-inputs.txt`, orbit data,
`Debug-qualified/`, `Release-qualified/` and `crops/`. Crops are rejected-prototype
numeric composites, not accepted Nutella screenshots or native motion acceptance.
Its Python extract/report scripts are **untracked conveniences**, not prerequisites;
regenerate from tracked capture/oracle code and the rules above if absent.
Older `build/ibl-surface-evidence`, `build/silhouette-evidence` and
`build/ibl-quality-evidence` are likewise local historical artifacts. Never clean
or overwrite them. The committed ledger/records survive a fresh clone.

Target TinyEmitter sharp rows **9, 15 and 18** (1-based in regenerated
`sharp-inputs.txt`): historical 1024/2048 deltas were 1.7899%, .9376%, 1.0102%
floor-relative. Refine these adaptively first, with bounded work and independent
convergence checks; then check all required/new held-out rows. Apparent candidate
agreement on an unconverged row cannot count as a pass.

## Implement next: accuracy-first go/no-go

1. Define separate angular transfer kernels, not just the existing scalar LUT:
   `K=D_GGX*V_correlatedSmith*max(Ns·L,0)`, `Fc=(1-V·H)^5`,
   `A=integral(E*K*(1-Fc))`, `B=integral(E*K*Fc)`, result `F0*A+B`.
   Preserve roughness floor .045, alpha=r², existing F0, AO/gain and horizon policy.
   Specify basis/rank meaning, frames/rotation, precision and interpolation.
   Before fitting, declare bounded storage/working-memory ceilings and count all
   per-environment/shared/view/roughness data; there is no approved unbounded grid.
2. Implement offline CPU screening of **ranks 8, 16, 32**, separately for A/B.
   Fit transfer kernels independently of Studio/material colors. Test stored
   nodes **and interpolation between stored view angles and roughness values**,
   endpoints/sharp/intermediate/r=1, normal/grazing views, metals/dielectrics/mixed
   metallic, bright/dark regions, seams/rotations and the actual pixel/orbit corpus.
   Hold out directions and lighting (Axes/TinyEmitter) from fitting/tuning. Preserve
   an independent converged FP64 oracle; do not share the candidate estimator.
3. Keep floor-relative RMS <=2%, max <=5%, denominator `max(reference,.02)`;
   below .02 also absolute RMS <=.0005, max <=.002. Report groups/worst rows, not
   a bright-peak-normalized or bake-average substitute. Reference uncertainty
   targets remain <=.5% sharp / <=.1% broad maximum floor-relative. Unconverged
   required rows block promotion. No threshold changes after seeing candidates.
4. **Only if offline accuracy and storage qualify**, build a test-only M4 OpenGL
   4.1 GPU prototype, total reconstruction <=64 texture fetches including A/B,
   coefficient/LUT accesses and view/roughness interpolation. Count ALU/bandwidth
   too. Time the **full shared native 1280×720 AA-off scene**, median <=16.67 ms,
   p95 <=20 ms, with stated warmup/sample count and no compile/readback overhead.
   The old fullscreen captured-input probe is not full-frame qualification.
5. If accuracy, storage or GPU cost rules it out, stop promotion: report each rank's
   RMS/max/near-black and interpolation/held-out failures, exact worst inputs and
   reference uncertainty, storage/peak memory/preparation time and measured costs.
   Record the best rejected result and smallest next experiment; do not ship a
   compromise, relax gates, hide HDR energy or recommend Stage 5 from a failed gate.

No production shader or asset-format edits during offline screening. Eventual
promotion requires valid grazing Fresnel, AA-off silhouette/fractional-LOD,
furnace/direct/picking/2D/UI regressions; real slow native motion with AA off;
fixed-view and Hazelnut/Nutella parity; and source-free prepared-asset/package
closure. Version a genuinely changed bake/cache contract deliberately and update
only required assets after qualification. Frozen Core_v1/Import_v1/Direct_v1/IBL_v1
conditions and authored showcase views remain evidence, not tuning knobs.

Use one bounded heavy job at a time, isolated projects/profiles and existing
runners. Stop heavy work on external-volume loss/I/O errors. No Stage 5, Metal,
AA, unrelated features/dependency churn, master merge or release. Commit/push
reviewable experiment/results checkpoints only on `feature/3d-foundations`.

After a qualified implementation, build matching Debug/Release targets and run
the existing focused qualification once per configuration, for example:

```sh
python3 scripts/hazel.py test --config Release --profile native --only Renderer2DSmoke --only RuntimeSessionSmoke
```
