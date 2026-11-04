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

`Core_v1.json` freezes named camera IDs/values, content hashes, idle clock/seed,
1280×720 logical and physical reference extents and output formats. Interactive
resizing is useful exploration; record the actual extents as an override when
capturing it. Future stages add named cases. Do not restage Core_v1 to conceal a
rendering discrepancy. A deliberate correctness revision needs numerical evidence
and a new case version.

The native maintainer recipe is `HazelProject create-rendering-reference FOLDER
TEMPLATES`, using a new destination and the matching `Hazel/Resources/Templates`.
It has no GPU, Python, importer or script compiler prerequisite. Checked-in bytes
and metadata remain authoritative, rather than runtime regeneration.

For a relocated package, build Release, compile the generated optional stub with
`script-build` (the current package auditor expects a module), then run:

```sh
python3 scripts/hazel.py script-build examples/RenderingReference/RenderingReference.hproj --config Release
python3 scripts/hazel.py package --app Nutella --project examples/RenderingReference/RenderingReference.hproj --name RenderingReference --output dist/reference
```

Keep the complete archive layout, resources and notices. The player needs no
source checkout or compiler SDK. Use private `HAZEL_DATA` and `HAZEL_SAVE_ROOT`
for experiments. See [runtime color](../../docs/color.md).
