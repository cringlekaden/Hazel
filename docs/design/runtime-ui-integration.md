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

## P2 versioned sizing and measure/arrange (historical checkpoint)

> Superseded by the canonical-container slice below: the version-1/2 resolver and conversion tooling described in this checkpoint were subsequently removed at the user's direction. These paragraphs record what the earlier checkpoint verified, not the current support contract.

Starting at `f8b05a85c21a20fc7d799f42b80998ec7da48fe2`. LayoutVersion 1
(also missing fields) retains legacy anchors, SizeDelta, pivot and flow exactly.
Version 2 sizes border boxes per axis: Fixed uses FixedSize; Fill uses definite
parent content space after margins; Content measures visuals/children plus padding.
Minimums are never shrunk. Optional finite maximums must be at least minimums.
Content width is natural width, capped by an explicit maximum, rather than
implicitly squeezed by its parent. Fixed/Fill or a Content maximum supplies text
wrap width before Content height is measured. Empty/zero-width text measures zero.
Images contribute source-region pixels as logical content units before padding
and independent axis constraints. A standalone unconstrained Content image with
zero padding uses native dimensions. The sprite/background fills the border box;
padding, combined text, constraints or other modes can stretch it. No aspect lock.

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
children inside new groups are unsupported. Convert a uniform legacy subtree
before attaching it there; an already invalid mixed tree must first be detached/
repaired, since it has no supported baseline geometry to preview. Nested canvases
stay independent.
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
hidden/interrupted targets must be revealed/repaired, and uniform legacy stacks
converted together (already invalid mixed trees must be repaired first). Logical
geometry and exact framebuffer coverage are compared before one
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

Verification at the implementation checkpoint:

- GCC/Premake Debug and Release builds with two jobs; vendor pins unchanged.
- All 16 Debug regression executables qualified: the canonical native run passed
  its first 15, then the corrected full EditorSmoke passed. The complete canonical
  Release `gl41` run passed all 16 on physical Intel hardware (GL/GLSL 4.1 forced).
- Model tests cover capped weighted Fill (including extreme weight saturation),
  minima/maxima, empty/hidden/zero/overflow, constrained content, dependency paths,
  deterministic arrangement and owned-axis gesture rules. Scene tests cover real
  font/text/image metrics, native writes/reflow, generation/cache reuse, fractional
  drawing/picking/routing agreement and serialization/copy/prefab round trips.
- The real ImGui editor harness creates a measured menu stack and corner HUD,
  changes a longer label, resizes the viewport, edits constraints and complete
  gestures, converts legacy fixtures, and verifies shared Undo/Redo, accepted/
  failed saves, save/reopen, independent prefab histories and repeated Play/Stop.
- The strict three-game logical/runtime regression passed, and all five source
  games passed both player/editor lifecycle checks without authored file changes. These launchers'
  `gl41` profile uses llvmpipe through Xwayland, unlike the hardware regression
  profile above. All five freshly packaged games passed relocation/lifecycle
  checks, and extracted Hazelnut/Nutella passed with source resources and the SDK
  unavailable, unrelated working directories and Unicode paths. SceneTransitions
  retains its packaged Kenney provenance and CC0 license.

Desktop startup/capture on the physical GPU was inspected. KWin's EIS permission
prompt blocked the earlier XTest helper's native pointer/key delivery after the
Wayland switch. Native click-by-click Inspector/gesture acceptance remains a
manual check; no unsuccessful injection was counted as a pass. No desktop packages
or persistent settings were changed. No new CPU/GPU performance budgets are
claimed: unchanged generation/text-measurement reuse is regression-tested, while
the known per-element draw bottleneck and broader font-cache work remain open.

Next bounded P2 slice: explicit fixed-track grids using the same sizing/dependency
and document transaction contracts, with cell placement, native Inspector and
save/reopen tests. Auto tracks, wrapped containers, aspect modes, scrolling,
structural/multi-selection history and the known per-element rendering bottleneck
remain separate work. This slice does not make the full UI system production-ready.

## Canonical containers and image presentation

The scope correction supersedes the version-compatibility plan above. Repository
UI uses one measured model; schema 3 rejects obsolete UI Rects.
The legacy resolver and conversion UI have been removed. The Sandbox user draft
remains uncommitted with its presentation preserved and schema fields updated.

Grid: 1–64 explicit Fixed/weighted-Fill tracks per axis, at most 4096 cells.
Fixed tracks clamp their authored sizes; Fill reserves minima then shares remaining
space with capped redistribution. Content container axes sum Fixed tracks/gaps;
Fill tracks require a definite container axis. Tracks never grow from children.
Explicit cells/spans reserve first; remaining children take first fitting cells
in row-major order. Overlap, out-of-range spans and capacity exhaustion reject
atomically. Hidden grid children retain reservations; tracks/empty cells remain.
Main/Cross align children horizontally/vertically within cells; anchors/offsets/
pivots do not own slot positions. Child minima may overflow a cell without shrink.

Wrap: Fixed/Fill main axis is required. Visible children break greedily in hierarchy
order using Fixed/Content sizes or Fill minima, including margins/item gaps.
Oversized first items retain size on their own line. Each line distributes its
remaining main space among Fill items; cross thickness is the maximum non-Fill
size or Fill minimum. Cross Fill fills that line thickness, not the entire panel.
LineGap separates lines; Main aligns each line, Cross aligns its items, LineAlign
positions the line block. Cross Content sums line thickness/gaps plus padding.
Hidden items collapse; disabled items participate. Indefinite child/text/container
dependencies reject with entity/axis paths, without convergence iterations.

Image fit does not size controls: base-image region pixels supply intrinsic
measurement; active button-state images use their own region for painting only.
Stretch keeps background painting over the border box. Contain/Cover/Native fit
inside padding, with normalized alignment and inherited clipping. Cover crops UVs
within the selected atlas region; Native uses source pixels as logical units.
Nine-slice requires Stretch and owns border fitting. Hits remain the arranged
control box/effective clip, including image letterboxing; fitted paint geometry is
separate transient diagnostic data. World sprites and renderer batching are unchanged.

The canonical slice removes `ResolveRect`, `ArrangeFlow`, the Rect legacy mode and
`UILayoutConversion`/its dialog/commands. One schema-3 Rect has one point Anchor;
Fixed/Fill/Content is the only sizing interpretation. All repository-owned UI
scenes/prefabs and fixture APIs are reauthored; no permanent migrator ships.
`ResolvedSprite::SourceSize` records atlas region pixel dimensions separately
from half-texel-safe sample UVs. Layout, nine-slice validation and fit now use
that extent. Image-fit diagnostics are last-paint values, separate from the
immutable layout snapshot; fitting/alignment are paint-only changes.

Stage checkpoints: canonical/grid `a73d455`, wrap `5cdd75b`. Focused CPU checks
pass for track/span/cap allocation, horizontal/vertical wrap, overflow/cycles,
and image fit/crop/alignment/zero inputs. Integration qualification follows
below; these CPU checks do not establish GPU performance.

Qualification: Debug and Release built with bounded `-j2`. All 16 Debug native
and all 16 Release hardware `gl41` regression executables passed. Focused runtime/GPU integration also passed on
physical HD4000 forced to GL4.1; managed layout/image round trips and domain/
Start-Stop regressions passed. The real editor harness exercised native creation
commands, shared Inspector transactions, history, save/reopen, prefab reuse,
Play/Stop, the actual ImGui Inspector/static preview and GPU entity picking.
These are automated native interactions, not physical desktop pointer acceptance.

Before removing the obsolete resolver, a private native probe captured 872
visible resolved bounds across repository scenes/prefabs at 1280x720, 1024x600,
960x720 and 1920x1080. The canonical implementation produced the same keys and
logged values (zero mismatches at 0.01 logical-pixel tolerance). The uncommitted
Sandbox draft was included; its content/presentation remains local and its old
file/diff were privately backed up before the required schema edits. Hidden
rects are additionally covered by dependency/participation tests. Temporary
conversion code and old header exports were removed; no migrator ships.


Release CPU layout profile on Grandpa (i3-3227U, Intel HD4000, 8 GB), no concurrent
build/test: 20 warm-ups, 200 unchanged boundaries and 100 alternating-width
changes per fixture. Times include native-write source detection and resolution,
not input routing, drawing, presentation or GPU duration. The context used physical
Intel Mesa 26.2.4, GL4.1 forced, GLFW X11/GLX through Xwayland on Plasma Wayland.

| Container / children | Unchanged median / p95 ms | Changed median / p95 ms |
| --- | --- | --- |
| Grid / 64 | 0.1250 / 0.1358 | 0.2746 / 0.4315 |
| Grid / 512 | 0.5486 / 0.7219 | 2.0230 / 2.4856 |
| Grid / 2048 | 2.2717 / 2.9400 | 9.2452 / 14.6242 |
| Wrap / 64 | 0.0684 / 0.0899 | 0.2385 / 0.2824 |
| Wrap / 512 | 0.5479 / 0.7949 | 1.9336 / 2.6154 |
| Wrap / 2048 | 2.3112 / 3.0323 | 8.8735 / 9.9195 |

Every unchanged fixture reused its generation and counted zero C++ `new`
allocations. Changed-boundary allocation totals over 100 samples were
3300/26159/133405 (grid) and 91/550/42704 (wrap); storage reuse does not make large
reflows allocation-free. These simple rect fixtures do not establish text-heavy,
GPU or whole-frame budgets. Run the existing integration executable with
`--profile-layout` to repeat; the normal regression suite does not time fixtures.


All five source examples passed player/editor startup, nonblank rendering, resize
and graceful shutdown checks with authored descriptor/scene hashes unchanged.
The strict three-game native regression and managed gameplay/progress/save tests
also passed. These launcher checks use two-thread llvmpipe GL4.1 through Xwayland;
they are distinct from the physical-GPU regression suite/profile above. Fresh
Lightkeeper and Sandbox startup captures were inspected. The active desktop was
confirmed using logind and KWin/Xwayland processes: inherited shell XDG variables
still reported the retired X11 session. Native Wayland is not supported/tested.

The final Nine Slice Add Component guard requires Stretch first; it never silently
changes image fit. Both affected editor workflows passed again in Debug/Release.
The native-authored grid/wrap/image fixture exported through normal dependency
closure and passed extracted Nutella startup/render/resize/shutdown with source
resources and SDK unavailable; its atlas, wrapped chips and combined image/text
capture were inspected. No fixture content was added to the example games.

Final package qualification passed: all five newly built game archives passed
relocated startup/render/resize/shutdown, and the strict native gameplay/save/
lifecycle regression passed against the extracted projects and shipped Mono.
Extracted Hazelnut and Nutella also passed from unrelated Unicode working paths
with source resources and SDK unavailable. These checks used llvmpipe GL4.1
through Xwayland. Master, stash, vendor pins and settings remain unchanged; the
Sandbox user draft is the only uncommitted file.

Physical acceptance still to perform: create Grid/Wrap from Hierarchy, assign
atlas regions in Properties, edit track counts/spans, narrow the preview, and
check debugger track/line/image overlays. Drag a Fixed edge, cancel with Escape,
then Undo/Redo and save/reopen a scene and prefab; computed axes must refuse edits.
Verify Nine Slice is unavailable until Stretch is selected. Repeat Play/Stop and
check the result in exported Nutella. KWin's earlier input permission block was
not bypassed; no physical pointer/key acceptance is claimed by the native harness.

Next bounded milestone: profile and reduce runtime UI draw submission/flush costs
using contiguous painter-ordered batching, with clipping, alpha blending, text/
image interleaving and entity-picking regressions. Preserve native-write detection
and measure source/resolve/draw CPU separately from GPU duration. Larger changed
layouts still allocate and should remain in that performance investigation. Do
this before expanding into large scrolling/data views. Structural/multi-selection
history, broader authoring tools and the rest of P2 remain open; the whole UI
system is not yet production-ready.

F5 startup follow-up: restoring a workspace with a selected, expanded UI Button
exposed an Inspector regression: its loop used the seven creation-preset count
against five state labels/colors/images/pickers. Bounds now derive from the state
array with compile-time checks across all state storage; the creation menu derives
its own count independently. The native ImGui fixture now restores an expanded
button and opens all five state-image editors, in addition to the grid Inspector.
The earlier grid-only render checks did not cover this path. Reproduction and
verification use private scene/workspace copies; no user settings reset is needed.

Follow-up verification passed: bounded Debug/Release builds, both focused native
editor suites with the expanded-button regression, and a no-argument Debug GDB
launch using the F5 signal policy and a private copy of the remembered Sandbox
workspace. That launch restored the selected button, rendered, resized and closed
normally. Original scene, session, preferences and ImGui-layout hashes were
unchanged. The desktop run used physical HD4000 via Xwayland; the Release editor
suite forced GL4.1. Other runtime/layout code and scene formats were unchanged.

### Painter-ordered submission and rendering qualification

The runtime UI pass drains pending world geometry before replacing its camera or
raster state. It appends primitives in existing snapshot painter order to bounded
Renderer2D runs. Adjacent compatible primitives merge; image/text, font-atlas and
line-width changes retain separate ordered draws. Each occupied primitive buffer
uploads once per chunk, rather than once per element. Texture-slot, geometry and
8,192-run capacity boundaries submit before storage is reused. There is no texture
sort, combined image/text shader, or second layout model.

Each primitive captures the snapshot's inward-snapped framebuffer clip. The
fragment test uses half-open framebuffer coordinates, including empty clips;
texture samples/MSDF derivatives precede discard. Differing clips therefore need
no mutable scissor boundary. UI disables depth/scissor during its pass, submits
before restoring the host's depth/scissor box and line width, and leaves viewport,
framebuffer and blend configuration untouched. Material bindings are renderer
owned; following world/overlay passes establish their camera/material normally.
Entity IDs, sprite UVs, image fitting and nine-slice geometry retain their existing
paths. Focus outlines retain the former line-before-glyph ordering.

CPU diagnostics separate exact source detection, layout, preparation and buffer
submission. UI-only counts are differences around the pass, excluding the drained
host geometry. GPU timestamp pairs use a bounded asynchronous ring: read only
available results, skip timing when full, and expose the originating submission.
Their command interval can include CPU-feeding gaps; it is not pure GPU busy time
or presentation latency. No normal rendering wait, synchronous per-sample query,
or `glFinish` is introduced.

