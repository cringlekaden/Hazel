# Camera and image formation qualification

Hazel's scene camera uses one physical lens and one exposure policy per
view. New native HDR scenes start with automatic exposure. Cameras can
inherit scene exposure or choose automatic or manual physical exposure.
Older signed-ev scenes migrate to the manual physical policy with their
previous gain represented as compensation. The runtime holds metering,
focus history and color assets per view with completion-safe GPU packet
ownership. The scene retains its existing PBR lighting and its
Khronos PBR Neutral tone curve.
[Master architecture and schema](camera-image-formation.md)

## Checkpoints

| Change | Commit | Qualification record |
| --- | --- | --- |
| 1. Physical camera and architecture | `b7760975e680f3bfc3e01f115e0da3af32f5936c` | [Foundation](camera-image-formation.md#phase-1-native-results) |
| Metal resource admission | `2d2f22929589683afe096713ad6d3efff92b9e38` | [Memory findings and pressure tests](metal-memory-admission.md) |
| 2. GPU exposure and adaptation | `c4963702432d4d150a0c1ea37132f8827134fbac` | [Exposure, migration and time traces](camera-exposure-admission.md#production-phase-2) |
| 3. Depth of field and tracked focus | `25eb1a6eac655e46fcbbe7e65d0b4f769946884c` | [Thin-lens optics and focus tests](camera-depth-of-field.md) |
| 4. Global-shutter motion blur | `41b582ce07d7d5e2b54a2a76cb3eea21793181b5` | [Shutter integration and motion fixtures](camera-motion-blur.md) |
| 5. Lens flare and sensor effects | `e24c7bee34b7257d304fcf071a77b688d661e6a6` | [Visible-source optics and captured studies](camera-lens-effects.md) |
| 6. Grading and presentation | `5721dbf6917d2e850662971220d47cf157b73262` | [Grading domains, controls and presets](camera-color-presentation.md) |

## Authoritative conventions

- Aperture is an f-number, shutter duration is in seconds, focal length
  and sensor dimensions are in millimeters, ISO is relative sensitivity,
  and focus distance is in world units. The optical DOF approximation
  uses one world unit per meter. The active gate fits the viewport
  aspect within the sensor. Perspective clipping follows the scene
  camera convention.
- `EV100 = log2(N² × 100 / (t × ISO))`. Manual gain is
  `exp2(10 - EV100 + compensationStops)`, with the exponent bounded to
  [−16, 16]. Positive compensation brightens. Hazel's reference light
  calibration is relative.
- Automatic exposure meters scene-linear Rec.709 luminance and adapts
  the gain toward a clipped weighted histogram estimate with exponential
  step `1 - exp(-rate × dt)`. The automatic path leaves the lens
  triangle fixed. Gain bounds, percentile cuts, regional weights, sky
  contribution and lock operate on the same policy.
- The signed sensor radius is `f² (z − s) / (2N z (s − f))`, for focal
  length `f`, axial object depth `z`, and focus distance `s`. Convert to
  output pixels by multiplying by pixel height over active sensor
  height. DOF uses half-resolution near/far reconstruction and tracks
  focus in inverse distance.
- Global-shutter displacement is `(p − pPrev) × duration / frameSeconds`
  for unjittered physical-pixel positions. Angle shutter replaces the
  time fraction with `angle / 360`. Quality limits the centered smear
  and cuts reset the correspondence history.
- World order is scene shading, scene-linear temporal reconstruction,
  metering, shutter, DOF, bloom and visible-light flare, lens geometry
  and vignette, camera gain and Bradford white balance, Neutral, linear
  grading and LUT, grain, screen UI, then one sRGB encode. The grading
  reference checks color while the distortion reference checks color
  rays, picking and depth.
[Camera pass ordering](camera-image-formation.md#target-pass-order-and-phase-gates)
[Display pipeline and formulas](camera-color-presentation.md)

## Hardware and coverage

Qualification uses the 16 GiB Apple M4 with native Metal 4 and Apple
OpenGL 4.1. OpenGL uses raster-based metering and supports the camera
effects, spatial rendering and SSR on that host. Metal also qualifies
the engine's compute AO and TAA. The linked phase studies include
Debug/Release native fixtures, scene and editor history tests, visual
captures, performance samples and known omissions.

The previous fixed aggregate limit was a Hazel admission policy. The
earlier second queued 4K frame failed with 2,212,338,984 tracked bytes
and Metal reporting 2,255,863,808 allocated bytes. The M4 recommended
working set was 12,713,115,648 bytes. The replacement derives separate
tracked and native limits from device guidance and RAM and reuses AO
scratch on the ordered queue. The pressure fixture completed 30 full-effects
4K frames with three queued slots and peaked at 2,467,825,496 tracked
and 2,605,285,376 Metal allocation bytes during temporary resize overlap.
[Resource history, admission formula and failure recovery](metal-memory-admission.md)

The camera checkpoint archive tests use relocated Release data. Metal
passes with shader sources and SDK access denied. OpenGL consumes the
portable GLSL in the archive. The Phase 2 packaged desktop run
presented live frames on each backend. The desktop console was locked
for Phases 3–5, which passed their authored offscreen fixtures. The
final Phase 6 relocated Release product passed desktop startup and
normal shutdown with the full authored camera. Metal completed 62
presentations and OpenGL met the 60-rendered-frame gate. Windows,
Linux, Intel GPUs and other Apple configurations remain unverified.

The default HEAD runtime suite continues to fail the historical
`IBLQuality_v2/Materials/M0R3.hmat` checksum gate. The camera project
keeps the frozen material and hash. The established immutable
`86c581c13fd5c944c2a06d720314a753d7607d8f` reference fixture has all
37 expected hashes. The selected scene foundation, editor, and
runtime suites passed against it in both Debug and Release.
[Historical fixture investigation](local-lighting.md)

## Incremental GPU measurements

Times are Release GPU medians in milliseconds at 720p, 1080p and
3840×2160, respectively. Optical phases use Balanced or the documented
standard configuration. Exposure and glare rows are isolated passes.
The depth-of-field and shutter rows are paired complete-frame deltas.
The source records include p95 values, per-pass results, scratch and
tracked memory. Measurements from different studies cannot be added
to predict one combined frame.

| Operation | Metal 4 | OpenGL 4.1 |
| --- | --- | --- |
| Automatic metering and adaptation | 0.022 / 0.032 / 0.025 | 0.069 / 0.022 / 0.091 |
| Depth of field, Balanced | 0.567 / 1.250 / 4.642 | 0.503 / 1.231 / 4.875 |
| Motion blur, Balanced | 0.658 / 1.067 / 3.868 | 0.569 / 0.831 / 4.404 |
| Standard lens flare pass | 0.021 / 0.022 / 0.062 | 0.025 / 0.024 / 0.025 |
| Color grade composite delta | 0.003 / 0.002 / −0.013 | 0.041 / 0.008 / 0.020 |

Camera controls do not change the underlying local-light, shadow,
reflection, AO or TAA reference thresholds. Frozen HDR camera reference
captures retain exact manual behavior across neutral camera additions.
Balanced depth of field and motion blur carry the performance qualification.
The production Cinematic paths have numerical and visual coverage. Their
final throughput remains unmeasured. Phase 5's first all-options Metal 720p p95 was 0.705 ms for a
0.700 ms limit. The controlled repeat measured 0.618 ms. The original
samples remain available.
[Camera lens performance](camera-lens-effects.md#gpu-cost)

The user-facing camera documentation includes all five presets and
the camera and scene ownership rules. The evidence captures contain
filtered thin-lens discs, shutter trails, visible and occluded glare,
the lens-dirt test and camera color grading.
[Camera controls](../color.md)

## Evidence and limits

- Automatic exposure covers daylight, high-contrast interiors, dark
  scenes with localized emitters, percentile rejection, startup,
  locking, cuts, and 30, 60 and 120 Hz adaptation. The independent
  comparison tracks exposure gain in stops over time.
  [Automatic exposure studies](camera-exposure-admission.md#production-phase-2)
- DOF includes overlapping geometry, thin foreground, defocused
  highlights, signed CoC, focus pulls, cuts and view resize. Autofocus
  tracks an authored subject's world pivot and falls back to the lens
  distance. Screen UI remains sharp.
  [Depth of field fixture](camera-depth-of-field.md)
- Motion blur covers object translation, camera translation and
  rotation, large velocities, low-frame-rate motion, cuts and a
  joint shutter/DOF capture. Existing rigid-mesh motion is used for
  the shutter. World sprites receive camera motion.
  [Global-shutter motion fixture](camera-motion-blur.md)
- Flare comes from visible HDR highlights. Its screen-space model
  covers ghosts, halo, diffraction-style streaks and lens dirt.
  The modeled source set is limited to the viewport.
  [Lens evidence](camera-lens-effects.md)
- Grading has independent thermal, film-curve and 3D LUT references.
  Neutral settings preserve the frozen 144-capture HDR suite. The
  isolated grade cost meets budget. Complete-scene p95 at 720p and
  1080p is inconclusive relative to the paired A/A variability.
  [Grading studies and data](camera-color-presentation.md)
