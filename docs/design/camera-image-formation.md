# Camera and image formation

Starting point: owner-accepted HDR bloom at `4d6bb7a` on `feature/3d-foundations`.
This record defines the phase gates and pass order for the complete camera.

## Current frame and ownership

Hazelnut's navigation camera and Nutella's active primary camera both reach
`Scene::DrawView` and `SceneRenderer::Collect/Render`. The host owns each
`SceneRenderView`; the scene owns authored settings and the component owns the
camera projection. Play copies authored components. Camera edits use the
inspector's draft/Undo path and the exact authored-state comparison. Runtime
inspection can substitute a separate view and camera. Probe bakes submit a
private radiance capture through the same renderer.

Directional and local shadow maps feed the shared forward pass. Metal AO
reconstructs view data by inverting the FP32 raster VP in FP64 on the CPU.
Directional cascades also reconstruct from an FP64 inverse. This is distinct
from SSR's FP32 reconstruction and from the current FP32 motion producer.
Metal TAA renders a jittered opaque sample and a separate unjittered motion
pass using the previous camera and previous world transforms, then resolves
opaque scene-linear radiance and unjittered depth/ID. OpenGL 4.1 currently
has neither TAA nor such a motion pass. Raw hardware depth and ID alone cannot
distinguish sky from every unpickable draw. Sky masking for exposure must use
the real background/opaque coverage contract before world 2D.

World 2D draws in scene-linear color after opaque TAA. Bloom filters the
combined FP32 HDR world image. Presentation adds bloom, scales HDR RGB by a
single gain, applies Khronos PBR Neutral and preserves alpha, signed IDs and
depth. Screen UI and editor gizmos use display-linear color. One sRGB encode
precedes the editor's encoded selection outline. Display-referred scenes
bypass HDR. See [HDR bloom](hdr-bloom.md), [TAA](taa.md),
[temporal quality](taa-qualification.md), [SSR](screen-space-reflections.md),
[XeGTAO](xegtao.md) and [combined shadows](combined-shadow-quality.md).

Keep the shared renderer, the viewport/controller boundary, orthographic
projection, HDR buffers, Neutral, the bloom pyramid and the sRGB boundary.
Preserve lighting, material/BRDF, reflection and shadow math and their frozen
oracles. Replace the scene's lone signed-EV exposure control with the camera
exposure policy and explicit compensation. Remove the old authored scalar and
label once the reader can migrate it to the canonical exposure schema; retain
one presentation gain and a read-only migration branch. Old signed stops map
one-for-one to compensation with the compatibility reference settings below.
Existing scene/projection/bloom fixtures therefore retain their expected output.
New automatic-camera fixtures have distinct intent and get separate references.
Any deliberate change to the reference values of opted-in camera modes needs
new independent numerical and visual evidence.

## Physical conventions

| Quantity | Convention |
| --- | --- |
| Focal length and sensor | Millimeters. A centered crop fits the viewport aspect `a` inside the nominal sensor. Active height is `min(sensorHeight, sensorWidth / a)`, active width is height × a. |
| FOV | `2 atan(activeDimension / (2 f))` for each axis. The projection uses vertical FOV in radians and the existing clip conventions. The authored vertical-FOV projection remains a selectable framing mode. FOV uses the nominal focal length at all focus distances, with no modeled focus breathing. |
| Manual settings | `EV100 = log2((N²/t) × (100/ISO))`. N is the f-number and t is duration in seconds. Higher EV100 darkens the image. The neutral reference is f/4, 1/64 s, ISO 100, exactly EV100 10. |
| Calibration | Existing direct light is color × authored intensity. Local lights add a finite-range falloff to inverse-square attenuation, and HDR assets, background, IBL gains and emission have relative scene values. Scene RGB does not imply cd/m². With calibrated `L` in cd/m², the saturation convention is `L / (1.2 × 2^EV100)`. Hazel maps one legacy scene-linear unit to a *reference* 1228.8 cd/m² **for the camera gain only**, giving `log2Gain = 10 − EV100 + compensationStops`. This preserves the legacy neutral gain and makes photographic changes proportional. No lighting migration or absolute photometric claim follows. |
| Compensation | Positive stops brighten. The current compositor accepts total log₂ gain in [−16,16]. Explicit manual exposure saturates at that boundary. |
| Focus and CoC | Distance is positive axial camera depth in world units, with a documented 1 m/world-unit optical convention. Convert f to world units. Signed sensor-plane diameter is `(f/N) × f × (z−s) / (z × (s−f))`, where s is focus distance and z is subject depth. Near is negative. Pixel radius is half the diameter scaled by physical image height / active sensor height in matching units. Require s > f. DOF applies to perspective cameras and uses a bounded support. |
| Shutter | Manual seconds affect EV100. Global-shutter angle, when selected for motion blur, gives image integration duration `frameSeconds × angle / 360`; this motion-only mode never changes ISO, aperture or manual photographic EV100. Displacement is unjittered current-to-previous displacement × integration duration / elapsed render time. |

