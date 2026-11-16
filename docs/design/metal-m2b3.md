# Metal 4 M2-B3: initialized quad-only ordered flush

Baseline `b250c8c53e08154388b52e3578a52d892917898e` on
`feature/3d-foundations`, synchronized with origin. Four unrelated untracked
.DS_Store files are preserved. This checkpoint does not enable end-user Metal
rendering, presentation, editor/player, blending or other primitives.

Predeclared acceptance gates:
- Explicit quad-only initialization without font/text/circle/line factories;
  existing DrawQuad/DrawSprite packing, texture-slot admission, QueueRun and
  FlushOrdered own the CPU submission. Remove B2's direct per-quad enqueue route.
- Preserve B1's Quad32 artifact/vertex ABI and B2's bounded offscreen profile:
  RGBA32Float/R32Sint/Depth32Float, 1..32 extents, eight total quads per frame,
  single sample, depth LESS/write, two-sided, opaque only, no blending or clip.
- One native indexed draw per shared run. Preserve texture indices, expanded
  shared index pattern, camera and immutable upload/descriptor/texture versions.
  M1's three allocator slots, completion feedback, budgets and drain stay intact.
- Debug/Release API + shader validation, deterministic full-target color/depth
  (absolute error <=1e-6) and exact signed IDs/clears. Exercise empty flush/end,
  multiple quads/textures, equal-depth ordering, repeated scenes/camera updates,
  explicit flush/reuse, rejected-chunk recovery, shutdown/reinitialization and
  pending-work shutdown. Retain B2 sampling and twelve-successive-submission tests.
- Relevant native OpenGL Debug/Release regressions, direct header/portability and
  warning checks; report unavailable platforms honestly. Update affected developer
  documentation and run the scheduled public link health check.

Implementation and final measurements will be recorded here after qualification.

## Ownership and supported path

`InitQuads` publishes an initialized Renderer2DData only after the Quad32 shader,
white texture and bounded CPU vertex storage succeed. It never initializes Font
or creates unrelated GPU vertex arrays/buffers or primitive shaders. Default
`Init` remains unchanged for OpenGL and explicitly unavailable for Metal. No
Renderer/scene traversal, default backend or application initialization changes.

`DrawQuad(transform, texture)` delegates to `DrawSprite`, which uses the existing
texture-slot search, BuildRenderer2DQuad, CPU vertex cursor, QueueRun and counters.
Metal snapshots immutable texture/sampler versions at admission alongside these
same texture slots. Equality requires the same wrapper and captured version:
SetData between pending quads gets another slot, even before a flush. Unchanged
versions reuse a slot. The white texture occupies zero; unused descriptors use
its captured version. There is no second slot-deduplication or batch algorithm.

Metal BeginScene owns initialized CPU scene state and the bounded backend camera
scope. Every supported scene is ordered. BeginOrderedScene drains an active
previous scene before changing the camera. EndScene calls Flush(End). Explicit
Flush consumes a chunk once, resetting its CPU cursors/slots/runs; repeating it
when empty performs no uploads or draws. Capacity uses the existing NextBatch
lifecycle. Unsupported primitive, picking-mask and clip APIs reject explicitly.
Colored-only DrawQuad remains unavailable; textured opaque quads are this slice.

FlushOrdered consumes the shared run's First/Count and vertices. The backend
validates and copies one immutable run packet, without repacking geometry,
sorting, changing texture indices or deduplicating slots. A run produces one
native indexed draw. The shared six-index pattern is expanded to eight quads;
vertex base offsets 0..28 and all 48 indices are exercised. First is honored as a
byte offset in native encoding; this quad-only coalesced path naturally starts
its run at zero. Nonzero run starts from mixed primitive runs are not qualified.

The existing native encoder now sizes its immutable upload for the run: camera
at zero, vertex address at 256, 512-byte tier-2 descriptors at the next 256-byte
boundary. Binding zero camera and vertex stream binding 16 remain unchanged;
fragment binding zero contains all 32 texture IDs followed by 32 sampler IDs.
QuadVertex remains 64 bytes with signed entity ID at 44 and clip rectangle at 48.
The unchanged B1 artifact/reflection checks validate this ABI and attachment
profile. No shader equations, compiler path, shader/cache behavior or IBL changed.

Frame packets retain shader/library/pipeline, texture/sampler snapshots, native
vertex/uniform/index/descriptor buffers, argument tables, attachments, residency
and readback storage until M1's ordered completion plus commit feedback. The
same three allocator slots, queue, shared event, barriers, budgets and drain are
used. A busy slot returns null without reusing its allocator or consuming the
packet. CPU-only readback handles remain stable after native resources retire.

