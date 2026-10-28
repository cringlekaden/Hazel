# SceneTransitions

A small Hazel project for learning the standalone host and queued scene changes. MainMenu's authored UI owner requests `Scenes/Level1.hazel`; Level1's Player script applies horizontal impulses and a grounded jump. Its return owner and Escape request the menu.

## Run and inspect

After [source setup](../../docs/getting-started.md), run from the checkout (Windows: `python`):

```sh
python3 scripts/hazel.py script-build examples/SceneTransitions/SceneTransitions.hproj --config Debug
python3 scripts/hazel.py run Nutella --config Debug --project examples/SceneTransitions/SceneTransitions.hproj
python3 scripts/hazel.py run Hazelnut --config Debug --project examples/SceneTransitions/SceneTransitions.hproj
```

Select Play in the menu. **A / D** move, **Space** jumps when grounded, **Escape** returns. In Hazelnut, inspect `MainMenu.hazel` / `Level1.hazel`, their screen owners and `Assets/Scripts/Source/Game.cs`. Stop restores the authored scene. `Scene.LoadScene` queues work outside callbacks; see [Scripting](../../docs/scripting.md#lifecycle-and-identity).

## Export

```sh
python3 scripts/hazel.py build --config Release
python3 scripts/hazel.py script-build examples/SceneTransitions/SceneTransitions.hproj --config Release
python3 scripts/hazel.py package --app Nutella --project examples/SceneTransitions/SceneTransitions.hproj --name SceneTransitions --output dist/games
```

Extract and follow `LAUNCH.txt`; the prepared game runs without the source SDK or compiler. [Packaging](../../docs/packaging.md) documents checksums and platform requirements. [LICENSE.txt](LICENSE.txt) covers original content and the retained Kenney frame; keep the included asset license with exports.
