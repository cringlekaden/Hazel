# MeadowRun

An original, independently authored Hazel example. WASD / arrow keys: move. Collect five lantern seeds, avoid the pond, then reach the north-east trail. R restarts; Escape returns to the title. The pond returns you to camp without losing seeds.

## Edit, build, play and package

From the Hazel checkout (Windows: use `python` instead of `python3`):

```sh
python3 scripts/hazel.py script-build examples/MeadowRun/MeadowRun.hproj
python3 scripts/hazel.py run Hazelnut --project examples/MeadowRun/MeadowRun.hproj
python3 scripts/hazel.py run Nutella --project examples/MeadowRun/MeadowRun.hproj
python3 scripts/hazel.py script-build examples/MeadowRun/MeadowRun.hproj --config Release
python3 scripts/hazel.py package --app Nutella --project examples/MeadowRun/MeadowRun.hproj --name MeadowRun --output dist/games
```

Build the engine first with `scripts/setup.sh` / `scripts/setup.ps1`; build Release
before packaging with `python3 scripts/hazel.py build --config Release`.
In Hazelnut, use the toolbar triangle to Play and square to Stop. Scene files are
editable through the hierarchy/inspector; supported public script fields expose
tuning. Stop preserves authored content. Scenes use existing `.hazel` serialization
and `Hazel.Scene.LoadScene`, without a separate game/runtime format.

Authored content: MainMenu, Meadow and Complete scenes; Explorer body and speed, collision boundaries, trees/rocks, pond, seeds, exit and HUD. The Meadow script owns only the current run’s progress.

Camera policy: fit the entire 16 x 12 world area, revealing additional background
at other aspect ratios. Gameplay bounds/speed remain fixed. Mouse hit testing uses
the current camera and active runtime viewport, including Hazelnut offsets. A very
small window remains functional but reduces text size; 900 x 640 or larger is recommended.

All assets/scripts belong to this project; no shared mutable state or assembly.
Texture paths are asset-relative. Original pixel art is documented in LICENSE.txt;
optional regeneration source is `examples/art/generate.py` (Pillow required only
for regeneration). Open Sans and all required engine/runtime licenses are packaged.

Extract the Nutella archive and run `Nutella` / `Nutella.exe` with no arguments,
from any directory. No checkout/compiler/SDK is needed for precompiled scripts.
Packages require OpenGL 4.1; Windows 10 x64+ or official Ubuntu 24.04/glibc 2.39+
Linux with X11/GLX. Compiling changed scripts separately requires the Hazel SDK.
