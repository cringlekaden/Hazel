# Hazel

[![Linux build](https://img.shields.io/github/check-runs/cringlekaden/Hazel/master?nameFilter=Linux%20Debug%20and%20Release&label=Linux)](https://github.com/cringlekaden/Hazel/actions/workflows/c-cpp.yml)
[![Windows build](https://img.shields.io/github/check-runs/cringlekaden/Hazel/master?nameFilter=Windows%20Debug%20and%20Release&label=Windows)](https://github.com/cringlekaden/Hazel/actions/workflows/c-cpp.yml)

A learning project following [TheCherno's Hazel engine](https://github.com/TheCherno/Hazel), with separate Linux and Windows platform implementations. Both use GLFW for window creation; rendering uses OpenGL. The repository builds Hazel as a static library and runs it through the Sandbox application.

The badges show the Linux and Windows **build jobs** on `master`. CI builds Debug and Release, but does not open the application or test rendering.

## Requirements

| Platform | Tools |
| --- | --- |
| Linux x86_64 | GCC, GNU Make, Git, OpenGL and X11 development libraries |
| Windows x64 | Visual Studio 2022 with **Desktop development with C++** and a Windows SDK, Git |

The project uses a pinned Premake 5 development revision because its Premake scripts require features unavailable in the 5.0.0-beta8 release. The commands below build the same revision used by CI. If you already have a compatible `premake5`, you can use it instead.

## Clone

Clone with submodules on either platform:

```sh
git clone --recurse-submodules https://github.com/cringlekaden/Hazel.git
cd Hazel
```

For an existing clone without its submodules, run `git submodule update --init --recursive` from the repository root.

## Build on Linux

Install the native development packages. On CachyOS or Arch Linux:

```sh
sudo pacman -S --needed base-devel git util-linux-libs libx11 libxext libxrandr libxinerama libxcursor libxi libglvnd mesa gtk3 pkgconf
```

On Ubuntu 24.04:

```sh
sudo apt-get update
sudo apt-get install -y build-essential git uuid-dev libx11-dev libxext-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libgl1-mesa-dev libegl1-mesa-dev libgtk-3-dev pkg-config
```

From the Hazel repository root, build Premake, generate Makefiles, and build Debug:

```sh
git clone --filter=blob:none https://github.com/premake/premake-core.git ../premake-core
git -C ../premake-core checkout --detach 71f2d33946947e9cf704f00c24200381e360f593
make -C ../premake-core -f Bootstrap.mak linux PREMAKE_OPTS=--curl-src=none
../premake-core/bin/release/premake5 gmake
make config=debug -j2
./bin/Debug-linux-x86_64/Sandbox/Sandbox
```

Build Release with `make config=release -j2`. Its executable is `bin/Release-linux-x86_64/Sandbox/Sandbox`.

## Build on Windows

Open the **x64 Native Tools Command Prompt for VS 2022**. From that prompt, clone the repository using the commands under [Clone](#clone), then run these commands from the Hazel repository root:

```bat
git clone --filter=blob:none https://github.com/premake/premake-core.git ..\premake-core
git -C ..\premake-core checkout --detach 71f2d33946947e9cf704f00c24200381e360f593
pushd ..\premake-core
call Bootstrap.bat vs2022 "PREMAKE_OPTS=--curl-src=none"
popd
..\premake-core\bin\release\premake5.exe vs2022
msbuild Hazel.sln /m:2 /p:Configuration=Debug /p:Platform=x64
bin\Debug-windows-x86_64\Sandbox\Sandbox.exe
```

Build Release with `msbuild Hazel.sln /m:2 /p:Configuration=Release /p:Platform=x64`. Its executable is `bin\Release-windows-x86_64\Sandbox\Sandbox.exe`. You can also open `Hazel.sln` in Visual Studio.

## Project layout

| Path | Purpose |
| --- | --- |
| `Hazel/src/Hazel/` | Engine API, events, layers, and renderer |
| `Hazel/src/Platform/Windows/` | Windows window and input implementations |
| `Hazel/src/Platform/Linux/` | Linux window and input implementations |
| `Hazel/src/Platform/OpenGL/` | Shared OpenGL implementation |
| `Sandbox/src/` | Example application and rendering code |
| `Hazel/vendor/` | Pinned GLFW, spdlog, ImGui, and GLM submodules; checked-in GLAD |

The current Sandbox shaders use GLSL 3.30. Running Sandbox requires an OpenGL 4.2
core-capable graphics driver and a desktop session; CI only compiles and links
the projects.

## Upstream migration

The staged migration targets TheCherno/Hazel commit
`1feb70572fa87fa1c4ba784a2cfeada5b4a500db`. See the
[progress and resume instructions](docs/migration/PROGRESS.md),
[source comparison](docs/migration/COMPARISON.md),
[preservation inventory](docs/migration/PRESERVATION.md), and
[staged plan](docs/migration/PLAN.md).

The renderer foundation stage adds upstream camera/uniform-buffer APIs and
integer/matrix vertex attributes adapted for OpenGL 4.2. The complete editor,
scene, physics, text and scripting migration remains
pending in the documented stages.

To build the focused GPU verification executable, generate with
`premake5 --migration-tests gmake` (or `vs2022` on Windows), then use the normal
Debug/Release build commands above. Execution requires a working desktop and
OpenGL 4.2; CI compiles the executable on both platforms and checks its presence.

Stage 2 adds the upstream application specification, main-thread queue, typed
input/events, filesystem/buffers/UUID/timer and editor camera/math. Native Linux
file dialogs require GTK3 development libraries; Windows uses native wide dialogs.
Layers detach and release their resources before renderer and window shutdown.


The migration's shader toolchain is built from exact source revisions on both
Linux and Windows, rather than requiring a Windows Vulkan SDK. Install CMake
(3.19 or newer), Python 3 and Git, then run:

```sh
python3 scripts/dependencies/build-shader-tools.py --jobs 2
premake5 --migration-tests --shader-tools gmake
make config=debug -j2
```

On Windows use `python` and the `vs2022` Premake action, then MSBuild with `/m:2`.
The setup builds Debug and Release sequentially under ignored `build/dependencies`;
source pins are in `scripts/dependencies/shader-tools.json`, and each installed
configuration has a provenance manifest. Dependencies use the dynamic MSVC CRT.
`MigrationShaderToolsSmoke` verifies compilation/reflection and GLSL 410 generation;
it does not establish an engine OpenGL 4.1 or macOS runtime port.
