# Migration progress

Last updated: 2026-10-01. Branch: `migration/upstream-1feb705`.
Baseline: `b030be7`. Fixed target: `1feb70572fa87fa1c4ba784a2cfeada5b4a500db`.

## Resume here

Stage8 complete locally: full Debug/Release builds and twelve desktop checks per
configuration passed. Actual docked editor/panels, Unicode paths, save/reopen,
duplicate, gizmo shortcuts/drawing, play/pause/step/simulate/stop and shutdown pass
on native Intel 4.2, forced Mesa 4.1 and software 4.6. Physical mouse drag/file-dialog
interaction remain untested. Stage8 checkpoint and branch Windows compile/software
runtime CI are next. Continue stage9 and all final acceptance work.

Stage7 correction aaf145a/Actions36930517894 passed Windows builds and CPU runtime;
Linux CI is still finishing. Stage6 c86d1dd/Actions36927686042 passed both OS/config
builds and CPU suites. Windows graphics runtime is pending the new test-only CI
runner and must not be described as verified yet.

Current implementation changes are migration-owned; no unrelated work found.
Proceed through stage8 and stage9/final acceptance without optional pauses. Do
not merge into master. No local build/test/debugger process is running. New raw logs remain ignored.

Local dependency prefix: `--mono-root=build/dependencies/mono/linux/usr`.
Gmake flags: `--migration-tests --shader-tools`; builds use
`make config=debug|release -j2 --jobserver-style=pipe` with CSC set to the relocated
Mono executable, `--config` build/dependencies/mono/linux/etc/mono/config and
build/dependencies/mono/linux/usr/lib/mono/4.5/mcs.exe. Desktop runtime:
`python3 scripts/migration/desktop-checks.py --config Debug|Release --stage stage8`.
Routine logs go to ignored build/migration/evidence. Read PLAN.md, PRESERVATION.md
and COMPARISON.md and check status/submodules before edits. Actual pinned upstream
source is /tmp/hazel-upstream-1feb705.

Final acceptance remains unmet: replace CMake shader dependency builds with
validated Premake-only integration, consolidate capability/settings detection,
audit shader selection/corrupt caches, complete editor and parity gates, clean
tracked raw logs and consolidate concise provenance/limitations records, then
fresh Linux/Windows builds and relevant runtime checks. Keep native Grandpa 4.2,
separate test overrides/software profiles, clean dependencies, both OS coverage,
future Mac/Metal architecture and all upstream features. No macOS implementation
or validation is claimed. Upstream remains pinned to the hash above.

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

Stage 1 checkpoint `81b3861` and stage 2 checkpoint `92eb60a` have passed
Linux and Windows Debug/Release compilation in Actions. Stage 2 also passed
local Debug/Release core, renderer and graceful Sandbox shutdown runtime checks
on Intel HD 4000/OpenGL 4.2. Windows engine runtime is untested. Stage 3 backend integration is locally
verified in Debug/Release on native 4.2, forced 4.1 and software 4.6; integrated
Linux/Windows Debug/Release compilation and optimized CPU shader tests passed CI.
Stage 4a foundation and stage 4b font prerequisite passed both OS builds/CPU
runtime; stage 4c complete Renderer2D prerequisites are being verified. Stages 4–9 remain incomplete. The sections below retain initial-session evidence, followed by
resumed-session results that supersede the initial unrun CI status.

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

Initial-session next implementation (completed in the resumed session below):
stage 2 core/platform architecture after reviewing the
remaining Windows gate. Keep the Scope-owned LayerStack when adapting upstream
Application; coordinate Base/Core/assert/event/input renames, avoid duplicate
OnAttach, investigate destruction order and the duplicate Windows glfwInit,
and implement platform utilities separately. Do not copy target raw layer owners
or Win32-only implementations onto Linux. Obtain Windows build evidence when a
Windows host is available. This initial-session no-push restriction was
superseded by the resumed user instruction authorizing migration-branch CI pushes.

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

## Resumed scope and verified stage 1 CI

