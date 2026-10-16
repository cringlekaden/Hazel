# Runtime UI integration

Baseline: clean feature/last-lightkeeper at 0213e48; existing content and saves are accepted. Both baseline CI jobs passed.

## Inventory

This inventory records the baseline. All applicable UI has since been migrated.

- LastLightkeeper: title/Continue/reset confirmation, credits and chapter screens; camera-child HUD, objectives, dialogue, journal, pause and save feedback. Manual projected mouse rectangles in Menu; panel visibility currently uses Transform scale.
- MeadowRun: title/completion buttons, HUD and objectives. SceneButton manually tests world-space rectangles.
- Skybound: title/restart/menu buttons, score/control ribbon and crash overlay. FitCamera moves HUD every frame; SceneButton tests world rectangles.
- SceneTransitions: Play and menu-return controls use manual world rectangles.
- Sandbox: prefab/physics demonstration; no UI to migrate.

## Checkpoints

1. Integrate the existing GameUI model into scene components, overlay rendering, assets, schema/snapshots and managed action/state APIs. Verify a camera-free scene and native scene/prefab/input regressions.
2. Add Hazelnut creation presets, Inspector, resolved selection/editing and viewport preview using the runtime layout. Verify authoring/save/reopen/Play/Stop.
3. Migrate all applicable authored examples; remove obsolete camera-child UI placement and hit tests while preserving controls, story and save logic. Document the implemented API.
4. Debug/Release, focused and existing regressions, all source and relocated projects, actual runtime/editor interaction, final Linux/Windows CI; push reviewable commits without merging.

## Architecture

Separate canvas, rect, flow, text-style, nine-slice and button components reuse SpriteRendererComponent/TextComponent. Plain authored values serialize; fonts and state sprites belong to ProjectAssets. Scene owns resolved geometry, text layout and input router. Hierarchy and canvas ordering determine both draw and hit priority. Overlay uses backend-independent depth/scissor commands with state restoration and explicit batch boundaries. Text wrapping/alignment uses renderer font metrics. Mixed emphasis uses styled text children, with no markup parser. Canvas modal state suppresses managed gameplay input; UI-specific key polling remains available for menu decisions. Actions are consumed by scene identity and live entity validation, never callbacks.

## Completed integration

Engine, managed bindings and native Hazelnut authoring are implemented. The
[authoring guide](../runtime-game-ui.md) documents creation, Inspector controls,
viewport editing, prefab reuse and action consumption using the actual APIs.
LastLightkeeper's twelve scenes, MeadowRun, Skybound and SceneTransitions use
authored UI. Sandbox keeps its intentional world-space welcome text and physics
demo; it has no menu or HUD to migrate. Accepted gameplay, LK1–LK4 migration and
all six Lightkeeper licenses remain unchanged.

Local Debug/native and Release/OpenGL 4.1 suites passed all sixteen engine/editor
regression executables, including camera-free rendering, clipping/hit agreement,
managed component bindings, modal routing and scene/prefab/lifecycle isolation.
All five source projects passed startup/render/resize/shutdown checks in both
configurations. All five Release Nutella packages passed relocated checks with
source trees unavailable. Cold Debug/Release C# builds were verified separately
after fixing the pinned Premake response-file directory race in SDK tooling.
Migration checkpoint `1fcc2f4` passed both Linux and Windows CI.

Live authoring checks also caught and fixed ImGui's viewport background MoveId
canceling runtime pointer gestures. Editor input now admits that specific gesture
and existing pointer capture while retaining real-widget/text/popup guards; Play
clicks no longer select scene entities. Canvas selection outlines its screen-space
safe area instead of a world transform. Focused Debug/Release editor regressions
passed after these fixes. A final visual check moved the remaining MeadowRun/Skybound
ribbons into canvases and replaced SceneTransitions' floor-tile button with the
existing Kenney Pixel Adventure frame, retaining its included CC0 license and
source provenance in that example's export.

Visual inspection and live input checks supplement the behavioral regressions;
they do not establish human aesthetic acceptance or playtime. Remaining scope is
documented in the guide: rectangular clipping, bounded flows and styled sibling
text, without scrolling lists, editable text boxes or automatic content sizing.