Quad-only Shutdown discards unflushed CPU work and closes its active backend
scene; the explicit frame owner cancels remaining unsubmitted frame packets.
Already submitted frames remain completion-retained. Shutdown/reinitialize works
without starting text services. Device shutdown still drains pending work before
releasing cache ownership. Call Renderer2D::Shutdown before RenderCommand::Shutdown,
as for normal Renderer2D ownership.

Validation failure rejects the current chunk before publishing a run, resets its
CPU batch and leaves the scene usable. Earlier flushed runs remain intact. The
single coalesced quad run is the only supported run kind. The ninth quad can be
CPU-admitted after a capacity flush, but its flush rejects the unchanged eight
quad per-frame bound; no profile extension is implied. QuadCount/GeometryCount
count admitted geometry (including a later rejected chunk); DrawCalls counts
successfully enqueued runs, Flushes counts successful nonempty chunks, and
UploadedBytes counts quad vertex bytes, matching the existing statistic's scope
rather than claiming total native allocation/upload traffic. SubmitMilliseconds
is CPU enqueue time, not GPU performance evidence.

## Native evidence

The maintained arm64 host is Apple M4, macOS 27.3 build 26C5035r, Xcode 27.3
build 27C5020n. Debug and Release execute source-free shader packages with
PATH/DEVELOPER_DIR unavailable, Metal API validation and shader validation enabled.
No window or drawable is created. These are GPU-completed offscreen submissions,
not presented frames or end-user renderer support.

Per configuration:
- 21 submitted/completed frames; 18 allocator slot reuses; peak in-flight three;
  three gated submissions pending during shutdown; zero Metal errors.
- 226,368 peak frame-owned bytes, 7,745,064 peak allocator bytes; zero remaining
  frame-owned bytes after drain. Budgets remain 1 MiB / 16 MiB. As in B2,
  external owner storage, shader/driver internals and argument-table implementation
  storage are excluded; these are not total process/GPU memory measurements.
- Five saved full 32×32 color/signed-ID/depth fixtures independently decoded by
  Python: camera/versions, FP32 bilinear, repeated sampling, ordered shared runs,
  full eight-quad capacity. 5,120 comparisons each configuration, maximum color
  and depth error zero, IDs exact. All saved Debug/Release byte hashes match;
  the earlier retained readback remains identical after allocator reuse.
- Empty flush/end produces zero draw calls/uploads; clear-only submission verifies
  all 1,024 pixels. Three same-batch quads with texture updates before flush retain
  their original versions and equal-depth painter order. A further flush in that
  scene preserves prior immutable uploads. The ordered fixture records four
  admitted quads, two indexed draws, two flushes and 1,024 vertex bytes.
- Full-capacity fixture records nine CPU-admitted quads, one successful capacity
  flush/draw and 2,048 vertex bytes: eight render correctly; the ninth's rejected
  flush discards only that chunk. A subsequent EndScene succeeds and the earlier
  run submits correctly. Invalid finite-value validation likewise permits a new
  valid chunk within the same scene.
- Twelve successive frames alternate mirrored geometry; three gated pending
  frames retain dropped texture owners, reject busy allocator reuse and drain
  correctly at shutdown. Weak native texture versions expire on completion;
  shader ownership expires after Renderer2D shutdown and device/cache drain.
  Shutdown with unflushed work followed by explicit frame cancellation and
  reinitialization also passes. Unsupported/nested/duplicate APIs reject clearly.

The original Quad32 artifact SHA-256 remains
`9b27836ce452e162809395fd22dbf07e424f79d1ecbd7b4ab4c2fd4ef8747a78`.
All ten packages are unchanged from B1/B2. The production MetalShaderSmoke
loading/cache/ownership regression passes Debug/Release, including cache
cold/hit/corrupt recovery, validation errors and source-free loading. No developer
compiler is invoked by the native runtime tests.

Early focused experiments are retained rather than hidden: the first InitQuads
white-texture specification omitted an explicit non-mipmap filter and correctly
failed validation. It now selects nearest explicitly. The first new depth-order
fixture incorrectly used identity-camera z=-0.5 for a supposedly farther quad;
its oracle rejected the overwrite. The fixture now uses z=+0.5 (depth 0.75).
Neither experiment changed shader mathematics or tolerance. The final before-flush
texture update test also prevents regression to flush-time resource capture.

## Reproduction

Use the unchanged B1 producer only if prebuilt artifacts are unavailable, then
run the existing focused runner (extended for B3 rather than duplicated):

