# Metal 4 M2-C — first integrated visible opaque quad scene

Baseline: `f15815ad736cbd9b70e30cbe4b0f444ac5421181`,
`feature/3d-foundations`. Unrelated untracked .DS_Store files are preserved.

The acceptance gates are the user's M2-C request: Debug/Release production
Scene → SceneRenderer → ordered Renderer2D → framebuffer → native M1 Metal 4
presentation, positive presentedTime plus real visible capture, established
orientation/sampling/color/exposure/depth/signed IDs, camera and texture changes,
resize/minimize/restore/unavailable drawable/native 1× and 2×, bounded resource
reuse and pending-work shutdown, M2-B4 numerical and focused native OpenGL
regressions, zero new unresolved first-party warnings or Metal validation errors.
Every required gate must pass before commit/push. No backend default, parity,
editor/ImGui, Renderer3D, blending, text, circles, effects or next stage is implied.

## Implementation contract

Nutella opts in with `--metal-quads`; default startup remains OpenGL.
`RendererInitialization::Quads` initializes only the existing Renderer2D quad
services. Scene admission explicitly rejects unsupported meshes, lights,
environments, circles, text, screen UI, nonopaque tint and more than eight
sprites. Ordinary script/scene/animation ownership remains RuntimeSession's.

SceneRenderer invokes the existing DrawWorld2D callback once. Display-referred
worlds bypass exposure; scene-referred worlds use the existing WorldPresentation
Neutral/exposure shader into display-linear RGBA32F, with exact signed ID copy.
DisplayOutput then encodes sRGB into the BGRA8Unorm CAMetalLayer drawable. World
uses signed R32Sint and Depth32Float LESS/write, clear ID -1 and depth one.
As in the existing OpenGL presentation compositor, the display composite's depth
clears to one; depth testing belongs to the world target.

Native details remain under Platform/macOS/Metal. The production offscreen quad
encoder is shared with visible submission. One M1 command buffer owns the concrete
world/conversion/output pass chain, explicit render-to-sample visibility,
residency sets, immutable arguments/buffers, drawable wait/signal/present and
completion feedback. No CPU readback/display copy or second scene traversal.
Offscreen Submit still requests complete color/ID/depth readback; EncodeOutput
queues GPU-only presentation, retaining no full-frame CPU arrays.

Offscreen rows retain the established OpenGL lower-left interpretation. Final
DisplayOutput reverses that convention once for the native window. Its existing
GLSL and lighting mathematics are unchanged: the offline MSL translator disables
vertex Y inversion for DisplayOutput only. The artifact profile is versioned so
source-free stale packages reject. Intermediate passes preserve row/ID alignment.

Framebuffer extents accept 1..4096 per axis, at most 8,388,608 pixels, one sample,
ordered RGBA32F/R32Sint/Depth32Float. Native textures are sampleable render targets.
Each owner reuses completion-retired generations, with at most four current-size
generations; resize drops its old pool, leaving submitted generations retained by
M1 until completion plus feedback. All live owner and in-flight attachment
allocations are bounded to 1 GiB; pending frame allocations have a separate 1 GiB
cap and allocator storage 64 MiB. These are allocation guards, not a total driver
memory counter. Actual footprint/reuse measurements and limitations follow below.

A queued window packet is consumed once by SwapBuffers. Zero/hidden/unavailable/
busy/mismatched drawable paths discard unsubmitted scene work without a GPU wait;
the next application frame uses current physical dimensions. Clear-only Present
remains available when no scene output was queued. Ordinary resize/submission
never globally waits; shutdown drains with the existing bounded failure policy.

## Evidence and verdict

Raw logs/captures are under `build/metal-m2c-evidence/`. The qualified source
hashes, file list and gate results are recorded in [metal-m2c-results.json](metal-m2c-results.json).
All required native M2-C gates pass as detailed below. Commit/push remains limited
to this milestone on `feature/3d-foundations`.

## Qualification — PASS (native Apple M4)

