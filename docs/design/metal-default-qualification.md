# Metal default-backend qualification and production cleanup

Starting tree: `93d39bd6d71b090a3a931f1a457a9df27347c91c`,
`feature/3d-foundations`. This checkpoint follows [M6](metal-m6.md).
Status: **INCOMPLETE — prior blockers diagnosed; corrected candidate awaiting final acceptance**.
Current checkpoint: stable native renderer/package gates PASS on Apple M4/macOS 27.3.
Automatic is now the default request. Debug/Release compilation and policy plus
Debug save/restart checks PASS. Native no-flag/capture and actual saved OpenGL
restart continuation now PASS; the latest closure section tracks final refreshed gates.
Sections below retain chronological failed attempts and superseded holds.
No merge, release, XeGTAO or new lighting features are included.

## Acceptance declared before final execution

- CPU backend-policy tests: Automatic with/without native support, explicit requests,
  CLI > project > platform precedence, duplicate/conflicting CLI rejection,
  legacy missing Rendering/Backend keys, round trip and malformed schema rejection.
- Resolve before window/device initialization in both applications. Runtime failures
  propagate; no post-initialization fallback or live backend replacement.
- Project Settings: save transaction, launch/active/requested backend, restart warning,
  active GPU/capabilities with provenance, inactive availability only; real screenshots.
- Debug/Release builds; focused policy regression before maintained native OpenGL,
  M3 exact-ID/independent line, M4 frozen PBR/IBL/numerical, shader/cache/source-free,
  M5 compute/editor/abort/recovery/pending shutdown and native 1x/2x gates.
- Actual editor/player products on both backends and Automatic; Release source-free
  relocated packages with spaces/Unicode, ordinary close and zero tracked retirement.
- Same-scene performance observations, warmup/sample counts, median/p95 CPU/GPU,
  resource peaks and RSS. Distinct scopes/formats cannot establish backend speedup.
- Full relevant Windows/Linux/hosted macOS CI for the committed candidate; no hosted
  GPU claim. If unavailable, full production qualification remains INCOMPLETE.
- Public docs/link/commands, warning and adversarial success/failure path review.

Machine-readable checkpoint: [metal-default-results.json](metal-default-results.json).

Raw logs and timing data: `build/metal-default-evidence/`. Existing unrelated
material edits and untracked developer shader artifacts are excluded from commits.

## Baseline and observations

- Requested M6 run `44212291235` matches candidate
  `35e4f4116cba7bc57a12e4451bdf293acc2c5cf3`. At start all Linux, Windows and
  hosted macOS jobs are IN PROGRESS; no successful whole-run result is inferred.
  Snapshot: `m6-ci-start.json`.
- No-change Release `build --tests`: 5.09 s wall (`baseline-build-release.log`).
  This includes dependency verification, Premake/Xcode, shader checks and example
  script build; it is incremental reuse, not clean compilation.
- Focused Release SceneFoundationSmoke: 6.06 s wall, including unrelated global
  SDK/project-creation/script compilation and tutorial preparation. Full/CPU suites
  retain those acceptance checks; explicit focused selection reports their exclusion.
- Four retained runtime/focus numerical comparisons: 18.169 s serial CPU wall.
  These measurements exclude capture and compilation (`baseline-comparison.json`).
- Runtime baseline: `baseline-benchmark/results.json`. Same M6 production benchmark,
  720 frames/extent, 120 warmup and 20 tail excluded. Original OpenGL RGBA16F versus
  Metal RGBA32F and GL elapsed queries versus Metal commit feedback are retained;
  there is no backend speedup claim. The initial CPU test setup overlapped part of
  this run, so it is diagnostic only; final timing must be isolated.

## Architecture and optimization decisions

One SceneRenderer, existing Renderer2D and completion-gated three-slot Metal queue
remain authoritative. Native queue barriers, drawable waits and retirement gates
are necessary contracts, not demonstrated obsolete debt. M6 shows drawable
acquisition dominating editor submission time. Removing those waits without a
new admission/pacing contract would change correctness and is not justified.
Metal's duplicated settings resolver is replaced with the shared RendererPolicy
resolver. Its offline shader path is described explicitly rather than falsely
reported as GLSL; capability limits distinguish engine contracts/family guarantees
from queried active-device facts.

