# Metal 4 M6 — production readiness qualification

Baseline: `819ac353879ea9e8f048054b580e5ae614085a6d`,
`feature/3d-foundations`. M6 qualification is **INCOMPLETE** until every required available
gate below has executed successfully. OpenGL remains the default. No merge,
release, new rendering feature or next stage is included.

## Acceptance gates

These gates follow the requested M6 acceptance and retain M1–M5 invariants:

- Pinned recursive source acquisition, checkout-owned Premake, documented setup,
  independent build storage, Debug/Release Hazelnut and Nutella builds.
- Actual Debug/Release products: OpenGL and Metal, authored 2D, direct 3D and
  prepared photographic PBR/IBL; private storage, normal close and teardown.
- Release packages for both applications and those projects: precompiled engine
  and ImGui shaders, native/managed/resource/project/license closure, checksums,
  signatures, spaces/Unicode relocation, unrelated cwd, source/SDK/Homebrew and
  developer shader-compiler access denied. Pixel and managed probes remain
  separate from pristine product launches.
- Existing native OpenGL suites, unchanged M3/M4 numerical/reference suites,
  shader/cache corruption and source-free gates, M5 compute/editor/lifecycle and
  failure cleanup, Debug/Release and native 1×/2× where applicable.
- Existing Windows/Linux/hosted macOS Actions workflow on the feature branch,
  with exact run SHA and logs; software rendering and hosted compilation do not
  establish native Apple GPU or physical Intel HD 4000 execution.
- Matched runtime benchmark: identical saved scene/camera/assets/settings and
  physical extents; CPU/GPU median/p95, memory, startup and pacing, with explicit
  boundaries. Incompatible intervals cannot support a speedup claim.
- Production ownership/synchronization/cache/warning review and README-linked
  onboarding, capability, packaging, screenshot, roadmap and CI audit.

## Reproduction

Use an unlocked Apple Silicon/macOS 27+ console and full Xcode. Install the
[documented prerequisites](../getting-started.md), then use the feature branch:

```sh
git clone --branch feature/3d-foundations --recurse-submodules https://github.com/cringlekaden/Hazel.git
cd Hazel
python3 scripts/hazel.py setup --config Debug --tests
python3 scripts/hazel.py build --config Release --tests
python3 scripts/macos/metal_m6.py --config Debug --only products --output build/m6-products-debug-fresh
python3 scripts/macos/metal_m6.py --config Release --only products --output build/m6-products-release-fresh
python3 scripts/macos/metal_m6.py --config Release --only packages --output build/m6-packages-fresh
```

The matrix snapshots committed projects into disposable directories; it does not
edit the owner's examples or settings. `test-packages` also exercises both
backends on macOS. Bundles remain locally ad-hoc signed: Developer ID,
notarization and fresh-download Gatekeeper are separate, unqualified deployment
modes. Source-free application packaging is distinct from the older shader-only
relocation gate. Native system Metal pipeline compilation remains necessary;
Xcode's developer shader compilers are not runtime dependencies.

CI preserves master pushes, PRs and manual dispatch, and the master badge:

```sh
gh workflow run c-cpp.yml --ref feature/3d-foundations
gh run list --workflow c-cpp.yml --branch feature/3d-foundations
gh run view RUN_ID --json headSha,jobs,url
gh run view RUN_ID --log-failed
```

Never treat an earlier SHA or one successful job as whole-run qualification.
Machine-readable status and local raw-log paths are in
[metal-m6-results.json](metal-m6-results.json). Results below will be reconciled
against executed evidence at the checkpoint.

## Implementation and review checkpoint

The renderer architecture is unchanged: one SceneRenderer, the existing
Renderer2D, one Metal 4 queue and three completion-gated slots. No shader
mathematics, PBR/IBL reference data, backend default or vendor source changed.

- ImGui's unchanged MIT MSL moved from a runtime string to an offline-produced
  library. The producer checks its source/toolchain and library hashes, recovers
  corrupt metadata/libraries and preserves deterministic bytes. Runtime uses
  `newLibraryWithURL:error:`; the first draft's deprecated file API was corrected
  before the final native builds. No warning suppression was added.
- macOS bundles now obtain engine/ImGui artifacts from the checked producer,
  replace any copied developer shader directory and include the native ImGui
  library without its authoring source. Both apps build the offline producer
  through Premake even without regression targets. The qualification aggregate
  also includes all maintained Metal smoke targets, preventing stale binaries
  from standing in for a fresh `build --tests`.
