# Skybound

An original, independently authored Hazel example. Space / left click: flap. A separate ready state waits for a fresh press after the title. Pass lantern towers to score; touching a tower, the ground or the ceiling ends the flight. R / fresh Space restarts after death; clickable Try Again / Title Menu controls and Escape are available.

## Edit, build, play and package

From the Hazel checkout (Windows: use `python` instead of `python3`):

```sh
python3 scripts/hazel.py script-build examples/Skybound/Skybound.hproj
python3 scripts/hazel.py run Hazelnut --project examples/Skybound/Skybound.hproj
python3 scripts/hazel.py run Nutella --project examples/Skybound/Skybound.hproj
python3 scripts/hazel.py script-build examples/Skybound/Skybound.hproj --config Release
python3 scripts/hazel.py package --app Nutella --project examples/Skybound/Skybound.hproj --name Skybound --output dist/games
```

Build the engine first with `scripts/setup.sh` / `scripts/setup.ps1`; build Release
before packaging with `python3 scripts/hazel.py build --config Release`.
In Hazelnut, use the toolbar triangle to Play and square to Stop. Scene files are
editable through the hierarchy/inspector; supported public script fields expose
tuning. Stop preserves authored content. Scenes use existing `.hazel` serialization
and `Hazel.Scene.LoadScene`, without a separate game/runtime format.

Authored content: MainMenu and Flight scenes; camera, clouds, mountains, ground, wind sprite, four obstacle pairs, HUD, ready prompt and off-screen game-over controls. Game scripts reuse these entities; entity counts remain fixed. Flight uses 120 Hz steps, a deterministic course, constrained gaps and once-only scoring. Long stalls above 250 ms count as a pause.

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