Independent CPU comparisons can run concurrently without changing a numerical
oracle. No GPU endurance or unique reference case is removed from full acceptance.


## Implementation verification and current blocker

- Debug affected build PASS, 277.724 s; maintained native OpenGL suite PASS,
  311.968 s / all 18 executables and ordinary reference-player launches.
- Debug M3 PASS, 26.85 s; Debug M4 PASS, 60.81 s / all 135 captures; Debug
  shader/cache/source-free PASS, including all 17 malformed/rejection cases.
  Exact results and unchanged numerical tolerances are in `full/`.
- M5 Debug compute PASS. The next native consumer attempt FAILS its console
  admission gate: `CGSSessionScreenIsLocked=true` in `process-session.json`.
  One submission completed, zero positive presentations; retirement is zero.
  This attempt is **not** counted as native presentation qualification.
  The full runner stopped on that failure; downstream gates are NOT RUN.
- Native continuation requires the owner to unlock the GUI session. The global
  IOConsoleLocked flag alone is insufficient; both session records and positive
  presentation are required. No assertion or console gate is bypassed.
- Actual editor/player duplicate/conflicting CLI requests PASS pre-window rejection
  (`cli-rejections/results.json`). CPU synthetic support/unsupported-device cases,
  project round trip, legacy keys and prelaunch precedence PASS.
- Initial native app inspection showed the private photographic editor; CUA menu
  actions were not verified. That observation is not a Project Settings interaction
  PASS. The maintained native capture fixture now has a backend-settings mode;
  its screenshot run remains blocked by the locked console.

## Measured efficiency changes

| Scope | Before (s) | After (s) | Evidence / invariant |
| --- | ---: | ---: | --- |
| Focused SceneFoundationSmoke wall | 6.06 | 2.64 | Explicit `--only` omits unrelated global SDK/authoring/tutorial setup; full/CPU retains it |
| Four frozen runtime/focus CPU comparisons | 18.169 | 9.38 | Two workers; every result field, hash, error and exact ID result identical |
| Product shader preparation | 2.056 | 0.330 | Includes copy + producer source/toolchain/hash validation; all eleven artifact hashes identical |

These are measured component scopes, not an estimated total full-suite speedup.
No numerical oracle, frozen reference, exact picking assertion, GPU frame count
or fixed endurance wait is weakened. Native GPU launches remain serial. The
product matrix reuses verified build shaders; cold/corrupt/forced-producer gates
retain their independent generation. Windows/Linux Debug CI runs the eight CPU
executables before Release/packaging and the ten graphics executables later;
the union is the original 18, with no repeated Debug CPU execution. Release all
and the hosted macOS CPU subset remain intact. Workflow triggers/badge scope
remain unchanged.

## Bounded production optimization

Metal creates at most six immutable depth compare/write combinations per device,
shared by indexed and 2D draws. Draw packets retain native state owners; shutdown
still drains and releases frame owners before releasing the cache/device. The
consumer fixture creates one state across its 900-frame run instead of per-draw
state creation. It does not add a generic cache framework or change pipeline keys.

Validation-disabled Release consumer preparation CPU (749 warmed samples) was
0.033583 / 0.067208 ms median/p95 before and 0.029917 / 0.053709 ms after.
Whole submission CPU was 4.179542 / 4.414208 versus 4.140875 / 4.324916 ms;
acquisition dominated both. Whole-commit GPU medians were 2.785792 versus
3.015917 ms, with p95 3.714333 versus 3.811500 ms. This does **not** establish a
GPU or frame-rate speedup. Preserve raw traces (`isolated-baseline-editor/`,
`optimized-editor-timing/`) and their actual positive-presentation/retirement
checks. Preparation is the optimization's scope, not compositor pacing. Driver
memory for native depth-state objects is unavailable; existing tracked resource
counters and RSS must not be described as total GPU allocation.

OpenGL's output and Neutral-composite guards now accept existing RGBA32F textures
in addition to RGBA16F. No shader math or ordinary application format changes.
A new independent IEC output vector verifies the additional source. The explicit
FP32-display benchmark permits controlled color formats without lowering accepted
Metal quality; depth and GPU timing boundaries still differ and are reported.
Its final isolated run subsequently PASSes; see the stable timing update below.


## M6 CI follow-up

