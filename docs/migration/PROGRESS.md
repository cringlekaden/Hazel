# Migration progress and final parity

Updated 2026-10-01 (local date). Branch `migration/upstream-1feb705`; default/master
remains `b030be7353fd730da78e4bcee6273e0421506fb3`. Fixed upstream target:
`1feb70572fa87fa1c4ba784a2cfeada5b4a500db`. Only the migration branch was pushed
for CI; no merge or unrelated system change. Pre-existing work/configuration and
ignored artifacts were preserved. Builds and runtime checks ran sequentially
with at most two compiler jobs and two atlas/software-renderer workers.

## Final status

All planned implementation stages and the four final acceptance requirements
are complete. Verified code checkpoint: `eebab04846b15720769083e60f0f6eac3980811e`;
the final record-only commit follows it. Remaining boundaries are explicitly
listed in KNOWN-LIMITATIONS.md; Windows hardware, macOS and manual interactions
are not claimed. No implementation stage remains pending.

| Final acceptance gate | Actual verification |
| --- | --- |
| Premake-only clean build | Fresh local checkout began with no generated/installed/build artifacts. Dependency inputs stayed identical while source advanced 9ed63ed -> 967f85c -> eebab04 before root compilation. CMake was absent from PATH. Pinned shader SDK Debug/Release and full root Debug/Release compiled/linked successfully (exit 0); no CMake command was used |
| Complete shader toolchain | Six Premake library projects retain shaderc/util, glslang/HLSL/OGLCompiler/SPIRV, SPIRV-Tools core/validation/full optimizer, Cross core/GLSL and actual grammar/registry/version generators. Both local manifests record Premake/two jobs/clean exact pins; all five dependency sources remain clean and five license directories are installed |
| Linux runtime after cleanup | Both fresh configurations passed 33 desktop checks each: native Intel 4.2 (15), forced Mesa 4.1 (9), software 4.6 (9). All 66 returned 0. Renderer/cache/settings/batching/text/scene/physics/project/managed/reload/watcher/editor and graceful close of Sandbox2D, actual generic ExampleLayer and Hazelnut passed |
| Windows and Linux clean CI |eebab04 / [Actions36944228477](https://github.com/cringlekaden/Hazel/actions/runs/36944228477), dispatched with rebuild_shader_tools=true, passed both OS/configurations. SDKs were rebuilt from source despite cache hits; job logs contain two clean Premake installs per OS. Full root builds, optimized GLSL/HLSL/semantic-SPIRV/reflection checks, CPU scene/project/physics/Mono/reload/watcher and standalone managed Debug/Release builds passed. MDd Debug/MD Release retained |
| Windows runtime |That run passed all 38 SOFTWARE WGL/llvmpipe/editor/graceful-close cases (19/config) across 4.6/forced 4.1. Logs show 38 zero exits and strict fixture cleanup. This is actual Windows runtime testing, separate from compilation; Windows hardware remains unrun |
| Standalone project files |Fresh local standalone gmake generation and both managed configurations compiled Hazel-ScriptCore/Sandbox successfully. Separate Linux/Windows CI generation/compilation gates passed; generated project outputs are scoped ignored |
| Capabilities/settings/cache |Default/device-clamped and requested 1000/two-slot batches, default/forced GLSL/native specialization, real version directive selection, cold/warm/changed-source/truncated/valid-header corruption recovery passed in both configurations. Warm files were not rewritten. Final native and software recovery produced color 64,64,192 / entity 73 / GL error 0, with integer/color blend state preserved/restored |
| Cleanup/provenance/parity |Four concise records retain progress/preservation/provenance/limitations; 129 routine raw logs/CI metadata were removed from tracking and archived in ignored output. Useful regression scripts, source/blob import records, baseline comparison, pins/licenses/build scripts remain. Root LICENSE matches the target blob exactly. Vendor sources/submodules are clean; vendor Makefiles were not recursively deleted |
| Future portability architecture |Separate Linux/Windows OS adapters and OpenGL backend retained. Common scene/project/asset/renderer specifications contain no added OS choices. Backend 4.1 resources/shaders preserve native 4.2 functions. No macOS/Metal implementation or validation claim |

Final local evidence: ignored `build/migration/evidence/final-clean-summary.json`,
`final-clean-*.log`, fresh checkout `build/migration/clean-checkout`, and its
`build/migration/evidence/final-{debug,release}-*.log`. CI raw job logs are ignored
`final-{linux,windows}-actions.log`; Windows runtime artifacts are attached to the
linked run. Older original ignored SDK/build outputs are preserved and were not
used as fresh-build proof. Original Mono SDK was supplied as an external SDK
prefix to the fresh checkout, as recorded below.

Both local helpers 10921 (SDK) and 99670 (root/runtime/standalone) exited 0. Process
audit found no surviving migration compiler/helper/app/test process; no interrupted
process remains. All CI runs used as final evidence are complete. The verification checkout
and recursively checked submodules are clean. The primary worktree contains only
the four final record updates before their documentation commit. Future work should start by inspecting status/processes and these records.

## Selected OpenGL paths

| Actual test profile | Selected path |
| --- | --- |
| Native Intel HD4000, Mesa 26.2.4-arch3.1, GL4.2/GLSL420 |Bind-based 4.1 resources; full optimized/reflected shaderc/Cross -> GLSL410;16-slot default batch; legacy330/420 retained; picking plus tested 4-sample color/depth MSAA. Integer texture samples 0 rejects unsupported integer MSAA explicitly |
| Same Intel device, process-local Mesa 4.1/GLSL410 override |Same complete generated GLSL410 path,16 slots, scenes/editor/examples. Only the original420 regression is skipped. This is a compatibility test, not Apple hardware validation |
| Linux llvmpipe, Mesa 26.2.4-arch3.1, GL4.6/GLSL460 |Loaded native SPIR-V specialization by default,32 slots; forced GLSL setting and two-slot batching also tested, along with supported integer MSAA |
| Windows process-local Mesa 26.2.3 WGL |Actual software 4.6 native specialization and forced4.1 GLSL410, including complete editor/examples. DLLs are checksum-pinned test artifacts, not installed drivers |

## Completed stages and checkpoints

| Stage | Checkpoints | Actual evidence |
| --- | --- | --- |
|0 Baseline/preparation |a20b1ba |Inspected clean tree/submodules, actual upstream checkout, baseline full Debug/Release builds, initial Sandbox startup; comparison/preservation/plan recorded before code |
|1 Renderer foundations |81b3861 |Linux native UBO/integer/matrix GPU checks. [Actions36801823201](https://github.com/cringlekaden/Hazel/actions/runs/36801823201) passed both OS Debug/Release compilation; Windows runtime was not tested by that run |
|2 Core/platform/lifetime |92eb60a |Debug/Release lifetime/partition/queue/input/args and graceful native Sandbox checks; duplicate glfwInit and destruction order fixed. [Actions36807370723](https://github.com/cringlekaden/Hazel/actions/runs/36807370723) both OS compilation |
|3 Shader/texture/framebuffer |b0bc147, ba10ef8,25362a8 |Full optimizer/reflection/cache/GLSL410/native-SPIRV/texture/MSAA/picking/state checks on native 4.2, forced4.1/software 4.6. [Actions36881572888](https://github.com/cringlekaden/Hazel/actions/runs/36881572888) both OS builds/tool runtime |
|4 Source prerequisites/full scenes |738fb32,8948e89,d39a2ad,9c80938,d69369f,f9f55c6,04b5622 |Actual source graph required font/rendering/physics/Mono/project prerequisites before full Scene integration; subsequent stages kept dedicated gates. [Actions36921981641](https://github.com/cringlekaden/Hazel/actions/runs/36921981641) both OS builds/CPU scene/script/reload/watcher tests; local full GPU scene/configuration checks |
|5 Complete2D/text |f37b3e6 |Debug/Release thirteen desktop checks/config; all primitives/text/UV/tiling/rotation/capacity/texture/font flushes, generic submit and failed-constructor recovery. [Actions36924603403](https://github.com/cringlekaden/Hazel/actions/runs/36924603403) both OS builds/CPU |
|6 Physics |c86d1dd |All body types, box/circle contacts/materials/gravity/transforms, pause/step/copy/restart/live duplicate/remove/add. Reproduced duplicate null-body crash before scoped synchronization fix. Debug/Release builds/ten desktop checks/config; [Actions36927686042](https://github.com/cringlekaden/Hazel/actions/runs/36927686042) both OS builds/CPU |
|7 Managed/projects |f3eff4a,aaf145a |Actual Player/Camera and project assets, component/internal-call/Unicode-text checks; nested metadata crash fixed using actual Mono tokens. Debug/Release sixteen desktop checks/config. MSBuild deployment uses TargetPath. [Actions36930517894](https://github.com/cringlekaden/Hazel/actions/runs/36930517894) both OS builds/CPU |
|8 Actual editor |cf30569,1505642,7096803 |Debug/Release twelve local desktop checks/config, actual editor/panels/docking/gizmos/workflows/scenes/scripts/payloads. Strict Windows cleanup failure diagnosed as Mono's retained debugger logfile, fixed using supported stdout while preserving debugger. [Actions36935755402](https://github.com/cringlekaden/Hazel/actions/runs/36935755402) both OS Debug/Release compilation and CPU suites, all17 Windows SOFTWARE WGL/editor/graceful-close checks/config across llvmpipe4.6/forced4.1. Windows hardware/macOS remain unrun |
|9 Premake/hardening/final parity |60f1733,80f0fce,9ed63ed,01c1a3e,967f85c,e3cf646,eebab04 |Complete Premake shader SDK and root Debug/Release builds, standalone managed builds,66 fresh local desktop checks and38 Windows software checks passed. Final source/cleanup/license audit complete; evidence above |

## Reproduce locally

Local environment: CachyOS Linux, GCC 16.2.1, GNU Make 4.4.1, current development
Premake, 8 GB, native Intel HD4000. Earlier stages used Mesa 26.2.3; final native
checks used 26.2.4-arch3.1. The migration did not change system packages/drivers.
A new checkout can use installed Mono development tools as documented in README;
this machine's preserved isolated SDK/CSC command is:

```sh
python3 scripts/dependencies/build-shader-tools.py --jobs 2
premake5 --migration-tests --shader-tools --mono-root=build/dependencies/mono/linux/usr gmake
make config=debug -j2 --jobserver-style=pipe CSC='/home/caelestia/Projects/Hazel/build/dependencies/mono/linux/usr/bin/mono --config /home/caelestia/Projects/Hazel/build/dependencies/mono/linux/etc/mono/config /home/caelestia/Projects/Hazel/build/dependencies/mono/linux/usr/lib/mono/4.5/mcs.exe'
python3 scripts/migration/desktop-checks.py --config Debug --stage final
# Then build Release with the same CSC and run its suite, sequentially.
# Standalone example: generate/build in Hazelnut/SandboxProject/Assets/Scripts.
```

Use an absolute --mono-root for a separate verification checkout. GNU Make 4.4's
pipe flag avoids this host's diagnosed FIFO jobserver failure; older Make already
uses pipes. Windows commands are in README. Manual CI input rebuild_shader_tools
forces fresh pinned dependency compilation. Routine logs belong in ignored output
or CI artifacts. No default-branch merge is part of this migration.
