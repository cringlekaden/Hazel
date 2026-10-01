# Migration progress

Last updated: 2026-09-30. Branch: `migration/upstream-1feb705`.
Baseline: `b030be7`. Fixed target: `1feb70572fa87fa1c4ba784a2cfeada5b4a500db`.

## Resume here

Read PLAN.md, PRESERVATION.md and COMPARISON.md. Check `git status` and submodule
cleanliness before edits. The upstream source checkout is in
`/tmp/hazel-upstream-1feb705`; if absent, clone TheCherno/Hazel and detach at the
fixed hash. Do not use a moving master. Do not push or merge. Build sequentially
with at most two jobs.

## Preparation status

- Inspected local tree before any write: clean master `b030be7`, four clean
  initialized submodules, existing ignored artifacts/configuration preserved.
- Dedicated branch created. Git metadata/display/network are restricted by the
  workspace permission profile, so those operations used explicit tool escalation.
- Downloaded real upstream Git source and verified its exact commit.
- Recorded full tracked-tree comparison and dependency pins, preservation
  inventory, staged plan and reproducible comparison script.
- Baseline `premake5 gmake`: passed with installed Premake 5.0.0-dev and the
  existing legacy-flags shim. Compiler GCC 16.2.1; GNU Make 4.4.1.
- Baseline `make config=debug -j2`: incremental pass.
- Baseline `make config=debug -j2 -B`: full compile/link pass, evidence in
  evidence/baseline-debug-full.log.
- Baseline `make config=release -j2 -B`: full compile/link pass (0), evidence in
  evidence/baseline-release-full.log.
- Desktop baseline Sandbox: startup/GLSL initialization completed on Intel HD
  4000, ran for ten seconds until timeout (124), no logged shader/startup errors.
  This is a startup smoke test, not visual validation or a clean shutdown test.
  Assets/imgui.ini were copied to /tmp/hazel-baseline-run; original files retained.
- A Release invocation briefly overlapped the final part of Debug; it was
  immediately interrupted (130) and restarted sequentially after Debug exit 0.
  All subsequent builds must remain sequential.
- Sandbox runtime inside the sandbox failed GLFW initialization (SIGTRAP, 133).
  `glxinfo -B` was also blocked there; outside sandbox it successfully identified
  accelerated Intel HD 4000 / Mesa 26.2.3 core 4.2, GLSL 4.20. Desktop rerun is
  necessary to distinguish display isolation from an engine failure.

## Implementation stages

Preparation checkpoint: `a20b1ba` — baseline evidence, comparison, preservation
inventory and plan, committed before any engine implementation changes.

Stage 1 implemented and Linux Debug/Release GPU checks passed; final Debug
rebuild after whitespace cleanup passed. Stages 2–9 pending. Windows MSVC
builds and runtime have not been run. Remote Actions coverage is retained and
extended to migration branches and compilation of the verification executable;
no remote workflow was triggered and no results are assumed.

## Stage 1 source provenance

Files copied from the actual target checkout, then adapted as described. The
upstream Apache 2.0 license is retained in UPSTREAM-LICENSE. Source changes carry
an adaptation notice; Camera.h is byte-identical to upstream.

| Path | Target blob | Adaptation |
| --- | --- | --- |
| Hazel/src/Hazel/Renderer/Camera.h | `9ab7ac3ca081ac7c2678f8106a2391c11320b960` | Exact upstream source |
| Hazel/src/Hazel/Renderer/UniformBuffer.h | `39d131f4e9788ac78749cf92afc259376414f567` | Local Core.h instead of future Base.h; explicit cstdint/std::uint32_t |
| Hazel/src/Hazel/Renderer/UniformBuffer.cpp | `e708d4f11fc91907a11cb80dbf9afd0935e0df53` | Same renderer API selection and CreateRef factory; explicit integer types |
| Hazel/src/Platform/OpenGL/OpenGLUniformBuffer.h | `8e70128fbf87f85e27c70dc1127601af66ff1c1a` | Explicit integer types |
| Hazel/src/Platform/OpenGL/OpenGLUniformBuffer.cpp | `870da78a95cc036724c68584dc085ccaaf426936` | glGenBuffers/bind/data/subdata for 4.2, generic binding restored, indexed binding preserved |
| Hazel/src/Platform/OpenGL/OpenGLVertexArray.cpp | `762041dcebe68571bdbfd3036e300b32fc7a9437` | glGenVertexArrays, integer pointers and instanced matrix columns retained; byte Bool maps to GL_UNSIGNED_BYTE because GL_BOOL is invalid for vertex pointers; uintptr_t offsets and None case retained |
| Hazel/src/Hazel/Renderer/Buffer.h | `94ffc68d4b8d2d24d6d2015aa1e64ff222a9216b` | Imported only target Mat3/Mat4 column counts (3/4) into otherwise preserved local header |

