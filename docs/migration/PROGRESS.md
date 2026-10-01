# Migration progress

Last updated: 2026-10-01. Branch: `migration/upstream-1feb705`.
Baseline: `b030be7`. Fixed target: `1feb70572fa87fa1c4ba784a2cfeada5b4a500db`.

## Resume here

Read PLAN.md, PRESERVATION.md and COMPARISON.md. Check `git status` and submodule
cleanliness before edits. The upstream source checkout is in
`/tmp/hazel-upstream-1feb705`; if absent, clone TheCherno/Hazel and detach at the
fixed hash. Do not use a moving master. Branch pushes are now authorized for CI; never merge
into master. Build sequentially
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

Stage 1 checkpoint `81b3861` and stage 2 checkpoint `92eb60a` have passed
Linux and Windows Debug/Release compilation in Actions. Stage 2 also passed
local Debug/Release core, renderer and graceful Sandbox shutdown runtime checks
on Intel HD 4000/OpenGL 4.2. Windows engine runtime is untested. Stage 3 backend integration is locally
verified in Debug/Release on native 4.2, forced 4.1 and software 4.6; integrated
Linux/Windows Debug/Release compilation and optimized CPU shader tests passed CI.
Stage 4 foundation is in progress; stages 4–9 remain incomplete. The sections below retain initial-session evidence, followed by
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
