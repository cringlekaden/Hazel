# Camera exposure qualification and Metal capacity history

Phase 1 is published at `b7760975e680f3bfc3e01f115e0da3af32f5936c`.
The preflight below records that checkpoint. It added a diagnostic in
`HDRBloomQualification.h`. At the time, the physical camera used the existing
presentation parameter without additional GPU work or resources.

## Metal 4 capacity

The fixture runs the HDR Bloom Study through Nutella's unbound scene →
`EncodeOutput()` → Metal drawable path. It uses a detached CAMetalLayer at
the measured physical extent and exercises AO, TAA, SSR, bloom, two
reflection probes and directional, point and spot shadows. It submits no
full-frame FP32, ID or depth readback. The queued variant holds GPU
completion with the existing test queue gate. The serial variant polls
completion between submissions. A zero timestamp on the detached layer
provides no desktop presentation evidence.

Apple M4, macOS 27.3, Release with API and shader validation:

| Workload | Result | Evidence |
| --- | --- | --- |
| 3840×2160, queued, full effects | **FAIL** | First frame submitted and completed. The second frame failed in world preparation with the existing 2 GiB storage-budget exception. The first frame reported 1,680,981,032 active-frame bytes and 1,530,700,032 attachment bytes. |
| 3840×2160, serial, full effects | **PASS** | Three frames completed, zero GPU errors, peak active-frame bytes 1,848,813,112. |
| 1920×1080, queued, full effects | **PASS** | Three in flight and three completed, zero GPU errors, peak active-frame bytes 667,772,408. |

With Metal validation disabled, queued full-effects 4K failed at the same
point. The otherwise identical 4K spatial control with TAA disabled held
three frames in flight and completed all three without GPU errors. Its peak
active-frame bytes were 1,839,699,112. The spatial run isolates the additional
pressure associated with enabling the current temporal path.

The active-frame and attachment counters overlap. Admission uses the union of
owned, idle and in-flight allocations. The counters above cannot be added to
calculate the rejected reservation size. The failure happens during world
preparation, before the second output pass or drawable presentation. It
blocked the first attempt at Phase 2. The owner subsequently authorized
replacing the fixed ceiling.
[Metal memory admission](metal-memory-admission.md)

### Reproduce

Prepare the authored HDR study using the
[camera foundation procedure](camera-image-formation.md). Build
`MetalRenderer2DQuadSmoke` through Premake using:

```sh
python3 scripts/hazel.py build --config Release --tests --target MetalRenderer2DQuadSmoke
```

The diagnostic uses the same resource and Mono paths as the HDR bloom runner.
Run from the repository root with a separate destination per case:

```sh
for admission_case in 4k-queued 4k-serial 1080p-queued; do
  DYLD_LIBRARY_PATH="$PWD/build/dependencies/mono/macos-arm64/sdk/lib" \
  HAZEL_CAMERA_ADMISSION="$admission_case" HAZEL_HDR_BLOOM=1 \
  HAZEL_REFLECTION_DIAGNOSTIC=1 HAZEL_METAL_TRACE=1 \
  MTL_DEBUG_LAYER=1 MTL_SHADER_VALIDATION=1 \
  HAZEL_RENDERING_REFERENCE_PROJECT="$PWD/build/camera-foundation-runtime/HDR Bloom Study/HDR Bloom Study.hproj" \
    "$PWD/bin/Release-macos-arm64/MetalRenderer2DQuadSmoke/MetalRenderer2DQuadSmoke" \
    "$PWD/Hazel/Resources" "$PWD/build/camera-exposure-spike/repro-$admission_case" --reflection-probes
done
```

Raw logs are in `build/camera-exposure-spike/confirmed-{4k-queued,4k-serial,1080p-queued}.log`.
To reproduce the validation-disabled comparison, run `4k-queued` and
`4k-queued-spatial` with `MTL_DEBUG_LAYER=0 MTL_SHADER_VALIDATION=0`.
The corresponding logs use the `no-validation-` prefix.

## OpenGL 4.1 metering prototype

A standalone native probe used the repository GLFW library and Apple's
OpenGL 4.1 driver. Raster points wrote mass to a sharded 128×16 R32F
histogram with additive blending. All bin counts matched an independently
counted three-band test image. A second shader trimmed the histogram and
advanced a 1×1 ping-pong adaptation texture. The isolated 128×72 histogram
median was 0.088 / 0.065 / 0.059 ms at 720p / 1080p / 4K. These queries use
completion for timing attribution. Test readbacks transfer the histogram
or one pixel for validation only.

