# XeGTAO — implementation and qualification record

Baseline: `86c581c13fd5c944c2a06d720314a753d7607d8f`,
`feature/3d-foundations`. Initial tree has an unrelated modified
`examples/RenderingReference/Assets/Reference/IBLQuality_v2/Materials/M0R3.hmat`,
untracked shader artifacts and Finder metadata. Preserve these; do not use the
modified material as frozen-reference evidence.

## Source-grounded integration decision

SceneRenderer owns world ordering and per-view framebuffer lifetime. Renderer3D
owns the single mesh/material traversal; MetalRendererAPI captures immutable draw
packets. MetalDeviceRenderer2D prepares arguments/residency and encodes the packet
into the existing Metal 4 submission, whose completion retains its resources.
ForwardPBR evaluates direct, diffuse IBL, specular IBL and emission separately.
The current color-compute ABI (one float input and one output) cannot represent
XeGTAO and must not be silently reused with incompatible resources.

Chosen order: masked mesh depth preparation → FP32 view-depth mip preparation →
XeGTAO horizon integral and edge output → spatial denoise → ordinary forward PBR
→ world 2D → world exposure/Neutral composition → screen UI. Reuse the mesh
traversal with a narrow depth shader, preserving material masks and culling.
Reconstruct geometric normals from depth using upstream edge-weighted neighbors;
no extra authored normal buffer, deferred renderer or alternate scene traversal.
Per-view working textures must be completion-retained and reused only after their
prior submission retires. Barriers cover render-to-compute, every dependent mip,
main-to-denoise and denoise-to-fragment. Final forward IDs/depth remain authoritative.

Diffuse visibility combines material AO and screen-space AO by minimum, avoiding
multiplication of overlapping occlusion estimates. Screen-space AO does not alter
direct, emission, sky, UI or specular IBL. Existing material specular occlusion and
accepted single-scattering GGX/split-sum equations stay unchanged. AO-off must
preserve existing output. OpenGL has no AO implementation and must disclose this.

## Upstream adaptation

Intel XeGTAO, pinned commit `a5b1686c7ea37788eeb3576b5be47f7c03db532c`:
https://github.com/GameTechDev/XeGTAO . Reviewed XeGTAO.h, XeGTAO.hlsli and
vaGTAO.hlsl. MIT notice accompanies the adaptation.

Retain the weighted depth mip filter, squared radial distribution, R1 sample
jitter, Hilbert/R2 fixed spatial noise, normal-relative horizon bounds, analytic
slice integral, slope-aware symmetric edges and edge-path spatial denoiser.
FP32 depth/math initially avoids FP16 normal-reconstruction artifacts. No bent
normals: they add cost without a qualified directional-lighting consumer here.
No temporal noise or TAA dependency. Screen-space missing/offscreen/hidden geometry
and height-field thickness ambiguity remain inherent limitations.

Raster GLSL remains authoritative through shaderc/SPIR-V/SPIRV-Cross. That pipeline
currently accepts only vertex/fragment stages and rejects storage images. A narrow
offline MSL compute adaptation is preferable to adding a general HLSL runtime or
misrepresenting raster artifact support. Runtime must load packaged metallib only.

## Predeclared acceptance

All requested visual, native Debug/Release editor/player, motion/resize/1×/2×,
AO-off frozen PBR/IBL, exact signed IDs/depth, M1–M6 relevant regressions, OpenGL,
source-free packaging, failure cleanup and sustained resource gates are required.
Geometric cases include isolated plane, contact box, perpendicular corner, thin
occluder, masks, double-sided surfaces, perspective and orthographic projections.
Unsupported projection must fail explicitly. Numeric plane visibility, bounded
finite AO and independent geometric contact expectations supplement screenshots.

Serialized 720p/1080p AO-off and each quality benchmark must report warmup/sample
counts, CPU/GPU median/p95, pass timing scope, complete-frame time, memory and frame
pacing. No timing result can substitute for a missing pass measurement. Windows
historical full-suite timeout is separately deferred. New platform execution is
reported separately from static portability review.

Status: **INCOMPLETE — focused gates executed; see continuation checkpoint below.**

## Continuation checkpoint (2026-11-21)

Actual resumed HEAD is `4e01c5c1d361110fc8fcff4e8b9b755e3be23581`
(`Start of XeGTAO integration`), with a clean initial tree. That owner-created
commit contains the initial work and the unrelated material/Finder/generated
artifact changes. They are preserved. New work is a diff on this checkpoint.

