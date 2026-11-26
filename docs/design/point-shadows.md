# Omnidirectional point-light shadows

Point lights use six axis-aligned, padded perspective faces in one 3×2 D32F
atlas. Native cubemaps have equivalent storage and can offer seamless hardware
sampling of cube color textures, but existing Hazel depth targets, viewports,
Metal packet capture and OpenGL 4.1 bindings already support a 2D atlas.
Hardware face filtering also cannot replace independent depth comparisons
against the geometric receiver plane. An atlas keeps the depth pass, MASK,
winding, and lifetime identical to the spotlight path and makes the seam
footprint explicit in canonical GLSL.

The first enabled, emitting shadow request in scene entity ID order gets a
map. Sides are 256 (default) or 512, for 1.5 or 6 MiB per current view atlas.
Excess requests retain ordinary Forward+ illumination. Pending generations
are held by existing Metal completion slots. World axes define the faces,
independent of light rotation, camera and temporal jitter. Light-relative
submission avoids multiplying absolute world positions by the perspective
matrix. Homogeneous caster rejection runs per face, and empty caster lists
skip depth draws. The near plane is range/1024 per axis and the far plane is
the axial range. Incident light still uses spherical range and the existing
attenuation. A short radial fade near the light avoids reading clipped depths
at corners.

A bilinear four-comparison filter projects geometric receiver depth along
individual texel rays with bounded grazing correction. Only axes within
roughly two face texels of the dominant axis contribute to the filtered result. Face
weights depend on their ratio to the dominant magnitude and are normalized,
so at equal magnitudes the same maps and weights contribute from either
side, including at cube corners. The guards enclose all these footprints.
Visibility scales incident point radiance after the ordinary range and
facing tests. Unshadowed lights do no depth sampling.

## Qualification contract

- Independent double-precision checks for all six face orientations, axial
  near/far depth, overlap, clipping and range. Use the spotlight normalized
  depth tolerance of 0.0002 and retain all previous reference thresholds.
- Native Metal and OpenGL shadow-on/off captures from all six directions and
  adjacent edges, with independent ray/box visibility on stable interiors.
  Require at least 98% agreement for lit and blocked samples separately.
  Measure seam transitions, contact, grazing and MASK cases and compare
  Forward+ against the full light loop. Keep known CSM stress and cutout
  silhouette exceptions attributable to their original fixtures.
- Exercise resize, moving lights/casters/cameras, mixed spot and point
  requests, second point light admission, AO/TAA, overlapping Metal frame
  slots and resource retirement, and source-free relocated artifacts.
- Collect native Metal timings at 720p, 256 and 512, with matched disabled
  controls and at least five warmups and fifteen samples per case. Report
  generation, culling, forward and memory separately. Capture presented
  timing and physical Retina admission separately from offscreen probes.

## Native Apple M4 results

The final shader uses affine **reciprocal axial depth** at the four tap centers.
An initial implementation clamped a per-tap axial correction and produced
0.0060 peak RGB difference on an unobstructed grazing receiver. Deriving depth
from the receiver plane in reciprocal space reduced the difference to zero on
Metal and 2.98e-8 on OpenGL. At nearly edge-on incidence the reciprocal slope
is bounded. Map edges stay hard at distance, with a fixed texel-scale filter.
The 256 tier trades visible edge precision for one quarter of the 512 storage.

Debug and Release passed the scoped Metal 4 and Apple OpenGL 4.1 probes.
The FP64 projection sweep reached 2.29e-8 maximum normalized depth error.
Each backend matched all 1,064,518 stable lit and blocked classifications,
including 8,322 lit and 4,128 blocked samples in the seam bands. The MASK
probe revealed 278 samples and kept 7,318 blocked. The contact strip checked
63 pixels. The tests verified near/far clipped blockers, spherical range,
mixing with a spotlight, Forward+ equality with the full loop, deterministic
request exhaustion, and AO/TAA under motion and resize. Queued Metal results
matched serial frames and retired both the shared and replacement atlases
after completion, with busy cancellation exercised. Debug and Release
controlled captures matched RGB within 9.06e-6 and had exact IDs and depth.

The authored 960×540 study had one cross-backend ID discrepancy at the
masked silhouette, present with point shadows on and off at the same pixel.
The point-shadow-enabled capture had 36 pixels above 0.003 RGB, including
35 receiver pixels at shadow edges. The disabled control had three pixels
above that threshold. The enabled capture's p99 was 2.60e-6. The focused
parity checks and the existing frozen references use their original inputs
and thresholds.

| Point face side | Current atlas MiB | Cull ms | Generation ms | Forward ms | Paired forward delta ms |
| ---: | ---: | ---: | ---: | ---: | ---: |
| 256 | 1.5 | 0.0178 | 0.0141 | 5.778 | +0.436 |
| 512 | 6 | 0.0173 | 0.0242 | 5.758 | +0.400 |

