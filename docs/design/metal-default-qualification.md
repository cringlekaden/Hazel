# Metal default-backend qualification and production cleanup

Starting tree: `93d39bd6d71b090a3a931f1a457a9df27347c91c`,
`feature/3d-foundations`. This checkpoint follows [M6](metal-m6.md).
Status: **INCOMPLETE — implementation and native qualification in progress**.
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
Its final isolated run is NOT RUN while the console is locked.


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


## Default activation hold

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
