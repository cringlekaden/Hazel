# MeadowRun

A lantern-seed expedition. Explore three garden stages, collect five lantern seeds in each and open the north-east trail. Pond recovery returns you to camp without losing collected seeds.

![MeadowRun gameplay](../../docs/images/meadowrun-gameplay.png)
*Authored garden, collectible sprites and runtime HUD. Linux capture.*

## Play and edit

WASD / arrows move; **R** restarts the current stage; **Escape** returns to title. Completion offers **Explore Again / Title Menu**; Enter starts a fresh expedition.

From a [prepared checkout](../../docs/getting-started.md), build Debug scripts and launch the player or editor (Windows: `python`):

```sh
python3 scripts/hazel.py script-build examples/MeadowRun/MeadowRun.hproj --config Debug
python3 scripts/hazel.py run Nutella --config Debug --project examples/MeadowRun/MeadowRun.hproj
python3 scripts/hazel.py run Hazelnut --config Debug --project examples/MeadowRun/MeadowRun.hproj
```

Open `Assets/Scenes/Meadow.hazel` in Hazelnut for direct practice. Play runs a copy; Stop retains authored content. MainMenu, Meadow, Orchard, LanternGrove and Complete are authored scenes. Explorer physics, controller speed, boundaries, trees/rocks, pond, seeds, exit and HUD are editable. `LanternSeed` is a detached prefab instantiated on entry and destroyed on pickup. Each stage has distinct scenery/collision and its own checkpoint.

Open `Textures/Lanterns.hsprites`: **Regions** contains the Explorer and seed variants; **Animation** exposes Lantern pulse with ordered frames, durations and Loop. The seed prefab uses that clip with autoplay. Save the sheet, assign its region/clip through the Inspector, save the scene/prefab and Play. See [Sprite authoring](../../docs/sprite-authoring.md).

The camera fits a 16×12 world area, revealing additional scenery at other aspects. Native menus/HUD use runtime UI canvas coordinates, including the editor viewport offset; gameplay/world queries remain separate. A 900×640 or larger window is recommended.

## Export

```sh
python3 scripts/hazel.py build --config Release
python3 scripts/hazel.py script-build examples/MeadowRun/MeadowRun.hproj --config Release
python3 scripts/hazel.py package --app Nutella --project examples/MeadowRun/MeadowRun.hproj --name MeadowRun --output dist/games
```

Extract the archive and run `./Nutella` / `Nutella.exe` from any directory. Prepared games need no compiler, Python or SDK. [Packaging](../../docs/packaging.md) covers checksums, host deployment baseline and the editor-export alternative.

## Assets

All assets/scripts belong to this project; no shared mutable state or assembly.
Texture paths are asset-relative. Original pixel art is documented in LICENSE.txt;
optional regeneration source is `examples/art/generate.py` (Pillow required only
for regeneration). Open Sans and all required engine/runtime licenses are packaged.

[Editor guide](../../docs/editor-guide.md) · [Runtime UI](../../docs/runtime-game-ui.md) · [All examples](../../docs/README.md#learn-from-an-example)
