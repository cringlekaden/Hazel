# Migration progress and resume record

Updated2026-10-01. Branch `migration/upstream-1feb705`; baseline/default branch `b030be7`. Fixed upstream `1feb70572fa87fa1c4ba784a2cfeada5b4a500db`. Branch pushes for CI are authorized; never merge into master. Preserve unrelated work, inspect actual status/processes before resuming, and build/test sequentially with at most two jobs. No unrelated work or system changes were introduced.

## Resume here

Stages2–8 implemented and verified; stage9/final acceptance is active. Current commits: `7096803` fixes diagnosed Mono logfile lock; `60f1733` adds complete Premake shader workspace/generators; `80f0fce` matches pinned SPIRV-Tools timer/configuration macros. Local Linux Debug Premake SDK, full engine build and toolchain test passed. Release SDK remains the earlier CMake-built installation until explicitly replaced and verified. New capability/settings/cache hardening and documentation cleanup are migration-owned working-tree edits, not yet a successful checkpoint.

Stage9b full Linux Debug compile/link and all30 sequential desktop checks passed
(0): native Intel4.2, forced Mesa4.1 and software4.6. Default and forced GLSL paths,
1000-slot clamping/two-slot batching, primitive/text/texture/scene/physics/managed/
project/queued reload/editor tests and both apps' graceful native close passed.
Warm cache reused files without rewriting; changed-source, truncated and valid-
header payload corruption recovered with correct color/entity output. Shader
version comments retain the legacy path. Native integer-blend probe was resolved
by backend indexed-state handling; final no-clear recovery yields64,64,192/73/0.
Reduced-slot batch test now respects white reservation and still verifies final
picking. The earlier Debug suite finished with no surviving process; current clean-checkout processes are recorded below.

This is a buildable Debug hardening/cleanup checkpoint, not final acceptance.
Next create a fresh verification checkout of this migration checkpoint, build
its pinned SDK Debug/Release and root engine Debug/Release with Premake only,
then run both30-check suites and standalone example build. Keep original ignored
artifacts/Mono SDK, do not delete vendor Makefiles or touch unrelated work.
Local Release SDK has not yet replaced the earlier CMake-built installation.

Premake correction80f0fce / [Actions36939167744](https://github.com/cringlekaden/Hazel/actions/runs/36939167744) passed Linux/Windows Debug/Release SDK/root compilation, optimized GLSL/HLSL tool validation/reflection, all CPU scene/project/physics/Mono/reload/watcher suites and all17 Windows SOFTWARE graphics/editor/graceful-close checks/config. This verifies the Premake integration on both OSes; final cleaned/hardened9ed63ed CI is [Actions36941013405](https://github.com/cringlekaden/Hazel/actions/runs/36941013405), currently building. Prior60f1733 Linux passed but Windows timer failure was investigated/fixed, not treated as overall success.

Fresh local checkout at build/migration/clean-checkout is exactly9ed63ed, initially clean with no generated/install/build artifacts. Source submodule initialization is running (session30105); it is independent Git I/O, not a compiler job. Its SDK helper is compiling Debug then Release with two jobs (session10921); only one local build runs. CMake is absent from PATH. All logs stay in original ignored build/migration/evidence/final-clean-*.log. Do not start another compiler/runtime until the SDK helper finishes. Then generate/build this fresh root with external preserved Mono SDK and run30 desktop checks/configuration sequentially, followed by standalone example build.


Working-tree cleanup archives129 routine logs/settings/CI metadata under ignored `build/migration/archived-records` and removes them from tracking. Source/blob import records, baseline comparison, dependency pins, licenses and regression scripts remain. Consolidating into this progress record, PRESERVATION.md, PROVENANCE.md and KNOWN-LIMITATIONS.md; PLAN/COMPARISON content is incorporated before their removal. Raw new evidence stays under ignored `build/migration/evidence` or CI artifacts.

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
|9 Premake/hardening/final parity |60f1733,80f0fce; in progress |Linux Debug SDK/full engine/tool test passed. VS SDK generation confirms six projects, Windows source, MDd/MD/UTF-8 (generation is not compilation). Pending results above |

## Remaining acceptance gates

1. Complete Premake-only SDK integration on both OS/configurations, preserving full shaderc/glslang/HLSL/Tools optimizer-validator/Cross reflection and generated inputs; exact clean pins and matching Windows CRT. Until Release/local and new CI pass, this requirement is unmet.
2. Finish capability/settings and shader-path/cache hardening. Source inspection found only public upstream RenderCaps TODO; the new records are documented local extensions following Hazel specifications/initialization/consumers. Native HD4000 features and GL4.1 fallback remain, with OS code separate from graphics code and no additional backend.
3. Finish concise documentation/cleanup, audit complete source/feature parity, retain useful tests/licenses/pins/scripts, stop tracking raw routine logs. Record portability obstacles and unrun interactions separately.
4. After cleanup, repeat clean Linux and Windows Debug/Release builds and relevant CPU/GPU/editor/close suites, including default/forced GLSL settings, texture batching and damaged-cache recovery. Validate standalone example Premake. Publish final parity/limitations and final local commit; no merge.

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

Final parity audit: restore actual dormant target ExampleLayer, with its missing
basic-texture asset recovered from actual public history7d120fb4 under a separate
asset name. Provenance is recorded; --example-layer selects it without replacing
Sandbox2D. Add owned graceful-close checks to both OS runners (final Linux33 and
Windows19 checks/config). Compilation/runtime for this addition is pending.
Standalone example Premake generation passed but exposed unignored generated
Makefile/.make/.csproj outputs; add scoped ignore rules. Update the fresh checkout
to this source-only correction while SDK compilation continues; shader dependency
inputs remain identical. After SDK finishes, run fresh root builds/runtimes and
standalone managed compilation.
