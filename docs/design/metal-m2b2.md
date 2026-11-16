# Metal 4 M2-B2: first production-owned offscreen Renderer2D quad

Baseline `15604e238fc6bfa6ab6cc071055fde6e21886e1a` on
`feature/3d-foundations`, synchronized with origin. Four unrelated untracked
.DS_Store files are preserved. This slice adds no presentation, editor, full
batching, circles/lines/text, Renderer3D, generalized passes or new effects.

Predeclared gates:
- Use existing Renderer2D BeginScene/DrawQuad/DrawSprite/EndScene entry points and
  shared transformed quad/UV/index packing. No test-side geometry/Metal encoder.
  Do not initialize unrelated font/text/circle/line factories for this slice.
- Production Texture2D factory supports an explicit bounded generated linear-encoded RGBA8/RGBA32F,
  mip-free texture/sampler subset (filtered RGBA32F requires native capability). Unsupported specifications fail, never degrade.
- Use the M2-B1 32-slot .hzmetal shader and single-sample RGBA32Float/R32Sint +
  Depth32Float offscreen profile only: depth less/write, two-sided, blending disabled
  and opaque inputs only. Target/texture extents 1..32, at most eight
  independent indexed quad draws per submission; no implicit batching or sorting.
- M1 device/queue/three allocator slots/shared completion timeline own submissions.
  Immutable per-draw vertex, uniform and descriptor versions plus texture/sampler
  snapshots survive through GPU completion. No allocator/storage reuse while busy.
- Debug/Release native deterministic readback: camera/world transforms, nonconstant
  UV pattern/orientation, nearest and linear sampling, exact signed IDs and clear,
  depth ordering and clear depth; absolute FP32 error <=1e-6, exact discrete values.
- Two updates of the same texture and camera within one submission preserve earlier
  draws. Gate three submissions in flight, reject busy-slot reuse, then execute at
  least twelve successive submissions and verify old results remain stable.
  Pending-work shutdown drains and releases frame resources; API validation clean.
- Relevant OpenGL regressions Debug/Release, headers/warnings and documentation
  health checks. Unavailable native platform coverage is reported honestly.

Shared seam: full Renderer2D::Init requires unrelated font/text/circle/line and
complete buffer/vertex-array factories. Instead the existing quad entry points
route their bounded Metal work to the renderer backend, sharing CPU packing with
OpenGL. The backend owns the offscreen frame contract; MetalDevice owns native
encoding and completion. Normal scene-renderer initialization remains unsupported
on Metal, and production applications remain OpenGL by default.

## Initial numerical finding

The first nearest/camera/version/depth fixture passes all 1024 pixels exactly.
A varying-UV RGBA8 linear-filter fixture produces a maximum 9.191036224365234e-5
color difference from ideal FP32 bilinear interpolation (red at pixel 13,12),
while alpha remains exactly 1. Raw diagnostics are retained in
`build/metal-m2b2-evidence/debug-diagnostic/linear.bin`. No tolerance or shader math
was changed. The strict linear precision fixture therefore explicitly selects
RGBA32F input, gated by `supports32BitFloatFiltering`, alongside the existing
RGBA8 nearest fixture. The attachment profile stays RGBA32Float/R32Sint/Depth32Float.
RGBA8 linear filtering retains this measured precision limitation; no universal
FP32 interpolation accuracy is claimed for normalized 8-bit inputs.

The explicit unblended profile rejects non-opaque texture uploads and tints;
alpha blending/discard parity is not claimed. Native culling is disabled, matching
Renderer2D's two-sided world pass; successive submissions alternate positive and
negative X scale to exercise mirrored geometry through the same shared packing.

## Ownership and shared-path coverage

`Renderer2D::DrawQuad(transform, texture)` still delegates to `DrawSprite`.
`BuildRenderer2DQuad` and `Renderer2DQuadIndices` are shared with the unchanged
OpenGL batching path. Quad world coordinates, sprite corners/UVs, tint, tiling,
entity IDs and vertex ABI have one CPU packing implementation. Metal's existing
BeginScene camera overloads route to the bounded backend scene state; EndScene
ends that scope. There is no second scene traversal or authored material model.

