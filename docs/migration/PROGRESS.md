# Migration progress and resume record

Updated 2026-10-01. Branch `migration/upstream-1feb705`; baseline/default branch `b030be7`. Fixed upstream `1feb70572fa87fa1c4ba784a2cfeada5b4a500db`. Branch pushes for CI are authorized; never merge into master. Preserve unrelated work, inspect actual status/processes before resuming, and build/test sequentially with at most two jobs. No unrelated work or system changes were introduced.

## Resume here

Stages2–8 and stage9 implementation/cleanup are committed. Current source checkpoint
is967f85c (license at repository root), after9ed63ed (capabilities/settings/cache/
integer-blend hardening and concise records) and01c1a3e (actual upstream generic
example and source provenance). No unrelated changes were found or discarded.
Final clean-checkout verification is active; do not declare acceptance yet.

| Gate | Actual evidence/status |
| --- | --- |
| Premake shader SDK |80f0fce / [Actions36939167744](https://github.com/cringlekaden/Hazel/actions/runs/36939167744) passed both OS Debug/Release SDK/root compilation and CPU/tool/Windows software runtime. Complete optimizer, HLSL, reflection, generated inputs and MDd/MD retained.60f1733 Windows timer failure was diagnosed/fixed, not counted as success |
| Hardened renderer/cleanup |9ed63ed / [Actions36941013405](https://github.com/cringlekaden/Hazel/actions/runs/36941013405) passed both OS Debug/Release compilation, CPU suites and17 Windows SOFTWARE graphics/editor/close checks per configuration. Local Linux Debug full build and30 desktop checks passed on native Intel4.2, forced Mesa4.1 and software4.6 |
| Restored actual generic example |01c1a3e / [Actions36942248796](https://github.com/cringlekaden/Hazel/actions/runs/36942248796) passed both OS builds/CPU suites and19 Windows SOFTWARE checks/config, including example graceful close. Native final suites now33 checks/config |
| License cleanup checkpoint |967f85c / [Actions36943018984](https://github.com/cringlekaden/Hazel/actions/runs/36943018984) currently running. Windows Debug passed; Release building. This changes no engine semantics |
| Fresh local build |build/migration/clean-checkout began from9ed63ed with no generated/installed/build artifacts; now fast-forwarded on its migration branch to967f85c, dependency inputs unchanged. All submodules are initialized and clean. CMake is absent from PATH. SDK Debug compiled/installed; Release SDK is compiling with two jobs in helper session10921. Only this local compiler process is running |
| Remaining local gates |After SDK finishes: fresh root Debug build/33 desktop checks, then Release build/33 checks, then standalone managed Debug/Release compilation, sequentially. Original ignored Mono SDK/artifacts preserved; old original Release SDK is not used as fresh verification evidence |

Full hardening checks verify default/forced GLSL paths, driver/1000-slot clamping,
two-slot batching, all primitives/text/texture/scene/physics/managed/project/queued
reload/editor functions and graceful shutdown. Warm caches are reused without
rewrite; changed-source, truncated and valid-header corrupted payloads recover
correct color/entity output. Comments retain the legacy shader path. Diagnosed
native integer blending now uses backend indexed state; no-clear recovery yields
64,64,192/73/0 and restores caller state. These checks must also pass the fresh
Release build before final acceptance.

Cleanup removed129 routine raw logs/CI metadata from tracking (retained under
ignored build/migration/archived-records and Git history). Four concise records
replace PLAN/COMPARISON; actual source/blob imports, baseline comparison, pins,
licenses and useful regression scripts remain. Logs go to ignored evidence or
CI artifacts. Vendor Makefiles and unrelated configuration are preserved.
Standalone project generation passed; separate CI compilation gates are being
added for both OS/configurations. No local runtime/debugger is currently running.

## Completed stages and checkpoints

| Stage | Checkpoints | Actual evidence |
| --- | --- | --- |
|0 Baseline/preparation |a20b1ba |Inspected clean tree/submodules, actual upstream checkout, baseline full Debug/Release builds, initial Sandbox startup; comparison/preservation/plan recorded before code |
|1 Renderer foundations |81b3861 |Linux native UBO/integer/matrix GPU checks. [Actions36801823201](https://github.com/cringlekaden/Hazel/actions/runs/36801823201) passed both OS Debug/Release compilation; Windows runtime was not tested by that run |
|2 Core/platform/lifetime |92eb60a |Debug/Release lifetime/partition/queue/input/args and graceful native Sandbox checks; duplicate glfwInit and destruction order fixed. [Actions36807370723](https://github.com/cringlekaden/Hazel/actions/runs/36807370723) both OS compilation |
|3 Shader/texture/framebuffer |b0bc147, ba10ef8,25362a8 |Full optimizer/reflection/cache/GLSL410/native-SPIRV/texture/MSAA/picking/state checks on native4.2, forced4.1/software4.6. [Actions36881572888](https://github.com/cringlekaden/Hazel/actions/runs/36881572888) both OS builds/tool runtime |
|4 Source prerequisites/full scenes |738fb32,8948e89,d39a2ad,9c80938,d69369f,f9f55c6,04b5622 |Actual source graph required font/rendering/physics/Mono/project prerequisites before full Scene integration; subsequent stages kept dedicated gates. [Actions36921981641](https://github.com/cringlekaden/Hazel/actions/runs/36921981641) both OS builds/CPU scene/script/reload/watcher tests; local full GPU scene/configuration checks |
|5 Complete2D/text |f37b3e6 |Debug/Release thirteen desktop checks/config; all primitives/text/UV/tiling/rotation/capacity/texture/font flushes, generic submit and failed-constructor recovery. [Actions36924603403](https://github.com/cringlekaden/Hazel/actions/runs/36924603403) both OS builds/CPU |
|6 Physics |c86d1dd |All body types, box/circle contacts/materials/gravity/transforms, pause/step/copy/restart/live duplicate/remove/add. Reproduced duplicate null-body crash before scoped synchronization fix. Debug/Release builds/ten desktop checks/config; [Actions36927686042](https://github.com/cringlekaden/Hazel/actions/runs/36927686042) both OS builds/CPU |
|7 Managed/projects |f3eff4a,aaf145a |Actual Player/Camera and project assets, component/internal-call/Unicode-text checks; nested metadata crash fixed using actual Mono tokens. Debug/Release sixteen desktop checks/config. MSBuild deployment uses TargetPath. [Actions36930517894](https://github.com/cringlekaden/Hazel/actions/runs/36930517894) both OS builds/CPU |
|8 Actual editor |cf30569,1505642,7096803 |Debug/Release twelve local desktop checks/config, actual editor/panels/docking/gizmos/workflows/scenes/scripts/payloads. Strict Windows cleanup failure diagnosed as Mono's retained debugger logfile, fixed using supported stdout while preserving debugger. [Actions36935755402](https://github.com/cringlekaden/Hazel/actions/runs/36935755402) both OS Debug/Release compilation and CPU suites, all17 Windows SOFTWARE WGL/editor/graceful-close checks/config across llvmpipe4.6/forced4.1. Windows hardware/macOS remain unrun |
|9 Premake/hardening/final parity |60f1733,80f0fce,9ed63ed,01c1a3e,967f85c; final verification active |Premake SDK/root both OS Debug/Release and Windows software suites passed CI. Local hardened Debug passed; fresh local Debug/Release and standalone project gates pending above |

## Final acceptance requirements

1. Concise progress/preservation/provenance/known-limitations records; raw routine
   logs ignored/artifacts, useful tests/pins/licenses/build scripts retained.
2. Clean checkout builds through Premake without CMake, retaining shaderc/SPIRV-
   Cross compilation/optimization/reflection/cache/generated inputs and matching
   Windows CRT. Both OS Debug/Release SDK CI passed; fresh local both configs pending.
3. Consolidated capabilities/settings and selected shader paths; preserve native
   HD4000 features, test-only overrides/software, audit damaged-cache recovery.
   Actual upstream inspection found only RenderCaps TODO; records are local extensions.
4. Future Mac/Metal architecture through separate OS/graphics adapters and portable
   scene/project APIs, without implementing or claiming another backend/OS.
5. After cleanup, clean Linux/Windows Debug/Release builds, relevant CPU/graphics/
   editor/close and standalone project checks, final parity/limitations report and
   local checkpoint. No merge into default/master. Windows runtime evidence must
   identify software rendering; hardware/macOS/manual interactions remain unrun.

## Reproduce locally

Environment: CachyOS Linux, GCC16.2.1, GNU Make4.4.1, Premake5-dev, Intel HD4000 native OpenGL4.2/GLSL420,8GB. Current native Mesa26.2.4-arch3.1; earlier stages26.2.3. No system driver/package change by migration. Forced Mesa4.1 and software profiles are test-only, not hardware/Mac validation.

```sh
python3 scripts/dependencies/build-shader-tools.py --config Debug --jobs 2
# Then Release, sequentially.
premake5 --migration-tests --shader-tools --mono-root=build/dependencies/mono/linux/usr gmake
make config=debug -j2 --jobserver-style=pipe CSC='/home/caelestia/Projects/Hazel/build/dependencies/mono/linux/usr/bin/mono --config /home/caelestia/Projects/Hazel/build/dependencies/mono/linux/etc/mono/config /home/caelestia/Projects/Hazel/build/dependencies/mono/linux/usr/lib/mono/4.5/mcs.exe'
python3 scripts/migration/desktop-checks.py --config Debug --stage final
# Repeat Release after Debug finishes.
```

Routine logs go to ignored build/migration/evidence. Dependency manifests/licenses live in ignored install prefixes; all source revisions are checked before/after building. Preserved vendor Makefiles are never recursively removed. CI builds with native Mono on Linux/exact upstream SDK on Windows. Windows software runtime uses checksum-pinned process-local Mesa DLLs and two llvmpipe threads; neither a system driver installation nor hardware claim.
