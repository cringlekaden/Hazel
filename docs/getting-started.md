# Getting started on Windows and Linux

Hazel is the engine library. Hazelnut edits projects; Nutella runs prepared games without the editor. This fork currently builds for **Windows x64 and Linux x86_64**, with **OpenGL 4.1 core**. macOS is not implemented. Linux windows use X11/GLX, including Xwayland; this is not native Wayland support.

## Choose a download or a source checkout

Choose a matching qualified [download snapshot](packaging.md#choose-a-snapshot), or build the source below. Prepared games run without Python, a compiler or the source SDK. Downloaded editors can edit/save and play prepared content; compiling changed scripts and exporting additionally require the authoring tools.

## Clone the source

```sh
git clone --branch feature/runtime-ui-hardening --recurse-submodules https://github.com/cringlekaden/Hazel.git
cd Hazel
```

For an existing checkout missing submodules, use `git submodule update --init --recursive`. Use the committed gitlinks, not `--remote`. Preserve local changes and dependency modifications before setup; do not reset or clean them to fix a build. These current UI/tutorial instructions target `feature/runtime-ui-hardening` until integration; a default clone of master selects the older integrated milestone. Use the explicit branch above for this guide.

## Prerequisites installed by you

Both platforms require Git and Python **3.9+**, with pip available unless the selected interpreter already provides the pinned PyYAML **6.0.3**. Check `python3 --version` / `python3 -m pip --version` on Linux, or `python --version` / `python -m pip --version` on Windows. The CLI uses `python` on Windows; a `py` launcher alone is insufficient for `setup.ps1`. Premake generates the builds; CMake and a Vulkan SDK are not required.

### Windows

Install [Visual Studio 2022 Build Tools](https://visualstudio.microsoft.com/vs/older-downloads/) (or Visual Studio 2022) with **Desktop development with C++**, the MSVC x64/x86 tools, MSBuild and a Windows SDK. Also install the [.NET Framework 4.7.2 Developer Pack](https://dotnet.microsoft.com/en-us/download/dotnet-framework/net472), including its targeting/reference assemblies. A runtime-only .NET installation does not supply that targeting pack. The IDE itself is optional. Install Python with pip and make `python` and Git available to the shell.

The matching Windows Mono native SDK and managed runtime profile are already tracked under `Hazel/vendor/mono` and `Hazelnut/mono`. Do not install an unrelated Mono or switch CRTs to solve an include/link error. Native Debug uses the dynamic Debug CRT; Release uses the dynamic Release CRT. MSVC source/execution encoding is UTF-8.

From PowerShell in the checkout:

```powershell
./scripts/setup.ps1 --tests
python scripts/hazel.py run Hazelnut --config Debug
```

### Ubuntu 24.04

The maintained CI uses this development-package set (installing packages requires your normal administrator permission; setup itself never elevates):

```sh
sudo apt-get update
sudo apt-get install -y build-essential git python3 python3-pip pkg-config mono-devel uuid-dev libgtk-3-dev libx11-dev libxext-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libgl1-mesa-dev libegl1-mesa-dev
scripts/setup.sh --tests
```

`mono-devel` supplies the native headers/library, `mono`, `mcs.exe`, framework assemblies and 4.7.2 reference profile. Graphics tests need a display. For the existing headless CI-style tests additionally install `xvfb xauth libxtst6`; optional compilation-database/debug tools are `bear gdb`.

### Arch / CachyOS

For local GCC builds:

```sh
sudo pacman -S --needed base-devel git python python-pip pkgconf gtk3 util-linux-libs libx11 libxext libxrandr libxinerama libxcursor libxi mesa libglvnd libarchive
scripts/setup.sh --tests
```

The UUID headers/pkg-config data come from util-linux-libs; `libarchive` supplies `bsdtar`. If a complete system Mono SDK is unavailable, setup uses the checksum-pinned isolated SDK recorded in [mono.json](../scripts/dependencies/mono.json). It downloads/extracts into the checkout, without changing `/usr`. Optional tools: `bear gdb`; Xvfb testing uses `xorg-server-xvfb xorg-xauth libxtst`. These are local rolling-distribution builds, not the Ubuntu deployment baseline.

### Mono selection and missing prerequisites

On Linux `HAZEL_MONO_SDK=/absolute/sdk/prefix` explicitly selects a complete SDK. Otherwise an existing `build/dependencies/mono/linux/usr` is selected, then `/usr`; if system Mono is incomplete and `bsdtar` exists, the recorded isolated SDK is acquired. An explicitly selected or already present incomplete SDK fails with its missing-file list rather than falling back silently.

A complete prefix needs `bin/mono`, `include/mono-2.0/mono/jit/jit.h`, `lib/libmonosgen-2.0.so`, `lib/mono/4.5/{mscorlib.dll,mcs.exe}` and `lib/mono/4.7.2-api/mscorlib.dll`, with the corresponding Mono config beside the prefix (`../etc/mono/config`, or `/etc/mono/config` for `/usr`). A runtime-only Mono profile can run prepared scripts but cannot compile new ones.

Use the canonical Python build command: invoking system Premake directly without `--mono-root` defaults to `/usr`, even when setup prepared an isolated SDK. Editor readiness and CLI `script-build` use the same `mono_prefix` selection. Preflight reuses that resolved prefix for `mcs.exe` and the 4.7.2 reference profile; it does not check unrelated default SDKs after accepting `HAZEL_MONO_SDK`. A selected SDK with missing compiler/profile files fails with its path and missing-file list, even if another SDK is installed. Paths may contain spaces/Unicode. Precompiled Play needs the runtime; readiness for compiling scripts additionally requires the complete compiler SDK.

Setup reports missing tools, pkg-config development modules, targeting packs, incomplete SDKs and checksum mismatches. Install the named prerequisite or select a complete existing SDK, then rerun. Do not install system GLM to compensate for a generated target missing the pinned include directory; GLM is a submodule. For checksum errors preserve the downloaded file and check the recorded source/hash before replacing it. Never edit vendor headers to bypass compiler diagnostics.

## What setup acquires and each command does

Setup checks prerequisites, initializes pinned submodules, prepares pinned Premake/shader tools and PyYAML, selects/prepares Mono, then builds the selected configuration (Debug by default). It **does build**, but does not install system packages. Network access is needed for missing checkout-owned inputs. PyYAML is installed with pip into `build/tools/python`; no global pip install is required. Shader dependencies are built from the recorded source pins through Premake. Unchanged builds are incremental and use at most two compiler jobs.

| Command | Behavior |
| --- | --- |
| `setup [--config Debug\|Release] [--tests]` | Check/prepare environment and build, including optional regression targets |
| `build --config Debug\|Release [--tests]` | Generate native/managed projects and compile SDK products; also build SceneTransitions scripts on a full build |
| `run Hazelnut\|Nutella --config Debug\|Release [--project path]` | Stage resources/runtime and launch existing binaries; does not compile them or the selected project's scripts |
| `script-build project.hproj --config Debug\|Release` | Compile that project's C# against matching ScriptCore; atomically deploy its module to the descriptor's ScriptModulePath |
| `package --app Nutella --project project.hproj [--name Name] [--output directory]` | Validate saved content, existing Release modules/resources/native closure and create archives; does not build missing/stale application or project outputs |
| `editor-export project.hproj --name Name --output directory` | Incrementally build Release Nutella, ScriptCore and native/managed audit tools; build the selected project's Release scripts; then package Nutella |
| `database --config Debug\|Release` | Rebuild through Bear to capture Linux clangd commands; optional Bear is required |
| `test --config Debug\|Release --profile native\|software\|gl41` | Run the existing regression suite; build with `--tests` first |

Run these as `python3 scripts/hazel.py <command>` on Linux or `python scripts/hazel.py <command>` on Windows, from the checkout. Each subcommand has `--help`. Only Debug and Release are supported by the public CLI. A compiler, Python and source SDK are build/export requirements, not packaged-game requirements.

## Open an example and the UI Tutorial

```sh
python3 scripts/hazel.py script-build examples/Sandbox/Sandbox.hproj --config Debug
python3 scripts/hazel.py run Hazelnut --config Debug --project examples/Sandbox/Sandbox.hproj
```

Open **Assets/Scenes/UI Tutorial.hazel** in Content Browser and Play; the project keeps its Start scene as startup. [The tutorial guide](../examples/Sandbox/README.md) maps its five authored pages. [Runtime UI](runtime-game-ui.md) covers layout, styles/fonts, controls, owner events and bounded Undo; [storage/audio](runtime-storage-audio.md) covers player data. Other examples: [MeadowRun](../examples/MeadowRun/README.md), [Skybound](../examples/Skybound/README.md), [The Last Lightkeeper](../examples/LastLightkeeper/README.md), and [SceneTransitions](../examples/SceneTransitions/README.md) for focused scene transitions.

Hazelnut's **File > New Project…** creates editable native content without compiling scripts. Use **Project > Build Scripts** before scripted Play. **Project > Export Game** uses saved files and the editor-export sequence. Stop returns to the retained authored scene. Prepared source tools and interpreter selection live in **Edit > Editor Preferences**.

## Release build and export

After setup, this complete ladder builds the native Release products and selected project's scripts before packaging:

```sh
python3 scripts/hazel.py build --config Release
python3 scripts/hazel.py script-build examples/Skybound/Skybound.hproj --config Release
python3 scripts/hazel.py run Nutella --config Release --project examples/Skybound/Skybound.hproj
python3 scripts/hazel.py package --app Nutella --project examples/Skybound/Skybound.hproj --name Skybound --output dist/games
```

Alternatively, the targeted export sequence is:

```sh
python3 scripts/hazel.py editor-export examples/Skybound/Skybound.hproj --name Skybound --output dist/games
```

`--name` and `--output` are required for editor-export. Plain `package` defaults to both applications and the SceneTransitions project if app/project are omitted. It uses saved assets and existing Release outputs; build changed scripts yourself first. Exports reject missing/external dependencies and preserve the project's declared licensed assets/notices. Existing instance trees are detached prefabs; exporting does not propagate prefab edits into authored scenes.

## Outputs, settings and developer launchers

| Location | Contents |
| --- | --- |
| `bin/<Config>-<OS>-x86_64/<target>/` | Native executables/libraries and managed SDK products, plus staged runtime resources beside applications |
| `bin-int/<Config>-<OS>-x86_64/` | Native/managed SDK intermediates |
| `build/` | Generated tool/test projects, pinned tools/dependency sources/installs/downloads, project-script compilation workspaces and test evidence |
| Project `Assets/Scripts/Binaries/` | Example/template deployed script modules; the descriptor's ScriptModulePath is authoritative for other projects |
| `dist/` | Default application archives/checksums; `--output` selects another destination |

Root native Makefiles and some generated VS files live beside project definitions and are ignored. Nothing needs moving to use this workflow. Local `.vscode/` is user-owned: copy the maintained `scripts/internal/vscode/linux/` or `windows/` templates only when wanted, preserving existing files. F5 builds Debug through the same CLI; Hazelnut restores its last project, while Nutella's template intentionally selects SceneTransitions. Linux cppdbg uses optional GDB; `database` supplies optional clangd/Bear integration.

[Settings and resources](settings-and-resources.md) explains application profiles, player saves, caches and runtime roots. Preserve user preferences/session/workspaces/layouts; use disposable profiles/projects for tests.

## Next steps

- [Editor guide](editor-guide.md): hierarchy, prefabs, Save, Undo and Play/Stop.
- [Scripting](scripting.md): create, compile and assign C# behavior.
- [Sprite authoring](sprite-authoring.md) and [Runtime UI](runtime-game-ui.md).
- [Packaging](packaging.md): download verification, deployment contents and export.
- [Development and troubleshooting](development.md): focused checks and canonical CI coverage.
- [Platforms and dependencies](platforms-and-dependencies.md): support scope, licenses and remaining physical acceptance.
