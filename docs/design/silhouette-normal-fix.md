# Geometric visibility and shading normals (2026-11-07)

The native Nutella readback confirms an implementation error in `d0fbac0`:
**the shading-normal cutoff blacked out covered, front-facing geometry**.
The earlier [IBL surface investigation](ibl-surface-investigation.md) incorrectly
treated zero radiance on those samples as an acceptable domain repair. Its
integral evaluated an invalid shading frame; that did not establish geometric
invisibility. This checkpoint repairs that handling, without removing the BRDF
guard, fading edges or changing HDR lighting.

## Native reproduction and pixel trace

Actual Apple M4 OpenGL 4.1 Metal–91.7, native Nutella framebuffer **1280×720**,
window **1280×720 points**, no offscreen scaling. Window samples=0; world HDR and
display framebuffer samples=1. AA remained off throughout the orbit and controls.
The existing SurfaceDetail view, Studio HDR, EV 0, yaw .349999994 and gain 1 were
unchanged. The original scene/materials, accepted lighting and frozen references
were not edited or rebaked.

Temporary local probes read the actual player world target before presentation,
the display target and the encoded **window back buffer before swap**. Exact
same-camera diagnostic submissions record geometric and raw/effective shading
N·V, direct/diffuse/specular terms, normals and positions. Inspection IDs/depth
are checked against the actual frame. The hooks were removed from product
sources afterward; there is no player diagnostic service or new backend API.

All coordinates below are native pixels with top-left origin. IDs are runtime
handles: **2 = copper UUID 114**, **0 = wood UUID 121**, **−1 = background**.
Depth is normalized D24, read as float, before and after presentation.

| Sample | ID / depth | Raw Ns·V / Ng·V | Linear HDR before → fixed | Window RGB8 before → fixed |
| --- | --- | --- | --- | --- |
| Copper (873,367) | 2 / .979987919 | −.001333641 / .050078701 | (0,0,0) → (.930073,.620147,.526332) | (0,0,0) → (237,196,181) |
| Wood (581,245) | 0 / .987223148 | −.024640318 / .002527305 | (0,0,0) → (.518927,.509344,.513717) | (0,0,0) → (184,182,183) |
| Adjacent white background (872,367) | −1 / 1 | no geometry | (1.023442,.999374,.990652), unchanged | (240,238,237), unchanged |
| Pale wood rim (575,325) | 0 / .987139344 | .065738350 / .113050185 | (.589688,.568392,.573744), unchanged | (196,192,193), unchanged |
| Bright wood reflection (810,261) | 0 / .985867739 | .509388626 / .481188208 | (1.168338,1.108666,1.075500), unchanged | (244,238,235), unchanged |

The corrected effective Ns·V is .025050053 on the copper sample and .001263670
on the wood sample. Copper radiance is entirely specular. The fixed wood sample
has diffuse (.000214137,.000045305,.000012777) plus specular
(.518712580,.509298682,.513703942); direct light is zero. At the pale rim,
diffuse is (.014872347,.003340653,.001114586), specular is
(.574815631,.565051436,.572629035), unchanged by the repair.

Each named sample and its eight neighbors retain the exact IDs, world/display
depths, HDR, display-linear/final color, normal dots and isolated lighting terms
in `pixel-report.json` and `pixel-neighborhoods.csv`. For example, the pixels
immediately left/below the black copper sample are white **background** at depth
1; the covered pixel to its right is copper (233,193,178). Their coverage is
correct with AA off. The center's black value first appears in the PBR lighting
terms, not during resolve or encoding.

## Coverage and presentation controls

The flat control uses the **actually overlapping** copper UUID 113 and wood
UUID 121, with their existing geometry/transforms and the same camera. They use
opaque unlit linear colors (.8,.025,.015) and (.015,.3,.8). The foreground/rear
interface is explicitly required by the regression, rather than inferred from
overlapping bounding boxes.

At interface pixel (603,337), foreground ID=1, depth=.982125223, HDR is
(.800000012,.0250000004,.0149999997), final RGB8=(229,28,6). Both meshes retain
their constant colors at covered silhouette samples. There are no holes or
color fringes. World color/depth/IDs and encoded window color are **byte-identical
before/after** the shading fix. Maximum presentation difference from independent
Neutral is .00009746; encoding difference is .00163705, within half an RGB8 step.

The lit dark-background control disables only environment **display** and uses a
dark flat surround; lighting resources, gain, yaw, exposure and materials stay
fixed. Covered HDR values differ by **exactly zero** from the normal-background
case, before and after repair. The normal and dark cases each reproduce 50 cutoff
holes before the fix and zero afterward. Depth and IDs remain exact. Maximum
lit presentation difference is .000488184 (FP16 storage); final encoding error
is .001960654 (RGB8 rounding). No bad pixel first appears after HDR.

## Normal policy and implementation

