# Metal 4 M3 — production Renderer2D runtime parity

Baseline: `3b22ada5e8f43a5b67b8210bf98dafeca24bec71`, branch
`feature/3d-foundations`. Acceptance is the user's complete M3 request.
**M3 PASS on the available Apple M4 host; see the current continuation below.**

## Current continuation from 808e31d

The approved [shared deterministic line implementation](metal-m3-deterministic-lines.md)
replaces Renderer2D's native line rasterization on both APIs. **M3 PASS on the
available Apple M4 host:** independent/frozen/cross-backend line gates and
Debug/Release native 1×/2× lifecycle, Nutella input/camera and extended-session
qualification pass. [Current results](metal-m3-deterministic-results.json)
distinguish this evidence from the historical INCOMPLETE record below.
Windows/Linux/Intel execution is NOT RUN. The earlier locked attempt and
[legacy investigation](metal-m3-lines.md) remain preserved. OpenGL remains the
default; no M4 implementation or merge is included.

## Acceptance gates

- Full Renderer2D initialization/shutdown, normal capacity and reduced texture
  slot flushes, ordered quad/sprite/circle/line/MSDF text, atlas/Unicode changes,
  statistics, immutable camera/texture versions and resource reuse.
- Native Metal 4 / native OpenGL numerical comparison: exact signed IDs and
  stable interiors, independent source-over RGB/alpha oracle, clipping,
  transparent overlap, orientation, depth, passive picking and HDR/Neutral.
  Preserve earlier M2-B4/M2-C numerical fixtures and their 1e-6 tolerances.
  Mixed FP32 fixtures allow 2e-5 color / 1e-6 depth, with exact IDs.
  The actual OpenGL display compositor requires RGBA16F; Metal retains RGBA32F.
  Authored runtime comparisons therefore require exact IDs, 1e-6 depth,
  <=1 encoded 8-bit RGB level, and <=3 FP16 ULP per channel plus the 2e-5
  shader bound. The three stores are world conversion, UI panel and glyph.
  These precision bounds do not replace the independent FP32 blend oracle.
- Real Nutella RuntimeSession scenes, world and screen UI, UI input, camera and
  texture changes, resize/minimize/restore/unavailable drawable, native scale,
  positive presentation timestamps and visible captures, Debug and Release.
- Capacity overflow, glyph page changes, repeated resource reuse, startup and
  submission failure recovery, pending shutdown; zero new unresolved warnings
  and zero Metal API/GPU validation errors in exercised paths.
- Matched 1280x720 / 1920x1080 CPU/GPU timings, draw/batch and attachment memory
  observations with settings, warmup and samples; investigate generations.
- Available OpenGL/Renderer3D/runtime regressions, shader artifact loading and
  source-free relocation, public docs/example/launch commands and link audit.
  Windows/Linux execution and unavailable hardware must be explicitly NOT RUN.

## Ownership

Scene and RuntimeSession retain the existing world/UI traversal. Renderer2D
owns shared CPU streams, batches, texture slots, ordered runs and statistics.
MetalRendererAPI captures native versions; MetalDevice owns encoding, residency,
barriers, queue completion and retirement. Generated shaders remain unchanged.
Renderer3D, PBR/IBL, Hazelnut Metal ImGui, defaults and M4 are out of scope.

Raw commands/logs are retained under `build/metal-m3-evidence/`.

## Historical verdict at 808e31d — INCOMPLETE

Implementation stopped at a measured smoothed-line coverage/picking blocker.
The normal acceptance runner **fails** in both configurations. No reference,
production OpenGL state or tolerance was changed to turn that failure green.
Results and source/evidence hashes are in [metal-m3-results.json](metal-m3-results.json).

Host: Apple M4 arm64, macOS 27.3 (26C5035r), Xcode 27.3 (27C5020n).
The complete Nutella visible/input/native-DPI/window-lifecycle gate is additionally
BLOCKED: the console locked during this work and the repository runner refused
execution. An earlier three-quad window regression passed during implementation;
it is not final M3 world/UI qualification. No positive presentation count or
physical UI observation is claimed for the final offscreen runs.

### Implemented ownership and behavior

`Renderer2D::Init`/`Shutdown` now own all four normal CPU streams and font services
on both backends. The temporary `InitQuads`, initialization mode, eight-sprite
admission bound, opaque-only texture/tint checks and primitive/UI rejections are
removed. Normal scenes retain OpenGL's quad/circle/line/text grouping; ordered
passes retain painter order, font transitions, clipping and picking masks.
Solid quads use shared slot-zero packing. Capacity is 20,000 quads; the 20,001
fixture flushes normally. Debug exercises two slots; Release exercises four.