## Runtime UI hardening (P1 and bounded early P2)

`feature/runtime-ui-hardening` contains production `master` at `f50c5559d0`
and its audit document checkpoint `b836e1290e`, without changing legacy rect/flow
semantics, scene schema, example scenes or save formats. It reproduces/fixes D1–D5 through model, scene, host/session and managed
fixtures; D6 uses resolved-edge inversion and a reversible native viewport
gesture; D7 corrects SceneTransitions' notice while retaining its Kenney license
and provenance in export. D8 and P3–P7 remain outside this milestone.

The scene owns an indexed, immutable geometry generation. Exact dependency-byte
comparison detects raw native writes at input/render/pending-delivery boundaries
and coalesces unchanged values; paint caches remain independent. Structural
retirement cancels subtrees; inactive input cancels both queues. Rendering and
focus/hit eligibility use the same framebuffer coverage. No renderer sorting or
new UI control family is introduced. The current guide describes the supported
hit, queue, mutation, encoding, editing and debugger contracts.

Native authoring now includes edge/corner edits with Escape cancellation,
computed flow positions and **View → Runtime UI Debugger**. The debugger reads
the actual snapshot and transient router, with optional bounds/clip/hit overlays
and CPU counters. This is not the complete P2 layout/undo workspace or a claim
that the whole UI system is production-ready.

### Local hardening verification and performance

Baseline reproductions reran the audit's source-equivalent binaries: inactive
outer activation survived, fully clipped focus/Accept activated, overlong and
surrogate UTF-8 passed/BOM failed, ancestor removal retained a child activation,
and cancelled pointer release plus Accept activated. The old D6 viewport recipe
moved the opposite edge by 5–70 logical pixels in fixed/pivoted, stretched and
minimum-constrained cases; flow movement wrote an ignored offset. Focused model,
scene, GPU, session/domain and native-editor regressions now cover these paths.
The 125% fixture additionally checks exact framebuffer-edge acceptance/rejection;
input routes in framebuffer space to avoid inverse-conversion rounding.

Debug and Release builds completed with two jobs and cached vendor dependencies.
All 16 regression executables passed in Debug/native and Release/native and
forced OpenGL 4.1, including new queued-action domain/scene replacement and editor
post-write overflow rollback checks. All five source projects passed logical/native gameplay and owned-window
editor/player startup, resize and shutdown in Debug/native and Release/OpenGL 4.1
(the latter uses the isolated software driver for application/package checks).

A disposable SceneTransitions copy was also inspected in real Release windows:
Nutella Play entered Level1 and Escape returned to the menu. Hazelnut selection
showed actual button bounds/coverage in the docked debugger, idle generation reuse,
and bounds/clip overlays; an Escape-cancelled right-edge drag restored the original
380×64 logical rect. This is a bounded interaction check, not full UX acceptance.

All five Release Nutella game archives passed relocated gameplay/save and owned-
window acceptance on the OpenGL 4.1 software path with source examples/resources
and Mono SDK unavailable. Both default Hazelnut/Nutella distributions passed
extracted startup/render/resize/shutdown and applicable CLI checks. Private output
directories preserved existing distributions. SceneTransitions' corrected notice,
Kenney CC0 license, provenance manifest and exact MenuFrame SHA-256 were confirmed
in its game archive and both default application archives. The first Windows CI build
found Windows SDK `near`/`far` macros colliding with resize-edge variable names.
Renaming those locals preserves the algorithm; Debug/Release model regressions
passed afterward. Linux/Windows CI runs at the corrected final integration
checkpoint; local HD4000 results do not qualify Windows hardware, mixed-DPI
desktop workflows or macOS.

Release profiles ran on Grandpa on 2026-10-14 at source checkpoint
`fda399b65d36e1ca439beb248e4ec28f02aafebd`: CachyOS kernel 7.2.7-1, GCC 16.2.1,
i3-3227U/8 GB, accelerated Intel HD4000/Mesa 26.2.4. Native GL4.2 and a forced
GL4.1 context both used the portable GLSL410 path. There was no concurrent build
or regression run. Private resources/settings/cache directories and an owned
1280×720 framebuffer isolated the probes from authored content and saves.