At the later poll, the same candidate run's hosted macOS job has PASSed and the
Windows job has FAILed. Windows completed Debug/Release compilation and reached
Debug software-rendering regression execution. `Renderer2DSmoke` fails the exact
unlit picking/depth invariant in `GeometricNormalChecks.h` on Mesa llvmpipe
26.2.3 / LLVM 23.1.2 / GL4.6 SPIR-V specialization. The combined assertion does
not identify whether IDs, depth or both differ; neither a driver bug nor an engine
cause is established. The new diagnostic prints the first mismatch coordinate,
IDs and hexadecimal depth values while preserving the exact assertion. No
reference reset or tolerance increase is applied. Raw job log is
`m6-windows-job.log`; `gh run view --log-failed` remains unavailable until the
whole run completes, so the completed job's log was retrieved directly.

That intermediate poll retained Linux as IN PROGRESS. The completed run now reports Linux FAIL at the same exact unlit picking/depth assertion. This is not candidate CI for the current
implementation, and no M6 production PASS is inherited. Windows reproduction
and an exact-preservation correction/qualification are prerequisites before
advancing to XeGTAO. Automatic Metal activation additionally requires the blocked
native presentation/editor/package gates to complete.


## Historical default activation hold (superseded by native completion)

The unlocked-console gate is still unavailable after independent work, so the
conditional authorization to activate a new platform default is **not satisfied**.
`DefaultRendererBackend` remains OpenGL. Missing Backend/Rendering keys preserve
legacy startup behavior. Explicit `--automatic` / a saved Automatic request exercise
the new portable policy; explicit Metal/OpenGL remain available. Project Settings
explains the qualification hold. No hardware capability is falsified to enforce it.
Once the native gates pass, changing the one default constant and qualifying default
product launches is the remaining activation step. This milestone remains
INCOMPLETE until that and the required CI/regression work is resolved.


## Resumed native qualification

The owner resumed work with an unlocked GUI session (`console-continue.json`).
Native M5 Debug 1x and 3D lifecycle 1x pass. The initial 2x workflow fails its
sprite-preview draw visibility assertion; preceding texture reload, cache identity
and separate linear/sRGB assertions pass. A focused diagnostic repeat proves the
expected native texture is present in the sprite panel's detached viewport
814097800 (five draw lists), while the assertion incorrectly searches only main
viewport 286331153. Evidence: `sprite-diagnostic-2x/workflow/run.log`, lines with
`Sprite preview draw evidence` / `expected texture present`. The fixture now
requires the panel to be active and searches that panel's own finalized viewport
draw data. It does not drop or relax the visibility/texture-identity assertion.
Both unsuccessful attempts are retained; they are not production renderer failures
or successful workflow qualification. Focused continuation follows the correction.


### Candidate checkpoint before default activation

The corrected 2x sprite-viewport assertion passes. That focused workflow then
fails the existing Import GLB activation check: the report still belongs to the
earlier SDK/Python request rather than a native-import request. No successful
native import is inferred. Focused activation/layout diagnostics are being added;
the Import button, native-producer execution and exact import assertions remain
mandatory. The candidate retains OpenGL as the default until native completion.
A branch CI dispatch is for cross-platform correctness and first-mismatch data,
not a claim of readiness or a badge-only run. All unfinished gates remain INCOMPLETE.


### Exact packed-depth correction and fixture stabilization

Candidate `f3340c8d6bcf39d1a521601098b54830c0746ab3` was pushed and dispatched as
run `44289135829`, with its head SHA verified. Windows reports ID `8 -> 8` at
pixel `(518,166)` but depth `0x1.f6b882p-1 -> 0x1.f6b884p-1`: one float32 ULP.
The unmodified exact-depth assertion therefore correctly fails. Raw completed-job
log: `candidate-windows-job.log`. The initial candidate workflow also incorrectly
placed the Linux graphics command in macOS; that ordering-edit error is corrected
and platform-local CPU/graphics/build ordering is validated before redispatch.
No macOS PASS is claimed for the erroneous workflow run.

