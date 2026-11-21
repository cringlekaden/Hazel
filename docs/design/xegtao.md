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

Status: **INCOMPLETE — implementation in progress; no acceptance gates executed.**