- CLI Xcode intermediates are checkout-local. This host's global custom
  DerivedData setting originally sent disposable and canonical builds to the
  same directory. Those overlapping attempts were stopped and superseded by
  independently isolated builds. The disposable checkout is used only for the
  explicitly authorized onboarding test; it does not replace the canonical one.
- Pristine bundle tests run both backends. Metal tests explicitly deny shader
  sources and developer compiler execution, clear inherited resource/loader
  overrides, and check native completion, positive presentation and retirement.
  Pixel/readback and managed probes run afterwards on disposable copies.
- Existing game tests now enumerate the same five projects as CI packaging.
  Broad discovery had attempted script builds for scriptless rendering studies
  and expected archives CI never produced. All five authored-game tests remain.
- Showcase assets have the same exact-byte Git attributes as rendering
  references. Their recorded hashes are unchanged; CRLF conversion reproduces
  the prior Windows byte-identity failure.
- Actual baseline branch CI exposed a second, earlier failure on both Windows
  and Linux: the Metal-only ImGui `.mm` exclusion was under a file filter.
  Premake removal now uses the project-level non-macOS system filter. Disposable
  MSVC generation reproduces inclusion before the fix and exclusion afterwards.
  Linux target generation on the Mac is BLOCKED by missing GTK pkg-config
  metadata; no fake prerequisites or error bypass were supplied.

Static review traced successful commit → queue event → feedback → slot/resource
retirement, unavailable/busy drawable admission and the editor's pre-commit
abort guard. Normal frame admission does not wait globally; shutdown drains
before layer/surface destruction. Native drawable synchronization and render/
compute visibility barriers are retained. No measured evidence justifies
removing them. Native pipeline caches remain owned by existing shaders/UI
formats, not a new renderer or scheduler. Resource ceilings remain explicit:
1…4096 per target axis, at most 8,388,608 pixels, single sample, and the existing
bounded GPU/allocator/ImGui budgets. Larger targets and additional effects are
unsupported; this checkpoint does not silently lift those limits.

### Qualification failures retained

The evidence root retains unsuccessful attempts; none are counted as a PASS:

- `products-debug-final`: one Metal submission and zero positive presentations.
  This attempt lacks a console-state snapshot; its environmental cause is not
  independently established. Later product runs record the console explicitly.
- `packages-visible`: an overbroad new isolation rule denied the GLSL resources
  shipped for OpenGL. Metal's existing optional source/artifact identity check
  correctly rejected the unreadable file. The harness now allows these shipped
  resources while denying the original source SDK, external Metal authoring
  sources and developer compiler execution. No runtime validation was relaxed.
- `packages-dual-backend`: pristine launches and the missing-library rejection
  ran, then the expanded renderer probe rejected missing frozen test projects.
  The maintained package harness now supplies the same committed reference and
  showcase projects as the main suite, strictly after pristine product checks.
- `packages-final-complete`: a Metal player hit the bound-frame viewport guard
  while a concurrent lifecycle test changed the display mode. An exact isolated
  product repeat passed 60 submissions/completions with 59 positive
  presentations; two debugger runs also passed. No viewport restriction was
  removed on this evidence. Final package and display-mode tests run serially.
- Initial M5 workflow repeats lacked `ImGui.metallib` in their private Resources.
  The ordinary source suite now copies the staged Metal artifacts when its
  explicit Metal-editor mode is selected. This exposed a real production abort
  defect: after an ImGui encoding exception the unsubmitted command buffer
  remained open. `MetalEditor.mm` now ends that render encoder and command buffer
  before the existing pending-resource guard unwinds. A new maintained native
  recovery case injects an unsubmitted callback exception and then renders again
  on the same application/device/command buffer; the original abort/shutdown
  checks remain intact.
- Initial matched timing runs overlapped other native activity and are excluded
  from final performance observations. Final timing runs are serialized after
  source/package/native correctness work.

The added recovery fixture changes a private macOS test helper signature, not a
portable engine interface. Its direct consumer is `MetalM1Smoke.cpp`; Debug and
Release builds are the header/interface checks. The production correction uses
existing owners, queue and pending guard, with no new renderer or scheduler.

## Executed local outcome

All available local Apple M4 gates below pass. **M6 remains INCOMPLETE** because
baseline CI fails on Windows/Linux and candidate CI is still in progress.
The implementation candidate is `35e4f4116cba7bc57a12e4451bdf293acc2c5cf3`;
its source-scope digest matches the locally qualified implementation. This
evidence update is documentation-only. No merge, release or default change is
authorized.

