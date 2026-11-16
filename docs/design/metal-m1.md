# Native Metal 4: M1 device, presentation and lifetime proof

**M1 PASS:** native Metal 4 qualification passes in Debug and Release at 1× and 2×;
all 18 macOS OpenGL regressions pass in each configuration.

Scope: M1 only on `feature/3d-foundations`. OpenGL remains the default for
Hazelnut and Nutella. No shaders, scene/material/asset formats, rendering
algorithms, IBL research, or Stage 5 work are changed by this assignment.

## Decision and feasibility

The owner superseded the original classic-Metal decision during M1 and selected
Metal 4. The maintained macOS target is already native arm64/macOS 27.0+.
Metal-cpp is Apple's official interface, pinned as the `Hazel/vendor/metal-cpp`
submodule at `8cdde68ae36a636c84cbb48ebcbc8f3b60251ea0` (macOS/iOS 27.0.1).
Its Apache 2.0 notice remains in the submodule; existing packaging notice
collection includes it. Only the macOS Premake integration adds its include path
and Metal/QuartzCore/Foundation framework links.

The installed Xcode 27.3 / macOS 27.3 SDK declares the required APIs. A syntax-only
probe against that SDK verified Metal-cpp's queue/allocator/command-buffer,
render-pass, transfer, drawable wait/signal, and shared-event calls before native
execution was implemented. Native capability checks require
`MTL::GPUFamilyMetal4`; queue creation failure is an error. There is no classic
Metal submission implementation or automatic fallback inside this backend.

References consulted:

