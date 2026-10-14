# Runtime UI model checkpoint

Historical checkpoint at `0213e48`. The scene/renderer/asset/managed integration now builds on this model; see [the integration plan](runtime-ui-integration.md). Its native Debug rendering/integration regression passed on Grandpa. Editor and complete example/package verification remain in progress. The original checkpoint description below records what existed at that commit.

This is a **partial milestone 3**, not a rendered or editor-authored UI system.
The complete Lightkeeper gameplay checkpoint remains playable. No game menus
have been migrated, no UI scene components have been registered, and no new
canvas/button authoring is claimed. The existing Hazel::UI ImGui helpers remain
editor-only.

`Hazel/UI/RuntimeUIModel.h` contains backend-independent logical-pixel geometry
and interaction in `Hazel::GameUI`:

- Uniform reference-size canvas fitting, safe insets, normalized anchors,
  offsets/pivot, minimum dimensions, margins/padding and rectangular clips.
- Horizontal/vertical flows, alignment, gaps and flex space. Hidden children
  consume neither space nor gaps. Callers retain output capacity.
- Topmost clipped hit regions, mouse capture/release, keyboard focus order,
  disabled/hover/pressed/focus states and one activation per update. An active
  pointer gesture owns activation; optional non-repeat host edges preserve a
  quick press/release between updates.
- Scene-identity action queues, input gating, held-key suppression on entry,
  removal of hidden/disabled actions and explicit element retirement. Actions
  contain IDs/strings, never delegates or managed/native callback pointers.

The host must pass live resolved buttons in painter order, with mouse coordinates
in the same logical space as their bounds. It must tag input with the active
Scene::GetIdentity (a session ID survives transitions), filter key repeat, and
call Retire when an element/component retires. Consume bindings must also check
current scene/entity/component ownership before handing an action to C#.
This module has no event/window hooks or graphics calls. It does not yet provide
text measurement, wrapping, assets, draw order or font/image presentation.

Limits are explicit: 4096 buttons/flow items and 64 queued actions. Validation
runs regardless of visibility/input enablement. ID and focus-order buffers reuse
capacity; action allocations occur on activation. Layout overflow remains
available for the future inherited clipping pass instead of silently altering
minimum sizes.

`GameUIModelSmoke` is part of the normal Premake regression suite. It checks
three viewport/aspect ratios, fixed/stretch anchors and pointer mapping; flow
spacing/flex/visibility; clip edges; quick clicks; capture/release; focus/disabled
states; held input; duplicate routes; viewport disable/restore; scene replacement
and retirement between dispatch and consumption. Debug/native and Release/GL4.1
suites passed all 15 executables. The final gesture/retirement refinements were
then rebuilt and their focused model tests passed in both configurations.
These are behavior checks, not rendered UI or art acceptance.

Next integrate the model with scene components, copy/prefab/serialization and
strict authored snapshots; Renderer2D shared text metrics/wrapping and overlay
ordering; asset-owned fonts/images/nine-slice states and package closure; checked
managed action/state APIs; then Hazelnut presets, Inspector and preview. Only
then migrate the game's overlays and remove legacy camera-parented panels and
click rectangles. See the completion plan for full acceptance criteria.