Geometric normals determine the reflective side of the surface; interpolated
or mapped normals define its shading frame. Confusing these can produce dark
spots on visible surfaces, as described in [PBRT's BSDF discussion](https://pbr-book.org/3ed-2018/Materials/BSDFs).

`ForwardPBR.glsl::resolveShadingNormal` keeps an already admissible shading
normal unchanged. Otherwise it rotates toward the geometric normal within the
original Ns/Ng plane, only as far as needed to keep the reflected view direction
on the geometric horizon. Let N(θ)=Ng cosθ+T sinθ, a=Ng·V>0, b=T·V. Then
R·Ng=a cos(2θ)+b sin(2θ); the first zero supplies the angular cap. Stable
half-angle expressions handle axis/grazing cases without an arbitrary angle,
energy floor or fade. The resulting normal drives both direct and indirect
BRDF evaluation and the existing LUT lookup. Geometrically back-facing views
still contribute no reflection; direct light behind the geometric side is
rejected. The BRDF's own N·V guard remains after normal resolution.

Geometric derivatives are evaluated before alpha-mask discard, so coverage
divergence does not invalidate the geometric frame. No vertex position, mesh
normal stream, material, alpha rule, ID, depth operation, tone mapping, encoding,
environment recipe or LUT data changes. NormalHorizon is a transient numeric
view (raw Ns·V, Ng·V, effective Ns·V), not an authored setting. The normal
inspection view shows the effective lighting normal.

## Regression and replay

`GeometricNormalChecks.h` renders the native-sized overlapping unlit pair through
HDR/presentation/encoding, then a 13-view lit orbit of the saved copper/wood and
occluder. It requires unchanged IDs/depth and nonzero radiance on visible samples
whose original shading normal faces away. It also compares the original authored
state. `GeometricNormalReference.h` independently solves the angular constraint
with FP64 bisection, rather than the shader's half-angle formula. Signed TBN
checks retain separate raw-normal assertions, then use the independently resolved
normal for BRDF comparison. Constant-furnace/grazing checks retain their .003
bound and verify that valid Fresnel is not replaced by an edge multiplier.

The normal public runner includes these checks:

```sh
python3 scripts/hazel.py test --config Release --profile native --only Renderer2DSmoke --only RuntimeSessionSmoke
```

The same renderer executable has `--geometric-edges` for a focused invocation
with the runner's private Showcase/resources/profile environment. It reports
other graphics coverage as not executed. A private resource copy containing the
baseline shader must fail the original visible-black-pixel assertion; current
source must pass. Do not replace a full qualification result with that focused mode.

From the canonical checkout, open the ordinary player view with:

```sh
python3 scripts/hazel.py run Nutella --config Release --project examples/RenderingShowcase/RenderingShowcase-SurfaceDetail.hproj --inspect-camera
```

Local native evidence is under ignored `build/silhouette-evidence`:
`before-studio`, `fixed-studio`, `before-dark`, `fixed-dark`,
`before-flat-overlap` and `fixed-flat`. Conditions include actual native sample
counts and shader hashes. Raw files are bottom-up RGBA32F, ID32, normalized
depth32 and final window RGBA8. The temporary native readback source/patch is
retained there for another exact player investigation; it is not shipped.
Earlier non-overlapping flat drafts are not acceptance evidence.

Across the saved slow orbit, the baseline has **724 black sample occurrences**
(45–69 per pose). Fixed output has **zero**, with minimum corrected Ns·V
.000515616. All thirteen complete native ID and depth buffers are byte-identical
to baseline. The sampled pale rim and bright wood reflection remain byte-identical.
AA-off silhouette stair steps and correct white background coverage remain;
neither is an HDR halo. This is actual native GPU/controller-sequence evidence,
not a claim of a new physical mouse acceptance test.

## Qualification

Apple M4 native **Debug and Release Renderer2DSmoke and RuntimeSessionSmoke
passed**. The renderer runs both default and reduced-slot/GLSL profiles; each
exercised the 724 repaired orbit samples. Existing direct, 2D/color/text, IBL
fractional LOD/energy, masks/picking, state restoration, history, fixed views,
inspection/reset and Play/Stop coverage remains included. This was a focused
two-executable qualification per configuration, not a repeated 18-test campaign.

The focused regression was also run with a private copy of the original shader:
it failed **"Visible geometric mesh sample went black at the shading-normal
cutoff"**. The fixed shader passed. No assertion or tolerance was relaxed.

Final, uninstrumented Release Hazelnut, fixed-view Nutella and inspection-enabled
Nutella rendered 60 frames and shut down normally. The relocated content-only
package passed source-checkout/SDK denial, native dependency closure and signature
checks with no source HDR, GLB or project assembly. Archive SHA-256:
`def2010514e53861e1982c9811b0fb52cddd9d85aee20e67e3304c19984a6237`.

All 352 inventoried reference/showcase/LastLightkeeper content files are unchanged.
Owner profiles/saves, vendor pins and recovery history remain intact; Finder
files remain uncommitted. No environment preparation, Stage 5, Metal work or CI
polling was performed. Linux/Windows and the separately documented historical
Windows gameplay timeout are not newly qualified here. The separate broad
rough-copper split-sum discrepancy remains documented; it is not the cause of
these now-repaired cutoff holes.
