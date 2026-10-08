# Hazel

A Linux/Windows extension of [TheCherno/Hazel](https://github.com/TheCherno/Hazel/tree/1feb70572fa87fa1c4ba784a2cfeada5b4a500db). **Hazelnut** edits projects; **Nutella** runs standalone games. This fork adds cross-platform tooling, managed scripting, sprite animation, detached prefabs and editor usability improvements.

## Downloads

Verified master builds from [`a31187d`](https://github.com/cringlekaden/Hazel/actions/runs/37756280905) (GitHub login required):

| Application | Windows x64 | Linux x86_64 |
| --- | --- | --- |
| Hazelnut + Nutella | [Download](https://github.com/cringlekaden/Hazel/actions/runs/37756280905/artifacts/11541340516) | [Download](https://github.com/cringlekaden/Hazel/actions/runs/37756280905/artifacts/11541596001) |
| MeadowRun | [Download](https://github.com/cringlekaden/Hazel/actions/runs/37756280905/artifacts/11541416345) | [Download](https://github.com/cringlekaden/Hazel/actions/runs/37756280905/artifacts/11541730832) |
| Skybound | [Download](https://github.com/cringlekaden/Hazel/actions/runs/37756280905/artifacts/11541147123) | [Download](https://github.com/cringlekaden/Hazel/actions/runs/37756280905/artifacts/11541532045) |

Extract the download, then the application archive inside it, and launch `Hazelnut` or `Nutella` (`.exe` on Windows). Checksums and launch instructions are included. Artifacts expire; [successful master builds](https://github.com/cringlekaden/Hazel/actions/workflows/c-cpp.yml?query=branch%3Amaster) provide newer downloads. No formal release is published.

Requires **OpenGL 4.1 core**, Windows 10 x64 or newer, or an X11/GLX Linux desktop. Official Linux builds target Ubuntu 24.04 / glibc 2.39 or newer. Prepared games need no SDK, Python or compiler; runtime dependencies are bundled.

## Build from source

```sh
git clone --recurse-submodules https://github.com/cringlekaden/Hazel.git
cd Hazel
scripts/setup.sh                  # Linux
```

On Windows, run `./scripts/setup.ps1` instead. Setup diagnoses missing prerequisites, prepares pinned tools through **Premake**, and builds Debug with two compiler jobs. Repeated builds are incremental; setup does not install system packages.

Prerequisites: Python 3.9+, Git and a C++ toolchain. Linux needs GTK3, UUID and X11/OpenGL development libraries; setup reports missing packages. Windows needs Visual Studio 2022 C++ Build Tools, Windows SDK and the .NET Framework 4.7.2 targeting pack. The IDE is optional.

```sh
python3 scripts/hazel.py run Hazelnut
python3 scripts/hazel.py build --config Release
python3 scripts/hazel.py build --tests
python3 scripts/hazel.py test --config Debug
```

Use `python` on Windows. All commands support `--help`. Generated tools/builds live in ignored `build/`; packages go to `dist/`. Vendor pins remain unchanged. Premake is the only build system.

## Use the editor

- **File > New Project / Open** creates or opens content. Normal Open reports recoverable missing resources while preserving references; invalid or unsupported input keeps the previous session.
- **Content Browser** imports textures, searches assets and opens sprite sheets. Slice regions, author clips, then assign them to the named entity.
- **Scene Hierarchy > Add Entity** creates a root. Drag onto an entity to parent it, or onto **Scene Root** to detach it. Both preserve world pose; invalid or nonrepresentable moves are rejected. Properties edit local transforms. Rigidbody/collider owners must remain roots.
- **Ctrl+S / File > Save** saves the active scene, prefab or sheet; **Save All** covers eligible dirty documents. Operation guards distinguish saving, using saved assets, cancellation and actual discard.
- The compact viewport toolbar controls **Play / Simulate / Stop / Pause / Step**. **View > Console** combines logs, tool progress and results.
- **Project** contains script build/export and settings. Runtime rendering requests travel with the project; device/effective values are read-only. **Editor Preferences** owns user settings and the optional Windows custom caption; Linux retains native decorations.

Hazelnut restores the last successful project. Explicit `--project` / `--scene` arguments override restoration; `--no-restore` disables it. VS Code templates live under `scripts/internal/vscode/`: older F5 configurations with an explicit project will always open that project. Local `.vscode/` and docking layouts are user-owned.

A **Hazel source SDK** is a compatible checkout prepared by setup, including the native/managed tools—not the application's executable or Resources directory. Python and SDK discovery are automatic; Preferences can override or reset them. If discovery fails, select the prepared checkout root. Distributed applications do not include this source SDK. Native creation, editing, saving and script-free or already prepared Play remain available without it; script builds and exports need the relevant toolchain.

## Games and export

[MeadowRun](examples/MeadowRun/README.md) is a garden expedition; [Skybound](examples/Skybound/README.md) is a scrolling flight game. Their READMEs describe controls and assets. `SceneTransitions` is the focused runtime/CLI example.

```sh
python3 scripts/hazel.py script-build examples/Skybound/Skybound.hproj --config Release
python3 scripts/hazel.py run Nutella --project examples/Skybound/Skybound.hproj
python3 scripts/hazel.py package --app Nutella --project examples/Skybound/Skybound.hproj
```

Export uses saved content, builds the selected project's Release scripts, and validates asset/runtime closure. Extracted Nutella discovers its single root `.hproj`; `--project <file>` selects one explicitly. Detached prefabs instantiate independent subtrees with remapped internal references; linked overrides and variants are deferred.

## Documentation

- [Editor usability design and remaining scope](docs/design/editor-usability-audit.md)
- [A–H implementation, contracts, verification and acceptance](docs/design/editor-usability-a1-a2.md)
- [Sprite-sheet authoring](docs/design/sprite-sheet-authoring.md)
- [Earlier authoring contracts](docs/editor-authoring.md) and [data/lifecycle safeguards](docs/authoring-reliability.md)
- [Standalone runtime and packaging](docs/nutella-runtime.md), [example games](docs/example-games.md), and [upstream provenance](docs/migration/PROVENANCE.md)

Linux/Windows CI checks Debug/Release, service/lifecycle regressions, software graphics and relocated packages. Automated checks do not replace physical mouse, DPI or GPU acceptance. macOS and additional graphics backends are not implemented.