Fixtures contain 64/512/2048 declared rects plus their canvas, with 8/128/512
controls, default Open Sans text and solid-color sprites. The 512-node fixture
has clipped list-like content (80 submitted elements); 2048 stresses visible
mixed text/images. These are current rect/text stand-ins, without scrolling,
virtualization, state-texture/nine-slice loading or 1000 bound inventory records.
Setup uses one staged scene import. Font generation/shader setup is excluded;
20 warm frames precede 300 idle samples, 30 changed-offset samples and 20 separate
GL_TIME_ELAPSED samples. Values are **median/p95 milliseconds**:

| Context | Rects/controls | Input dependency check | Routing | Render dependency check | Changed resolve+route | Submission wall time | GPU query span |
| --- | --- | --- | --- | --- | --- | --- | --- |
| native | 64/8 | 0.0713/0.1253 | 0.0040/0.0058 | 0.0558/0.0993 | 0.2372/0.3184 | 2.5802/8.0464 | 2.4007/10.9367 |
| native | 512/128 | 0.5382/0.8720 | 0.0098/0.0164 | 0.4701/0.7759 | 1.5986/2.6027 | 3.2397/5.3070 | 2.2863/2.9577 |
| native | 2048/512 | 2.2935/3.3806 | 0.0302/0.0529 | 1.9849/3.3028 | 6.4642/9.2404 | 79.7102/90.3163 | 79.5878/87.2343 |
| gl41 | 64/8 | 0.0687/0.1263 | 0.0039/0.0053 | 0.0556/0.0985 | 0.1844/0.2553 | 2.4978/4.5684 | 2.0704/10.9159 |
| gl41 | 512/128 | 0.5414/0.8998 | 0.0100/0.0173 | 0.4745/0.8197 | 1.5825/2.8034 | 3.5403/6.3759 | 2.5562/11.8850 |
| gl41 | 2048/512 | 2.1992/3.5478 | 0.0303/0.0568 | 1.9597/3.2572 | 6.2075/7.5021 | 80.2579/91.9056 | 80.0939/87.3577 |

Idle checks/routing made **zero ordinary non-aligned C++ new allocations** across
300 samples and kept one published generation in every fixture/context. This
counter excludes rendering, aligned allocation and GL/OS malloc. Draw calls/quads
were 63/218, 80/275 and 2047/8660 in both contexts. Routing is indexed; pointer
occlusion remains a reverse painter traversal. Changed resolutions reuse vector
and stable hash-node storage, but are not allocation-free.

Submission timings measure CPU-side wall time, including driver waits; GPU query
spans are separate measurements, not values to add to submission. Twenty GPU
samples give a coarse tail estimate, not a universal bound or pure GPU busy-time
claim. No Release before/after speedup is inferred from the audit's loaded,
nonoptimized diagnostic timings. The proposed 1/3 ms CPU and 2 ms larger-fixture
GPU targets **have not passed**. This foundation publishes the measured envelope
above with no 60-Hz promise for the stress fixture. Per-element image/text flushes
remain a measured submission limit; batching/persistent vertex work requires its
own painter-order, clipping, blending and picking acceptance. No texture sorting
or renderer redesign was added to this milestone.


Remaining focused manual acceptance (use a disposable project copy):

- Select fixed, stretched/negative-delta, pivoted and minimum-constrained rects;
  drag each edge/corner, verify the opposite edge, and cancel with Escape. Repeat
  with focus loss and minimization. Flow children expose preferred size/margins
  and computed position; their viewport gestures must leave authored values alone.
- Open **View → Runtime UI Debugger** in Edit and Play. Inspect selected bounds,
  clipping, generation and ownership; compare bounds/clip/hit overlays with the
  image and test keyboard focus on a partly/fully clipped control.
- Activate then deactivate the viewport/window before consuming; restore and
  verify no delayed action. Repeat Play/Stop and inspect focus/capture reset.
- On Windows hardware, exercise mixed-DPI monitors and detached editor viewports;
  the isolated fractional framebuffer tests do not qualify those OS workflows.