Final API **and GPU shader validation** runs are
`build/metal-m2c-evidence/{debug,release}-validated-native{,-2x}`.
The independently correlated counts below exclude zero/missing presentation
timestamps. A summary printed before the final presentation callbacks can be
slightly lower than the complete raw-log join; neither count includes zero times.

| Gate | Debug | Release |
| --- | --- | --- |
| Integrated 1× completed / positive-presented join | PASS — 326 | PASS — 326 |
| Integrated native 2× completed / positive-presented join | PASS — 324 | PASS — 324 |
| Original M2-B4 offscreen fixtures + production Scene checks | PASS — 25 completed submissions | PASS — 25 completed submissions |
| Production shader loading/cache/reflection | PASS | PASS |
| M1 clear-only transfer/presentation regression | PASS | PASS |
| Six focused native Apple OpenGL regressions | PASS | PASS |
| Direct public/native headers without PCH | PASS | shared C++17 syntax check |
| SDK workflow / public-docs link checks | PASS — 7 tests / 0 findings | shared checks |

Host: Apple M4 arm64, macOS 27.3 (26C5035r), Xcode 27.3 (27C5020n).
The native 2× runs select a real advertised 1280×720 logical / 2560×1440
physical monitor mode, verify Cocoa/GLFW scale two, and restore the exact prior
mode. Cocoa can constrain the initial window to 1280×563 logical (2560×1126
physical); the framebuffer follows that actual extent. Resize then verifies
960×540 logical / 1920×1080 physical. 1× resize verifies 960×540 physical.
No synthetic extent injection is claimed for the integrated runs; the original
clear-only test retains its separate synthetic 2× diagnostic.

Both configurations cover real resize, unavailable drawable, zero extent,
minimize/restore, VSync policy changes, allocator reuse and pending shutdown.
All four final scene runs have peak in-flight three, three retained slots at
shutdown, zero errors, 11 attachment allocations across the original/resized
world and display owners, and 668–678 generation reuses. At the shutdown gate,
CAMetalLayer may permit only two additional drawable submissions; an earlier
GPU-completed slot can still await commit feedback. The test either observes
that third retained slot or fills it with production offscreen work before
closing, without waiting for a drawable. This proves retention through both
completion and feedback, not three GPU-gated window drawables. The preserved
M2-B4 lifecycle fixture separately gates three offscreen GPU submissions.

### Visible evidence and color conventions

Actual production Nutella captures are in `debug-capture-final/` (1×) and
`release-capture-2x/` under the evidence root. These execute the built Nutella
binary, temporarily wrapped in uniquely named local app bundles for reliable
computer-use selection. This is capture staging, not a second renderer or a
qualified distribution package. The public representative capture is
[metal-quads.jpg](../images/metal-quads.jpg).

Direct observation confirms three textured quads with the green nearer quad
below and overlapping the others, matching its authored negative Y and positive
Z. RMB+W moves the camera visibly; a held Home restores the authored view.
The existing SpriteAnimation changes the left quad between its checkerboard
and corner region across frames. Debug and Release captures retain these states.
Native logs show thousands of positive presentation timestamps during the
capture sessions, zero Metal errors, and zero remaining frame/attachment storage
at ordinary shutdown. The original scene texture is Hazel's existing opaque
32×32 colored checkerboard fixture, copied into Sandbox's owned asset root.

The first visible capture revealed that preserving the offscreen lower-left row
convention through final output inverted world Y. Only final DisplayOutput's
generated vertex orientation is corrected. All ten canonical GLSL files are
unchanged. All nine other shader packages' native libraries, and DisplayOutput's
fragment library, are byte-identical to the baseline; only DisplayOutput's vertex
library changes. The artifact profile changes for all packages so stale output
libraries reject even without source files. Both package loaders pass.

The production Scene fixture additionally renders an asymmetric 96×64 target,
changes its actual texture version and camera between submissions, checks exact
signed entity IDs and depth, and compares Neutral/exposure to an independent
CPU formula with absolute error <=1e-6. It rejects unsupported content and drawing
after world conversion. The original nine saved B4 fixtures and retained readback
remain byte-identical to the baseline in both configurations: 6,272 pixels per
configuration, exact signed IDs/clears and zero maximum color/depth error.
A capture color sanity check also agrees with independently calculated
sRGB → tint → Neutral → sRGB interior colors within two 8-bit levels after JPEG
capture; it is not substituted for the float/ID/depth GPU readback oracle.