Trimming the outer 25% on either side yielded −0.411431 gain stops, 0.0625
stops from the unquantized FP64 target. Both adaptation directions matched
the FP64 exponential at 30, 60 and 120 Hz, with maximum error 4.631×10⁻⁶
stops against the binned target. This is a numerical and API feasibility
result. The probe has no integration with SceneRenderer, per-view
ownership, presentation, camera cuts, authored settings or scene defaults.
Prototype source and log are in
`build/camera-exposure-spike/gl_histogram.cpp` and `gl_histogram.log`.

## After the memory correction

The first Metal 4 metering feasibility probe runs 72 workgroups of 128
samples and reduces the 128-bin histogram into GPU ping-pong gain buffers.
It measures the same three-band HDR field as the OpenGL prototype. The
Metal and OpenGL bins match independent counts exactly. Both reach the
same target and follow the FP64 trace at 30, 60 and 120 Hz with a maximum
adaptation error below 4.7×10⁻⁶ stops. Native Metal GPU validation passed.
Release medians for the isolated meter and adaptation command buffer
were 0.030 / 0.028 / 0.031 ms at 720p / 1080p / 4K.

Sources and execution logs are in `build/camera-exposure-spike/metal`.
The probe has explicit barriers and production-style Metal 4 submission,
with CPU waits and shared readback only for feasibility validation. It is
independent of scene rendering.

The Phase 2 manual migration was qualified separately. It retains signed compensation when reading legacy
`Presentation.Exposure`, writes only the versioned representation, and
rejects documents containing both. The Release scene regression passed.
Changing only the exposure representation in a copy of the HDR Bloom Study
preserved all 78 captures byte-for-byte across Metal and OpenGL.

## Production Phase 2

Scene Settings owns the default exposure policy for scene-referred views.
A primary camera can override that policy as a whole. HDR starter projects
select Automatic. Existing saved defaults and scenes retain their authored
manual compensation through a read-only legacy scalar migration. Display-
referred scenes keep their existing direct presentation.

The renderer samples up to 128×72 stable strata from the unexposed world
after TAA and world 2D. Both backends use one deterministically jittered
pixel per stratum and 128 log-luminance bins over −24 to +24. Small targets
sample each texel at most once. Metering rejects invalid RGB in the meter,
clamps finite negative channels to zero and caps extreme luminance before
taking the logarithm. The sky option uses clear depth in the world buffer.
Spot controls use top-left screen coordinates and a short-edge-relative
radius. Spot weight falls to zero at the authored radius. Percentile rejection
accounts for fractional mass at the boundaries, and both backends accumulate
integer weights so the mass is exact at the selected grid size.

Metal 4 builds one histogram row per threadgroup and reduces it on compute.
OpenGL 4.1 blends point contributions into 16 histogram rows and resolves one
fragment. Presentation reads a single gain texel. Both maintain two GPU gain
versions per render view. History is valid only after submission and is
invalidated by camera or scene identity, explicit cuts, view reset, and
extent changes. A valid black image reaches a bounded target. An empty mask
holds prior gain or uses neutral gain until the first valid sample. Rates
are applied in stops with the exact exponential for the elapsed frame time.
Manual mode and renderer diagnostic views omit both meter passes.

### Native qualification

Apple M4, macOS 27.3, Metal 4 and Apple's OpenGL 4.1 driver. Native Debug
and Release passed the camera and HDR bloom study. Metal ran with API and
shader validation. Its full-effects case included AO, TAA, SSR, two
reflection probes and all three shadow types. OpenGL used its established
spatial path. All 144 frozen manual study captures (36 per build/backend)
matched byte for byte. AA-off bloom increment RGB p99 across backends was
`1.1920928955078125e-07` over 230,399 matching pixels, with one picking
mismatch out of 230,400 as covered by the established gate.

| Independent test | Metal 4 | OpenGL 4.1 |
| --- | ---: | ---: |
| Maximum uniform startup error, stops | 1.59e−7 | 9.70e−8 |
| Maximum asymmetric adaptation error, stops | 5.16e−7 | 5.16e−7 |
| Maximum 30/60/120 Hz drift at matched times, stops | 4.59e−7 | 4.88e−7 |
| Manual physical reference, maximum display-linear error | 8.95e−10 | 8.95e−10 |