Resumed at clean `81b3861`, with unchanged clean dependency pins. Verified via
GitHub API that [Actions run 36801823201](https://github.com/cringlekaden/Hazel/actions/runs/36801823201)
completed successfully for `81b38617fb7fbdbd65d861929c69537332077c0a`:
Linux Debug/Release and Windows Debug/Release compilation passed. Raw job data
is in evidence/stage1-actions.json. This supersedes the earlier unrun compilation
status, but **Windows runtime remains untested**.

The user authorized stages 2–9 without stopping for checkpoint confirmations
and migration-branch pushes for Windows compilation CI. Preserve the fixed
upstream, ownership, official ImGui, platform/API separation and pin decisions.
No master merge. Keep future macOS portability in APIs/data formats without
adding speculative platform code or claiming support. Stage 3 will evaluate a
real GLSL/OpenGL 4.1 fallback while retaining Grandpa's tested 4.2 behavior;
4.2 context hints/assertions stay until that path is actually implemented/tested.

Stage 2 started: target core/platform/camera/math imports recorded by blob in
evidence/stage2-imports.json. Baseline graceful-close experiment uses a copied
Sandbox working directory and normal X11 WM_DELETE_WINDOW, not forced termination.

Stage 2 early investigation: baseline normal-close returned 0, but shutdown trace
showed Renderer2D shutdown before Sandbox/ImGui OnDetach. Explicit Scope LayerStack
Clear now detaches/destroys layers while renderer, ImGui (for user layers), and
graphics context remain alive; renderer shuts down before window/context. Clear
is idempotent and resets the insertion index. Windows glfwInit was called twice
inside the first-window branch; it now runs once with the error callback installed
before initialization. The first Debug attempt found missing KeyEvent includes
in the new focused test (not engine failure); corrected explicitly. Removed an
unused upstream math variable warning.

Native dialogs: GTK3 only in Platform/Linux (new native build dependency), wide
Win32 only in Platform/Windows, UTF-8 public paths, standard filesystem in common
FileSystem. Common scene/project path strings will use UTF-8 and relative/generic
paths when those stages arrive. Future macOS requires a native dialog adapter;
no GTK/Win32 types enter common APIs. Typed input has separate native implementations;
Unicode character events retain 32-bit codepoints rather than upstream 16-bit
key IDs. OpenGL context hints now live in Platform/OpenGL, selected by the
graphics-context factory; native window files contain no OpenGL-version literals.

GCC also exposed upstream Timer.h extra class qualifications on in-class methods;
removed those nonstandard qualifications and retained monotonic steady_clock.
The immediate retry was interrupted to apply this already-known correction
before compiling the test; no overlapping builds.

Final lifecycle review also found that queued callbacks can retain GPU resources
until member destruction after window reset. Cancelled queue captures now release
before layer/renderer/window teardown, with a focused destructor probe. Restored
explicit Ref logger types and separated the existing ownership helpers into
Memory.h to make Base/Log/Assert headers self-contained without include cycles.
PCH now includes upstream Base.h (as target hzpch.h does). An optional GLM
PCH include was tried and removed during the investigation below. The first Release attempt
was interrupted before completion for these final changes; builds remain sequential.

The full PCH rebuild exposed a namespace lookup collision between new assertion
helpers in Hazel::detail and upstream math's unqualified detail::scale. Qualified
the intended glm::detail::scale calls; no vendor changes. Earlier math smoke
checks had passed without Base in this translation unit's PCH.

GCC 16 Debug also rejected stb_image SSE immediate operands after GLM's SIMD
headers were placed in the PCH. Removed that optional GLM PCH include (upstream
PCH includes Base, not GLM); vendor source remains untouched and SIMD is retained.
A few preliminary Debug runtime checks overlapped the interrupted first Release
attempt; the final Debug-build/runtime then Release-build/runtime verification
sequence is strictly serial, with at most -j2.

Removing GLM from PCH alone did not solve the GCC 16 SSE failure. An isolated
compile of the unchanged stb_image.cpp without forced PCH passed at -O0. Verified
the installed development Premake exposes enablepch (legacy flags is absent).
The root-owned stb_image file filter now uses enablepch Off; the wrapper still
includes hzpch.h normally, SIMD stays enabled, and vendor source is unchanged.
The engine retains its upstream-style Base PCH.


## Stage 2 verification and checkpoint

Final verification ran sequentially: Debug build, Debug runtime checks, Release
build, Release runtime checks, with `make --jobserver-style=pipe config=debug -j2`
and then `config=release -j2`. The pipe jobserver avoids stale FIFO warnings after
an interrupted local Make 4.4 build; CI does not need this local option.

| Check | Actual result | Evidence |
| --- | --- | --- |
| Linux Debug compile/link | Pass (0), final PCH rebuild across the recorded attempts | stage2-debug-pass.log and preceding failure logs |
| Linux Release compile/link | Pass (0), final rebuilt engine and all three clients | stage2-release-final.log |
| Core Debug/Release runtime | Pass (0), real HD 4000 context; layer ownership/partitions, Unicode events, owning buffers, filesystem, UUID, timer, math/camera, arguments, native input, reentrant queue, repeated application creation, live ImGui/renderer/context during user detach and queued capture destruction | stage2-core-debug.log, stage2-core-release.log |
| Renderer Debug/Release regression | Pass (0), stage 1 UBO and integer/matrix vertex attributes and actual color/entity readback | stage2-renderer-debug.log, stage2-renderer-release.log |
| Sandbox normal close Debug/Release | Pass (0), WM_DELETE_WINDOW; detach user layers then ImGui, shut down renderer, destroy window | stage2-graceful-after-debug.log, stage2-graceful-after-release.log |
| Windows VS2022 generation and inspection | Pass; seven native projects keep /MDd Debug and /MD Release/Dist, UTF-8 consumers, Windows-only OS sources, and stb_image without forced PCH | stage2-windows-generation.log, stage2-windows-settings.txt |
| Vendor preservation | Four original submodules clean at unchanged pins; stb_image unchanged | git submodule status / foreach status inspected before checkpoint |

Native file dialog code compiled on Linux, but interactive open/save selection,
cancellation and Unicode path behavior have not been exercised. Windows runtime
is untested. Windows compilation for this stage is pending the authorized branch
push and Actions result; VS generation is not a substitute.

Checkpoint subject: `Import upstream core and platform architecture with safe shutdown`.
Source provenance: evidence/stage2-imports.json. Continue with stage 3 after the
Windows compilation result; preserve all earlier ownership and platform decisions.


Stage 2 checkpoint: `92eb60a000008df81a93a033c327e075e4fb34ca`, pushed only to
`migration/upstream-1feb705`. [Actions run 36807370723](https://github.com/cringlekaden/Hazel/actions/runs/36807370723)
completed successfully: Linux and Windows Debug/Release compile/link and expected
output checks passed. evidence/stage2-actions.json records the exact head and job
results. These jobs compiled the smoke executables but did not run them;
**Windows runtime remains untested**. Stage 2 compilation gate is satisfied.

## Stage 3 dependency preparation

Started after the stage 2 CI pass. No existing vendor pin changes. Evaluate
shaderc v2023.6 (`39aa522785f130130927cd4766a37e8813af6d66`) with its exact DEPS
revisions and SPIRV-Cross Vulkan SDK 1.3.268.0
(`2de1265fca722929785d9acdec4ab728c47a0254`), contemporary with the pinned Hazel
checkpoint. Build static shader tools from clean source outside vendor in ignored
build/dependencies; use the same source pins on Linux and Windows, dynamic CRT
with configuration-matched Debug/Release libraries, and at most two build jobs.
The installed Linux shaderc 2026.3.1 is deliberately not mixed with an older Cross
build. Test compilation/reflection/GLSL generation before integrating the pipeline.

CMake was absent locally. Downloaded the official 3.31.6 Linux x86_64 archive to
/tmp and verified SHA-256 against the official release manifest:
`5a1133ff103c71eb5120e2cc3de922733e7d8a26a98ae716397e8676adb367bf`.
Temporary tool path: /tmp/cmake-3.31.6-linux-x86_64/bin/cmake. No system install.

Concrete future-portability obstacles identified for stage 3: target DSA texture
and framebuffer operations, glClearTexImage, glSpecializeShader and GLSL 450,
plus 420 explicit resource bindings and depth texture storage if supporting 4.1.
Use bind-based equivalents, manually bind reflected resources, and generate GLSL
410 where viable. Context hints/assertions still require 4.2 at this point; no 4.1
or macOS support is claimed. Backend tests must prove functionality before lowering
the requirement. Keep the native 4.2 regression tests intact.


Stage 3 dependency Debug attempt failed in glslang SpvBuilder.h: GCC 16 no longer
provides uint32_t through unrelated standard headers. Root-owned
scripts/dependencies/shaderc-compat.cmake supplies cstdint only to glslang's SPIRV
target under GCC/Clang, deferred until that target exists. No pinned vendor edits,
optimizer disablement or feature reduction. Requires CMake 3.19+ for DEFER.
Failure output: evidence/stage3-tools-debug.log. Retry uses the same build tree.
The shallow SPIRV-Tools version generator also logs a git describe failure and
contains a broken rev-parse fallback; it continues with a date. SOURCE_DATE_EPOCH
now fixes that date to the shaderc commit timestamp, while the installation
manifest carries all exact source hashes. This diagnostic is not a compile failure.


Stage 3 Debug pinned tool build and independent toolchain runtime test passed (0):
evidence/stage3-tools-debug-fixed.log, stage3-tools-smoke-build-debug.log and
stage3-tools-smoke-debug.log. All five source trees were checked clean by the
builder. Release tool build is in progress. The build only selects required
library targets; shader compilation/optimization/reflection features are retained.
Vendor example/test executables are not part of the engine dependency build.

Engine stage 3 adaptations are being prepared alongside this tool-only work and
are not yet verified: texture specifications/formats/mips and UTF-8 memory loading;
framebuffer integer picking/full clears/depth-only/MSAA resolving; target shaderc
and Cross pipeline with source-dependent cache keys, always-regenerated GLSL 410,
manual reflected UBO/sampler binding, checked failure cleanup, and a gated native
OpenGL 4.6 SPIR-V program path. Existing GLSL 330/420 direct compilation remains
for shaders using default uniforms (including the original Sandbox and stage 1
regression test). No engine 4.1 requirement change yet. Target source blob records
are in evidence/stage3-imports.json; these pending engine files must not be included
in the earlier tool-only checkpoint until engine builds and GPU checks pass.


## Stage 3a dependency checkpoint

The dependency-only checkpoint deliberately excludes the pending engine backend
files and RendererFeaturesSmoke. Stage 2 engine source remains the buildable
engine in this checkpoint. All tool builds and runtime checks ran sequentially,
with at most two compiler jobs.

| Check | Actual result | Evidence |
| --- | --- | --- |
| Debug shaderc/Cross libraries | Pass (0), after the investigated GCC header correction | stage3-tools-debug-fixed.log |
| Debug tool test compile/link and runtime | Pass (0), Vulkan/OpenGL SPIR-V, reflection, GLSL 410 without 420pack, invalid-input diagnostics | stage3-tools-smoke-build-debug.log, stage3-tools-smoke-debug.log |
| Release shaderc/Cross libraries | Pass (0) | stage3-tools-release.log |
| Release tool test compile/link and runtime | Pass (0), same coverage | stage3-tools-smoke-build-release.log, stage3-tools-smoke-release.log |
| Dependency source cleanliness and exact revisions | Pass, checked by builder after each configuration | stage3-tools-manifest-debug.json, stage3-tools-manifest-release.json |
| VS2022 generation and tool test CRT/UTF-8 inspection | Pass, generation/static checks only | stage3-tools-windows-generation.log, stage3-tools-windows-settings.txt |

The retained Linux/Windows workflow now builds both library configurations before
Premake, with two jobs, and runs the CPU shader-tool test on both hosts. Installed
libraries are cached by OS/toolchain plus the pins/script/compatibility-hook hash.
A 90-minute job limit accommodates first-time source builds; parallelism remains
limited to two. Windows shader-tool compilation/runtime is pending CI. Windows
engine/GPU runtime is untested. No engine OpenGL 4.1 runtime is claimed yet.

Checkpoint subject: `Pin and verify portable shader compilation dependencies`.
Continue directly with engine stage 3 integration, local Debug/Release builds and
real 4.2/4.1 GPU checks; investigate CI/tool failures before advancing to stage 4.


Stage 3a checkpoint: `b0bc147`, pushed to the migration branch for tool/engine
compilation CI and the CPU tool runtime test. Engine backend integration is now
included in the local build generator, with tool headers on Hazel and matching
static libraries linked by clients. RendererFeaturesSmoke owns an isolated
unique temporary fixture directory, so its cache-damage tests cannot overwrite
existing project assets/caches. Run native 4.2 first, then the Mesa 4.1 override.


Stage 3a Actions run 36865262516 Windows job failed after successful native shader
library compilation: the installer expected spirv-cross-glsl.lib but upstream
Cross emits spirv-cross-glsld.lib (and cored.lib) for MSVC Debug. Normalize the
copied install name within the configuration-specific directory; retain /MDd and
unmodified vendor source. Failure log: stage3-tools-windows-failure.log. No Windows
tool runtime or Hazel compilation ran in that failed job. Linux job still running.

Initial engine Debug build passed (stage3-debug.log). The first GPU feature test
failed in its fixture, not an unsupported GL call: its GLSL 450 shader used
OpenGL gl_VertexID, which Vulkan-targeted shaderc expects as gl_VertexIndex.
Replaced the fixture with actual target-style vertex attributes and engine VAO/
vertex buffers. Legacy GLSL 330/420 direct compilation retains gl_VertexID support.
Failure evidence: stage3-features-debug.log. Added depth-occlusion readback coverage
for both single-sample and multisample attachments before the next run.


The next GPU run rendered the entity ID correctly but failed color readback.
Investigation of the pinned shaderc compiler.cc showed that its performance
optimization profile inserts StripDebugInfo unless GenerateDebugInfo is enabled;
this removed original sampled-image names used by manual GLSL binding. Enable
GenerateDebugInfo while retaining performance optimization, and include that
policy in the cache contract (v2). Extend the CPU tool test to cover optimized
resource-name preservation as well. Failure evidence: stage3-features-debug-fixed.log.
The framebuffer/shader stage remains unverified until rerun; no assertion/version
change was used to bypass the failed test.


Stage 3 GPU investigation now verifies the optimized names fix: correct color
128,64,192,255 and entity ID 73, followed by passing single-sample depth/clear/
resize checks. Four-sample combined integer picking hit a real hardware limit:
GL_MAX_SAMPLES/color/depth texture samples=8, GL_MAX_INTEGER_SAMPLES=0. A diagnostic
GL_R32I four-sample renderbuffer also failed with GL_INVALID_OPERATION (1282),
so switching storage kinds cannot fix it. Evidence: stage3-features-debug-names.log,
stage3-msaa-probe-build.log, stage3-msaa-probe.log.

Retain integer MSAA allocation/resolve on capable GPUs, explicitly reject unsupported
requests with the reported limit, and test color/depth MSAA plus single-sample
integer picking on HD 4000. Do not silently downgrade sample counts or remove
MSAA/picking. Run the combined path on a software GL context as additional evidence.
The target editor uses single-sample framebuffer attachments; if future Linux/
Windows editor MSAA is enabled on this device, a separate picking draw pass is the
likely adaptation. This is a hardware obstacle, unrelated to speculative macOS code.

Stage 3a fix checkpoint: `ba10ef8`, pushed only to migration branch.
[Actions run 36867983531](https://github.com/cringlekaden/Hazel/actions/runs/36867983531)
passed both OS Debug/Release library and engine compilation and executed the CPU
shader-tool test successfully on both OSes. Raw step evidence is in
stage3-tools-fixed-actions.json. This verifies **Windows shader-tool CPU runtime**;
Windows engine/window/input/dialog/GPU runtime remains untested. The later optimized
name test and pending backend integration are not part of that CI checkpoint.


Corrected Debug GPU feature checks passed (0) on accelerated HD 4000 native 4.2
and a process-local Mesa 4.1/GLSL 410 override: stage3-features-debug-capabilities.log
and stage3-features-debug-gl41.log. Both prove texture formats/mips/UTF-8/uploads,
manual bindings, cache paths, color/integer/depth readback, clears/resize and
color/depth MSAA. The override is a Mesa compatibility test, not a macOS run.

A separate software context reported OpenGL 4.6 and integer sample limit 8, and
passed both single-sample and four-sample color/entity/depth tests (0):
stage3-features-debug-software.log. It exercised native SPIR-V specialization
(the GLSL fallback branch did not execute), including warm and damaged caches.
Added explicit backend-mode and renderer-name logging for final evidence.

Only after these operation/shader/readback checks did OpenGLContext's hints and
release/debug version guard change to minimum 4.1. Final Application/Sandbox tests
under that minimum and both build configurations are next. No macOS port or runtime
support is implemented or claimed. Grandpa's native 4.2 checks remain in the suite.


### Stage 3 final local verification

The new root-owned `scripts/migration/desktop-checks.py --config Debug` runs a
fixed test matrix sequentially and records each subprocess exit. All nine checks
passed (0): native 4.2 features/core/graceful Sandbox/foundation/CPU tools; forced
4.1 features/core/graceful Sandbox; software 4.6 features. Logs use
`stage3-debug-{native,gl41,software}-*.log`. Explicit renderer/mode logging proves
Intel hardware for native/4.1, llvmpipe for 4.6, GLSL 410 fallback for older contexts,
and native OpenGL SPIR-V specialization for 4.6. The preserved foundation test
still compiles/renders GLSL 420 and checks integer/matrix attributes on native 4.2.
Core repeats application create/destroy; Sandbox returns 0 after WM_DELETE_WINDOW
with layer -> ImGui -> renderer -> window teardown. Its X11 window lookup now
requires the spawned PID as well as title, preserving unrelated desktop windows.

Debug final build passed (stage3-debug-final.log). Release full integration build
also passed (0), followed by all nine Release runtime checks (0):
`stage3-release.log` and `stage3-release-{native,gl41,software}-*.log`. Command:
`make --jobserver-style=pipe config=release -j2`, then
`python3 scripts/migration/desktop-checks.py --config Release`. The pipe jobserver avoids GNU Make 4.4's
stale FIFO from a previously interrupted build; vendor Makefiles are retained.

VS2022 generation and XML inspection passed for all nine native projects: /MDd
Debug, /MD Release/Dist, applicable UTF-8 options, Windows sources included/Linux
translation units excluded, OpenGL kept separate, and matching configuration
shader libraries linked by final applications. Evidence: stage3-vs2022.log and
stage3-vs2022-inspection.txt. This is project inspection, not an MSVC build or
Windows engine runtime. New branch CI will compile the integrated backend and
feature tests on both OSes and run the extended optimized-resource-name CPU test.

`.gitattributes` disables whitespace warnings only for raw evidence logs so actual
compiler/Actions output can be retained byte-for-byte; source/script/document
whitespace checks remain active.

### Stage 4 dependency inspection

The actual target Scene.cpp directly calls Renderer2D's camera/sprite/circle/text
APIs, Box2D and ScriptEngine. Components.h initializes TextComponent with the
default Font; SceneSerializer.cpp requires Project asset roots and ScriptEngine
field reflection/persistence. Those implementations were originally assigned to
stages 5–7. A monolithic stage-4 import cannot link against the current engine.
Do not replace these functions with stubs or remove component/serialized fields.

Split stage 4 into buildable prerequisites: camera/ECS/YAML and filewatch source
inventory first, then introduce the needed font/rendering, physics and managed
script/project dependencies in explicitly recorded sub-checkpoints, followed by
full Scene/entity/serialization integration and its round-trip/runtime gates.
Stages 5–7 retain their dedicated completeness and behavior verification gates.
The pinned target source and public component/asset formats remain the authority.
This dependency ordering is an implementation subdivision permitted by PLAN.md;
no stage is declared complete before its required features and checks pass.


Stage 3 local exit gates passed: two sequential full engine builds, both nine-test
runtime matrices, generated Windows project inspection and clean vendor/source
pins. Expected rejection/invalid-shader diagnostics in passing test logs are
intentional negative checks; every runtime subprocess returned 0. Release retains
exceptions and validation when assertions are disabled. Shader-tools optimized
name preservation is verified in both configs. MSAA integer limit is reported,
not silently reduced; software 4.6 covers the retained combined path.

Checkpoint subject: `Integrate upstream shader, texture and framebuffer features`.
Push only this migration branch for integrated Windows compilation/CPU test CI.
No Windows engine, GPU, dialog or macOS runtime has been executed or claimed.
Continue directly with stage 4 prerequisites; record CI result before declaring
cross-platform stage 3 compilation verified.


Stage 3 checkpoint: `25362a8aab4ac395b49b4d694f165088a89b1361`.
[Actions run 36881572888](https://github.com/cringlekaden/Hazel/actions/runs/36881572888)
completed successfully for both Linux and Windows Debug/Release, including all
integrated backend/test compilation and optimized-resource-name CPU shader-tool
runtime checks. Raw job/step evidence: stage3-actions.json. Windows CPU tool
runtime is verified; Windows engine/window/input/dialog/GPU runtime is untested.
Stage 3 compile gates are complete. No merge into master.

### Stage 4a camera/ECS/YAML foundation

Started from a clean tree after stage 3 commit/push. Exact target SceneCamera,
EnTT amalgamation/license and filewatch header were copied; blob provenance is
stage4a-imports.json. YAML is a new clean submodule at exact target gitlink
`25be1f208665b9732ba40aa7b05ec2da97104192` (0.6.3); actual vendor Premake inspected.
A root-owned project sets absolute output paths, C++17, dynamic MSVC CRT and
Debug/Release/Dist consistently, leaving vendor scripts/source unchanged.

Target SceneCamera initializes aspect ratio to zero then recalculates, producing
a non-finite default projection. Initialize to 1, preserve perspective/
orthographic behavior and ignore minimized (zero-size) viewport updates, retaining
the last valid projection. The CPU foundation test checks these, target EnTT
views/entity generations/lifetimes and YAML UUID/UTF-8/component/camera values.
This is dependency/component-shape round-trip coverage, not yet a SceneSerializer
or full Scene runtime test. Those gates remain stage 4 completion work.

First Debug build failed (2): target YAML emitterutils.cpp lacks the integer
header and GCC 16 no longer supplies uint16_t/uint32_t transitively. Evidence:
stage4a-debug.log. Root-owned scripts/dependencies/yaml-compat.h supplies cstdint
and its two global names through portable Premake forceincludes, scoped to the
YAML dependency project, preserving the pin and clean source. A file-specific
forceincludes attempt was ignored by this gmake generator (actual emitted
FORCE_INCLUDE was empty; stage4a-debug-fixed.log repeated the failure), so the
project-scoped standard header is used on both OSes. Emitted flags were checked
before the final Debug retry. Final Debug and Release builds passed (0), followed
sequentially by the foundation CPU test, core lifecycle and graceful Sandbox
checks in each config (all 0). Evidence: stage4a-debug-final.log, stage4a-release.log,
`stage4a-{debug,release}-native-*.log`. Commands: Premake with --migration-tests
--shader-tools gmake; make --jobserver-style=pipe config={debug,release} -j2;
python3 scripts/migration/desktop-checks.py --config {Debug,Release} --stage stage4a.

Concrete future-port obstacle: the target filewatch header's `__unix__` path
includes Linux sys/inotify.h; it provides Win32 and Linux implementations, not a
macOS watcher. Keep its exact source now; a future port needs a native watcher
adapter (kqueue/FSEvents) in OS-specific code, without changing scene/script APIs
or serialized paths. No speculative macOS implementation is added.


Stage 4a Windows generation/XML inspection passed for eleven native targets:
correct dynamic CRT per config, UTF-8 for applicable projects, YAML forced header,
OS source separation and SceneCamera inclusion. Evidence: stage4a-vs2022.log and
stage4a-vs2022-inspection.txt. CI now compiles these on both OSes and executes the
CPU camera/ECS/YAML test in both configs; Windows CI result pending next push.
Windows engine/GPU runtime remains untested.

Verified copied vendor blobs are byte-identical to the target and licenses remain
included. YAML source is clean at the exact target pin; original four pins are
unchanged. FileWatch.h retains upstream trailing whitespace under its narrowly
scoped .gitattributes entry; adapted engine/scripts/docs retain whitespace checks.

Checkpoint subject: `Import and verify upstream scene foundation dependencies`.
Stage 4a is buildable and locally verified; **stage 4 as a whole is incomplete**.
Continue directly with the actual target font dependency/API prerequisite, then
renderer, physics, managed scripting/project dependencies and full scene integration.
No feature stubs or reduced serialization format were introduced.


Stage 4a checkpoint: `738fb321735460057c85a44eadc66bdb973c85d1`, pushed only
migration branch. Actions run 36886198003 completed: Linux Debug/Release build
and CPU shader/foundation runtime passed; Windows Debug failed before linking,
so Windows Release and CPU runtime were not run. Raw metadata: stage4a-actions.json;
full compiler/job output: stage4a-windows-failure.log.

Failure investigation: MSVC C1083 could not resolve the generated relative
forced-include path `../../scripts/dependencies/yaml-compat.h`. The file is tracked
and present; /FI include searching differs from GNU Make. Keep the standard
header fix and source pin. For Visual Studio only, emit an explicit /FI path
rooted at MSBuild's $(SolutionDir), removing the relative forceincludes entry.
Linux retains its verified GNU Make flag. Generated VS XML confirms absolute
macro-rooted /FI, /utf-8, /MDd and /MD in all configs; evidence:
stage4a-vs2022-forced-include-fix.log. No Windows compile pass is claimed yet.

Font prerequisite imports/local Debug build were prepared while CI was pending;
that independent uncommitted work is retained while this stage 4a failure is
resolved. Do not advance that checkpoint before the CI fix is tested. Checkpoint
subject: `Resolve YAML compatibility include from Windows solution root`.


Stage 4a path-fix checkpoint: `8948e89b7df9fc9a7a256a3d9c78827cc1303305`,
pushed only migration branch. CI run 36889492677 is pending. The commit contains
only the YAML path fix and stage 4a failure evidence; separate prepared font
changes were preserved in the working tree/index, not included in that checkpoint.

### Stage 4b font/component prerequisite (in progress)

Actual target Font.h/.cpp, MSDFData.h and Components.h imported; exact source/asset
blobs and relocation to Sandbox's runtime assets are in stage4b-imports.json.
All target OpenSans variants and their license are retained. Original Sandbox
assets were not overwritten. New submodule matches target atlas pin
`b50e101d24b1f6009841ce3a386e1bc9365dc66a`; nested msdfgen
`b9061f976e79fcc0b20ac6fcd5abaa8fafaf6f91`, FreeType (2.11.0)
`2d57b0592805c76d676b51fbf9553de71c5a5c78`, dlg
`d142e646e263c89f93663e027c2f0d03739ab42d`. Actual vendor Premake/source inspected;
all four clean pins verified in stage4b-font-pins.json. Keep exact source and
existing glyph generation, overlap/scanline processing, metrics and Latin-1
charset. No dependency pin was silently upgraded.

Root-owned scripts/dependencies/fonts.lua includes the target build scripts then
sets absolute generated/output paths, C++17 where appropriate, dynamic MSVC CRT,
UTF-8 and Debug/Release/Dist consistently. FreeType remains C. Archives stay
separate; final applications link atlas -> msdfgen -> FreeType. Build and font
atlas worker concurrency are capped at two (atlas target default was eight).
The existing Premake shim is unchanged. Vendor Makefiles are retained.

Concrete lifecycle adaptation: the target function-static default Font retains a
GPU texture past application/context shutdown. Keep shared default-font behavior
but move that owner to an explicit cache reset by Renderer2D::Shutdown. Core
stage-2 layer -> ImGui -> renderer -> window ordering is retained. MSDF state uses
Scope, and a local RAII guard releases both FreeType handles on success/failure.
Load actual pinned msdfgen::loadFontData from FileSystem bytes rather than a narrow
OS filename, supporting native UTF-8 paths through the shared filesystem API.
Missing/invalid fonts and atlas failures throw in Debug and Release instead of
leaving an unusable partial Font. No OS or OpenGL version branch is added to Font,
components or asset formats; atlas uploads use the existing Texture API/backend.

All actual target component shapes are present, including text, scripts and both
physics collider kinds. Native script factory/destructor pointers default null;
headers explicitly include their prerequisites and guard the GLM experimental
macro. Full Scene/entity/native-script lifecycle and serializers remain pending;
component declarations are not a claim that those runtimes are integrated.

Debug full build passed (0), with vendor source unchanged: stage4b-debug.log.
The sequential --stage stage4b desktop runner passed five Debug checks (0):
native 4.2 FontSmoke/foundation/core/Sandbox, then forced 4.1 FontSmoke. Logs:
`stage4b-debug-{native,gl41}-*.log`. FontSmoke creates/destroys two Applications,
checks shared default TextComponent assets, 191 actual glyphs/metrics (target
range requests 224 codepoints), reads RGB signed-distance atlas data back from the
GPU, loads a copied UTF-8 font path, rejects missing/invalid files and proves the
default cache expires each shutdown. Both forced contexts identify Intel HD 4000.
This proves atlas generation/upload and cache lifetime; complete text drawing is
a later Renderer2D gate. Forced 4.1 is Mesa evidence, not a macOS run.

Release build passed (0), followed by all five Release runtime checks (0),
sequentially with Debug/runtime already complete. Evidence: stage4b-release.log
and `stage4b-release-{native,gl41}-*.log`. Windows font compilation remains pending. Do not commit this prerequisite until the
stage 4a Windows fix result is known and investigated if it fails. Windows font/
engine/window/GPU runtime has not been run or claimed.


Stage 4a fix [Actions run 36889492677](https://github.com/cringlekaden/Hazel/actions/runs/36889492677)
passed both Linux and Windows Debug/Release compilation and both CPU shader and
camera/ECS/YAML runtime suites. Evidence: stage4a-fixed-actions.json. The actual
Windows compile confirms the macro-rooted /FI path fix. Windows CPU foundation
runtime is verified; application/window/input/dialog/GPU runtime remains untested.
The stage 4a failure gate is resolved before the next prerequisite checkpoint.

Stage 4b VS2022 generation and inspection passed for fifteen native projects:
per-config dynamic CRT, UTF-8, FreeType .c sources, font/YAML project references
and external matching shader libraries. Evidence: stage4b-vs2022.log and
stage4b-vs2022-inspection.txt. Initial inspection assumptions were corrected:
.c extensions imply C when no CompileAs override exists, and Premake emits native
static dependency links as ProjectReferences, not AdditionalDependencies strings.
These are inspection corrections, not compiler failures or Windows runtime passes.

Both local config builds and five-check runtime matrices pass. Clean exact font
pins and original vendor source/pins are retained. Checkpoint subject:
`Integrate upstream font atlas and component prerequisites`. Push only migration
branch for actual Windows font compilation; FontSmoke execution remains a Linux
GPU test here. Stage 4 is still incomplete: full Scene/Entity/SceneSerializer,
physics/scripts/project integration and complete Renderer2D behavior gates remain.

Next concrete renderer portability issue is confirmed on native hardware:
`glxinfo -l` reports 16 fragment texture units, 80 combined and 16 vertex texture
units on Intel HD 4000 core 4.2. Evidence excerpt: stage4c-native-texture-limits.txt.
Preserve the original local 16-slot fix while allowing the target's 32 slots on
capable devices: query backend limits, specialize the actual shader sampler array
and switch cases to the chosen capacity, include that capacity in cache identity,
and flush batches at the same boundary. Common renderer-facing APIs should expose
semantic limits, with GL queries/version requirements inside the OpenGL backend.

Stage 4b checkpoint `d39a2ad` [Actions run 36891314760](https://github.com/cringlekaden/Hazel/actions/runs/36891314760)
passed Linux and Windows Debug/Release builds and both CPU shader/foundation
runtime suites. Windows font/application/GPU runtime remains untested.

## Stage 4c in progress: actual upstream Renderer2D

Imports and target blob IDs are in evidence/stage4c-imports.json. The actual
quad/circle/line/text implementations and shaders replace the tutorial renderer;
retain Scope-owned CPU arrays and explicit context-bound renderer teardown.
Backend factories and generic Shader::SetMat4 follow the target, removing the
legacy Renderer::Submit OpenGL cast. Primitive/text capacity checks and font-atlas
switch flushes preserve geometry; text decodes UTF-8 with the pinned atlas library.
Native 16-slot hardware batches and the shader array/cases use the same limit;
32-slot hardware retains the full target limit. The selected capacity is included
in shader cache identity through specialized source. Application startup failures
now release partial renderer resources before the window and restore the previous
working directory/singleton. Runtime verification of these changes is pending.

An initial Debug link failed because forwarding Scope array sizes ODR-used target
static const capacities. C++17 static constexpr capacities fix this without
changing their values (20,000 quads/80,000 vertices/120,000 indices). Debug then
compiled/linked successfully. The expanded pixel/overflow/recovery suite is being
built before runtime verification and Release work.

Public upstream capability inspection, 2026-10-01: a fresh origin/master fetch
resolves to the pinned `1feb70572fa87fa1c4ba784a2cfeada5b4a500db`; target
Renderer2D.cpp:66 retains only `TODO: RenderCaps`. Available public refs inspected:
asset-manager `cca493f4abbbfd759fc624b3cc58d338cd316b5b`, projects
`e4b0493999206bd2c3ff9d30fa333bcf81f313c8`, scripting
`f8f8e3089b47c8371dc61448c3d47d43cad31079`, text at the pinned commit.
They likewise expose no RenderCaps/RendererCapabilities/RendererSettings system.
The current semantic texture-limit accessor and backend query are local
extensions. A consolidated record remains a final acceptance requirement; use
existing specification/factory patterns, keeping OpenGL versions/functions/path
selection inside that backend. No attribution to an unavailable implementation.

New routine logs go to ignored build/migration/evidence; the desktop runner now
uses that directory. Prior tracked raw logs will be removed during final cleanup;
provenance/pins/licenses and useful regression sources remain tracked. The current
CMake shader dependency build remains an unmet final acceptance requirement.

Stage 4c Debug runtime matrix passed all 13 checks (exit 0), sequentially:
native Intel HD 4000/OpenGL 4.2 Renderer2D/readback/features/font/foundation/core/
graceful Sandbox/legacy GLSL 420 renderer/CPU shader checks; process-only Mesa 4.1
Renderer2D/features/graceful Sandbox; llvmpipe software OpenGL 4.6 Renderer2D and
features. Renderer2DSmoke verifies solid/textured/transformed/rotated/sprite quads,
tiling and tint, circles/rings, lines and both rectangle overloads, color and
integer entity outputs, UTF-8 glyph picking/whitespace, atlas-switch flushing,
texture-limit flushing (16 native/forced, 32 software), and quad/circle/line/text
buffer overflow including final visible geometry and statistics. Generic Submit
uses the target Shader interface and uploads transform/view-projection correctly.
A missing-shader Application failure restores working directory and permits a
subsequent valid Application; owners are released with their context alive.
Logs are ignored build/migration/evidence/stage4c-debug-*.log. Forced 4.1 and
software 4.6 are test paths, not macOS or hardware 4.6 validation. Release and
this checkpoint's Windows compilation are still pending.

Stage 4c Release build passed (0), followed by the same 13-check runtime matrix
(all 0), with Debug and Release builds/tests sequential and maximum two compiler
jobs. Logs: ignored build/migration/evidence/stage4c-release*.log. VS2022 generation
and new test dynamic-CRT/UTF-8 inspection passed; both CI jobs retain Debug/Release
and now require the new Renderer2DSmoke executable. Generation is not MSVC/runtime
verification. The runner clears inherited Mesa/software test overrides before
applying a profile, so native labels cannot silently inherit those test settings.
No OS window/input/filesystem code or vendor sources/pins were changed.

Stage 4c checkpoint subject: `Integrate and verify complete upstream Renderer2D`.
Push only the migration branch for MSVC compilation. Windows application/GPU
runtime remains untested; stage 4 remains incomplete pending physics, managed
script/project prerequisites and complete Scene/Entity/SceneSerializer gates.
Next: actual pinned Box2D and project prerequisites, then real Mono integration
and full scene integration (no scripting stubs or omitted serializer fields).
Final Premake-only dependencies, consolidated capabilities/cache audit and
repository cleanup remain acceptance requirements, not completed work.

Stage 4c checkpoint `9c80938c9c074f5b3bdeb6dca4ed7986a48ad248`
[Actions run 36898770968](https://github.com/cringlekaden/Hazel/actions/runs/36898770968)
passed Linux and Windows Debug/Release compilation, new Renderer2DSmoke output
checks, and existing CPU shader/foundation runtimes. Windows renderer/application/
font GPU tests were not executed. The compilation gate is resolved before the
next prerequisite checkpoint.

## Stage 4d in progress: Box2D and project prerequisites

Box2D uses exact target gitlink `80e17bef53f217fa8b1696718a047e29e9f25def`
(TheCherno/box2d), inspected from actual source before integration. Its source
reports version 2.4.0 and has no nested submodules. Its own Premake project is
included unchanged; root-owned physics.lua directs generated files/outputs and
GCC/Windows CRT/UTF-8 settings. No vendor patches or CMake are introduced here.
Project/ProjectSerializer are imported from target blobs in stage4d-imports.json,
preserving all target config fields and active-project flow. Native streams accept
filesystem paths, persisted paths use generic UTF-8, and reads decode UTF-8; failed
parses/saves preserve live configuration and active project state. This addresses
concrete Windows narrow-filename/common-format issues in actual target source.
Build and CPU project/Box2D checks are pending. Scene's Box2D integration and
managed fields/runtime remain later gates, not claims from this prerequisite.

Stage 4d Debug build passed (0), followed sequentially by ProjectPhysicsSmoke
and graceful Sandbox shutdown (both 0) on native Intel HD 4000/OpenGL 4.2.
Project tests cover all config fields and Unicode filename/content round trips,
relative asset resolution, no-active-project diagnostics, missing/invalid/partial
loads and failed-save preservation. Box2D tests exercise gravity, both box/circle
contacts, static/dynamic/kinematic bodies, density/friction/restitution threshold,
fixed rotation, destruction and repeated worlds. These are CPU prerequisite tests;
full Scene start/stop/copy/physics integration remains pending. Logs are ignored
build/migration/evidence/stage4d-debug*.log. Release and Windows checks are pending.

Read-only next-prerequisite inspection: actual target Mono static archives embed
version 6.12.0 and DEFAULTLIB directives MSVCRTD (Debug)/MSVCRT (Release), matching
our required dynamic CRT. This removes the need to guess or discard target Windows
libraries. Linux Mono is not installed (mono/mcs/mono-2 pkg-config absent); available
CachyOS/Arch package metadata lists 6.12.0.206-1. No Mono runtime integration is
claimed from this inspection, and no system installation has been performed.

Stage 4d Release build passed (0), followed sequentially by ProjectPhysicsSmoke
and graceful Sandbox shutdown (both 0) on native 4.2. Debug completed before
Release, with two build jobs maximum. Logs: ignored build/migration/evidence/
stage4d-release*.log. Both OS CI jobs now build/check the new executable and run
its CPU project/physics checks in Debug/Release. VS2022 generation and inspection
of Box2D/new test passed matching dynamic CRT and UTF-8; actual MSVC compilation
is pending the next branch push, and no Windows GPU runtime is claimed.
Checkpoint subject: `Import pinned Box2D and portable upstream projects`.
Continue with real Mono dependencies/integration and full scenes; stages 4–9 and
final acceptance requirements remain incomplete.

Stage 4d checkpoint `d69369f1575031a6b83237a8e4084efdec33018c`
[Actions run 36902508074](https://github.com/cringlekaden/Hazel/actions/runs/36902508074)
passed both OS Debug/Release builds and CPU shader, foundation and new project/
physics runtime suites. Windows project/Box2D CPU prerequisite runtime is verified;
Windows Scene/app/GPU runtime is not. No scene-level physics claim from this test.

## Stage 4e in progress: actual managed API and Mono SDK

Actual target Hazel-ScriptCore source/project, Windows Mono SDK and managed runtime
files are copied from the pinned checkout; blobs in stage4e-imports.json. Keep the
target managed API/names and .NET 4.7.2 project. Root-owned output changes isolate
Debug/Release assemblies. Windows native archives preserve MSVCRTD/MSVCRT matches.
Linux uses native Mono development SDK headers/shared SGen runtime, with optional
--mono-root for a local SDK prefix; no OS condition is added to managed APIs.

Local package 6.12.0.206-1 was downloaded into ignored build/dependencies/mono and
verified against SHA-256 `5c30dcc286b1f65073cf267a4befa5ef4ae282351c4337beada740a15ae1e528`
from /var/lib/pacman/sync/extra.db (official Arch package through CachyOS mirror).
It is extracted locally, without a system installation. Runtime/compiler report
6.12.0; initial runtime/compiler invocations work. The package carries its licenses.
A clean Linux environment can instead install its native mono development package.

Premake generation initially resolved relative --mono-root against the included
script directory, then exposed the unavailable rpathdirs API. Resolve against the
repository root and use current Premake runpathdirs; generation now passes. An old
generated Makefile ran after the failed generator but did not include Mono targets;
that build is not Mono verification. The narrow existing flags shim is unchanged.
The actual new managed/native build and domain/reflection/GC tests are pending.

First actual managed build failed with mcs CS2011 despite its response file
existing. A direct reproduction showed that the extracted runtime lacked its
installed /etc/mono/config DLL mappings. Passing the extracted --config path to
the compiler fixes the same command (0); no generated-response-file patch is
needed. The native embedding test parses the build-selected native Mono config
before JIT startup. Premake selects its existing Mono compiler set by default on
Linux; Windows keeps the target Visual Studio managed flow. Normal installed SDKs
supply their own config/compiler launcher; the relocated local SDK uses explicit
CSC arguments with its config. Debug managed/native rebuild is pending.

Managed compilation's next concrete failure was missing System.Linq under
Premake /noconfig. Add explicit System/System.Core assembly references and the
Mono -sdk:4.7.2 profile instead of removing the upstream using/API. Actual Debug
managed/native build now passes (0). Sequential MonoSmoke and graceful Sandbox
both pass (0). MonoSmoke uses actual compiled Hazel-ScriptCore, a derived fixture,
native FileSystem assembly bytes at a Unicode path, entity inheritance, float/
UTF-16 char/uint64 reflection, actual Hazel.Vector3/method execution, GC handles,
managed exception reporting and two created/unloaded script domains. This verifies
the dependency/managed API, not the pending ScriptEngine/Glue/Scene integration.
Exact SDK/package/CRT decisions are in scripts/dependencies/mono.json. No macOS
runtime/support is implemented or inferred. Release and Windows checks are pending.

Stage 4e Release managed/native build passed (0), followed by MonoSmoke and
graceful Sandbox (both 0), after Debug completed. Logs remain ignored
build/migration/evidence/stage4e-*.log. Both native builds use at most two jobs.
VS2022 generation/inspection passed managed .NET 4.7.2 projects, native MonoSmoke
UTF-8/dynamic CRT, and matching Debug/Release static SDK paths. CI now installs the
native Linux Mono development package and runs real managed/native embedding on
both OSes/configurations. Windows compilation/Mono runtime remains pending that
push; Windows GUI/GPU runtime is untested. Actual upstream SDK/runtime bytes and
license are retained, not rebuilt or patched; adapted managed project settings
and complete source provenance remain reviewable. Checkpoint subject:
`Build and verify upstream managed API with native Mono SDKs`.
Continue with the actual full Scene/ScriptEngine/Glue sources and lifecycle/field/
serialization/physics/render gates after investigating any CI failure.

Stage 4e checkpoint `f9f55c6e46c85ebcf378cc1066633b3a7a2fba1a` is pushed;
[Actions run 36905876374](https://github.com/cringlekaden/Hazel/actions/runs/36905876374)
is pending. Prepare the next source integration while that runs, but investigate
any preceding CI failure before committing a later checkpoint.

## Stage 4f in progress: complete scenes and scripting

Actual target Scene/Entity/ScriptableEntity/SceneSerializer, ScriptEngine/Glue and
Physics2D source are imported; existing adapted SceneCamera and Components are
preserved. Exact target blobs: stage4f-imports.json. An initial compile probe is
running before compatibility/lifetime adaptations. No full scene/runtime parity
is claimed yet.

Concrete source concerns to investigate in this stage: copied components retain
live native-script/body/fixture pointers; runtime stop does not destroy native
scripts or clear body observations; DestroyEntity does not remove its live physics
body/script instance; pause step counters decrement below zero. Managed objects
lack GC handles, script field buffers use unaligned casts/static shared storage,
System.Char serialization uses 8-bit char despite Mono's UTF-16 field, and component
registration depends on MSVC RTTI spelling. Filewatch's Windows close-event handle
leak and filesystem/queued reload lifetimes also need investigation. Target
runtime serializer methods are explicitly unimplemented upstream; preserve that
limitation rather than fabricating a format. All published editor serializer
fields, renderer calls, scripts and physics features must remain available.

The full import compile probe exposed GCC's rejection of a reference to a YAML
operator[] temporary, packaged Mono's public-header boundary (tabledefs.h is
internal), and function-pointer registration's implicit conversion to const void*.
Use YAML::Node values, public attrdefs/row-indexes, and explicit Mono API casts.
Modern fmt also requires explicit UTF-8 path/UUID/GLM string conversions. All
internal-call functions and scene fields remain present. Windows stage 4e CI has
passed both config builds and actual Mono embedding runtime; Linux is pending.

Initial SceneSmoke compiled, then failed (1): a destroyed Entity's boolean still
reported valid. Check the registry's generation validity. The expanded Debug
probe now passes (0): UUID/UTF-8 tag setup, stale handle validity, live scene copy/
duplicate sanitation and simulation stop/restart. Physics world ownership uses
Scope/CreateScope; copied body/fixture/native-script observations reset, fixture
creation records its observation, and stopping clears observers before destroying
the owned world. These fixes retain all component configuration and target physics
settings. Full native-script/managed/serializer/render/lifecycle tests are pending.

Stage 4e [Actions run 36905876374](https://github.com/cringlekaden/Hazel/actions/runs/36905876374)
now passed both Linux and Windows Debug/Release compilation and real native Mono
embedding tests, plus existing CPU shader/foundation/project/physics suites.
Windows Mono dependency/managed API runtime is verified; full ScriptEngine/Glue,
Application, input/dialog, renderer/font/editor GPU runtime is not inferred.

Additional source inspection: original filewatch public master
`a59891baf375b73ff28144973a6fafd3fe40aa21` still closes only the directory handle,
leaving both stop and overlapped event handles open. Do not upgrade that pin or
import its unrelated macOS implementation to claim a fix. Keep the clean target
header and MIT notice. A local FileWatcher factory/OS adapter now uses actual
pinned filewatch on Linux and its ReadDirectoryChangesW/overlapped algorithm in
Platform/Windows, with Scope/RAII ownership of both events, directory and joined
worker, native UTF-16 filenames and worker error handling. Common watcher API
contains only filesystem paths/callbacks/five target event meanings. This is a
local adaptation, not upstream Hazel RenderCaps or macOS support. Actual watcher
notification/handle-count regression checks remain pending.

Managed adaptation in progress: Scope-owned engine data, pinned GC handles with
explicit invalidation, aligned local field reads/memcpy (no aliasing/static shared
buffer), public access-mask reflection, accurate signed/unsigned byte mapping,
explicit qualified names of all three actual managed component classes, and
native config/assembly paths from the validated SDK. Reload snapshots/validates
images before destroying the previous domain, invalidates external instance/class
observations, retains field values and rebuilds runtime instances; joins watcher
producers and advances an epoch so queued work cannot consume old metadata.
These changes compile in the current Debug probe; runtime/Release are pending.
Entity reference setters follow the [official Mono embedding sample](https://github.com/mono/mono/blob/main/samples/embed/test-invoke.c):
reference objects pass directly; stored scene fields retain UUIDs. Native runtime
verification is still required before claiming this conversion works.

Stage 4f expanded Debug SceneSmoke now passed (0), after the actual native/managed
Debug rebuild passed (0). Ignored evidence: build/migration/evidence/
stage4f-scene-debug-first.log and stage4f-scene-test-build.log. It verifies UTF-8
file notifications and stable Linux descriptor counts across 13 joined watcher
lifetimes; actual ScriptEngine class/access/signed-byte reflection, UTF-16 char,
uint64/vector layouts, GC ownership, managed component queries/transform/physics
impulse calls, UUID-backed/null/unscripted entity reference conversions, native
Scope script removal/stop/restart, pause/step, external instance invalidation,
corrupt assembly rejection retaining a working domain and valid reload retaining
live fields. This supersedes the preceding pending entity-reference runtime note.
No Windows watcher/full ScriptEngine runtime or scene YAML/GPU result is inferred.

Serializer adaptation is now in progress: native UTF-8 streams, 16-bit Char data
(legacy non-digit ASCII char scalars accepted), retained stored field metadata for
missing classes/fields, checked writes, and staged scene/field parsing before
committing. Source-copy/duplicate retains all target component specifications;
duplicate UUIDs receive independent saved script fields. Runtime serializer
methods remain explicitly unimplemented upstream and now throw in Release too.
CPU YAML round-trip/failure tests and the full scene GPU gate are pending.

Two additional concrete Debug regressions were reproduced and fixed before
advancing. The CPU YAML round trip failed (1) because the pinned yaml-cpp emitter
turns uint8_t into a character although its reader expects an integer; emit both
byte kinds explicitly as numbers, retaining their published field types. Atomic
DLL replacement then failed (1) because pinned Linux filewatch subscribes only to
IN_MODIFY/CREATE/DELETE. The project-owned Platform/Linux adapter now follows its
actual inotify directory/filename algorithm with rename/close-write/overflow
events, Scope/RAII descriptors, poll/eventfd stop and joined callbacks. Both OS
adapters retain the full original MIT notice; the vendor header stays clean.
Debug SceneSmoke now passes (0), including atomic replacement, stable descriptor
count, all supported script-field YAML bytes, renderer/camera/physics component
settings, missing-class/removed-field preservation, UTF-8 paths, failed writes
and malformed/missing input retaining the previous scene/field data. Evidence is
ignored stage4f-atomic-serializer-debug.log; no Windows result is inferred.

Application lifetime integration and full scene GPU tests are now in progress.
Scene-owning layers stop while Mono is alive; engine shutdown joins the watcher,
invalidates metadata/GC handles, cleans the VM and cancels its final queued work
before renderer/window destruction. Native consumers explicitly link matching
Mono/Box2D libraries. CI adds both-config CPU SceneSmoke (including real watcher/
full engine reload), but Windows results await the checkpoint push.

Full Debug rebuild passed (0). The fixed desktop runner's stage4f completed all
16 checks sequentially (0): native Intel HD 4000 OpenGL 4.2 scene/Renderer2D/
framebuffer/font/core/foundation/Mono/CPU scene/graceful Sandbox; forced Mesa 4.1
scene/Renderer2D/framebuffer/Sandbox; separate llvmpipe OpenGL 4.6 scene/Renderer2D/
framebuffer. SceneGPUSmoke verifies actual target editor/runtime/simulation scene
rendering with color/entity readback, UTF-8 texture/text YAML, minimized-camera
projection, real atomic watcher -> Application queue reload preserving live
fields, and scene-layer -> Mono -> renderer/window teardown. Logs are ignored
build/migration/evidence/stage4f-debug-*.log. Release is now building, at -j2.
Neither overrides/software results nor VS generation are Windows/macOS hardware
validation. Windows full Scene/ScriptEngine/watcher runtime remains pending CI;
Windows app/editor/GPU runtime remains untested.

Investigated the Debug-only `debugger-agent: Unable to listen on <fd>` diagnostic
at cleanup. [Mono 6.12.0.206 debugger-agent.c](https://github.com/mono/mono/blob/mono-6.12.0.206/mono/mini/debugger-agent.c)
socket_transport_accept prints it when accept returns -1; cleanup's
stop_debugger_thread -> socket_transport_close1 closes/shuts down the listening
socket specifically to wake that thread, then joins it. Timing and all process
exit codes (0) are consistent with this dependency cleanup diagnostic. Keep
the actual upstream Debug debugger options and SDK sources unchanged. No debugger
client attachment/session is tested or inferred from the scripting tests.

The added Debug initialization regressions pass (0): native-only scenes run/stop
without a Mono project; managed play before initialization is rejected before
marking the scene running; missing/invalid initial assemblies raise errors;
replacing an invalid initial app assembly recovers through a new script domain
under the existing root VM. A local Initialized state prevents partially loaded
images from appearing ready. Failed project reload validation restores prior
assembly paths; domain metadata clears before unloading. Final Debug relink
passed (0); its desktop suite is rerunning, then the updated Release build/runtime
gate and VS generation remain. These are local error/lifetime adaptations around
actual upstream initialization/consumer flow, not a new script API or VM backend.

Stage 4f final Linux Debug/Release builds passed (0), and both final desktop suites
passed all 16 checks (0); commands/profiles are in the resume block. The public
Hazel.h now exposes the actual target Scene/Entity/ScriptableEntity/Base/Assert/
MouseCodes consumer includes while retaining earlier public headers. Normalize
trailing whitespace only in project-owned imported engine sources; vendor source
bytes/pins remain clean. VS2022 inspection passes native Windows watcher source
with Linux exclusions, MDd/MD/MD CRT and UTF-8 in engine/apps/scene tests, Box2D/
Mono links and both managed solution build dependencies. Initial inspection
assumptions about native-to-managed ProjectReference were corrected: Premake
places dependson edges in solution ProjectDependencies, as verified. No MSVC or
Windows runtime result is inferred from generation. No routine raw logs are
newly tracked. Stage4's scene/serialization/render/lifetime Linux gate is met;
the dedicated stages5–7 and Windows CI gates remain ahead.

Stage4f checkpoint `04b562278589e951008ee751168c09d8b51d21c2` is pushed;
[Actions run 36921981641](https://github.com/cringlekaden/Hazel/actions/runs/36921981641)
passed both Linux and Windows Debug/Release compilation and all CPU runtime steps,
including actual full ScriptEngine/Glue/scene/YAML/physics/manual reload, invalid
initial/reload recovery, native script ownership and UTF-8/atomic watcher events
with stable resource counts. Windows automatic Application queue reload and GPU/
input/editor runtime are not inferred. No master merge.

## Stage 5 in progress: renderer/text consumer and API parity

All target rendering/font implementations and shaders entered as the documented
stage4 prerequisites. A fresh comparison with pinned Renderer2D.h confirms its
complete public API; the sole functional signature adaptation is DrawLine's const
second endpoint. Reuse actual TextParams/Statistics/Font APIs and initialization
instead of introducing settings types. The dedicated gate now adds pixel tests
for all four 2D/3D position and rotated textured quad overloads, tint/default
picking ID, Statistics vertex/index helpers and multi-byte non-Latin1 glyph fallback
to the actual target '?' atlas glyph. Existing font loading/failure, primitives,
spacing, atlas switch, texture-slot limits and all capacity overflows remain.
No larger Unicode atlas or macOS/backend implementation is implied. Stage5 builds
and 13-check desktop suites per configuration are pending; raw evidence ignored.

Stage5 Debug renderer test build passed (0), followed by all 13 sequential desktop
checks (0) on native Intel 4.2, forced Mesa 4.1 and separate software 4.6 profiles.
The new overload/statistics/fallback cases pass alongside existing renderer/font/
framebuffer/core/Sandbox and CPU shader-tool regressions. Release test build is
running; that configuration's desktop suite remains pending. Evidence:
ignored build/migration/evidence/stage5-*.log.

Stage5 Release build passed (0), followed by all 13 desktop checks (0), after
Debug's equivalent pass. Native 4.2 functionality is retained; overrides/software
are separate test profiles, not macOS/hardware validation. The dedicated stage5
gate is complete locally. Windows compile of the expanded GPU test will be checked
by the branch CI; Windows GPU execution remains untested. Checkpoint subject:
`Verify complete 2D renderer and text parity`. Proceed to actual scene physics
integration tests, preserving pinned Box2D and its target material/body APIs.

Stage5 checkpoint `f37b3e6d4183928fd9e359a8a91cf042ac7c234c` is pushed;
[Actions36924603403](https://github.com/cringlekaden/Hazel/actions/runs/36924603403)
passed both OS/config builds and CPU suites. Expanded GPU test compilation passed
Windows, but Windows GPU execution is still untested. Stage6 is active,
preserving exact Box2D source/pin.

## Stage 6 in progress: complete scene physics gate

New CPU ScenePhysicsChecks exercise actual Scene start/update/stop/restart,
box/circle contacts, all body types, material/density/fixed rotation, transform
updates, pause/step, live scene copy, entity/component removal and live duplication/
addition. The Debug probe builds (0), then runtime crashes (139) on live body
duplication. Investigating the null runtime body observation before any advance;
ignored evidence stage6-scene-before.log and native backtrace. The proposed local
adaptation will reuse actual target body/fixture setup for newly added observations
instead of retaining copied pointers or dropping components/features. No success
is claimed yet. Windows stage5 CI must be checked before another checkpoint.

GDB confirms SIGSEGV at Scene.cpp's runtime transform read (body observation null)
from ScenePhysicsChecks live duplication. Sandbox ptrace denial was rerun through
the required tool escalation; the valid native backtrace is ignored evidence.
Private SynchronizePhysics2D now reuses the exact target body/box/circle fixture
definitions/materials to initialize missing observations at start and before
updates (including after native callbacks). Component additions clear borrowed
body/fixture observations; world ownership and removal/stop cleanup are preserved.
Debug build and expanded CPU SceneSmoke now pass (0), including actual scene
contacts/gravity/transform updates, static/dynamic/kinematic types, fixed rotation/
density/materials, pause/step, copy, stop/restart, runtime duplicate/remove/re-add
body/collider lifetimes and the earlier serialization/managed/watcher checks.
Stage6 full Debug/Release relinks and desktop checks are next. This is a local
lifetime adaptation of Scene.cpp from the fixed source, with no Box2D pin/vendor
changes, renderer/backend change or macOS implementation.

Stage6 final Linux gate: full Debug/Release builds and ten sequential desktop
checks per configuration passed (0). Native Intel HD4000 4.2 scene contacts,
rendering/picking, serialization/scripts/physics/core and graceful Sandbox close
passed; forced Mesa 4.1 and software 4.6 are distinct test profiles. Evidence is
ignored build/migration/evidence/stage6-*.log. No process survives the gate.
Checkpoint subject: `Verify scene physics and live body ownership`. Windows
build/CPU runtime is pending branch CI; Windows graphics runtime is untested.
Continue stage7 dedicated managed component/input/example/project gates.

## Stage 7 in progress: managed API and actual example project

Stage6 checkpoint c86d1dd is committed/pushed; its CI is pending. Import actual
pinned SandboxProject scene/texture/project/example sources (stage7-imports.json).
Root Premake builds the real Player/Camera scripts and deploys Sandbox.dll into
ignored project Binaries. The test fixture also compiles those unchanged consumers.
SceneGPUSmoke now exercises all managed Text properties, Transform, body type,
both impulse overloads, velocity, entity lookup/cast, Input call and runtime
restart alongside the existing field/serialization/project/reload/watcher gates.
Input key-state retrieval is tested; no synthetic physical key press is claimed.
Debug/Release builds and desktop suites are pending. No vendor/OS/backend changes.

Stage7 first Debug GPU probe aborted in Mono class-init.c (`klass` null).
Backtrace identifies ScriptEngine::LoadAssemblyClasses namespace/name lookup on
compiler-generated nested types introduced by the component fixture's array.
Resolve TypeDef rows by actual Mono metadata token and reject missing types
explicitly before subclass inspection. This preserves assembly discovery rather
than removing the triggering script constructs. Rerun pending; ignored evidence
stage7-debug-native-SceneGPUSmoke.log. Host Mesa now reports 26.2.4-arch3.1/native
Intel 4.2; this session has changed no system packages or driver configuration.

Stage7 corrected full Debug build passed (0), then all sixteen sequential
native 4.2/forced 4.1/software 4.6 desktop checks passed (0). Actual Player/Camera
and all managed-component tests pass without deleting the nested-type trigger.
Release build is running next; VS2022 generation passed (generation only).
Current evidence is ignored stage7-*.log. No macOS or Windows graphics runtime
validation is inferred. Public upstream refresh still resolves master to the fixed
checkpoint; available asset-manager/projects/scripting refs contain only the
RenderCaps TODO, with no equivalent capability/settings implementation.

Stage6 checkpoint `c86d1dde9a89a52627d50b75c478efa00efe75d9`:
[Actions36927686042](https://github.com/cringlekaden/Hazel/actions/runs/36927686042)
completed successfully for Linux/Windows Debug/Release compilation and CPU
scene/engine/physics/script/watcher suites. Windows graphics remains untested.
Stage7 full Release build passed (0); its sixteen-check runtime suite is running.

Stage7 full Debug/Release builds and all sixteen desktop checks/configuration
passed (0); preserved managed/scenes/projects/physics/rendering/text/ownership
regressions, actual examples and new component/internal-call coverage. Source
record: evidence/stage7-imports.json; standalone example Premake replaces target
vendor-Premake Windows paths with system Premake, using the same actual managed
sources/API and net472 references. Root generation/deployment and VS generation
passed; standalone workspace build will also be checked during final clean build.
Checkpoint subject: `Verify managed components and import upstream example project`.
Windows stage7 compilation/CPU runtime awaits branch CI; GPU remains untested.

## Stage 8 in progress: actual Hazelnut editor

Stage7 checkpoint f3eff4a is committed/pushed; CI pending. Import exact editor,
panels, icons, fonts and layout from the fixed checkpoint; provenance recorded
in stage8-imports.json. Keep the actual ImGuizmo 218d60b pin, clean vendor source
and current official docking ImGui/backends. Root-owned ImGuizmo settings provide
math operators and matching CRT/UTF-8. Imported editor now uses Scope application/
layer ownership, actual upstream ImGui fonts/theme/BeginFrame/widget-ID flow and
separate UTF-8 content payloads/paths on both OSes. Local fixes keep NewScene's
editor scene consistent, save the editor scene during play, stop runtime before
project switches/teardown, remove global GPU font ownership and expose all three
body types in the inspector. Complete builds/runtime workflow gate pending.

Initial editor Debug compilation exposed the previously missing target public
OrthographicCameraController::OnResize API and an indirect KeyPressedEvent include.
Import the actual target resize operation, keep minimized-size protection and
reuse it for window resize; add the direct event include. The exact ImGuizmo pin
already compiled against retained official ImGui without source patches. Windows
command-line UTF-16 conversion now lives exclusively in Platform/Windows and
feeds UTF-8 to common Application args, keeping editor/project paths portable.

Stage7 Windows deployment investigation: Actions36928990041's Windows Debug
compilation produced the native and managed binaries, then SandboxScripts failed
MSB3073 because its C# post-build event resolved a relative copy source from the
output directory. Use MSBuild $(TargetPath) for that source on VS; retain the
working GNU Make path for Linux. VS generation confirms the expanded post-build
command. Linux Debug/Release/runtime source is unchanged by this build-script fix.
A separate stage7 CI correction checkpoint preserves all in-progress stage8 edits;
Windows Release and CPU suites must be rerun before claiming stage7 CI passed.

Stage7 correction checkpoint `aaf145aa892f0f835a18fc8a7722cc31d25e9e3e`:
[Actions36930517894](https://github.com/cringlekaden/Hazel/actions/runs/36930517894)
Windows job passed Debug/Release builds and shader/project/physics/Mono/full CPU
scene/script/reload/watcher suites. This includes token-based discovery with the
nested-type fixture. Linux CI remains in dependency installation; previous local
Debug/Release and sixteen desktop checks passed. No Windows graphics test yet.
Stage8 full Debug build now passed (0), including Hazelnut/real editor smoke and
exact clean ImGuizmo source. Official ImGui stdlib and popup API flags replace
target fork include paths/obsolete signature, and portable snprintf preserves
inspector string controls. The twelve-check Debug desktop suite is running.

Stage8 Debug's twelve sequential desktop checks passed (0), including actual
editor workflow on native Intel 4.2, forced Mesa 4.1 and software 4.6. Docked panels,
UTF-8/non-BMP project/scene/texture paths, save/reopen, duplicate, gizmo shortcuts/
drawing, managed play/pause/step/restart, simulation, project reopen and ordered
shutdown passed. The test invokes actual editor commands; it does not claim a
physical mouse drag or file-dialog user interaction. Release build/runtime next.

Stage8 full Release build and all twelve desktop checks passed (0), after the
Debug gate. VS2022 generation confirms MDd Debug/MD Release+Dist and UTF-8 on editor,
engine and ImGuizmo, with OS source exclusion retained. All submodules are clean,
including the exact new ImGuizmo gitlink. No local build/test/debugger remains.
Checkpoint subject: `Integrate and verify upstream Hazelnut editor`.

Windows graphics execution is now scheduled in branch CI: a test-only Mesa
26.2.3 MSVC WGL package from pal1000/mesa-dist-win, exact release URL/SHA256 in
scripts/migration/windows-mesa.json (verified against GitHub release digest and
local download). Runner copies DLLs beside temporary executables, selects llvmpipe
with two threads, tests software 4.6 and forced 4.1, then removes temporary files.
No system driver install, hardware override in engine code or macOS claim.
Actual Windows results are pending; Python syntax/VS generation on Linux are
preflight checks only. It exercises rendering, actual editor workflow, queued
reload and both applications' native graceful close, including Unicode CLI project
arguments. Raw evidence becomes CI artifacts/ignored output. Physical mouse drag
and native file-dialog interaction still require separate execution.

Stage8 Actions36932885871 Windows Debug/Release compilation passed, then the
new software46 runtime passed renderer/core/features/full 2D/font/full scene GPU
(including automatic reload), and the editor's workflow/shutdown assertions passed.
EditorSmoke then failed strict temporary-directory deletion with sharing violation
32. Overall Windows graphics gate is not yet passed. Add Windows-only cleanup
path/native-cwd/remaining-file diagnostics and rerun, preserving every functional
check. The process had exited; no surviving CI runtime is assumed. Raw failed job
log is ignored stage8-ci-windows.log. Linux run is finishing Release/CPU gates.

Stage8 Windows diagnostic Actions36934638145 again passed Debug/Release native
compilation and the first six software46 renderer/scene suites. Editor workflow
and engine shutdown passed; strict fixture deletion stopped at MonoDebugger.log
(error32), with both C++ and Win32 current directories correctly restored.
Actual Mono 6.12.0.206 source opens the debugger logfile with fopen and its cleanup
never closes that stream:
https://github.com/mono/mono/blob/mono-6.12.0.206/mono/mini/debugger-agent.c
Use Mono's documented default stdout sink, retaining loglevel3, soft breakpoints
and the debugger agent. Keep strict fixture removal and diagnostics; no vendor
patch or suppressed failure. Linux recompilation/runtime and Windows rerun are
pending for this correction; existing source/build gates remain recorded above.

Stage9a dependency checkpoint: Linux Debug's six Premake libraries compiled and
installed from unchanged clean source pins. Full engine Debug compile/link passed.
MigrationShaderToolsSmoke passed GLSL and HLSL optimization, semantic Vulkan/OpenGL
SPIR-V validation, UBO/sampler reflection/names/bindings, GLSL410 generation and
invalid-source diagnostics. No CMake execution. SDK VS generation confirms six
projects, Windows-only glslang OS source, MDd/MD and UTF-8; generation is not an
MSVC build. Replace the CMake hook with a root-owned forced cstdint header and
actual pinned generators/source manifest. Eighteen grammar/generated headers
match prior validated output; build-version timestamp is now deterministic from
the shaderc pin's SOURCE_DATE_EPOCH rather than configure time. Incremental
generation only publishes changed content. CI cache keys include every generator/
workspace/source-manifest input and force a new Premake cache.

This is a buildable Debug dependency checkpoint, not final acceptance: local
Release SDK/build/runtime and CI both configurations remain pending. Capability/
settings/cache hardening edits remain in the working tree and are excluded from
this dependency-only commit. Editor correction7096803 Actions36935755402 passed
both OS/config builds/CPU suites and all17 Windows SOFTWARE graphics/editor/close
checks per configuration, including strict cleanup. Windows hardware and macOS
remain untested. Continue sequentially through the pending stage9 gates.
