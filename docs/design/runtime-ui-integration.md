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
