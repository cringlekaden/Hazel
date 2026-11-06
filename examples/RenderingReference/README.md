# Rendering Reference — Core_v1

A maintained diagnostic project, independent of showcase art direction. It uses
native-authored scenes, the normal project asset service and the ordinary editor
and RuntimeSession paths. No scripts need to be compiled for viewing or Play.
This checkpoint demonstrates unlit 2D with orthographic and perspective cameras;
it does not demonstrate mesh import, PBR or environment lighting.

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
components explicitly prevent Play until stage 3. Keep the Core_v1 scene unsaved
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
