# Authored local lights and Metal Forward+

**Native automated feature qualification: PASS. Owner visual approval: PENDING.**
Windows/Linux execution and their CI are NOT RUN at this macOS checkpoint.
Starting revision: `3a4647c8f0ad7f2b6cba9dd6534fb491d530796f`,
`feature/3d-foundations`. Qualification ran on the scoped working tree; exact
shader/BRDF hashes and gates are in [the qualification summary](local-lighting-qualification.json).
No shadow resources, depth passes or sampling filters were added.

## Source audit and ownership

- `SceneCollection.cpp` resolves transforms and collects backend-neutral
  `SubmittedLocalLight` values. Transform supplies position and normalized local
  -Z spot travel; authored range stays in world units, independent of scale.
  Submission sorts by entity UUID, with point before spot when both inhabit one entity.
- `Lighting.h` owns validated shared parameters and point/spot definitions.
  `Components.h::AllComponents`, `SceneSerializer.cpp`, `DocumentSchema.cpp` and
  `AuthoredState.cpp` cover persistence, copying, duplication and prefab/history state.
  `UIEditCommand` also registers both types for exact property preview/Undo/Redo,
  cancellation, component retirement and validation. Legacy directional ownership remains.
- `Renderer3D.cpp` packs the shared std140 block. `ForwardPBR.glsl::direct` remains
  the single accepted Multiscattering_v2 BRDF, byte-identical to starting HEAD.
  Local contributions share its shading-normal/geometric horizons, complementary
  diffuse and HDR accumulation. Direct light stays independent of material AO/XeGTAO.
  Emissive, IBL and TAA semantics remain intact.
- Metal records **one immutable CPU light snapshot per scene submission**, shared
  by its draws. Retaining the mutable `UniformBuffer` allocation was rejected by
  independent-view color tests. `MetalDeviceRenderer2D.cpp` prepares/encodes light
  work through existing frame packets, residency and three-slot buffer retirement.
  Editor, player and offscreen paths reset/reuse the counter pool; drain releases it.
  No second scene graph, alternate material model or general render graph exists.

## Mathematics and limits

RGB is linear Rec.709. Intensity is renderer-relative incident radiance at unit
distance in the inverse-square far field, before range/cone factors; **not candela
or lumens**. With `s=distance²`, `R=range`, attenuation is
`max(1-(s/R²)²,0)² * (s/(s+0.0001))² / (s+0.0001)`.
The smooth centimetre-scale source core tends continuously to zero at coincidence,
where direction is undefined. This regularized point approximation is not an area
emitter. Cone attenuation is cubic smoothstep between outer/inner cosines;
files store radians, UI half-angle controls display degrees. Zero cone influence
skips BRDF work without changing the lighting model.

Both backends admit **320 enabled local lights**, plus the existing single
directional key. The 15,376-byte block fits GL 4.1's 16 KiB guarantee. Excess
collection/submission fails explicitly; nothing is truncated. Disabled components
validate but consume no submitted slot. OpenGL uses the same bounded GLSL loop.

Metal uses sphere-versus-tile-frustum rejection, including near/far planes and an
outward FP32 margin. The spot sphere is conservative. Actual viewport/jittered VP
and physical extent drive culling. Avoiding depth reduction removes AO/TAA/prepass
dependencies and depth-discontinuity holes. One thread builds each ordered list:
17 uint4 texels/tile hold count and 64 indices; **count >64 evaluates every light**.
No atomics or allocation order can reorder a tile's list. Changing sizes retains
old in-flight packets; new lists use the new extent.

Measured 16/32-pixel tiles favor 16 for the common 32-light sparse case (1.660 vs
2.171 ms forward, 0.050 vs 0.036 ms cull at 1080p); 32 lowers culling cost for
large workloads but has coarser candidate coverage. Default stays 16. At eight
sparse lights the full loop measured 1.458 ms versus forced tiled 2.087 ms plus
0.042 ms culling, supporting the <=8 bypass. These are workload-specific choices.

Payload is 15.02 KiB per light block. At 1080p, 16-pixel lists require 2,219,520
bytes (2.12 MiB) per slot/view; three slots reuse storage. The existing allocator
may retain capacity up to four times a smaller request, so switching to 32-pixel
tiles need not immediately reduce retained bytes. Stats report retained buffer
capacity; whole-device accounting includes attachments and pending resources.

## Qualification evidence

| Gate | Result |
| --- | --- |
| Full MacOSQualification Debug/Release builds; direct-include ABI check | PASS; no new first-party warnings identified |
| Maintained native regressions | PASS 18/18 collectively Debug; 18/18 Release |
| Independent FP64 local accumulation, attenuation/cones, order | PASS both builds, native Metal/OpenGL; max sweep error ~1.1e-5 |
| Zero/one/few/320, single spot, near-source/HDR and range boundary | PASS; no invalid outputs |
| Tile edges, offscreen/near plane, projection/camera cuts, moving light/mesh, resize | PASS matched full-loop comparisons; 16/32 tiles |
| Dense overflow | PASS all 20 odd-viewport tiles overflow with 320 lights; FP64/full-loop contributions retained |
| Metal/OpenGL gallery | PASS exact IDs, max depth 1.79e-7, max RGB 9.08e-6 |
| Two independently lit TAA views | PASS 600 frames/build, real drawable colors, 320x180→640x360, stable warmed resources, positive presentations and retirement |
| Frozen GGX/XeGTAO/TAA | PASS Debug/Release, references unchanged |
| Hazelnut/Nutella gallery | PASS Debug/Release × Metal/OpenGL, managed light motion and teardown |
| Export/relocation | PASS signed inventory; source/SDK/MSL/compiler denied; isolated Metal/GL products; missing culling library rejects |
| Windows/Linux and owner visual approval | NOT RUN / PENDING respectively |