## P2: shared scene/prefab UI edit history

Built on `6d744604bda9046650c04a55615cd5cfd43366ed` on the same feature branch.
`DocumentHistory` is owned independently by the existing scene and prefab drafts.
Typed `DocumentCommand` entries accept context from the owner; `UIEditCommand`
stores authored before/after fields, document/entity IDs and transient lifetime
stamps. Neither entries nor save points store GPU/font/cache/managed objects or
registry handles. Generic history supports later scene/prefab command types.

Inspector draws retain the observed scene transiently and recheck the document,
selection and component lifetimes after property callbacks, before publication.
Inspector property helpers expose scoped interaction boundaries, with drafts
validated before publication. Continuous changes coalesce into one entry;
text/numeric commits, anchor presets and image/font assignments use the same
transaction path. Sprite-sheet Save and Assign routes UI source changes through
the same scene history, after successful sheet I/O; captured assignment targets
include entity lifetime tokens. World-sprite/clip and structural edits keep their
existing paths. Complete viewport gestures reuse D6 inversion, include
release-frame motion and commit once. Cancel/no-op produces no entry and retains
the redo branch. Selection, focus/minimize, target retirement, document transitions
and Play cancel unfinished edits before replacement/runtime copying. Cancellation
restores matching fields on unchanged-lifetime components; explicit native
replacement/overrides take precedence. Undo/Redo rejects conflicts atomically.

Save accepts its point only after successful I/O. Dirty checks retain the existing
exact binary authored-state comparison, including out-of-history mutations; no
per-frame YAML/document serialization was added. Evicting a command/save cursor
never discards the accepted content baseline. Limits: 100 retained operations/
64 MiB conservatively accounted command payload; one in-flight command separately
capped at 64 MiB, with growth rejected before mutation. Accepted document bytes
and temporary validation buffers are separate existing/editor storage. New edits
discard redo; oldest-entry eviction and rejected operations have concise feedback.

**Edit → Undo/Redo** shows command names; Ctrl+Z, Ctrl+Shift+Z and Ctrl+Y respect
text fields, popups, active gestures and authoring availability. Scene/prefab
switching keeps independent history; Open/New/Close/recovery replacement resets
its document. Stop preserves authored history and valid selection. Native field
conflicts require reverting the external edit or clearing obsolete document
history. Vectors/colors are atomic fields for conflict checks; first-touch baselines
preserve native values introduced before a field joins the transaction. Unassigned
image modes remain editable; changing a fileless generated GPU source is rejected
before publication because history cannot retain that resource. Structural/world/
script editing and sprite-sheet history remain outside this slice; those
operations still mark the document dirty.

Local qualification: Debug and Release builds with two jobs; all 16 Debug native
regression executables; the final focused Debug editor regression, including
shared history, real document ownership, successful/failed saves, recovery,
sprite assignment, retirement and repeated Play/Simulate/Stop. Disposable native
editor walkthroughs exercised numeric dragging, text-field local Undo, anchor
presets, fixed/stretched/minimum-constrained/pivoted resizing, Escape cancellation,
save/reopen, independent prefab edits and computed flow positions. These checks
do not qualify mixed-monitor DPI or the complete detached-window workflow.
The final integration checkpoint uses the existing Linux/Windows C/C++ CI,
including Debug/Release regressions, authored games and extracted OpenGL 4.1
packages; consult the run associated with the feature commit for platform results.

Focused manual acceptance on each desktop platform:
- In a disposable menu, drag one Inspector value, then one viewport edge;
  each release adds one named entry. Undo/Redo restores edges and dirty state.
- Escape a drag; switch selection/document or enter Play during another drag;
  verify cancellation and no stale entry. Stop retains valid authored selection.
- Assign an image/font, save, Undo/Redo and reopen. Repeat in a prefab; scene
  history must remain independent. A failed save must retain the accepted point.
- Edit text with Ctrl+Z while its field is active, then leave it and use document
  Undo. Verify focus loss and detached/mixed-DPI windows on real platform hardware.

