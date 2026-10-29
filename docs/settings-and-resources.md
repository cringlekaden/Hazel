# Settings, storage and resource ownership

Project data, editor state and player preferences have separate owners. Copying a project should not carry machine paths or another user's docking layout.

## What belongs where

| Owner | Location / behavior |
| --- | --- |
| Portable project defaults | `.hproj`: display name, startup scene, asset root, script module, renderer requests and UI bindings; **Project > Project Settings…** |
| Authored content | Project `Assets`: scenes, detached prefabs, sprites, styles, families, licensed face/image/audio bytes and project scripts |
| Editor preferences | Application user data `preferences.yaml`: Python/SDK/editor selection, UI scale, recents, renderer launch preferences and session restore; **Edit > Editor Preferences…** |
| Editor session | `session.yaml`: last successful project and session selection |
| Project workspace | `workspaces/<path-key>.yaml`: editor camera, open documents, panel/preview state for that descriptor path |
| Docking/window layout | Application user data `imgui.ini`, seeded from shipped resources only when absent |
| Player overrides / saves | `Hazel/Games/<namespace>/slot-<name>.save`, separate from authored defaults and editor profile |
| Shader cache | Application user data `cache/shader/opengl/`; derived backend data, not project source |

## Roots and isolation

Default application data: Linux `${XDG_DATA_HOME:-~/.local/share}/Hazel/<app>/`; Windows `%LOCALAPPDATA%/Hazel/<app>/`. Hazelnut and Nutella use distinct application names. `HAZEL_DATA` replaces the **application** profile root. `HAZEL_SAVE_ROOT` separately replaces the **games** root; see [SaveData](runtime-storage-audio.md#durable-progress). Neither changes project Assets.

Engine Resources and Mono default beside the executable, independent of cwd. `HAZEL_RESOURCES` and `HAZEL_MONO` explicitly replace those runtime roots. Project assets/module paths resolve from the descriptor. A source SDK is a separate authoring tool location; `HAZEL_MONO_SDK` selects the Linux **compiler SDK prefix**, not the runtime Resources/Mono root.

Use disposable project copies and unique `HAZEL_DATA`/`HAZEL_SAVE_ROOT` for tests or experiments. Do not delete preferences, sessions, layouts, saves, ignored VS Code files or caches as a blanket troubleshooting step.

## Persistence and recovery

Preferences/settings apply on explicit actions rather than rewriting each frame. Malformed/future preference files are preserved while defaults are used; explicit **Apply and Save** can preserve the original in `recovery/` before replacement. Concurrent/external preference changes are reported and require reloading the saved values. Session ownership uses a lease; secondary editor instances use private instance state instead of overwriting the primary session. Workspace identity is path-based, so relocating a project can select a new workspace.

Scene/prefab history is document-owned and survives Play/Stop; it resets on document replacement/recovery. Its cursor is not the sole dirty-state authority: native edits are compared exactly. Safe saves use checked sibling temporaries and atomic replacement, without promising power-loss durability or multi-writer merging. See [authoring contracts](authoring-reliability.md).

Nutella opts into persistent game storage. Hazelnut Play starts a session-only overlay, keeps it across its scene transitions and clears it on Stop. Player UI/audio preferences use the project's namespace and do not modify the `.hproj`. [Storage/audio](runtime-storage-audio.md) gives exact limits and failure semantics.

## Fonts, styles and caches

Project text selects `.hfont` families through `ProjectAssets`; world and UI text share accepted faces, shaping and glyph-page ownership. The superseded raw project-font map is removed. Low-level native resource work still uses `Font::Load`. Styles select family/face/image dependencies; every declared variant participates in package closure.

Resource refresh/reload is explicit. A failed font/family/style replacement retains its accepted version for existing users and reports the candidate error. A missing first font load uses the documented engine fallback; repair and explicitly retry to accept new bytes.

Sprite-sheet reload advances the asset epoch and invalidates derived sprite/clip caches even when the replacement fails. Edit previews and running scenes show a magenta diagnostic fallback; starting Play and export still reject invalid assets. Existing resource pins remain alive, but new resolutions report the cached error. Repair the sheet/image, then explicitly reload/refresh to accept it; editing the file alone does not retry each frame. Authored references stay unchanged. See [sprite repair](sprite-authoring.md#edit-safely).

CPU faces/layouts and GPU glyph pages have separate admission/accounting. Active layouts/draws pin accepted resources; pressure can reject new resources until users release pins and retry. GPU resources are created/destroyed on the renderer thread and released before context shutdown. Persistent shader caches are derived files; accepted text/page caches are process-owned, not a second user-data asset store. [Runtime UI's resource reference](runtime-game-ui.md#font-families-and-declared-coverage) records budgets, fallback and notice rules.
