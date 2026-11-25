# TAA v1 — implementation and qualification

Owner subsequently **rejected geometric-edge quality at `4c25138`**. The initial
records below describe that checkpoint; they do not establish visual acceptance.
See the [focused investigation and correction](taa-edge-investigation.md).

Baseline: `3a0f71cb13728a6802ad1c9eaadf6ed384d8e4a0`, clean
`feature/3d-foundations`, Apple M4 / macOS 27.3. Status: INCOMPLETE.
Owner manual visual acceptance is required and has not occurred.

Current results, commands, moving captures and measured costs are in the
[qualification record](taa-qualification.md) and
[measurement record](taa-measurements.json). Sections labelled checkpoint below
preserve intermediate evidence; their pending/blocked statements describe that
checkpoint, not the final gate status.

## Source audit and contract

SceneCollection collects immutable static mesh assets and entity transforms;
Renderer3D is the sole material/mesh traversal. SceneRenderer orders depth/AO,
forward radiance, world 2D, Neutral presentation and screen UI. SceneRenderView
owns view targets. MetalFramebuffer captures packets; MetalDeviceRenderer2D
prepares residency and encodes nested producers into the three-slot Metal 4
queue. Completion retains packets and immutable attachment generations.

Use Halton(2,3) bounded subpixel jitter only for mesh radiance and its AO depth
producer. Input/culling/picking/motion/world-2D/UI retain the unjittered camera.
A narrow motion shader uses current and previous rigid model transforms and
unjittered camera matrices, sharing masks/culling with forward rendering.
Resolve reconstructs jittered HDR at the unjittered pixel grid, reprojects rigid
motion, validates depth and signed IDs, clips history in compressed YCoCg and
adapts contribution to motion and luminance changes. History is pre-exposure;
world 2D and UI never enter it. No sharpening or independent AO history.

History resource identities are completion-retained; private contents are reused
only after queue barriers retire all earlier GPU readers/writers. Packets retain
both ping-pong generations; final presentation/picking targets remain versioned. CPU state
is accepted only with a submission token published after queue commit. Cancelled
packets therefore invalidate history. No extra CPU GPU wait is authorized.

Rigid meshes only: skinning, morphs, vertex deformation, transparent 3D and
animated material correspondence are outside this renderer's current mesh
contract. New/replaced meshes reject history. Per-view histories cannot be
shared across scenes/projects. Large camera discontinuities, invalid matrices,
settings, extent and backend changes reset history. Explicit Reset handles cuts.

## Predeclared gates

1. Debug/Release builds, shader reflection/package identity, no new first-party
   warnings; OpenGL unchanged when disabled/unsupported.
2. Native API/GPU validation: perspective/orthographic, camera/object motion,
   cuts, disocclusion, finite HDR, exact disabled output and unjittered signed
   IDs/depth, independent views, resize, cancellation and retirement.
3. Moving sequences: thin edges, silhouettes, fine texture, specular highlights,
   static convergence, no persistent trails/leaks. Quantitative registered motion
   comparisons and captures; owner manual review remains required.
4. AO off and Low/Medium/High/Ultra off/on; frozen PBR/IBL unchanged disabled.
5. Native editor/player repeated launches, 1x/2x, minimization/invalid drawable,
   three pending slots, failure recovery and bounded allocations.
6. Source-free exports/relocation, full relevant Metal/OpenGL suites, docs audit.
7. Serialized 720p/1080p matrix: CPU/GPU median/p95, resolve interval, memory,
   reuse and pacing. Baseline `xegtao-measurements.json`. GPU completion intervals
   may overlap and must never be added as exclusive costs.

No PASS or qualified commit/push until required executed gates are recorded;
manual visual acceptance is reported separately. Windows historical full-suite
 timeout remains deferred. Raw evidence belongs in `build/taa-evidence/`.

## Focused checkpoint

`build/taa-evidence/native-debug-study/` passes native API/GPU validation of the
initial resolve: 160 synthetic perspective/orthographic motion frames and 320
authored AO-study frames across Off/Low/Medium/High/Ultra. Exact signed IDs/depth
and screen UI are unchanged. Cancellation, independent views, odd/1-pixel resize,
three gated mesh submissions and retirement pass. This checkpoint precedes the
sky-reprojection addition; it does not qualify that later code. Captures are
native offscreen readbacks, not editor/player screenshots or owner acceptance.

