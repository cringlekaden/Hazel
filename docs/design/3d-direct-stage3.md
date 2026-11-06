# Direct rendering — Stage 3

Work in progress on `feature/3d-foundations`. The canonical checkout and Xcode
workspace are `/Volumes/External/kadencringle/Projects/Hazel` and
`Hazel.xcworkspace`. Stage 4 environment preparation and IBL are excluded.

## Contracts

`SceneRenderer::Collect` produces common semantic camera, accepted mesh/material,
world transform, stable-slot, entity-picking and directional-light inputs. Both
Scene camera paths consume that submission. CPU documents and import code know
nothing about GL programs or texture units. `ProjectAssets` realizes accepted
immutable geometry lazily through typed indexed vertex/index resources (64-entry,
128 MiB GPU admission limit). Per-view HDR targets belong to the editor or
RuntimeSession; shared direct resources belong to Renderer. Runtime has no parser.

Direct_v1 uses single-scattering isotropic GGX, height-correlated Smith visibility,
Schlick Fresnel and Lambert diffuse, partitioned by `(1-metallic)*(1-F)`. F0 is
`mix(.04, baseColor, metallic)`. Perceptual roughness is clamped to .045 for
evaluation, then squared once to microfacet alpha. Incident RGB multiplies the
BRDF and NoL once. One enabled directional light travels along normalized world
local -Z; its color is linear Rec.709, intensity is renderer-relative [0,1000].
There is no ambient term, AO multiplier on direct light, IBL or shadow map.

Normals use inverse transpose; transformed tangents are orthogonalized, with
handedness multiplied by world determinant sign. Negative scale changes front
winding. Double-sided backs reverse the mapped normal. Normal maps are linear
UNORM, +Y; top-left UVs and cooked Mikk tangent signs are retained. Base/emissive
images decode sRGB; MR uses G roughness/B metallic. Opaque writes alpha one;
MASK discards before depth and integer picking. No alpha-blended mesh mode exists.

Display-referred scenes retain their lightweight RGBA16F linear world/UI path.
Lit surfaces require scene-referred presentation; unsupported combinations report
an editor/readiness diagnostic. Orthographic PBR and perspective unlit are valid.
Scene-referred views use a separate RGBA32F world color, integer ID and depth;
exposure multiplies all world radiance by `2^EV`, followed by fixed
[Khronos PBR Neutral](https://modelviewer.dev/examples/tone-mapping/) into the
linear display composite. FP32 preserves sharp conductor highlights beyond the
65504 half-float limit. Screen UI composes afterward, independent of exposure.
Depth and picking copy at equal pixel extent with nearest semantics. Existing
final sRGB encoding runs once; ImGui's own contract is unchanged.

Presentation mode/exposure/background and light values are authored and included
in strict codecs, exact comparisons, copy, prefab and document history. Editor
navigation/framing remains private workspace state. Project defaults only seed
new content. The prepared direct starter is copied by native creation without
running an importer, baker, GPU or script SDK.

## Authoring

New Project offers **2D** and **Direct PBR**. Project Settings / New scene defaults
exposes content, projection, presentation and navigation independently. Direct
content requires scene-referred presentation. Scene Settings applies presentation
and exposure through Undo/Redo; Save persists them. Add a Directional Light in
Properties and rotate its transform to aim the key. Existing native materials,
texture selectors and stable mesh slot overrides drive the viewport and Play.

Use Orbit (Alt-left orbit, Alt-middle pan, Alt-right/wheel dolly), or Planar for
orthographic editing. **F** frames selected transformed geometry bounds including
its subtree, with the current viewport aspect. It does not change authored cameras.

## Reference and evidence

`RenderingReference-Direct-PerspectivePBR.hproj`, `-OrthographicPBR.hproj` and
`-PerspectiveUnlit.hproj` select additive Direct_v1 saved cases. Camera, lighting,
output and scene hashes live in `Assets/Reference/Direct_v1/outputs.json`. Core_v1
and Import_v1 remain frozen. The original analytic sphere is cooked through engine
codecs, repeated by all grid instances; native cube diagnoses mirrored/nonuniform
scale. No expected lighting image substitutes for an unimplemented environment.

Qualification and completion status will be filled from actual results.