These are medians at 1280×720 with 17 casters and 12 emitting local lights.
The control and enabled views alternate after five pairs of warmups and
retain separate view resources. Each row contains 15 pairs. The p95 paired
forward deltas were +0.793 and +0.615 ms. Map generation p95 was 0.549 and
0.030 ms, including one visible 256 timing excursion. The same executable
using the spotlight revision's shaders gave 5.202 and 5.201 ms unshadowed
forward medians, compared with 5.337 and 5.356 ms for the current disabled
controls. These consecutive runs show a 0.14–0.16 ms difference on this
fixture. The timings support retaining one point map. Larger scenes and
further point slots require separate measurements.

The presented study used two lights with AO/TAA off and the 512 point map
toggled. After discarding 30 positive presentations per extent, 91–93 frames
per case remained. Whole-frame GPU medians were 2.796 → 3.154 ms at 720p
and 5.880 → 6.651 ms at 1080p. Presented interval medians were
3.755 → 3.389 ms and 5.813 → 6.614 ms on the 180 Hz display, with compositor
variation. The separate 3200×1800 editor-target check presented 24 completed
frames while cycling AO, TAA, point admission and map sizing. It peaked at
1,592,522,944 tracked bytes across all view resources and retired to zero.
The attached display had 1× backing scale. The high-resolution test exercised
the render-target memory workload and editor presentation on that display.

## Reproduction

```sh
python3 scripts/hazel.py build --config Debug --tests \
  --target MetalShaderTools --target MetalRenderer2DQuadSmoke --target MigrationRenderer2DSmoke
python3 scripts/macos/metal_shaders.py --config Debug --output Hazel/Resources/shaders/metal
python3 scripts/macos/point_shadows.py --config Debug --output build/point-debug

python3 scripts/hazel.py build --config Release --tests \
  --target MetalShaderTools --target MetalRenderer2DQuadSmoke --target MigrationRenderer2DSmoke \
  --target MetalM1Smoke --target Hazelnut --target Nutella \
  --target SpriteAssetAudit --target PackageAudit --target HazelProject
python3 scripts/macos/point_shadows.py --config Release --output build/point-release
python3 scripts/macos/point_shadows.py --config Release --backend metal --benchmark \
  --unshadowed-baseline 58fd90c3ef02177a7e16647531f597ac8fb54f38 --output build/point-timing
python3 scripts/macos/point_shadows.py --config Release --presented --output build/point-presented

python3 scripts/hazel.py package --app all \
  --project examples/RenderingReference/RenderingReference-PointShadows.hproj --output build/point-archives
python3 scripts/macos/point_shadows.py --config Release --archives build/point-archives --output build/point-relocation
```

- [Debug qualification](evidence/PointShadows_v1/debug.json) and [Release qualification](evidence/PointShadows_v1/release.json)
- [Metal study](evidence/PointShadows_v1/metal-study.png) and [OpenGL study](evidence/PointShadows_v1/opengl-study.png)
- [Three-face corner](evidence/PointShadows_v1/corner.png), [contact](evidence/PointShadows_v1/contact.png) and [texture MASK](evidence/PointShadows_v1/masked.png)
- [Offscreen phase timings](evidence/PointShadows_v1/timing.json), [paired samples](evidence/PointShadows_v1/timing.csv) and [baseline samples](evidence/PointShadows_v1/timing-baseline.csv)
- [Presented timings and editor admission](evidence/PointShadows_v1/presented.json)
- [Debug and Release authoring](evidence/PointShadows_v1/authoring.json) and [archive inventory and source-free Metal relocation](evidence/PointShadows_v1/relocation-offscreen.json)
- Release regressions for [GGX/IBL](evidence/PointShadows_v1/regression-ggx.json), [local lighting](evidence/PointShadows_v1/regression-local.json), [spotlights](evidence/PointShadows_v1/regression-spot.json) and [scoped CSM](evidence/PointShadows_v1/regression-csm.json)

## Checkpoint status

The native point-shadow checkpoint passed for owner visual inspection. The
changes were tested over `58fd90c3ef02177a7e16647531f597ac8fb54f38`, using
the source and artifact hashes recorded in the reports. Debug and Release
built with baseline warning classes. Scene serialization, legacy defaults,
copy/duplicate/prefab, exact inspector Undo/Redo, save/reopen and editor Play/Stop
passed on macOS. The Release GGX regression passed all 735 frozen components.
The spotlight regression used the existing local study edits, whose digest
is recorded. The CSM regression covered scoped analytic visibility, motion
and atlas lifetime. The previously documented strict CSM stress-parity
failure remains outside this checkpoint.

Both Release archives passed inventory, digest, and native signature checks.
The Metal probe ran from the relocated Nutella resources in a Unicode path
with checkout and toolchain access denied and all GLSL and MSL removed.
When the console is locked, `--offscreen-only` reproduces that check without
the packaged window runs.

Packaged window launches were **BLOCKED** by the locked macOS console. Native
2× display, Windows/Linux and Intel GPU execution were **NOT RUN**. The native
3200×1800 editor-target and presented 1× runs establish macOS coverage for the
workloads above. Broader production qualification is **INCOMPLETE**. The
authored point and spot study is ready for owner visual acceptance.