| Gate | Outcome | Evidence under `build/metal-m6-evidence/` |
| --- | --- | --- |
| Independent recursive clone, pinned inputs, Premake Debug/Release | PASS, explicit M6 source overlay | `clean-clone.log`, `clean-submodules.txt`, `clean-recovery-build-{debug,release}.log` |
| Clean-source applications with canonical checkout denied | PASS, 24 combinations | `clean-source-pipe-gates.json` |
| Canonical Debug/Release builds and actual source apps | PASS, 24 combinations | `recovery-build-*.log`, `final-products-{debug,release}/results.json` |
| Native Apple OpenGL suite | PASS, 18 executables per configuration | `opengl-{debug,release}.log` |
| M3 Renderer2D and independent line coverage | PASS, Debug/Release | `m3-{debug,release}/results.json`, `lines-results.json` |
| M4 unchanged 3D/PBR/IBL captures | PASS, 135 per configuration; zero ID mismatches; maximum depth error 1.7881393432617188e-7 | `m4-{debug,release}/results.json` |
| Native 3D lifecycle and startup/failure paths | PASS, Debug/Release, 1×/2× | `m1-3d-{debug,release}-{1x,2x}/summary.txt` |
| M5 workflows, compute, pending shutdown and abort/recovery | PASS, Debug/Release; four detached hosts at 1×/2× | `final-m5-*/results.json` |
| Shader/cache/source-free regressions | PASS, 17 rejection cases per configuration | `cache-{debug,release}/results.json` |
| Full relocated Release application bundles | PASS, six bundles / twelve pristine backend launches | `final-packages/results.json`, `package-inventory.json` |
| Independent-root shader reproducibility | PASS, all eleven library bytes match in each configuration | `independent-shader-reproducibility.json` |
| New native warnings / public docs audit | Zero in changed native files / zero local link findings | `final-warning-review.json`, `public-docs-final.log` |

Final M5 Debug/Release workflows complete 1,543/1,128 submissions with
1,416/1,013 positive presentations at the shutdown snapshot. Same-device abort
recovery completes 880/871 submissions with 869/858 positive presentations.
All final native retirement gates report zero errors, pending bytes, attachment
generations and ImGui bytes. These are tracked ownership counters, not a claim
that driver memory is measurable or every hardware condition is qualified.

The isolated onboarding runner initially directed its error log into the very
checkout it denied. A focused comparison (`clean-clang-descriptor-proof.json`)
reproduces Clang exit 1 with that descriptor and exit 0 with a pipe. Capturing
through pipes and writing logs in the parent preserves the same source denial
and allows both normal script invocations to pass. The clean clone owns its
inputs and intermediate storage; it does not consume canonical build products.

Package products run with no Resources/Mono/loader overrides, from unrelated
spaces/Unicode paths. All eleven offline shader-file hashes match `build.json`;
ImGui authoring MSL and the producer executable are absent, and engine, ImGui,
Metal-cpp, Mono and project notices are included. Pristine products are verified
before test-only probes. Pixel/picking probes use OpenGL; native Metal numerical
parity is the separate retained M3/M4 gate, using matching artifact bytes. These
local ad-hoc checks do not establish public signing or physical UI interaction.

The OpenGL, M3/M4 and 3D lifecycle runs precede the later editor-only abort
cleanup. Their exercised rendering paths and GLSL bytes are unchanged. Final
editor, source-product and package matrices include that cleanup.

## Matched timing observations

Final timing runs occur after all other Hazel native work has exited, with GPU
validation disabled. Both backends use the photographic project's saved camera,
assets and settings, four texture slots, VSync-off request and one sample.
Each physical extent runs 720 frames, excluding 120 warmup and 20 tail frames;
CPU distributions contain 580 samples. GPU samples require valid intervals and,
for Metal, positively presented submissions. Raw commands, input hashes, console
state, per-frame traces and RSS samples are retained in
`final-benchmark-{debug,release}/`.

Milliseconds below are median / p95. Scene CPU excludes native Metal submission;
its acquisition/encoding/commit CPU is reported separately. Loop interval is a
common CPU tick-to-tick boundary including window update and polling, not physical
screen cadence. The GPU column is GL elapsed queries versus whole Metal commit
feedback, which may include drawable synchronization. Production HDR formats
remain RGBA16F on OpenGL and RGBA32F on Metal. **These scopes and formats do not
establish a backend speedup.** No exclusive/additive pass-cost claim is made.