The existing large-UI submission bottleneck and unmet performance budgets above
are unchanged; no batching, typography dependency, control or layout semantics
were added.

The next layout slice from this checkpoint is recorded below.

## P2 versioned sizing and measure/arrange

Starting at `f8b05a85c21a20fc7d799f42b80998ec7da48fe2`. LayoutVersion 1
(also missing fields) retains legacy anchors, SizeDelta, pivot and flow exactly.
Version 2 sizes border boxes per axis: Fixed uses FixedSize; Fill uses definite
parent content space after margins; Content measures visuals/children plus padding.
Minimums are never shrunk. Optional finite maximums must be at least minimums.
Content width is natural width, capped by an explicit maximum, rather than
implicitly squeezed by its parent. Fixed/Fill or a Content maximum supplies text
wrap width before Content height is measured. Empty/zero-width text measures zero.
Images measure source-region pixels as logical units; Content on both axes keeps
native aspect. Other combinations can stretch; there is no implicit aspect lock.

Free children use point anchors, Offset and the final-size Pivot. Fill owns its
position on that axis: anchor/offset/pivot must be zero; margins supply insets.
Stacks own positions, ignoring anchor/offset/pivot and cross Stretch resizing:
only Fill owns expansion. Fill main-axis children start at minimum and divide
remaining space by positive Flex (zero means weight 1), redistributing at maximums.
Fixed/Content children never shrink. Overflow retains sizes and reports warnings;
Main/Cross alignment positions spare space. Hidden collapses space/gaps; disabled
participates; clipping affects drawing/hits, not measurement. Padding clamps the
available inner box at zero. A zero surface suspends geometry and input.

Content measures free child extents from zero anchor/pivot (Offset can overflow
negatively), or stack sums/maxima. Content/Fill and anchored Content dependency
cycles fail with entity/axis paths, without iterative convergence. A new free
subtree can live in a legacy free parent; new/legacy mixed stacks and legacy
children of new groups require conversion. Nested canvases stay independent.
No derived layout values persist. Failed editor transactions restore authored
values; no partial snapshot is published and input is cancelled on resolution
failure. Runtime errors follow the existing host fail-stop path.

Typed sizing/editor edits and save/load validation also check the dependency graph
with discarded zero visual metrics, including hidden nodes and zero surfaces; this
probe never supplies published geometry or loads GPU assets. Runtime raw writes
are observed at the existing boundaries; hidden/zero-surface geometry is deferred
until resolved. Text caches retain natural/constrained line layouts, image metrics
observe base-source identity/resource extent and asset epoch, and changed layout
passes reuse node/child storage. Unchanged dependency bytes reuse the snapshot.

Conversion uses native Inspector preview and a staged scene copy. Point-fixed,
full Fill with nonnegative insets, and complete legacy stacks retain representable
responsiveness. Partial spans/negative insets require explicit Fixed freezing;
hidden/interrupted targets must be revealed/repaired, and mixed stacks converted
together. Logical geometry and exact framebuffer coverage are compared before one
shared history command commits. Cancel, failed validation and stale preview leave
the document unchanged. No automatic conversion/save occurs. Fixed-axis gestures
also reject parent reflow/alignment that would move the opposite edge; Inspector
size edits remain available. New fields participate in command comparison **and
publication**, exact authored state, serializer, copy/prefab and runtime APIs.

Current backend observed 2026-10-15: Plasma Wayland compositor, GLFW X11/GLX through
Xwayland `:0`, accelerated physical Intel HD4000 / Mesa 26.2.4 / core OpenGL 4.2.
The inherited shell's X11 session label/auth path were stale; the active Xwayland
auth path was used for tests. This is neither native Wayland support nor Xvfb/
software rendering. Prior CI at `f8b05a8` passed Linux and Windows. Existing
Sandbox Start.hazel user edits are outside this implementation.

Next bounded P2 slice: explicit fixed-track grids using the same sizing/dependency
and document transaction contracts, with cell placement, native Inspector and
save/reopen tests. Auto tracks, wrapped containers, aspect modes, scrolling,
structural/multi-selection history and the known per-element rendering bottleneck
remain separate work. This slice does not make the full UI system production-ready.
