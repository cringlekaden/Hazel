# Stage 1: 2D rendering foundations

Branch: `feature/3d-foundations`, based on master `a4c18a7`. Approved design:
[`3d-rendering-plan.md`](3d-rendering-plan.md), originally reviewed at `f21f708`.
This checkpoint implements unlit foundations. It adds no GLB import, meshes,
PBR, IBL, Metal or environment system.

## Implemented contracts

- Project `NewSceneDefaults` seeds newly created scenes and fresh editor
  workspaces. The shared native factory supplies an orthographic camera,
  one-meter sprite, world text and screen UI. New Scene can explicitly override
  projection with a working unlit perspective camera. Existing scenes/workspaces
  retain their values when project defaults change.
- Scene `Presentation` owns the opaque display-referred background. Unsupported
  scene-referred presentation is rejected. Projection, presentation and private
  Planar/Orbit navigation are independent.
- Renderer requests continue through `RendererPolicy`; requested/effective
  values, source and capability explanations are visible in diagnostics.
  Device identity, resource layout and capability resolution have one owner.
- SceneVersion 3, PrefabVersion 3 and UIStyle.Version 2 identify linear authored
  RGB. Color textures decode sRGB before filtering; data/alpha remain linear.
  Shared paths with different interpretations have different cache identities.
  Runtime world/text/UI composite in linear RGBA16F and encode once at output.
  ImGui's own rendering contract remains unchanged. See [color](../color.md).
- Presentation, transform, camera and circle edits use document history alongside
  existing sprite/text/UI authoring. Copy, exact authored comparison, detached
  prefabs, strict codecs and dependency inspection carry the authored contract.
  Workspace navigation and GPU/effective state stay outside scene history.
- Private workspaces keep independent planar and orbit poses, navigation, grid
  and tool selection. Planar uses XY pan/zoom and planar manipulation; orbit
  retains Alt orbit/pan/dolly. Navigation never edits the runtime camera.

Native creation is content-first: a name/location, suggested technical identifier
under Advanced, and one working 2D starter. It requires no Python, GPU, importer
or script SDK. Project Settings groups new-scene defaults separately from
renderer requests; Scene Settings applies authored presentation through history.

## Permanent reference

[`examples/RenderingReference`](../../examples/RenderingReference/README.md)
contains frozen Core_v1 orthographic/perspective cases. Normal native assets cover
known linear/sRGB swatches, color/data interpretations of one asymmetric image,
alpha overlaps, circle coverage, world text and styled screen UI. Camera/output
metadata, content hashes and asset provenance are checked in. Later rendering
stages add named cases; they must preserve completed reference conditions.

## Preservation and integration

Implementation uses a separate worktree. The original `feature/macos-support`
checkout, its two LastLightkeeper edits and both Finder files remain untouched;
their SHA-256 hashes were checked before and during qualification. Saves,
preferences, layouts, local Xcode state, dependency pins and vendor sources were
not edited. Tests use private profiles, saves and committed example snapshots.

All five maintained examples receive deliberate schema and RGB palette updates
from clean committed source. Alpha, state gain multipliers, scalar data, geometry
and gameplay are preserved. LastLightkeeper's MainMenu and project descriptor
also have unrelated owner edits in the original checkout. Applying that owner
patch to the updated clean files fails context checks in both files; integration
requires deliberate reconciliation, without replacing the owner's files.
Linear compositing changes translucent overlaps and edges. Owner visual review
of all five example palettes remains useful; no identical-appearance claim is made.

## Qualification

Apple M4, native OpenGL `4.1 Metal - 91.7`, 2560×1440 desktop. The reference's
numerical conditions remain 1280×720, with a separately identified 2× framebuffer
exercise at 2560×1440. No Retina/hardware performance claim follows from that
explicit offscreen scale exercise.

| Check | Evidence |
| --- | --- |
| Maintained Debug/Release native suites | Each passed all 17 sequential regressions, including the actual content-only player CLI cases |
| All five games, Debug/Release | Native rendering, gameplay, saves, transitions and UI campaigns passed |
| Actual editor authoring | Scene Settings Apply via ImGui, Undo/Redo, Save/reopen, repeated Play/Stop; native tutorial authoring; project defaults/new-scene isolation and private navigation checks passed |
| Core_v1 numeric cases | Normal saved project/RuntimeSession, both projections at 1×/2×, linear swatches, texture interpretation/orientation, alpha overlaps, final encoding and IDs passed |
| Relocated Reference archives | Both rebuilt products rendered 60 frames and closed normally with source/SDK denied; signatures and loaded dependency closure passed |
| Content audit | 59 maintained authored documents: targeted RGB conversion; alpha, data/state gains, transforms and gameplay unchanged apart from explicit schema/default/encoding/hierarchy additions |

Local logs live under `build/testing/logs`, `build/testing/games/{Debug,Release}-native`
and `build/testing/packages/macos`; private preservation/content-audit evidence
is under `build/stage1-evidence`. Reference archives are in `dist/reference`.
Representative frames from all five games were inspected against the qualified
committed baseline. Text/palettes remain readable; corrected translucent overlaps
differ. Additional review covered LastLightkeeper beam/tower gameplay and tutorial
text/composition. These are focused inspection captures, not frozen expected
lighting images or a new screenshot framework.

