# Direct rendering — Stage 3

Implemented and locally qualified on `feature/3d-foundations`. The canonical checkout and Xcode
workspace are `/Volumes/External/kadencringle/Projects/Hazel` and
`Hazel.xcworkspace`. Stage 4 environment preparation and IBL are excluded.

## Contracts

`SceneRenderer::Collect` produces common semantic camera, accepted mesh/material,
world/normal transforms and handedness, stable-slot, entity-picking and directional-light inputs. Both
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
Depth copies at equal pixel extent with nearest semantics; integer picking uses
exact texel fetches in the presentation pass. M4 rejected the matching R32I blit,
so no filtered/converted color path is used for IDs. Existing
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


Viewport base/normal/tangent/material-channel inspection and SDR-clamped raw
radiance are private workspace tools. Inspection bypasses tone mapping/exposure;
normal player presentation remains unchanged. The additive SurfaceConventions
case includes mapped +Y normals, mirrored/nonuniform instances, asymmetric
mask/double-sided material, rear geometry and a frozen Stage-2 COLOR0/tangent mesh.
Its conditions are in a separate Surfaces/outputs.json.

## Checkout consolidation and Stage 2 closure

The sole worktree is the original Hazel directory on feature/3d-foundations.
Temporary and nested retired worktrees were inspected and removed normally.
Useful prior logs/archives are retained under ignored build/historical-evidence;
clean retired submodule Git repositories remain under .git for recovery. No build
cache was copied into the canonical checkout. Pins/vendors remain unchanged.

The owner's exact LastLightkeeper drafts are recoverable at local commit bfbce81
on preserve/lastlightkeeper-owner-20261106. Commit 310a4d3 reconciles the scene's
307 explicit default fields with Stage-1 schema/linear RGB. The project draft had
no semantic value delta; its current Stage-1 defaults remain. Existing converted
colors were retained byte-for-byte. Native project/scene validation passed. Finder
files remain untracked; personal saves/preferences/layouts and Xcode schemes were
retained. Tests use disposable profiles/projects.

Completed CI 42158669043 at 623b90b passed Linux and hosted macOS checks. Windows
built both configurations and passed both extracted-archive profiles, including
the path/UTF-8 repair, native import/reimport and content-only package contracts.
Its later frozen-artifact CRLF failure is repaired by 6d7c478 byte-preservation
attributes. The historical Windows Debug gameplay timeout was not reached there;
it is separate unresolved debt. No timeout or retry policy was changed.

## Launch from the canonical checkout

Open `Hazel.xcworkspace` in the original Hazel directory. The native launch
commands are:

```sh
cd /Volumes/External/kadencringle/Projects/Hazel
python3 scripts/hazel.py open
python3 scripts/hazel.py run Hazelnut --config Release --project examples/RenderingReference/RenderingReference-Direct-PerspectivePBR.hproj
python3 scripts/hazel.py run Nutella --config Release --project examples/RenderingReference/RenderingReference-Direct-PerspectivePBR.hproj
```

The editor navigation camera remains private. Play uses the frozen authored
camera, as does Nutella. The orthographic, perspective-unlit and surface-convention
selectors are adjacent project descriptors. `conditions.json` freezes the completed
Direct_v1 materials, images, cooked geometry and scene bytes; future cases are
additive. Captures are inspection evidence, not expected images for absent IBL.


## Checkpoint qualification

At code checkpoint `acad603`, native Apple M4 OpenGL 4.1 passed the maintained
Debug and Release suites: **18 sequential regression executables each**, with
actual graphics checks. The final build uses the owner's selected Xcode 27.3 beta;
older Xcode 27.2 evidence remains historical. No graphics skip is counted as a pass.

The direct checks compare GPU radiance against an independent FP64 evaluator
(2e-3 ordinary-point bound), including the roughness/metallic grid and grazing
inputs. Separate hemisphere quadrature checks convergence, energy and analytic
`1-ln(2)` rough-conductor single-scattering loss. Readbacks cover FP32 highlight
range, nonuniform/signed tilted normals and tangents, linear COLOR0, separate
indexed primitive/material ranges, color/data sampling and UV orientation,
mask/depth/ID transfer, exposure/Neutral/UI independence, surface inspection and
producer-failure/retry state restoration. Camera-free HDR output and CPU-only
physics/script updates have separate positive cases.

Native ImGui/production-command checks passed import/material/texture/slot
operations, light and scene history, save/reopen, Play/Stop, defaults isolation,
bounds framing and direct creation without Python/SDK readiness. Original-path
Core_v1/editor and all four direct player selectors completed 60 real frames and
normal teardown. The same saved direct content passed editor/runtime comparisons
and 1x/2x picking. All five examples passed their maintained native gameplay,
render/UI/save/transition checks; representative frames were inspected for palette,
readability and clipping. This does not assert historical pixel identity.

Both direct Reference bundles were exported without **any GLB or project assembly**,
then relocated and run with source/SDK access denied. Native dependency closure,
signatures, actual direct readbacks and normal shutdown passed. Missing/invalid
required module/class and managed closure checks still reject. Corrupt geometry
rejects before publication and preserves accepted archives/checksums. Runtime
binaries contain no TinyGLTF/Mikk producer symbols. Evidence and archives are under
ignored `build/stage3-evidence`; no test output is committed.

Canonical [run 42231113856](https://github.com/cringlekaden/Hazel/actions/runs/42231113856)
passed Linux full and hosted macOS CPU/service/managed checks. Its Windows build
hit MSVC C1128 in the expanded inspector; `8355605` adds file-scoped `/bigobj`.
The subsequent run built Windows Debug/Release and passed both extracted-archive
profiles, including the GL4.1 path. Its separate CPU no-camera regression was
reproduced locally and repaired in `acad603`, with focused and full local passes.
[Run 42273886851](https://github.com/cringlekaden/Hazel/actions/runs/42273886851)
qualifies the final code; results pending at this documentation checkpoint.
Windows is not claimed fully qualified. The historical Windows Debug gameplay
timeout remains separate unresolved debt; no timeout or retry was increased.

Physical OS dialogs/window/input interaction remains separate acceptance from
native ImGui authoring and automated product frames. This checkpoint supports
static opaque/masked meshes and one direct key; mesh blending, shadows,
environment preparation and IBL are absent. Stage 4 has not begun.

Stage-4 entry diagnosis of that completed run: Linux and hosted macOS passed.
Windows built both configurations and passed extracted archives and packaged
GL4.1, then first failed `RuntimeSessionSmoke` at `Frozen Direct_v1
geometry/material/image/reference condition changed`; gameplay was not reached.
Authenticated job and uploaded executable logs agree. Reproducing `git archive
HEAD:examples/RenderingReference` with `core.autocrlf=true` changes the first
inventoried file, `Native/Direct_v1/Import.himport`, from SHA-256 `481ddad7…` to
`c930d069…`. Subtree archives omit the root `.gitattributes`. Fixture snapshots now
archive the full-tree path and strip its prefix after validation; all 61 frozen
Direct files retain their exact hashes under autocrlf. The native assertion also
reports the affected path and both digests. No frozen bytes, tolerances, driver
settings or timeouts changed. Windows rerun remains required; this was a fixture
extraction defect, separate from the historical gameplay timeout.