Upstream temporal sampling reference:
[vaGTAO.hlsl at the pinned Intel commit](https://github.com/GameTechDev/XeGTAO/blob/a5b1686c7ea37788eeb3576b5be47f7c03db532c/Source/Rendering/Shaders/vaGTAO.hlsl#L64-L76).
The temporal Hilbert offset is `288 * (frameIndex % 64)`; zero retains the previous
fixed spatial noise. AO depth reconstruction uses physical camera forward under
jitter, with the off-centre UV-to-view scale/bias. TAA-off retains prior arithmetic.

The initial registered-floor experiment compares captures four scene steps apart,
reprojects static ground using perspective depth and camera translation, rejects
mixed-ID/depth neighborhoods and measures luminance RMS. Physical AO changes
from the moving occluder remain in the signal, so it is not pure noise or ground
truth. Initial Debug study ratios (on/off) are .805 Low, .783 Medium, .872 High,
.902 Ultra; AO-off .946. This supports a local reduction in residual variation,
not an overall quality PASS or evidence that Medium replaces Ultra universally.
`native-debug-study/registered-comparison.json` retains counts and per-pair values.

## Review checkpoint

Adversarial source review traced successful submit/retire, busy-slot cancellation,
failed allocation/pipeline creation and skipped-drawable paths. Corrections:
noncopyable/movable view ownership; backend-device recreation detection;
physical-extent jitter culling guard; account idle framebuffer/history storage in
the admission budget; reusable history-owned counter heaps; rotation-aware sky
reprojection; clip history variance without suppressing the current highlight.
No extra CPU GPU wait, render graph, AO history or PBR/IBL equation change.
Fresh final-tree execution is required after these corrections.

## Historical environment gate

`build/taa-evidence/console-preflight.json` reports a locked CG session. Visible
editor/player interaction, positively presented 720p/1080p benchmarking, native
1x/2x lifecycle and packaged-product presentation are BLOCKED until the desktop
is unlocked. Offscreen completion measurements are a separate matched scope and
must not be substituted for those gates or compared directly to the presented
XeGTAO baseline. An asynchronous unlock request was sent while independent work
continues. No qualified commit/push is inferred from partial checks.

Timing diagnostics include radiance shading (including AO when active), rigid
motion preparation and the resolve. Counter boundaries are GPU completion
intervals, may contain synchronization/overlap, and are not additive exclusive
costs. With TAA active, the AO forward interval excludes the later world-2D
composition. Retained history-color memory excludes other view targets; device
accounting includes idle/in-flight framebuffer pools and AO storage.

The owner unlocked the desktop; `console-unlocked.json` passes the native-console
preflight. Previously blocked visible gates can now run. The final focused Debug
motion fixture additionally compares raster previous-UV/depth against independent
CPU reconstruction from authoritative current depth: 4,394 perspective and 7,265
orthographic pixels, worst .001965/.001588 pixel correspondence error and
2.385e-7 normalized-depth error. It also covers low-contrast fine texture, alpha
masks and photographic specular motion.

Review found a high-HDR precision ceiling if the current signal were round-tripped
through compressed clipping coordinates. Accumulation now retains original
scene-linear current/history radiance when in range; compression guides clipping
and responsiveness only. Linear relative-luminance change also controls response.
A high-radiance/low-exposure boundary probe is added; fresh native runs are needed.
The first final fixture run passed temporal lifetime then attempted a second use
of the device's deliberately single-use test queue gate. The harness now uses
one temporal gate in that process; this was a test sequencing failure.

## Measured 2x storage correction

The first 2x run (2560x1126 physical pixels after window-manager clamping) hit the
unchanged 1 GiB aggregate budget after two submissions. GPU retirement was clean;
it was a real storage-admission failure, not a validation/presentation PASS.
The private temporal pipeline now uses one queue-ordered jittered target and one
motion target per view, plus two HDR/depth/exact-ID histories. Radiance/depth/IDs
for final world 2D, presentation and picking keep ordinary completion-owned
attachment generations. No external consumer can submit private scratch.

Every scratch render begins with an all-stage queue visibility barrier; the
resolve also waits on prior queue readers/writers. A history resolve reads one
texture and writes the other. Metadata/depth lives in FP32 HDR alpha and exact
signed IDs in a separate R32Sint history texture. Timing heaps have an independent
four-entry completion-owned pool, so CPU invalidation never races an older GPU
submission. Completion retains all referenced resource identities. No CPU wait
was added. Resize drops only disposable retired caches before admission; submitted
packets retain old extents. Disabling TAA releases its private view resources.

`queue-storage-native-debug/` passes fresh native motion, HDR, AO, detail/mask/
specular, IDs/depth/UI and retirement checks. Its lifetime test warms history,
gates three pending successors, checks each completed frame against the FP64
Neutral oracle with distinct captured colors, exercises a busy fourth CPU packet,
and destroys the view before completion. This explicitly tests queue-ordered
scratch/history reuse rather than merely allocation survival. Fresh product,
2x and performance gates are still required after this correction.
