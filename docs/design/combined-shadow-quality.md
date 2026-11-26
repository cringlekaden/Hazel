# Combined shadow quality on Apple M4

This investigation started on `feature/3d-foundations` at
`075608ce4d448f5493fc31ea0abe75390f8f8a0f`. It tested the three shadow
types in one scene and screened deterministic wider PCF for the two local
light types. Production retains its existing PCF kernels and resolution
controls. A separate OpenGL atlas clear fix addresses stale point shadows
when a spotlight precedes the point pass.

## Audit

| Light | Filter in production | World-space footprint and bias |
| --- | --- | --- |
| Directional | Four stabilized cascades. Each sample averages a three-texel box into a shared 4×4 footprint. Cascade blending can sample a second map. | Each cascade has its own orthographic world units per texel. The authored normal offset and depth bias use this scale. Per-tap geometric plane correction bounds the grazing slope. |
| Spot | A three-texel box accumulated as 16 explicit comparisons. Each read stays inside the light's tile. | With axial distance $z$ and padded cone half-angle tangent $t$, one texel spans $2zt/N$ on a plane perpendicular to the light axis. The filter intersects each tap ray with the geometric receiver plane and bounds its axial correction near grazing. |
| Point | Bilinear comparison with four explicit taps per face. Normalized overlap weights combine up to three faces, for up to 12 comparisons. | Each face uses the same perspective mapping with a padded 90° field. Geometric receiver depth is affine in reciprocal axial depth and evaluated for each tap. The slope is limited at grazing. |

The local normal offset scales with 0.2 texel, face or cone cosine and
receiver slope. Both use a 0.05-texel base depth bias and fade near the
light's near plane. The receiver normal comes from position derivatives
before alpha discard. A fixed texel footprint gives a narrower world
transition as resolution increases and a wider transition as axial
distance increases. Cone angle and face direction change the world width
too. This explains why equal map sides give different visible softness
across the three light types. The spot and point controls serialize each
light's shadow enable and resolution. CSM settings serialize in scene
presentation, including map side, distance, split, bias and blend.
There is no softness or physical emitter-size setting.

Scene collection sorts local lights by entity ID. SceneRenderer admits up
to four emitting shadowed spots and the first emitting shadowed point.
The scene also supports one enabled directional light.
The view owns three D32F atlases. The shared caster list includes opaque
and MASK static meshes before camera culling. Each map bounds-tests its
casters and uses the same depth material and winding rules. The maximum
active atlas allocation is 86 MiB per view (64 directional, 16 spot, 6
point), plus outstanding generations and driver overhead. A horizontal
spot atlas can waste rows when tile heights differ. Metal makes producer
attachments available to the consuming forward pass and retains their
generations until GPU completion. The 2 GiB owner and in-flight budget
covers more than the current atlas payload.

Unshadowed local lights skip all shadow texel reads. After the normal,
range and cone tests, the shader still checks the point-light index for
each surviving light and searches up to four spot indices for a surviving
spot when any spot maps exist. SceneRenderer scans the local list once
per shadow type, and fits and submits the 12 atlas tiles for this study
each frame. The measured work is concentrated in forward shading. A
separate unshadowed ablation would be needed to attribute cost to the
index checks or compiler register pressure. An earlier matched point
study reported a small disabled-path difference between shader revisions.
See the [point shadow record](point-shadows.md). Shadow visibility
multiplies incident direct lighting and the IBL path is independent.

## Filtering decision

The [study shader patch](evidence/CombinedShadows_v1/widen-pcf.patch) widens
the spot kernel to a separable `[1, 2, 3, 2, 1]` convolution with
subtexel interpolation (36 comparisons), and the point kernel to the
three-texel box (16 comparisons per face, up to 48 at a corner). The
experiment retains geometric per-tap receiver bias, has no rotated noise
or temporal input, and uses the existing atlases. The candidate remains a
research patch applied only to copied shader resources.

In the existing point overlap band an adjacent axis is at least
$1-4/N$ times the dominant axis. With the face projection scale
$1-8/N$, bounding the normal displacement by 0.2 face texel
keeps the biased sample center at least 1.61 texels from the physical
tile edge on each axis at 256 and 512. This conservative bound permits
the candidate's complete 4×4 footprint, including when all three face
weights are active. Both kernels blend the same normalized face weights.
The six-wide spot footprint can reach the tile clamp near the cone edge
where incident light falls off. A larger spot guard would change sampling
density and needs its own screen.