```sh
python3 scripts/macos/metal_shaders.py --config Debug --build --output build/metal-quad-resources/shaders/metal
python3 scripts/macos/metal_m2b2.py --config Debug --build --resources build/metal-quad-resources --output build/metal-m2b3-evidence/debug-final
python3 scripts/macos/metal_m2b2.py --config Release --build --resources build/metal-quad-resources --output build/metal-m2b3-evidence/release-final
```

Fresh evidence directories are required. Actual acceptance runs reuse
`build/metal-m2b2-evidence/Resources`. Build regression targets through the existing
Premake producer:

```python
import sys
sys.path.insert(0, 'scripts')
import hazel
names = ['ShaderToolsSmoke', 'RendererSmoke', 'RendererFeaturesSmoke',
         'Renderer2DSmoke', 'SceneGPUSmoke', 'SpriteSmoke']
for configuration in ['Debug', 'Release']:
    hazel.build(configuration, tests=True,
        targets=['Migration' + name for name in names] + ['MetalShaderSmoke', 'MetalM1Smoke'])
```

```sh
python3 scripts/hazel.py test --config Debug --profile native --only ShaderToolsSmoke --only RendererSmoke --only RendererFeaturesSmoke --only Renderer2DSmoke --only SceneGPUSmoke --only SpriteSmoke
python3 scripts/hazel.py test --config Release --profile native --only ShaderToolsSmoke --only RendererSmoke --only RendererFeaturesSmoke --only Renderer2DSmoke --only SceneGPUSmoke --only SpriteSmoke
python3 tests/macos/sdk_workflow.py
python3 tools/agents/check_public_docs.py --root .
```

MetalShaderSmoke receives an isolated copy of the resource root and a fresh
user-data directory per configuration; the test intentionally exercises invalid
packages. Exact source, artifact/readback hashes and local raw evidence paths are
recorded in [metal-m2b3-results.json](metal-m2b3-results.json).

## Acceptance — PASS

Debug/Release native Metal gates above pass. Six focused regressions pass in each
configuration: ShaderToolsSmoke (CPU toolchain), SpriteSmoke, RendererSmoke,
RendererFeaturesSmoke, Renderer2DSmoke and SceneGPUSmoke (native Apple OpenGL).
Renderer2DSmoke covers its existing full primitive/texture-slot/capacity and
ordered/picking behavior. Other suites were not rerun. M1's diagnostic executable
is rebuilt in both configurations to check the changed thin backend interface;
presentation is not executed or claimed by this checkpoint.

Direct inclusion of Renderer2D and affected native headers passes C++17
-Wall/-Wextra without a PCH. Seven SDK workflow tests pass. New/changed C++ files
produce zero warnings. Retained build logs contain pre-existing scene/component
copy/unused-parameter, SpriteSmoke expression, vendor/archive and Xcode build-phase
warnings; no suppressions or unrelated cleanup were introduced. Warning inventory
is `build/metal-m2b3-evidence/warnings.json`. Premake remains authoritative; no
generated IDE file is changed.

The development guide now points to this initialized shared flush checkpoint and
keeps experimental limits explicit. README/default OpenGL, examples, screenshots,
platform prerequisites and CI triggers remain unchanged. The static public-docs
health check passes; no end-user Metal support is claimed. Feature-branch CI is
NOT RUN (the workflow runs master pushes, PRs or explicit dispatch); no dispatch
or merge is performed. Windows/Linux native compilation/execution and physical
Intel GPU coverage are NOT RUN; local Docker's daemon is unavailable. Source
platform guards and Premake macOS exclusions are inspected, not presented as
native cross-platform build evidence.

Remaining limits: eight total opaque textured quads, fixed tiny offscreen target
profile, no other primitives or general buffer/vertex-array/framebuffer factories,
no picking masks/primitive clips, no generalized passes, scene integration,
blending or presentation. RGBA8 linear interpolation's B2 precision limitation is
unchanged; strict linear tests explicitly use RGBA32F input. Mixed-primitive
nonzero run starts are unqualified. M1 device-loss/unsignaled completion timeout
still aborts rather than releasing potentially live GPU resources. No shader math,
PBR/IBL model or frozen reference changes are included.

## Smallest next production integration slice

Connect the same bounded offscreen attachment/frame lifetime to Hazel's existing
Framebuffer ownership, retaining the exact qualified formats, quad-only ordered
submission, eight-quad bound and completion rules. Prove bind/clear/submit/readback
and cancellation through that resource boundary before expanding attachment
profiles, batch capacity, scene/editor integration or presentation. This is a
recommendation only; no next checkpoint is implemented.
