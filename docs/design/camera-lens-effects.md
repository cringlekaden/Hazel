# Lens effects

Phase 5 design. Lens effects are authored on the active camera and start at
zero strength. Exposure continues to meter the scene-linear world before
all optical passes. A flare source must come from rendered HDR highlights.
An occluded world light is therefore unable to inject flare by itself.

Lens glare reuses a reduced level of the existing HDR highlight pyramid.
The flare pass maps each output location back to candidate source locations
for the configured ghost reflections and halo. Offscreen source coordinates
contribute zero. Directional starburst and anamorphic response use bounded
samples of the same visible highlights. A flare-only view builds the
necessary downsample levels and omits unused bloom upsamples. The film
compositor adds the flare in scene-linear space before applying exposure and
Khronos Neutral. This is a screen-space optical approximation with visible
sources, inspired by Filament's source sampling. [Filament flare material](https://raw.githubusercontent.com/google/filament/main/filament/src/materials/flare/flare.mat)

Optional dirt is a camera-local texture mask that multiplies the glare by
`1 + strength × mask`. It has a neutral fallback and contributes no color
to an unlit image. Crop it to the viewport aspect and scale around its center.
It shares the scene's existing asset ownership and runtime loading.

Vignette scales radiance by a user-controlled fraction of the cosine-fourth
chief-ray falloff. Lateral chromatic response offsets the red and blue
sampling positions around the green chief ray, in resolution-scaled pixels.
Distortion maps destination rays into the original camera gate with a bounded
radial coefficient. The same mapping must apply to color, picking and depth.
Sensor grain is small zero-mean luminance-dependent noise in display-linear
space after Neutral. Screen UI follows all of these operations.

The qualification includes masked and unmasked highlights, the response to
visible emitter placement, glare-only dirt, non-square gates, neutral
settings, picking/depth under distortion and native GPU cost. Preserve the
established Neutral, scene-linear TAA, and depth-of-field/shutter order.
[Camera and image formation](camera-image-formation.md)

## Camera and compositing behavior

The camera owns `CameraComponent.LensEffects` version 1. Older cameras
use zero effect strengths. The editor stores the same settings as the
runtime and its history captures lens changes as part of the camera
transaction. Dirt uses an Assets-relative image path. The cache tracks
the asset store and its publication epoch. If the image is unavailable,
presentation uses unmodulated glare. Dirt adds no separate draw.

At the default setting the renderer does not build or retain lens targets.
For bloom-enabled shots, flare needs just one additional RGBA16F target
at one eighth the viewport width and height, rounded up. Flare-only
shots need the first three highlight-pyramid levels. At 3840×2160 their
combined texel payload with flare is 22,809,600 bytes. Metal also counts
allocation alignment and in-flight generations. There is no full-size
lens scratch. With all flare shapes enabled the shader takes at most
29 texture samples per output pixel.

With destination UV `p`, let `r² = 4 |(p - 0.5) × extent|² / |extent|²`.
The radial source sample is `0.5 + (p - 0.5) (1 + k r²) / (1 + max(k, 0))`,
where `k` is the authored distortion. IDs use nearest-source selection
and depth uses the original surface sample at the same pixel. Only a
warped view writes depth through presentation. Screen UI is drawn afterward
in undistorted coordinates. Chromatic shift is the offset of each outer
channel relative to green at the long edge of the gate, expressed in
1080p-equivalent pixels. Below one pixel it varies continuously with the
fragment and distance from the optical axis.

Screen-space glare has no offscreen emitter response and visibility comes
from the shaded image. Bright visible reflections can generate flare.
The model does not claim to represent individual coated optical elements.
Grain is formulated in the display-linear domain, with maximum variance
near the middle of its luminance range. Its per-view sequence resets on
an explicit camera cut, camera switch, scene switch and resize.

## Native qualification

The Apple M4 study runs the same shader source through Metal 4 and native
Apple OpenGL 4.1 in Debug and Release. The independent radial-map reference
found 1,690 changed source mappings and zero ID or depth disagreements.
The fractional red edge differed by at most `4.69e-6` on both backends.
The cosine-fourth reference differed by at most `3.58e-8` in linear display
value. Sensor grain had mean `-1.70e-5` and RMS `0.00575` for the fixed
gray test. It repeated exactly after a camera cut. The visible HDR emitter
produced a reflected ghost at the predicted location. The white dirt mask
increased its linear amplitude by exactly 1.5×. Full emitter occlusion
yielded zero glare and an unavailable texture gave the neutral fallback.
The authored camera ran through the runtime scene path with exact picking
and depth for the unwarped configuration. Screen UI remained exact at the
tested overlap.