Hardware bilinear compare would reduce lookup instructions for a grouped
kernel, but uses one reference depth for its four comparisons. It cannot
evaluate four different points on Hazel's receiver plane. Replacing the
current comparisons requires a separate bias and contact validation
across both depth-sampler backends. A fixed disk pattern can provide
isotropic coverage, but sparse wider samples can separate at thin
features. Screen-space rotation introduces noise without temporal
accumulation. Both alternatives remain design options pending measurements. See the
[optimized PCF derivation](https://www.ludicon.com/castano/blog/articles/shadow-mapping-summary-part-1/).

Native Release (321×239) measured the following edge and feature changes.
Visibility ratios use an otherwise equivalent unshadowed direct-light
capture. Widths are linearly interpolated 10–90% crossings. The point
motion probe shifts a blocker along a stationary receiver across nine
poses spaced 4 mm apart with TAA off. Its oracle projects the right edge
from the light-facing side of the blocker onto the receiver in double
precision.

| Measure | Production | Wider PCF |
| --- | ---: | ---: |
| Spot median edge width | 1.67 px | 2.15 px |
| Point median edge width | 1.66 px | 4.29 px |
| Point p95 edge movement error vs analytic displacement | 0.730 px | 0.728 px |
| Spot MASK samples fully revealed / retained | 166 / 1,879 | 109 / 1,772 |
| Point MASK samples fully revealed / retained | 278 / 7,318 | 77 / 6,494 |
| Spot contact samples | 48 | 48 |
| Point contact samples | 63 | 63 |

Both kernels passed the existing spot FP64, contact and MASK gates, and
the point gates across all 26 signed face, edge and corner orientations.
The 1,064,518 point ray-box classification samples per backend include
8,322 stable lit and 4,128 stable blocked samples in the seam bands.
The wider point filter expands the transition and preserves the measured
motion error. It reduces fully revealed cutout samples by 72% and the
spot improvement is under half a pixel on this fixture. Combined forward
shading adds roughly 1 ms at 720p and 2.5 ms at 1080p. The measured
tradeoff favors the production filters and existing resolution controls.

- Combined capture [production](evidence/CombinedShadows_v1/combined-production.png)
  and [wider PCF](evidence/CombinedShadows_v1/combined-wider.png).
- Isolated spot edge [production](evidence/CombinedShadows_v1/spot-production.png)
  and [wider PCF](evidence/CombinedShadows_v1/spot-wider.png).
- Isolated point edge [production](evidence/CombinedShadows_v1/point-production.png)
  and [wider PCF](evidence/CombinedShadows_v1/point-wider.png).
- Point three-face corner [production](evidence/CombinedShadows_v1/corner-production.png)
  and [wider PCF](evidence/CombinedShadows_v1/corner-wider.png).
- Point MASK [production](evidence/CombinedShadows_v1/mask-production.png)
  and [wider PCF](evidence/CombinedShadows_v1/mask-wider.png).
- Profile evidence [production](evidence/CombinedShadows_v1/probes-production.json)
  and [wider PCF](evidence/CombinedShadows_v1/probes-wider.json).

## Combined workload and timing

The authored project contains a sun, two shadowed spots, one shadowed
point and ten unshadowed points. Casters include a hard contact block, a
smooth sphere, a mirrored block and a thin patterned MASK caster. The
native harness moves the camera, the point light, a spot and the contact
block. It checks the same lights with all maps disabled, compares
Forward+ with the full loop, and checks Metal AO and TAA during motion.

The four study configurations set CSM sides to 256/512/1024/2048. Each spot
uses the same side up to 1024, and the point face side caps at 512. These
are benchmark configurations using existing settings. The benchmark
uses fixed camera and geometry, AO/TAA off, no validation, and native
Metal phase timestamps from serialized completed offscreen frames.
Alternating enabled/disabled order retains separate view resources.
Each row has five warmup pairs and fifteen measured pairs. Timing of
generation is the sum of three per-frame atlas intervals. The paired
forward delta includes all shadow sampling and its control flow.

| CSM / spot / point | Atlas MiB | Atlas ms at 720p / 1080p | Forward ms at 720p / 1080p | Shadow forward delta at 720p / 1080p | Wider forward increase at 720p / 1080p |
| --- | ---: | ---: | ---: | ---: | ---: |
| Low 256 / 256 / 256 | 3 | 0.044 / 0.044 | 5.55 / 12.08 | 1.85 / 4.16 | 1.13 / 2.52 |
| Medium 512 / 512 / 512 | 12 | 0.036 / 0.036 | 5.57 / 12.06 | 1.88 / 4.14 | 0.97 / 2.54 |
| High 1024 / 1024 / 512 | 30 | 0.065 / 0.037 | 5.49 / 12.08 | 1.80 / 4.16 | 1.25 / 2.55 |
| Ultra 2048 / 1024 / 512 | 78 | 0.208 / 0.218 | 5.55 / 12.09 | 1.91 / 4.16 | 1.19 / 2.53 |

Values are medians in milliseconds, with MiB showing the active atlas
payload. Matched unshadowed forward medians were about 3.7 ms at 720p
and 7.9 ms at 1080p. Wider filtering allocates the same maps. The
phase benchmark peaked at 345,106,288 tracked bytes (233,660,672
attachment bytes), and retired to zero. The High 720p map interval
varied between runs even though the same depth shaders generated both
atlases. The raw samples retain those excursions. See the
[production CSV](evidence/CombinedShadows_v1/timing-production.csv),
[wider CSV](evidence/CombinedShadows_v1/timing-wider.csv), and
[timing summaries](evidence/CombinedShadows_v1/timing.json).

Presented Metal medians use completed frames with positive presentation
timestamps after discarding 30 per physical extent. These use the authored
High configuration at 1× display backing. The parenthetical values are
whole-frame GPU intervals. The compositor and display contribute to the
presented interval.

| High | 720p presented (GPU) ms | 1080p presented (GPU) ms |
| --- | ---: | ---: |
| Production, AO/TAA off | 6.30 (6.34) | 13.31 (13.46) |
| Wider, AO/TAA off | 7.51 (7.52) | 15.94 (16.00) |
| Production, High AO and TAA | 12.05 (12.33) | 27.64 (27.05) |

The High AO/TAA presented test reached 557,931,328 tracked frame bytes
and ended with no live attachment generations or pending bytes. Native
2× presentation was unavailable on the attached display. The separate
3200×1800 offscreen target exercised Ultra CSM with AO/TAA under Metal
validation. It peaked at 1,061,569,864 tracked bytes, including
789,340,416 attachment bytes, and retired to zero. See [presentation evidence](evidence/CombinedShadows_v1/presented.json).

## OpenGL atlas reuse and parity

The spotlight pass sets a scissor per tile. On OpenGL, the last tile's
scissor was still enabled when the point atlas was cleared, and
`glClear` respects that scissor. With two 1024 spot tiles and a
1536×1024 point atlas this left much of the point atlas uncleared between
frames. Older depths could survive a moving light or caster and cast
ghost shadows. Metal uses an attachment clear.

SceneRenderer now clears each OpenGL shadow atlas with scissoring
disabled and depth writes enabled. Local tile draws still use the
existing scissor. The surrounding render state scope restores caller
state. A regression check compares color, ID and depth exactly within
each backend for the same moving scene state using reused and fresh
atlases. It passed in Debug and Release on both backends.

| Moving frame | RGB deltas above 0.003 before | After | After p99 RGB delta |
| --- | ---: | ---: | ---: |
| 1 | 4,033 | 18 | 5.89e-6 |
| 2 | 6,231 | 11 | 6.09e-6 |
| 3 | 8,220 | 5 | 6.11e-6 |

All three moving frames retained zero ID mismatches. At the initial
static pose, the one masked silhouette ID mismatch also occurs with
all shadow maps disabled. Directional-only and map-disabled controls
at the final pose have zero pixels above 0.003. The local-only
control has five. Remaining raster differences are retained in the
evidence. The documented strict CSM stress parity failure and
historical silhouette differences remain separate from this repair.
See the [CSM record](directional-shadows.md#csm-cohesion-investigation-and-ultra).

- OpenGL at the final motion pose [before](evidence/CombinedShadows_v1/opengl-motion-before.png)
  and [after](evidence/CombinedShadows_v1/opengl-motion-after.png).
- [Metal with AO/TAA under motion](evidence/CombinedShadows_v1/metal-motion-ao-taa.png).
- [Native Debug and Release capture reports](evidence/CombinedShadows_v1/captures.json).

## Contact hardening

For an emitter with physical radius $R$, similar triangles give a
receiver-plane penumbra radius near $R(z_r-z_b)/z_b$, using linear
light-space receiver and blocker distances along the relevant ray.
Read back perspective depth $d$ as
$z=n/[1-d(f-n)/f]$ before accumulating blockers. Point samples on
different faces must be converted into a common distance convention
before averaging. The fixed-radius PCF experiment does not change the
physical source size.

For a 2.5 cm source and a receiver at 3 m, blockers at 2.9 m, 1.5 m and
0.5 m yield about 0.86 mm, 25 mm and 125 mm receiver-plane penumbra
radii. On the 512 point face near its center these correspond to about
0.07, 2.1 and 10.5 face texels. The first two fit within a small bounded filter. The third exceeds the existing seam guard. A
shorter range bounds the receiver interval, but small blocker depths
still produce large radius ratios. A hard kernel cap changes the result
for those closer blockers. A sparse blocker search also risks missing
thin MASK occluders and shifting its estimate as lights or casters move.

The NVIDIA perspective PCSS reference decodes depth, searches for
blockers and adjusts the PCF radius. Its smallest published Poisson
preset uses 25 search and 25 filter samples. Current Filament code
includes a perspective search from a mip of exponential variance
moments with a world-space bulb radius and capped search range.
Filament's moment lookup and filtering require a moment pyramid and
different resource admission from Hazel's D32F depth atlases. A raw
depth sample gives the closest surface and cannot supply the same
filtered blocker estimate. The [NVIDIA implementation](https://developer.download.nvidia.com/SDK/10.5/direct3d/Source/PercentageCloserSoftShadows/doc/PercentageCloserSoftShadows.pdf)
and [Filament shader source](https://raw.githubusercontent.com/google/filament/main/shaders/src/surface_shadowing.fs)
document these approaches.

The prior [directional PCSS study](directional-pcss.md) missed its
cost target. Here the regular wider filter already performs poorly
against the combined forward budget. Contact hardening remains a feasibility study. Independent motion,
thin-caster, near-blocker, point overlap and workload measurements would
be needed to qualify a blocker search.

## Reproduction

Run on Apple Silicon with native Metal 4 and OpenGL. The capture mode
runs both backends with Metal validation on. Timing and presentation
run Metal only. Each invocation writes to a fresh output directory.

```sh
python3 scripts/hazel.py build --config Debug --tests \
  --target MetalRenderer2DQuadSmoke --target MigrationRenderer2DSmoke
python3 scripts/macos/combined_shadows.py --config Debug --output build/combined-debug
python3 scripts/hazel.py build --config Release --tests \
  --target MetalShaderTools --target MetalRenderer2DQuadSmoke \
  --target MigrationRenderer2DSmoke --target MetalM1Smoke \
  --target Hazelnut --target Nutella
python3 scripts/macos/combined_shadows.py --config Release --output build/combined-release
python3 scripts/macos/combined_shadows.py --config Release --mode probes --output build/combined-probes
python3 scripts/macos/combined_shadows.py --config Release --mode timing --output build/combined-timing
python3 scripts/macos/combined_shadows.py --config Release --mode presented --output build/combined-presented
python3 scripts/macos/combined_shadows.py --config Release --mode presented --effects --output build/combined-effects
```

To reproduce the wider filter, copy Resources, apply the research patch
inside that copy and build its offline Metal artifacts. Pass
`--resources build/combined-wider-resources` to the same harness.

```sh
cp -R Hazel/Resources build/combined-wider-resources
patch -d build/combined-wider-resources -p1 < docs/design/evidence/CombinedShadows_v1/widen-pcf.patch
PYTHONPATH=scripts python3 - <<'PY'
from pathlib import Path
from macos.metal_shaders import produce
shaders = Path("build/combined-wider-resources/shaders").resolve()
produce("Release", shaders / "metal", shaders)
PY
```

The source-free check uses a signed, relocated Release archive and
loads the authored project under the same native isolation harness as
the [spotlight and point tests](point-shadows.md).

```sh
python3 scripts/hazel.py package --app all \
  --project examples/RenderingReference/RenderingReference-CombinedShadows.hproj \
  --output build/combined-archives
python3 scripts/macos/combined_shadows.py --config Release --mode relocation \
  --archives build/combined-archives --output build/combined-relocation
```

Use `--offscreen-only` for archive inventories and the source-free Metal
probe when the native desktop is locked.

## Qualification status

| Gate | Result |
| --- | --- |
| Combined exact fresh/reused equality, geometry coverage and Forward+ equivalence | PASS in native Debug and Release |
| Point and spot FP64 references, MASK, contact, clipping and queued Metal lifetimes | PASS in native Debug and Release |
| Point and spot schema, legacy defaults, round trip and editor Undo/Redo | PASS in Debug and Release |
| Focused CSM, local lighting, GGX, AO and TAA regressions | PASS in native Release |
| Strict CSM stress overlap parity | FAIL, reproduced with the existing gate and thresholds |
| Signed Release archives and relocated source-free Metal | PASS |
| Packaged GUI relaunch | BLOCKED by locked desktop |
| Native 2× presentation, Windows/Linux and Intel | NOT RUN |
| Integrated owner visual acceptance | Pending |

The OpenGL fix qualifies within this scope. Overall shadow qualification
remains incomplete. The [qualification record](evidence/CombinedShadows_v1/qualification.json)
preserves the strict stress results and scoped regression status. Debug
and Release introduced zero first-party compiler warnings. Existing test
and build warnings were retained. Raw captures, trace logs and package
outputs are under `build/integrated-shadow-quality/`.