| Config / backend / extent | Scene CPU | Native submit CPU | CPU loop interval | GPU observation | GPU samples |
| --- | ---: | ---: | ---: | ---: | ---: |
| Debug / opengl / 1280x720 | 2.811 / 3.686 | included in GL calls | 5.549 / 6.065 | 3.178 / 3.375 | 580 |
| Debug / opengl / 1920x1080 | 5.004 / 5.329 | included in GL calls | 6.908 / 8.582 | 4.779 / 5.014 | 580 |
| Debug / metal / 1280x720 | 1.024 / 1.117 | 0.785 / 4.111 | 1.942 / 5.253 | 0.957 / 0.995 | 557 |
| Debug / metal / 1920x1080 | 1.048 / 1.158 | 1.811 / 4.590 | 3.079 / 5.743 | 1.936 / 2.054 | 567 |
| Release / opengl / 1280x720 | 2.288 / 3.166 | included in GL calls | 5.456 / 6.150 | 3.193 / 3.437 | 580 |
| Release / opengl / 1920x1080 | 4.489 / 4.758 | included in GL calls | 6.561 / 8.358 | 4.731 / 5.025 | 580 |
| Release / metal / 1280x720 | 0.071 / 0.112 | 1.574 / 4.789 | 1.680 / 4.887 | 0.965 / 1.019 | 559 |
| Release / metal / 1920x1080 | 0.187 / 0.349 | 2.480 / 4.634 | 2.786 / 4.974 | 1.954 / 2.079 | 562 |

Release process RSS peaks are 275,392 KiB OpenGL and 234,736 KiB Metal. Metal's
sampled tracked peaks are 243,827,996 frame/resource bytes, 302,383,104 attachment
bytes and 7,745,064 allocator bytes; these categories overlap and must not be
summed. OpenGL driver GPU allocation is unavailable from the maintained API.
Release startup observations are 940.163 ms OpenGL and 685.615 ms Metal, measured
from application construction through scene readiness, before Run or first
presentation. Each is one observation with fresh private application storage;
OS/driver caches are uncontrolled. They are not a cold-start or speedup guarantee.
Debug measurements, positive Metal presentation pacing and all CPU distributions
remain in the machine-readable evidence.

## CI, deployment limits and verdict

[Feature-branch run 44168056768](https://github.com/cringlekaden/Hazel/actions/runs/44168056768)
tested exactly `819ac353879ea9e8f048054b580e5ae614085a6d`. Hosted macOS
Debug/Release compilation and the CPU/service/managed subset PASS. Windows and
Linux Debug compilation FAIL because the Metal-only ImGui `.mm` was selected;
subsequent tests and package jobs did not execute. The local Premake correction
has MSVC project-generation proof, not a successful Windows/Linux compiler run.
Master's workflow bytes, triggers and badge are unchanged.

Candidate `35e4f4116cba7bc57a12e4451bdf293acc2c5cf3` was committed and pushed
normally to `feature/3d-foundations`. [Candidate run 44212291235](https://github.com/cringlekaden/Hazel/actions/runs/44212291235)
was started through `workflow_dispatch`; its `headSha`, branch and event were
verified against that candidate. All three jobs were IN PROGRESS at handoff.
The owner explicitly requested reporting after push/dispatch without waiting
for CI completion. No hosted candidate success is claimed, and the baseline run
is not substituted for candidate evidence. Physical Intel HD 4000,
other Apple Silicon GPUs, actual minimum macOS 27.0 execution, Developer ID,
notarization and fresh-download Gatekeeper remain NOT RUN. Native M4, hosted
compilation, software GL and physical Intel are separate evidence categories.

**M6 INCOMPLETE — candidate CI in progress, not awaited at owner request.** All
available local gates passed. Retain OpenGL as the macOS default while the outstanding CI
and deployment/hardware gates remain. Metal remains an explicitly selected,
locally qualified path on the tested M4. No default change is applied. Existing
single-sample/target/resource limits and the accepted frozen split-sum IBL
approximation remain; no new rendering features are implemented.


### Subsequent default-backend checkpoint

The [following qualification record](metal-default-qualification.md) resumes from
actual HEAD `93d39bd6d71b090a3a931f1a457a9df27347c91c`. A subsequent poll of
candidate run `44212291235` reports hosted macOS PASS, Windows FAIL in the exact
unlit picking/depth regression on llvmpipe, and Linux FAIL at the same exact invariant. The new
record retains the completed job log and diagnostic follow-up. The M6 verdict
therefore remains INCOMPLETE; the historical local evidence above is preserved.
