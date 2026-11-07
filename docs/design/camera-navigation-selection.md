# Camera, viewport navigation and selection checkpoint

Canonical checkout: `/Volumes/External/kadencringle/Projects/Hazel`, branch
`feature/3d-foundations`, baseline `81c929f`. Stage 5 is not part of this work.
No reference asset or prepared lighting is rebuilt.

## Reproduction and ownership

The native M4 EditorSmoke harness reproduces a scene-image press acquiring
ImGui's viewport `MoveId`. The old editor's frame-update gate then rejects
`IsAnyItemActive()`, stopping movement after the first update while the button is
still held. This is not an orbit angular limit or a PBR/framebuffer problem.
The existing runtime UI guard already admitted this ID. The old scroll event gate
explicitly required Edit, although the Simulate update branch used the same editor
camera. It also updated an unused second orthographic controller in Edit.

Projection is now discoverable as PerspectiveProjection / OrthographicProjection,
shared by SceneCamera and the transient NavigationCamera. PerspectiveCameraController
receives logical-point deltas and seconds; editor gesture routing belongs to
Hazelnut/Camera/ViewportNavigation. SceneCamera remains the authored projection
owner; no serialized camera schema or game-specific camera is replaced.

## Read-only MetalCup comparison

Inspected `~/Projects/MetalCupWorkspace/MetalCupEngine/MetalCupEngine`:
`Core/Rendering/OutlineSystem.swift`, PickingPass in `RenderPasses.swift`,
`Assets/Shaders/FinalShaders.metal`, `PickingShaders.metal`, and
`Game/Cameras/EditorCameraController.swift`. No MetalCup files were changed.

MetalCup renders a separate depth-tested uint pick target, then compares selected
IDs with neighboring texels into an outline mask, and blends that mask in final
presentation before tone mapping. That produces coverage boundaries rather than
triangle/material edges. The inspected simple picking fragment emits only an ID;
it does not itself demonstrate masked-material alpha coverage. Its editor camera
receives frame input, retains a pivot, uses modifier orbit/pan/dolly and RMB fly;
its numeric speeds and architecture are not adopted wholesale.

Hazel already writes entity IDs alongside the real forward mesh draw, after
alpha-mask discard and scene depth testing, then carries IDs through HDR
presentation. This is a better source of exact visible coverage here than a second
geometry/mask pass. The editor can compare this existing ID image and draw only
an edge into its separate encoded viewport output. Picking IDs, world lighting,
UI, game output and Nutella remain independent of selection presentation.

Implementation is committed through `5b06590`; automated qualification is below.
Physical input acceptance remains open. Stage 5 has not begun.

## Example camera audit

| Project | Camera behavior and checkpoint decision |
| --- | --- |
| Rendering Showcase | Exact named authored views by default; optional native perspective inspection in Play and Nutella. No assembly or new content dependencies. |
| Rendering Reference | Fixed diagnostic cameras and captures stay unchanged; editor workspace navigation remains available. |
| Sandbox | `Start.hazel` is the simple planar authoring demonstration: pan, zoom, F and Home in Edit/Simulate. UI Tutorial retains its screen-UI/runtime behavior. No game-script replacement. |
| SceneTransitions | Fixed 2D scene/loading demonstration; free movement would obscure its purpose. |
| MeadowRun | Aspect-fitting game camera is coupled to gameplay presentation; retained. |
| Skybound | FitCamera determines flight bounds and obstacle dimensions; retained. |
| LastLightkeeper | Game-specific fitting/follow/region camera behavior and owner content retained. |

The unused GLFW-polling OrthographicCameraController had no concrete caller after
removing the editor's duplicate camera. PlanarCameraController replaces it as the
actual reusable pan/zoom policy used by NavigationCamera. OrthographicCamera remains
the simple matrix/pose type used by Renderer2D's standalone callers and tests.

## Silhouette choice

Reuse the real RED_INTEGER target. No mask or second geometry pass is needed.
The editor requests a selected ID, framebuffer-pixel radius and encoded UI color
through the existing Framebuffer resource boundary. OpenGL owns the scoped state
and shader. An inner morphological edge cannot extend onto a foreground wall;
material slots and triangles of one entity share an ID and create no internal edge.
Only the separate RGBA8 viewport output changes after normal scene/UI encoding.
The game image, ID image, depth, lighting and Nutella output are not outline targets.
No selected mesh means no outline draw. Missing mesh coverage yields no fake box;
render diagnostics remain visible. Multi-sample silhouette targets are explicitly
unsupported by this operation, rather than silently approximated.

MetalCup's `API/EngineWindow.swift` dispatches mouse/key/scroll events before
unhandled deltas enter `Core/Input/InputAccumulator.swift`; button-up clears the
held state even when handled. Its frame snapshot accumulates motion and then
resets deltas. `EngineScene.updateCamera` applies the editor controller to an ECS
camera transform. Hazel keeps its existing event/frame routing and uses private
view poses instead: no editor camera entity or transform-authority architecture
was imported. MetalCup's outline is two-sided and participates in HDR final
presentation; Hazel's inner edge lives on visible selected pixels in an editor-only
encoded output, so it does not paint a foreground occluder or vary with exposure.

## Final controls and view ownership