A standalone packaged-editor inspection showed Core_v1 using the ordinary
viewport. Automated editor tests supply the actual native authoring evidence;
the exploratory desktop inspection does not establish every physical input or
dialog interaction. Product normal-close evidence comes from the guarded 60-frame
runs. Public Developer ID signing/notarization is outside this checkpoint.

Initial local attempts failed ImGui's monitor-list assertion. A standalone probe
linked to the pinned GLFW observed zero monitors; after the display woke, it
observed the real VG272U V3 at 2560×1440/180 Hz. GLFW's unchanged Cocoa backend
excludes sleeping displays. Qualification used process-scoped `caffeinate -di`
assertions after verifying an awake display, without changing system preferences.
Five old single-entity example prefabs initially lacked explicit canonical root
metadata; the authored documents were corrected and the gameplay campaigns passed.

CI [41814121451](https://github.com/cringlekaden/Hazel/actions/runs/41814121451)
and diagnostic [41878623773](https://github.com/cringlekaden/Hazel/actions/runs/41878623773)
exposed a texture-oracle precision error. Mesa SRGB8 sampled byte 128 as .216553
in RGBA16F (M4 .215820; exact IEC .2158605), with unchanged alpha .501953. Mesa's
published polynomial accounts for the discrepancy. The initial 6e-4 bound covered
only half storage. The corrected, independent hardware-texture oracle requires
recovery within half an sRGB code value; authored linear/data/alpha/compositing
bounds remain 6e-4 and final output remains within one byte. No Core_v1 bytes,
cameras, output conditions or production color paths changed to mask this error.

[41945508193](https://github.com/cringlekaden/Hazel/actions/runs/41945508193)
passed Linux and hosted macOS. Windows passed color checks then exposed the new
test selector's narrow environment-path read; it now uses the existing native
Unicode environment helper. Actual content-only Nutella launches then exposed an
obsolete CLI assembly-file prerequisite, removed while retaining RuntimeSession's
assigned-class validation. The maintained suite now launches both Core_v1 camera
cases in the actual player, without a compiled module. Debug/Release actual
Hazelnut launches of both cases also completed 60 frames and teardown.

Packaging inspection also caught reference metadata/notices outside the ordinary
copy inventory. The unchanged Core_v1 metadata now lives at
`Assets/Reference/Core_v1.json`, and provenance uses the packager-supported
`NOTICE.txt` name. Final archives are checked for these files, their exact bytes
and source content hashes. No rendering condition changed. Final clean-source reference archives identify
`8340aa7` (documentation/metadata packaging after the same runnable code). Their
SHA-256 values are `d653a1121458b30c013ef01076927131c6ff1121c91537619f1a50fda11f999f`
(Hazelnut) and `8e8451fec3c2a977092265bf897e80d711b32b14bfc60429f64edee1a71ae6bd`
(Nutella). The closing qualification edit is documentation only.

Final canonical CI at runnable code `ccf65b9`:
[41950789249](https://github.com/cringlekaden/Hazel/actions/runs/41950789249).
Linux passed the full Debug/Release/GL4.1, product and game/archive campaigns.
Hosted macOS passed its build/CPU/service/managed boundary. Windows passed
Debug/Release native regressions, GL4.1, both content-only Reference player cases
and extracted-product checks, then timed out at 480 seconds in the Debug game
fixture. The overall run is **not green**. Its 53 captures match the earlier
failure's named set, with no `FAIL` assertion; its final ObservatoryDawn milestone
was at 475 seconds. Windows Release game execution and game archive acceptance
were not reached.

The earlier Unicode-fix run
[41950054892](https://github.com/cringlekaden/Hazel/actions/runs/41950054892) passed
full Linux and hosted macOS. Windows passed Debug/Release, forced GL4.1 and
extracted-product checks, then its Debug native game fixture hit the unchanged
480-second bound. Release game execution and subsequent Windows game archive
acceptance were not reached and are not passes.

Investigation compared the downloaded native log/captures with the pre-stage-1
[41720163554](https://github.com/cringlekaden/Hazel/actions/runs/41720163554) failure:
both produced the same 53 named captures, logged no `FAIL` assertion and continued
through ObservatoryDawn before termination. Relative to process startup, baseline
CliffCistern/Observatory milestones were 333/436 seconds; stage 1 reached them at
316/415 seconds and continued through ObservatoryDawn at 471 seconds. This
confirms the existing duration failure mode; it does not establish its cause or
prove performance equivalence. The existing qualified baseline had completed the
same fixture in 5m43s. No bound, assertion, retry or campaign condition was changed
to conceal the recurrence. Comparison evidence is retained under
`build/stage1-evidence/windows-timeout-comparison.json`.

## Remaining limitations

- Windows full gameplay/package acceptance remains limited by the recurring
  Debug fixture timeout; its cause is still unresolved. It is not a graphics
  correctness pass or a claim that the timeout was fixed.
- The content-only compiled-module limitation recorded at stage 1 is repaired
  by [stage 2](3d-import-stage2.md). Packaging now uses native assigned-script
  requirements; the reference needs no unused compiled stub.
- Reconcile the two owner-edited LastLightkeeper files deliberately, and review
  the five example palettes. Their original bytes remain preserved.

This record describes the stage-1 checkpoint. Subsequent format-independent GLB
import/cooking and native asset authoring are tracked in [stage 2](3d-import-stage2.md);
mesh drawing, PBR and IBL remain later stages.
