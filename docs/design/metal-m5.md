# Metal 4 M5 — Hazelnut and native compute dependencies

Baseline: `3f58ae56b51e15dcff756d80bbfcb1a4e6612636`,
`feature/3d-foundations`. **M5 PASS — Apple M4 local qualification, November 17, 2026.**
OpenGL remains default. No M6 implementation, merge, release or backend-default
change is included.

## Acceptance scope

M5 covers the real Hazelnut editor, multiple detached ImGui windows on the
available display, native 1×/2× rendering, project/scene/runtime/camera ownership,
selection/depth/outlines, previews and texture replacement, production compute
dependencies, bounded GPU retirement, applicable regressions and measured costs.
Physical multi-monitor support and cross-monitor movement were removed from the
requirements by the owner. They create no M6 obligation. The owner also removed
the six-minute session test and will perform longer performance testing; sustained
performance is not claimed by this acceptance run.

Hardware: Apple M4, native arm64 macOS 27.3 / Xcode 27.3, one VG272U V3 display.
Native 2× uses the display's advertised 1280×720 logical / 2560×1440 physical mode;
the existing M1 mode owner restores the original mode in `finally`.
Windows/Linux, Intel HD 4000 and new branch CI execution remain **NOT RUN**,
separate from local Apple M4 qualification. General Metal game export remains
unqualified. No frozen M3/M4 reference, shader mathematics or tolerance changed.

## Production ownership

- One SceneRenderer and the existing editor/scene/material submission remain
  authoritative. `Framebuffer::CreateEditor` explicitly chooses Metal's FP32,
  signed-ID and depth32 profile; ordinary resource creation retains its strict
  rejection contract. Exposure/display encoding remain shared shader operations.
- `EditorTexture` carries a full-width handle, immutable generation owner, sampler
  and orientation. ImGuiLayer adapts every editor image site. Native texture
  pointers never pass through a uint32 OpenGL texture ID. Scene/material/prefab,
  font/style previews, sprite sheets, icons and content-browser images use this
  boundary. Pending UI-only mipmaps are prepared before consumption.
- MetalDevice owns the existing device, queue, command buffer and three allocator
  slots. Scene producers, display encoding, depth-resolved ID outlines and all
  window consumers execute on that queue. Each slot retains user texture versions,
  producer packets, drawables, layers and residency until the ordered completion
  event and commit feedback both retire. Normal frames do not globally wait.
- GLFW owns windows and callbacks. Detached callbacks explicitly forward Hazel
  events without interpreting the main window's user pointer. Polling, clipboard
  and pointer capture follow the focused/captured host; destruction releases
  capture before destroying that host. ImGui owns docking, settings and navigation.
- Picking publishes completed CPU-owned signed IDs. Scene replacement and resize
  invalidate old observations. Completed GPU resources retire even when no fresh
  UI frame is queued during main-window minimization.
- A drawable whose actual extent changed after ImGui built its draw data is
  deferred for that host. The next GLFW/ImGui frame rebuilds it. There is no
  stretching, stale scissor encoding or GPU wait to conceal a resize race.

The compute contract is deliberately small: precompiled native library/entry,
read-only RGBA32F texture 0, write-only RGBA32F texture 1, exact reflected immutable
parameter bytes at buffer 0 (at most 256), fixed 8×8 groups with kernel bounds
checks. A framebuffer packet captures the dependency and preserves signed IDs and
depth. Existing allocators, residency, barriers, completion and readback execute
it. Missing entries, ABI mismatch, in-place execution and invalid destinations
are rejected. OpenGL reports unsupported compute explicitly. No effect, graph,
scheduler, duplicate scene traversal or parallel GPU test framework is added.

## ImGui dependency and code review

