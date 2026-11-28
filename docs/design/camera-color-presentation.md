# Camera color and presentation

## Ownership and frame order

The active camera can inherit scene exposure or supply an automatic or
manual physical policy. A camera bloom override controls enablement.
Bloom intensity and radius come from the scene. The camera owns physical
lens settings, depth of field, motion shutter, lens effects and
`CameraComponent.ColorGrade` version 1. Editor navigation uses scene
exposure and neutral camera color. Play and camera inspection use the
authored camera. Optional fields in earlier scenes inherit exposure and
bloom and use neutral grading.

The renderer completes the scene-linear world with AO, shadows,
reflections and TAA. Metering measures scene-linear Rec.709 luminance
before the optical effects. Motion blur precedes depth of field together
with the corresponding bloom input reconstruction. The world
presentation resolves optical rays, adds scene-linear bloom and flare
with optional dirt, and applies vignette, camera gain and white balance.
Khronos PBR Neutral maps this radiance into display-linear Rec.709.
Film controls and the optional LUT operate in that display-linear
domain. Grain then precedes the screen UI composite. The output
boundary encodes sRGB once. Transfer, tone mapping, picking and depth
keep their existing neutral-path contracts.
[Camera and image formation](camera-image-formation.md)
[Hazel color conventions](../color.md)

## White balance and grading

