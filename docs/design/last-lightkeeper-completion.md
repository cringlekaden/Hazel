# Completion checkpoints and runtime UI design

Historical content plan and initial UI inspection. The content checkpoints below
are complete. Runtime UI is now integrated and the example migration is complete;
see [the implementation checkpoint](runtime-ui-integration.md) and
[the authoring guide](../runtime-game-ui.md) for the implemented workflow and APIs.

Inspected baseline: `feature/last-lightkeeper`, clean `13cbbdd`; master,
stash, settings and vendor pins are outside this work. Existing two chapters
retain their completed composition and writing. Durations remain human-untimed.

## Inspection and boundaries

Scenes and prefab subtrees are native version-2 documents. Root Box2D bodies,
managed Entity references, asset-relative queued transitions, stable sprite IDs,
nearest sheet sampling, animations, bounded light-prefab pools and audio sources
already cover the remaining game. Ordinary C# owns puzzles, dialogue and story.
No gameplay engine feature or new dependency is needed for those chapters.
The project owns durable storage through its unchanged SaveNamespace. Writes
must succeed before a transition retires unsaved session progress.

The current menu polls Enter/N/C and mouse edges, projects the cursor into the
camera, then checks fixed rectangles. HUD and dialog panels are camera children;
nine border sprites are composed manually. Renderer2D draws MSDF text at a
transform baseline with explicit newlines, kerning and line spacing, without
measurement, wrapping or alignment. Text fonts are runtime references; serialized
font selection needs an asset-owned path. Hazelnut exposes sprite sources and
text but no runtime canvas. Hazel::UI is editor ImGui only. Scene copy,
serialization, exact authored snapshots, prefab reference remapping, validation,
managed bindings and exporter closure all need UI integration together.

## Playable stages

1. **Observatory.** Cliff Dawn becomes a departure checkpoint. Restore the
   survey hall's two opposed sightlines across its collapsed central floor.
   A collimating lens extends focused lantern range, exposing distant latches
   without rotating another mirror chain. East and west firing terraces create
   a spatial decision; a jammed survey carriage must be parked clear of a line.
   Meet Nessa (Orrin's sister) and Cal in the north instrument room. They secure
   the tower winch and provide the missing calibration. Endpoint: crew safe,
   tower route ready. Verify old-save entry, blocked rays, carriage states,
   latch prerequisites, collision closure, recovery, failure-safe writes and
   relocated startup before building the tower.
2. **Upper tower.** A bounded ascent and storm encounter combine lantern aim,
   obstruction and telegraphed danger. Repairs persist through recovery. Resolve
   the crew evacuation and each existing character's stake; ending and credits
   return to title with explicit completed Continue behavior. No duration claim.
3. **Runtime UI foundation.** Integrate the architecture below, focused native
   and managed regressions, prefab/scene roundtrips and lifecycle tests.
4. **Authoring and migration.** Hazelnut creation presets/Inspector/viewport
   preview and documented C# action polling; migrate all game overlays and menus.
   Retire camera-parenting and manual click regions only after replacements work.

Each stage has its own reviewable commit and working game. If a stage cannot be
finished responsibly in this run, retain the last verified playable checkpoint
and explicitly report the remaining stages.

## Runtime UI architecture (implementation contract)

- `UICanvasComponent`: reference pixel resolution, uniform fit policy, safe
  inset, sort order and enabled state. Screen-space orthographic pass follows
  world rendering, independent of gameplay cameras. Identical scene traversal
  feeds editor preview and Nutella; no ImGui dependency or backend-specific state.
  The pass also works in a UI-only scene without a primary world camera. Preserve
  painter order across sprite/text batches by flushing at ordered type boundaries;
  restore render state after the overlay instead of depending on world depth.
- `UIRectComponent`: normalized anchor min/max, offsets, pivot, minimum size,
  visibility and padding. Hierarchy owns grouping; children inherit clipping
  and enabled state. Layout groups support horizontal/vertical flow, gap and
  alignment; explicit rects remain available. Resolve parent-before-child into
  a per-scene scratch array. Bound nodes/depth, reuse capacity; dirty content,
  viewport or authored settings invalidate layout rather than repositioning
  every child in C# every frame.
- Reuse SpriteSource and Font asset ownership. UI images/panels reference existing
  sheet regions/textures; optional nine-slice borders are authored source-pixel
  insets validated against the source rectangle, with borders clamped on small
  panels. Text adds asset-relative font, reference-pixel size, alignment,
  wrapping and spacing. A shared CPU glyph-measure/layout routine must match
  drawing. Start with separate text children for mixed emphasis: explicit,
  inspectable and no markup parser/dependency.
- `UIButtonComponent`: action string, interactable state, normal/hover/pressed/
  disabled/focus tints or sprite overrides, tab order. Resolved rect is the
  authoritative hit area; topmost enabled button wins. Mouse capture activates
  on release inside the same live button. Tab/arrows move focus; Enter/Space
  activate on edges. Suppress the second route when mouse and keyboard coincide.
  Focus/capture/action queues belong to the active scene/session identity and
  clear on transition/Stop/input disable or button retirement.
  Reuse Scene::GetIdentity and managed Entity.CheckedID ownership checks: a
  RuntimeSession identity alone does not change on every scene transition.
- Managed APIs expose text, visibility, enabled state and `TryConsumeAction`
  (entity ID plus action string), checked against the current scene. No
  serialized delegates, retained callback closures or native calls into retired
  managed objects. C# decides New Game, save confirmation, pages and transitions.
  Engine resolves layout, input, state and rendering before game action polling.
- Hazelnut presets create normal canvas/panel/text/button entities. Inspector
  edits all fields and asset references; viewport shows reference-size presets
  and real resized layout. Validate finite/nonnegative values, hierarchy and
  source borders; include all fields in authored snapshots and component copy.
  Package closure scans UI font/image/state references and existing licenses.

UI acceptance: scene/prefab save-load preserves settings; no editor-only package
dependency; readable at 1024x600, 1280x720 and 4:3; reliable mouse release,
keyboard focus/disabled state and no stale/duplicate actions; repeated Play/Stop,
resizing and scene transition tests. Both Debug/Release and GL 4.1 stay supported.

## Upper tower content boundary

Authored UpperTower → HarborHome → Ending/Credits completes the gameplay route.
The exposed west approach leads through a fixed-mirror latch into a three-contact
lantern drive. Independent north/east/south sightlines, cover and the learned
telegraphed wisp encourage choosing firing positions and retreat routes. All
repairs survive recovery; main-light activation is separate from drive power.
At the landing, Nessa and Orrin settle the compass/boat thread, Cal establishes
the mooring signal, and Iona commits to Mara's dated steps/handrail repair.
The southern mooring finishes the journey. Completed Continue revisits this
landing and can replay the farewell. LK4 retains all LK1/LK2/LK3 settings and
adds only tower checkpoint/contact data. Human playtime remains unmeasured.

At the original content checkpoint, no UI engine implementation was implied. The existing
menu/presentation layer remains until stages 3 and 4 deliver the architecture
and migration above.

The first milestone-3 implementation was the standalone logical layout/input
model in `Hazel/UI/RuntimeUIModel.h`, with focused native regressions. It has no
rendering, managed bindings or scene/editor UI authoring at that checkpoint. Its contracts and
remaining integration are recorded in [the model checkpoint](runtime-ui-model-checkpoint.md).
