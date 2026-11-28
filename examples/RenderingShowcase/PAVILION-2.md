# Pavilion 2.0 stabilization

Apple M4, 16 GiB, macOS 27.3. The starting scene was `98356b7`.
The native tour, title repair and temporal cut correction were tested separately
from the remaining fine-highlight instability.

## Entrance and loop

The title plane and fascia front were both at Z = 7.915 m. The title face is
now at Z = 7.950 m, with a backing whose front is at Z = 7.946 m. The repair
adds actual geometry and moves the authored title entity. It uses the same
material, shadows and depth tests as the rest of the scene. Both 256-face
reflections were rebaked and their cooked specular mip zero matched the native
FP32 capture. [Title comparison](evidence/pavilion2/fascia-before-after.jpg)

Nutella reads the optional project-owned `.hcam` through its normal scene
startup. It drives one runtime perspective camera and the ordinary renderer.
The closed, time-parameterized Hermite curve shares positional and look-target
derivatives at each key. Aperture, focal length, inverse focus distance and
optical strength interpolate between keys. Shutter history follows the moving
view. On an 80-second track with 1,609 sampled frames, the endpoints matched
exactly and the finite-difference seam velocity mismatch was 0.00966 per second
across the tested matrix columns. Pausing holds the sampled pose. Inspection
freezes the track and uses its own transient navigation camera; returning uses
the retained tour time. Restart invalidates view history deliberately.
[Loop render](evidence/pavilion2/loop.mp4) ·
[Controls](README.md#nutella-tour-controls)

## Temporal integration and residual shimmer

The player and Inspect Camera both submit the scene's TAA setting to
`SceneRenderer`. The inspection transform has a separate view identity, but
the authored physical projection and photographic settings are inherited.
Ultra and High request TAA. The Metal path jitters world color in clip space,
writes motion from unjittered geometry, evaluates SSR on the jittered view
before temporal resolve, and keeps unjittered IDs for picking. OpenGL 4.1
logs inactive TAA and uses the spatial SSR/composite path.

**Correction.** The original shared camera-cut detector selected the smallest
positive center depth of any visible mesh batch. A large batched wall, floor
or frame can surround the eye. Its center can cross the eye plane despite its
meaningful surfaces remaining meters away. The resulting small proxy depth
caused ordinary camera movement to exceed the cut threshold. The new proxy uses
the midpoint of the positive part of the transformed bounds' axial depth
interval. Wholly in-front bounds retain their old center depth. The cut
threshold and renderer history rejection rules are unchanged. The correction
is shared by TAA, shutter correspondence, autofocus/DOF and exposure history.
The matched loop went from **81 history resets to zero**, and retained Metal
history at the wrap. [Motion comparison](evidence/pavilion2/history.mp4)

**Residual.** The 960×540 near-subpixel floor sweep keeps exposure, grade,
focus, motion blur and bloom fixed or disabled, discards 16 warmup frames and
samples 32 frames. It measures display luma change in a fixed floor ROI.
The motion spans ±2.2 mm at the camera. These changes include physical
visibility and reflectance variation, so they measure visible instability and
are not a supersampled numerical error oracle.

| Floor ablation | RMS frame delta in display codes | p99 delta |
| --- | ---: | ---: |
| Authored Metal TAA and SSR | 4.10 | 2.79 |
| TAA off | 1.20 | 0.72 |
| SSR off, TAA retained | 3.66 | 1.79 |
| Probes off | 4.11 | 2.79 |
| Global IBL off | 4.19 | 2.21 |
| All direct lights off | 4.68 | 2.78 |
| Normal map contribution off | 4.10 | 2.86 |
| ORM point mip selection | 4.10 | 2.79 |
| SSR with 80 steps | 4.07 | 2.93 |
| AO off | 4.10 | 2.79 |

Metal TAA accumulation advanced to phase 48 in this test. The lower delta
with TAA disabled does not establish lower spatial aliasing. A nearly
stationary spatial sample can hold a hard edge for many frames while jitter
continues to move sample positions. The analytic TAA edge suite retains the
independent spatial and temporal accuracy gates.

The largest changes cluster near narrow luminous or reflective geometry and
coverage transitions. Probe, normal and roughness mip toggles barely change
this workload. SSR adds measurable floor variation, but it is not the sole
source. Its ray march has a first crossing, depth/normal support tests, and
smooth confidence weights that blend into the probe/global fallback.
The direct-light toggle removes diffuse and specular together. A direct-lobe
split and a supersampled moving high-HDR coverage/reflection oracle remain open.
The available results do not support changing roughness, weakening the normal
maps, disabling SSR, or adding a whole-engine specular AA filter. A future
normal-variance filter should estimate the unresolved distribution over the
pixel footprint, apply it consistently to direct/IBL terms and SSR roughness,
then verify narrow-projection and mapped-normal cases against a supersampled
reference. [TAA pair](evidence/pavilion2/taa.mp4) ·
[Specular AA reference](https://yusuketokuyoshi.com/papers/2019/ImprovedGeometricSpecularAA.pdf) ·
[Filament shading reference](https://google.github.io/filament/Filament.md.html)

## Optical comparisons

The runtime camera opens to f/1.3 at the helmet and pulls focus to the back wall.
It then traverses the archive with visible HDR practicals, a stronger
source-driven dirt plate, and lens flare. F and G toggle separate effects.
The dirt pair increased from 3 to 28 display codes maximum after adjusting the
passage plate and strength. About 1.09% of pixels differ by at least three codes
in the final matched pose. The darkness outside the light spread is retained.
The separate lens reference tested exactly occluded sources.

[Focus](evidence/pavilion2/focus.jpg) · [Flare](evidence/pavilion2/flare.jpg) ·
[Dirt](evidence/pavilion2/dirt.jpg) · [Shutter](evidence/pavilion2/shutter.jpg) ·
[Exposure](evidence/pavilion2/exposure.jpg) · [Grade](evidence/pavilion2/grade.jpg) ·
[Pair measurements](evidence/pavilion2/optical-differences.json)

## Native verification

The title, tour and camera history correction passed focused native
qualification. The broader optical shimmer goal is **incomplete**. The test
retains high-contrast subpixel variation even with uninterrupted history.

| Gate | Outcome |
| --- | --- |
| Release window startup | Nutella tour and Inspect Camera on Metal 4 and OpenGL 4.1. Ultra and High on Metal. Hazelnut authored-scene startup on both APIs. Exact README launch commands reached ready. |
| Debug in both APIs | Nutella and Hazelnut startup and normal close. The final cut correction also ran the matched floor fixture, which advanced to phase 48 on Metal and stayed spatial on OpenGL. |
| Fixed and moving cameras | Metal presented resolve traces for default tour and inspection. The isolated authored camera is exercised by the window benchmark. Native loop frames used production scene submission with exact seam pose and preserved Metal history. |
| Independent renderer references | Metal TAA projection, fast and sloped silhouettes, invalidation and lifetime passed. Native XeGTAO, SSR, box-probe overlap and combined shadow references passed. No acceptance thresholds were relaxed. |
| Independent camera references | Exposure gain, adaptation and cuts, depth of field, shutter, lens occlusion and resource handling passed on both native APIs. |
| Released source-free archives | Both signed applications opened relocated packaged content on both APIs with checkout and tooling access denied. |
| Frozen rendering references | The selected Release runtime suite passed with immutable fixture `86c581c`. The historic default IBLQuality_v2 byte failure and CSM stress-parity limits remain open. |
| Windows and Linux | Not run. |

### Presented performance

The M4 benchmark timed the authored arrival pose, with Ultra or High optics,
using completed GPU frames matched to positive desktop presentation timestamps.
These are steady arrival-view measurements. Scene complexity and effect cost
vary throughout the tour. The 1440p samples used fullscreen at the display's
current mode.

| Physical extent | Ultra median / p95 | High median / p95 |
| --- | ---: | ---: |
| 1280×720 | 8.96 / 16.49 ms | 8.57 / 16.50 ms |
| 1920×1080 | 17.56 / 26.64 ms | 16.71 / 24.82 ms |
| 2560×1440 | 25.66 / 26.01 ms | 24.33 / 24.68 ms |

[Native and temporal measurements](evidence/pavilion2/results.json)

## Reproduce

```sh
python3 scripts/hazel.py build --config Release --tests \
  --target MetalRenderer2DQuadSmoke --target MigrationRenderer2DSmoke \
  --target MetalM1Smoke --target Nutella --target Hazelnut
python3 examples/RenderingShowcase/tools/study.py loop-check --width 640 --output build/pavilion2-loop-check
python3 examples/RenderingShowcase/tools/study.py loop-pairs --output build/pavilion2-optics
python3 examples/RenderingShowcase/tools/study.py present --width 2560 --output build/pavilion2-present
```

The floor tool needs Pillow, NumPy and PyYAML in the running Python environment.

```sh
python3 examples/RenderingShowcase/tools/shimmer.py --output build/pavilion2-floor
```

See the [asset inventory and glTF roadmap](GLTF-AUDIT.md).