Hazel.h now exports Camera/UniformBuffer. No ownership/core, OS, ImGui, vendor
source or dependency-pin changes. Premake adds opt-in `--migration-tests` and a
separate verification project with the same CRT/UTF-8 settings as Sandbox.

## Stage 1 verification commands

```sh
premake5 --migration-tests gmake
make config=debug -j2
./bin/Debug-linux-x86_64/MigrationRendererSmoke/MigrationRendererSmoke
make config=release -j2
./bin/Release-linux-x86_64/MigrationRendererSmoke/MigrationRendererSmoke
```

Run sequentially. The focused executable requires a working desktop display and
creates a hidden OpenGL 4.2 core window. It checks UBO uploads/offsets and binding
preservation, float/integer/byte and Mat3/Mat4 attribute state, and actual instanced
GLSL 420 rendering with RGBA and integer entity readback. Its framebuffer is test
infrastructure; the upstream engine Framebuffer API remains stage 3 work.

On Windows, generate with `premake5 --migration-tests vs2022`, build via
`msbuild Hazel.sln /m:2 /p:Configuration=Debug /p:Platform=x64`, and execute
`bin\Debug-windows-x86_64\MigrationRendererSmoke\MigrationRendererSmoke.exe`.
Repeat Release. These Windows build/run commands have not been executed here.

Windows project generation on Linux passed with `--os=windows`. XML inspection
passed for /MDd Debug and /MD Release/Dist on all six native projects, /utf-8 on
Hazel/Sandbox/test (the spdlog consumers), Linux .cpp exclusion, and Windows/OpenGL
source inclusion. Evidence: stage1-premake-windows.log and
stage1-windows-settings.txt. GNU Make source lists select Linux and exclude
Windows .cpp. Generation and static inspection do not establish MSVC compatibility.

## Stage 1 results and next work

| Check | Actual result | Evidence |
| --- | --- | --- |
| Linux Debug compile/link | Pass (0), Hazel/Sandbox/verification executable | stage1-debug.log |
| Final Debug rebuild after whitespace cleanup | Pass (0), followed by passing GPU rerun | stage1-debug-final.log, stage1-gpu-debug.log |
| Linux Release compile/link | Pass (0), Hazel/Sandbox/verification executable | stage1-release.log |
| Debug GPU checks | Pass (0), Intel HD 4000 / OpenGL 4.2 | stage1-gpu-debug.log |
| Release GPU checks | Pass (0), same real driver | stage1-gpu-release.log |
| Sandbox Debug startup | Initialized and ran ten seconds, timeout 124; no logged shader/startup errors | stage1-sandbox-debug.log |
| Windows VS2022 generation and CRT/UTF-8/source inspection | Pass; generation/static checks only | stage1-premake-windows.log, stage1-windows-settings.txt |
| Core/OS/ImGui/event/vendor/pin preservation | No diff from baseline; all submodules clean; Camera exact target bytes | stage1-preservation-check.txt |
| GitHub Actions workflow | YAML parsed; both native jobs retained and test binaries added to output checks | .github/workflows/c-cpp.yml; not executed remotely |

Untested: MSVC compilation, Windows runtime, remote CI execution, visual Sandbox
validation and graceful Sandbox shutdown. The GPU probe does exercise resource
destruction while its context is alive and reports no OpenGL error, but it is not
an Application/LayerStack lifecycle test. No stage 2–9 functionality is claimed.

Next implementation: stage 2 core/platform architecture after reviewing the
remaining Windows gate. Keep the Scope-owned LayerStack when adapting upstream
Application; coordinate Base/Core/assert/event/input renames, avoid duplicate
OnAttach, investigate destruction order and the duplicate Windows glfwInit,
and implement platform utilities separately. Do not copy target raw layer owners
or Win32-only implementations onto Linux. Obtain Windows build evidence when a
Windows host is available; do not push merely to trigger CI.

Stage 1 checkpoint subject: `Import upstream renderer foundations for OpenGL 4.2`.
Resolve its hash with `git log -1 --format='%h %s' -- tests/migration`. The
preparation checkpoint is `a20b1ba`. Both are local checkpoints on the migration
branch; no changes were pushed or merged. The checkpoint working tree should be
clean apart from ignored/generated artifacts.

## Evidence policy

Logs under evidence/ are actual command output. Distinguish full and incremental
builds, timeout-limited startup and clean shutdown, project generation and MSVC
execution, and synthetic checks from real GPU rendering/readback. Logs and tree
comparison are a snapshot; update this document as each gate completes.