`MetalRenderer2D.cpp`, `MetalDeviceRenderer2D.cpp` and `MetalRenderer2DFrame.h`
replace the old quad-only module names. Backend batches capture only each run's
vertex range, camera and texture versions, atomically. Native pipelines carry
source-over RGB/alpha factors and integer/color write masks. The device owns
encoding, residency, barriers, depth state, mip generation and retirement.
Buffers reuse storage only in GPU-completed, feedback-retired frame slots;
idle retained storage is included in resource accounting. Failed batches publish
no partial draws. `CancelScene` lets SceneRenderer unwind a failed world/UI pass
and recover on the next pass.

The normal SceneRenderer/RuntimeSession scene and UI traversal remains shared.
Native world conversion now preserves world depth through an explicit GPU copy
and load before UI. Source inspection found OpenGL's existing depth blit in
`OpenGLFramebuffer::CompositeWorldTo`; M2-C's written statement that OpenGL also
left destination depth cleared was incorrect. M3 corrects native behavior and
the corresponding production test. The frozen B4 quad data remains unchanged.

RGB8 MSDF atlases expand to native RGBA8. RGBA8 linear/sRGB and generated RGBA32F
textures retain immutable versions and accept alpha. Native sampler/mip support
is implemented; generation is validation-tested, while exhaustive minification
and wrap/filter numerical qualification remains NOT RUN. Existing canonical
GLSL, generated Metal artifacts and the qualified 32-slot tier-2 ABI are unchanged.

### Measured line blocker

OpenGL initialization enables `GL_LINE_SMOOTH`. The existing line artifact emits
flat/interpolated color without a shared analytical coverage representation.
Native `PrimitiveTypeLine` produces aliased coverage. Added diagonal/vertical
fixtures expose this difference, including integer writes on OpenGL's smoothed
fringe:

| Gate, each configuration | Result |
| --- | --- |
| Mixed fixture 0 / 1 signed-ID mismatches | FAIL — 308 each |
| Mixed fixture 2 mismatches after textured overlap | FAIL — 266 |
| Maximum mixed linear color error | FAIL — 0.353042793 |
| Nonordered circle/quad grouping fixture | PASS — exact IDs/depth; color <=1.781e-6 |
| Separate aliased-OpenGL diagnostic | Exact IDs/depth; color <=2.772e-6 |

`--diagnostic-aliased-lines` exists only in the regression executable. It disables
smoothing in that isolated process to distinguish coverage from binding,
blending, orientation or ID corruption. It cannot qualify M3 and is never used by
the normal acceptance command. Production OpenGL and frozen references retain
smoothing. A native-only line tessellator or relaxed image tolerance was not added.
The next M3 decision is a shared line coverage/picking contract compatible with
canonical shader generation and existing OpenGL behavior.

### Acceptance matrix

| Gate | Debug | Release |
| --- | --- | --- |
| Focused builds, full Renderer2D startup | PASS | PASS |
| API + GPU shader validation, offscreen completion | PASS — 190 | PASS — 190 |
| Prior nine B4 fixtures, retained readback | PASS — exact saved bytes | PASS — exact saved bytes |
| Capacity overflow, two/four slots, atlas/Unicode changes | PASS — two slots | PASS — four slots |
| Independent blend/clip/passive-ID oracle | PASS — <=1e-6 | PASS — <=1e-6 |
| Authored world + screen UI at both resolutions | PASS, offscreen | PASS, offscreen |
| OpenGL smoothed-line parity | FAIL | FAIL |
| Gated resize/replacement/destruction, busy retry, pending shutdown | PASS, offscreen | PASS, offscreen |
| Failed mixed batch and failed SceneRenderer pass recovery | PASS | PASS |
| Six focused native OpenGL/runtime regressions | PASS | PASS |
| Source-free shader/cache/relocation, Unicode/space path | PASS | PASS |
| Direct native/shared headers without PCH | PASS | shared check |
| Real Nutella UI/input/camera/resize, positive presentation, native 1x/2x | BLOCKED | BLOCKED |
| Extended window session and final window pending-work shutdown | NOT RUN | NOT RUN |
| Windows/Linux execution, Intel hardware, new CI, full Metal export | NOT RUN | NOT RUN |