### Performance and memory observations

These are measured observations with Metal API and GPU shader validation enabled,
not an optimization comparison. First 130 submissions and final ten are excluded.
Steady resized scene samples have VSync enabled on the 180 Hz display.

| Configuration / physical extent | Samples | Present CPU median / p95 ms | GPU median / p95 ms |
| --- | ---: | ---: | ---: |
| Debug / 960×540 | 193 | 2.457 / 2.951 | 0.404 / 0.424 |
| Debug / 1920×1080 | 193 | 2.525 / 3.175 | 1.233 / 1.452 |
| Release / 960×540 | 192 | 2.815 / 3.429 | 0.393 / 0.426 |
| Release / 1920×1080 | 194 | 2.830 / 3.672 | 1.245 / 1.457 |

The exact CPU values are recorded in the machine-readable results. CPU timing
covers Present polling, drawable acquisition, immutable native preparation,
encoding and commit; it excludes Scene/RuntimeSession CPU work. GPU feedback
covers the explicit world/Neutral/output work. No full engine-frame CPU or
OpenGL speedup claim is made.

Final 1× runs peak at 105,414,656 attachment-owner bytes; native 2× runs peak at
336,789,504 bytes. Allocators peak at 7,204,392 bytes. Other accepted integration
runs under different drawable timing retained more generations and reached
479,920,128 attachment bytes, still below the guard. Pool occupancy depends on
GPU/feedback timing; the smaller final-run peak is not a claimed optimization.
All final runs retire to zero attachment generations and zero pending native
frame bytes. Frame-owned and attachment-owner counters overlap and must not be
added. Driver/library, native argument-table implementation storage and external
asset-cache ownership are not totalled by these counters. No total-system GPU
memory or unlimited-session leak claim is made.

### Reproduction and remaining limits

See the [public development commands](../development.md#experimental-metal-shader-resources)
for artifact preparation and the exact Nutella launch. Qualification extends
`MetalRenderer2DQuadSmoke` and `MetalM1Smoke`; no duplicate smoke executable or
scene renderer is added. The latter's `--quads <resource-root>` mode runs the
existing authored Sandbox quad scene through RuntimeSession/SceneRenderer and
animates its primary camera. Repeat the documented runners with both Debug and
Release, using fresh evidence directories and an unlocked console.

The full checkpoint build log is `build-checkpoint.log`; final focused builds
are `build-final-source.log`. Final focused builds have zero C++ warnings.
The complete rebuild also exposes pre-existing first-party deprecated-copy,
unused-parameter and unhandled-Metal factory-switch diagnostics at unchanged
code sites, plus existing archive/Xcode phase warnings; raw diagnostics remain
in `warnings.txt`. Newly exposed copy-assignment diagnostics for the components
used by these tests are fixed with explicit default assignments. No blanket
warning suppression is added, and no new unresolved warning is accepted.

Windows/Linux native builds, Intel GPU execution, CI dispatch, full editor Metal
integration, general Metal export/relocation and full Renderer2D parity are
NOT RUN / outside this native milestone. Source platform guards and existing
Premake exclusions are inspected; local Apple OpenGL results do not claim those
platforms. Public README-linked docs, example instructions and the new screenshot
are updated; the static audit has zero findings. CI policy is unchanged and no
remote success is claimed.

The current opt-in limit is eight opaque quads, the explicit attachment/sampling
profile above, and the fixed world/Neutral/output pass sequence. No blending,
other primitives, text/font atlas, UI/ImGui, Renderer3D, render graph or default
backend change is included. Recommend next a cohesive **production ordered
Renderer2D world-and-screen-UI parity** milestone: normal batching capacities,
quad/circle/line/text streams, alpha/clipping and IDs through the same owners,
qualified in player/UI on both backends. That recommendation authorizes no work.
