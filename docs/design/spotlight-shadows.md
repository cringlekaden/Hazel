# Spotlight shadow maps

## Rendering and authoring

Opt in on an individual Spot Light. Existing scenes deserialize with shadows
off and a default shadow resolution of 512. Available map sides are 256, 512 and
1024. Enabled lights with positive incident RGB enter the existing Forward+
submission in ascending entity ID order. The first four shadow requests get
maps. Other spotlights keep their ordinary cone and range attenuation. The
editor Lights popup reports admitted and requested counts and atlas bytes.

Each light gets a perspective view oriented along local −Z with authored +Y
as the up reference. The depth pass translates caster models to light-relative
coordinates before multiplication, matching the view-independent receiver delta.
The square projection encloses the outer circular cone
with two texels of padding per edge. Near distance is range / 1024 and far
distance is range, both measured along the light axis. Radial range attenuation
continues to use the actual distance. The depth pass uses OpenGL clip depth
and the standard Metal conversion to [0, 1]. Virtual receiver rays evaluate
the geometric plane at individual texel centers, with a bounded grazing
correction. A normal offset and depth bias scale with the receiver's
perspective texel footprint. The receiver fades to fully lit from twice the
near distance to the near plane. Casters inside the near plane have no depth
representation.

The per-view D32F atlas is a horizontal row of up to four individually sized
tiles. It is allocated only for admitted requests. Allocated width is the sum
of tile sides and height is the largest side, at most 4096 × 1024 or 16 MiB
before driver overhead. Mixed resolutions can leave unused rows. Each tile
gets a viewport, scissor, and clamped 4 × 4 PCF footprint. The separate
directional atlas retains its cascade fitting and existing PCF. Both passes
use one scene caster list collected before camera culling, homogeneous
frustum culling and the same masked, mirrored and double-sided depth draw
path. Neither pass submits 2D or transparent geometry.

OpenGL orders depth writes and forward sampling on the context stream. Metal
attaches both producers to the consumer that samples them, including the
jittered HDR pass under TAA. Each producer holds its attachment generation
and draw data through completion. A queue barrier orders private atlas reuse,
and replacement creates a new generation. Admission or resolution changes
reset TAA history. Under motion, TAA uses its existing rejection and
neighborhood clamp.

The forward shader keeps each local light's existing three-vector record,
tile membership, and overflow behavior. A small separate uniform block maps
selected Forward+ indices to shadow projections and tiles. Visibility
multiplies incident attenuation before its contribution is added, preserving
the direct BRDF and independent IBL. The canonical GLSL drives both backends.

## Qualification contract

- Validate projected axial near and far, cone-edge coverage and pose invariance
  against FP64 geometry. Require normalized depth error below 0.0002, test
  homogeneous rejection of disjoint casters, and check mixed tile layout and
  deterministic admission.
- Use matched single- and two-light RGB renders and independent ray-box tests
  for ground pixels. Away from edges, require at least 98% of geometrically
  blocked and lit sample classifications to match each light separately.
  Require unoccluded receiver radiance within 0.003 per channel of the
  independent attenuation and accepted direct BRDF reference.
- Exercise texture cutouts, contact attachment, tilted-plane self-shadowing,
  cone/range limits, mixed map sides, a fifth light, Forward+ and full-loop
  equivalence, enable/disable, resize, and moving camera, caster and light.
  Preserve captures for inspection. Exercise both AO and TAA on Metal.
- Compare serialized and three concurrently queued Metal frames with moved
  casters/lights and changed map resolution, including busy cancellation and
  retirement. Require identical IDs/depth and color error under 1e-6 in the
  queued frame comparison.
- Record native cull, spot-map and forward GPU times for matched unshadowed
  controls and one, two and four shadow casters. Compare forward deltas to
  isolate the incremental sampling cost. Report extent, warmups, sample
  counts, median, p95, and validation settings. OpenGL uses a full local loop.
- Verify Debug/Release native Metal and GL, the canonical artifact and
  source-free execution. Preserve accepted PBR/IBL inputs and directional PCF
  behavior. Keep the existing strict CSM stress parity failure separate.

## Reproduction

Use the existing Premake workspace on an Apple Silicon Metal 4 host.

```sh
python3 scripts/hazel.py build --config Debug --tests \
  --target MetalRenderer2DQuadSmoke --target MigrationRenderer2DSmoke
python3 scripts/macos/spot_shadows.py --config Debug --output build/spot-qual-debug

python3 scripts/hazel.py build --config Release --tests \
  --target MetalShaderTools --target MetalRenderer2DQuadSmoke \
  --target MigrationRenderer2DSmoke --target Hazelnut --target Nutella
python3 scripts/macos/spot_shadows.py --config Release --output build/spot-qual-release
python3 scripts/macos/spot_shadows.py --config Release --backend metal --benchmark \
  --output build/spot-timing
python3 scripts/hazel.py package --app all \
  --project examples/RenderingReference/RenderingReference-SpotShadows.hproj \
  --output build/spot-archives
python3 scripts/macos/spot_shadows.py --config Release --archives build/spot-archives \
  --output build/spot-relocation
```

