# Camera depth of field

Phase 3 native rendering checkpoint on Apple M4, macOS 27.3. The
qualification includes Balanced performance and Cinematic numerical and
visual fixtures.

Depth of field uses the active perspective camera's `PhysicalLens`. The
camera component enables it independently and owns quality, pupil shape and
the optional manual focus half-life. With no authored lens, the physical
reference camera supplies the same default optics used by manual exposure.
Orthographic cameras, the editor navigation camera, probe capture and
inspection views bypass defocus.

## Circle of confusion

`f`, `z` and `s` are focal length, positive axial object depth and focus
distance in meters. Hazel uses one meter per world unit for camera optics.
For each focus setting, the sensor lies at its conjugate image plane. A signed
sensor radius in meters is `(f / (2N)) × f × (z − s) / (z × (s − f))`.
Near blur is negative. Multiply by physical image height in pixels over
active sensor height in meters.

With depth-buffer sample `d`, the axial reciprocal is
`1/z = (2d − 1 + Projection[2][2]) / Projection[3][2]`, using GLM's
column-major indexing. The CPU computes the focus depth and slope in FP64.
The GPU uses `(d − focusDepth) × pixelsPerDepth`. Projection scale supplies
the active sensor height, `2f / Projection[1][1]`, both for a physically
cropped gate and an authored perspective FOV. Field of view stays fixed
during focus pulls. The test reference independently intersects the pupil
edge ray at the sensor plane.

A zero half-life uses the authored focus immediately. A positive half-life
interpolates the reciprocal focus distance exponentially using elapsed frame
time. A new view, a different camera/scene, a cut or a changed extent snaps
to the current focus. This state belongs to the individual render view.

The optional focus subject supplies axial depth from its world-space pivot.
Tracking ignores changes smaller than 2% of the requested optical power
or 0.002 inverse world units, whichever is greater. It moves at most
2 inverse world units per second and uses a 0.18-second half-life when
the half-life control is zero. Losing the subject returns to authored
manual focus under the same rate limit. A cut snaps to the current valid
subject or manual distance. Scene and prefab copies remap local references
through the shared entity-reference policy.

Balanced supports up to 1% of image height in blur radius (32 pixels
maximum) with 12 pupil samples. Cinematic supports 1.75% of image height
(48 pixels maximum) with up to 32 samples. Both retain the same signed
camera equation and foreground/background separation. Radii above the
chosen support are clipped.

## Renderer contracts

- Meter the unexposed world before defocus. Depth comes from the same
  pre-presentation world as the HDR color, after the supported TAA path.
- Classify 16×16 full-resolution depth tiles by maximum near and far radius.
  Dilate the bounds conservatively using the maximum supported pupil radius.
- Prefilter to half resolution with a depth-aware 2×2 kernel. Gather separate
  near and far FP32 layers from the reduced input. Use inverse
  pupil area for foreground coverage, and reject foreground taps from the
  background color. Provide a circular pupil or a rotated regular polygon.
- Apply the same reconstruction during bloom extraction and presentation.
  Both use unmodified picking and opaque depth. Screen UI and editor
  overlays follow world presentation. Disabled defocus skips all its passes
  and allocations. Metal packets retain their scratch generations until
  completion and use queue barriers for reusing view-owned scratch.

Half-resolution FP32 layers preserve the same HDR range as the input
without a separate full-resolution color allocation.

The reconstruction uses only surfaces already visible in the world buffer.
For out-of-focus foreground silhouettes it approximates hidden background
colors with neighbors. The quality gates include small highlight discs,
thin silhouettes over bright backgrounds, motion, focus pulls and cuts.
See the master design for the 720p, 1080p and 4K budgets and the 112 MiB
scratch limit. [Camera and image formation](camera-image-formation.md)

