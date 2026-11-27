# Camera exposure preflight and Metal capacity gate

Phase 1 is published at `b7760975e680f3bfc3e01f115e0da3af32f5936c`.
The tests here exercise the renderer at that checkpoint. The only source
change for this investigation is a diagnostic in `HDRBloomQualification.h`.
The physical camera uses the existing presentation parameter, with no new
GPU work or resources.

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

Phase 2 production integration and qualification are incomplete. Phases
3–6 have the design record and remain unimplemented.