The prior nine fixtures cover 6,272 pixels per configuration, exact signed IDs
and zero color/depth error; all ten saved files including retained readback match
M2-C hashes. The new authored scene compares 921,600 + 2,073,600 pixels per
configuration with **zero ID mismatches**, maximum depth error 5.961e-8, and
maximum encoded RGB difference one 8-bit level. Maximum raw color error is
0.000516952, within the declared per-pixel FP16-store bound; the production
OpenGL display is RGBA16F and native Metal remains RGBA32F. These checks retain
that real format difference rather than modifying OpenGL's compositor.

The offscreen scene is owned by RuntimeSession, receives deterministic input
through its existing router, and invokes Scene's runtime update with an owned
SceneRenderView. It does not call window-dependent `RuntimeSession::Update` and
therefore does not replace the blocked real Application/player gate.
The new public image is an actual GPU readback exported to sRGB PNG, not a window
capture or proof of presentation. The source example is
`examples/Sandbox/Sandbox-Renderer2D.hproj`.

### Performance and memory observations

These are validation-enabled **offscreen** measurements, including GPU readback.
They are not window frame costs or an OpenGL speed comparison. Each resolution
runs 80 serial scene frames; first ten and last ten are excluded, leaving 60.
Scene CPU includes Scene/RuntimeUI work, excludes physical-device polling.
Submit CPU includes native preparation and allocation of CPU readback arrays;
GPU time includes world, Neutral/depth transfer, UI and readback copies.

| Release extent | Scene CPU median / p95 ms | Submit CPU median / p95 ms | GPU median / p95 ms |
| --- | --- | --- | --- |
| 1280x720 | 0.058 / 0.082 | 1.406 / 1.586 | 0.745 / 0.977 |
| 1920x1080 | 0.066 / 0.083 | 1.494 / 1.854 | 1.401 / 2.170 |

Both have 13 draws, eight flush batches and 48 geometry items per scene frame.
Steady attachment allocations stay at six / eight total, with 118 generation
reuses in each 60-frame measured interval. Attachment bytes are 46,309,632 /
101,097,728. Buffer allocation counters stay at 95 / 98 throughout those
intervals, while each interval reuses 1,003 buffers. Idle retained slot buffers
occupy 66,427,168 / 149,395,744 bytes, principally diagnostic readback storage.
There is no steady frame-growth in these samples; 160 scene frames are not an
extended-session leak qualification.

Release peaks: 254,994,976 frame/retained allocation bytes, 124,231,936 attachment
owner bytes and 7,892,520 allocator bytes. These counters overlap and must not be
added. Accounting now deduplicates shared allocations and includes idle slot
buffers; older peak counters are not a direct comparison. Pools retire to zero
at shutdown. One deliberately retained 32x32 framebuffer owner remains alive
until the offscreen fixture returns; it is not pending GPU work. Driver/library,
sampler implementation storage and assets outside captured work are not totalled.

### Reproduction, warnings and public readiness

Commands for the final results (normal line gates deliberately fail):

```sh
python3 scripts/macos/metal_m3.py --config Debug --texture-slots 2 --resources Hazel/Resources --output build/metal-m3-debug-fresh
python3 scripts/macos/metal_m3.py --config Release --resources Hazel/Resources --output build/metal-m3-release-fresh
python3 scripts/macos/metal_m1.py --config Debug --renderer2d Hazel/Resources --output build/metal-m3-window-fresh
```

Build the existing smoke targets first, or add `--build`; use fresh directories.
The window runner requires an unlocked intended console. Native logs preserve
validation diagnostics, timings, pixel data and CPU/resource CSVs under
`build/metal-m3-evidence/`. Source-free shader tests use copied artifacts with
`PATH` and `DEVELOPER_DIR` unavailable, without changing original packages.

There are zero new unresolved first-party warnings. Existing deprecated-copy,
unused-parameter, unhandled factory-switch and test missing-field diagnostics,
plus archive/Xcode phase and C# toolset warnings, remain in build logs. No broad
suppression was added. The public-docs link audit reports zero findings. Launch
commands, status/feature matrix, example and readback image are updated; the
master CI badge remains labelled master, with no claim of CI for this work.
The canonical Sandbox snapshot test now selects `Sandbox.hproj` explicitly,
so multiple shipped sample descriptors do not break the regression runner.

Finish smoothed-line parity, then the real player/UI/DPI/long-session gates before
considering M3 complete. Only after M3 passes, the recommended M4 entry is a
bounded unlit cooked-GLB vertex/index/material submission with depth and signed
IDs, through existing SceneRenderer ownership. No M4 implementation, merge,
backend-default change, Renderer3D or PBR/IBL work was started.
