# Rendering Showcase — Studio materials

A small surface gallery, separate from the immutable Rendering Reference cases.
It uses the original Studio Small 09 HDR, photographed wood and leather materials,
and a copper roughness sweep. All rendering uses Hazel's ordinary saved scene,
cooked mesh/material/environment assets and shared editor/player pipeline.

From `/Volumes/External/kadencringle/Projects/Hazel`, after matching builds:

```sh
python3 scripts/hazel.py run Hazelnut --config Release --project examples/RenderingShowcase/RenderingShowcase.hproj
python3 scripts/hazel.py run Nutella --config Release --project examples/RenderingShowcase/RenderingShowcase.hproj
```

In Hazelnut, **Play** uses the saved Gallery camera. Edit navigation remains your
private workspace pose. The adjacent `RenderingShowcase-RoughnessSweep.hproj` and
`RenderingShowcase-SurfaceDetail.hproj` select the other fixed authored views;
use either descriptor in the same commands. Camera/scene hashes, EV 0, yaw .35
radians and unit environment gain are recorded in `Assets/views.yaml`.

Look for:

- Copper progressing from sharp highlights through broad, stable rough reflections.
- Smooth dielectric illumination between the leather and wood's authored texture detail.
- Color-map sRGB interpretation versus linear ARM/normal data; fine +Y normal detail.
- UI text retaining its brightness independently of world presentation.

The scene uses prepared image lighting, with no hidden ambient multiplier,
automatically extracted sun, bloom, local reflection or shadow approximation.
Direct shadows, local reflections and antialiasing are separate future work.
Rough metals retain the documented single-scattering energy loss. This is a PBR
surface gallery, not a claim of a complete game renderer or GTA/Unreal parity.

The bundled **High** preparation uses 512 background/specular faces, 64 irradiance
faces, 16,384 base MIS samples per proposal and increasing per-level budgets up
to 65,536. Specular levels are explicit GGX convolutions with a 4×4 angular tail;
the shared 128² A/B LUT retains its BRDF recipe. No preparation is needed to launch.
Source HDR availability is required only for rebuilding. See the
[quality record](../../docs/design/ibl-quality-pass.md) for costs and actual evidence.

## Provenance

All downloaded pixels are original, unmodified 1k assets under Poly Haven's
[CC0 license](https://polyhaven.com/license):

- [Studio Small 09](https://polyhaven.com/a/studio_small_09), Sergej Majboroda.
- [Wood Table 001](https://polyhaven.com/a/wood_table_001), Dimitrios Savva
  (photography) and Rico Cilliers (processing).
- [Leather Red 02](https://polyhaven.com/a/leather_red_02), Rob Tuytel; Coll1 albedo.

Exact download URLs, sizes, upstream MD5 and locally verified SHA-256 are in
`Assets/Materials/PolyHaven-original-downloads.json`. Native file hashes and channel
semantics are in `Assets/NOTICE.yaml`. Base color is sRGB; ARM is linear
R=occlusion/G=roughness/B=metallic, and normals are linear +Y tangent data.
Copper factors and the native sphere/cube are original Hazel content under the
repository Apache-2.0 license. The showcase adds no importer or runtime source dependency.

## Inspect in motion

The fixed named views remain the default. In Hazelnut **Play**, opt in with the
viewport's **Inspect camera** checkbox; **Restore authored view** resets exactly.
Nutella uses the same transient view and reusable perspective movement policy:

```sh
python3 scripts/hazel.py run Nutella --config Release --project examples/RenderingShowcase/RenderingShowcase.hproj --inspect-camera
```

Start at Gallery, then use Option/Alt + left drag (or Space + left drag) to orbit
around the spheres. Shift + left drag or middle drag pans. Hold right mouse to
look and use WASD plus Q/E to move; release right mouse or press Escape to release
the pointer. Shift boosts speed; Ctrl gives slow close-up movement for the wood
and leather. Scroll dollies; Home restores the exact authored view. The on-screen
inspection hint appears only when inspection is enabled.

Inspection changes the submitted view, not scene entities, saved camera poses,
materials, exposure or prepared IBL. It needs no script module or SDK and remains
content-only. Omit `--inspect-camera` for fixed captures. In Hazelnut Edit, F frames
the selected mesh/subtree; private planar and orbit workspace poses remain separate.