A completed GLSL chunk uses a free fragment sampler for its font when available. Full
texture chunks share unit zero and restore white before later quad runs; this
keeps all configured quad slots. Sampler changes happen after the previous chunk
submits, and the text sampler returns to zero at EndScene for ordinary world text.
SPIR-V programs retain declared sampler binding zero: named lookup is not guaranteed
by [ARB_gl_spirv](https://github.com/KhronosGroup/OpenGL-Registry/blob/main/extensions/ARB/ARB_gl_spirv.txt)
and its shared-unit fallback is covered by the same ordered renderer. A llvmpipe
GL4.6 reproduction failed the immediate-reference ID comparison before this guard;
the physical GL4.1 profile uses GLSL and its measured path is unchanged.
Multiple atlases stay pinned by their runs until submission. The renderer retains
its existing 20,000-quad-per-primitive and 40,000-line-segment limits per chunk.

Prepared normalized glyph geometry is scene-owned, separate from snapshots and
last-paint image diagnostics. Text bytes, font/path/asset epoch, wrapping width,
kerning, line spacing, size, content dimensions and alignment invalidate it.
Position, canvas scale and tint remain live draw values. Exact native detection at
input/render/pending-delivery boundaries is unchanged; paint-only writes can reuse
a layout generation while rebuilding glyph data. Image fitting/source UVs are
still read from current presentation/resource state; direct texture uploads stay
visible without layout invalidation. Retirement/pruning/reset release glyph data.
Retention is capped at 16 MiB per scene and 4 KiB of source text per cached control;
longer text or an exhausted budget uses uncached preparation, within the existing
64 KiB UI-text limit. Capacity is reused; this is not an atlas/font-family cache.

Inactive instrumentation now avoids clock reads, owned profile-name construction
and locking. A session epoch prevents an old scope entering a replacement trace.
Layout warnings retain typed codes/entity IDs and static messages instead of
allocating the same warning string for each overflowing child on every reflow.
These changes do not alter authored fields, scene formats, history or UI routing.

**Release qualification on Grandpa, 2026-10-20.** Frozen baseline `0383019` added
phase counters/profile fixtures to `79648e2` without changing its per-element
submission path. Final measurements use the new run/cache implementation. Same
1280x720 offscreen RGBA8/entity-ID/depth target, no MSAA, i3-3227U/HD4000, Mesa
26.2.4, physical Intel GL4.1 forced, GLSL410, 16 texture slots, GLFW X11/GLX through
Xwayland on Plasma Wayland. Native Wayland and Xvfb were not used. No concurrent
build/test; normal desktop activity and driver scheduling remain sources of jitter.

Each fixture/mode has 20 warm-up and 120 CPU samples. GPU timestamp pairs use a
32-pair ring in the probe; only ready results are collected, with `glFlush` outside
the CPU sample and no waits. Baseline collected 119–120 GPU samples, final 109–119;
zero ring-saturation skips. Pending tail samples are discarded. GPU intervals may
include CPU-feeding gaps and other scheduling, especially at many ordered draws;
they do **not** measure GPU busy time or end-to-end frame/presentation latency.
The normal UI timer uses 16 pairs and reports pending/unsupported hardware rather
than waiting. Timestamp counter availability/wrap are handled. See the
[Khronos timer-query contract](https://github.com/KhronosGroup/OpenGL-Registry/blob/main/extensions/ARB/ARB_timer_query.txt).

Fixtures are maintained in `RenderProfile.h`: N translucent image rectangles,
each with its own clip; **mixed** additionally puts four-glyph text on every second
rectangle (3N image/glyph quads). Mode **unchanged** repeats the boundary/pass;
**move** alternates one rect's offset; **paint** alternates one native image resource
and, for mixed, one four-character label. All fixtures retain exact painter order.
A move rebuilds the current whole layout, not just that rect. Text/image mutation
in these Fixed boxes leaves layout generation unchanged and still repaints.

Below, each cell is **baseline median/p95 → final median/p95 ms**. Source is exact
native dependency detection; layout is Build/measure/arrange; preparation includes
shared `Scene::PrepareSprites` plus UI preparation/pass bookkeeping; submission
is timed buffer Flush/upload/draw work. Boundary+paint includes publication and
reconciliation overhead, so individual phase percentiles should not be summed.
Debugger preparation excludes the shared scene-wide PrepareSprites call. These
profiles exclude input routing, gameplay/scripts, world draws and presentation.

Unchanged:

| Fixture / N | Source | Layout | Preparation | Submission | Boundary+paint | GPU interval |
| --- | --- | --- | --- | --- | --- | --- |
| images / 64 | 0.0774/0.1337 → 0.1078/0.1614 | 0.0000/0.0000 → 0.0000/0.0000 | 1.0336/1.5118 → 0.1214/0.2271 | 0.5212/3.2046 → 0.0220/2.0687 | 1.6280/4.8627 → 0.2636/2.3293 | 0.2693/0.2738 → 0.0419/0.0622 |
| images / 512 | 0.6733/1.0541 → 0.5857/1.0231 | 0.0000/0.0000 → 0.0000/0.0000 | 11.7180/14.6926 → 0.4422/0.7731 | 6.8840/11.9942 → 0.0386/0.0712 | 19.5945/27.3361 → 1.0801/1.8806 | 15.3490/21.4493 → 0.4702/0.7624 |
| images / 2048 | 2.3811/3.2527 → 2.4952/3.2064 | 0.0000/0.0000 → 0.0000/0.0000 | 41.7161/44.3729 → 1.7865/2.2358 | 21.7934/25.6108 → 0.1084/0.1522 | 66.2076/73.2030 → 4.4701/5.3669 | 60.3767/65.9783 → 2.3854/2.9812 |
| mixed / 64 | 0.0803/0.1116 → 0.0927/0.1540 | 0.0000/0.0000 → 0.0000/0.0000 | 1.3608/1.8178 → 0.1411/0.2226 | 1.2377/3.0773 → 0.3389/0.6033 | 2.6928/4.8009 → 0.5941/0.9620 | 1.3240/4.6063 → 0.3815/1.9601 |
| mixed / 512 | 0.5979/0.8215 → 0.9132/1.2255 | 0.0000/0.0000 → 0.0000/0.0000 | 11.8924/13.1423 → 1.0651/1.3710 | 10.6684/12.7509 → 3.1980/4.3360 | 23.2735/25.9350 → 5.2569/6.4993 | 21.2827/24.6070 → 5.0650/9.5294 |
| mixed / 2048 | 2.3843/3.0358 → 2.9885/3.6733 | 0.0000/0.0000 → 0.0000/0.0000 | 47.8586/53.3623 → 3.4150/4.2649 | 43.4124/52.2363 → 17.1169/19.0429 | 93.9778/108.7344 → 23.7344/26.1793 | 90.1349/104.8554 → 19.6930/25.8299 |

One moving rect:

| Fixture / N | Source | Layout | Preparation | Submission | Boundary+paint | GPU interval |
| --- | --- | --- | --- | --- | --- | --- |
| images / 64 | 0.0722/0.1197 → 0.0840/0.1528 | 0.1621/0.2840 → 0.1808/0.3138 | 1.0271/1.4449 → 0.0927/0.1737 | 0.5096/2.2850 → 0.0193/0.0373 | 1.7917/3.6777 → 0.4027/0.6438 | 0.2686/0.2722 → 0.0401/0.0450 |
| images / 512 | 0.6059/0.8615 → 0.5460/0.6276 | 1.1956/1.7246 → 1.2124/1.7033 | 9.9066/11.4578 → 0.3761/0.6164 | 5.0486/7.1804 → 0.0342/0.0483 | 17.0862/19.9177 → 2.1973/3.0323 | 12.4131/15.9264 → 0.7594/0.8718 |
| images / 2048 | 2.3916/3.5033 → 2.3853/3.2192 | 5.0812/6.6198 → 5.3678/6.6219 | 42.7887/52.4261 → 1.5036/1.9688 | 21.2912/27.9346 → 0.1011/0.1230 | 72.2500/89.6516 → 9.5752/11.0094 | 60.8094/77.5548 → 3.4030/3.4075 |
| mixed / 64 | 0.0778/0.1209 → 0.1242/0.2095 | 0.1576/0.2058 → 0.2648/0.3945 | 1.3582/1.8129 → 0.1920/0.2980 | 1.2521/1.8652 → 0.4542/0.6826 | 2.8597/4.0203 → 1.0604/1.4545 | 1.3218/3.8494 → 0.5699/3.2299 |
| mixed / 512 | 0.5979/0.7909 → 0.8830/1.2038 | 1.1915/1.8179 → 1.7264/2.3177 | 11.9481/13.0960 → 0.9673/1.2298 | 10.6573/11.5789 → 3.5220/4.3313 | 24.6078/26.3887 → 7.0547/8.5599 | 21.0458/24.5177 → 5.9822/11.2336 |
| mixed / 2048 | 2.3774/2.7890 → 2.6757/3.3955 | 4.9963/6.3915 → 5.5547/7.5655 | 48.2495/50.3149 → 2.8548/4.0715 | 42.9238/47.4155 → 19.0033/25.5453 | 99.1486/103.7965 → 30.7090/38.5616 | 89.2402/95.3888 → 22.0376/29.4598 |

One changing image/label:

| Fixture / N | Source | Layout | Preparation | Submission | Boundary+paint | GPU interval |
| --- | --- | --- | --- | --- | --- | --- |
| images / 64 | 0.0733/0.1116 → 0.0697/0.1174 | 0.0000/0.0000 → 0.0000/0.0000 | 1.0049/1.4231 → 0.0944/0.1600 | 0.5178/2.3577 → 0.0203/2.0564 | 1.5969/3.5952 → 0.1960/2.2584 | 0.2690/0.2764 → 0.0314/0.0704 |
| images / 512 | 0.6139/0.9097 → 0.5708/0.8459 | 0.0000/0.0000 → 0.0000/0.0000 | 10.0870/11.1016 → 0.4235/0.6958 | 5.0229/6.7932 → 0.0443/0.0688 | 15.9084/17.7802 → 1.0586/1.5646 | 12.7870/15.9016 → 0.3827/0.6037 |
| images / 2048 | 2.3670/3.0286 → 2.4814/3.2760 | 0.0000/0.0000 → 0.0000/0.0000 | 42.1859/45.2738 → 1.8196/2.7541 | 21.5514/27.4793 → 0.1184/0.1652 | 66.3785/73.9084 → 4.4821/5.6949 | 60.6232/68.0514 → 2.3318/2.3366 |
| mixed / 64 | 0.0814/0.1174 → 0.1709/0.2698 | 0.0000/0.0000 → 0.0000/0.0000 | 1.3769/1.7242 → 0.2723/0.4016 | 1.2541/1.7711 → 0.5996/0.8400 | 2.7251/3.9781 → 1.0685/1.5949 | 1.2795/3.7770 → 0.5914/3.6441 |
| mixed / 512 | 0.5985/0.8201 → 0.9265/1.3054 | 0.0000/0.0000 → 0.0000/0.0000 | 12.0487/13.4332 → 1.0927/1.5365 | 10.7493/12.0007 → 3.1968/3.9655 | 23.5348/25.3577 → 5.2500/6.3094 | 21.5034/24.8984 → 4.9592/10.1676 |
| mixed / 2048 | 2.3780/3.1384 → 3.0783/3.9959 | 0.0000/0.0000 → 0.0000/0.0000 | 48.2829/55.1592 → 3.4607/4.7259 | 42.8558/50.2502 → 17.7301/25.1289 | 93.8784/107.3428 → 24.5103/32.9873 | 89.3878/103.0132 → 20.3511/29.6940 |

Counts below apply to every mode. Flush counts were entirely **End** before and
remain one **End** upload group after; texture/capacity/external flushes are zero
in these particular fixtures. Separate regressions force those rollover paths.
Image/text and font boundaries remain draw runs, not buffer reuploads.

| Fixture / N | Draws before → after | Upload bytes before → after | Ordinary C++ new, 120 unchanged samples, before → after |
| --- | --- | --- | --- |
| images / 64 | 64 → 1 | 12288 → 16384 | 69240 → 120 |
| images / 512 | 512 → 1 | 98304 → 131072 | 553440 → 480 |
| images / 2048 | 2048 → 1 | 393216 → 524288 | 2213760 → 1920 |
| mixed / 64 | 96 → 65 | 32768 → 45056 | 84600 → 120 |
| mixed / 512 | 768 → 513 | 262144 → 360448 | 676320 → 480 |
| mixed / 2048 | 3072 → 2049 | 1048576 → 1441792 | 2705280 → 1920 |

The retained clip vec4 increases vertex upload bytes; fewer uploads/draws provide
the measured win. Final unchanged/move mixed fixtures reuse 32/256/1024 prepared
labels (4/32/128 KiB), with zero rebuilds after warm-up. Paint mode rebuilds one
label and reuses the rest. New counts for moving rects equal unchanged counts;
paint adds 120 allocations to final totals (baseline added 240). The residual
1/4/16 allocations per frame originate in shared sprite-preview progress message
construction, plus changed sprite resolutions. Counts cover ordinary C++ new,
not aligned new, driver/OS malloc, setup or retained bytes; no allocation-free
whole-frame claim is made.

A measured intermediate at 2048 mixed, before free-sampler/glyph reuse, was
3.9895/6.3417 ms preparation and 18.9093/25.6200 ms submission, 26.1874/34.5272 total.
The free sampler alone measured 3.9367/4.6971, 17.3262/19.4094 and 24.6049/27.0462.
Final unchanged is 3.4150/4.2649, 17.1169/19.0429 and 23.7344/26.1793. These small
incremental differences have scheduling noise; the robust result is removal of
per-element uploads and inactive trace allocations, not a universal cache speedup.
The largest mixed case still has 2049 required ordered runs and substantial CPU
submission cost. No universal 16.7 ms budget passes; do not use these fixtures to
certify long captions, many fonts, heavy overdraw or whole-game frame times.

The existing `--profile-layout` separately repeats 20 warm-ups, 200 unchanged
boundaries and 100 alternating-width grid/wrap reflows. It does not draw or route
input. Same-run baseline versus final below; timings are median/p95 ms and new
counts are totals across 100 changed boundaries. Every unchanged fixture reused
its generation and counted zero ordinary C++ new.

| Container / children | Unchanged before → after | Changed before → after | Changed new before → after |
| --- | --- | --- | --- |
| grid / 64 | 0.0664/0.1195 → 0.0673/0.1053 | 0.2512/0.2894 → 0.2454/0.4216 | 3300 → 100 |
| grid / 512 | 0.5121/0.6961 → 0.5207/0.5605 | 1.9787/2.5856 → 1.9234/2.1836 | 26159 → 559 |
| grid / 2048 | 2.1100/3.0038 → 2.1367/2.4655 | 9.4427/11.1669 → 8.5799/9.8556 | 133405 → 2105 |
| wrap / 64 | 0.0641/0.1005 → 0.0650/0.0874 | 0.2324/0.2826 → 0.2349/0.3841 | 91 → 91 |
| wrap / 512 | 0.5120/0.7562 → 0.5206/0.6377 | 1.8948/2.6197 → 1.8981/2.8592 | 550 → 550 |
| wrap / 2048 | 2.2188/3.0895 → 2.2002/2.8032 | 10.0908/17.3172 → 10.1859/15.2196 | 42704 → 2104 |

These changed-allocation totals include the first dirty build of the spare
snapshot: the warm-up only exercises the unchanged generation. Its previously
empty index alone adds one map node per resolved element. Do not interpret the
100-sample total as a steady per-frame allocation rate; zero steady-state reflow
allocation is not claimed. The render profile separately warms each mutation
mode before counting. Large source detection/reflows remain linear costs. Static
warning text removes most observed overflow-related allocations without retaining
per-document strings. This work does not add virtualization, scrolling, typography
libraries or change D8's existing text coverage/blend convention.

Renderer checkpoint: **`272a5aa`**. Debug and Release built with bounded `-j2`;
the full 16-executable suite passed once in Debug physical/native GL4.2 and once
in Release physical/forced GL4.1. After the final SPIR-V sampler guard, both
configurations were rebuilt/relinked, and affected checks passed again: Release
hardware Core/Renderer2D/UI integration, software llvmpipe GL4.6 Renderer2D
(default SPIR-V plus two-slot GLSL), and both native editor suites. The expanded
Rendering costs section was explicitly opened in the real ImGui harness. The
unchanged physical GLSL paths reuse the broader suite/example evidence rather
than repeating all historical checks. The software specialization test is not
HD4000 GPU-performance evidence.

New regressions compare full 128x128 RGBA (one-LSB tolerance) and exact entity IDs
against immediate painter order for translucent quad/text/line/circle overlap,
multiple fonts, nested/empty/adversarial clips. They assert one draw for 64
compatible different-clip quads; force texture-slot, all geometry/index and
8,192-run rollover; exercise a completely full sampler chunk and subsequent
ordinary world text. Real scene checks cover pending host geometry/stat isolation,
raster/line-width restoration, nine-slice picking, fractional framebuffer coverage,
raw text/font/alignment/image/resource mutations with unchanged generations,
prepared reuse/oversized fallback, retirement/reset and replacement scenes.
Inactive tracing/session retirement is checked separately. The existing atlas
fit/crop/color/picking, scene/prefab/history, P1 interaction, scripting/domain and
reliability regressions remain in the suite. Native editor fixtures now render
three complete Play/Stop cycles and verify unchanged authored state/history;
the expanded-five-state Button Inspector regression remains enabled.

All five source projects passed player/editor startup, rendering, resize and
shutdown on physical HD4000 GL4.2 via Xwayland, with authored hashes unchanged.
The existing strict native three-game gameplay/save/lifecycle checks and managed
example contracts passed. Fresh Lightkeeper menu, HUD and Mara dialogue captures
were inspected. These are bounded automated/native observations; they do not
claim physical pointer/key usability or native Wayland support. No desktop
packages/settings were changed and no denied input injection is credited.

A fresh Nutella/LastLightkeeper archive from checkpoint `272a5aa` passed relocated
startup/render/resize/shutdown from an unrelated Unicode working directory, with
source resources/examples/SDK and the bundled GAC temporarily unavailable. It
loaded shipped libraries/resources, used private saves/data, and rendered on
physical HD4000 forced to GL4.1 through Xwayland. The extracted menu capture was
inspected. Export dependency/license closure passed; no new dependency or asset
license was introduced. Master, stash, vendor pins and the exact Sandbox draft
remain unchanged. No example scene/prefab/script was reauthored for this work.

Remaining physical acceptance: launch Lightkeeper from F5, inspect its menu,
dialogue and HUD at 1280x720 and 1024x600, and check focus/hover/selection. In
Hazelnut open **Runtime UI Debugger → Rendering costs**, inspect positive clip/
submission counts, delayed GPU age and selected bounds; repeat Play/Stop and
inspect image/text picking and the expanded Button Inspector. Repeat ordinary
resize/focus/minimize behavior on the actual display and Windows hardware. CI is
triggered by the final push and **not awaited**; no Windows hardware result is
claimed locally.

Remaining limits: large mixed interfaces still incur primitive/program changes
and many draws; source detection/reflows are linear and changed layouts still
allocate. Font-family/atlas ownership remains a separate cache problem; the new
16 MiB cap covers prepared glyph geometry only. D8's coverage/blend correction,
broader P2 authoring/history, controller/navigation, scrolling and production
controls remain open. The whole UI system is not production-ready.

Recommended next bounded audit milestone: P3 text-rendering correctness and
resource ownership—fix D8 with analytic coverage/opacity reference tests on
contrasting backgrounds, and define bounded font/atlas lifetime and failure
behavior. Keep the existing layout model and painter-run contracts. Larger
scrolling/data views should wait for their navigation/control prerequisites and
a visible-item/virtualization budget that accounts for the measured mixed-run
cost; do not certify them from image-only throughput.

## P3: text coverage and bounded font resources

D8 reproduction used a private RGB8 atlas filled with distance 128/255, a small
quad forcing `screenPxRange=1`, and an independent RGBA8 numeric reference.
On physical HD4000 GL4.1, red at 40% authored opacity read 21 versus expected 41.
The shader now outputs straight tint RGB and `As = coverage * authoredAlpha`.
Default straight-alpha RGB blending gives `Cout = Ctint*As + Cdst*(1-As)`.
For text-only transparent targets, Cdst/Cout are accumulated associated RGB;
clear to transparent black (or an associated clear color).
Text submission alone uses `Aout = As + Adst*(1-As)`; its scope captures/restores
attachment-zero blend factors/equations, leaving enable, other attachments and
sprite/material semantics unchanged. This also keeps an opaque target opaque.
Ordinary sprites retain their existing `As² + Adst*(1-As)` stored-alpha behavior;
a mixed transparent render target is therefore not a general reusable opacity
layer. This milestone does not change sprite or render-target material semantics.

RGBA8/RGB8 are linear UNORM formats; Hazel currently does no sRGB texture decode,
framebuffer conversion or automatic tint transfer. Equations apply to the stored
numeric channels, not a claimed physically linear presentation pipeline. MSDF
coverage/derivatives still evaluate before nonuniform clip/zero-alpha discard;
zero coverage/opacity does not write picking. The same shader/submission policy
serves world text, UI, window and offscreen targets, GLSL410 and SPIR-V.

`Font::Load` is a small context-wide LRU, keyed by canonical source identity,
bounded content digest and fixed generation parameters. ProjectAssets keeps weak
accepted references and bounded failure metadata, not a second owning font cache.
Refresh/reload reads files explicitly; draw/measure boundaries perform no freshness
filesystem probes. Same content reuses the accepted font. A completed changed face
publishes a new immutable Font/atlas; older prepared geometry keeps its own Font.
CPU draw runs pin atlas textures until submission; backend command lifetime covers
already submitted GPU work. Atlas-only external owners are included in live counts.
Eviction releases cache ownership and cannot invalidate pins. Project retirement
releases its root's reusable entries; Trim and renderer Shutdown release reusable
and default ownership before context destruction. Native resource owners must
retire on the renderer thread before Application teardown, as with other GPU Refs;
shutdown diagnoses remaining owners, and construction/cache APIs reject wrong
threads. This is not an asynchronous font-generation API.

Default limits (tests may lower them only with no live owners; a valid cache
configuration must admit at least one maximum-size atlas):

| Resource | Limit / accounting |
| --- | --- |
| Opened font input | 4 MiB, checked before allocation/read; no whole-file oversized read |
| Per atlas | 1024×1024, 3 MiB RGB8 level zero, also checked against the backend limit |
| Reusable LRU | 8 entries / 12 MiB atlas bytes; oldest entries evict first |
| Live atlas objects | 24 / 24 MiB, including default, caches, active and pending-draw pins |
| Controlled generator scratch | 32 MiB: input + RGB8 atlas + two float-RGB glyph buffers + two error buffers |
| Work | One synchronous font generation, two workers; Latin-1 only, ≤32768 loaded outline edges |
| Project reference/error metadata | 256 entries; additional references use default with one limit warning |

Atlas bytes estimate format storage, not driver overhead, deferred GPU deletion,
CPU metrics/outlines, allocator capacity or total process memory. FreeType parsing,
outline loading/normalization before the edge check, kerning structures, MSDF
per-edge work and worker stacks have dependency-owned allocations outside the
scratch calculation. Input/glyph/work/count limits constrain exposure but do not
establish a hard whole-process temporary-memory bound. The existing 16 MiB
prepared-glyph capacity remains a separate per-scene limit.

Admission first evicts reusable entries; if live pins still prevent it, loading
rejects before atlas generation/upload. Project resolution retains the last
accepted resource (including failed replacement after admission pressure), or
uses shipped Open Sans when none remains. An evicted inactive resource is not a
promise to retain historical bytes indefinitely. Failure diagnostics are cached,
logged on change, and retried only by explicit Reload/Refresh; files are never
silently rewritten. A failed shipped default at renderer startup is fatal.
Validation/export share CPU font planning and remain strict about malformed,
missing or oversized packaged references. Editor documents retain such references
and reopen with editable problems. History restores paths even after file loss.

Accepted asset epochs invalidate Content measurement/wrapping and prepared UVs
together; a failed replacement uses the same accepted metrics/atlas for both.
Raw native text/font-path writes remain observed at the existing boundaries.
**UI Text Style** shows atlas diagnostics and Reload font; **Runtime UI Debugger →
Font resources** distinguishes reusable, live and pinned estimates/limits.
There are no author-facing tuning switches or new typography dependencies.

Qualification at implementation checkpoints `184f965` and `b4977f0`: Debug and
Release Premake/GNU Make builds used two jobs. All 16 regression executables
passed in Debug physical/native GL4.2 and final Release physical/forced GL4.1.
Release Renderer2D also passed on software llvmpipe GL4.6 (SPIR-V and two-slot
GLSL). Tests exercise analytic RGBA/picking for partial coverage/opacity,
contrasting opaque and associated translucent backgrounds, overlapping text,
subsequent sprite semantics, real DrawString/scene UI/window targets and indexed
blend-state restoration. RGBA8 references disable sRGB conversion/dithering and
allow two byte levels for quantization/driver rounding; window tests assert RGB
because destination alpha is platform-selected. Existing ordered/clip/capacity,
expanded Button Inspector, scene/prefab/history and Play/Stop regressions remain.
Font checks lower count/byte/scratch/dimension limits without large allocations,
pin atlas-only and pending-draw owners, reject wrong-thread load/non-admitting
limits, preserve failed replacements, retry explicitly, compare changed content
with identical pathname/size/timestamp, and verify Content/wrap/prepared version
consistency, editable missing-font round trips and strict export rejection.

Warm Release rendering repeated the existing 64/512/2048 image/mixed fixtures,
including unchanged/move/paint modes, on Grandpa: i3-3227U/8 GB, kernel
7.2.7-1-cachyos, GCC 16.2.1, physical HD4000/Mesa 26.2.4-arch3.1 forced GL4.1,
GLSL410, 1280×720 RGBA8/entity-ID target. GLFW X11/GLX ran through Xwayland on
Plasma Wayland; no native Wayland or Xvfb claim. No build/test ran concurrently.
The same 20 warm-ups/120 CPU samples and asynchronous GPU timestamps apply.
The table is **baseline `1bac10d` → final, median/p95 ms**, unchanged mode:

| Fixture / N | Boundary + paint CPU | GPU command interval |
| --- | --- | --- |
| images / 64 | 0.4194/0.6459 → 0.1992/2.2214 | 0.0478/0.0977 → 0.0408/0.0437 |
| images / 512 | 1.2563/2.1653 → 1.0930/1.6399 | 0.6106/0.8719 → 0.4057/0.7627 |
| images / 2048 | 4.3917/6.1196 → 4.5582/5.0780 | 3.4058/3.4078 → 2.6499/3.4046 |
| mixed / 64 | 0.7148/1.2661 → 0.8535/1.3238 | 0.5735/0.6150 → 0.5790/1.9949 |
| mixed / 512 | 6.0896/7.5105 → 5.2158/6.2452 | 5.6207/8.2588 → 4.7278/9.6325 |
| mixed / 2048 | 23.7743/30.4500 → 24.7913/32.8851 | 17.6966/24.9083 → 21.3736/29.0111 |

This is boundary/source/layout/preparation/submission wall time, excluding input,
gameplay, world drawing and presentation; GPU intervals can include CPU-feeding
gaps and are not GPU busy time or values to add to CPU time. Draw/upload/ordinary
C++ new counts are unchanged: images one draw, mixed 65/513/2049 draws; 120/480/
1920 new calls over 120 unchanged samples. Prepared labels reuse 32/256/1024
entries; paint rebuilds one. The 2048 mixed submission phase rose from
16.9387/23.6508 to 18.3867/27.1424 ms. Correct text alpha requires state transitions
between alternating image/text runs; redundant equation calls are skipped when
already ADD, but those required boundaries remain. Small-case/tail variation
shows scheduling noise; no general speedup or universal frame budget is claimed.
Cold font generation is excluded. Large mixed rendering, linear source/reflow
costs and changed-layout allocations remain separate performance debt.

All five source examples passed Release physical-HD4000 GL4.2 player/editor
startup/render/resize/shutdown with descriptor/scene hashes unchanged. Existing
strict native three-game gameplay/save/lifecycle and managed progress contracts
passed. Fresh Lightkeeper menu/dialogue/HUD/world-label and Sandbox captures were
inspected; authored colors were not adjusted to reproduce D8's darkening.
A newly exported Lightkeeper Nutella archive passed relocated physical GL4.1
startup/render/resize/shutdown from an unrelated Unicode directory with source
resources/examples/SDK and GAC unavailable. Manifest/dependency closure, exact
shipped Open Sans bytes and Apache license were checked. Private post-extraction
missing/corrupt custom-font references both rendered with fallback and unchanged
authored references; both fault captures and normal package text were inspected.
No extra font/dependency/notice was introduced. Master, stash, settings, vendor
pins and the exact unrelated Sandbox draft were preserved. CI is triggered by
push and **not awaited or polled**; no new local Windows qualification is claimed.

Remaining physical acceptance: in a disposable project assign a TTF/OTF in
**UI Text Style**, compare partial-opacity text over dark/light panels and world
text, then rename/corrupt the file and Reload. Verify the last accepted/fallback
message, retained path and normal wrapped Content layout; restore the file and
Reload, Undo/Redo, save/reopen and repeat Play/Stop. Open **Font resources** to
compare cache/live/pinned estimates and limits. Repeat on Windows hardware and
with actual desktop focus/DPI transitions; native ImGui harness results are not
physical pointer/key or native Wayland acceptance.

Next bounded P3 slice: native font-family descriptors (regular/bold/italic face
references), declared coverage diagnostics and package/notice validation on these
ownership budgets. Review shaping/bidi/boundary dependencies and the language
corpus separately before expanding glyph coverage. This slice does not complete
P3, broader P2 authoring, or the production UI system; large mixed rendering and
scrolling/virtualization prerequisites remain open.

Final cache-pressure follow-up: Load/Refresh also reuses an identical live
accepted version after its LRU entry was evicted; it must not generate a duplicate
atlas or reject that unchanged face under pin pressure. The regression checks
same-content identity with the reusable entry absent and the live budget full.
After this final guard, Debug and Release focused Font/UI integration/native
editor suites passed again; rendering/SPIR-V/profile/example/package paths are
unchanged and reuse the broader qualification above. Both applications were
relinked against the final font implementation. Full native Resource Refs remain
context-bound; no cross-thread/final-owner-after-context support is implied.

## P3: native font-family assets and declared coverage

Built on `6cbe965`. `FontFamilyDefinition`/`.hfont` Version 1 owns bounded project
metadata: a required Regular face, optional real Bold/Italic/Bold Italic slots,
per-face notice associations and Required/Sample UTF-8 corpora. All paths are
asset-root-relative. `TextComponent::Font` is the sole authored family/style
selection for UI and world text; UI Text Style retains geometry/alignment only.
Empty selection uses the four already shipped Open Sans faces. Missing optional
styles select Regular explicitly; no synthetic weight/slant or inactive fallback/
locale/shaping fields exist. Raw nonempty UI FontPath references reject rather
than invoke an old authoring path. Writers remove empty markers; the exact local
Sandbox draft keeps its bytes and its empty marker has no selection semantics.

ProjectAssets keeps at most 128 family metadata/failure records with weak accepted
face references. It does not own a second atlas cache. Font's existing bounded
LRU/admission rules remain; a bounded weak live-identity index lets families share
an identical still-live face even after eviction. Active Text components and UI
prepared data pin immutable accepted fonts; draw runs pin atlas textures through
submission. Scene copies/duplication clear transient text pins, Stop releases them,
and project/renderer retirement keeps the P3 resource-thread contract. Scene/
prefab history captures only authored selections, including world-text Inspector
edits. The earlier 16 MiB prepared-glyph and cache/live atlas limits still apply.

Descriptor/face refresh happens explicitly at first use, Reload or Refresh/Play,
never as a filesystem freshness probe per frame. A malformed/missing dependency
retains the accepted descriptor and live face; rejected atlas admission retains a
live accepted face or uses engine Regular. Failed references are not retried on
every draw. Asset epoch and exact family/style native comparisons invalidate
Content/wrapped measurements and prepared geometry together. Measurement and
painting use the same accepted immutable face. Inactive evicted historical bytes
are not guaranteed retention. Runtime never rewrites the authored selection.

Coverage uses strict scalar decoding and CPU font/atlas planning without texture
generation/upload. It distinguishes source cmap presence, the CPU-planned
Latin-1 glyph set, conservative unsupported combining/joining/bidi flags and `?`
replacement. Source inspection describes current files, not a potentially older
accepted resource after failed reload; preview separately reports actual glyph
availability in its accepted atlas. Ungenerated faces have no live atlas.
CR/newline/tab/FEFF follow rendering's control policy. The recognizer
is not a Unicode property table or proof of correct language typography. Both
corpora cap at 8192 bytes/4096 unique scalars, with 32 problem details per result;
descriptors cap at 64 KiB, names at 128 bytes, paths at 1024 bytes and notice lists
at eight per face. Required coverage must pass on every supplied style before
export; Sample is informational. A valid unmet requirement can be saved for repair,
but cannot falsely pass export. Existing FreeType/MSDF parsing scratch limitations
remain; no new dependency/font is shipped.

Content Browser's **New Font Family…** and `.hfont` Open use the native asset editor
with face/notice choose/import, bounded coverage inspection and an explicit real
face preview. Detailed declarations/diagnostics are collapsible. Family drafts
use FileDocument atomic writes/external-conflict checks and the existing editor
Save All, guarded replacement/close, Play/export and Save Copy/Reopen/Cancel
services. Asset draft Undo is not added (same scope as sprite-sheet drafts);
scene/prefab family/style edits use shared history. No Python is needed to author,
inspect or assign a family. Runtime remains independent of ImGui.

Native export audits every `.hfont` and referenced family, all supplied face styles
and their notices, preserving root-relative structure. Duplicate slots/notice paths,
malformed/oversized content, external/traversal/symlink escapes, missing dependencies
and unmet required atlas/typography coverage reject with family/style/file context.
The normal packager verifies these dependencies are included, and separately
validates all four engine default faces and notices before/after resource staging.
An associated notice is not a redistribution-permission certification; fixtures
reuse shipped Apache-licensed Open Sans without new assets.

Implementation checkpoint **`30fcb1b`** passed the focused Release font, real-scene/UI,
CPU-only document and native editor checks. Full Debug and Release builds passed
with two compiler jobs and unchanged dependency artifacts. All **16 regression
executables per configuration** passed: Release on physical HD4000 forced
OpenGL/GLSL 4.1/410, Debug on the same hardware's native GL4.2 context. This includes D8's
independent analytic compositing and ordered-run/clip/picking regressions, managed
family/style round trips, accepted-version reflow, cache pressure and teardown.
The native ImGui harness creates/imports a family, edits/inspects its declaration,
saves/reopens with shared conflict guards, assigns through scene/prefab history,
checks real preview glyph readback and repeats Play/Stop. It also retains the
expanded five-state Button regression. An earlier development run mixed objects
compiled while a header was changing; those objects were rebuilt before the
passing editor/full-suite checkpoints. No crashed run is counted as a pass.

Representative Release warm check on Grandpa: i3-3227U/8 GB, kernel
7.2.7-1-cachyos, GCC 16.2.1, Intel HD4000/Mesa 26.2.4, physical GL4.1 forced,
GLSL410, 16 texture slots, 1280×720 RGBA8/ID target, no concurrent build/test.
GLFW X11/GLX runs through Xwayland on Plasma Wayland; this is not native Wayland
or Xvfb. Existing profile infrastructure uses 20 warm-ups / 120 CPU samples,
nonblocking timestamp pairs (111–119 ready GPU samples, zero saturation skips).
Values below are median/p95 ms; comparison is the previously recorded P3 resource
checkpoint, whose unchanged warm drawing path also applies to `6cbe965`.

| 64 elements, unchanged | Previous CPU boundary+paint → current | Previous GPU interval → current |
| --- | --- | --- |
| Images | 0.1992/2.2214 → 0.1994/2.3887 | 0.0408/0.0437 → 0.0402/0.0619 |
| Mixed images/text | 0.8535/1.3238 → 0.6489/1.0484 | 0.5790/1.9949 → 0.4911/1.8662 |

Draws remain 1/65, ordinary C++ new totals 120 across 120 samples, and mixed
preparation reuses 32 labels / 4 KiB with zero unchanged rebuilds. Moving one rect
measured CPU 0.3552/0.6107 (images), 0.9446/1.5089 (mixed); changing paint/one
label measured 0.1938/0.3553 and 1.0290/1.4000, with one mixed label rebuild.
These exclude input, gameplay, world draws and presentation; GPU command intervals
include possible CPU-feeding gaps and are not busy time or additive to CPU.
Scheduling/tail variation is visible; no universal speedup/frame budget is claimed.
Large mixed-run costs and changed-layout allocation debt remain open. Family file
freshness/coverage are explicit operations, outside the warm rendering loop.

A normal export of the native-authored family fixture passed extracted Nutella
startup/render/resize/shutdown on physical GL4.1 from an unrelated Unicode path,
with source resources/examples/SDK and GAC unavailable. Project Regular/Bold faces,
associated notice and all four engine default faces/license were checked byte for
byte. Post-extraction missing/corrupt face, missing notice and malformed descriptor
cases all rendered safe fallback with unchanged authored references. Normal Bold
and fallback Regular text captures were inspected. Required-coverage failure is
separately asserted by native closure tests; notices are associations, not legal
permission certifications. No font/dependency/vendor/pin was added.

The Release source-example suite passed all five projects (MeadowRun, Skybound,
LastLightkeeper, Sandbox and SceneTransitions) in Nutella and Hazelnut, including
startup/render/resize/close and authored-file hash checks. Its existing bounded
gameplay/save/lifecycle checks and managed example builds passed too. These ran on
physical HD4000 native GL4.2 via Xwayland, not software rendering. Lightkeeper menu
and Sandbox text captures were inspected. The exact unrelated Sandbox draft,
master, stash and vendor pins remain unchanged.

Remaining physical acceptance: create a family from Content Browser (also try a
face's context menu), choose/import a face and correct notice with native dialogs,
inspect a required `café Ω` versus informational sample, and verify export rejects
unmet required atlas coverage. Assign real Bold/absent Italic in world and wrapped
Content UI text, Undo/Redo, save/reopen scene and prefab, then Play/Stop. Corrupt a
replacement, Reload and inspect accepted/fallback status; repair and retry. Repeat
on Windows hardware and actual desktop DPI/focus transitions. Native harness and
owned-window checks do not certify physical pointer/dialog usability or native
Wayland support. CI is triggered by push and **not awaited or polled**.

Next bounded P3 work is the shaping/bidi/boundary dependency and language-corpus
review, followed by a shared CPU shaped-layout adapter and glyph-ID/page ownership
acceptance slice. Choose actual shipping scripts, representative combining/bidi/
line-break cases and licensed faces within reviewed input/page budgets before
pinning HarfBuzz, boundary and bidi libraries (see the audit's alternatives).
Do not infer language support from cmap presence or expand the fixed Latin-1 atlas
as a substitute for shaping. Family/notice authoring does not complete P3 or make
the UI production-ready.

## P3: CPU typography foundation (renderer integration follows)

Built from `aff70d1`, preserving the current family/coverage/atlas/painter path.
No component/schema, family field, managed API, example, save format or authoring
switch is added. The current renderer and export coverage still use Latin-1.
The new service is the shared CPU foundation for the next glyph-ID integration,
not newly rendered language support or completion of P3.

### Dependencies and build review

All new source is pristine pinned Git submodules; existing gitlinks are unchanged.
Root-owned `scripts/dependencies/typography.lua` builds static archives with the
existing Premake configurations, C11/C++17 and matching Windows dynamic CRT.
Their ELF/Mach-O symbols are hidden so the private static HarfBuzz cannot
interpose or depend on GTK/Pango's independently loaded system HarfBuzz.

| Dependency / exact pin | Needed capability and selected data |
| --- | --- |
| HarfBuzz **12.3.2**, `b42511e071162fe76102f613a6ccc009726c99af` | OpenType substitution/positioning, glyph IDs/clusters, horizontal metrics and normalization; built-in Unicode **17.0.0** tables |
| libunibreak **8.0**, `28a2756b864c343f438cd22537d49d394d4666a5` | Unicode **17.0.0** UAX14 line and UAX29 extended-grapheme boundaries; word implementation exists upstream but is not needed/compiled here |
| SheenBidi **3.0.0**, `cfe430e7375a7845b679adae9d51dac6deaa8858` | Unicode **17.0.0** UAX9 paragraphs/levels, line L1/L2 visual runs and script locator (including paired-punctuation handling) |

The [HarfBuzz configuration](https://github.com/harfbuzz/harfbuzz/blob/12.3.2/CONFIG.md)
and [checked-in UCD table](https://github.com/harfbuzz/harfbuzz/blob/12.3.2/src/hb-ucd-table.hh)
were inspected against its public shaping/cluster/plan APIs. The inspected newer
14.6 tag already has Unicode 18 data; this pin keeps the foundation's Unicode data
aligned at 17. It includes the release's allocation/fuzzing fixes. This is an
explicit source selection, not a claim that future security updates are unneeded.
[libunibreak's release](https://github.com/adah1972/libunibreak/releases/tag/libunibreak_8_0)
contains updated grapheme/line tables; older releases' differing boundary versions
are not assumed equivalent. [SheenBidi's release](https://github.com/Tehreer/SheenBidi/releases/tag/v3.0.0)
adds Unicode 17 and allocator support. It supplies bidi, not shaping or line breaking.
The combination satisfies this bounded script/corpus scope without ICU.

HarfBuzz compiles upstream `src/harfbuzz.cc` with OT font funcs and built-in data;
no hb-ft/system HarfBuzz, ICU/GLib/Graphite/platform shaper, subset/raster/GPU/tools/
bindings target is enabled. AAT, variations, vertical/color/paint/draw and optional
file/environment APIs are disabled. CFF outline shaping remains available.
UniBreak compiles eight library sources; word/tests/generators are excluded.
Sheen compiles its unity source with scratch-pool caching disabled, without its
experimental editing API. Generated headers/tables already exist at these pins;
Premake builds never run Meson, Autotools, CMake, gperf or Unicode generators and
never fetch typography inputs. A source checkout/submodule acquisition is the
separate initial network step. Plain C/C++ and upstream Windows atomic branches
fit Linux/Windows and future macOS; no macOS build/support is claimed here.

Pinned existing FreeType is **2.11.0** (`2d57b059`), msdfgen `b9061f9`, atlas-gen
`b50e101`. HB's OT functions do not share or mutate their FreeType handles. All
face consumers use the same accepted bytes/index-zero face. Upcoming MSDF work
can open those bytes independently for glyph-index outline extraction.

Licenses: HarfBuzz Old MIT COPYING (and subsidiary notices), libunibreak zlib-style
LICENCE, SheenBidi Apache-2.0 LICENSE and source copyright, plus Unicode License V3
are packaged by the common application/game notice collector. Its formerly
missing `LICENCE`/`NOTICE` spellings are included; missing linked dependency/Unicode
notices reject packaging. Exact Unicode notice/provenance lives under
`scripts/dependencies/licenses/typography`. Test-only fonts/corpora are outside
runtime Resources/Assets and do not enter game packages.

### Corpus, results and ownership contract

`tests/fixtures/typography/corpus.tsv` declares English/French/German, composed and
decomposed accents, Greek/Cyrillic, Arabic joining with Latin/numbers, Hebrew
mixed direction, punctuation, ligatures, whitespace and a long unbroken string.
It is representative CPU test coverage, not every language/font feature.
Shipped Open Sans is reused for accepted Latin/Greek/Cyrillic face checks. One
unmodified static **DejaVu Sans 2.37**, 757,076 bytes, supplies a complete face for
mixed Arabic/Hebrew/Latin fixtures. The official archive checksum, exact font/
complete license hashes and redistribution terms are recorded in its fixture
README. No system font, giant atlas, font subsetter or added game font is used.
CJK, color emoji, vertical text, variable features, cross-family fallback,
dictionary/hyphenation and discretionary soft-hyphen painting are outside this
slice. They require a declared corpus/font/budget review and, where appropriate,
additional algorithms or renderer contracts; none has an inert authored field.
CJK needs licensed face/page budgets and a line-tailoring corpus; color glyphs
need raster/compositing formats, vertical text needs vertical metrics/orientation,
and variations need axis-keyed matching HB/FT instances before those claims expand.

`Hazel/Text/TextLayout` exposes thread-affine `Text::Service`, immutable `Face` and
immutable published `Layout` through existing Ref/Scope conventions. Each accepted
`Font` owns its exact-byte CPU face, shared through `GetCPUFace()`; family/style
resolution remains ProjectAssets' existing accepted-version path. No second font
or atlas cache exists. CPU layout results pin CPU faces, never Font/atlas/GPU refs,
and can survive scene/project/renderer retirement. Live CPU input admission is
separate from atlas admission. A failed replacement still keeps the prior accepted
Font/CPU face; metadata/source freshness remains explicit, outside drawing loops.
CPU admission first evicts reusable entries from the existing Font LRU; if live
CPU result/Font pins still prevent admission it rejects with ResourcePressure.
There is no retry of rejected files on every frame, or second owning CPU LRU.
Accepted resources carrying variable/color tables can continue on the unchanged
renderer path, but Arrange explicitly rejects those unimplemented CPU behaviors.

Service creation/calls and final Ref release belong to the service's owner thread;
`Service::Main()` is the renderer/main-thread instance. Wrong-thread calls reject.
Independent services have separate caller-owned budgets. Sheen's process-default
allocator is scoped under an adapter mutex, all its objects die before the old
allocator is restored, and per-call allocation is checked. Other engine code must
use this adapter rather than concurrent direct Sheen calls. HB buffers/plans are
per operation; plans are uncached, so language/feature combinations cannot build
an unbounded retained shape-plan cache. Language tags are the finite tested set
`und/en/fr/de/el/ru/ar/he`; no arbitrary HB language interning loop exists.

Strict UTF-8 reuses Hazel's decoder: leading BOM ignored, embedded NUL/overlong/
surrogate/out-of-range/truncated sequences reject. Each scalar and terminal
boundary retain original byte and C# UTF-16 offsets, including skipped leading BOM
units. No boundary is invented inside a UTF-8 encoding or surrogate pair. Extended
grapheme and permitted/mandatory line boundaries are distinct from HB glyph
clusters. Source remains in logical order. Glyph clusters are scalar half-open
ranges, with possibly many scalars/glyphs per cluster; byte/UTF16 ranges follow the
boundary map. Actual caret/ligature caret positions and selection widgets follow
later, not an assumption that one glyph is one editable character.

Paragraphs retain source/separator ranges and resolved base levels. Sheen line
runs apply whitespace reset and visual reordering, then intersect indexed script
runs. There is one explicitly selected complete face per call, no per-code-point
fallback. Script/direction cuts through an extended grapheme reject. Glyph IDs,
advances, offsets, source ranges and visual run positions are backend-independent.
Coordinates are logical units with +X right/+Y down, `EmSize` logical units/em;
glyph X/Y is its pen/baseline and offsets are separate. Line height/baseline use
that accepted OT face's horizontal extents. RTL glyphs/runs remain in visual
left-to-right order, while source/ranges retain logical identities.
Width is shaped advance, not a tight ink box; overhang bounds require the
glyph-index geometry integration that follows.

Absent Width is unwrapped; finite nonnegative Width is definite (including zero).
Greedy candidates are permitted UAX14/grapheme boundaries and are **reshaped with
the chosen line's context**, never cut out of paragraph-shaped geometry. If the
first unbroken item cannot fit, explicit emergency grapheme wrapping overrides
UAX14 opportunities; it is marked separately from permitted breaks. An oversized
single grapheme retains its advance and reports Overflow, never truncates/scales.
Following emergency fragments avoid reshaping the whole remaining long word.
Trailing space advances are retained; there is no trim/justification/hyphenation.
Tabs have four-space advance (not tab stops); CRLF is one paragraph separator,
line separators break, interior FEFF remains zero-width through HB. Empty input
has one empty caret line; trailing paragraph separators add an empty line.

### Budgets and failure policy

Defaults may be lowered for private test services, never silently raised:

| Resource | Limit / accounting boundary |
| --- | --- |
| Input | 64 KiB UTF-8, 16,384 decoded scalars; static face input remains ≤4 MiB |
| Published result | 4,096 lines/runs, 65,536 glyphs; retained source/vector capacity accounted |
| Contextual work | 32,768 actual script/bidi shaping calls, 1,048,576 cumulative candidate-line scalars per Arrange |
| HB work buffers | compiled fresh-buffer maximum 65,536 glyph entries / 1,048,576 operations; fail rather than publish incomplete shaping |
| Sheen workspace | 8 MiB per Arrange through checked allocate/reallocate/free; scratch pool disabled |
| Retained CPU faces | 24 / 32 MiB of source/vector/identity/engine-object storage per service, including external pins |
| Retained CPU results | 8,192 / 16 MiB per service, including externally pinned results; count allows two 4,096-element UI generations, subject to bytes; no automatic result cache |

Identity strings are bounded to 4096 bytes, language to the finite tested set;
glyph/run/line growth is checked before publication. Script intersections use
indexed ranges. Temporary candidate/output vectors are bounded by the same glyph/
run/input caps but are separate from retained-result bytes; at most accepted and
trial candidates plus current output coexist. HB's sanitized tables/lazy OT data,
dependency parsing/plans, allocator/control-block overhead and worker stacks are
not an exact total in the input/result counters. Input/output/work caps constrain
exposure; they do not establish a hard process-memory or frame-time limit. The
existing atlas/cache/generator/prepared-geometry caps are unchanged and separate.

Malformed text/faces, unknown scripts/languages, missing glyph ID zero,
unsupported features, wrong thread and exhausted caps throw bounded actionable
errors. No partial Layout is published, accepted resources are immutable, and a
caller can retain its previous result. Source-missing coverage is not quietly
reported as successful replacement shaping. GPU generation/scene dispatch is not
part of this service. Runtime fallback and author-visible family/coverage status
remain unchanged until actual renderer integration.

### Next bounded integration

1. Add immutable glyph-ID atlas pages backed by accepted `Text::Face` bytes.
   Pinned APIs are `FontGeometry::loadGlyphset(..., enableKerning=false)`,
   `GlyphGeometry::load(FontHandle*, geometryScale, GlyphIndex)`,
   `FontGeometry::getGlyph(GlyphIndex)` and `msdfgen::loadGlyph(..., GlyphIndex)`.
   Disable MSDF pair kerning: HB already supplies positioned advances. Use one
   accepted byte resource for FT outline extraction and HB metrics; never reopen
   a mutable pathname for an old layout's page.
2. Separate renderer-owned page admission/uploads from CPU face/result ownership.
   Change ProjectAssets' accepted family records to CPU-face-first resolution:
   `FamilyFont` currently requires constructing a Latin-1 atlas. Preserve its
   style/status/reload/failure contracts while replacing that coupling, keeping
   CPU → GPU ownership absent and one reusable face identity/cache policy.
   Bound page bytes/glyphs/pending generation, prewarm this corpus and pin page/UV
   versions through prepared geometry and ordered draw runs. No live atlas UV
   replacement. Define missing-page behavior before dynamic requests; do not
   smuggle cross-family fallback into per-code-point page generation.
3. Switch `Renderer2D::MeasureText/LayoutText/PrepareString/DrawString` and
   `RuntimeUIMeasure` to the same immutable result, converting the current
   font-height-based size/spacing into explicit em metrics. Keep layout/paint
   native-write keys and family epochs authoritative. Then validate glyph-ID page
   coverage, compositor/picking/order and old examples at declared viewports,
   update Hazelnut/export guarantees only when those rendered checks pass.

### Local qualification (2026-10-21)

Implementation checkpoint **`f838084`**, with the final cluster-error wording,
passed bounded Debug/Release Premake/GNU Make builds with two compiler jobs and
cached existing dependencies. New Debug library targets built from empty outputs
with a compiler/basic-shell-only PATH: no Python, Meson, CMake, Ninja, Autotools,
gperf, pkg-config, Git or curl available to their build. No generated input was
missing. Windows `--os=windows --migration-tests --shader-tools vs2022` generation
passed; source lists, C11/C++17, static archives, /MDd-/MD and consumer project
references were inspected. This is not an MSVC or macOS build result. Both actual
ELF HB objects have hidden public symbols and Nutella exports no `hb_*` symbols.

All **17 regression executables per configuration** passed once after integration
stabilized: Release physical HD4000 forced GL4.1/GLSL410 and Debug physical native
GL4.2. Grandpa is i3-3227U/8 GB, GCC 16.2.1, kernel 7.2.7-1-cachyos, Intel HD4000
Mesa 26.2.4-arch3.1. Active logind session is Plasma Wayland; GLFW uses X11/GLX
through Xwayland `:0`. No Xvfb/software or native Wayland qualification is claimed.
Selected late-edited objects and new Release libraries were explicitly rebuilt:
GNU Make does not track changed compiler flags automatically. Earlier failed
development builds/checks are not counted as passes.

New CPU checks pass **766 GraphemeBreakTest, 19,338 LineBreakTest and 91,707
BidiCharacterTest Unicode 17 cases**, without skips, and all 12 authored corpus
rows. Line tests use strict UAX14 behavior and map the library's indeterminate
complete-input EOF to a boundary; bidi tests compare per-line L1-reset levels and
L2 ordering. This qualifies the pinned dependency algorithms, not every script in
Hazel's narrower selected-script/shaping policy. Service references independently
check cmap presentation forms for Arabic initial/final and lam-alef joining,
Latin ffi, composed/decomposed equivalence, multiple glyphs per combining cluster,
RTL punctuation mirroring, mixed Hebrew/Latin/numbers source order, em scaling,
byte/UTF16 maps, paragraph/empty/zero-width handling and contextual Arabic
reshaping at emergency line boundaries. Repeated corpus positions/ranges are
identical. Missing known-script coverage and unsupported scripts remain distinct
failures. Limits, failed publication/recovery, owner-thread rejection, noncopyable
resource-accounted results and CPU pins surviving service retirement are checked.

Real accepted Font checks cover shared source identity, changed/failed reload,
CPU layout surviving renderer shutdown without pinning GPU resources, repeated
Application contexts and zero retained CPU owners afterward. One private padded
4 MiB font file creates seven pinned immutable input versions (under 32 MiB total)
with ordinary small Latin-1 atlases; the next version forces reusable eviction,
rejects before raster generation, and recovers when a CPU pin is released. Private
services also lower count/byte/work/workspace caps. No enormous atlas/RAM-exhaustion
test is used. Existing D8 analytic pixels, ordered batches/clips/picking, family/
history/prefab/native mutation, expanded Button Inspector and Play/Stop checks pass.
Native ImGui harness results do not claim physical dialog/pointer usability.

The normal package pipeline exported a fresh Release Nutella/SceneTransitions
archive. Extraction in an unrelated Unicode directory passed `--help` and
`--list-projects`; all three upstream notices and Unicode notice matched their
source bytes, no test font/corpus leaked, and private HB symbols were absent from
dynamic exports. An isolated missing-notice source root rejects packaging.
This is new dependency/notice/CLI closure evidence, not another rendered relocated
game acceptance. Unchanged shader/painter paths and example content reuse the
preceding GPU/SPIR-V/source-game/package evidence; no whole-frame or new warm
performance result is inferred. Font creation now retains CPU source data; current
draw/measure loops do not call the new layouter or probe files per frame.

Master, stash, existing vendor pins, settings and exact Sandbox draft remain
unchanged. CI is triggered by pushes and **not awaited or polled**. Remaining
acceptance is Windows compilation/runtime and competent-reader review when the
declared corpus is actually rendered by the next integration. No new language
support is advertised in Hazelnut/export. This foundation completes this bounded
slice, not all of P3 or the production UI milestone.

## P3: shared rendered typography and immutable glyph-ID pages

Continues `2c3166a`; the preceding CPU dependency/conformance review remains in
force. World text, UI intrinsic measurement/wrapping, prepared geometry and family
previews use `Text::Layout`. There is no Latin-1 resolver or renderer switch.
Component/family schemas, managed family/style APIs and authored assets are
unchanged. CPU source/layout owners never own a texture or require a context.

### Ownership, preparation and drawing

`Font` accepts immutable CPU bytes/metrics before any GPU generation. Its existing
bounded identity/LRU and ProjectAssets' accepted-family records remain the only
owning face cache. Page construction opens **accepted bytes**, never the current
pathname. `FontGeometry::loadGlyphset(font, 1, ids, true, false)` uses the pinned
`GlyphGeometry::load(..., GlyphIndex)` / `msdfgen::loadGlyph(..., GlyphIndex)` path;
lookup/UVs are keyed by glyph IDs. MSDF pair kerning is disabled: HarfBuzz already
positions glyphs, including substitutions without a source character mapping.

A page contains at most 128 glyph IDs. A CPU-shaped printable-ASCII cohort
keeps common labels together; other requests use deterministic 128-ID blocks
(last block may be shorter). The optional cohort is packing only, not a separate
renderer/coverage path, and is omitted if it exceeds its work cap. It is immutable after completed generation/upload; no incremental
repack invalidates UVs. Whitespace advances come from the CPU result without a
quad/page request. Prepared glyphs pin their exact page/CPU face. Ordered runs
pin textures until submission, including when a page object is evicted. Weak
records account for those texture-only pins. Trim/project retirement/shutdown
release reusable ownership without invalidating active owners. All generation,
uploads, draw preparation and final GPU-owner release remain renderer-thread
bound; CPU results can survive renderer/application retirement independently.

One synchronous preparation runs at a time, with two existing MSDF workers.
Graphical application loading with a nonzero viewport prewarms startup text and attempts declared family corpora with existing
progress feedback. New contextual glyphs may prepare on first changed-text/wrap
use. No background scheduler, filesystem polling or per-frame regeneration is
introduced. A weak CPU-source identity index reuses accepted bytes even after the Font wrapper
LRU retires while pages/results remain pinned; it owns no face. Per-source cohort
metadata is bounded to 128 IDs. One pass-local last-layout page lookup avoids
rescanning a paragraph for every line. Two reusable ID work vectors request at
most 512 KiB in total and retire on renderer shutdown. Font layouts retain a
bounded 128-result / 256 KiB LRU per live Font,
subject to the shared CPU service's 16 MiB live-result admission. UI keeps natural/
constrained/paint results; world components pin their current CPU result. Exact
native text/spacing/selection/asset-owner/epoch checks remain authoritative.
Paint-only position/tint changes do not reshape; layout changes cannot hide behind
a snapshot generation. Copy/Stop clear component CPU caches and font pins; history
stores authored values only.

UI lines retain the full CPU result and line index, with source byte/UTF16/cluster
maps intact. Painting does not reshape substrings. Plane bounds are in em units,
scaled by `1 / Face::HeightEm()` into the established font-height units, then by
UI size/canvas scale or world transform. CPU +Y-down offsets become renderer
+Y-up; first world baseline is zero, UI top alignment adds the accepted ascender.
Line advance is accepted horizontal extents plus authored LineSpacing. Content
height remains one font height plus subsequent line advances; advance width is
not tight ink width and overhang can clip. `Kerning` is extra tracking once per
complete HB cluster (including its trailing advance), never an additional MSDF
pair adjustment; it is suppressed within Arabic joins except whitespace.
HarfBuzz's standard kerning/substitution/mark positioning remain enabled.

Horizontal Start/End alignment remains physical left/right; there is no automatic
RTL mirroring of canvas/containers.

D8 straight-tint RGB / coverage × opacity alpha, source-over text alpha, derivative
ordering, painter runs, per-vertex clips and picking are unchanged. A run changes
texture for each page/font boundary; no translucent sorting or shader merger is
introduced. Existing capacity/texture rollover and state restoration still apply.

### Limits and failure behavior

- Face input ≤4 MiB; CPU face/results/work bounds remain those above.
- Each page ≤128 IDs, 1024² / 3 MiB RGB8; all live page/texture records ≤24 / 24 MiB.
  Reusable page ownership ≤12 MiB / 24 pages; the reusable face LRU remains eight.
  Active/prepared/pending-draw pins count against live admission.
- Generator estimate ≤32 MiB: source bytes, RGB8 atlas and existing two-worker
  pixel/error workspaces. Page outlines ≤32,768 edges, checked after dependency
  parsing. FT/parser/outline intermediates, HB lazy tables, allocator/driver
  overhead and worker stacks are not a hard total-process memory bound.
- Requests are bounded by 65,536 layout glyphs; preparation deduplicates IDs and
  only admits complete pages. Per-scene prepared glyph retention stays ≤16 MiB,
  now reserved from shaped glyph count, not source byte count. Uncached scratch
  is bounded by CPU output limits and separately retained vector capacity.
- Failed replacement keeps accepted metrics/bytes/pages. Missing source glyphs
  explicitly publish `.notdef` status in renderer layouts (strict CPU Arrange
  still rejects by default). No per-code-point cross-family fallback occurs.
  Empty `.notdef` uses a visible placeholder. Unsupported scripts/features and
  exhausted CPU-work/result limits reject rather than publish partial layout.
- Failed page admission/generation preserves CPU advances/offsets and unaffected
  text, paints magenta metric placeholders and reports once per accepted face /
  explicit retry. Reload retries; Trim permits retry at the next preparation.
  Completed pages remain valid. Prepared placeholders refresh on the normal
  family reload/asset epoch, text/layout invalidation or explicit preparation.

Coverage inspection now runs CPU source/shaping checks without GPU construction.
Required declarations block export on missing source/unsupported behavior; Sample
is informational. It does not certify live page admission, reader correctness or
arbitrary language/font features. Runtime/preview diagnostics separately expose
missing source warnings, page availability/failure, cache/live/pinned bytes and
layout reuse. Project face/notice closure and test-only fixture separation remain
unchanged. Tested rendering scope is the pinned Latin/Greek/Cyrillic/Arabic/Hebrew
corpus, not CJK/color emoji/vertical/variable fonts, general family fallback,
soft-hyphen painting, rich text or editing/IME.

Headless scene/script lifecycle does not prepare GPU resources. Custom renderer hosts can prepare pages explicitly or on first drawing.

### Release checkpoint

Release Premake/GNU Make with two compiler jobs passes all 17 regression
executables on physical HD4000 forced GL4.1 through Xwayland (Plasma Wayland).
The llvmpipe GL4.6/SPIR-V and explicit two-slot/GLSL alternatives also pass the
renderer/corpus/compositor assertions. Native ImGui checks import the licensed
fixture, declare coverage, save a mixed-script scene/prefab and render the actual
family preview; they do not certify physical dialog usability or competent-reader
language acceptance. Headless scene/domain tests, failed-measurement recovery,
accepted-byte reload/page pins and all prior history/input regressions pass.

The initial fixed-ID-only packing increased the 64-element mixed fixture from
65 to 97 draws; the measured refinement groups CPU-shaped printable ASCII into
one immutable cohort page and restores 65. Every glyph still uses the same CPU
layout/glyph-ID page path. Other page/font boundaries remain ordered draws.
Debug, source-game and relocated-package qualification are recorded below.

### Measured Release costs (2026-10-21)

Grandpa i3-3227U/8 GB, GCC 16.2.1, HD4000/Mesa 26.2.4-arch3.1, physical
GL4.1/GLSL410 through Xwayland on Plasma Wayland. No compiler was active before
or after either measured run; a concurrent unrelated package build earlier in
development was left alone and its timings are not qualification. No desktop
package/settings, vendor or dependency pin was changed by this work.

Existing `--profile-render-small`: 1280×720 RGBA8/ID, 20 warm-up / 120 samples;
median/p95 ms. These are UI boundary + paint costs, excluding input, gameplay,
world draws and presentation. Timestamp intervals are asynchronously collected,
can include CPU feeding gaps and are not GPU busy time or additive to CPU.

| 64 unchanged elements | Source | Layout | Prepare | CPU submit | Boundary + paint | GPU interval | Draws |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Images | .0667/.1184 | 0/0 | .0900/.2043 | .0168/2.2479 | .1833/2.4899 | .0409/.0726 | 1 |
| Mixed (32 labels) | .1531/.2216 | 0/0 | .2864/.4494 | .6465/1.0499 | 1.1056/1.7339 | .5719/3.5483 | 65 |

Mixed labels reuse all 32 prepared entries (6 KiB, up from 4 KiB because glyphs
now pin page versions). Both unchanged cases record 120 ordinary C++ new calls
across 120 frames. Moving a rect: total .3152/.5177 images and 1.3631/2.1184 mixed.
Changing paint/one label: .1812/.2735 and 1.0220/1.3449, 240/840 allocations over
120 frames; mixed rebuilds exactly one label. One upload/end flush per frame;
primitive/page transitions remain ordered runs. Ready GPU counts are 110–119,
zero query-ring saturation skips. Relative to the previous recorded unchanged
.1994/2.3887 images and .6489/1.0484 mixed, this is **not a warm CPU speedup**.
Tail variation is visible and the proposed small-UI p95 budget is not established.
No 512/2048 rendering campaign was repeated or large-run budget claimed.

New `MigrationRenderer2DSmoke --profile-typography` uses the exact 12-row pinned
corpus as one 24-pixel world text block on a 1280×720 RGBA8/ID target. Cold CPU:
accepted face load **3.440 ms**, shaping **.517 ms**, page/geometry preparation
**5301 ms** including upload enqueue, not GPU completion. Ten live pages estimate
**7,274,496 bytes**; 296 ink quads. Unused IDs in a requested block contribute to
cold work/storage; loading feedback is important. No new test font is shipped
with ordinary games.

| Warm corpus, 20 warm-up / 120 samples | CPU prepare | CPU submit | GPU interval | Ready samples / draws |
| --- | --- | --- | --- | --- |
| Cached glyph paint (UI equivalent) | .0001/.0002 | .1937/3.4530 | .1668/.1788 | 70 / 65 |
| World geometry preparation each frame | .1057/.1673 | .2043/.4260 | .1673/.1781 | 64 / 65 |

Both have **zero new shaping or page generation**. The optional probe uses the
existing nonblocking GPUTimer; latest-result collection can discard intermediate
ready samples. The cached-paint CPU tail is not concealed or interpreted as a
universal GPU cost. A single post-measurement capture was inspected internally:
accents, Greek/Cyrillic, joined Arabic and mixed Hebrew/Arabic/Latin/numbers are
present. Automated IDs/bounds/pixels plus this inspection are not competent-reader
acceptance. Page alternation within a font can still add many ordered draws;
ASCII grouping avoids the measured ordinary-label regression, not that general
limit. World geometry preparation and changed-text/changed-layout allocations
remain costs. The known large alternating image/text submission debt is separate.

### Final local qualification and remaining acceptance

Implementation checkpoint **`7725874d566225e2e71f2a05e0454984dd0e98c4`** passes
bounded Premake/GNU Make Debug/Release builds with two compiler jobs. After
stabilization, **all 17 regression executables per configuration pass**: Release
physical HD4000 GL4.1 and Debug physical native GL4.2. Release's llvmpipe GL4.6
SPIR-V specialization and explicit two-slot/GLSL alternatives also pass. This is
X11/GLX through Xwayland, not native Wayland. No MSVC/macOS build or physical
Windows language/DPI acceptance is claimed; no new platform-specific production
code or dependency is introduced. CI is **not awaited or polled**.

Checks include all 12 rendered corpus rows against CPU positions/source maps,
composed/decomposed accents, ffi and multi-glyph clusters/offsets, Arabic line
context, mixed bidi/numbers, multiple pages/fonts/capacity, empty outlines,
placeholder pixels and picking, immutable accepted bytes after file corruption,
weak source/page reuse after Font LRU retirement, lowered admission/work limits,
active page and texture-only draw pins, reload recovery and repeated contexts.
D8 independent analytic RGBA8/window pixels, adversarial clips/painter order,
resource/native-write invalidation, scene/prefab history, repeated Play/Stop,
expanded Button Inspector and headless domain lifecycle remain passing. The
rejected-measurement regression restores previous authored text/bounds without
leaving empty lines under a valid cache key. Private fixture setup now copies its
two pinned font/notice inputs into the editor's isolated working directory.

Release source-example checks pass all five projects in Nutella and Hazelnut:
startup/render/resize/graceful close and authored-file hashes unchanged. Existing
managed/native gameplay, Lightkeeper LK1/LK2 migration, LK1–LK4 progress/retention,
chapter transitions and lifecycle assertions pass. Fresh Lightkeeper menu, HUD
and Mara dialogue captures were inspected internally at supported fixture sizes;
no authored appearance correction was needed. Gameplay, saves and example assets
are unchanged.

A fresh normal Nutella export of the native-authored corpus scene/family/prefab
passes startup/render/resize/shutdown from an unrelated Unicode directory while
source resources/examples/SDK and GAC are unavailable. The imported DejaVu face,
complete associated notice, descriptor and scene are byte-exact; four engine faces/
notice and linked dependency notices are checked. Test corpus/conformance inputs
and the unimported test-font collection are absent. Post-extraction missing and
corrupt project-face cases render the established engine fallback with explicit
missing-source status, retain authored references and close cleanly. Fallback does
not acquire Arabic/Hebrew glyph coverage: `.notdef` remains an honest failure
indicator. The healthy mixed-script package capture was inspected internally.

Master remains `f50c5559`; stash, existing vendor pins/settings/layouts and the
exact unrelated Sandbox draft remain unchanged. Only implementation/tests and
existing guide/record changed; temporary captures, profiles and private exports
remain ignored.

Manual acceptance still needed:

1. In a disposable project, import a complete licensed face/notice, create a family
   and inspect Required versus Sample. Preview the pinned accent/Greek/Cyrillic/
   Arabic/Hebrew examples; have competent readers review joining, punctuation and
   mixed-direction wrapping. Automated positions/pixels are not reader acceptance.
2. Assign it to Content-height UI text and world text; narrow/widen the viewport,
   try physical left/center/right alignment, style fallback, tracking and spacing.
   Undo/Redo, save/reopen scene/prefab and Play/Stop. Check actual desktop focus/DPI
   transitions and repeat on Windows hardware.
3. Corrupt a replacement while a screen is live, Reload and confirm the accepted
   face remains; repair/reload and confirm wrapping and paint change together.
   Inspect cache/live/pinned/page diagnostics. Initial page generation can pause;
   warm unchanged text must not repeatedly prepare.

Remaining P3: declared-reader/platform qualification, explicit overflow/cluster-
safe ellipsis and later bounded spans/locale preview. Cross-family fallback is
still deferred and must respect grapheme/joining runs; CJK, color/vertical/variable
fonts, editable text/IME and localization databases remain excluded. This does
not complete P3 or make the UI production-ready. **Smallest next slice:** explicit
Clip/Ellipsis overflow and MaxLines, reshaping at the chosen truncation boundary,
using these shared results/pages, with native preview/history and mixed-bidi
regressions. Keep scrolling/new controls and the large alternating-run performance
work separate.

## P3: explicit text overflow and MaxLines

Continues `51e1f97` / `7725874`; implementation checkpoint `1dd4460`. One shared
CPU service remains authoritative for world/UI text, metrics and immutable
accepted-face glyph pages. No dependency, font asset, shader/blend convention,
layout schema generation, game content or renderer mode is added.

### Presentation and source contract

`Text::Options` adds independent Wrap, optional Height, Clip/Ellipsis and MaxLines;
UI authors the last two in `UITextStyleComponent`. Defaults are Clip / 0
(unlimited within existing line limits), preserving current documents. Width is a
constraint independently of Wrap. Clip leaves height to the caller's pixel clip;
positive MaxLines removes whole subsequent lines in either mode. Clip retains
the established border-box/inherited clip; padding alone does not add a scissor.
Ellipsis also
limits to complete font-height boxes: first box is accepted ascender–descender
height, then accepted line advances including authored spacing. Padding supplies
the inner UI box; ancestor clipping does not reflow text. A four-float-ULP height
edge tolerance absorbs normalization/arrangement rounding; zero stays empty. Width
fitting is strict. Zero/insufficient height
has no visible line. If U+2026 alone cannot fit width, the affected line is blank,
with MarkerDoesNotFit; a missing complete line has the same explicit status.

Ellipsis marks actual omitted content, including omitted explicit/trailing empty
lines. Fitting text gets no marker; empty source has no omitted content. Each
horizontal overflow line replaces a logical source tail; vertical/MaxLines
omission marks the last retained line. U+2026 is shaped **with** its retained
logical line, rerunning script/bidi/contextual processing with the original
paragraph base direction. Arabic final forms, ligatures/combining clusters and
neutral punctuation follow that final context. RTL's marker is at its visual
trailing edge; physical left/center/right UI alignment is unchanged.

Only original extended-grapheme boundaries are candidates. Up to 128 boundaries
are tested longest-first; longer lines use an advance-estimated ±48-boundary
neighborhood, geometrically decreasing checkpoints and the empty prefix, at most
128 candidates. Every admitted candidate is fully reshaped and measured. Prefix
width monotonicity is never assumed. ConservativeTruncation explicitly reports
that a long line may omit more than the mathematically longest fitting prefix.
The existing 1,048,576 cumulative shape-scalar / 32,768 dependency-call caps cover
original layout, marker and all trials together. Exceeding work/output/input
limits rejects publication rather than returning partial success. Unsupported
font rules merging generated and authored content into one cluster also reject.

Layout Source/Scalars/Boundaries/Paragraphs retain the complete original logical
input and byte/UTF-16 mapping, including BOM offsets. Retained glyph clusters
remain original scalar ranges. Generated glyphs set Generated and both cluster
indices to UINT32_MAX; runs flag ContainsGenerated and retain only authored
ranges. Lines expose retained End/Ellipsized, and layouts OriginalLines/Omitted,
conservative/fit status and MissingMarkerGlyphs. No generated character acquires
an invented authored offset. Interior FEFF at a trial's first scalar retains its
format semantics instead of becoming a newly ignored document BOM.

U+2026 uses the same accepted face. Strict CPU calls reject absent marker coverage
with marker context; renderer layouts explicitly retain `.notdef` and its missing
marker count. Empty `.notdef`/unavailable pages use the existing visible metric
placeholder. No cross-family fallback is added. Glyph ink/placeholder overhang is
clipped to UI's padded content box using the existing inward framebuffer coverage
helper, while images retain their own clip. Painter runs capture that clip before
later submissions; no mutable-scissor ordering boundary is introduced.

### Measurement, lifetime and native editing

Content measures the width-constrained result and MaxLines without constraining
it by its own future height. Natural Content width stays indefinite; Fixed/Fill or
an enabled Content maximum supplies definite width as before. Final paint uses
arranged padded width/height, so a constrained height may remove additional lines
without feeding back into its intrinsic size. UI keeps its existing one-font-
height plus subsequent line-advance sizing; CPU Height remains line advances.
Measurement and paint use the same final-layout service/accepted face, with their
explicitly different intrinsic/final constraints. No derived text rewrites source.

Font's existing bounded result LRU keys width, wrapping, spacing, overflow,
MaxLines and relevant height. Scene-owned measurement/paint keys and exact native
Content dependency bytes include the authored fields. A zero-line final result
has its own retained immutable handle and reuses normally. Publication assigns
keys only after success; discarded geometry is compacted when its capacity is
large, retaining original source maps. CPU trial buffers remain separately bounded
temporary storage, not a claimed hard process-memory cap. Page/live-pin/prepared
budgets are unchanged; only final glyphs request pages. Graphical world loading
keeps its prewarm; UI no longer prewarms an unconstrained surrogate of its source
before painting. Final constrained UI preparation may still pause on first use.

Native **UI Text Style** exposes Overflow and Max lines (0 unlimited) beside Wrap
and alignment. Shared scene/prefab Inspector history, exact dirty comparisons,
serialization, copy/duplicate and prefab paths include both fields. Invalid edits
keep valid values; Escape/cancel and Undo/Redo keep the full string. Managed
`UITextStyleComponent.Overflow`, `MaxLines` and `SetOverflow(mode, lines=0)` validate
Clip/Ellipsis and 0–4096, using the established live-entity/thread guard. Play
copies stay isolated. Runtime UI Debugger reads the last completed paint result;
line/omission/marker/search status is transient, with no reshaping or action
side effects from inspection.

### Local qualification and measured limits (2026-10-22)

Production checkpoint **`93d6df0bea5a14fbe579160fb489dc801c93899b`** builds with
Premake/GNU Make, two compiler jobs. All **17 regression executables** pass in
Debug physical/native GL4.2 and final Release physical/forced GL4.1. The earlier
Release overflow renderer checks also pass on llvmpipe GL4.6/SPIR-V and explicit
two-slot GLSL; the final CPU-only negative-constraint guard does not change those
valid rendering cases. Grandpa: i3-3227U/8 GB, GCC 16.2.1, Intel HD4000/Mesa
26.2.4-arch3.1. Plasma Wayland hosts GLFW X11/GLX through Xwayland, not native
Wayland or Xvfb. No Windows hardware/MSVC or macOS result is inferred. CI is
**not awaited or polled**.

New CPU checks independently reference cmap advances at fit boundaries, retained
ASCII/grapheme ends, Arabic final forms and RTL marker placement. They cover
Clip MaxLines, finite height/spacing and normalized-height rounding, marker-only/
too-small/empty boxes, CRLF/U+2028/trailing empty lines, accents, ffi, mixed bidi,
BOM/byte/UTF-16 mappings and generated sentinels. A private in-memory fixture cmap
removes U+2026: strict layout rejects; permitted rendering reports generated
`.notdef`. Long 4096-character input exercises conservative bounded selection;
lowered work limits reject without publication. A targeted C++ allocation failure
also proves `Renderer2D::LayoutText` preserves its old complete line view; view
publication now swaps only after every entry succeeds.

Rendered checks compare final contextual glyph IDs/positions with the complete
retained string, exercise physical left/center/right, translucent ordering,
subsequent empty clips, padded ink/picking and marker-page admission failure.
Real scenes verify native mutations with reused Fixed-box generations, Content
reflow, zero-line reuse, scene/copy/duplicate/prefab and full-source round trips.
The Unicode 17 conformance/corpus, D8 analytic pixels, capacity, reload/resource
retirement, domain lifecycle, history and expanded Button Inspector checks remain
passing.

The maintained native fixture creates three useful labels through native commands
and shared Inspector transactions: responsive unwrapped Ellipsis, Fixed-width
mixed-direction wrapped Ellipsis/MaxLines=2, and Clip/MaxLines=2 with explicit
newlines. It explicitly selects sizing ownership and asserts the paragraph's
actual 300-unit width, two lines and omission, rather than merely assigning an
inactive FixedSize value to Content sizing. Actual ImGui properties/scene preview,
resize, Undo/Redo, scene/prefab save/reopen and repeated Play/Stop are exercised.
After the final fixture-only sizing/assertion correction, affected EditorSmoke
passes again in both configurations and the normal export is rechecked. These
native harness checks do not certify physical pointer/dialog usability.

Release source checks pass all five games in Nutella and Hazelnut on physical
native GL4.2, including startup/render/resize/close and unchanged authored hashes.
Existing native/managed gameplay, Lightkeeper progress/save migration and chapter
checks pass. Fresh Lightkeeper menu, HUD and Mara dialogue captures were inspected;
no authored correction was needed. A normal export of the native fixture renders
from an unrelated Unicode directory on physical GL4.1 with source resources,
examples, SDK and GAC unavailable. Scene/prefab, project faces/notices and engine/
dependency closure are checked; the test collection is absent. Missing/corrupt
project-face cases retain authored references and render the established fallback.

Frozen-code Release measurements use a 1280×720 RGBA8/ID target with no concurrent
compiler/test. The new `--profile-overflow` fixture has 121 UTF-8 bytes, width 8
font-height units, Wrap/Ellipsis, MaxLines=2, height 2.2, spacing .1 and 24-pixel
text. Accepted-face load/ASCII planning is outside its cold timer. Cold CPU layout
and line views: **1.277 ms**, 52 shaping calls / 370 cumulative scalars. First
page/geometry preparation: **1940 ms**, including upload enqueue, not GPU
completion; 24 ink quads. This pause remains a practical limit.

| Overflow fixture | Median / p95 ms | Scope |
| --- | --- | --- |
| Changed source + width | .7244 / 1.1410 | 100 samples after 20; CPU layout/LRU, no page generation |
| Warm lookup | .0008 / .0021 | 120 after 20; zero new shaping or pages |
| Cached glyph CPU submission | .0598 / 2.4122 | Eight ordered draws; no presentation |
| GPU command interval | .0195 / .0443 | 53 ready nonblocking samples; no synchronous waits |

The existing 64-element guard uses 20 warm-up / 120 samples. Unchanged images:
CPU boundary+paint **.1717/2.4084 ms**, GPU interval **.0401/.0618**, one draw.
Mixed (32 labels): **1.1451/1.5269**, GPU **.5702/3.6619**, 65 draws; all 32
prepared labels reuse (6 KiB), zero rebuilds. Both count 120 ordinary C++ new calls
across 120 frames. Mixed move is 1.2563/1.9761; changing paint/one label is
.8118/1.2356 with one label rebuild and 480 new calls total. One End flush;
no capacity/texture flush in this guard (separate regressions force them).
These exclude input, gameplay, world rendering and presentation; GPU intervals
can include CPU-feeding gaps and are not additive busy time. Scheduling tails
vary; no universal budget or speedup is claimed. No large-fixture campaign was
repeated. Large alternating-run submission, changed-layout allocations, trial
shaping and cold synchronous page preparation remain costs.

Manual acceptance:

1. In a disposable scene/prefab, select a complete licensed family, Fixed/Fill
   width and Ellipsis/MaxLines=2. Resize, change padding/spacing, try Clip and 0,
   and confirm full Source survives Undo/Redo, save/reopen and Play/Stop.
2. Check marker-only/too-small boxes and the debugger's omitted/fit/search status.
   Have competent readers review Arabic joining, Hebrew/Arabic/Latin punctuation
   and truncated lines; numeric/capture checks do not replace that review.
3. Repeat physical Windows/DPI/focus/detached-viewport checks and a normal export.

This completes bounded overflow implementation, not all P3 or production UI.
Existing script/font exclusions and cross-family fallback remain. Rich spans,
locale preview and editing are separate work. **Smallest next endpoint slice:**
remappable device-neutral UI actions with scoped directional/gamepad focus (P4),
before composing more controls or scrolling/data views. Retain the measured
rendering and cold-preparation limits in that work; do not infer large-list budgets
from the small guard.

## P4: device-neutral actions and scoped spatial focus

Continues `bc33f35`. `UIInputAdapter` maps bounded hardware samples/ordered host
edges to Point/Press, directional Navigate, Next/Previous, Accept and Cancel.
`InputRouter` remains the single geometry/focus/activation authority. Platform
Input owns standardized gamepad sampling; common UI has no GLFW constants.
Scroll/Adjust remain future control contracts, without inactive binding fields.

Project `UIBindings` Version 1 is an optional `.hproj` section; omission selects
engine defaults. Bindings carry primary/alternate physical key codes, Shift match
and standardized gamepad button per action, plus pointer button, selected stick,
engage/neutral thresholds and repeat timings. Invalid enums/nonfinite values,
ambiguous overlapping chords and shared gamepad buttons reject the whole edit.
Next/Previous/Accept/Cancel retain keyboard alternatives. Ctrl/Alt/Super chords
are reserved for host/editor ownership. This is a UI map, not gameplay actions.

Defaults: left mouse Press; arrows spatial navigation; Tab/Shift+Tab ordered
navigation; Enter/Space and gamepad South Accept; Escape and East Cancel; D-pad
and left stick navigation. Stick engages at .55, rearms at .25; navigation repeat
starts after .35 seconds, every .10 seconds. Repeat applies only to navigation,
not Accept/Cancel. Host repeat events are ignored. Long update gaps produce at
most one repeat per sampled action, using at most .1 seconds of repeat clock.
The first standardized connected gamepad is used. Connection revisions, ownership
loss and rebinding require neutral rearming; no held connect/restore activation.
Pointer movement alone does not change focus; a fresh navigation gesture owns
keyboard/gamepad focus, and a pointer press selects its actual hit target.

Tab order remains authored TabOrder then stable painter-list rank. First eligible
control is initial focus (last for initial Previous). Directional candidates use
the center of bounds intersected with effective framebuffer clip; their center
must lie strictly in the requested half-plane. Score is forward-distance squared
plus four times perpendicular-distance squared, ties by tab rank. No candidate
retains focus; directional navigation does not wrap. Partly clipped controls use
their visible region; empty/disabled/retired targets are excluded. Active highest
modal canvas restricts the same published hit list. Nonmodal canvases share scene
scope zero. Up to 64 scene-local scope/return-focus records retain eligible previous
focus; oldest records evict. No entity/component pointer crosses a publication.

Cancel is a scoped, destructive request, coalesced once per sampled update and separate from button activation;
it cancels capture and pending activations, never fabricates a click or loads a
scene. A canvas consumes only its active scope; ordinary scene scope is zero.
Delivery reconciles native mutations just like existing button delivery. Scene,
domain, Stop, ownership loss and scope replacement retire pending records.
Pointer gestures retain release-frame ownership even with simultaneous Accept.

Runtime overrides are complete validated maps in the existing RuntimeStorage slot
`engine-ui-bindings`, under the project's SaveNamespace. Precedence is engine
initial defaults → authored project map → valid player override. A reset stores
`default` and follows future project defaults. Missing namespace rejects persistent
overrides. Editor Play uses memory only. Writes succeed before effective state
changes; corrupt user files remain untouched with one diagnostic and usable
project defaults. This does not modify project assets/editor-wide preferences.

Native Project Settings exposes the binding form, key capture/selection, gamepad
buttons, stick/dead zones/repeat and reset/revert; Save Project Settings uses the
existing staged descriptor/conflict/atomic-write path. Runtime UI Debugger shows
scope, focus/capture, device, navigation misses, dropped edges and effective Play
bindings, with isolated override/reset. Capture is editor-owned and Escape cancels.
The normal scene/prefab history is unchanged; settings forms use their existing
save/revert document workflow rather than a competing undo service.

### Local qualification

Bounded Premake/GNU Make builds used two compiler jobs. All **17 regression
executables per configuration** pass: Release physical HD4000 forced GL4.1 and
Debug physical native GL4.2, via GLFW X11/GLX through Xwayland on Plasma Wayland.
The final outside-press editor ownership guard additionally passes the affected
Release editor check; the Debug full suite includes it. No native Wayland,
physical controller, Windows/MSVC or mixed-DPI qualification is inferred.

New deterministic CPU checks cover spatial/tab order, exact ties/no candidate,
clipped eligibility, modal return focus, ordered quick clicks/Accept, cancelled
release plus Accept, opposing directions, neutral/dead-zone/disconnect/repeat,
pointer takeover of held navigation, overlapping physical bindings, modifiers,
binding round trips/conflicts and bounded edge overflow. Real RuntimeSession
checks exercise the production adapter/router with synthetic gamepad samples,
ordered host key events, once-only delivery, scoped Cancel, native retirement,
managed binding/settings access and persistent/corrupt/reset/failed-write cases.
The same suite retains domain/scene replacement and all P1 lifetime assertions.

Native ImGui authoring creates a four-button spatial menu using normal commands
and shared Inspector transactions, saves/reopens scene/prefab and project
bindings, and tests three Play/Stop cycles with remapped keyboard samples. Its
binding form, preview and expanded five-state Button Inspector are drawn by real
ImGui. These are native harness assertions, not physical input acceptance.
A pre-existing minimized-tool test incorrectly included same-frame cold preview
work in its five-second polling deadline; the deadline now starts at the first
actual minimized/idle tick, retaining the no-frame-advance/completion assertions.

Play's viewport disables ImGui navigation inputs. Keyboard ownership no longer
depends on pointer hover, and an outside press retires viewport ownership before
the next ImGui focus update. Text fields, popups, binding capture, other editor
windows, focus loss and minimization still gate input. Nonmodal ownership is
retained underneath modal scopes; closing a pointer-opened modal does not acquire
keyboard ownership. Consumed held UI inputs in the effective map keep the
established gameplay gate until neutral, including across scene transitions.
Raw native polling and game-specific `GameUI.IsKeyDown` shortcuts remain deliberate
escape hatches, not newly remapped gameplay actions.

No renderer/text/font/cache implementation changed and no new performance budget
is claimed. Prior rendering measurements exclude input and remain the applicable
rendering-only evidence. Normal binding lookup/sampling does not serialize a
document or probe project files. Binding parsing/storage happens at Start or
explicit edit/reset; idle adapter storage is bounded and reusable.

Source-example/package qualification follows below. CI is not awaited or polled.

### Final integration and remaining acceptance

Implementation checkpoint `25a1df4` is followed by ordering correction `2946e3a`.
The focused regression first reproduced a quick pointer click incorrectly
suppressing a fresh arrow edge later in the same sample. Pointer takeover now
disarms only directions already held; subsequent fresh navigation remains
ordered. Model and real RuntimeSession/scene checks pass again in Debug and
Release on physical GL4.1. The additional Play debugger test passes in both configurations: it draws effective
bindings/host gating and checks viewport NoNavInputs without changing authoring.

The existing native harness's simulated Other-widget ID now hashes the retained
window ID: calling GetID outside Begin/End was invalid after long cold-loading
frames allowed ImGui to compact the inactive window's transient stack. Vendor
source and production ID handling are unchanged. This is separate from the
expanded Button Inspector regression, which remains enabled.

Release source checks pass all five projects in Nutella and Hazelnut, including
startup/render/resize/close, authored hashes and existing native/managed gameplay,
progress/save and chapter assertions. Lightkeeper menu, HUD and Mara dialogue
captures and the native four-button menu were inspected internally. No game,
scene/prefab, save format, font or license was reauthored for this milestone.
After the ordering correction, both final applications were relinked, all five
source-example checks repeated successfully, and the normal Nutella export of
the native-authored spatial menu passes relocation
with source resources/examples/SDK/GAC unavailable: project defaults, persisted
override and restart, corrupt override rejection without rewriting it, and reset
back to project defaults. Scene/prefab/project dependency and notice closure are
checked; no test-font collection ships. This is rendered startup/lifecycle and
persistence qualification, not physical keyboard/controller acceptance.

On 2026-10-23 the restarted Plasma Wayland session changed its Xwayland auth path.
An initial example launch failed before GLFW initialization using the expired
path; it is not a successful run. Repeating with the current session passed.
Both sessions use accelerated HD4000/Mesa 26.2.4-arch3.1 and GLFW X11/GLX through
Xwayland; there is no native Wayland claim. No desktop packages/settings were
changed by this work. No input injection denied by KWin is credited.

Master, stash, vendor pins, settings/layouts and the exact unrelated Sandbox draft
remain unchanged. CI is triggered by pushes and not awaited/polled. No new frame
budget is claimed; large alternating rendering runs and cold glyph preparation
remain the previously measured separate costs.

Focused physical acceptance (disposable project, then Windows hardware):

1. In Project Settings, capture/select an alternate Accept key and remap Cancel;
   inspect conflicts, cancel capture, reset/revert, Save and reopen. Create a
   four-button menu, set TabOrder, save/reopen scene/prefab, and repeat Play/Stop.
2. In Play and Nutella, Tab/Shift+Tab wrap; arrows choose spatial neighbors and
   retain focus at an edge. Check disabled/partly/fully clipped controls, modal
   return focus, a quick click followed by an arrow, and no gameplay input while
   UI owns the gesture. Text fields/popups/other editor panels keep ownership.
3. Connect while holding confirm, release to neutral, then test D-pad/stick,
   repeat, jitter, pointer takeover, held Accept, disconnect/reconnect and focus/
   minimize/restore. Cancel is consumed by the active scope, never an invented
   button activation. Verify a persistent override/reset in a relocated player.

Physical keyboard/controller usability, Windows input/DPI and detached-monitor
workflows remain unverified locally. Existing game-specific raw-key shortcuts
remain game logic; this is UI remapping, not gamepad gameplay support or a general
input framework. One standardized controller and the present highest-modal /
scene scopes are supported, without a screen/popup asset stack or new controls.

**Next bounded P4 slice:** scene-owned screen/modal stack with explicit initial,
default/cancel and return-focus targets, one declared scoped event owner, native
configuration and deterministic push/pop/retirement. Build composed controls on
that ownership foundation afterward; keep scrolling and large data views behind
their separate rendering/virtualization budgets. This does not complete all P4
or the practical screen-space production endpoint.

## P4: scene-owned screens, focus targets and scoped event owners

Baseline: `ef9f946`, after `2946e3a` and `25a1df4`, on
`feature/runtime-ui-hardening`. The existing action adapter, spatial navigation,
typography, painter ordering and document history remain in place. This is the
screen/ownership slice, not completion of P4 or production UI.

### Authored configuration and live authority

`UICanvasComponent::Screen` is a small optional authored policy: Managed,
InitiallyOpen, Cancel (Ignore/Close/NotifyOwner), InitialFocus, DefaultAction,
CancelTarget (semantic context), ReturnFocus and EventOwner. Ordinary canvases
keep their existing defaults. Canvas serialization/strict validation, exact binary
authored comparison, shared Inspector transactions, scene copy/duplication and
prefab remapping include every field. Prefabs reject external screen references,
using the same connected-subtree rule as script Entity fields; export checks it
again. No screen asset format, migrator or competing history is introduced.

`RuntimeUI`, owned by the Scene, is the single live authority. Its screen code is
split into `RuntimeUIScreen.cpp`; the router no longer owns independent return
records or computes a modal filter. One snapshot supplies painter order, active
scope and modal blocking to drawing, picking and routing. Ordinary canvases draw
first in authored sort/UUID order. Open managed screens follow in opening order;
initial opens use sort/UUID order. Closed screens do not participate at runtime.
The foreground visible/enabled managed screen owns UI focus/hits, including a
nonmodal overlay. Underlying UI controls cannot click through. Nonmodal does not
impose unconditional gameplay blocking; existing keyboard/pointer ownership gates
apply. Any available modal still blocks gameplay, even without buttons. Without
a managed foreground, ordinary highest-modal/scene-zero scopes use the same list.

Open order, requests, focus/keyboard-return records, events and diagnostics are
transient. They never write Visible, history or dirty state. InitiallyOpen changes
an Edit default, not a running stack. Hidden/disabled open roots remain remembered
but unavailable; their descendants cannot interact. Canvas geometry remains an
independent screen surface, including authored nested canvases. Closing a non-top
screen does not implicitly close higher screens or their independent surfaces.

### Requests, focus and delivery

Scene request/focus/owner-consume APIs and managed Canvas Open/Close/ReplaceTop,
GameUI.PopScreen/Focus/TryConsumeEvent expose the service. Requests are FIFO and
commit before input and after script callbacks, outside control iteration.
Consume/Resolve never commit requests. Open/Close are idempotent; Open does not
raise/reset an open screen. ReplaceTop accepts a closed target or the current top;
an already-open lower target rejects atomically. Pop removes the last open screen.
Opening a closed unavailable target rejects at commit; close remains available.
The API bool means enqueued, with actual state available through IsOpen and
rejection diagnostics. Retired target/component or managed issuer/class rejects.

Opening selects eligible configured initial focus, otherwise first TabOrder/
painter target. Selection can be published while input is still rearming; it does
not arm activation. DefaultAction is considered only without eligible focus; it
never bypasses another focused control. Explicit GameUI.Focus requires a live input
owner and current eligibility; null clears focus. Closing restores eligible prior
focus and keyboard ownership, then the closing ReturnFocus, underlying initial
focus and deterministic first eligible target. Missing, retired, reparented,
disabled and fully clipped targets diagnose/fall back, without registry borrowing.
Pointer-opened modal dismissal preserves the underlying nonmodal ownership state.

Every screen transition cancels pending delivery/capture and rearms the existing
session input adapter. Native owner/class/component changes and managed-status
changes do so even when the scope UUID remains equal. Managed scope changes caused
by eligibility changes also retire the gesture. An enabled release-only sample can
establish neutral, but its releases never route into the new scope; fresh down
edges cannot click through. RuntimeSession retains its physical held-input gameplay
gate until the effective map returns neutral. Accept still activates once, Cancel
wins over simultaneous activation, and navigation repeat remains separate.

One outer delivery stream contains typed Activated/Cancel records with source,
screen, declared owner, monotonic sequence and authored action identity. There are
no delegates or unnecessary lifecycle notifications. Managed records are invisible
to unscoped button/global/Cancel consumers; unmanaged canvases retain those simple
conveniences, without duplicate queues for game delivery. The managed bridge
derives the consumer from its executing Entity lifecycle callback and verifies its
actual declared script class. Native callers supply the designated scene-local
owner ID. Source eligibility/action, scope, owner lifetime/component/class and
accepted policy are rechecked at candidate delivery, preserving exact native-write
detection. A new/retired receiver cannot inherit old events. Stop, scene/domain
replacement and reset retire queues, published scope IDs and return records.

Cancel Ignore consumes/cancels the gesture without a game event. Close requests
closure of the current managed screen and needs no script owner. NotifyOwner emits
a semantic Cancel, with an eligible configured context or the screen itself as
source; it never manufactures a button activation or loads a scene. Missing/unready
owners reject events with a bounded diagnostic. Raw native input remains a trusted
escape hatch; native gameplay must honor the established session/scene gate.

Limits: 64 declared managed screens, 64 pending requests, 64 outer events and 64
return records (oldest return record evicts; newest queue entry rejects/drops).
Existing 4096-rect, depth-256 and 128-byte action limits remain. Owner/issuer class
metadata is capped at 512 bytes with no NUL; identity metadata contains no registry
handles, GPU resources or managed objects. Bounded vectors reserve before stack
mutation; diagnostics are rate limited and retained for inspection. These are
service limits, not a total-process memory or frame-budget claim.

### Native authoring and representative use

Hierarchy → Runtime UI → Screen creates a managed modal Canvas. Canvas Properties
exposes initial state, Cancel and named, document-local owner/focus selectors;
detailed default/Cancel-context/return fields are progressive. HUD Canvas remains
simple. Selecting a screen/child previews that screen in Edit even if initially
closed; otherwise initially-open screens compose. Prefab preview follows selection.
Preview and runtime stack changes do not alter authored values. The existing Runtime
UI Debugger shows screen availability/order, scope, owner, pending/retired events,
request counts and recent issues; no extra management panel or optimization mode.

The maintained native harness creates Menu → Settings → Confirmation using current
Panel/Text/Button presets, shared Inspector edits and an ordinary owner script
(`tests/fixtures/AuthoringProject/Assets/Scripts/Source/ScreenOwner.cs`). It requests
screen operations and relies on engine focus/gating/retirement. SceneTransitions'
TitleUI/main menu now declares its Canvas as event owner and consumes the typed
Play event; the per-button polling class is removed. Its gameplay scene, save/data
formats, art, fonts and notices are unchanged. Other examples remain on ordinary
canvases through the same pipeline, without a bulk rewrite.

### Qualification

Premake/GNU Make builds with two compiler jobs pass in Debug and Release. All
17 regression executables pass in each configuration on the final implementation
checkpoint `b992b4f`: Release forced OpenGL 4.1 / GLSL410, Debug native OpenGL 4.2,
on accelerated Intel HD4000 / Mesa 26.2.4-arch3.1. The desktop is Plasma Wayland;
GLFW uses X11/GLX through Xwayland, not native Wayland. No desktop packages/settings
were changed. CI is not awaited or polled.

Focused production checks cover initial/default/remembered/fallback focus,
republication while unarmed, nested modal/nonmodal scopes, empty modals, unavailable
roots, idempotent/FIFO/replace/non-top operations, request limits, native owner/class
and managed-status changes, component retirement, prefab/copy remapping and exact
snapshot reuse. Managed checks verify an unrelated script cannot steal records,
Notify-owner Cancel/context delivery, once-only activation, held transition gates,
retired request issuers, domain replacement and three Play/Stop cycles. Synthetic
input crosses the production adapter/router boundary; it is not physical controller
acceptance.

The actual native ImGui harness creates/configures the three-screen flow with
shared Inspector transactions, Undo/Redo, scene/prefab save/reopen, selected closed-
screen preview and three scripted keyboard Play/Stop runs. Inspection does not
mutate authored state or dispatch actions. Existing P1, binding, expanded five-state
Button Inspector, history, typography/overflow, D8/painter/capacity and lifecycle
regressions remain enabled. New regressions exposed and fixed both loss of initial
focus during unarmed reflow and semantic Cancel being cleared by ordered-input final
synchronization. Aggregate cancellation suppresses activations for the sample;
final held synchronization preserves the Cancel record and observes the actual
release so the next quick gesture can rearm.

No rendering shader, font/resource policy or platform backend changes are part of
this slice. Prior render-only performance evidence still applies; no new
whole-frame budget or GPU timing is claimed. Native mutation detection remains
exact at input/render/candidate-delivery boundaries; idle event polls do no extra
source walk when there is no matching recipient.

Release source acceptance passes all five projects in Hazelnut and Nutella:
startup, rendering, resize, graceful shutdown and unchanged authored hashes. The
existing native/managed game checks pass collection/completion, Lightkeeper
chapter/progress/save/failure recovery and repeated lifecycle. A small extension
to the maintained native probe exercises SceneTransitions' actual MenuOwner:
production synthetic Accept → typed Play → Level1, with the old scope retired.
This also caught the controller block's incorrect placement after the scene-version
field; the corrected scene and its prefab structure are validated and exercised.
No other game content or save/progress format was changed.

The normal Nutella export of the native-authored screen fixture passes extraction
to an unrelated Unicode directory, startup/resize/shutdown and a bounded scripted
nested-open/close/return probe, with source resources/examples, SDK and external
Mono GAC unavailable. The descriptor, scene/prefab, real face files and exact
notices are checked; no typography test corpus/font collection ships. Packaged
assets remain byte-identical. The package probe exercises the service, not physical
keyboard/controller input. Current package menu, SceneTransitions title and
Lightkeeper HUD/Mara dialogue captures were inspected internally. No screenshot
comparison or competent-reader language acceptance is inferred.

Master, stash, vendor pins, settings/layouts and the exact unrelated Sandbox draft
remain unchanged. Native Windows/MSVC, physical controllers, DPI/monitor transitions
and native Wayland remain unqualified locally. No denied desktop injection is
credited, and no CI run was awaited or polled.

Concise physical acceptance (disposable project or extracted fixture):

1. Create a Screen; assign a script owner and named initial/default targets. Make
   Settings/Confirmation initially closed, configure Cancel, and save/reopen their
   connected prefab. Undo/Redo policy and target edits; select a closed screen to
   preview it without changing initial state.
2. In Play and Nutella, open Settings and Confirmation by pointer and keyboard;
   Cancel back to each invoking control. Check non-top close, nonmodal ownership,
   disabled/clipped/deleted return targets and the debugger's current scope/owner.
3. Hold Accept or a pointer press across open/close, release, then press again.
   Try quick Cancel gestures, focus loss/minimize/restore, gamepad neutral/repeat
   and disconnect. Verify no activation reaches a newly exposed screen or gameplay.
4. Reload scripts and repeat Play/Stop; authoring/history stay unchanged. Repeat on
   physical Windows hardware and mixed-DPI/detached viewports before claiming them.

**Next bounded composed-controls slice:** Toggle/checkbox and radio-group state,
plus a simple progress display, composed from the existing layout/visual/input
primitives. Add validated values and owner-only change events, non-color selected
cues, native presets/Inspector/history and keyboard/controller tests. Sliders can
follow with real Adjust actions. Keep scrolling/data views, themes, structural
history and the known large alternating draw-run debt separate. This screen slice
does not finish all P4 or the practical screen-space production endpoint.