The reduced-resolution separation and conservative dilation follow the
responsibilities demonstrated in AMD's FidelityFX depth-of-field pipeline.
[FidelityFX Depth of Field 1.1](https://gpuopen.com/manuals/fidelityfx_sdk/techniques/depth-of-field/)

## Native evidence

The first half-resolution prefilter candidate measured 8.620 pixels near
and 8.703 pixels far against an independently calculated radius of 8.441
pixels. Metal 4 and the native Apple OpenGL 4.1 driver agreed within
0.00002 pixels. The FP64 camera test also covered 4K landscape and portrait
framing, a virtual gate for a conventional FOV, and axial depths from
0.5 to 900 world units. The two-backend disc test preserved picking and
depth exactly.

The initial Balanced performance run used four warm-up frames and 15
measured frames per case on Apple M4. Isolated times include classification,
dilation, prefilter and gather. The full-frame delta compares defocus on and
off with the same lens and scene. It also includes the reconstruction cost
in bloom and presentation.

| Backend / extent | Isolated median / p95 ms | Paired frame delta median / p95 ms |
| --- | ---: | ---: |
| Metal 4 720p | 2.109 / 2.290 | 1.551 / 2.288 |
| Metal 4 1080p | 2.359 / 2.791 | 2.763 / 3.359 |
| Metal 4 4K | 8.174 / 8.655 | 8.978 / 10.533 |
| OpenGL 4.1 720p | 1.907 / 2.108 | 1.273 / 2.876 |
| OpenGL 4.1 1080p | 2.774 / 2.958 | 1.816 / 3.058 |
| OpenGL 4.1 4K | 6.788 / 7.278 | 7.006 / 8.181 |

The first candidate exceeded all three Metal median budgets and the OpenGL
720p isolated-pass budget. Metal used 102,006,784 allocated scratch bytes
at 4K. OpenGL used 100,569,600 bytes of texture payload. The Metal run
completed 171 submissions with zero errors or live attachment generations
at shutdown. The timing includes the full 4K interaction case. The isolated
passes also exceeded the budget. [Metal samples](evidence/CameraDepthOfField_v1/metal-initial-timings.csv)
[OpenGL samples](evidence/CameraDepthOfField_v1/opengl-initial-timings.csv)

The candidate with block depth gathers, precomputed pupil taps and a
continuous-background gather measured these costs on the same fixture. The
20-sample Balanced candidate below predates the 12-sample preset check.

| Backend / extent | Isolated median / p95 ms | Paired frame delta median / p95 ms |
| --- | ---: | ---: |
| Metal 4 720p | 0.742 / 0.852 | 0.837 / 1.614 |
| Metal 4 1080p | 1.206 / 1.353 | 1.327 / 2.209 |
| Metal 4 4K | 4.387 / 4.936 | 5.116 / 5.874 |
| OpenGL 4.1 720p | 1.059 / 1.437 | 0.885 / 2.888 |
| OpenGL 4.1 1080p | 1.850 / 2.496 | 1.500 / 2.904 |
| OpenGL 4.1 4K | 4.868 / 5.422 | 5.941 / 7.167 |

All isolated phase budgets passed. The complete 4K Metal delta was 0.116 ms
over the 5.0 ms phase median budget in this sample. A four-pixel red occluder
over a green background had a 0.0153 mean absolute coverage error against
the independently integrated pupil area, with red outside its support zero
on both backends. The 3×3 near and far light fixtures retained 99.74% and
99.98% of the measured emitter energy. [Metal samples](evidence/CameraDepthOfField_v1/metal-balanced-timings.csv)
[OpenGL samples](evidence/CameraDepthOfField_v1/opengl-balanced-timings.csv)

A later 20-sample run alternated enabled and disabled frames in two
independent warmed views. This kept the temporal phases matched and
reduced the effect of different clocks across blocks. The Metal 4K
isolated median was 4.408 ms and the complete-frame delta was 6.014 ms.
The Metal 4K full phase budget is 5.0 ms. [Metal samples](evidence/CameraDepthOfField_v1/metal-interleaved-20-timings.csv)
[OpenGL samples](evidence/CameraDepthOfField_v1/opengl-interleaved-20-timings.csv)

The final Balanced run used 12 samples per pupil, direct premultiplied
composition and one tile-coverage lookup per aligned bloom extraction
block. These results passed the phase budgets for both the isolated passes
and the paired complete-frame delta.

| Backend / extent | Isolated median / p95 ms | Paired frame delta median / p95 ms |
| --- | ---: | ---: |
| Metal 4 720p | 0.855 / 1.163 | 0.567 / 1.205 |
| Metal 4 1080p | 0.980 / 1.047 | 1.250 / 1.694 |
| Metal 4 4K | 3.497 / 3.840 | 4.642 / 5.862 |
| OpenGL 4.1 720p | 0.695 / 0.890 | 0.503 / 2.170 |
| OpenGL 4.1 1080p | 1.371 / 1.621 | 1.231 / 2.624 |
| OpenGL 4.1 4K | 4.174 / 4.642 | 4.875 / 7.210 |

The final Balanced disc measured 5.272 pixels against 4.748 pixels
expected. The Cinematic discs measured 8.687 near and 8.703 far against
8.441 pixels expected, with less than 0.06% emitter energy error. The
thin-occluder mean absolute coverage error remained 0.0153. The held
three-frame Metal queue matched serial output exactly after view reset
and resize. The paired Metal run reached 3,296,026,880 tracked attachment
bytes across two views and retired all generations at shutdown.
[Metal final samples](evidence/CameraDepthOfField_v1/metal-final-timings.csv)
[OpenGL final samples](evidence/CameraDepthOfField_v1/opengl-final-timings.csv)
[Metal preceding 12-sample run](evidence/CameraDepthOfField_v1/metal-balanced12-timings.csv)
[OpenGL preceding 12-sample run](evidence/CameraDepthOfField_v1/opengl-balanced12-timings.csv)

| Gate | Result |
| --- | --- |
| Native Debug and Release, Metal 4 and Apple OpenGL 4.1 | PASS disc, occluder, subject tracking, focus pull, cut, resize and combined HDR interaction. Metal validation stayed enabled during the numerical checks. |
| Frozen manual and HDR bloom captures | PASS all 144 byte-for-byte. |
| Debug and Release editor/scene regression | PASS authored round trip, focus-reference remap, Undo/Redo and Play/Stop. |
| Relocated signed release archive | PASS integrity and offscreen depth-of-field tests on both backends. The Metal probe ran with shader sources and the SDK denied. |
| Relocated desktop player | BLOCKED by the locked macOS console. |

The timing study covers Balanced. Cinematic performance and hardware
outside the native Apple M4 configuration remain unverified. OpenGL uses
the spatial path with TAA and XeGTAO inactive on this host. Depth of field can
extrapolate visible background at foreground silhouettes. Hidden
background geometry and lens breathing are outside this approximation.

The disc images show near sharp, near defocused, far sharp and far defocused
in that order. Each panel subtracts the fixed fixture background and
amplifies the remaining linear signal by 36× for inspection.
[Metal disc panels](evidence/CameraDepthOfField_v1/metal-disc-signal.png)
[OpenGL disc panels](evidence/CameraDepthOfField_v1/opengl-disc-signal.png)

The combined study moves the camera with automatic exposure, HDR bloom,
probes, SSR and all three shadow types enabled. The captures include a
focus cut. [Matched OpenGL sharp frame](evidence/CameraDepthOfField_v1/opengl-gallery-sharp.png)
[OpenGL defocused frame](evidence/CameraDepthOfField_v1/opengl-gallery.png)
[OpenGL focus cut](evidence/CameraDepthOfField_v1/opengl-focus-cut.png)
[Matched Metal sharp frame](evidence/CameraDepthOfField_v1/metal-gallery-sharp.png)
[Metal defocused frame](evidence/CameraDepthOfField_v1/metal-gallery.png)
[Metal focus cut](evidence/CameraDepthOfField_v1/metal-focus-cut.png)

### Reproduce

```sh
python3 scripts/hazel.py build --config Release --tests \
  --target MetalShaderTools --target MetalRenderer2DQuadSmoke --target MigrationRenderer2DSmoke
python3 scripts/macos/metal_shaders.py --config Release --output Hazel/Resources/shaders/metal
python3 scripts/macos/hdr_bloom.py --project 'build/hdr-bloom-project/HDR Bloom Study/HDR Bloom Study.hproj' \
  --config Release --defocus-study --output build/qualify-defocus-discs
python3 scripts/macos/hdr_bloom.py --project 'build/hdr-bloom-project/HDR Bloom Study/HDR Bloom Study.hproj' \
  --config Release --defocus-benchmark --output build/qualify-defocus-timing
```

For a runtime fixture, use `--from-gallery` with `--prepare-only
--runtime-only --automatic-camera --defocus-camera`. Export it with
`scripts/hazel.py editor-export` and qualify it with `hdr_bloom.py
--archive … --defocus-study`. Use `--offscreen-only` when testing a
noninteractive host.