- [Apple Metal-cpp integration](https://developer.apple.com/metal/cpp/)
- [Apple Metal 4 core API](https://developer.apple.com/documentation/metal/understanding-the-metal-4-core-api)
- [Apple Metal 4 triangle and allocator lifetime example](https://developer.apple.com/documentation/metal/drawing-a-triangle-with-metal-4)
- Installed `Metal.framework/Headers/MTL4CommandQueue.h`,
  `MTL4CommandBuffer.h`, `MTL4CommandAllocator.h`, `MTL4CommandEncoder.h`,
  `MTL4ComputeCommandEncoder.h`, `MTL4CommitFeedback.h`, `MTLResidencySet.h`.

The installed queue header explicitly requires `waitForDrawable:` before commits
using a drawable and `signalDrawable:` after those commits, before `present`.
Its `signalEvent:value:` orders the signal after all prior GPU work. Allocators
must not reset until their recorded GPU work has completed. Metal 4 command
buffers do not supply classic command-buffer resource-retention/hazard-tracking
semantics; the engine owns referenced resources and explicit synchronization.

## Source ownership

| Source | Responsibility |
|---|---|
| `Application.{h,cpp}` | Explicit API and initialization mode, configure before window creation, normal loop and teardown. Defaults remain OpenGL plus drawing services. |
| `RendererAPI`, `RenderCommand`, `Renderer` | Deferred backend creation, immutable selection, separation of presentation-only initialization from drawing initialization, cleanup of partial drawing startup. |
| `GraphicsContext.cpp` | Context selection and backend-specific GLFW hints; reset stale hints between lifetimes. |
| `MacOSWindow.{h,cpp}` | Existing GLFW window/event owner, constructor-failure cleanup, context-first destruction and backend-specific VSync. |
| `Platform/macOS/Metal/MetalRendererAPI` | M1 backend adapter. Unimplemented geometry/state/text operations fail explicitly. No OpenGL delegation. |
| `MetalDevice` | One device and native Metal 4 queue, reusable command buffer, three frame slots, resource residency, synchronization, verification, capture and drain. |
| `MetalContext` | Surface owner borrowing device state; adds/removes the layer's residency set, updates physical extent, delegates native submission. |
| `MetalSurface.mm` | Minimal Cocoa bridge. Attach retained CAMetalLayer to GLFW's existing content view; query native scale and GLFW sizes; detach before native-window destruction. No second event loop. |
| `MetalImplementation.cpp` | Apple's private-implementation macros, exactly once. |
| `MetalProof` | Bounded observations and explicit diagnostic controls. Observer owns no GPU resources and may outlive Application. |
| `tests/macos/MetalM1Smoke.cpp` | Real Application/GLFW lifecycle and acceptance checks. No independent Metal device/window setup. |
| `tests/macos/MetalM1Environment.mm` | Read-only process/session, Cocoa ownership, GLFW monitor, framebuffer and native-scale evidence. |
| `scripts/macos/metal_m1.py` | Premake invocation, console preflight, private validation output, independent frame correlation and reversible native 2× qualification. |
| `scripts/dependencies/macos.lua` | macOS-only sources' dependencies, native test target and framework links. Generated Xcode files remain disposable. |

`PresentationOnly` skips engine drawing-service initialization. It requires
ImGui disabled. Selecting Metal with the normal drawing mode fails before
Renderer2D or ImGui resource factories execute. Production applications do not
expose a Metal command-line selection in M1.

Backend selection stays locked while its context exists. The release operation
is private to Application; renderer shutdown alone cannot unlock selection. Application shutdown
stops layers/runtimes and releases project assets, drains rendering, destroys the
context/window, then releases the backend and resets selection. Contexts retain
the internal device state during teardown. Startup failures unwind through the
same ownership path, including failure after attaching the native layer.

## Minimal native execution

There is no shared render-pass abstraction in M1. Presentation creates one
`MTL4::RenderPassDescriptor` with a clear load action, store action, and the
acquired drawable texture, encodes it with `MTL4::RenderCommandEncoder`, and ends
encoding. It compiles no shader or pipeline.

Each of three slots owns:

- One `MTL4::CommandAllocator` and `MTL4::ArgumentTable`.
- Optional diagnostic upload/readback buffers, private buffer and private texture.
- The sequence number of its last submission and a small presentation receipt.

The reusable `MTL4::CommandBuffer` records into the selected allocator. A pending
slot is reusable only after the shared event reaches its submission number.
Normal frames poll and skip a busy slot; they do not globally wait for the GPU.
Shutdown alone drains outstanding work. A timeout never permits allocator or
storage reuse. A ten-second shutdown completion failure aborts with a diagnostic
instead of releasing potentially in-use resources; device-loss recovery is not
implemented in M1.

The native submission order is:

1. Poll completed sequence numbers and verify readback before reuse.
2. Check physical extent/visibility, reserve an available slot and acquire drawable.
3. Reset that completed slot's allocator; begin the reusable command buffer.
4. Encode optional diagnostic transfers and the drawable clear pass; end buffer.
5. `MTL4::CommandQueue::wait(drawable)`.
6. `commit(buffers, count, options)` with native commit feedback.
7. `signalDrawable(drawable)` then `drawable->present()`.
8. `signalEvent(completionEvent, frameNumber)`.

Residency is explicit: one owned-allocation `MTLResidencySet` plus the
CAMetalLayer's residency set are attached to the queue. Slots retain their native
resources regardless of commit completion callbacks. Sets and resources are
removed/released only after draining. Argument tables use separate Metal 4
namespaces: buffer 0 upload, buffer 1 readback, texture 0 diagnostic. The proof
populates and binds those tables; shader access/layout qualification is M2.

The transfer encoder is `MTL4::ComputeCommandEncoder`; its native blit operations
require no compute shader or pipeline. For each of two versions in a frame:

- Encode upload-slice to private buffer and upload-image to private texture.
- Encode blit-to-blit device-visibility barrier.
- Copy both private results to separate readback slices.
- Encode another barrier before the next version overwrites the private storage.

The CPU writes the second upload slice after encoding the first version's
consumption. It never overwrites the first slice. Both buffer versions and every
texel carry deterministic frame/version/index patterns and are verified after
the GPU completion event. This distinguishes within-frame version safety from
merely rotating three frame buffers.

The proof bounds diagnostic native allocation bytes to 1 MiB and allocator bytes
to 16 MiB; actual `allocatedSize` values and peaks are reported. There are always
three allocators/tables and twelve diagnostic resource allocations. Counters
exclude driver-internal memory and the system-owned drawable pool. This is
lifetime/accounting evidence, not a whole-engine memory or performance claim.

GPU completion, commit feedback and presentation are separate observations.
Only `MTLDrawable::addPresentedHandler` with positive `presentedTime` increments
`Presented`. Diagnostic output records frame IDs and actual timestamp values;
the Python runner independently joins positive timestamps with verified GPU
completion records and requires at least 300 matching frame IDs. `CompletedPresented` additionally requires the same receipt to have
passed GPU completion/readback processing. Submission and drawable acquisition
never count as presentation.

## Window and failure coverage

The smoke uses Application's ordinary loop and GLFW callbacks. It checks:

- No current GL context, GLFW `NO_API`, and no Glad load on Metal startup.
- Immutable backend selection and explicit failure of unfinished drawing services.
- Failure after device creation, queue creation and layer attachment, each followed
  by successful Application construction/destruction in the same process.
- Physical drawable extent independent from logical window dimensions.
- A one-frame injected zero extent and a post-reservation unavailable drawable;
  subsequent frames recover without consuming a slot.
- An injected double-sized physical CAMetalLayer drawable without changing logical
  dimensions. Separately, native 2× display mode qualification checks actual
  GLFW/Cocoa scale and framebuffer sizes. macOS may constrain initial logical
  window height to its available desktop; the test uses the actual logical size.
- GLFW iconify/restore, resize to 960x540, actual resize event delivery and VSync
  request off/on through the native layer.
- At least 300 frames that both complete and have positive presentation timestamps.
- A queue wait on a separate diagnostic shared event, then ordinary close with three
  submitted frames still pending. A test-owned thread releases the gate after
  250 ms, proving shutdown drains before freeing resources.
- Zero remaining engine-owned allocation/allocator counts, layer detachment and
  device-owner release; default OpenGL initialization in the same process afterward.

A locked console prevents presentation qualification even if GPU transfers
complete. A visible first frame is submitted before relying on native occlusion
state; subsequent fully occluded/minimized windows are skipped. The first native
run encountered a locked console and correctly did not claim presentation.

## Reproduction and evidence

Run from the canonical checkout. Commands generate/build with Premake through
Hazel's existing launcher, never handwritten/generated Xcode modifications:

```sh
python3 scripts/macos/metal_m1.py --config Debug --build --capture --output build/metal-m1-evidence/debug
python3 scripts/macos/metal_m1.py --config Release --build --capture --output build/metal-m1-evidence/release
```

Use a fresh private output directory for another capture. `--build-only` builds
without requiring an unlocked desktop. The runner supplies the prepared Mono
library path, `MTL_DEBUG_LAYER=1`, error mode `assert`, warning mode `nslog`, and
`MTL_CAPTURE_ENABLED=1`. The executable has a bounded native-run deadline, and the
runner additionally has a process timeout. `--hidpi` selects an advertised native
1280x720-point / 2560x1440-pixel display mode, verifies the process observes scale
2, and restores the exact original mode in `finally`, including test failures.
`console-before.json`, `process-session.json`, `environment.json`, and
`display-mode.json` preserve environment evidence. `frame-proof.json` preserves
each qualifying frame ID and positive timestamp plus scoped timing statistics.
Logs, summary and `.gputrace` remain
under ignored build output.

Measured acceptance results are recorded below only after execution.

## M2–M5 boundaries and ImGui

No shared shader compiler extraction or GLSL-to-MSL implementation occurs here.
The first M2 checkpoint must qualify all existing raster shaders through the
pinned shaderc/SPIRV-Cross MSL emitter, then Apple's Metal 4 pipeline compiler,
with explicit argument-table bindings, reflected CPU layouts, depth/orientation
conventions and integer outputs. Shader compilation success alone is insufficient.
OpenGL's existing GLSL 410 path and shader assets remain unchanged.

Metal 4 changes the future pipeline setup to `MTL4Compiler` and Metal 4 pipeline
function descriptors. Native argument tables replace classic encoder resource
bindings. Every future render/compute dependency needs explicit stage ordering
and visibility; it must not assume queue commit order implies hazard tracking.
M3/M4 extend the same device, slots and authoritative scene submission path.

The official Dear ImGui docking branch now contains `imgui_impl_metal4.mm/.h`.
The inspected docking ref was `cf53467ab0db7a783f04b32dd9160e6aa5b720ef` on
2026-11-15. Its API accepts the application's Metal 4 queue, frames-in-flight count
and explicit frame-slot index. It supports Metal-cpp and detached viewports.
Hazel is pinned to ImGui 1.89.4 WIP (`e25e4526...`), without that backend or the
new dynamic texture/ImTextureRef interfaces it uses. Copying the new backend into
the old core is not a supported drop-in integration.

Before M5, separately review a pinned core+docking+GLFW/OpenGL backend update
against Hazel's ImGui internals, ImGuizmo and editor APIs, or scope a maintained
backport. Review detached-window allocator/residency/event ownership and producer
texture synchronization against the engine's primary queue. The upstream header
and implementation also describe texture identifiers differently; inspect and
freeze the actual pinned implementation's convention during integration. No
ImGui pin or backend source is changed in M1, and no classic Metal dependency is
introduced to accommodate the old backend.

- [Official Metal 4 backend header](https://github.com/ocornut/imgui/blob/cf53467ab0db7a783f04b32dd9160e6aa5b720ef/backends/imgui_impl_metal4.h)
- [Official Metal 4 docking implementation](https://github.com/ocornut/imgui/blob/cf53467ab0db7a783f04b32dd9160e6aa5b720ef/backends/imgui_impl_metal4.mm)

## Qualification results, 2026-11-15

Host: M4 Mac mini, 16 GB; macOS 27.3 (26C5035r), Xcode 27.3
(27C5020n), SDK 27.3. Checkout and live upstream both began at
`099545f45432a8ebf718986f6967dfd054fe43ca`, branch
`feature/3d-foundations`. Existing uncommitted M1 work and all earlier artifacts
were preserved. Unrelated `.DS_Store` files are excluded from the checkpoint.
Structured results are in [metal-m1-results.json](metal-m1-results.json).

### Console state and before/after comparison

The original locked attempt had `IOConsoleLocked=Yes` and
`CGSSessionScreenIsLocked=Yes`: each configuration completed one GPU submission
and verified both transfer versions, but its only presentation callback had time
zero. Neither configuration qualified a single presented frame. The old traces,
logs and `partial-metal-results.json` remain under
`build/metal-m1-evidence/`; none were discarded.

On resume, before any source changes, the **existing Debug executable** passed:
339 submitted/completed/verified, 324 positive presented/completed-presented,
three slots, 336 reuses, three pending at shutdown, zero errors and zero remaining
owned resource counters. The unchanged Release executable then passed with
338 completed/verified, 325 positive presented/completed-presented and 335 reuses.
This establishes that unlocking resolved the previous one-frame/zero-timestamp
presentation blocker without a renderer fix.

The unlocked console reports UID 501, login done and on-console in security/audit
session 100002. `IOConsoleLocked=false`; the CG session's screen-lock key is absent
rather than an explicit false value. Cocoa owns the GLFW window in the smoke
process's NSApplication. Both GLFW and Cocoa enumerate VG272U V3 at 180 Hz.
Native 1×: monitor 2560x1440 points/pixels; window 1280x720 points/pixels.

The console locked again during instrumentation rebuilding. That run was rejected
before its first submission by the process's CG session check, despite the outer
IOKit boolean reporting false. Qualification stopped until the owner unlocked
again. A temporary `caffeinate -d -i -t 1800` assertion then prevented idle sleep;
it does not authorize or bypass a locked console. Both runner preflight and the
process's independent CG session gate remain strict.

A new WindowServer-list ownership assertion initially rejected a newly created
window before rendering. The diagnostic now verifies ownership using
`NSApplication.windows`, retaining WindowServer listing as information only.
The latter is false on this host at the initial query, so it is not used as a
presentation or ownership boundary. No permissions or privacy settings changed.

### Final native Metal 4 runs

Every row enables API validation (error mode `assert`, warning mode `nslog`) and
GPU capture. Presentation totals below are the intersection of independently
logged positive native timestamps and completed, verified frame IDs.

| Configuration / native scale | Completed / verified | Positive presented and completed | Slot reuses | Zero-time callbacks excluded |
|---|---:|---:|---:|---:|
| Debug / 1× | 338 / 338 | 324 | 335 | 14 |
| Release / 1× | 339 / 339 | 325 | 336 | 14 |
| Debug / 2× | 333 / 333 | 325 | 330 | 8 |
| Release / 2× | 336 / 336 | 324 | 333 | 12 |

All four runs report peak in-flight 3, pending-at-shutdown 3, one aborted drawable
reservation, one injected zero extent, one injected unavailable drawable, real
minimize/restore skips, resize event delivery, VSync request off/on, all commit
feedback received and zero Metal errors. Every completed frame verifies both
buffer versions and both 8x8 integer texture versions after the ordered GPU
completion event. The second CPU upload update uses a distinct slice; explicit
Metal 4 blit barriers protect the shared private destinations between versions.

Diagnostic resource peak is **29,184 bytes** and allocator peak **7,745,064 bytes**
in every final run, with twelve diagnostic residency allocations and three frame
slots. Limits are 1 MiB diagnostic storage / 16 MiB allocators. In-flight count,
owned allocation bytes, allocator bytes and residency allocation count all return
to zero; shutdown-complete, layer-detached and device-owner-released flags are
true. These are explicit owner/accounting results, not claims that driver memory
or system drawable allocations are zero. Metal 4 resource retirement follows
ordered shared-event GPU completion; presentation callbacks do not release slots.

Three separate startup failure points (device, queue, attached surface) each
recover in the same process. Unfinished drawing mode rejects explicitly,
selection cannot change with a context alive, and default OpenGL initialization
succeeds after Metal teardown in the same process.

The expanded CoreGraphics mode query found native 2× modes omitted by its default
mode list. The runner selects mode 50: 1280x720 points / 2560x1440 pixels @180 Hz.
Both native 2× runs observe GLFW and Cocoa scale 2, initial window
1280x563 points / 2560x1126 framebuffer pixels, then restored/resized
960x540 points / 1920x1080 pixels. macOS constrains initial window height to the
available desktop. The original hard-coded test assumption of 1280x720 logical
size failed at frame 50; the test now compares actual logical dimensions across
the injected physical extent. **No shared rendering change was needed.** Exact
original mode 112 (2560x1440 points/pixels @180 Hz) was restored after both the
failed and passing 2× runs, with `CGDisplaySetDisplayMode` return code 0.

### Capture and measurement evidence

The four final `.gputrace` directories have per-file SHA-256 manifests. Xcode's
metadata groups each trace into one captured frame, while its replay summary
shows **four command buffers, four render encoders and four compute encoders**;
there are no draws or dispatches because M1 encodes only clears and native copies.
Debug 1× replay was inspected in Xcode on the Apple M4. The API navigator shows
`MTL4CommandQueue` commits, drawable waits/signals, event values 1–4,
`MTL4CommandAllocator` 1/2/3 and reuse of 1, `MTL4ArgumentTable`, the two distinct
buffer/texture transfer versions, explicit
`barrierAfterEncoderStages:beforeEncoderStages:visibilityOptions:` with
`MTL4VisibilityOptionDevice`, native render encoding, and CAMetalDrawable present.
The dependency view shows corresponding transfer/clear/drawable/event nodes.

Xcode's Issue Navigator is empty. Its API-usage insights flag diagnostic readback
resources as unused by later GPU work; CPU verification is outside that GPU
consumer analysis. Those insights are recorded separately from API validation
errors (zero). Parent command-buffer labels show the final reused object's name;
individual `setLabel` entries preserve each recording's original frame label.
Xcode initially offered a Metal Toolchain download; “Not Now” was selected and
shader-free replay/API inspection succeeded. A later environment check resolved
`metal` and `metal-libtool` in the mounted MetalToolchain-v27.3.5020.11 component.
Tool availability changed during qualification; no shader compilation or M2
qualification was performed. Its exact paths are preserved in
`metal-toolchain-discovery.json`. Review compiler/tool availability again at M2.

At **1280x720 physical pixels**, final validation/capture-enabled clear + diagnostic
transfer baselines (60 samples each) are:

| Build | CPU present path mean | GPU commit interval mean |
|---|---:|---:|
| Debug | 2.889632 ms | 0.047966 ms |
| Release | 2.945388 ms | 0.048562 ms |

CPU duration includes polling, drawable acquisition, transfer/pass encoding,
submission and accounting; GPU duration uses native Metal 4 commit feedback
start/end timestamps. Capture, validation, scheduling and logging affect these
numbers. They are M1 diagnostic baselines, not throughput or scene-performance
comparisons. No Metal-over-OpenGL performance improvement is claimed; comparable
scene workloads require backend parity in later milestones.

### OpenGL and portability

Debug full native suite: **18/18 PASS**, including CoreSmoke, SceneGPUSmoke and
EditorSmoke. The old ImGui empty-monitor assertion disappeared with the unlocked
desktop, without changing ImGui, shared scene submission or shaders.

Release full native suite: **18/18 PASS**, including CoreSmoke, SceneGPUSmoke and
EditorSmoke. Every native M1 acceptance gate passed in both build configurations.

The suite covers typography/UI, shader/cache/format behavior, imports and invalid
asset diagnostics, sprites, scene/project/physics ownership, managed runtime,
CoreSmoke, renderer capabilities, 2D/3D/PBR/IBL/geometric-normal/selection readbacks,
AA-off silhouette/ID coverage, fonts, SceneGPUSmoke, actual Nutella orthographic/
perspective startup and 60 runtime frames, plus docked editor panels, save/reopen,
Unicode paths, asset/project relocation, duplicate/undo and repeated Play/Stop.
These are existing OpenGL regressions; they do not qualify Metal drawing parity.

The Premake native qualification builds include Hazelnut/Nutella, smoke targets,
SDK/platform-services qualification and example script modules. Full
Debug and Release qualification builds pass, including the final native proof
sources.
`python3 tests/macos/sdk_workflow.py` passes all seven host/toolchain contract tests.
Linux and Windows native compilation/regressions and physical Intel HD 4000 tests
were **not executed** on this Mac. Docker's daemon is unavailable. Source guards
and Premake exclusions were inspected; that inspection is not a native compile
result. Only macOS adds Metal-cpp headers/frameworks/sources. OpenGL 4.1, existing
shader inputs, ImGui pin and all scene/material/asset formats are unchanged.

### Exact final commands and artifact locations

All commands run from the canonical checkout; campaigns run sequentially:

```sh
python3 scripts/macos/metal_m1.py --config Debug --build --capture --output build/metal-m1-evidence/unlocked-20261115/debug-final-unlocked
python3 scripts/macos/metal_m1.py --config Debug --build --hidpi --capture --output build/metal-m1-evidence/unlocked-20261115/debug-native2x-qualified
python3 scripts/macos/metal_m1.py --config Release --build --capture --output build/metal-m1-evidence/unlocked-20261115/release-final-unlocked
python3 scripts/macos/metal_m1.py --config Release --hidpi --capture --output build/metal-m1-evidence/unlocked-20261115/release-native2x-qualified
python3 scripts/macos/metal_m1.py --config Debug --capture --output build/metal-m1-evidence/unlocked-20261115/debug-qualified-final
python3 scripts/hazel.py build --config Release --tests
python3 scripts/hazel.py build --config Debug --tests
python3 scripts/hazel.py test --config Debug --profile native
python3 scripts/hazel.py test --config Release --profile native
python3 tests/macos/sdk_workflow.py
```

The Debug full suite ran before the final diagnostic-only native instrumentation;
shared OpenGL code did not change afterward. The final Debug 1× run uses the
rebuilt harness that also passed native 2× qualification. `debug-unchanged` and
`release-unchanged` contain the required before-edit reruns. Full build/runner
console logs, per-run validation logs, summaries, frame-proof/environment JSON,
display-mode restoration evidence, capture manifests and `capture-inspection.txt`
are preserved in `build/metal-m1-evidence/unlocked-20261115/`. Individual OpenGL
logs are copied there from `build/testing/logs` after the suite completes.

### Remaining boundaries

M1 has no geometry/shader/ImGui integration, shader-visible binding-layout proof,
new effects, render graph or scene-format change. Device-loss recovery remains
unimplemented; shutdown completion timeout aborts instead of freeing in-use
resources. Unavailable-drawable and zero-extent paths use explicit fault injection;
minimize/restore and native display scaling are real window-system operations.
VSync policy is checked through the native layer, without claiming compositor
latency guarantees. No physical Retina monitor, Windows/Linux GPU or HD 4000 was
available; the attached monitor's real 2× backing mode supplies the native scale
qualification. Owned-resource counters exclude OS/driver internals.

The recommended first M2 authorization is **shader qualification only**: pin and
exercise the shared GLSL → SPIR-V → MSL path against the existing shader corpus,
then a tiny native Metal 4 pipeline with reflected argument-table/layout,
clip-depth/orientation and integer-target checks. Review the installed optional
Metal Toolchain and compiler availability first. Keep production apps on OpenGL
and defer scene/PBR porting until that checkpoint passes. M2 has not begun.
