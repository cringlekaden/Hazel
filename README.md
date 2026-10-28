# Hazel

A Linux/Windows extension of [TheCherno/Hazel](https://github.com/TheCherno/Hazel/tree/1feb70572fa87fa1c4ba784a2cfeada5b4a500db). **Hazelnut** edits projects; **Nutella** runs standalone games. This fork adds cross-platform tooling, managed scripting, sprite animation, detached prefabs and authored screen-space runtime UI.

## Downloads

Historical verified master snapshot [`a31187d`](https://github.com/cringlekaden/Hazel/actions/runs/37756280905) (GitHub login required). It includes the editor usability/sprite-authoring work and MeadowRun/Skybound, **before** the later runtime UI and Last Lightkeeper integration:

| Application | Windows x64 | Linux x86_64 |
| --- | --- | --- |
| Hazelnut + Nutella | [Download](https://github.com/cringlekaden/Hazel/actions/runs/37756280905/artifacts/11541340516) | [Download](https://github.com/cringlekaden/Hazel/actions/runs/37756280905/artifacts/11541596001) |
| MeadowRun | [Download](https://github.com/cringlekaden/Hazel/actions/runs/37756280905/artifacts/11541416345) | [Download](https://github.com/cringlekaden/Hazel/actions/runs/37756280905/artifacts/11541730832) |
| Skybound | [Download](https://github.com/cringlekaden/Hazel/actions/runs/37756280905/artifacts/11541147123) | [Download](https://github.com/cringlekaden/Hazel/actions/runs/37756280905/artifacts/11541532045) |

Extract the download, then the application archive inside it, and follow `LAUNCH.txt` to run `Hazelnut` or `Nutella` (`.exe` on Windows). Verify the included archive checksum and file inventory. Artifacts expire: select a **successful whole run**, confirm its SHA/platform and inspect its artifacts; an upload alone does not establish that later gates passed. [Download and checksum instructions](docs/getting-started.md#choose-a-download-or-a-source-checkout) explain authentication, retention and package contents. No formal release is published.

Requires **OpenGL 4.1 core**, Windows 10 x64 or newer, or an X11/GLX Linux desktop. Official Linux builds target Ubuntu 24.04 / glibc 2.39 or newer. Prepared games need no SDK, Python or compiler; runtime dependencies are bundled.

## Build from source

```sh
git clone --branch feature/runtime-ui-hardening --recurse-submodules https://github.com/cringlekaden/Hazel.git
cd Hazel
scripts/setup.sh                  # Linux
```

These current UI/tutorial instructions target `feature/runtime-ui-hardening` until branch integration. On Windows, run `./scripts/setup.ps1` instead. Install the [Windows or Linux prerequisites](docs/getting-started.md#prerequisites-installed-by-you) first: Python 3.9+ with pip/pinned PyYAML, Git, native compiler/development libraries, and the documented Mono compiler/profile or Windows .NET 4.7.2 targeting pack. Setup checks prerequisites, acquires checkout-owned pinned tools through **Premake**, and builds Debug with two compiler jobs; it never installs system packages. Premake is the only build system.

```sh
python3 scripts/hazel.py script-build examples/Sandbox/Sandbox.hproj --config Debug
python3 scripts/hazel.py run Hazelnut --config Debug --project examples/Sandbox/Sandbox.hproj
```

Use `python` on Windows. `run` stages/launches existing products; it does not compile them. [Getting started](docs/getting-started.md) has concrete package lists, SDK selection, troubleshooting, VS Code templates and command semantics. Native outputs are in `bin/`, intermediates in `bin-int/`, generated tooling/dependencies in `build/`, deployed project scripts at their configured asset paths, and packages in `dist/` by default.

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

Start with the [five-page Sandbox UI Tutorial](examples/Sandbox/README.md): open **Assets/Scenes/UI Tutorial.hazel** and Play. [MeadowRun](examples/MeadowRun/README.md) is a garden expedition; [Skybound](examples/Skybound/README.md) is a scrolling flight game. [The Last Lightkeeper](examples/LastLightkeeper/README.md) begins an authored island restoration adventure. Their READMEs describe controls and assets. `SceneTransitions` is the focused runtime/CLI example.

```sh
python3 scripts/hazel.py build --config Release
python3 scripts/hazel.py script-build examples/Skybound/Skybound.hproj --config Release
python3 scripts/hazel.py run Nutella --config Release --project examples/Skybound/Skybound.hproj
python3 scripts/hazel.py package --app Nutella --project examples/Skybound/Skybound.hproj
```

Plain `package` validates/packages saved content and **existing Release outputs**; it does not build them. Editor **Export Game** / CLI `editor-export` performs targeted Release native and project-script builds before packaging; see the [complete export sequence](docs/getting-started.md#release-build-and-export). Extracted Nutella discovers its single root `.hproj`; `--project <file>` selects one explicitly. Detached prefabs instantiate independent subtrees with remapped internal references; linked overrides and variants are deferred.

## Documentation and verification

- [Getting started, downloads, setup, scripting builds and export](docs/getting-started.md)
- [Runtime UI authoring, styles, fonts, input and bounded history](docs/runtime-game-ui.md)
- [Sandbox UI Tutorial](examples/Sandbox/README.md) and [runtime saves/audio](docs/runtime-storage-audio.md)
- [Sprite-sheet reference and historical design](docs/design/sprite-sheet-authoring.md), [data/lifecycle contracts](docs/authoring-reliability.md)
- [Cleanup audit and implementation evidence](docs/design/post-ui-cleanup-audit.md#first-cleanup-slice-implementation-status); older [editor](docs/editor-authoring.md) and [runtime](docs/nutella-runtime.md) milestone records retain their checkpoint context

The [Linux/Windows workflow](https://github.com/cringlekaden/Hazel/actions/workflows/c-cpp.yml) is intended to qualify Debug/Release, rendering/lifecycle regressions and relocated packages. Both jobs failed at compilation at audit baseline `650d09e`; the cleanup status records repair results rather than treating older successful runs as current guarantees. Automated checks do not replace physical Windows/controller/DPI or competent-reader typography acceptance. macOS and additional graphics backends are not implemented.

Source follows the retained [Apache-2.0 license](LICENSE); dependencies and project assets have their own notices. [Upstream provenance](docs/migration/PROVENANCE.md) records imports. The broader documentation inventory/navigation consolidation remains a later cleanup slice.
