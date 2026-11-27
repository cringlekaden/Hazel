# Near-field sphere reflections in the Reflection Study

## Reproduction

The owner’s screenshot at `docs/images/SSR.png` shows a large orange lobe
below and to the right on the polished sphere, smaller reflected shapes
to the left, and a pale rim. The native capture uses the same saved project
and baked probes and recreates a close orbit in the style of the screenshot.
Within the capture set, camera, exposure, geometry and authored lights are
identical. AO and TAA are off for the isolation captures. The editor grid
is an overlay and is omitted. The scene's sky
background remains hidden and the capture adds no lighting.

| Reflection sources | Matched Metal capture |
| --- | --- |
| Authored global IBL | [Global IBL only](evidence/NearFieldProbeSpheres_v1/owner-global.png) |
| Box-projected local probes and residual global IBL | [Probe-only reproduction](evidence/NearFieldProbeSpheres_v1/owner-box-probe.png) |
| SSR with local probes disabled | [SSR without local probes](evidence/NearFieldProbeSpheres_v1/owner-ssr-without-probes.png) |
| SSR with local probes | [SSR with probe fallback](evidence/NearFieldProbeSpheres_v1/owner-ssr-with-probes.png) |
| Distant lookup with the same local bakes | [Distant probe control](evidence/NearFieldProbeSpheres_v1/owner-distant-probes.png) |

The distorted lobe appears with SSR disabled. In the sampled region of the
close probe-only and hybrid views, their p95 display RGB difference is zero.
The distant control changes the silhouettes using the same cubemap texels.
These controls isolate the primary effect to probe projection and captured
visibility.

## What the probe sees

The room box approximates rays that hit its planes. It supplies no distance
to a nearby sphere and cannot reconstruct visibility at a new origin. A
probe outside the polished receiver also sees that receiver during the
ordinary HDR bake. Sampling that baked image from the receiver can
therefore produce an apparent self-reflection even where a convex mirror's
outgoing ray physically hits the floor.

The saved study uses cubes and spheres with axis-aligned transforms. For each
guarded interior sample on the warm polished sphere, the diagnostic uses
the actual raster position and repaired shading normal. An independent
FP64 ray-box and ray-sphere test finds the first geometric hit of the
reflection ray. A second test intersects the room proxy, redirects the
lookup from the dominant probe's capture center, and determines the first
object along that direction. The geometry oracle includes the receiver when
classifying the captured direction. The real bake itself is unchanged.

Both native APIs produced the same classification. Of 11,927 stratified
samples with at least half-strength probe influence and shading
`N·V ≥ .2`, 2,827 selected a different object from the physical ray.
In 1,372 samples the probe direction selected the polished sphere itself.
The adjacent smooth dielectric sphere occupied 425 physical samples and
313 probe samples. The large background patch includes directions with
no geometry, which are expected in this scene.

The left panel classifies the physical ray. The right panel classifies the
projected probe lookup. Magenta identifies the receiver itself, blue the
floor, green and yellow two neighboring spheres, and near-black an
uncovered direction. The corresponding raw warm cube faces retain the
captured spheres across −Y and +Z.

[Source classification](evidence/NearFieldProbeSpheres_v1/owner-source-ids.png) ·
[Raw warm cube faces](evidence/NearFieldProbeSpheres_v1/owner-raw-minus-y-plus-z.png)

## Independent controlled sphere study

A chrome sphere of radius 1 at the origin reflects an orange sphere of
radius .65 at (−1.68, 1.56, 1.62). No environment supplies incident light in
the baseline. The orange sphere uses a constant emissive material so
silhouette classification does not depend on a light fit. The probe
captures are at (0, 0, 0) and (.35, −.22, .31), with the same world-space
proxy and with exposure and geometry fixed. The CPU mirror oracle uses
independent quadratic intersections. A second reference uses the
raster position and repaired normal to isolate the projected lookup from
geometric tessellation.

| Center pose comparison | p95 contour distance from ideal | p95 direction error at physical sphere hits |
| --- | ---: | ---: |
| Effectively unprojected probe | 5.83 px | 17.45° |
| Room-box projection, centered capture | 2.24 px | 8.65° |
| Same proxy, offset capture | 1.00 px | 3.04° |

Across nine camera poses, GPU probe contours agree with the corresponding
FP64 lookup model within 1 px at p95. The centered raw six-face bake had
zero incorrect classifications among 97,723 guarded directions per API.
It covered 300 edge samples and 12 corner samples inside the orange
silhouette. The offset capture passed 97,771 guarded directions. The
largest raw contour deviation across the two captures was .64°. The
production preparation retained the native capture bit for bit at
specular mip zero. The satin control broadens the reflected detail through
the existing prefilter. The probe-to-ideal direction error is much larger
than the .36° p95 difference between the raster and ideal mirror directions
at the center pose.