Full Renderer2D initialization, ordered-run batching, text/font/circle/line
services, statistics and generalized buffer/vertex-array factories are not
covered. The offscreen seam deliberately stops before those unrelated services.
The native test calls the real Renderer2D entry points without Renderer2D::Init;
normal Renderer::Init drawing mode on Metal remains unavailable. Generic
RenderCommand draw/state/Framebuffer APIs are not newly enabled. This is not
scene-level or end-user Metal support.

`MetalRendererAPI` owns a bounded unsubmitted packet. Each quad is an individual
indexed draw, not a batch. Camera/vertex values and immutable texture/sampler
versions are captured at draw time. `Texture2D::Create(specification)` resolves to
a production Metal texture only for the explicit generated subset; file loading,
sRGB, mipmaps and other image formats remain unsupported. SetData replaces a
version rather than modifying storage already captured by a draw. Uploads require
complete images and opaque alpha; sampler snapshots retain their own native state.

`MetalDevice` encodes into its existing three allocator slots and reusable native
MTL4 command buffer, with the same queue, monotonically ordered shared event and
shutdown drain used by M1. Submission/poll mutation is private to the backend and
uses the existing single render-thread ownership. There is no second queue or
separate retirement ring. A busy slot returns no result and leaves the CPU packet
unchanged for retry/cancel. Per-draw immutable 1024-byte uploads contain aligned
camera, vertex and 512-byte tier-2 descriptor slices. Vertex and fragment argument
tables are distinct and retained; the fixed six-index buffer and all target,
readback and sampled texture allocations belong to the pending frame. Shaders,
libraries, pipelines, depth state and samplers remain strongly owned through
completion. All 32 texture/sampler entries are initialized to the single captured
texture; vertex texture index is zero.

Device-visible render-to-copy barriers precede readback. CPU result arrays stay
empty until both ordered GPU completion and commit feedback retire. Polling copies
readback to CPU-owned result vectors, then releases frame-native resources. Error
feedback is surfaced and does not publish successful pixel data. Old result
handles therefore retain CPU data only and cannot pin native ring allocations.
Shutdown cancels unsubmitted work, drains pending GPU work, polls completed
results, then releases the shader cache. M1's timeout/abort policy remains; a
timeout never authorizes freeing in-use GPU resources.

Frame-owned allocation accounting remains inside M1's 1 MiB resource and 16 MiB
allocator budgets. It conservatively counts shared texture allocations per pending
frame. External texture-owner storage, shader/driver internals and argument-table
implementation storage are not included in this frame counter. No performance
improvement or total process/GPU-memory claim is made.

Readback row indices preserve M2-A's numeric OpenGL framebuffer convention:
logical bottom rows have small y, despite Metal's physical top-left target origin.
The common sprite UV convention maps source top-left rows to upper logical quad
vertices. Final display composition is outside this offscreen checkpoint. The
fixed pass uses depth less/write, clear depth 1, signed clear -1, no culling and
no blending. Attachment formats/sample count cannot be supplied or silently
changed by callers.

## Reproduction

On the maintained native arm64 macOS 27+ / Metal 4 host, prepare the unchanged
M2-B1-qualified shader packages if they are not already available:

```sh
python3 scripts/macos/metal_shaders.py --config Debug --build --output build/metal-quad-resources/shaders/metal
python3 scripts/macos/metal_m2b2.py --config Debug --build --resources build/metal-quad-resources --output build/metal-m2b2-evidence/debug-final
python3 scripts/macos/metal_m2b2.py --config Release --build --resources build/metal-quad-resources --output build/metal-m2b2-evidence/release-final
```

Fresh evidence directories are required. Runtime execution uses precompiled
`.hzmetal` resources with PATH and DEVELOPER_DIR unavailable, API validation and
GPU shader validation enabled. It creates no window or presentation surface.
The runner independently decodes every pixel of saved color/int/depth records,
checks numeric error <=1e-6, exact IDs and readback stability after slot reuse.

## Final acceptance — PASS

Both configurations execute 18 GPU-completed offscreen submissions, including
12 successive submissions alternating normal/mirrored geometry and three gated
submissions pending at shutdown. Each records 15 slot reuses, peak in-flight 3,
zero Metal errors, 225,480 peak frame-owned bytes, 7,745,064 peak allocator bytes
and zero remaining frame-owned bytes after retirement. A fourth submission while
all slots are gated is rejected without allocator reuse; its CPU packet can be
cancelled. Native texture versions remain alive while pending and expire after
retirement; the shader cache releases only after drain. These are completed
submissions, not positively presented frames.

