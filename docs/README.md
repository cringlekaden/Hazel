# Hazel documentation

Build a 2D game with **Hazel**, author it in **Hazelnut**, and run it in **Nutella**.
These guides describe `feature/runtime-ui-hardening`; master remains the earlier integrated version until final acceptance.

## Start here

| I want to… | Guide |
| --- | --- |
| Try a download or build the source | [Getting started](getting-started.md) |
| Create a project, edit a scene, save and Play | [Editor guide](editor-guide.md) |
| Add C# behavior | [Scripting](scripting.md) |
| Slice sprites and animate them | [Sprite authoring](sprite-authoring.md) |
| Build a menu or HUD | [Runtime UI](runtime-game-ui.md) · [five-page tutorial](../examples/Sandbox/README.md) |
| Export a standalone game | [Packaging and downloads](packaging.md) |

## Learn from an example

| Project | What to explore |
| --- | --- |
| [Sandbox — UI Tutorial](../examples/Sandbox/README.md) | Five authored pages: layout, typography, controls, screens and reusable compositions |
| [The Last Lightkeeper](../examples/LastLightkeeper/README.md) | Island adventure, dialogue, puzzle progression, persistent saves and settings |
| [MeadowRun](../examples/MeadowRun/README.md) | Three garden stages, physics, collectibles and detached prefabs |
| [Skybound](../examples/Skybound/README.md) | Scrolling flight, fixed-step rules, animation and pooled course slots |
| [SceneTransitions](../examples/SceneTransitions/README.md) | Small scene-transition, scripting and player-host example |

## Reference and development

- [Platforms, dependencies and licensing](platforms-and-dependencies.md): deployment baseline, supported graphics, pins and notices.
- [Settings and resource ownership](settings-and-resources.md): project defaults, editor profiles, player storage, fonts and caches.
- [Saves and audio](runtime-storage-audio.md): exact storage/string contract and bounded WAV playback.
- [Authoring reliability](authoring-reliability.md): transactions, entity identity, reference remapping and save guarantees.
- [Development, testing, CI and troubleshooting](development.md): fast checks, qualification coverage and native diagnostics.
- [Contributing](../CONTRIBUTING.md): useful bug reports and scoped changes.

[Historical records](history.md) retain dated designs, migration evidence and completed checkpoint reports. They describe their recorded revisions; use the guides above for current commands and behavior.