The fixture reads FP32 color, signed IDs and depth. The PNGs apply sRGB
encoding for inspection. The benchmark uses serialized offscreen captures at
1280 × 720, a receiver with 17 cube casters and 12 total local lights. It compares
the same one, two or four emitting lights with and without maps. Five frames
warm each group before fifteen timed frames. Validation is enabled for
correctness and disabled for timings. Forward delta includes shadow sampling and
its shader control flow. The OpenGL correctness path uses the Apple M4 4.1 core
driver and its full light loop. OpenGL phase timings were outside this screen.

## Native checkpoint on Apple M4

Debug and Release passed the focused Metal 4 and OpenGL 4.1 cases.
The 1,296 FP64 cone samples recorded maximum normalized axial depth error
of 3.67e-8. Maximum direct-light error was 4.08e-7. Each backend matched
7,066 geometrically blocked and 48,330 lit samples across the two lights.
There were 1,178 jointly blocked pixels and 48 contact pixels. The texture
cutout revealed 166 previously blocked pixels and kept 1,879 blocked.
Tilted-receiver error was at most 1.20e-7. Forward+ and the full loop matched
exactly in the two-light probe.

Across backends, the four controlled captures had identical IDs. The hard
fixture had one RGB discrepancy above 0.003 in 76,719 pixels at a shadow
edge. The 960 × 540 authored comparison had three such pixels and one
ID mismatch at a cutout silhouette. Release parity reports retain maxima
and p99 values. Metal AO changed authored shading with identical IDs,
and the moving fixture exercised TAA history reuse and reset on resize and
shadow disable. Both three-slot Metal tests passed, including before-completion
view destruction, busy cancellation and generation replacement. They retired
tracked resources to zero.

The separate Release CSM foundation regression passed its 24 analytic cases
and scoped atlas/temporal lifetime checks. The established strict full-frame
CSM stress gate remains FAIL. Its reference and thresholds are preserved.

| Spotlights | Map side | Atlas MiB | Cull ms | Map ms | Forward ms | Forward delta ms |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 1 | 256 | 0.25 | 0.025 | 0.014 | 2.002 | +0.920 |
| 1 | 512 | 1 | 0.025 | 0.025 | 2.199 | +0.720 |
| 1 | 1024 | 4 | 0.025 | 0.017 | 2.041 | +0.969 |
| 2 | 256 | 0.5 | 0.025 | 0.025 | 3.284 | +1.330 |
| 2 | 512 | 2 | 0.021 | 0.025 | 2.601 | +1.201 |
| 2 | 1024 | 8 | 0.018 | 0.015 | 2.204 | +0.671 |
| 4 | 256 | 1 | 0.017 | 0.025 | 3.157 | +0.704 |
| 4 | 512 | 4 | 0.016 | 0.024 | 3.069 | +1.291 |
| 4 | 1024 | 16 | 0.017 | 0.038 | 3.255 | +1.500 |

Values are native Metal medians. Four 1024 maps had p95 cull, generation
and forward intervals of 0.025, 0.079 and 4.686 ms. The ordinary local-light
culling path stays active with or without maps. Forward controls were
collected in consecutive groups and showed run-to-run variation. The CSV
preserves per-frame values and the summary includes every p95.

- [Release qualification](evidence/SpotShadows_v1/release.json) and [Debug qualification](evidence/SpotShadows_v1/debug.json)
- [Metal authored study](evidence/SpotShadows_v1/metal-study.png) and [OpenGL authored study](evidence/SpotShadows_v1/opengl-study.png)
- [Hard blockers](evidence/SpotShadows_v1/metal-hard.png), [texture cutouts](evidence/SpotShadows_v1/metal-masked.png) and [AO/TAA motion](evidence/SpotShadows_v1/metal-motion-1.png)
- [Native phase timing summary](evidence/SpotShadows_v1/timing.json) and [raw timing CSV](evidence/SpotShadows_v1/timing.csv)
- [Scoped directional PCF regression](evidence/SpotShadows_v1/directional-scoped.json)

Both Release archives passed digest/signature checks and launched the authored
scene with Metal and OpenGL from a relocated Unicode path with checkout and SDK
access denied. The relocated Metal numerical probe also passed with all GLSL and
MSL removed. Apple M4 native 2× presentation, Windows/Linux, and Intel hardware
execution were outside this checkpoint. Owner visual inspection is pending.
See the [archive relocation results](evidence/SpotShadows_v1/relocation.json).

The benchmark and captures use this implementation over base `d80bb5e`.
The current production shader package includes spotlight bindings and contains
no experimental PCSS programs. The [PCSS archive](directional-pcss.md) preserves
the earlier prototypes, failed gates and reproduction instructions.

Debug and Release scene and editor regressions passed the shadow settings
round trip, legacy defaults, prefab/copy and exact property Undo/Redo. The
Release multiscattering regression passed all 735 components on both
backends and its image parity gates. Frozen inputs and thresholds were
preserved. See the [authoring checks](evidence/SpotShadows_v1/authoring.json)
and [GGX regression](evidence/SpotShadows_v1/ggx-release.json).

The timing screen peaked at 111,724,976 tracked resource bytes and 40,001,792
attachment bytes, with zero tracked resource bytes after shutdown. These are
engine observer counts. The admitted 16 MiB atlas limit excludes pending
replacement generations and driver overhead.