### Adaptation details and discriminating experiments

- Upstream's finite slice integral exhibits a small view-dependent constant
  visibility bias on an isolated plane. The initial strict >=.97 plane gate
  failed. Hazel normalizes the sampled integral by the same analytic integral
  over the unobstructed tangent hemisphere before the upstream final power.
  Horizon sampling/integration, radius/falloff, R1 jitter, Hilbert/R2 pattern and
  edge-path filter remain XeGTAO. This normalization is an explicit adaptation,
  not an upstream-equivalent byte port. The gate is unchanged.
- The first implementation used a queue producer barrier between dispatches in
  one encoder. Native captures exposed zero-valued tiles. This was corrected to
  `barrierAfterEncoderStages`; render/compute boundaries retain queue barriers.
  Metal API/GPU validation alone had not caught the missing dependency.
- The first MSL compile had three syntax errors; the first box-contact fixture
  placed the sampled front face outside the AO radius. Both failed attempts
  remain in `build/xegtao-evidence/`. The reference ray test initially compared a
  hidden floor point to a visible box pixel near the silhouette. It now traces
  the nearest primary surface and evaluates the same 3x3 pixel footprint.
- Initial matched high-preset main-pass medians were 2.784 ms (720p) and
  6.192 ms (1080p), Debug/native M4, 579 samples per extent after warmup/tail.
  This exposed repeated inverse-matrix reconstruction per horizon sample. The
  current constant block supplies the equivalent per-view UV-to-view mapping.
  One subsequent exploratory benchmark overlapped a validation fixture at its
  tail and is excluded from final performance claims.
- The mesh preparation framebuffer now allocates/stores **depth only**. It
  neither allocates nor writes color/picking attachments. The final forward pass
  still owns authoritative depth and IDs. The per-view depth framebuffer owns a
  completion-safe AO resource pool shared by shaded/inspection consumers.
  Counter heaps are reused with each retired generation; idle and in-flight AO
  textures participate in the existing 1 GiB native resource admission budget.
- Low/medium/high/ultra use 4/8/18/54 horizon samples per pixel at full physical
  resolution. Low/medium use two edge-aware passes; high/ultra use three to
  support spatial stability without TAA. Constants/math/depth are FP32; scalar
  working visibility is R16F and edge weights are RGBA8. Bent normals and
  screen-space specular occlusion are not implemented. Material specular AO
  remains unchanged; diffuse combines material and GTAO visibility by minimum.

### Focused executed evidence

`build/xegtao-evidence/depth-only-debug/` passes native API/GPU-validated plane,
contact, fixed-frame determinism, perspective/orthographic, masked geometry,
camera/object motion, exact direct/ID/depth preservation, diffuse/specular IBL
separation, singular-camera rejection/recovery, odd/one-pixel resize, three gated
pending frames, view destruction before GPU completion, and final retirement.
The independent physical-radius cosine hemisphere reference uses 16,384 rays per
pixel in eight 3x3 probes. RMS visibility error is .0541 against the predeclared
.15 bound. This is a contact fixture, not a general ray-traced ground-truth pass.

The later maintained fixture additionally checks depth-only API rejection,
reprojected contact variation during camera orbit/translation/dolly (range <=.15),
and exact screen UI preservation. These later additions require their own fresh
runs; the earlier focused result does not cover them.

### Frozen inputs and qualification provenance

The owner's checkpoint changed `IBLQuality_v2/Materials/M0R3.hmat` relative to the
requested baseline. Tests must not reset that file or bless its new bytes as a
frozen reference. `HAZEL_TEST_EXAMPLE_REVISION=86c581c13fd5c944c2a06d720314a753d7607d8f`
selects explicitly logged private example snapshots for frozen regression tests.
`Renderer3DParity` honors the existing `HAZEL_RENDERING_REFERENCE_PROJECT`
selector so Metal and OpenGL consume that same private snapshot. No second source
checkout is created. The new AO demonstration uses the current authored study,
not a changed frozen scene or reference generation.

**INCOMPLETE:** broader Debug/Release/native product, temporal, 1x/2x, shader/cache,
AO-off frozen, OpenGL, relocated export, warning and performance gates are still
being executed. Windows historical full-suite timeout remains separately deferred.
No completion commit/push or full feature PASS is claimed at this checkpoint.
