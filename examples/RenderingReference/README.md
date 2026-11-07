# Rendering Reference — Core_v1

A maintained diagnostic project, independent of showcase art direction. It uses
native-authored scenes, the normal project asset service and the ordinary editor
and RuntimeSession paths. No scripts need to be compiled for viewing or Play.
Core_v1 demonstrates unlit 2D with orthographic and perspective cameras. Additive
Import_v1 and Direct_v1 cover native assets and direct PBR. Additive IBL_v1 cases cover prepared image-based lighting.

After building the selected configuration:

```sh
python3 scripts/hazel.py run Hazelnut --config Debug --project examples/RenderingReference/RenderingReference.hproj
python3 scripts/hazel.py run Nutella --config Debug --project examples/RenderingReference/RenderingReference.hproj
python3 scripts/hazel.py run Nutella --config Debug --project examples/RenderingReference/RenderingReference-Perspective.hproj
```

Open either scene in Hazelnut's Content Browser and Play to use its named authored
camera. The editor navigation view remains independent. Select the swatches,
texture samples, overlapping sprites, circle, text and UI control to inspect their
UUIDs/components; resize the viewport to inspect picking/layout at framebuffer
scale. Hover/focus/press the styled UI control to exercise its ordinary state
colors. Screen UI is independent of world-camera projection.

The four grayscale patches store linear RGB 0, .21404114, .5 and 1, corresponding
to sRGB 0, .5, approximately .735357 and 1. Both texture samples reference the same
8×8 image: Color decodes sRGB; Data keeps numerical channel values. Its asymmetric
quadrants are top-left red, top-right dark green, bottom-left blue and bottom-right
light magenta; one corner has alpha 128. The layered red/blue sprites have alpha .5
and a white backing. Correct linear blending deliberately differs from historical
encoded-space blending. World text, circle coverage and screen UI share the
canonical linear RGB/linear alpha contract.

`Assets/Reference/Core_v1.json` freezes named camera IDs/values, content hashes, idle clock/seed,
1280×720 logical and physical reference extents and output formats. Hash paths
are relative to the source project root. A package includes its selected project
descriptor and both scenes; the second source descriptor is not an installed
startup choice. Metadata and NOTICE travel through the ordinary package paths. Interactive
resizing is useful exploration; record the actual extents as an override when
capturing it. Future stages add named cases. Do not restage Core_v1 to conceal a
rendering discrepancy. A deliberate correctness revision needs numerical evidence
and a new case version.

The native maintainer recipe is `HazelProject create-rendering-reference FOLDER
TEMPLATES`, using a new destination and the matching `Hazel/Resources/Templates`.
It has no GPU, Python, importer or script compiler prerequisite. Checked-in bytes
and metadata remain authoritative, rather than runtime regeneration.

For a relocated content-only package, build Release, then run:

```sh
python3 scripts/hazel.py package --app Nutella --project examples/RenderingReference/RenderingReference.hproj --name RenderingReference --output dist/reference
```

Keep the complete archive layout, resources and notices. The player needs no
source checkout or compiler SDK. Use private `HAZEL_DATA` and `HAZEL_SAVE_ROOT`
for experiments. See [runtime color](../../docs/color.md).

## Import_v1: native geometry metadata

Core_v1 startup/cameras and source assets are unchanged. Additive
`Assets/Reference/Import_v1` cases qualify import/cooking/metadata, with no rendered
mesh/PBR acceptance or expected lighting images.

Open the usual project in Hazelnut. In Content Browser, open
`Reference/Import_v1/Native/SlotsAndInstances/Import.himport` for mesh/primitive/slot
and local-bounds metadata. **Open detached prefab** exposes the native nodes;
**Instantiate prefab** copies independent authored nodes into the scene. Mesh
instances now render with their authored materials. Lit materials need scene-referred
presentation; select a directional key for direct lighting. Keep the Core_v1 scene unsaved
when exploring; use a disposable project for edits.

- `SlotsAndInstances`: two independent material slots; one shared mesh in three
  instances; normalized UV/color, interleaved attributes and U8/U16 indices;
  mirrored tangent boundary requiring corner splits; signed nonuniform transforms;
  embedded asymmetric sRGB color and linear normal data; Mask and explicit Unlit.
- `ProvidedTangents`: provided FLOAT normal/tangent/UV/color, U32 indices, default
  glTF material and exact reflected affine matrix.
- `Sources`: named malformed/unsupported inputs, each with an expected diagnostic
  in `sources.json`. Import into a new disposable destination; rejection must leave
  no accepted output folder.