Filament's saturation exposure and reflected-light metering provide a useful reference.
Its exposure path assumes physical scene luminance. Its camera API also considers
focus-dependent effective focal length, which Hazel defers to avoid changing
projection during focus pulls. The EV and sensor model do not require adopting
Filament's pre-exposure, frame graph, reverse-Z or color stack.
[Camera API](https://github.com/google/filament/blob/main/filament/include/filament/Camera.h) ·
[Exposure implementation](https://github.com/google/filament/blob/main/filament/src/Exposure.cpp) ·
[Rendering documentation](https://google.github.io/filament/Filament.md.html)

## Authored settings and migration

Phase 1 adds an optional camera `PhysicalLens` map with `Version: 1`,
`Projection`, `FocalLengthMM`, `SensorWidthMM`, `SensorHeightMM`,
`Aperture`, `ShutterSeconds`, `ISO` and `FocusDistance`. Presence selects the
physical manual settings for that camera; `Projection` selects whether the lens
controls framing. Absent settings inherit the neutral reference. Existing
perspective and orthographic clip/size/FOV data retain their meanings.

Phase 2 introduces the canonical scene exposure block, applicable to the editor
navigation camera and the inherited runtime policy. Migration reads the old
`Presentation.Exposure` scalar into compensation with manual reference
settings. Writers emit only the new schema. New HDR scenes start in Automatic;
display-referred 2D continues to bypass camera presentation. An explicit camera
override supplies the exposure policy for that view. Manual camera exposure
uses the camera triangle. Automatic computes gain without changing the
triangle. There is one resolved policy, never additive scene and camera
compensation sliders. Lens settings do not activate depth of field or motion
blur. Phase 6 exposes validated presets composed of the same settings.
History lives in the render view, keyed by scene and active camera identity,
with cut/reset handling independent of editor history and serialization.

Use `Presentation.CameraExposure` for scene policy and optional
`CameraComponent.CameraExposure` for an authored override. Each block has
`Version: 1`, `Mode: Automatic | ManualPhysical`, and `CompensationStops`.
Automatic additionally has `Metering: Average | CenterWeighted | Spot`,
`LowPercentile`, `HighPercentile`, normalized `SpotCenter` and `SpotRadius`,
`IncludeSky`, `MinGainStops`, `MaxGainStops`, `BrighteningRate`,
`DarkeningRate` (s⁻¹) and `Locked`. Runtime locking uses the current view's
adapted gain. A fixed shot can select ManualPhysical in its camera override.
The decoder rejects simultaneous old and new exposure keys. A camera
override replaces the scene exposure block. Display-referred presentation
has no exposure block. Depth of field, motion blur and optics add separate
versioned effect settings in their phases, without another exposure field.

The luminance meter evaluates `Y = dot(RGB, [0.2126, 0.7152, 0.0722])` from
unexposed Rec.709. A trimmed weighted mean in log₂ space estimates the
geometric mean of the retained scene luminance. With retained mean `m` in
stops, `targetGainStops = clamp(log2(0.18) − m, minGain, maxGain)`; camera
compensation is added to the adapted gain before final gain saturation.
The brackets limit the automatic component in signed stops. Rates are the
reciprocals of the exponential time constants for gain in stops. Locking holds
that state while compensation remains an explicit artist control. Startup,
a camera identity change, cut, or reset initializes the state before it can
be locked; the render view never resumes another camera's gain. Quantization
error and sampling error are qualified separately. A 128-bin histogram over
48 stops starts with 0.375-stop bins and at most 0.1875 stops of center
quantization error within its range.
[Unreal Engine auto exposure](https://dev.epicgames.com/documentation/unreal-engine/auto-exposure-in-unreal-engine)

## Target pass order and phase gates

1. **Physical camera.** Author and round-trip the optional lens, resolve a
   physical manual gain in the existing presentation pass, and independently
   check f-stop/shutter/ISO reciprocity, horizontal/vertical framing, migrated
   gain, bounds and camera isolation. Require ≤2e-6 stop error, ≤2e-6 radians
   FOV error and existing native bloom/presentation thresholds. Capture matched
   physical/compatibility frames. Zero new GPU passes or persistent GPU bytes.
2. **Meter and adapt.** Opaque TAA and world 2D → sample finite scene-linear
   luminance before optics or bloom. Use weighted log2 luminance percentiles,
   equal/center/spot distributions and optional sky exclusion. Prefer a bounded
   stratified grid and a histogram with fractional rejection at percentile
   boundaries. Metal can reduce local histograms in compute. OpenGL 4.1 must
   reduce on GPU with raster draws; an additive binned raster pass and tiny
   resolve are a candidate. Keep ping-pong EV on GPU. Initialize to the target
   on a cut or first valid frame. A valid black image gets a finite bounded
   target; no samples hold previous state or use a neutral startup. With gain
   in stops, use `current + (target − current) × (1 − exp(−rate × dt))`.
   Select the rate by sign, lock by holding state, and allow an explicit
   cinematic override. Preserve unexposed TAA and probe data. Require ≤0.25
   stop target error against qualified FP64 distributions and ≤0.02 stop
   frame-rate drift at matched times (30/60/120 Hz fixed step). Study
   interior/exterior, night, transient highlights, multi-view/resize and cuts.
   [Phase 2 implementation and native evidence](camera-exposure-admission.md#production-phase-2)
3. **Defocus.** Use scene-linear color with axial depth and separate near and
   far gather, foreground coverage and bilateral reconstruction. Start with
   half-resolution, a conservative tile radius and bounded ring/polygon
   sampling. Start with fixed focus and qualify regional/subject autofocus as
   a separate mode with hysteresis and a bounded rate. Require measured CoC
   within max(1 pixel, 5%) for isolated supported discs and compare thin
   foreground coverage and leakage to a sampled thin-lens reference. An
   authored FOV can define a virtual gate from the actual projection and
   focal length so perspective defocus agrees with the image framing. Follow
   [AMD's production DOF technique](https://gpuopen.com/manuals/fidelityfx_sdk/techniques/depth-of-field/)
   for separable responsibilities and occlusion examples.
4. **Motion blur.** Reuse or decouple the opaque motion producer from TAA so
   Metal and GL can submit unjittered velocities independently of AA. Use
   depth-aware shutter reconstruction with tile and neighbor maxima, gated by
   cut/projection/camera identity and valid elapsed time. Preserve picking and
   overlays. Require ≤1 pixel endpoint error in supported constant-motion
   cases, successful cuts and bounded disocclusion error against independent
   time-sampled references. Use the [McGuire reconstruction filter](https://casual-effects.com/research/McGuire2012Blur/)
   as the screen-space baseline. Render motion blur before DOF with original
   depth for both. Compare against combined time/lens sampling before
   qualifying that order; both are single-view approximations with missing
   occluded information.
5. **Optics.** From visible HDR highlights build reduced-resolution flare,
   ghosts, halo and optional streak/starburst. Flare injection follows the
   DOF/motion chain and shares exposure with the image. Dirt modulates glare
   only. Keep vignette/aberration/distortion opt-in. Test occluded/visible
   sources, asymmetric viewport handling and zero-effect equivalence. The
   source-isolation fixture requires zero flare at full occlusion and unchanged
   world radiance when toggling dirt. Use
   [Filament's post-process implementation](https://github.com/google/filament/blob/main/filament/src/PostProcessManager.cpp)
   for reduced-resolution flare and dirt/presentation integration.
6. **Presentation.** Apply optical geometry and vignette to the world image
   and associated optics before display transform. Add scene-linear bloom
   and flare, apply gain and linear white balance, use Neutral, then apply
   display-referred grading/grain ahead of screen UI and the one sRGB encode.
   Identify every grading operation by domain, with neutral settings and
   missing optional assets preserving the base path. Require Neutral to
   retain existing numerical tolerances, neutral grading to preserve output,
   and new optional operations to match independent color references and
   source-free packaging. Verify presentational warps against picking and UI.

### Performance ceilings

These are added Release GPU medians in milliseconds for 720p / 1080p /
3840×2160 on the M4, with a p95 ceiling of twice the median budget. Measure
the phase itself and the paired change in complete GPU frame time using
identical inputs and settings. Scratch caps are incremental live per-view
allocation at 3840×2160. Count replacement generations retained by the GPU
against the native device's renderer budget. On Metal, the renderer uses
working-set guidance, physical memory on unified devices, and per-allocation
limits. The phase scratch caps still apply.
[Metal memory policy](metal-memory-admission.md)

| Phase | Metal 4 ms | OpenGL 4.1 ms | Scratch cap |
| --- | --- | --- | --- |
| 1 — Foundation | 0 / 0 / 0 additional pass work | 0 / 0 / 0 additional pass work | 0 |
| 2 — Exposure | 0.25 / 0.30 / 0.45 | 0.45 / 0.60 / 0.90 | 1 MiB |
| 3 — DOF | 0.9 / 1.6 / 5.0 | 1.6 / 2.8 / 8.0 | 112 MiB |
| 4 — Motion | 0.8 / 1.3 / 4.0 | 1.4 / 2.4 / 7.0 | 96 MiB |
| 5 — Optics beyond bloom | 0.35 / 0.60 / 1.9 | 0.60 / 1.0 / 3.4 | 32 MiB |
| 6 — Grading and assembly delta | 0.15 / 0.25 / 0.75 | 0.25 / 0.40 / 1.3 | 8 MiB |

Run Debug/Release native Metal and GL, independently generated numerical
references, matched captures, configured UI/editor/player/packaging checks
and combined regressions at each phase boundary. Capture resolution, GPU
allocation (including retirement), warmup, sample count, median and p95.
A submitted frame is not evidence of desktop presentation.

The bloom checkpoint already hit 2 GiB with full FP32 readback, TAA and
4K on Metal even with bloom disabled. Keep that historical failure. Use
small targeted readbacks and timing with no full-frame readback to measure
the production frame; qualify full-effects admission and in-flight resize
independently. Do not trade picking, frozen image parity or numerical
correctness for memory headroom. GL histogram behavior, blur discontinuities,
sky classification, output warp/selection alignment, and the availability of
native 2× presentation and non-M4 machines are qualification risks.

## Reproduce the Phase 1 study

Use a prepared HDR Bloom Study from the [HDR bloom procedure](hdr-bloom.md).
For both Debug and Release, build Hazelnut, Nutella and the four test schemes
listed below. Run the CPU and editor checks with `scripts/hazel.py test`.

```sh
python3 scripts/hazel.py build --config Release --tests \
  --target MigrationSceneFoundationSmoke --target MigrationEditorSmoke \
  --target MetalRenderer2DQuadSmoke --target MigrationRenderer2DSmoke \
  --target Hazelnut --target Nutella
python3 scripts/hazel.py test --config Release --only SceneFoundationSmoke --only EditorSmoke
python3 scripts/macos/hdr_bloom.py --config Release \
  --project 'build/hdr-bloom-project/HDR Bloom Study/HDR Bloom Study.hproj' \
  --output build/camera-foundation-native-release
```

Each backend writes reference, physical and equivalent-compensation captures.
The physical pair uses f/8, 1/64 s and ISO 200, one stop darker than the
reference settings. ID and depth must remain exact for the matched framing.
[Metal reference](evidence/CameraFoundation_v1/metal-reference.png) ·
[Metal physical](evidence/CameraFoundation_v1/metal-physical.png) ·
[OpenGL reference](evidence/CameraFoundation_v1/opengl-reference.png) ·
[OpenGL physical](evidence/CameraFoundation_v1/opengl-physical.png)

Repeat with `--benchmark` in a fresh output directory to measure the shared
presentation path at 720p, 1080p and 4K. Use `--physical-camera --runtime-only
--prepare-only` with `--from-gallery` to prepare the authored-lens project
for Nutella export and relocation.

## Phase 1 native results

On Apple M4, the Debug and Release Metal 4 and OpenGL 4.1 runs all returned a
maximum display-linear error of `8.9407e-10` across the five FP64 physical
exposure cases. Each study verified exact RGB for equal gains, exact IDs and
depth, and isolation when switching back to the compatibility camera. The
existing bloom oracle reached `1.43051e-08`. The across-API bloom increment
was `1.19209e-07` at RGB p99 over 230,399 same-ID pixels. The one coverage
difference is also in the baseline. The Metal study retained AO, TAA, SSR,
two reflection probes and directional, point and spot shadows. Both builds
passed the queued reset/resize check on Metal and UI isolation on both APIs.

The CPU and editor regressions passed lens schema validation, exact
round-trip, FOV and exposure math, authored-state comparison, Play copy,
and Inspector Undo/Redo in both configurations. The initial development
round-trip identified a missing key in the central scene schema validator.
The final reader validates the physical lens for scene load, prefabs and
package auditing through the same decoder.

The eight prior AA-off bloom captures (two configurations, two backends,
on/off) matched byte-for-byte. Hazelnut and Nutella rendered and shut down
cleanly with the authored-lens project. The relocated Release Nutella
archive passed signature, source-free Metal, packaged GLSL and desktop
checks. The archive's native Metal run had 62 completed presentations and
zero GPU errors.

Phase 1 changes only the CPU calculation of the existing presentation
parameter. It adds zero GPU passes and zero GPU allocation. The second
offscreen Release run measured the shared pipeline below. Each state used
four warm-up frames and 15 measured samples. The first run showed large
timing variation, including a faster bloom-enabled 4K result despite the
added bloom passes. Both runs are retained, and the second run is consistent
with the earlier bloom baseline. This comparison measures the existing
pipeline and does not ascribe frame-time noise to physical exposure.

| Native API | Extent | Extract / down / up / present median ms | Frame median ms off → on | Bloom scratch MiB |
| --- | --- | --- | --- | ---: |
| Metal 4 | 1280×720 | 0.205 / 0.020 / 0.023 / 0.308 | 9.98 → 10.36 | 5.14 |
| Metal 4 | 1920×1080 | 0.420 / 0.030 / 0.039 / 0.656 | 21.23 → 21.92 | 11.08 |
| Metal 4 | 3840×2160 | 1.555 / 0.085 / 0.132 / 2.599 | 58.65 → 60.61 | 43.20 |
| OpenGL 4.1 | 1280×720 | 0.084 / 0.303 / 0.416 / 0.666 | 34.12 → 34.54 | 4.64 |
| OpenGL 4.1 | 1920×1080 | 0.088 / 0.322 / 0.413 / 1.138 | 37.27 → 38.54 | 10.44 |
| OpenGL 4.1 | 3840×2160 | 0.090 / 0.504 / 0.804 / 3.568 | 61.34 → 64.54 | 42.08 |

The offscreen Metal run completed all 114 submissions with zero GPU errors,
zero pending bytes and zero live attachment generations after shutdown. It
reported peak frame bytes of 1,708,463,240. The 4K workload has native AO and
spatial rendering on Metal, with TAA disabled for the full readback benchmark.
The 4K TAA/full-readback limit remains the historical bloom result. Native
desktop 2×, Windows, Linux and other GPUs were untested.
[Phase 1 results and raw timing index](evidence/CameraFoundation_v1/results.json)