White balance describes the assumed illuminant in kelvin from 2000 to
12000. The Krystek fit gives the CIE 1960 `uv` Planckian locus. Hazel
transitions from Planckian to CIE daylight between 4000 and 5000 K with a
smoothstep. The daylight locus is anchored to D65 at 6504 K.
Tint is a signed distance in the CIE 1960 diagram along the normal to
the local illuminant locus. Positive tint models an illuminant with
more green and adds a magenta correction. Both the source white and
D65 are normalized to `Y = 1`. The renderer computes a Bradford
adaptation in FP64 on the CPU and sends its RGB matrix rows to the
compositor. The white-balance matrix preserves a reference-white
luminance and bounds the prepared gamut by flooring negative
radiance at zero before Neutral.
[Planckian fit](https://doi.org/10.1002/col.5080100109)
[CIE daylight reference](https://www.cie.co.at/publications/colorimetry-4th-edition)
[Linear Bradford adaptation and sRGB](https://www.w3.org/TR/css-color-4/)

After Neutral, saturation scales chroma around the Rec.709 luminance.
The grade then computes contrast about display-linear 0.5 followed by
separate shadow and highlight weights. Let `y` be the saturated
color's luminance, `c` contrast, `s` shadows and `h` highlights. The
scalar curve is

```text
t = y + 1.8 c y (1 - y) (y - 0.5)
z = t + 0.65 t (1 - t) [(1 - t) s + t h]
```

Both curve stages preserve endpoints and remain increasing for the
supported parameter ranges. A higher target luma mixes the chromatic
value toward white. A lower target scales it toward black. This
keeps the result in the output gamut after saturation clipping.

A 3D `.cube` LUT takes and returns display-linear Rec.709 RGB. Hazel
supports grid sizes 2 to 64, red-fastest triples, optional
`DOMAIN_MIN` and `DOMAIN_MAX` or `LUT_3D_INPUT_RANGE`, and finite output
triples in [0, 1]. One-dimensional shapers and encoded looks require
conversion to a single linear 3D table before import. The asset lives
under Assets. Hazel repacks it to a half-float 2D atlas with two hardware
bilinear plane samples and one blue-axis interpolation. A missing or
invalid asset leaves the other controls active and the LUT contribution
neutral. A 64³ table uses 2 MiB of texel payload. The disabled path
allocates no grade targets or LUT texture. Metal frame packets hold an
immutable texture snapshot until GPU completion.
[Blackmagic Design 3D LUT workflow](https://documents.blackmagicdesign.com/UserManuals/DaVinci_Resolve_10_Reference_Manual.pdf)

## Presets

Applying a preset writes camera values. The preset does not persist as
a special mode. Presets preserve composition, focus subject and distance,
grading, and the authored lens effects.

| Preset | Exposure policy | Photographic settings | Optical defaults |
| --- | --- | --- | --- |
| Automatic / Balanced | Center-weighted with standard rejection and adaptation | f/4, 1/64 s, ISO 100 | Scene bloom, Balanced blur quality, blur off |
| Cinematic | Automatic, with dark/bright transition rates 0.6/2.5 s⁻¹ | f/2.8, 1/48 s, ISO 100 | Bloom on, cinematic perspective DOF and 180° motion blur |
| Outdoor daylight | Average meter with sky excluded | f/8, 1/250 s, ISO 100 | Scene bloom, Balanced blur quality, blur off |
| Low light | Center-weighted meter rejects the upper 18%, dark transition rate 0.55 s⁻¹ | f/1.8, 1/48 s, ISO 800 | Scene bloom, Balanced blur quality, blur off |
| Manual photographic | Manual physical at the reference EV100 | f/4, 1/64 s, ISO 100 | Scene bloom, Balanced blur quality, blur off |

Automatic metering leaves aperture, shutter and ISO where the artist
sets them. Shutter and aperture can be authored with common numerical
presets. Selecting a physical shutter duration leaves motion blur's
independent enablement in place.

## Native reference study

The thermal fixture integrates Planck radiance over the CIE 1931
2-degree observer at 1 nm intervals, normalizes to `Y = 0.2` and
converts into linear Rec.709. Its RGB inputs are independent of the
production polynomial and Bradford matrix. The study also compares a
color patch against an FP64 film curve and an affine 17³ LUT with a
non-default input range. The observed absolute errors were
0.001015 for white balance, `2.71e-8` for film controls and
`9.07e-5` for the LUT. Both native backends agreed. The fixture
checks the signed tint, disabled and malformed LUT paths, camera
bloom authority and authored runtime settings. A combined shot
exercises automatic metering, DOF, shutter and flare with AO/TAA on
Metal and spatial SSR on both backends.
[CIE standard observer data](https://cie.co.at/datatable/cie-1931-colour-matching-functions-2-degree-observer)
[Independent reference generator](../../scripts/macos/camera_grade_reference.py)

In the lifetime fixture, Metal retains three queued frames while the view
switches LUT assets twice, resets and resizes its target. Captured color,
picking and depth match serial results exactly. The post-grade screen UI
comparison is exact in the overlay region.

The full native camera study and 144 frozen manual HDR captures passed
on both backends in Debug and Release. The captures are byte-identical
to the original fixture in color, ID and depth. The 68 automatic
captures also match the Phase 5 run byte for byte. The signed relocated
Release package passed on both offscreen backends and in Nutella's
desktop smoke tests with the authored camera. Metal completed 62 actual
presentations and OpenGL met the 60-rendered-frame gate. Both products
closed normally and retained their bundle signatures. The default HEAD
example snapshot hit the documented IBLQuality_v2 checksum failure
for `Materials/M0R3.hmat` before the corresponding rendering check.
The same mismatch is present in the Phase 5 commit. The established
immutable fixture `86c581c13fd5c944c2a06d720314a753d7607d8f` retains all
37 published hashes. The scene foundation, editor and runtime gates
passed against that immutable fixture in Debug and Release. The runtime
gate included normal Nutella startup, 60 rendered frames and teardown.
The Scene Foundation and Editor cases cover all five presets, camera
serialization, LUT path rejection and exact Undo/Redo.
[Native numerical register](evidence/CameraColor_v1/numerical-results.json)
[Metal combined, neutral grade](evidence/CameraColor_v1/metal-gallery-base.png)
[Metal combined, authored grade](evidence/CameraColor_v1/metal-gallery.png)
[OpenGL combined, neutral grade](evidence/CameraColor_v1/opengl-gallery-base.png)
[OpenGL combined, authored grade](evidence/CameraColor_v1/opengl-gallery.png)
[Historical checksum record](local-lighting.md)

## Reproduction

```sh
python3 scripts/hazel.py build --config Release --tests \
  --target MetalShaderTools --target MetalRenderer2DQuadSmoke --target MigrationRenderer2DSmoke
python3 scripts/macos/metal_shaders.py --config Release --output Hazel/Resources/shaders/metal
python3 scripts/macos/hdr_bloom.py --from-gallery 'build/ssr-gallery-project/SSR Gallery/SSR Gallery.hproj' \
  --prepare-only --runtime-only --automatic-camera --physical-camera --defocus-camera \
  --shutter-camera --lens-camera --grade-camera --output build/camera-color-source
python3 scripts/macos/hdr_bloom.py --project 'build/camera-color-source/HDR Bloom Study/HDR Bloom Study.hproj' \
  --config Release --grade-study --output build/qualify-camera-color
python3 scripts/macos/hdr_bloom.py --project 'build/camera-color-source/HDR Bloom Study/HDR Bloom Study.hproj' \
  --config Release --grade-benchmark --output build/qualify-camera-color-timing
```

Use `--grade-benchmark-scenario control` for a full-frame A/A control and
`--grade-benchmark-scenario combined` to retain the other authored camera
effects in the frame pairs. Repeat the numerical study in Debug.
Export the authored project with `scripts/hazel.py editor-export` and run
`hdr_bloom.py --archive … --grade-study` on its archive.

## Apple M4 GPU timing

Release used 31 interleaved pairs per extent after four warmup pairs.
The isolated measurement compares the same world-presentation pass
with and without white balance, film controls and a 17³ LUT. The
complete-scene bracket includes the gallery's bloom, probes, SSR and
shadows. Metal also enables TAA and AO. Both measurements use GPU
timing without bulk image readback.

| Backend | Extent | Composite delta median / p95 ms | Full-frame grade delta median / p95 ms | Full-frame A/A p95 ms |
| --- | --- | --- | --- | --- |
| Metal 4 | 1280×720 | 0.003 / 0.047 | −0.009 / 0.634 | 0.602 |
| Metal 4 | 1920×1080 | 0.002 / 0.117 | 0.122 / 0.693 | 0.636 |
| Metal 4 | 3840×2160 | −0.013 / 0.615 | 0.014 / 0.900 | 1.054 |
| OpenGL 4.1 | 1280×720 | 0.041 / 0.251 | −0.348 / 1.267 | 1.938 |
| OpenGL 4.1 | 1920×1080 | 0.008 / 0.374 | −0.108 / 1.891 | 1.188 |
| OpenGL 4.1 | 3840×2160 | 0.020 / 0.466 | 0.105 / 1.773 | 1.259 |

Negative paired medians show variation around a small effect cost. The
isolated composite passes the declared median and p95 budgets at every
resolution. In the standard full-scene fixture, 720p and 1080p paired
tails exceed the nominal targets on both backends. The A/A control
exceeds those same targets with grading held off in both complete
frames. Those full-scene tail estimates remain inconclusive.

With automatic exposure and the authored depth of field, motion blur,
flare, dirt, vignette and grade all enabled, full GPU frame medians were
11.494 / 23.653 / 89.609 ms on Metal and 35.225 / 50.303 / 73.660 ms on
OpenGL at 720p, 1080p and 4K. The combined p95s were
12.116 / 24.299 / 91.226 ms and 35.908 / 58.294 / 75.460 ms respectively.
The grade's 4K paired contribution in this fixture measured 0.614 ms
on Metal and 0.250 ms on OpenGL. The combined run reports gallery
throughput at the selected settings.

The 17³ test atlas occupied 147,456 Metal allocation bytes and
39,304 OpenGL texel bytes. With all selected camera effects enabled,
Metal's two paired views reached 3,470,287,616 tracked attachment
bytes. All 420 submissions completed with zero GPU errors and all
tracked attachment generations retired.
[Timing summary](evidence/CameraColor_v1/timing-summary.json)
[Metal standard](evidence/CameraColor_v1/metal-standard-timings.csv)
[OpenGL standard](evidence/CameraColor_v1/opengl-standard-timings.csv)
[Metal A/A](evidence/CameraColor_v1/metal-control-timings.csv)
[OpenGL A/A](evidence/CameraColor_v1/opengl-control-timings.csv)
[Metal combined](evidence/CameraColor_v1/metal-combined-timings.csv)
[OpenGL combined](evidence/CameraColor_v1/opengl-combined-timings.csv)