At the center pose the ideal full reflection covers 200 pixels. Only 91
have camera-visible physical hits and the SSR-only capture shows 90 above
the classification threshold. Across the sweep both APIs recovered all
266 guarded visible hits with zero guarded false hits. The worst per-pose
SSR hit-position p95 was 1.28 px. The largest change in centroid travel
between consecutive samples was .21 px for the centered boxed probe,
.80 px for SSR alone, and .31 px for SSR with the offset probe. These
measurements describe the sampled camera track.

The strip shows the ideal silhouette, centered boxed probe, SSR with
distant probe fallback, and SSR with offset probe fallback. The outline
identifies the dark receiver. The source placements change under the same
camera and exposure and show the effect of authoring a probe, independent
of shader changes.

[Polished roughness control](evidence/NearFieldProbeSpheres_v1/controlled-polished.png) ·
[Satin roughness control](evidence/NearFieldProbeSpheres_v1/controlled-satin.png)

[Matched placement strip](evidence/NearFieldProbeSpheres_v1/controlled-placement.png) ·
[SSR hit U/V/confidence channels](evidence/NearFieldProbeSpheres_v1/controlled-ssr-uv-confidence.png)

## Pale grazing rim

The authored Constant environment has Lighting enabled and Background
disabled. The neutral grazing rim remains with only this indirect
specular illumination. It disappears in the direct-only pass. In the
FP32 indirect-specular capture the average blue-to-red ratio rises from
.110 near face-on to .806 at grazing on the polished copper-colored sphere.
An independent evaluation of the existing split-sum and Kulla–Conty terms
matched the native result across 125,255 guarded samples, with maximum
linear RGB error of 4.6×10⁻⁶. The existing neutral incident radiance
and increasing neutral Fresnel response explain the pale rim.

[Direct lighting only](evidence/NearFieldProbeSpheres_v1/owner-direct-only.png) ·
[Authored uniform IBL only](evidence/NearFieldProbeSpheres_v1/owner-uniform-ibl.png)

## Qualification

The controlled and owner-scene studies passed in Debug and Release on Apple
M4 with native Metal 4 and OpenGL 4.1. The controlled AA-off parity maximum
was 5.97×10⁻⁸ at the same-surface RGB p99 with identical picking IDs.
Metal AO/TAA accumulated throughout the moving track. Its worst contour
p95 against the corresponding AA-off composite was 1.42 px.
The existing native baked-probe FP64 checks and SSR Gallery lifecycle,
resize, and fallback checks passed in both configurations. Builds emitted
the existing fixture warnings and no new warnings in the added code.

[Controlled Release](evidence/NearFieldProbeSpheres_v1/controlled-release.json) ·
[Controlled Debug](evidence/NearFieldProbeSpheres_v1/controlled-debug.json) ·
[Owner scene Release](evidence/NearFieldProbeSpheres_v1/owner-release.json) ·
[Owner scene Debug](evidence/NearFieldProbeSpheres_v1/owner-debug.json) ·
[Existing regression results](evidence/NearFieldProbeSpheres_v1/regressions.json)

## Disposition

The reproduced silhouette error is inherent to sampling scene radiance from
a single baked origin and reprojecting it on the room proxy. Probe placement
can improve a selected receiver and view range, as the controlled offset
shows. Geometry-aware reflections with per-ray scene intersections are
the appropriate technique when nearby curved-object silhouettes must remain
accurate over a wider view range.

The investigation adds a diagnostic fixture and evidence. Production shader
sources and packages were excluded from the changes, so the investigation
adds no rendering cost. Packaged shader and editor workflow requalification
were outside the affected surface.

## Reproduce the native checks

Use a generated SSR Gallery for the synthetic test and a saved Reflection
Study for the close orbit. The optional trace instrument uses a private
shader resource tree and records normalized hit coordinates and confidence.

```sh
python3 scripts/hazel.py build --config Release --tests \
  --target MetalRenderer2DQuadSmoke --target MigrationRenderer2DSmoke \
  --target HazelProject --target MetalShaderTools
python3 scripts/macos/reflection_spheres.py --trace \
  --gallery 'build/ssr-gallery-final/SSR Gallery/SSR Gallery.hproj' \
  --output build/sphere-contours
python3 scripts/macos/reflection_spheres.py \
  --gallery 'build/reflections-study/Study Project/Reflection Study.hproj' \
  --workspace docs/design/evidence/NearFieldProbeSpheres_v1/close-orbit.yaml \
  --output build/sphere-owner-controls
```