Independent Python analysis checks all 1024 pixels of each of three readbacks per
configuration (3072 comparisons): camera/world/texture versions with depth/clear,
FP32 linear filtering, and nearest repeated sampling with tiling factor two.
Maximum color and depth error is zero; signed IDs and clear -1 are exact.
Debug/Release readback byte hashes match. The saved original readback remains
byte-identical after allocator-slot reuse. GPU API and shader validation are
active; the native test encodes no Metal commands itself.

Six focused regressions pass per configuration on the native Apple OpenGL profile:
ShaderToolsSmoke (CPU toolchain), RendererSmoke, RendererFeaturesSmoke,
Renderer2DSmoke, SceneGPUSmoke and SpriteSmoke. The separate production
MetalShaderSmoke loading/cache/ownership regression also passes in both builds.
Other suites were not rerun. Seven SDK workflow tests pass. Direct inclusion of
the new/native headers passes C++17 -Wall/-Wextra without a PCH. Changed files
produce zero warnings. Existing unrelated/vendor/archive warnings remain visible;
no blanket suppression or unrelated warning cleanup was performed.

Initial build diagnostics identified two include-boundary issues: the new native
quad test needs the existing public Renderer2D header's YAML dependency, and the
MetalRendererAPI interface must forward-declare native frame state to avoid leaking
Metal-cpp into existing M1 consumers. Premake's test include and the thin header
boundary fix those causes. Failed build and RGBA8 precision diagnostics remain
under `build/metal-m2b2-evidence/`; final runs are `debug-acceptance` and
`release-acceptance`. The shader packages are byte-identical to the validated
M2-B1 artifacts, consumed without runtime developer tools. No GLSL, OpenGL backend
shader/cache implementation, lighting equations or frozen references changed.

Source guards/Premake exclusions preserve Windows/Linux compilation boundaries;
native Windows/Linux and physical Intel GPU execution are NOT RUN. Docker's local
daemon is unavailable. Presentation, 2× window-system qualification, full scene
parity, game export and CI execution are outside this checkpoint and are not
inferred from the native offscreen tests. M1 device-loss/unsignaled completion
recovery remains unimplemented; its completion timeout still aborts rather than
freeing potentially in-use allocations.

Final commands use the prebuilt M2-B1 resource bundle:

```sh
python3 scripts/macos/metal_m2b2.py --config Debug --resources build/metal-m2b2-evidence/Resources --output build/metal-m2b2-evidence/debug-acceptance
python3 scripts/macos/metal_m2b2.py --config Release --resources build/metal-m2b2-evidence/Resources --output build/metal-m2b2-evidence/release-acceptance
python3 scripts/hazel.py test --config Debug --profile native --only ShaderToolsSmoke --only RendererSmoke --only RendererFeaturesSmoke --only Renderer2DSmoke --only SceneGPUSmoke
python3 scripts/hazel.py test --config Release --profile native --only ShaderToolsSmoke --only RendererSmoke --only RendererFeaturesSmoke --only Renderer2DSmoke --only SceneGPUSmoke
python3 scripts/hazel.py test --config Debug --profile native --only SpriteSmoke
python3 scripts/hazel.py test --config Release --profile native --only SpriteSmoke
python3 tests/macos/sdk_workflow.py
python3 tools/agents/check_public_docs.py --root .
```

The existing Premake producer built MetalRenderer2DQuadSmoke, MetalShaderSmoke,
MetalM1Smoke and selected Migration regression targets in Debug/Release through
`hazel.build(..., tests=True, targets=[...])`; no generated IDE files are source.
Source hashes, every changed file, exact measurements, artifact/readback hashes
and raw evidence paths are in [metal-m2b2-results.json](metal-m2b2-results.json).
The development guide documents this experimental offscreen seam without claiming
normal Metal renderer support. Public link health passes; README screenshots,
platform prerequisites, OpenGL defaults and CI/master badge policy are unchanged.

## Smallest next integration step

Qualify quad-only initialization and one ordered quad run through the existing
Renderer2D flush ownership, keeping the same offscreen target profile and
completion-retained versions. Add only the buffer/vertex-array adapters genuinely
required to reach that seam. Other primitives, blending, scene/editor rendering
and presentation remain separately scoped. No next slice or merge is performed
by this checkpoint.