Core/GLFW/OpenGL stay pinned to 1.89.4 WIP,
`e25e4526cd41cd6536194de098540d54244f54e9`. Only the official Metal 4 renderer is
backported from `9dbcb2f1cc0e8a3ffac51d7f9d28efdb228db14b` (MIT).
[Backport provenance and maintenance differences](../../Hazel/src/Hazel/ImGui/Backends/README.md)
identify the November 16 fixes, including
[per-slot wait correction, #9624](https://github.com/ocornut/imgui/commit/fb57aa8aa3b92720859aeeaeb09822679eb784b9)
and [deferred draw-buffer recycling, #9625](https://github.com/ocornut/imgui/commit/514f6a8e7f2deeab14063bf9850ddacaadde9aee).
Hazel removes upstream secondary-window queues/allocators/events; its own slot
completion covers every consumer. Buffer storage is recycled only when that slot
is reused. Per-viewport projection reservations, reset callbacks, four-byte
16-bit index alignment, drawable residency and target formats are preserved.

The bounded compatibility work uses the static 1.89 font atlas and command-list
API. The unused Metal-C++ wrapper/manual-autorelease alternative, empty teardown
and dead display-sync state were removed: this bridge is Objective-C++ with ARC.
Premake sets the native target ARC setting because the pinned Xcode generator
omits file-filtered build options; the new bridge files reject non-ARC builds.
MetalSurface transfers exactly one retain into its existing C++ owner.
Native allocation/pipeline failure checks prevent nil resources entering Cocoa
arrays or GPU encoding. The injected draw-callback abort verifies unsubmitted
resource cleanup; it is not presented-frame evidence.

Review corrections include normalizing Metal file texture paths exactly like
OpenGL, fixing asset-relative save/relocation identity; retiring completed slots
while minimized; and excluding offscreen submissions from editor counter results.
Success, busy admission, stale extent, pending-window destruction, initialization
cleanup, encoder abort and shutdown were traced through their owners. No general
profiling framework or additional device system was introduced. Conservative GPU
visibility barriers remain explicit; no per-window CPU/global GPU waits were added.

## Qualification infrastructure and evidence

Raw commands, native session identity, mode records, logs, actual drawable PPMs
and per-run results remain under `build/metal-m5-evidence/`. The tracked aggregate
[results](metal-m5-results.json) identifies accepted runs and exclusions.

```sh
python3 scripts/macos/metal_m5.py --config Debug --build --output <fresh-directory>
python3 scripts/macos/metal_m5.py --config Release --build --output <fresh-directory>
python3 scripts/macos/metal_m5.py --config Debug --only consumers --native-2x --output <fresh-directory>
python3 scripts/macos/metal_m5.py --config Release --only consumers --native-2x --output <fresh-directory>
python3 scripts/macos/metal_m5.py --config Release --only consumers --timing --output <fresh-directory>
```

`--only workflow` reuses the maintained EditorSmoke executable through Metal;
its synthetic keys activate real ImGui/editor commands, not a duplicate editor.
Its few GL-specific boundaries use production asynchronous readback on Metal.
All original GL assertions remain, including font glyph IDs and texture reload
identity. It covers scene/project replacement and relocation, save/reopen,
Edit/Simulate/Play/Pause/Step/Stop, authored versus inspection camera invariants,
renderer request/apply-time/VSync policy, history/gizmos/hierarchy, material/mesh
and environment authoring, preview IDs and asset lifetime. The final readonly
PBR project transition captures the actual native editor drawable and verifies
that capture/inspection did not change authored scene inputs.

The four-window fixture performs repeated native docking/undocking, main and
secondary iconify/restore with GLFW attribute checks, focus/pointer capture,
clipboard round-trip with pasteboard restoration, window/producer resizing,
late-drawable rejection and pending-window closure/recreation. Reset callbacks
exercise projection/sampler restoration. Four actual drawable captures check
**80 samples**, including a capture after lifecycle transitions, asymmetric output,
depth-resolved outlines, nearest sampling, UI-only mipmaps and immutable texture
replacement. The encoded bound stays one level; prior rendering tolerances are
unchanged. Presentation timestamps of zero are excluded.

`--only shutdown` closes while **three genuine production slots are gated**:
two UI submissions plus a production offscreen readback. Layer teardown releases
the diagnostic gate, then normal shutdown retires all consumers. It does not
freeze a third extra drawable and exhaust the compositor's buffer pool.
`--only abort` throws from an ImGui draw callback before commit and verifies zero
submissions and zero retained GPU resources. Neither case creates another queue.

The compute fixture proves render→compute→readback and
render→compute→render→readback across three gated production submissions, with
HDR values above one, exact signed IDs/coverage/depth, immutable parameters,
rejection paths, pending destruction and retirement. Native Debug/Release authored
2D, 3D and PBR Hazelnut startup/render smokes remain separate from the full workflow.
Earlier user-reported physical input/docking checks are supporting evidence;
executed native lifecycle/controller tests are the reproducible acceptance basis.

## Timing boundaries

With validation disabled, `acceptance-timing-release/results.json` contains
739 positive-main-presentation GPU samples, 749 submission CPU samples and 759
application CPU samples. Three timestamp entries per existing slot are resolved
only after its completion event; native counter storage is fixed at nine entries.
Its opaque driver storage is not assigned an invented byte size.

| Cost | Median ms | p95 ms |
| --- | ---: | ---: |
| Application scene/controller CPU | 0.023 | 0.041 |
| Application ImGui CPU | 1.420 | 1.926 |
| Retirement/counter-resolution CPU | 0.005 | 0.022 |
| Drawable acquisition CPU | 3.851 | 4.128 |
| Resource preparation CPU | 0.032 | 0.051 |
| Scene encoding CPU | 0.017 | 0.029 |
| ImGui encoding CPU | 0.043 | 0.062 |
| Queue commit CPU | 0.040 | 0.052 |
| Present request/event handoff CPU | 0.012 | 0.019 |
| Whole-submission GPU | 2.818 | 3.843 |
| Producer/scene GPU completion interval | 0.186 | 2.897 |
| ImGui GPU completion interval | 0.199 | 3.558 |
| GPU end → positive presentation, host time | 17.802 | 20.803 |

Metal 4 `writeTimestampIntoHeap` guarantees completion of earlier work but may
include subsequent overlapping work. These **completion intervals are not
exclusive pass costs and are not additive**. The producer interval includes
scene/display work and first-use mip preparation. No extra barriers isolate
measurements. Counter frequency is 24 MHz on this M4; invalid/reversed samples
are rejected (zero rejected in this run). Commit feedback and drawable timestamps
are documented host times; their lag includes compositor buffering/pacing and is
not a GPU presentation-kernel cost. Exclusive per-pass GPU costs and a separate
GPU submission/presentation cost are unavailable from these boundaries. CPU
application and submission distributions have different sample domains. The
fixture permits drawable readback from first presentation and makes no speedup or
sustained-performance claim.

## Failures retained, warnings and limitations

Earlier failed diagnostics remain available: locked-console/zero-presentation
runs; missing readback residency; teardown callbacks entering a destroyed layer
stack; oversized occluding 2× fixture windows; stale shader-test binaries; missing
ARC flags; and the live DPI scissor assertion. These were fixed or excluded for
the documented environmental reason, never accepted through relaxed bounds.
The final workflow port also exposed the Metal texture path identity defect and
corrected its own GL-specific reads and ID-coordinate assumptions. The cancelled
long hold is not counted as a correctness failure or sustained-session PASS.

Independent direct-include checks pass for seven affected headers. Debug/Release
builds, Python/whitespace checks and the README-linked public-docs audit pass.
The final include-only portability correction adds Hazel's required PCH header
to the new portable sources, preserving the MSVC contract; Windows execution
is still NOT RUN. Compiler warning classification is recorded in the evidence.
Existing deprecated-copy, unused/C++20-extension, Cocoa deprecation, vendor/archive
and Xcode script-phase warnings remain distinguished from new first-party warnings;
no blanket suppression was added. The known frozen split-sum IBL approximation
remains accepted, not repaired by M5. Source-free shader/cache relocation is not
certification of general Metal game export or another platform.

Recommended M6 qualification scope remains platform/package/source-free validation,
clean-clone onboarding, native metrics on the targeted hardware, and an explicit
owner decision about the macOS default. No M6 work is included here.

## Acceptance summary

| Available gate | Debug | Release |
| --- | --- | --- |
| Full native editor workflow/configuration and authored startup | PASS | PASS |
| Four-window lifecycle, focus/clipboard/capture, repeated docking, 1×/2× | PASS | PASS |
| Pending shutdown and encoder-abort cleanup | PASS | PASS |
| Production compute dependencies, exact IDs/depth and retirement | PASS | PASS |
| Native OpenGL regressions | PASS, 18 executables | PASS, 18 executables |
| Unchanged M3/M4 numerical/reference cases | PASS, 135 captures | PASS, 135 captures |
| Shader/cache/source-free qualification | PASS, 17 rejections | Retained applicable shared-toolchain evidence |

All accepted native runs finish with zero GPU errors, pending bytes, attachment
generations and ImGui bytes. Full workflow submissions/completions are 1,536 in
Debug (1,409 positive presentations) and 1,097 in Release (985 positive). Pending
shutdown runs each complete 32 submissions after closing with three slots gated.
The results JSON records each detached host, allocation peaks and exact evidence
paths. **Untested conditions remain Windows/Linux, Intel HD 4000 and new branch
CI execution; general Metal export is not certified.** No sustained-performance
claim is made. Commit/push is scoped to this M5 milestone only.
