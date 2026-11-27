# HDR bloom and manual exposure

## Render order

`SceneRenderer` retains the same submission path for Hazelnut and Nutella.
Directional cascades and the local-light atlases supply visibility to the
forward pass. Metal's XeGTAO uses a depth prepass and feeds the forward
lighting pass. Environment, direct lighting, material emission, global IBL
and accepted static probe data form the scene-linear Rec.709 HDR image.
SSR uses opaque FP32 radiance, depth, signed IDs and surface data, replacing
the corresponding probe/global specular fallback by hit confidence. Metal
TAA resolves jittered opaque radiance in the same scene units, before
unjittered world-space 2D. Bloom reads that world image after world 2D.

Presentation adds filtered highlight radiance, applies manual exposure, and
runs the existing Khronos PBR Neutral curve. This leaves a display-linear
image while preserving alpha, picking, and depth. Screen UI and editor
overlays draw after this step. The output pass applies the sRGB transfer
function once. The editor selection outline is drawn into encoded output
using resolved IDs. Native probe captures and renderer inspection views
bypass bloom. The display-referred path bypasses HDR presentation.

## Filter and controls

A threshold-free pyramid spreads all scene energy, including the midtones
and large dark-room surfaces. A single wide dual filter is cheaper to tune
but has less control over near-source concentration and broad falloff.
Hazel uses a normalized multiscale pyramid, following the overlapping
multiresolution filtering approach in [Jimenez's post-processing
presentation](https://www.iryoku.com/next-generation-post-processing-in-call-of-duty-advanced-warfare/).
The soft highlight response keeps midtones out of the halo while the weighted
scales give a smooth, tunable falloff.

- The first pass extracts linear Rec.709 luminance from four FP32 texels and
  averages them into one half-resolution RGBA16F texel. With luminance `Y`,
  the extracted luminance is `max(Y - 1, 0.5 * clamp(Y - 0.5, 0, 1)^2)`.
  This preserves RGB ratios and has continuous slope at both ends of the
  knee. A hue-preserving shoulder on the bloom branch starts at a peak
  channel of 32 and approaches 64 to bound exceptional highlights.
- Further levels halve the dimensions, rounding up. A five-tap filter
  combines the center and four bilinear diagonals. A four-tap bilinear
  filter expands the coarser result and blends it into the matching finer
  level. The radius advances the coarse weight from 0.18 to 0.68. Both
  filters use nonnegative weights that sum to one. Four levels are used
  for a short edge up to 1440 physical pixels, with a fifth at larger
  extents. Edges clamp to the viewport.
- Intensity ranges from 0 to 0.25 and defaults to 0.08. Bloom starts off in
  existing scenes. The result is added in scene units. The world input, the
  lighting BRDF and the probe bakes retain their existing values.
- The world and bloom receive the same `exp2(Exposure)` gain. Exposure is
  compensation in signed stops relative to Hazel's existing scene scale.
  It preserves the serialized `Exposure` field and the [-16,16] range.
  It has no absolute photometric camera calibration. A positive stop doubles
  the radiance presented to the tone mapper. The bloom onset stays in scene
  units, so a change in EV scales the scene and halo together.
- Emission is authored as linear color times **Emissive strength**, a
  multiplier from 0 to 1000. A missing field resolves to 1. The multiplier
  is applied before uploading the existing emission uniform.
- Temporal history consumes unexposed opaque radiance before bloom.
  Changing EV, bloom intensity, radius or enable keeps history valid.
  A bounded automatic exposure scheme would require luminance metering,
  per-view adaptation and explicit cut/pause/restart behavior. The current
  presentation boundary leaves room for a later gain source without
  involving TAA or probe bakes. A suitable follow-up would meter unexposed
  luminance before bloom, clamp the target EV, and adapt with separate
  brightening and darkening time constants. It needs a bounded GPU
  reduction and a portable completion strategy for OpenGL 4.1.

## GPU ownership

Each render view owns a lazily allocated pyramid. A resize builds a complete
replacement before installing it. Disabling bloom or setting zero intensity
releases the view's scratch. OpenGL uses distinct single-sample textures and
ordered draws. Metal captures immutable attachment identities in the retained
frame packet, includes them in residency and the renderer budget, and orders
each write after earlier readers on the queue. Scratch never enters picking,
readback, or temporal history. Metal frames already in flight retain their
textures through completion.

At common extents the color payload is approximately 4.9 MB at 1280×720,
11.0 MB at 1920×1080, and 44.2 MB at 3840×2160 before allocation alignment.
A filter pass does not allocate a full-resolution bloom target. The only
full-resolution bloom work is the extra sample in the existing presentation
pass. At zero intensity that sample is bypassed. Very small highlights can
still inherit sampling variation from the source rasterization and SSR.
OpenGL 4.1 has no Hazel XeGTAO or TAA implementation.

## Reproduce the study

Generate a prepared SSR Gallery using the
[SSR study procedure](screen-space-reflections.md), then run:

```sh
python3 scripts/hazel.py build --config Release --tests \
  --target MetalRenderer2DQuadSmoke --target MigrationRenderer2DSmoke
python3 scripts/macos/hdr_bloom.py \
  --from-gallery 'build/ssr-gallery/SSR Gallery/SSR Gallery.hproj' \
  --motion --output build/hdr-bloom-study
python3 scripts/macos/hdr_bloom.py \
  --project 'build/hdr-bloom-study/HDR Bloom Study/HDR Bloom Study.hproj' \
  --benchmark --output build/hdr-bloom-timing
```

The generated project can be opened in Hazelnut or run in Nutella. The study
adds an open sunlit court, a roofed room, bright emissive tubes, a glossy
dielectric, and all three shadow types. It retains the two accepted static
probes and SSR. The fixture moves the camera and the warm tube along the
same track in both modes. The Metal track also runs AO and TAA.

## Qualification

The gates cover native Debug/Release, matching AA-off bloom response across
APIs, 27 FP64 uniform source/exposure cases, unchanged IDs/depth, deterministic
AA-off output, zero-intensity behavior, isolated UI, continued temporal history
through live exposure edits, and three queued Metal submissions through
reset/resize. Performance uses warmed serial offscreen samples at 720p, 1080p
and 3840×2160. The 4K timing case uses spatial rendering with AO active on
Metal. The original 4K Metal workload with TAA and full FP32 readback reached
the renderer's 2 GiB budget with bloom disabled. GPU phase timestamps measure completion intervals and may
include synchronization. Metal full-frame submissions include the readback.
The OpenGL elapsed query covers world rendering through display composite.
OpenGL phase queries use separate samples and explicit completion boundaries
because Apple's batched queries returned zero for several phases. These
phase samples include isolation overhead and driver attribution effects.
They should be read alongside the non-isolated full-frame measurements.

For a runtime-only archive, generate another copy with
`--runtime-only --prepare-only`. This retains the accepted probe generations
and omits producer inputs. Export and qualify it with:

```sh
python3 scripts/hazel.py editor-export \
  'build/hdr-bloom-runtime/HDR Bloom Study/HDR Bloom Study.hproj' \
  --name HDRBloomStudy --output build/hdr-bloom-export
python3 scripts/macos/hdr_bloom.py \
  --archive build/hdr-bloom-export/HDRBloomStudy-macos-arm64-Release.zip \
  --output build/hdr-bloom-relocation
```

### Matched captures

These captures are from the Apple M4 Metal study with AO, TAA, SSR, two
probes and directional, point and spot shadows enabled. Both modes use the
same scene, exposure and camera trajectory.

[Bloom off](evidence/HDRBloom_v1/off.png) ·
[Bloom on](evidence/HDRBloom_v1/on.png) ·
[Matched motion, off left and on right](evidence/HDRBloom_v1/matched-motion.png)

### Native results

Debug and Release passed on Apple M4 with Metal 4 and Apple's OpenGL 4.1
driver. The 27 FP64 uniform checks reached a maximum display-linear error of
1.43×10⁻⁸. The AA-off bloom increment matched at RGB p99 1.19×10⁻⁷ over 230,399
same-ID pixels, with one native cross-API coverage difference. Within each
backend, enabling bloom preserved all IDs and depth. The one-pixel source
retained the same integrated halo energy across the four sampling phases and
the Metal disocclusion fixture registered zero residual. The moving Metal
study advanced TAA across bloom and EV edits. Three gated Metal submissions
survived a view reset and odd resize with exact results.

Debug and Release passed the frozen IBL, PBR/IBL parity, GGX/Kulla–Conty,
SSR, TAA/AO, and combined-shadow regressions. The near-field probe trace
reproduced with the bloom changes. The editor regression exercised Apply,
Undo/Redo, Save/reopen, and Play copy. The runtime-only export passed package
signature and content checks, source-free Metal rendering, packaged GLSL
rendering, and relocated desktop smoke. Hazelnut and Nutella completed at
least 60 Metal presentations and shut down cleanly. OpenGL passed the
rendered-frame and clean-shutdown smoke checks.

### GPU and memory

Release, four warm-up frames, 15 samples per state. The phase columns show
median elapsed milliseconds. The final presentation column includes the
existing Neutral pass. Frame columns show median elapsed milliseconds and the
paired increase reports median / p95. Metal bytes are aligned allocation
sizes and OpenGL bytes are texture payload.

| API | Physical pixels | Extract | Down | Up | Present | Frame off → on | Paired increase | Scratch |
| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Metal 4 | 1280×720 | 0.202 | 0.020 | 0.022 | 0.307 | 10.60 → 10.61 | 0.27 / 0.64 | 5.14 MiB |
| Metal 4 | 1920×1080 | 0.409 | 0.029 | 0.038 | 0.664 | 22.17 → 22.77 | 0.62 / 1.15 | 11.08 MiB |
| Metal 4 | 3840×2160 | 1.572 | 0.086 | 0.132 | 2.659 | 61.80 → 64.46 | 2.72 / 7.39 | 43.20 MiB |
| OpenGL 4.1 | 1280×720 | 0.032 | 0.298 | 0.367 | 0.600 | 33.08 → 33.44 | 0.39 / 9.13 | 4.64 MiB |
| OpenGL 4.1 | 1920×1080 | 0.032 | 0.326 | 0.336 | 1.064 | 36.19 → 37.73 | 1.29 / 9.16 | 10.44 MiB |
| OpenGL 4.1 | 3840×2160 | 0.059 | 0.589 | 0.854 | 3.717 | 61.21 → 63.92 | 3.26 / 3.76 | 42.08 MiB |

The full-effects 4K Metal FP32 readback remains blocked by the renderer
budget. The 4K row covers Retina-equivalent physical pixels offscreen. Native
2× desktop presentation, Windows, Linux, and Intel GPU execution were
unexercised. Owner visual acceptance remains pending.

[Machine-readable qualification](evidence/HDRBloom_v1/results.json) ·
[Metal GPU samples](evidence/HDRBloom_v1/metal-timing.csv) ·
[OpenGL GPU samples](evidence/HDRBloom_v1/opengl-timing.csv)