The production fixture also passed trimmed hot/cold patches, top-left spot
orientation, empty/sky-only/black frames, lock, camera cuts, camera switch,
odd and single-pixel resize, and independent view history. Three Metal
frames held in flight matched the serial exposure results exactly. Debug
and Release Scene Foundation and Editor Smoke passed migration, component
round trip, Undo/Redo and Play/Stop. The focused native TAA tests passed
their numerical gates in both builds. Separate TAA visual acceptance
remains open in that suite. The authored automatic camera also ran in a
relocated, signed, source-free Nutella export on both backends with normal
startup/shutdown and unchanged signature.

The native captures show the shadowed room and bright emissive accents
at the same pose. [Metal automatic](evidence/CameraExposure_v1/metal-auto.png)
[Metal manual](evidence/CameraExposure_v1/metal-manual.png)
[OpenGL automatic](evidence/CameraExposure_v1/opengl-auto.png)

The independent adaptation traces give the measured and reference stop
value for every frame of each transition.
[Metal trace](evidence/CameraExposure_v1/metal-adaptation.csv)
[OpenGL trace](evidence/CameraExposure_v1/opengl-adaptation.csv)

The fixture covers a moving interior/exterior camera and controlled extreme
luminance patches. Dedicated outdoor daylight and night walkthroughs, observer
studies, and qualification on Windows, Linux, Intel and other GPUs remain
unverified. Single-frame targets are intentionally bounded by studio defaults
and authored gain limits. Automatic exposure operates on Hazel's relative
luminance and does not calibrate legacy light intensities as physical units.

### GPU cost

Release on Apple M4. Each mode has four warm-up frames and 15 measured
frames at each extent. The isolated diagnostic times histogram and
adaptation. The complete frame includes bloom and the backend's supported
effects, with exposure diagnostics disabled. Metal uses native timestamps
and queue feedback. OpenGL uses elapsed queries; the isolated query
completes its two meter draws for attribution.

| Backend / extent | Meter median / p95 ms | Manual frame median ms | Automatic frame median ms | Paired frame delta median / p95 ms |
| --- | ---: | ---: | ---: | ---: |
| Metal 4 720p | 0.022 / 0.026 | 10.664 | 10.604 | −0.015 / 0.124 |
| Metal 4 1080p | 0.032 / 0.040 | 22.444 | 22.465 | +0.036 / 0.162 |
| Metal 4 4K | 0.025 / 0.030 | 85.919 | 86.044 | +0.087 / 1.459 |
| OpenGL 4.1 720p | 0.069 / 0.086 | 35.021 | 35.199 | +0.174 / 1.194 |
| OpenGL 4.1 1080p | 0.022 / 0.094 | 38.632 | 38.730 | −0.052 / 0.545 |
| OpenGL 4.1 4K | 0.091 / 0.128 | 64.946 | 65.011 | −0.039 / 1.346 |

Meter cost and complete-frame median deltas were within the Phase 2
ceilings. The complete-frame p95 deltas at Metal 4K and OpenGL 720p
exceeded twice the median phase ceilings. Whole-frame variation was
larger than the isolated meter time, and some paired median deltas were
negative. Treat those tail deltas as inconclusive measurements of incremental exposure cost.
[Metal samples](evidence/CameraExposure_v1/metal-timings.csv)
[OpenGL samples](evidence/CameraExposure_v1/opengl-timings.csv)

Metal scratch was 196,864 allocated bytes per view, including padded
texture allocations. OpenGL scratch was 32,800 bytes of texture payload
per view, with driver overhead unmeasured. Both fit the 1 MiB design cap.
The Metal full-effects timing run completed 171 submissions with zero
errors and zero pending bytes or live attachment generations at shutdown.
The device-aware admission policy remains in effect.

### Reproduce production checks

Use the authored HDR Bloom Study from the
[foundation procedure](camera-image-formation.md). Build the smoke target
for both configurations and run the focused study. Substitute an unused
output directory when repeating.

```sh
for config in Debug Release; do
  python3 scripts/hazel.py build --config "$config" --tests --target MetalRenderer2DQuadSmoke
  python3 scripts/macos/hdr_bloom.py --project 'build/hdr-bloom-project/HDR Bloom Study/HDR Bloom Study.hproj' \
    --config "$config" --output "build/qualify-exposure-$config"
done
python3 scripts/macos/hdr_bloom.py --project 'build/hdr-bloom-project/HDR Bloom Study/HDR Bloom Study.hproj' \
  --config Release --exposure-benchmark --output build/qualify-exposure-timing
```

Use `--from-gallery` with `--prepare-only --runtime-only --physical-camera
--automatic-camera` to generate the automatic package fixture. Export it with
`scripts/hazel.py editor-export` and run the same study script with
`--archive` for relocated source-free checks.