The official Mesa 26.2.3 source archive was inspected read-only, without installing
or changing a dependency: <https://archive.mesa3d.org/mesa-26.2.3.tar.xz>, SHA256
`1628058a8d2c0615975de5a15ab7bbb9638c50000b5bed9456ff423ea034a81f`.
`lp_surface.c:lp_blit` first tries `util_try_blit_via_copy_region`;
`u_surface.c:util_can_blit_via_copy_region` requires all components of the packed
format to be copied. Hazel requested only depth from D24S8, making that direct
copy ineligible. The existing transfer now copies the complete matching depth/
stencil attachment. Color/ID shaders, PBR/IBL math, source references and exact
assertions are unchanged. A new high-depth seed matching the recorded failure
checks exact packed depth **and stencil** bits; independent FP32 Neutral/IEC color
and integer-ID checks remain. Current Debug Renderer2D, M3 and M4 PASS after this
change. CI confirmation remains required.

2x fixture diagnosis also established two test-harness assumptions: the preview
can belong to a detached viewport, and a queued navigation activation is not an
accepted job. Tests now inspect the preview's own viewport, focus the intended
real widget for keyboard navigation, reveal actual property-row bounds and require
a newer tool-operation report. They retain actual widget dispatch, native producer
execution, document-history checks, failure/cancel acceptance-byte retention and
the original 30-second bounds. The full focused Debug 2x workflow now PASSes
(`workflow-nav-2x/results.json`). All unsuccessful diagnostic attempts remain.

A local autorelease pool now also encloses the added Metal OS/driver string query;
its temporary Foundation objects do not depend on GLFW having created an outer
pool. Native availability queries already have scoped pools. No global queue wait,
resource budget or shader-loading rejection was relaxed.


### Stable candidate review