`outputs.json` freezes native file hashes/identities and completed geometry
metadata. `sources.json` records licensed source hashes and exact meanings. The
small standard-library generator recreates **source fixtures only**. Normal native
import owns validation/cooking/publication; replacing accepted reference artifacts
requires a deliberate case revision. See [native import](../../docs/design/3d-import-stage2.md).


## Direct_v1 — direct rendering

Open `RenderingReference-Direct-PerspectivePBR.hproj` in Hazelnut or select it with
Nutella's `--project`. `RenderingReference-Direct-OrthographicPBR.hproj` and
`RenderingReference-Direct-PerspectiveUnlit.hproj` exercise the independent camera
and shading choices. The same saved content feeds both hosts. Direct lighting has
no ambient/IBL; rough metals can remain dark under this single-scattering model.

The 18 repeated analytic spheres cover roughness 0/.1/.25/.5/.75/1 and metallic
0/.5/1; the mirrored nonuniform cube, world sprite/circle/text and screen UI share
the view. Scene Settings exposes world exposure; the footer stays independent.
Use Orbit and **F** on a selected instance. Material overrides use stable slots.
Camera/output/light conditions and scene hashes are frozen in
`Assets/Reference/Direct_v1/outputs.json`. Geometry/material provenance is in its
NOTICE and Source.generated. No external photographic or lighting assets are used.
Core_v1 and Import_v1 bytes/conditions remain unchanged.

From the canonical checkout:

```sh
python3 scripts/hazel.py run Hazelnut --config Release --project examples/RenderingReference/RenderingReference-Direct-PerspectivePBR.hproj
python3 scripts/hazel.py run Nutella --config Release --project examples/RenderingReference/RenderingReference-Direct-PerspectivePBR.hproj
```

The maintained renderer regression compares direct GPU radiance to a separate
FP64 BRDF oracle. Runtime regression checks saved Direct_v1 through editor/player
submission and 1x/2x picking. These are direct-only cases, not IBL acceptance.


`RenderingReference-Direct-SurfaceConventions.hproj` adds mapped +Y normals,
mirrored/nonuniform transforms, asymmetric masked/double-sided material, rear
geometry and the frozen Stage-2 supplied tangent/COLOR0 mesh. Its separate
`Surfaces/outputs.json` freezes conditions without changing the original grid.
Viewport inspection offers base color, world normal/tangent, metallic/roughness/AO
and SDR-clamped pre-exposure radiance. These private tools bypass world tone
mapping and never change the authored scene or player presentation.

`Reference/Direct_v1/conditions.json` additionally freezes the completed materials,
images, geometry, native documents and scenes by asset-relative SHA-256. The
maintained native reference check verifies this inventory before rendering. Add
new cases rather than changing these conditions to accommodate a discrepancy.


## IBL_v1 — prepared image lighting

From `/Volumes/External/kadencringle/Projects/Hazel`:

```sh
python3 scripts/hazel.py run Hazelnut --config Release --project examples/RenderingReference/RenderingReference-IBL-Combined.hproj
python3 scripts/hazel.py run Nutella --config Release --project examples/RenderingReference/RenderingReference-IBL-Photo.hproj
```

Adjacent `-IBL-DirectOnly`, `-IBL-IBLOnly` and `-IBL-AxesYaw90` descriptors choose
the other saved cases. DirectOnly disables both uses of its image environment;
IBLOnly disables the sun and uses constant RGB .25. Combined uses the asymmetric
axis environment and explicit directional key. Photo uses original Studio Small
09 by Sergej Majboroda / Poly Haven (CC0-1.0), with the sun disabled. Yaw90 rotates
the axis environment +90 degrees. Exposure is 0 and presentation is fixed Neutral.
No automatic exposure, extracted sun or ambient compensation is involved.

`Assets/Reference/IBL_v1/conditions.yaml` freezes saved scenes, cameras, source/
resource/recipe hashes, orientation, gain and output conditions. Source provenance
is in each `.henv` and NOTICE. Default cubes are 256/128/32 background/specular/
irradiance faces with explicit GGX mips and 1024 convolution samples; the shared
128-square A/B LUT uses 4096 samples. These are portable FP32 resources. Existing
Core_v1, Import_v1 and Direct_v1 bytes remain unchanged.

In a disposable copy, open a `.henv` in Content Browser for accepted versus stale
status, Save and native Prepare/Cancel. Scene Settings owns runtime yaw/gain and
background/lighting flags. Prepare is never required by native New Project or
player startup: the PBR starter bundles a small exact constant environment.
Runtime closure excludes the HDR source and baker. Stage 4's focused closure
checks do not replace Stage 5's broader mixed-scene/export qualification. See
[the Stage-4 record](../../docs/design/3d-environment-stage4.md) for actual evidence.