All 144 frozen manual HDR color, depth and ID captures match byte for
byte. The built-in Scene Foundation and Editor tests passed the new
camera settings round-trip, asset path rejection, and Inspector Undo/Redo
in both configurations. A Metal fixture held all three command packets
while changing the dirt resource, resetting the view and resizing the
output. All three completed and matched serial color, ID and depth
exactly. The signed archive passed the relocated offscreen study with
sources and the SDK denied on Metal. The native desktop console was
locked during this checkpoint. OpenGL compute-only AO and TAA remain
unavailable. Other GPU and OS configurations require separate evidence.

[Native numerical results](evidence/CameraLensEffects_v1/numerical-results.json)
[Metal gallery](evidence/CameraLensEffects_v1/metal-gallery.png)
[OpenGL gallery](evidence/CameraLensEffects_v1/opengl-gallery.png)
[OpenGL visible ghost](evidence/CameraLensEffects_v1/opengl-flare.png)
[OpenGL ghost with dirt](evidence/CameraLensEffects_v1/opengl-dirt.png)
[Metal radial distortion](evidence/CameraLensEffects_v1/metal-distortion.png)

## GPU cost

Release used 31 interleaved pairs per resolution after four warmup pairs.
Both sides used the gallery with bloom, SSR, probes and shadows configured.
Metal also ran AO and TAA. The standard shot adds three ghosts, halo,
white dirt modulation and vignette. The all-options shot uses four ghosts
plus starburst, anamorphic streak, distortion, chromatic shift and grain.
The isolated flare timer measures the downsampled glare pass in an
otherwise simplified scene with the existing HDR highlight pyramid.

| Backend | Extent | Isolated flare standard / all, median ms | Standard frame delta median / p95 ms | All-options frame delta median / p95 ms |
| --- | --- | --- | --- | --- |
| Metal 4 | 1280×720 | 0.021 / 0.058 | 0.041 / 0.625 | 0.103 / 0.618 |
| Metal 4 | 1920×1080 | 0.022 / 0.088 | 0.025 / 0.228 | 0.085 / 0.400 |
| Metal 4 | 3840×2160 | 0.062 / 0.263 | −0.086 / 1.464 | 0.208 / 1.412 |
| OpenGL 4.1 | 1280×720 | 0.025 / 0.025 | 0.094 / 0.873 | −0.138 / 0.939 |
| OpenGL 4.1 | 1920×1080 | 0.024 / 0.022 | −0.163 / 1.440 | −0.067 / 1.036 |
| OpenGL 4.1 | 3840×2160 | 0.025 / 0.075 | 0.061 / 2.233 | 0.067 / 2.788 |

The complete-frame differences include background variation. Negative medians
indicate that the incremental work was below the resolution of those paired
samples. The standard runs met the master budgets. The first all-options
Metal sample set measured a 0.705 ms p95 at 720p against the 0.700 ms target.
The repeat after concurrent compilation ended measured 0.618 ms p95.
With bloom enabled, 4K
adds 1,196,032 Metal allocation bytes or 1,036,800 OpenGL texture payload
bytes, plus the 2×2 dirt image. The Metal standard run completed all 315
frames with zero GPU errors. The tracked attachment peak across both
full-effects benchmark views was 3,195,609,856 bytes and the renderer
retired all tracked generations.
[Metal standard samples](evidence/CameraLensEffects_v1/metal-standard-timings.csv)
[Metal all-options repeat](evidence/CameraLensEffects_v1/metal-all-options-timings.csv)
[Metal all-options first sample](evidence/CameraLensEffects_v1/metal-all-options-first-timings.csv)
[OpenGL standard samples](evidence/CameraLensEffects_v1/opengl-standard-timings.csv)
[OpenGL all-options samples](evidence/CameraLensEffects_v1/opengl-all-options-timings.csv)

## Reproduction

```sh
python3 scripts/hazel.py build --config Release --tests \
  --target MetalShaderTools --target MetalRenderer2DQuadSmoke --target MigrationRenderer2DSmoke
python3 scripts/macos/metal_shaders.py --config Release --output Hazel/Resources/shaders/metal
python3 scripts/macos/hdr_bloom.py --from-gallery 'build/ssr-gallery-project/SSR Gallery/SSR Gallery.hproj' \
  --prepare-only --runtime-only --automatic-camera --physical-camera --lens-camera --output build/camera-lens-source
python3 scripts/macos/hdr_bloom.py --project 'build/camera-lens-source/HDR Bloom Study/HDR Bloom Study.hproj' \
  --config Release --lens-study --output build/qualify-camera-lens
python3 scripts/macos/hdr_bloom.py --project 'build/camera-lens-source/HDR Bloom Study/HDR Bloom Study.hproj' \
  --config Release --lens-benchmark --output build/qualify-camera-lens-timing
```

Add `--lens-benchmark-full` for the optional higher sample count and
optical warp. Repeat the native study in Debug. Export the generated
project with `scripts/hazel.py editor-export` and qualify its ZIP with
`hdr_bloom.py --archive … --lens-study`. The automated archive check
also has an `--offscreen-only` option.