`a6f22f23cca10acd29058dd7625a025b497c0647` is pushed on the requested branch.
Run [44297371674](https://github.com/cringlekaden/Hazel/actions/runs/44297371674)
is verified against that SHA; its jobs remain in progress at dispatch, not PASS.
The corrected workflow preserves platform-local CPU/graphics ordering.

Source review traced CLI/project selection through both application factories before
Application/window construction, explicit unsupported-request exceptions, transactional
project-save failure/conflict retention, restart guards, immutable cached depth-state
ownership through frame packets, completion-gated drain and cache/device release.
No additional correctness finding remains from that review. This is source review,
not execution on another GPU or an independent reviewer. Existing first-party
warnings (deprecated implicit copies, unused parameters and related baseline code)
and generated Xcode build-phase warnings remain; no blanket suppression is added.
The changed native implementation compiled without new first-party warning sites.

Profiling distinguishes incremental build orchestration (5.09 s, not clean compiler
time), focused test setup/execution, shader production/validation, CPU numerical
comparisons and process launches. The stable matrix records build, GPU capture-plus-
comparison suites, product and package wall times separately in `stable/results.json`;
product logs record process-spawn-to-ready observations with 20 ms polling, not
first presentation. Capture suites include readback and independent in-executable
oracles; their total must not be called isolated GPU pass time. Polling sleeps and
endurance frame counts are unchanged. No isolated clean-compiler or exclusive GPU
pass timing baseline is claimed. Resource feedback covers whole submissions; it
does not resolve individual world/Neutral/UI GPU passes.


### Stable native completion and measured timing update

`stable/results.json` records every gate PASS: Debug build 207.511 s, Renderer2D
92.292 s, M3 26.831 s, M4 60.325 s, remaining Debug 2x product/editor 10.320 s,
2x lifecycle 13.609 s and all eighteen explicit-backend/Automatic product launches
76.283 s; Release build 147.027 s, Renderer2D 12.181 s, M3 16.155 s, M4 20.128 s,
M5 1x/2x 44.932/46.526 s, lifecycle 5.539/6.983 s and eighteen product launches
22.751 s. The three complete source-free package sets PASS in 237.135 s. Earlier
CPU/full OpenGL/cache and focused Debug 2x workflow evidence remains applicable;
unchanged assertions are not rerun simply to change a default request constant.
This is native Apple M4 on macOS 27.3 (26C5035r), not physical Windows/Linux,
Intel or minimum macOS 27.0 hardware qualification.

The conditional native default-activation prerequisite is satisfied. The central
default is changed to Automatic, the temporary production qualification-hold UI
is removed, legacy missing keys now resolve the portable default, and save/restart
tests choose a different request independently of that default. Explicit serialized
OpenGL/Metal choices and CLI precedence remain unchanged. The maintained product/
package matrix now includes launches with **no backend CLI flag**. Focused activation
builds, policy/editor tests and new default launches are recorded separately in
`activation/`; previous explicit-backend and independent numerical gates remain.

An idle retained-capture CPU comparison repeat measures **17.701 s serial versus
8.983 s with two workers**, with identical complete result dictionaries. Initial
18.169/9.380 s measurements remain above; this later measurement supersedes them
for an isolated comparison claim. It excludes compilation and capture.

The final validation-disabled editor consumer repeat (`stable/editor-timing/`)
retains 749 warmed CPU and 740 positive-presentation GPU samples. Median/p95 ms:

| Scope | Before | Final after |
| --- | ---: | ---: |
| Packet preparation | 0.033583 / 0.067208 | 0.032667 / 0.059000 |
| Whole submit CPU | 4.179542 / 4.414208 | 4.142084 / 4.379417 |
| Drawable acquisition CPU | 4.018167 / 4.256167 | 3.988333 / 4.200833 |
| Render encode CPU | 0.017292 / 0.036541 | 0.016667 / 0.032750 |
| ImGui encode CPU | 0.045250 / 0.065167 | 0.046000 / 0.069916 |
| Application ImGui CPU (759 samples) | 1.249709 / 1.699167 | 1.289167 / 1.676459 |
| Whole-commit GPU | 2.785792 / 3.714333 | 1.603396 / 3.817583 |

The smaller preparation gain in the final repeat and varying whole-commit GPU
median demonstrate pacing/noise limits. The cache conclusively removes repeated
native state creation (one state in 900 consumer frames; bounded by six), but does
not establish a GPU/frame-rate win. CPU binding/encoding, queue submission and
retirement remain much smaller than acquisition and editor UI work. Existing
pipeline caching and upload/retirement pools are retained; no added global waits
or lower-quality attachments are justified. Completion counter scene/ImGui
intervals are available in the raw result, but may include overlapping later work
and are not exclusive pass costs. No speculative optimization follows from them.

Production and FP32-display diagnostic timing both PASS in 16.082 / 16.882 s.
All authored input hashes match the original benchmark, including the owner's
unchanged material draft; the input manifest records that exact scope. At 1080p,
production GL CPU-loop median/p95 is 6.778646/8.473583 ms and Metal is
2.827459/5.494916 ms. FP32-display GL is 7.830063/8.860416 ms and Metal is
2.995417/6.269875 ms. These common CPU-loop boundaries include native output and
event polling, not physical scanout; GL D24S8 versus Metal D32F and differing GPU
intervals still prevent a GPU backend-speedup claim. Each extent has 580 warmed
CPU samples; only positive-presentation Metal feedback contributes GPU samples.

Production RSS peak GL/Metal is 276368/235056 KiB, versus the initial overlapping
baseline 276096/235120 KiB; this is not a memory improvement claim. Metal tracked
peak frame/attachment/allocator bytes are 243827996/302383104/7745064, max two
in flight in this observation (initial 344622364/302383104/7745064, max three).
Those categories overlap and exclude driver-owned depth-state memory. The FP32
observation reaches three in flight and 403177472 attachment bytes; ordinary
Metal formats are unchanged. Lifetime/endurance tests retain completion-gated
retirement and zero tracked shutdown allocations. Neither reduced instantaneous
in-flight count nor RSS noise is attributed to caching.

### Activation continuation: console locked again

After the Automatic change, the focused Debug build PASSes (127.011 s), policy
PASSes (23.043 s), and actual editor save/restart regression PASSes (88.692 s).
The next default product matrix stops **before process launch** at the native
console gate: `CGSSessionScreenIsLocked=true` although global `IOConsoleLocked`
is false (`activation/default-products-Debug/Hazelnut-2d-default/console-before.json`).
This attempt is retained as BLOCKED environmental admission, not a renderer failure
or successful default-launch qualification. The owner was asked to unlock; Release
compilation/CPU checks continue independently. Default product launches, new-default
source-free launches and representative Project Settings captures remain INCOMPLETE
until the native console is available. Earlier stable renderer/explicit-Automatic
native/package PASS results are unaffected; no gate is bypassed.


### Correction candidate CI: Windows color gate fails

Run 44297371674 at `a6f22f23cca10acd29058dd7625a025b497c0647` now reports
Windows **FAIL** after a successful Debug build and all eight CPU/policy checks.
The graphics job fails the unchanged `GeometricNormalChecks` unlit HDR color
assertion: channel 0 actual `0.799994`, expected `0.8`, tolerance `1e-6`.
It does not report the former first-depth mismatch, but this is **not** proof that
the entire exact-depth loop passes: color and depth assertions are interleaved,
and the later independent `CanonicalColorChecks` packed-depth test is not reached.
All later Windows Release/package/game gates are skipped, not PASS.
Raw completed-job log: `candidate2-windows-job.log`.

The fragment input includes perspective-interpolated vertex color and a white
texture multiplied by the uniform base factor (`ForwardPBR.glsl`); the small error
is an observation, not an established compiler/driver cause. No tolerance increase,
reference update, forced compatibility path or shader-mathematics change is made.
Focused Windows/software-GL reproduction and an exact contract-preserving fix or
a genuine architectural decision are required before that gate can be qualified.
Linux and hosted macOS remain in progress at this poll. This run predates the
later Automatic-default constant; it cannot qualify the final containing commit.


### Pushed checkpoint scope and outstanding acceptance

The Automatic activation affects only the portable default and corresponding
legacy/save-restart expectations; affected Debug/Release builds and policy tests
PASS. Release focused policy takes 2.639 s versus baseline 6.06 s, with identical
selected coverage. Updated Release bundles for both products and all three scenes
are prepared and signature-verified (`default-packages-ready/`); **new-default
source-free execution is NOT RUN** while the GUI console is locked. The earlier
complete explicit OpenGL/Metal/Automatic source-free matrix remains PASS.

Project Settings screenshot capture is implemented using the actual production
widget and native editor capture path, but representative final captures are
**BLOCKED**, not synthesized or substituted with another UI. Release editor
save/restart and no-flag Debug/Release launches remain to complete after unlock.
No merge, public release or XeGTAO is performed. The next milestone requires
resolving the unchanged Windows color assertion, final-candidate platform CI and
the outstanding default/capture gates; additional physical GPU/platform and
Developer ID/notarization coverage remains explicitly outside local evidence.

Final qualification-helper review also ensures a no-flag packaged launch expects
OpenGL when the packaged project explicitly saves OpenGL. It must not confuse
Automatic's native preference with permission to override an authored request.
The explicit-Automatic package case remains separate and expects native Metal.


## Closure from 1a06c9c — 2026-11-20

Starting HEAD is exactly `1a06c9cdedb5d9d4749772d3cd97b52f8d5e21e6`, on the
requested branch, with the owner's material edit and untracked files preserved.
Completed run [44300482954](https://github.com/cringlekaden/Hazel/actions/runs/44300482954)
was inspected job by job. Hosted macOS PASSes its Debug/Release CPU/build subset.
Windows job 134788620928 FAILs unlit HDR color, actual 0.799994 versus 0.8 at 1e-6,
on Mesa 26.2.3 / LLVM 23.1.2 / GL4.6. Independently, Linux job 134788620775 FAILs
that named assertion with actual **0.799992**, on Mesa 25.2.8 / LLVM 20.1.2 /
GL4.5. Later Release/packages/games are skipped in both. These are two inspected
observations, not an inferred common root cause. Raw full logs and the exact
metadata are under `build/metal-default-evidence/closure/`.

The unlit production path is `u_BaseFactor * v_Color * texture(u_Base, v_UV)`.
MeshGeometry packs FP32 colors; missing source colors default to white. Submitted
unlit materials use linear factors (.8,.025,.015) or (.015,.3,.8), no texture
assignments, and Renderer3D binds its RGBA8 all-white fallback. SceneRenderer's
radiance target is RGBA32F, read as GL_FLOAT before Neutral/SDR presentation.
A float output format does not make preceding interpolation exact. The independent
FP64 reference converts the original FP32 factors to double; it does not include
raster interpolation error. Production shaders and frozen references remain intact.

A standalone GLSL 410 diagnostic checks every actual source vertex color is white,
queries actual FP32 attachment precision, removes the texture sampler and compares
smooth with flat color interpolation over the same mesh inputs. It is outside
production architecture and never supplies a replacement color oracle. Apple GL
reports zero error in both modes across 82,729 covered pixels. Focused dispatch
[numeric run 44814375306](https://github.com/cringlekaden/Hazel/actions/runs/44814375306)
at `b14cced478480f99da5921e2940c405ad3db6455` runs this separately on Windows and
Linux in ordinary software GL and forced GLSL/GL4.1 modes. Full acceptance remains
the default workflow dispatch; no full assertion is removed.

The [OpenGL 4.1 specification](https://registry.khronos.org/OpenGL/specs/gl/glspec41.core.pdf)
section 2.1.1 permits roughly 1e-5 individual fixed-function FP operation accuracy;
section 3.6.1 defines perspective interpolation. This is not itself a total error
bound or justification for a blanket tolerance. Mesa's inspected setup computes
FP32 planes for perspective-weighted varyings and reciprocal W; fragment evaluation
divides them. `lp_build_rcp` uses LLVM floating division, with its approximate RCPPS
branch disabled. An approximate-reciprocal-instruction explanation is therefore
not established. Platform probe measurements are required before a test revision.

### Native continuation and genuine startup findings

The desktop is unlocked. Initial no-flag Debug/Release product cases PASS; Release
editor save/restart regression PASSes. The first settings capture was not accepted
as a settings screenshot because the capture window belonged to another viewport.
The fixture now pins that actual widget to the main native capture viewport and
uses a readable window extent; [native captures](../backend-selection.md#native-project-settings-captures)
are inspected, not synthesized.

Five source-free default launches PASSed; PBR Nutella then revealed that 60 CPU
layer ticks could close after only four Metal submissions (one positive presentation
at the shutdown snapshot). The minimum-50 completed/presented qualification gate
correctly FAILed. The smoke path now waits for **60 correlated completed positive
presentations**, with a 30-second bound and a 1 ms yield while waiting; normal Run
is unchanged. Early user/window exit cannot produce smoke acceptance. No device,
queue, synchronization or production rendering architecture is changed. All six
Release no-flag product cases subsequently PASS with 60–68 positive presentations,
65–73 submissions, equal completions, no errors and zero tracked retirement.

The restart capture saves the OpenGL request through native SaveRenderingRequests,
asserts the active GPU remains Metal and exports only its disposable project after
all authoring assertions complete. An initial attempt saved before those assertions
and correctly failed a later validation; it is retained and not counted. The final
save/capture passes. Actual no-flag relaunch of the exported project selects OpenGL
in Hazelnut, but Nutella exposed a real stale-default call: RuntimeSession passed no
launch request to RestartReason, implicitly comparing the saved OpenGL request with
Automatic and refusing Play. It now passes ApplicationSpecification.ProjectBackend,
as the editor already does. The implicit API default is removed so every caller
must supply the launch snapshot. Focused policy tests keep explicit launch values.
The saved-project player relaunch is being requalified after this correction.


### Windows isolated result and proposed narrow precision contract

Windows focused job 136558775274 reproduces the failure in both software GL4.6 /
SPIR-V and forced GL4.1 / GLSL paths. The independent sampler-free smooth probe
has **93 / 82,729** covered pixels above 1e-6 and maximum absolute channel error
**1.9371509552e-5**. The flat probe has **zero** error in all covered pixels. This
isolates sufficient error in fixed-function smooth interpolation without material
UBO packing, texture sampling, PBR, tone mapping or output quantization. The actual
production check still FAILs at its original threshold in this diagnostic run; it
is not a passing acceptance run. Raw log: `closure/136558775274.log`.

Only the two unlit silhouette reference colors use the proposed allowance
`max(4e-5 * abs(expected channel), 4 * FLT_EPSILON)`. At .8 this is 3.2e-5,
1.65 times the independent Windows maximum (relative error 2.42144e-5). At .025
it remains 1e-6; at .015 it tightens to 6e-7. The floor is approximately 4.77e-7.
This fixture-specific contract measures flat coverage, not the accuracy of a
lighting approximation. It does not assert a universal bound for arbitrary
interpolation geometry, nor change shader arithmetic or reference expectations.
Linux's independent result is still required; full production readback maxima and
exact ID/depth assertions must pass on both platforms before acceptance.

New independent negative controls reject explicit binary16 radiance rounding,
zero/neighbor colors, 1/255 neighboring-surface blending, 0.1 percent radiance gain,
sRGB encoding, NaN and infinity. At .8, binary16 rounding changes the value by
1.95324e-4 (over six times the allowance); 0.1 percent gain is 25 times the relative
allowance. The probe's flat-control threshold remains 1e-6. All PBR/IBL, Neutral,
encoded output, exact signed-ID and depth assertions and frozen reference cases
remain unchanged. Production per-pixel maximum error and allowance utilization
are printed before assertions; a non-finite value still fails. Native Apple GL
passes this fixture and all 13 orbit views with zero unlit error.

The exported native restart project now launches in **both Hazelnut and Nutella**
without a backend flag and reports requested/active OpenGL before window creation,
then renders and closes normally (`closure/restart-launch-final/`). This completes
the actual save → retain Metal → close → new OpenGL device sequence; it does not
replace a live device. C++ interface/warning and public-doc audits are applied to
these scoped changes. Initial checked changed-native warning sites are empty;
baseline engine/vendor/generated warnings remain.

### Independent Linux result; final proposed bound

Linux focused job 136558775638 reproduces smooth-only error independently on its
Mesa 25.2.8 / LLVM 20.1.2 stack: **106 / 82,729** covered pixels above 1e-6,
maximum absolute error **3.5285949707e-5**. Its flat control is exact, and both
software GL4.5 and GL4.1/GLSL modes produce those same probe statistics. First
reported raw-probe failure is at (616,172), distinct from Windows (490,172).
The original production assertion independently reports 0.799992 at the old
threshold. Linux thus supplies its own measured interpolation diagnosis, not an
assumption based on the Windows failure. Raw log: `closure/136558775638.log`.

This larger result disproves the provisional 4e-5 relative allowance as a common
bound **before** full acceptance was launched. The final proposed fixture bound
is **8e-5 relative**, with the same 4*FLT_EPSILON floor: 6.4e-5 at .8, 2.4e-5 at
.3, 2e-6 at .025, 1.2e-6 at .015. The relative Linux maximum is 4.41074e-5, giving
1.81x headroom; Windows is 2.42144e-5. This is a common fixture contract, not a
platform-name exception. Binary16 .8 radiance remains over three times the bound;
0.1 percent gain is 12.5 times the relative bound. All listed negative controls
remain required.

In addition, a new **production SceneRenderer** control uses the same geometry,
materials, UBO uploads, fallback sampler and fragment shader with orthographic
constant clip W, removing only perspective interpolation division. It retains the
original **1e-6** color tolerance for both materials, with required nontrivial
coverage; Apple GL covers 9,818 red and 20,160 blue pixels. This ensures that
uniform, texture, output precision or color-encoding regressions cannot simply
hide inside a larger perspective-interpolation allowance. The existing perspective
silhouette/ID/depth/13-view tests remain in place, and no shader source, lighting
mathematics or frozen numeric reference is changed. Both production readback
maxima and exact invariants must still pass final Windows/Linux acceptance.


### Corrected candidate acceptance checkpoint

The scoped corrections are stable under focused native Debug/Release policy and
geometric checks. The final bound awaits the comprehensive cross-platform run;
passing the smaller native bound on Apple is not cross-platform acceptance. The
maintained product matrix now adds saved OpenGL/Metal no-flag requests on PBR,
catching the actual RuntimeSession launch-snapshot regression without multiplying
every scene/backend combination. `native_result` explicitly requires the minimum
correlated completed-positive presentations as well as complete retirement.

Next: one final maintained local Full tier on this corrected source, including
refreshed Debug/Release products and source-free bundles, and normal full C/C++ CI
against the exact corrected commit. Focused numeric dispatches are deliberately
not full acceptance. No previously unexecuted job or physical GPU is counted.

### Warning-only test follow-up

Completed MSVC diagnostic logs also identify new probe warning sites: C4459 for a
local `target` shadowing the older test's global target, and C4996 for direct
`getenv`. These are fixed at their cause: the probe uses `probeTarget`, and new
environment switches/capture export use Hazel's existing FileSystem environment
helper. The two pre-existing shadowing lambda parameter names in the touched
geometric fixture are also clarified. No suppression is added. Renderer/application
source, shader math, all assertion values and case selection are unchanged by this
follow-up. Existing local Full-tier results remain evidence for that identical
production code; touched test consumers receive targeted rebuild/checks. The
in-progress 4c0a42e CI is superseded rather than duplicated by a full dispatch on
the warning-corrected test source.
