# Camera motion blur

Phase 4 native rendering checkpoint on Apple M4, macOS 27.3. The effect
is opt-in on the active camera.
Seconds mode reads the same physical shutter duration that contributes to
EV100. Angle mode selects the fraction `angle / 360°` of the actual time
between the two source frames. It controls optical blur without changing
the camera's manual exposure triangle. Zero degrees, invalid elapsed time
and camera cuts disable the blur for that frame.

For unjittered current and previous image positions `p` and `pPrev`, in
physical pixels, the signed shutter displacement is
`(p − pPrev) × integrationSeconds / frameSeconds`. This is the image
velocity under locally constant motion. The global shutter is centered
on the rendered time, with endpoints `p ± displacement / 2`. Bound the
half-displacement by the quality preset's support. The shape is verified
against independently time-sampled constant-motion trajectories.

Preserve the scene-linear TAA history. Reuse its unjittered rigid-mesh
correspondence when available. The spatial path uses the same alpha
mask and previous-position shader with a private reduced-resolution
correspondence target. Full-resolution entity and depth identify where
the reduced samples apply. For other visible world content, reconstruct
camera-only velocity from the original world depth where possible. This
qualifies moving meshes and camera motion without requiring two renderers.

The reconstruction uses tile and neighboring velocity maxima, axial-depth
separation and a bounded shutter gather. The gather samples a fixed set of
times in the shutter window. Each candidate's own velocity determines whether
it covers the receiver at that time. The sampled colors are averaged over the
entire shutter window, including stationary background where a foreground
surface moves away. A single visible surface and two
moments of its motion cannot reconstruct all occluded surfaces or nonlinear
trajectories. Test low-frame-rate movement, silhouettes and missed thin
features explicitly. [McGuire et al., A Reconstruction Filter for Plausible Motion Blur](https://casual-effects.com/research/McGuire2012Blur/index.html)

Meter TAA/world HDR before optics. Reconstruct motion before depth of field
and bloom. Both optical effects refer to the same original camera/depth
image. Composite screen UI after the world image. Combined sampling order
requires independent acceptance. Measure the phase itself and paired frame
impact at 720p, 1080p and 4K. At 4K, the reduced correspondence target and
FP32 reconstructed layer use about 79.1 MiB before the tile maps, within the
96 MiB phase scratch cap. Metal retains attachment generations through
completion. [Camera and image formation](camera-image-formation.md)

## Qualification

The independent CPU shutter test covers 24, 30, 60 and 120 Hz, invalid elapsed
time, zero angle and versioned persistence. The first native moving stripe
case exposed over-weighting of visible foreground samples. The revised
full-window integration measures 8.158 pixels of support against 8.000
expected on both backends. Maximum endpoint error is 0.1495 pixels and
integrated emitter energy is 1.0173× the sharp reference. The inputs were an
orthographic rigid object moving 16 pixels per frame and a 180° global
shutter. A cut preserved picking and depth.

The expanded native study also exercises stationary geometry under camera
translation and rotation and fixed-duration captures at 24 and 120 Hz.
The final stripe measurements subtract the fixture's nonzero background.
The maximum supported endpoint error was 0.673 pixels in Balanced and
0.227 pixels in Cinematic, including the viewport's matched camera and
moving geometry cases. The 32-pixel fast-motion step at 24 Hz measured
15.713 pixels of shutter support against 16.000 pixels expected.
Simultaneously integrating
shutter and pupil gave a 3.045-pixel horizontal shutter half-width
against 4.000 pixels expected and an 8.705-pixel aperture radius against
8.441 pixels expected. A moving gallery sequence with automatic exposure,
DOF, SSR and bloom preserved picking, depth and cut behavior.
The native tests verified screen edges and UI coverage. In the Metal queue
test, three gated frames matched their serial rendering byte for byte
after a reset and odd-extent resize.
[Metal shutter-support crop](evidence/CameraMotion_v1/metal-shutter-support.png)
[OpenGL shutter-support crop](evidence/CameraMotion_v1/opengl-shutter-support.png)
[Metal combined capture](evidence/CameraMotion_v1/metal-combined.png)
[Metal matched shutter-off capture](evidence/CameraMotion_v1/metal-combined-shutter-off.png)
[OpenGL combined capture](evidence/CameraMotion_v1/opengl-combined.png)

The first Balanced benchmark exposed both a vector-production cost on
the spatial path and expensive reconstruction with reused temporal
vectors. Separating moving-object IDs from static camera reprojection
reduced the cost. Static geometry uses the camera/depth transform and
need not be drawn again. A compact CPU bitmask admits only IDs that can
need a mesh correspondence. Hash collisions can admit extra lookups,
and the shader still checks the actual ID.

| Backend / extent | Initial isolated / paired median ms | Moving vectors only isolated / paired median ms |
| --- | ---: | ---: |
| Metal 4 720p | 0.724 / 1.481 | 0.545 / 1.126 |
| Metal 4 1080p | 1.245 / 2.366 | 0.912 / 1.648 |
| Metal 4 4K | 4.973 / 8.971 | 3.575 / 5.562 |
| OpenGL 4.1 720p | 0.830 / 6.072 | 0.530 / 0.840 |
| OpenGL 4.1 1080p | 1.257 / 7.140 | 0.954 / 1.167 |
| OpenGL 4.1 4K | 4.454 / 11.547 | 3.403 / 4.214 |

The moving-vector candidate used 85,131,264 native Metal attachment bytes
at 4K on its spatial path. OpenGL used 83,980,800 payload bytes. The
paired-frame test includes vector generation or reuse and world
composition, along with the backend's full supported effects. Metal
combined medians exceeded the phase budgets at all three extents.
[Initial Metal samples](evidence/CameraMotion_v1/metal-initial-timings.csv)
[Initial OpenGL samples](evidence/CameraMotion_v1/opengl-initial-timings.csv)
[Moving-vector Metal samples](evidence/CameraMotion_v1/metal-moving-vector-timings.csv)
[Moving-vector OpenGL samples](evidence/CameraMotion_v1/opengl-moving-vector-timings.csv)

Balanced supports five shutter samples and a half-width of 0.8% of image
height, capped at 24 physical pixels. Cinematic uses up to 17 samples
and 1.6% of image height, capped at 40 pixels. Final Balanced
measurements use 31 samples per case after four warm-up frames.
The two frame comparisons alternate the enabled and disabled mode on
matched warmed views. The phase budgets apply to the measured Balanced
case.

| Backend / extent | Isolated median / p95 ms | Paired added median / p95 ms |
| --- | ---: | ---: |
| Metal 4 720p | 0.480 / 0.557 | 0.658 / 1.292 |
| Metal 4 1080p | 0.645 / 0.694 | 1.067 / 1.466 |
| Metal 4 4K | 2.412 / 2.855 | 3.868 / 7.351 |
| OpenGL 4.1 720p | 0.604 / 0.784 | 0.569 / 1.809 |
| OpenGL 4.1 1080p | 0.888 / 1.306 | 0.831 / 2.575 |
| OpenGL 4.1 4K | 2.937 / 3.437 | 4.404 / 6.382 |

An earlier candidate with the same sample counts measured a 4.119 ms
Metal 4K paired median. Avoiding repeated square roots and divisions
for below-cap shutter vectors brought this within the 4.0 ms phase
median budget. The final Metal run completed 315 frames with zero GPU
errors. It reached 3,228,754,176 tracked attachment bytes across the
two paired views and retired all generations at shutdown.
[Final Metal samples](evidence/CameraMotion_v1/metal-final-timings.csv)
[Final OpenGL samples](evidence/CameraMotion_v1/opengl-final-timings.csv)
[Preceding Metal samples](evidence/CameraMotion_v1/metal-preceding-timings.csv)
[Preceding OpenGL samples](evidence/CameraMotion_v1/opengl-preceding-timings.csv)

The spatial path estimates missed half-resolution mesh samples from
matching neighbors, then uses camera-only motion when the object is absent
from that buffer. World sprites and other content without a tracked
rigid mesh use camera-only motion. Cinematic performance, native desktop
presentation of the relocated package and hardware outside Apple M4
require separate evidence.

Native Metal 4 and Apple OpenGL 4.1 passed the shutter, depth-of-field and
HDR bloom fixtures in both Debug and Release. The 144 frozen manual
captures are byte-identical. Scene serialization and targeted Inspector
Undo/Redo passed. The relocated signed Release archive passed its
source-free tests with active shutter blur on both backends. The Metal
test ran with shader sources and SDK access denied. The desktop console
was locked during this checkpoint.

The runtime fixture uses `--from-gallery --prepare-only --runtime-only
--automatic-camera --defocus-camera --shutter-camera`. Export it with
`scripts/hazel.py editor-export` and qualify the relocated archive with
`hdr_bloom.py --archive … --shutter-study`.

```sh
python3 scripts/hazel.py build --config Release --tests \
  --target MetalShaderTools --target MetalRenderer2DQuadSmoke --target MigrationRenderer2DSmoke
python3 scripts/macos/metal_shaders.py --config Release --output Hazel/Resources/shaders/metal
python3 scripts/macos/hdr_bloom.py --project 'build/hdr-bloom-project/HDR Bloom Study/HDR Bloom Study.hproj' \
  --config Release --shutter-study --output build/qualify-camera-shutter
python3 scripts/macos/hdr_bloom.py --project 'build/hdr-bloom-project/HDR Bloom Study/HDR Bloom Study.hproj' \
  --config Release --shutter-benchmark --output build/qualify-camera-shutter-timing
```