Planar pans with middle/right drag, Shift-left or Option/Alt-left; scroll zooms.
Perspective uses Option/Alt-left or Space-left orbit, Shift-left/middle pan,
Option/Alt-right or scroll dolly, and right-button look with WASD plus Q/E.
Shift boosts fly from 3 to 15 metres/second; Ctrl slows it to .6. Mouse look uses
logical-point deltas without timestep or framebuffer-scale multiplication.
Keyboard motion uses elapsed seconds. F frames the selected world subtree; Home
resets the active editor mode. Empty selection does nothing; degenerate bounds
stay finite. Help is drawn inside the viewport without owning an input item.

A held gesture remains owned outside the image. Mouse-up, Escape, focus loss,
mode changes and zero-size viewports release it; entry/re-entry seeds deltas.
Other widgets, text, popups and active gizmos block acquisition. Navigation
shortcuts cannot change selection or tools during a gesture. Edit and Simulate
share scroll policy. Play's F/game keys do not alter the hidden editor view.
Planar/orbit workspace poses use the existing private persistence; no authored
camera fields, schema, history or project defaults were added or rewritten.

Restored orientations do not snap on a zero/first delta. Runtime inspection
preserves authored roll/eye/projection, and Home restores its exact starting
view. Inspection requires a primary perspective camera with unit world scale;
unsupported camera/parent scale reports a diagnostic instead of silently changing
the initial view. Normal authored rendering retains its existing camera support.

From the canonical directory:

```sh
python3 scripts/hazel.py run Hazelnut --config Release --project examples/RenderingShowcase/RenderingShowcase.hproj
python3 scripts/hazel.py run Nutella --config Release --project examples/RenderingShowcase/RenderingShowcase.hproj --inspect-camera
```

In Hazelnut Play, enable **Inspect camera**; **Restore authored view** resets it.
Inspection is an explicit transient input/view mode. Omit the Nutella flag for
fixed named views and deterministic captures. No script, importer, baker or new
asset dependency is required. The Showcase README lists the other named views.

## Executed qualification and remaining acceptance

Apple M4, OpenGL **4.1 Metal - 91.7**, matching Debug and Release builds:
**4/4 selected native executables passed in each configuration**:
CoreSmoke, Renderer2DSmoke, RuntimeSessionSmoke and EditorSmoke. This is focused
qualification, not a claim that all 18 executables or the five-game campaign ran.
Builds/tests ran sequentially. No CI was launched or polled, as requested.

Checks include a 229-degree uninterrupted synthetic-held orbit; leaving the
image; Space alternate; focus cancellation/re-entry; widget/gizmo ownership
policy; Shift held before right-button fly; mouse frame subdivision and time-based
fly; shared authored/transient projection math; restored pitch/roll; workspace
persistence; scene isolation, native history, save/reopen and Play/Stop.
The hidden native test window correctly rejects unfocused scroll; the same
production routing function verifies focused Edit/Simulate and Play exclusion.
A blocker-ID test uses a root hash because compacted ImGui windows can have empty
ID stacks outside Begin/End. Debug assertions were retained.

Actual GPU checks cover transformed nonrectangular indexed geometry, partial and
full foreground occlusion, alpha cutouts, exact ID preservation, no changes to
interior/occluder/background pixels, 1x/2x thickness and pass-state restoration.
Existing selected suites also cover 2D, direct/IBL, color, exposure-independent UI,
picking and fixed Reference cases. Gallery motion checks actually render a >90
orbit and visible slow approaches to both wood and leather, then restore the
exact saved view without changing runtime camera transforms or authored state.

Actual Release Hazelnut fixed, Nutella fixed and Nutella inspection launches each
rendered 60 frames and completed normal teardown in private source-free projects.
A relocated interactive Nutella package passed with the source checkout and SDK
access denied, without HDR/GLB inputs or a project DLL, and with valid native
loaded closure and unchanged signature. ZIP SHA-256:
`84831664bdc0d4ccd89475a03b9ef45bbe053b98b6ab224bd949fb0fcd2b961f`.
Logs and reproducibility data are under ignored `build/camera-navigation-evidence`.

**Not physically verified:** an actual mouse/trackpad Option drag on the owner's
M4, its uninterrupted >90-degree hard acceptance, the alternate gesture's OS
behavior, and subjective feel/visual readability in motion. The native UI tool
could not resolve the unbundled executable as an app target. Synthetic input,
native ImGui and GPU evidence above do not count as a physical-device pass.
Owner checks should include focus-loss return, gizmo/selection arbitration,
Retina edge thickness and the opt-in Gallery controls/reset in both hosts.
No skipped graphics or physical checks are counted as passes.

All 313 inventoried existing Reference/LastLightkeeper/Finder files remain
byte-identical, including frozen Core_v1/Import_v1/Direct_v1/IBL conditions and
prepared lighting. The owner recovery ref remains `bfbce811`; vendor pins/source
and private settings/saves/layouts were not committed. No repository IBL was
rebaked; existing disposable native test fixtures retain their prepare/cancel
coverage. There remains one canonical worktree, with no master merge or release.

Reviewable checkpoints: `955d18d` camera/input split; `44d9ef5` silhouette
operation/readbacks; `6f97718` final controller ownership and native test target;
`ed2bdf4` native Gallery inspection/editor integration; `5b06590` restored pose
and Debug test-scope repair.