The original HEAD full-suite attempt hit the **pre-existing** frozen M0R3 material
hash mismatch documented in [TAA qualification](taa-qualification.md). Its bytes
match HEAD; they were not changed here. RuntimeSessionSmoke/EditorSmoke Debug and
full Release use the established immutable example revision
`86c581c13fd5c944c2a06d720314a753d7607d8f`. The failed HEAD log is retained locally.
No reference hashes/images were reset. Current-working-tree gallery outputs are
qualified separately. Existing deprecated-copy/unused/C++20-capture and Xcode
script-phase warnings remain; no broad suppressions were introduced.

[Numeric CSVs and versioned summaries](evidence/LocalLights_v1/) retain the measured
results. Full raw logs remain local in `build/qualification/local-lighting/` and
`/tmp/hazel-local-*.log`. Earlier fragment-stage timing and the initial mutable
snapshot experiment are superseded, not acceptance evidence.

## Performance

Apple M4 native Metal, AO/TAA off. Matched offscreen workloads alternate point/spot
lights, preserve one contributing central light in sparse cases, use five warmup
and twenty measured frames. GPU scopes bound culling and forward/world execution;
CPU light preparation overlaps recording and includes deferred native resources.
Scopes are not additive. Serialized frame timings also include completion/readback.
The separate native pacing study uses point lights, 120 positive presentations per
physical extent, thirty warmup frames, and excludes zero/missing presentation times.
VSync off is a request, not a compositor guarantee. Full median/p95 CPU, GPU and
memory data for both 720p/1080p are in the linked JSON/CSV records.

1080p medians in ms; presentation columns are median/p95. Tiled entries below nine
lights are forced diagnostic comparisons; production bypasses compute there.

| Lights | Sparse forward full / tiled16 | Cull16 | Dense tiled16 forward | Native sparse presentation | Native dense presentation |
| ---: | ---: | ---: | ---: | ---: | ---: |
| 0 | 0.802 / 0.915 | 0.036 | 0.842 | 2.299 / 5.360 | 2.314 / 6.871 |
| 1 | 1.854 / 1.615 | 0.040 | 1.606 | 2.420 / 9.412 | 3.146 / 8.081 |
| 8 | 1.458 / 2.087 | 0.042 | 3.762 | 3.161 / 8.807 | 4.190 / 5.899 |
| 32 | 1.885 / 1.660 | 0.050 | 13.749 | 3.147 / 9.170 | 11.626 / 20.585 |
| 128 | 4.987 / 3.879 | 0.176 | 53.321 | 4.602 / 11.135 | 41.899 / 55.918 |
| 256 | 8.609 / 6.274 | 0.342 | 109.155 | 6.418 / 12.200 | 83.560 / 94.702 |
| 320 | 10.373 / 7.457 | 0.425 | 136.090 | 7.599 / 16.501 | 105.843 / 122.767 |

Dense 320-light overlap is expensive: a correct unshadowed full contribution loop
cannot eliminate contributing lights. Overflow deliberately preserves quality at
higher cost. This is a bounded foundation, not a claim of arbitrary light counts
or 60 FPS under extreme overlap. A shader-only zero-light comparison against
starting HEAD measured 0.824 vs 0.813 ms at 1080p (0.332 vs 0.329 ms at 720p),
within measurement noise; it is not a whole-HEAD CPU comparison.

## Reproduction and next boundary

Build using the tracked Premake workflow; build scripts for the gallery's motion:

```sh
python3 scripts/hazel.py build --config Debug --tests
python3 scripts/hazel.py build --config Release --tests
python3 scripts/hazel.py script-build --config Debug examples/RenderingReference/RenderingReference-LocalLights.hproj
python3 scripts/macos/local_lighting.py --config Debug --output NEW_EVIDENCE_DIRECTORY
python3 scripts/macos/local_lighting.py --config Release --benchmark --backend metal --output NEW_PERFORMANCE_DIRECTORY
python3 scripts/macos/local_lighting_presented.py --output NEW_PRESENTATION_DIRECTORY
python3 scripts/hazel.py package --app all --project examples/RenderingReference/RenderingReference-LocalLights.hproj --output NEW_ARCHIVE_DIRECTORY
python3 scripts/macos/local_lighting_relocation.py --archives NEW_ARCHIVE_DIRECTORY --output NEW_RELOCATION_DIRECTORY
```

[Gallery authoring/launch instructions](../../examples/RenderingReference/README.md#locallights_v1-authored-point-and-spot-gallery)
cover creation, fine intensity/range drags, half angles, selectable influence
gizmos and transient radiance/occupancy/overflow inspection. Owner visual approval
remains separate from all automated results.

Future shadows should key resources by scene-instance identity + entity UUID +
light type; append the directional entity UUID to its existing optional record
when needed. Directional CSM, perspective spot maps and omnidirectional point
representations supply scalar visibility [0,1] multiplying that light's direct
incident RGB before the unchanged BRDF. PCF/PCSS share this visibility contract.
Explicit per-light bias, resolution, softness, quality and update policy belong
to the next milestone. No shadow allocation, depth rendering, filters or speculative
scheduler were implemented here.
