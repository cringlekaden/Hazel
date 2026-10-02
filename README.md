# Hazel

Linux/Windows adaptation of [TheCherno/Hazel](https://github.com/TheCherno/Hazel/tree/1feb70572fa87fa1c4ba784a2cfeada5b4a500db), pinned to `1feb70572fa87fa1c4ba784a2cfeada5b4a500db`. Includes Sandbox, Hazelnut, scenes/YAML, managed scripting/assembly reload, Box2D, and complete 2D/MSDF rendering. Migration status and verification are in [PROGRESS.md](docs/migration/PROGRESS.md).

Texture references, authored script fields, editor failure recovery and save semantics
are described in [authoring reliability contracts](docs/authoring-reliability.md).

## Dependencies

Clone master with `git clone --recurse-submodules https://github.com/cringlekaden/Hazel.git`. The completed migration remains available on `migration/upstream-1feb705` as history. Existing clones use `git submodule update --init --recursive`.

Builds use Python 3, Git, and current development Premake 5. CI pins Premake to `71f2d33946947e9cf704f00c24200381e360f593`; released beta8 lacks required APIs. **No CMake is required.** Shader dependencies compile through a project-owned Premake workspace with exact pins in [shader-tools.json](scripts/dependencies/shader-tools.json). Generated inputs, dependency checkouts, installed libraries and logs stay under ignored `build/`. Vendor sources remain clean.

Linux needs GCC/C++17, GNU Make, Mono development tools, pkg-config, GTK3, OpenGL and X11 development libraries. Example package lists (installation is a user operation):

```sh
# CachyOS/Arch
sudo pacman -S --needed base-devel git python mono util-linux-libs libx11 libxext libxrandr libxinerama libxcursor libxi libglvnd mesa gtk3 pkgconf
# Ubuntu 24.04
sudo apt-get install -y build-essential git python3 mono-devel uuid-dev libx11-dev libxext-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libgl1-mesa-dev libegl1-mesa-dev libgtk-3-dev pkg-config
```

Windows needs VS2022 with Desktop development with C++, Windows SDK, .NET Framework 4.7.2 development tools, Python and Git. The pinned upstream Mono SDK is included; native libraries consistently use MDd (Debug) and MD (Release/Dist), with UTF-8 source settings.

## Build

Use at most two build jobs, and run configurations sequentially. With `premake5` on PATH:

```sh
# Linux, from repository root
python3 scripts/dependencies/build-shader-tools.py --jobs 2
premake5 gmake
make config=debug -j2
make config=release -j2
```

GNU Make 4.4 supports `--jobserver-style=pipe` if its FIFO jobserver fails. A relocated Mono SDK can be supplied using `--mono-root=SDK_PREFIX`; provide an equivalent `CSC` compiler command when generating GNU Make projects for a relocated SDK. The local tested prefix/command is recorded in PROGRESS.md. Dependency setup also accepts `--premake=/path/to/premake5`.

```bat
rem Windows: x64 Native Tools Command Prompt for VS2022
python scripts/dependencies/build-shader-tools.py --jobs 2
premake5 vs2022
msbuild Hazel.sln /m:2 /p:Configuration=Debug /p:Platform=x64
msbuild Hazel.sln /m:2 /p:Configuration=Release /p:Platform=x64
```

If building Premake itself, check out the CI pin in a separate checkout. Linux uses `make -f Bootstrap.mak linux PREMAKE_OPTS=--curl-src=none`; Windows uses `Bootstrap.bat vs2022 "PREMAKE_OPTS=--curl-src=none"`. Pass that built executable to dependency setup or add its directory to PATH.

## Run

Launch from each application's source directory so its assets resolve:

```sh
(cd Sandbox && ../bin/Debug-linux-x86_64/Sandbox/Sandbox)
(cd Hazelnut && ../bin/Debug-linux-x86_64/Hazelnut/Hazelnut SandboxProject/Sandbox.hproj)
```

Windows follows the same working-directory rule, using `bin/Debug-windows-x86_64/.../*.exe`. Root builds compile Hazel-ScriptCore and SandboxScripts and deploy their assemblies to the editor/example project. Hazelnut supports a project path as its first argument; without it, the native project-open dialog is used.

`Sandbox --example-layer` runs the restored upstream generic-renderer example.

The OpenGL backend requires 4.1 core and preserves the tested native HD4000 4.2 features. Actual upstream GLSL450 shaders keep shaderc optimization, SPIR-V reflection and caching. Loaded core4.6 functions select native specialization; other contexts use generated GLSL410 with explicit reflected bindings. Original GLSL330/420 shaders retain direct compilation. Device limits determine batching/MSAA, including the HD4000 integer-multisample limit; single-sample entity picking and color/depth MSAA remain supported. Mesa overrides and software drivers belong exclusively to regression tests. This repository does not implement macOS or Metal support.

`ApplicationSpecification::Rendering` supplies local renderer settings. TextureSlots includes white and clamps to device/upstream capacity; PreferShaderBinaries can select GLSL loading; EnableDebugOutput applies when supported. Configure before application creation; Renderer::GetCapabilities()/GetSettings() report detected/effective values. These types extend the public upstream RenderCaps TODO locally.

## Verify and inspect

Generate with `premake5 --migration-tests --shader-tools gmake` (Windows: `vs2022`) to build the retained regression executables. Linux desktop checks run sequentially:

```sh
python3 scripts/migration/desktop-checks.py --config Debug --stage final
python3 scripts/migration/desktop-checks.py --config Release --stage final
```

The native profile clears inherited Mesa/software overrides. Additional profiles exercise Mesa4.1 and software rendering; they do not validate macOS. Windows CI compiles both configurations, runs CPU suites, and uses a checksum-pinned, process-local Mesa WGL package for graphics/editor/graceful-close tests. Evidence stays in ignored output or CI artifacts. Check PROGRESS.md for actual completed gates and remaining limitations.

OS-specific window/input/time/dialog/filewatch/command-line code is in `Hazel/src/Platform/Linux` and `Platform/Windows`. Graphics code is in `Platform/OpenGL`; common engine APIs, scene/project serialization and UTF-8 asset paths remain OS-independent. Root-owned integration scripts live in `scripts/dependencies`; exact upstream import records and licenses remain in `docs/migration` and vendor directories.
