# Runtime UI integration

Baseline: clean feature/last-lightkeeper at 0213e48; existing content and saves are accepted. Both baseline CI jobs passed.

## Inventory

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
