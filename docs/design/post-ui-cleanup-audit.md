# Post-UI cleanup and architecture audit

> **Current status:** [Final cleanup disposition](#final-cleanup-disposition). The original audit and earlier implementation appendices below are historical checkpoints. Their pending/“next slice” statements describe those checkpoints, not additional current scope.

Audited **`feature/runtime-ui-hardening` at `650d09ea23dd9db309fdc6774e643f4e288f4c8e`**. Inspection date: 2026-10-27 America/New_York / 2026-10-28 UTC. This is an audit and bounded implementation plan. It does not implement repairs, restore CI, merge branches, or certify release readiness.

## Assessment and decision

The agreed runtime UI implementation endpoint is present. The accumulated slices form one system: Scene owns RuntimeUI; measured geometry supplies one published snapshot; the existing router handles composed controls; screen and control notifications share an owner-addressed stream; world/UI text share CPU shaping and glyph pages; editor property and structural commands share document history. Preserve these foundations. There is no evidence-based reason to replace them with a widget framework, an asset database, a backend framework, or another UI feature programme.

The branch should receive a bounded cleanup before macOS. The first slice should repair three confirmed problems: Windows compilation with precompiled headers, the typography test target's undeclared GLM include dependency, and managed string truncation before native validation. Documentation must change with those repairs. Subsequent work should remove the superseded raw-font asset path, reduce duplicated reference/validation authority, make the canonical workflow and CI coverage explicit, and separate everyday documentation from checkpoint evidence.

Nothing found requires extending the agreed UI feature endpoint. Broken Windows/Linux build gates and the string-boundary defect block final integration confidence, not completion of another UI feature family. The owner has successfully tested the Sandbox tutorial. Physical Windows/controller/monitor/DPI acceptance and competent-reader typography acceptance remain outstanding. Automated device samples, software graphics, screenshots, and the owner's tutorial result do not establish those other acceptances.

Sequence remains: audit → bounded repair/documentation/CI implementation → final acceptance and branch integration → macOS/OpenGL 4.1 → 3D → PBR/IBL. No integration is performed here.

## 1. Baseline, preservation and method

### Exact repository state

| Item | Observed state |
| --- | --- |
| Branch / HEAD | `feature/runtime-ui-hardening`, `650d09ea23dd9db309fdc6774e643f4e288f4c8e` |
| Remote feature branch | `git ls-remote origin` returned the identical SHA; no newer remote feature work at inspection |
| Comparison | Local `master`, `origin/master` and merge base are `f50c5559d015086573a1c3848d6c9490d23e3073`; compare `master...650d09e` |
| Expected integrations | `c32c760` session audio/exit; `b4789ca` ordinary HUD owner delivery; `a734cfb` examples/tutorial; `650d09e` local qualification record are all ancestors of HEAD |
| Initial tracked changes | Only ` M examples/Sandbox/Assets/Scenes/Start.hazel`; no staged changes |
| Draft identity | Adds `UI Canvas` UUID `4725327554835410179` and `UI Button` UUID `1233722617798641945` to the existing Start scene; **SHA-256 `da6c537dfb525efd0e70fa993a4081323af4c6c336030c658e59d388ae285219`** |
| Stash | `stash@{0}` = `2948648a2332d8bb809fe893cd8aa554265bd6b4`, “On migration/upstream-1feb705: Preserved user Hazelnut layout before authorized master migration merge”; contains only historical `Hazelnut/imgui.ini` |
| Layout seed | Stash layout blob and current tracked `Hazelnut/Resources/imgui.ini` both `5a7fca6d0231d583e4e2d6df6fb67c9311cecf65`; the old `Hazelnut/imgui.ini` working file is absent |
| Local configuration | Ignored `.vscode/`, user preferences/session/workspace/layout files, caches, binaries, dependencies and distributions are existing user/developer state; none is cleanup scratch space |
| Instructions | No applicable `AGENTS.md` found in repository/ancestor locations or first-party subtrees. `docs/migration/PRESERVATION.md` retains ownership/pin requirements; its old branch restriction is explicitly superseded there. This run follows the user's current audit-only instructions |

Protected VS Code SHA-256 values: `settings.json` `bec0cdb45112bd6e31c31cb851a58bcaf936a88cee4c062bb1c4eec3b7ffb406`; `tasks.json` `660eba323e7c53d1ea406d042e7849ab9b0e250529a2d7982f22bc1117b29f6a`; `launch.json` `ce56ce53bac2e21b72714cceece621ca98b333a73cbc19c97c4badb2e994c778`. Preserve `.vscode/README.md` too. These files are not maintained public documentation.

The actual editor profile is `/home/caelestia/.local/share/Hazel/Hazelnut`. Recorded `preferences.yaml` SHA-256 is `0d0ce23ce2de2f7805b46f283e38edafeb6d2b3a17adff6e1cbb6af6b7e3fb51`, `session.yaml` is `cadc02b45729b276b83d9b7adb48c587d8d967c655a73cf9799c855b04185b3e`, and `imgui.ini` is `22a2d77265c3cade3fa757431fec2a7426970f3c80c1d31ce8ce5794a6d8fbc1`. A read-only manifest of all 49 existing profile/VS Code files, excluding Python bytecode, was recorded outside the repository for final preservation checks. Absolute paths here identify audit evidence, not public setup commands.

### Dependency identity

All 14 recursive submodule working trees were clean. Preserve these exact gitlinks; do not use `submodule update --remote` or normalize vendor trees:

| Dependency | Commit |
| --- | --- |
| Box2D | `80e17bef53f217fa8b1696718a047e29e9f25def` |
| GLFW | `026a148d7dd78d597de380c4e77ca0869f0ceaab` |
| ImGuizmo | `218d60bde7d22061ac525d0d71e05360b4dcf978` |
| SheenBidi | `cfe430e7375a7845b679adae9d51dac6deaa8858` |
| GLM | `8d1fd52e5ab5590e2c81768ace50c72bae28f2ed` |
| HarfBuzz | `b42511e071162fe76102f613a6ccc009726c99af` |
| ImGui | `e25e4526cd41cd6536194de098540d54244f54e9` |
| libunibreak | `28a2756b864c343f438cd22537d49d394d4666a5` |
| msdf-atlas-gen | `b50e101d24b1f6009841ce3a386e1bc9365dc66a` |
| nested msdfgen | `b9061f976e79fcc0b20ac6fcd5abaa8fafaf6f91` |
| nested FreeType | `2d57b0592805c76d676b51fbf9553de71c5a5c78` |
| nested dlg | `d142e646e263c89f93663e027c2f0d03739ab42d` |
| spdlog | `57cb5fb7a8ff30079751728234623230535a5c92` |
| yaml-cpp | `25be1f208665b9732ba40aa7b05ec2da97104192` |

Non-gitlink pins also stay intact: `scripts/internal/toolchain.json` selects Premake `71f2d33946947e9cf704f00c24200381e360f593` and PyYAML 6.0.3; `scripts/dependencies/shader-tools.json` selects shaderc `39aa522785f130130927cd4766a37e8813af6d66`, glslang `76b52ebf77833908dc4c0dd6c70a9c357ac720bd`, SPIRV-Headers `124a9665e464ef98b8b718d572d5f329311061eb`, SPIRV-Tools `e553b884c7c9febaa4e52334f683641fb5f196a0`, and Cross `2de1265fca722929785d9acdec4ab728c47a0254`. Mono acquisition is specified in `scripts/dependencies/mono.json`; checked-in Windows SDK/profile files are intentional dependencies. Miniaudio 0.11.22 is source-pinned at `350784a9467a79d0fa65802132668e5afbcf3777` with source/license hashes in its `PROVENANCE.json`. Typography pins are 12.3.2 / 8.0 / 3.0.0, with Unicode 17 data. Git describe labels alone are not reliable dependency versions.

### Local constraints and checks actually performed

Host: CachyOS/Arch x86_64, Linux `7.2.7-1-cachyos`, GCC 16.2.1, Clang 23.1.4, Python 3.14.8; isolated Mono 6.12.0 SDK under `build/dependencies/mono/linux/usr`. The project-owned Premake reports `5.0.0-dev`. Direct system Premake invocation from the checkout fails on absent `/usr` Mono headers; the canonical Python tool supplies the isolated prefix. That is not evidence that canonical setup fails.

`glxinfo -B` reports accelerated Intel HD4000, Mesa 26.2.4, maximum core GL 4.2 through display `:0`. This is a capability query, not new GPU acceptance. No Windows or Mac execution occurred. Existing local builds can hide missing declared dependencies: a minimal header compilation with TypographySmoke's include set found **`/usr/include/glm/glm.hpp`**, whereas the Ubuntu CI log shows it missing.

Read-only/small checks: Git/GitHub state and completed logs; public CLI `--help`; source/call-site searches; local Markdown path/fragment traversal; external HTTP HEAD checks; isolated Mono conversion probe; existing Release `MigrationGameUIModelSmoke`; existing Release `MigrationTypographySmoke` with private fixtures. Both existing executables pass. They were not rebuilt, so these passes are sanity evidence for the available binaries, not a clean build of audited HEAD. The first standalone typography invocation passed its core checks then failed because it lacked the cwd-relative Open Sans assets; supplying the actual runner layout passed (D12 below). No full qualification campaign, desktop automation, dependency install/update, package construction, settings change or source mutation was performed.

## 2. Inventory and architecture

The initial tracked inventory contains 1,370 entries, including gitlinks and intentional dependency binaries. First-party areas inspected before conclusions:

| Area | Tracked entries / responsibility |
| --- | --- |
| `Hazel/src` | 242: Core/platform services, project/storage, assets, scene/physics/lifecycle, Mono glue, text, UI, rendering |
| `Hazelnut/src` | 66: application/editor host, Authoring controllers/history/documents, panels, reusable ImGui property helpers/debugger |
| `Hazel-ScriptCore` | 17 including Premake: managed entity/component/UI/storage/game API and internal calls |
| `Nutella` | One maintained source file: standalone CLI, project selection and RuntimeSession host |
| `tools` | Four entries: native HazelProject creation/validation/sheet commands and SpriteAssetAudit metadata/dependency validation, plus Premake files |
| `scripts` | 38: public Python CLI and two thin setup wrappers, internal build/package/test/process helpers, VS Code templates, pins and Premake dependency definitions |
| `tests` | 105: native/model/managed/editor/game checks, headers consumed by smoke executables, fixtures and provenance |
| `examples` | 195: five descriptors, authored assets, project-local C#, licenses, guides and historical art/composition evidence |
| `docs` | 39: 18 Markdown records/guides, 14 migration evidence files and seven design-reference images |
| `.github` | Two workflows; no repository-local issue/PR templates, contribution/support/security guidance or CODEOWNERS |

Source/resources/vendor roots are already understandable. `tests/migration` now contains maintained regressions, not just disposable migration tests. Keep its fixtures and names unless a real workflow benefit justifies a later move; directory naming alone is not a cleanup blocker. Header-based checks are live includes, not dead code simply because they lack their own executable.

### Ownership that should remain

| Boundary | Source evidence and assessment |
| --- | --- |
| Engine / hosts / game | `Nutella/src/NutellaApp.cpp` hosts RuntimeSession; `Hazelnut/src/EditorLayer.cpp` retains authored scenes and runs copies. Game rules, story, journey schemas and settings decisions stay in project C#. No example-only engine UI renderer or script-built interface is required |
| Runtime / scene / scripting | `Scene/RuntimeSession.cpp:Start, Stop, Update, CommitPendingTransition` owns session/storage/input/queued exit and scene transitions. Scene owns ECS, physics/audio and UI. `ScriptEngine.cpp:GetSceneContext` restricts borrowing to the runtime thread; project domain/watchers outlive ordinary same-project transitions |
| Geometry / input / screen / render | `UI/RuntimeUI.cpp:CaptureSources, ResolveSurface, Build, Draw`; `MeasuredLayout.cpp`; `RuntimeUIModel.cpp`; `RuntimeUIScreen.cpp`. One published geometry snapshot, stable painter order and shared clipped eligibility. Screen commits happen outside script iteration; composed dropdown/tab requests reuse that boundary |
| Authored / transient | Components contain authored defaults; RuntimeUI owns values, captures, focus, requests, events and prepared geometry. `Scene.cpp:CopyForScene` clears transient font/layout/physics/native instance data; normal scene copy uses a fresh scene identity |
| History / restoration | `DocumentHistory.cpp:Execute, Undo, Redo, Reconcile`; `StructuralCommand.cpp:CheckSide, Apply`; `UIEditCommand.cpp:Target, Publish, Reconcile`; `Scene.cpp:CopyForAuthoring, PatchAuthoring, PublishAuthoring`. Commands stage before registry publication, keep exact owned fields, and reconcile authorized restored lifetimes. Independent UUID recreation must not satisfy old commands |
| Typography / GPU | `Text/TextLayout.*` owns immutable CPU faces/results and strict source maps. `Renderer/Font.*` owns renderer-thread accepted faces/pages; `Renderer2D.cpp` retains page/texture owners through submission. `ProjectAssets` and components select accepted resources, not system fonts |
| Shutdown | `Application.cpp:ShutdownResources` clears queued captures/layers, stops scripts/watchers, releases project assets and renderer/fonts, then destroys the native window. Scene/runtime UI caches and GPU timer are scene-owned. Maintain this order |
| Settings | `.hproj` stores portable project defaults/renderer requests/UI bindings; RuntimeSession/RuntimeStorage hold player overrides/save namespace. Hazelnut preferences, session, workspace and docking layout belong to user data. Runtime overrides do not rewrite authored defaults or editor preferences |

`Entity` remains a borrowed `(registry handle, Scene*)`, not a durable identity: `Entity.h:operator bool` dereferences Scene. Callers must keep Scene alive and check `BelongsTo` before testing former observations. Current selection/transition code deliberately clears or reconstructs observations. No new reproduced dangling-scene defect was found. Do not replace every borrowed entity with shared ownership. Public snapshots and pointers returned from them also expire on the next publication; retain IDs/reacquire, not element pointers across setters/refreshes.

Ordinary unowned action polling and declared-owner events are two consumer forms over the same bounded delivery system. Keep the former useful Hazel API; do not invent a second event bus or delete compatibility conveniences merely for uniform naming. Managed `TryConsumeEvent` derives its recipient from the executing lifecycle callback. New event envelopes wrap identities in fresh managed objects; comparisons use `.ID`, as the corrected tutorial demonstrates.

## 3. Prioritized findings

Priority A = correctness/CI blockers; B = material ownership simplification; C = repository/workflow/documentation; D = small usability; E = necessary preparation for the later port. “Confirmed defect” means logs, a reproduction or deterministic source control flow establishes the problem. “Risk” is not an established failing user workflow. “Cleanup” does not imply incorrect current output.

### A1. Windows PCH ordering breaks three new translation units — confirmed defect

Evidence: [current completed run](https://github.com/cringlekaden/Hazel/actions/runs/40617455266), Windows job `122583179751`, step **Setup and build Debug (two jobs)**. Initial errors include `RuntimeUIControls.cpp(7): C2871 'GameUI': a namespace with this name does not exist`, missing `RuntimeUI`, `GPUTimer`, `RendererAPI`, `OpenGLGPUTimer`, followed by cascading syntax/member errors. Source includes precede the configured `hzpch.h` in precisely these engine TUs:

- `Hazel/src/Hazel/UI/RuntimeUIControls.cpp:1–2`.
- `Hazel/src/Hazel/Renderer/GPUTimer.cpp:1–4`.
- `Hazel/src/Platform/OpenGL/OpenGLGPUTimer.cpp:1–2`.

The project declares `pchheader "hzpch.h"` in `premake5.lua:231`. MSVC's PCH boundary skips declarations before that header; the observed missing declarations match those misplaced includes. Linux does not exercise the same PCH semantics. This is a source/build portability defect, not an unreliable UI test. MSVC was not rerun locally.

Fix: put the configured PCH first in these TUs using the existing project convention; preserve normal header self-containment. Verify generated VS projects and both Windows configurations, then let the existing suites/package gates run. Do not disable PCH, remove tests, or suppress these diagnostics. Update current build/CI documentation with the verified repair SHA and actual results.

### A2. TypographySmoke depends accidentally on a system GLM installation — confirmed defect

Evidence: same run, Linux job `122583179467`, same setup step: `Hazel/src/Hazel/Text/TextSource.h:7:10: fatal error: glm/glm.hpp: No such file or directory`, compiling `MigrationTypographySmoke/TypographySmoke.o`; make exits with errors 1/2. `tests/migration/premake5.lua:57–69` creates this separate target without `Hazel/vendor/glm`, unlike the main smoke-target loop at lines 19–25. `TextLayout.h` includes TextSource, so this is a genuine public-header dependency. The local host's `/usr/include/glm` masks it.

Fix: declare the pinned GLM include root for that target; check the target's complete public include requirements. Do not install an unpinned system GLM to compensate. Verify on the existing clean Ubuntu runner and MSVC, including linking/running the CPU typography target after this first error is removed. A successful local incremental build is insufficient proof. Keep Premake as the only build system.

### A3. Managed strings truncate before save/path validation — confirmed boundary defect

Evidence: `Hazel/src/Hazel/Scripting/ScriptGlue.cpp:31–38` converts a Mono string with `mono_string_to_utf8`, then `std::string(cStr)`. `SaveData_Write:63–66` and Read use that helper before `RuntimeStorage::SlotPath/Write` reject NUL/invalid names. Prefab/sprite/scene-path entry points also use it. Newer Text/Font/Style bindings already validate UTF-16/NUL explicitly, showing inconsistent boundary policy.

Small isolated probe: constructed a Mono UTF-16 string `journey\0other` with explicit length 13 using the checked-in SDK, then ran the helper's exact conversion. Output: **`Managed UTF-16 units=13; current helper result bytes=7; result=journey`**. No engine files, save slots or player data were written. Source then unambiguously passes the truncated prefix to native storage. `SaveData.Write("journey\0other", payload)` can address `journey`; an embedded NUL in payload can silently shorten accepted data. This is stronger evidence than a hypothetical parser concern, though an end-to-end SaveData invocation was not run in this audit.

Fix: reuse a checked, bounded managed-string conversion at these data/path boundaries; validate complete UTF-16 (including unpaired surrogates/NUL where forbidden) before conversion and native mutation. Retain operation-specific size caps and managed exceptions. Consolidate the existing repeated text/style checks narrowly inside ScriptGlue, without a new compatibility/resolver framework. Review direct `mono_string_to_utf8` sites too.

Meaningful regression: existing managed fixture writes a valid isolated slot, attempts NUL-containing slot/payload and invalid-surrogate replacements, and asserts rejection plus unchanged old value. Add one malformed scene/prefab path test showing no truncated-path request is queued. Verify valid Unicode and normal text/emphasis behavior. Update `docs/runtime-storage-audio.md` with the enforced contract, not a relaxed promise.

### B1. Raw-font asset path is superseded by family ownership — cleanup

`ProjectAssets.{h,cpp}` still exposes `FontAsset`, `FontStatus`, `ReloadFont`, `FontEntry` and a 256-entry raw-face map. Production world/UI text goes through `FamilyFont`, `FamilyStatus`, `ResolveTextFont(s)` and `.hfont`. Repository-wide call-site search finds external raw-face callers only in `FontResourceChecks.h`, `TextCompositingChecks.h`, and `MeasuredSceneChecks.h`. Refresh/direct-face reload service the duplicate map and family map separately.

Fix: migrate those checks to `Font::Load` for low-level resource tests or `.hfont`/FamilyFont for project-ownership tests; then remove the unused raw project API/map. Keep `Font::Load`, accepted-resource retention, explicit reload and low-level renderer testing. Do not remove useful fixtures or the native font resource service. This removes actual parallel authority without changing the family format or introducing a resolver.

Verify failed replacement retains the accepted face/pages, missing-first-load uses engine fallback, retry is explicit, epochs/root ownership invalidate components correctly, and shutdown releases pins before GL. Keep pinned bytes/notices and typography pressure coverage.

### B2. Entity-reference ownership is enumerated repeatedly — cleanup with drift risk

`Scene.cpp:CloneSubtree` (around 398–418) enumerates UI screen/control references for remapping; `Prefab.cpp:Validate` (around 40–55) enumerates them again for internal ownership; `scripts/internal/packaging.py:validate_project` duplicates that list after already calling the native auditor. Python's list includes UITab.Page but omits UITab.InitialFocus; native validation still catches it. This is **not a demonstrated export escape**. It is concrete drift in redundant authority.

Fix: one narrow native visitor for authored entity-reference fields can serve clone remap and prefab ownership checks; preserve the different external-reference policies. Let the native auditor remain authoritative for scene/prefab schema/references, with Python retaining only package inventory/exclusion and managed closure work it actually owns. Use native dependency results rather than a second YAML schema. Keep serialization/exact snapshot fields explicit; a generic reflection framework is unnecessary.

Verify every named reference (screen focus/owner, indicator/group, progress/slider/scroll parts, dropdown, tab page/initial focus, tooltip and script Entity fields) across duplicate, detached prefab and history restore. Internal references remap once; allowed external duplicate references remain external; prefab escape rejects atomically. Retain missing incoming references for author repair as the existing history contract requires.

### B3. Repeated source scans are real; invalidation must retain native-write safety — cleanup

`SceneUI.cpp` refreshes before many getters/setters, focus and input. `RuntimeUI.cpp:ResolveSurface` always calls `CaptureSources` before discovering an unchanged source. This traverses components, compositions, links and dependency bytes. `RuntimeUIScreen.cpp:ConsumeEvent` correctly avoids traversal when no candidate exists, but each queued event consumption can scan again. `.NET` property wrappers often call several such getters; `UIScrollViewComponent.Normalized` reads Offset and Range separately. This is CPU preparation, even when layout generation is reused.

Fix only redundant calls within a proven no-mutation boundary or one combined native query. Keep candidate-delivery checks after arbitrary native/script writes. Do not replace exact comparison with an unproven “dirty bit” while public native mutable components remain supported. Use existing SourceChecks/Resolves/Reuses and focused profiles to assess the change. No frame-rate target or new observer infrastructure is justified by this audit.

### B4. Failure-atomicity edges merit focused checks, not a rewrite — suspected risks

The following differ from the stronger staged history path. They are investigation items, not reproduced ordinary-workflow defects:

| Location / source observation | Smallest useful follow-up / decision |
| --- | --- |
| `ScriptGlue.cpp:UI_SetValue` writes the authored bool, retires subtree interactions, then calls RefreshUI inside a catch; unlike sizing/history setters it does not restore the bool if refresh throws | Inject one layout/resource failure at that refresh and decide whether the supported setter contract promises rollback. If so stage/restore the value and define capture cancellation explicitly; do not claim cancelled events can be resurrected |
| `RuntimeSession.cpp:CommitPendingTransition` validates before stopping old scene, but a failure during target OnRuntimeStart restarts the previous scene after stopping it; startup/OnCreate/audio side effects are not a complete transaction | Use an isolated target with controlled native startup failure. Verify previous session usability, storage/audio/queued requests and error reporting. Document the actual fallback; do not advertise rollback of arbitrary script side effects |
| `ProjectAssets.cpp:Reload` clears sprite-derived caches and increments epoch before candidate sheet/image validation; failed sheet replacement caches an error. Fonts/styles instead retain accepted resources | Exercise invalid replacement → repaired replacement in Edit and Play. Preserve the established sprite diagnostic/repair policy unless a real regression warrants changing it; make docs distinguish asset-specific replacement behavior |

History's allocation-before-publication and exact restoration tests are substantive protections. Do not weaken them to simplify these checks. Structural staging's temporary full scenes/YAML and accepted dirty baselines are outside retained history's advertised 64 MiB; documentation must continue saying so.

### C1. Canonical commands are sound but entry guidance conflates build/export operations — documentation/workflow defect

`hazel.py` owns setup/build/run/script-build/package/test/database and editor-export. Thin `.sh`/`.ps1` setup wrappers are useful OS entry points, not redundant implementations. VS Code maintained templates invoke the same CLI; editor jobs use the same script-build/editor-export. Keep this arrangement.

However README's package example follows Debug-only setup without a Release engine build, and its prose says export builds Release scripts while `package()` merely validates/packages existing outputs. **`editor-export`** builds selected Release native targets and scripts. The plain package subcommand does not. Publish the precise command ladder in D2; do not change package semantics silently just to match prose. `run` stages existing binaries; it is not build-and-run. App help accepts options beyond the Python `run` wrapper's intentionally small argument set; document which interface is used.

### C2. CI trigger/coverage duplication obscures maintained gates — cleanup

`c-cpp.yml` has a historical branch-name allowlist; `editor-usability.yml` pushes only on `feature/editor-usability` and additionally runs on path-selected PRs. Both perform full Debug/Release builds and general `hazel.py test`; the latter's “bounded editor” name understates its actual suite. The editor workflow supplies a Release `gl41` regression profile that the main general suite does not, so deletion without mapping coverage loses a useful dimension.

Consolidate into one canonical workflow/gate description with shared command definitions or clearly distinct jobs, preserving Debug/Release, both OSes, software and GL4.1 coverage and relocated package closure. Replace stale branch-specific routing with an explicit maintained push/PR policy at implementation time. Preserve job identity or deliberately update required-check configuration when integrating; this audit did not inspect or change branch protection settings. Do not weaken tests, add retries, hide warnings or call a skipped downstream gate a pass.

The main Windows job uploads application artifacts after package checks but before its later general/game suites; an artifact's presence alone therefore does not mean the entire run passed. Download guidance must select a successful **whole run** and identify SHA/platform.

### C3. Tutorial authoring qualification exists but is not selected in CI — confirmed coverage gap

`tests/migration/EditorExampleIntegrationChecks.h:ExampleIntegrationWorkflowChecks` immediately returns unless `HAZEL_FINAL_UI_PROJECT` is set. Neither workflow sets it. `scripts/internal/game_tests.py` does pass Sandbox to native runtime integration, so tutorial runtime coverage is already real; do not describe the entire tutorial as untested.

Enable the existing private-copy authoring check in an appropriate milestone job after script builds. It exercises property/structural interleaving, prefab instantiation, save/reopen and Play/Stop/exit with authored tutorial data. No new bot or screenshot programme is needed. Require its existing PASS evidence so a missing environment selector cannot masquerade as exercised coverage. Preserve the original Sandbox draft/startup scene.

### C4. SDK preflight can disagree with the configured Mono SDK — source-confirmed false-negative path

`hazel.py:mono_prefix` supports `HAZEL_MONO_SDK`. `scripts/internal/authoring.py:preflight` subsequently checks only `ROOT/build/dependencies/mono/linux/usr/lib/mono/4.5/mcs.exe` and `/usr/lib/mono/4.5/mcs.exe`. A complete explicitly selected SDK can pass native_contract yet fail that hardcoded compiler check if neither default exists. No local installation was removed to trigger this path.

Use the already resolved prefix for compiler readiness. Do not introduce another SDK search. Verify with a complete custom-prefix fixture and absent default candidates using the existing tool-discovery test infrastructure; keep explicit-invalid configuration an error. Update the setup/troubleshooting guide with HAZEL_MONO_SDK and the separation between run-time profile and compiler SDK.

### C5. Historical authoring generators are unsafe as current recipes — cleanup

`examples/LastLightkeeper/Authoring/{compose,cliff_chapter,observatory,tower_chapter}.py` are one-time scene composition evidence, not maintained launch/build dependencies. They construct older camera/transform HUD/menu arrangements and do not encode the completed screen/style/tutorial integration. Their `--overwrite` path can replace subsequent authored work. Do not run/regenerate them to validate this audit.

Retain them as clearly historical source evidence, preferably under an explicitly historical Authoring location with a warning/link to current Hazelnut editing. Remove them from any apparent everyday instructions. Keep `examples/art/generate.py` as the optional original-art provenance tool, with Pillow explicitly optional. Keep `scripts/migration/compare-upstream.py` as a maintainer provenance utility, but document its historical baseline/default output and require a scratch `--output` in current reproduction examples. It otherwise writes into tracked migration evidence.

`tools/HazelProject/main.cpp:measure-scene` is a diagnostic-only command with no maintained caller found. It measures metadata/YAML/exact snapshots, not graphical scene-open time. Keep it only as an explicitly documented maintainer probe if still useful; otherwise retire that branch after preserving its historical findings. The native tool's usage error lists only `create`, despite maintained `validate`, `slice-sheet`, `add-clip` and `set-region` commands. A small native help/usage correction is appropriate; do not add another wrapper or Python authoring route.

### C6. Ignore/output inventory is mostly deliberate — small cleanup

`bin/`, `bin-int/`, `build/`, `dist/`, compilation databases, local `.vscode/`, generated VS and example script projects are ignored. `Hazelnut/mono` and vendor libraries/source are intentionally tracked dependencies, not accidental engine build output. Large `.hazel` scene files are authored content. Migration evidence is about 288 KiB and Lightkeeper Authoring evidence about 1.3 MiB; those sizes do not justify blanket deletion. The duplicated 757,076-byte DejaVu face is intentional test-versus-project ownership with independent notices.

The root ignore file has no general `.cache/clangd` rule; the present index is ignored by its own generated `.gitignore`. Add a narrowly scoped root clangd-cache rule only if adopting that generated directory as a supported developer output. Preserve all existing caches/configuration; do not “clean ignored files.” No recursive vendor cleanup or fixture pruning is warranted.

### D1. Common authoring friction is mostly navigation and terminology — cleanup

Keep `UI/PropertyUI`, normal component fields, named entity selectors, Content Browser asset editors, flat style variants and ImGui helpers. Useful small repairs: use the actual **Project > Project Settings** / **Edit > Editor Preferences** labels consistently; present owner-required versus ordinary unowned button polling at the connection point; explain silent value setters versus queued screen/tab requests beside their API examples; cache wrappers in examples where repeatedly allocating them serves no purpose. Native immediate radio selection versus queued tab selection is purposeful composition behavior, not evidence for a new generic setter/event framework.

The current font/style workflows already expose actual accepted/fallback resources, explicit refresh and local override groups. Improve their getting-started links before adding more panels or systems. Keep implementation budgets in the reference/debugger, not compulsory new-project forms. Root README need not enumerate every resource cap.

## 4. Completed CI evidence inventory

GitHub authentication/API and completed logs were available. No new run was dispatched, polled or awaited. Retrieved failed logs for current HEAD, preceding `a734cfb`, the first failure after the latest successful C/C++ run, and the latest failed editor-usability run; inspected job/step metadata and successful baselines.

| Completed run / SHA | Result and evidence |
| --- | --- |
| [39441163171](https://github.com/cringlekaden/Hazel/actions/runs/39441163171), `79648e2f808c8ece162b566ad6d05733b8b48408`, 2026-10-20 14:39 UTC | Most recent successful C/C++ run discoverable in the queried history. Linux job `118691061525` and Windows job `118691061762` both success |
| [39506977795](https://github.com/cringlekaden/Hazel/actions/runs/39506977795), `1bac10dc74009e84c710a742d4c356a85ce0a4ce`, 2026-10-20 22:17 UTC | Linux succeeds; Windows fails during Debug build in GPUTimer/OpenGLGPUTimer. Intervening source commits `0383019` / `272a5aa` introduced timing/ordered batching, followed by the record commit. PCH issue already present here |
| [40610076929](https://github.com/cringlekaden/Hazel/actions/runs/40610076929), `c32c760`; [40614635731](https://github.com/cringlekaden/Hazel/actions/runs/40614635731), `b4789ca` | Completed failure conclusions found in run list. Their individual logs were not downloaded; no additional cause inferred |
| [40616457690](https://github.com/cringlekaden/Hazel/actions/runs/40616457690), `a734cfb` | Failed logs show the same GLM typography compile failure and Windows UI/timer declaration cascades as current HEAD |
| [40617455266](https://github.com/cringlekaden/Hazel/actions/runs/40617455266), audited `650d09e`, 2026-10-28 01:50 UTC | Linux job `122583179467` and Windows job `122583179751` both fail in Setup/Debug compilation. A1/A2 are independently actionable. Release, application/runtime tests and packaging are downstream and **not qualified by this run** |
| [38054607085](https://github.com/cringlekaden/Hazel/actions/runs/38054607085), master `463cbdd`, 2026-10-10 | Latest successful master C/C++ run returned by the master query. Current master `f50c555` has no successful run returned at that exact SHA; do not promote old master evidence to current feature assurance |
| [38054411823](https://github.com/cringlekaden/Hazel/actions/runs/38054411823), `463cbdd`, 2026-10-10 | Latest discoverable editor-usability workflow run: success. It is historical, not coverage of current feature HEAD |
| [37701773900](https://github.com/cringlekaden/Hazel/actions/runs/37701773900), `4479040`, 2026-10-07 | Latest failed editor-usability run returned: Windows success; Linux Release GL4.1 Renderer2DSmoke exits -6 after repeated window/context creation with `GLFW Error (65550): X11: Failed to open display :99`. This is display-environment failure evidence, not a failing rendering assertion. Later commit `88e1e77` adds Xvfb `-noreset` to both workflows; later editor runs pass. Current workflows retain it. Do not reclassify this historical failure as an active product bug or add retries |

Current failures are not dependency-download, shader generation, runner timeout, packaging or flaky input assertions in the inspected logs. Pinned SDK generation/caching proceeds to native compilation. Logs also contain warnings, including misleading indentation in compressed ScriptGlue and local shadowing. Repair misleading control flow when touching those areas; do not broadly suppress warnings. Later test/package defects may emerge once builds pass; their absence from current logs is not evidence that those gates pass.

After A1/A2, run the existing whole jobs once at the repair SHA, inspect actual completed results, and repair any newly exposed failure on its evidence. Keep Windows dynamic CRT, UTF-8 source, two compiler jobs and explicit clean dependency selection. Cache keys currently cover project-owned shader/Premake inputs; no cache corruption was established. Do not invalidate/update every dependency speculatively.

## 5. Performance and verification discipline

No optimization campaign was run. Existing measured evidence in `runtime-ui-integration.md` is checkpoint/hardware-specific; do not turn it into current frame-budget guarantees.

| Cost / source | Assessment and bounded action |
| --- | --- |
| Source detection, `RuntimeUI.cpp:CaptureSources/ResolveSurface` | Linear per refresh, plus relationship/composition work and exact dependency-byte construction. Reused layout does not imply zero CPU work. B3 targets redundant boundaries while keeping raw native edits observable |
| Changed layout, `RuntimeUIMeasure.cpp`, `MeasuredLayout.cpp` | Existing vectors reuse some capacity; FlowLayout/Grid track copies, child lists, sets/maps and rebuilding changed snapshots still allocate. Use existing `--profile-layout` for unchanged/change cases if modifying this path; retain dependency/cap/hidden-node behavior |
| Submission, `RuntimeUI.cpp:Draw`, `Renderer2D.cpp` | Painter order and clipping deliberately constrain batching. Alternating image/text can require many draws. Adjacent compatible ordered runs and prepared glyph reuse already exist. No texture/font sort that changes transparency/picking order; no second UI renderer |
| Cold shaping/pages, `Font.cpp:Load/Layout/PreparePages/GeneratePage` | Synchronous outline/MSDF/page preparation can stall; offscreen omitted text skips page preparation. Keep explicit admission/failure caching and bounded prewarm. Existing `LoadingUI` reports project/scene phases between work; it does not make every cold glyph generation asynchronous or guarantee a continuously animated loading screen |
| Dirty state, `AuthoredState.cpp:CaptureAuthoredState`, `DocumentController.cpp:Documents` | Exact binary authored snapshots avoid older YAML-per-frame validation, but still allocate/sort IDs and serialize every authored field for scene/prefab checks. Reuse one snapshot inside an unchanged UI pass where provable; do not use a history cursor as dirty truth because native/untracked edits exist |
| Structural history, `StructuralCommand.cpp:Snapshot/Apply` | Whole-scene codec/copy staging is a cost on an authoring command, not a render-frame operation. Retained deltas are bounded separately. Do not trade atomicity/lifetime correctness for arbitrary file-size or allocation goals |
| GPU timing, `OpenGLGPUTimer.cpp` | Nonblocking availability reads and delayed submission-tagged intervals. UI GPU interval can include CPU feeding gaps and excludes presentation. CPU source/layout/prepare/submit, GPU interval, swap/compositor timing and whole frame are distinct |

Useful fast iteration suite using existing infrastructure:

1. Build changed Premake targets incrementally, configuration explicit; cap compiler concurrency at the existing two jobs. Check PCH/native portability in the appropriate compiler rather than relying on one Linux desktop.
2. Run `MigrationGameUIModelSmoke` for router/layout/control geometry changes; `MigrationTypographySmoke` for CPU typography with the runner's private fixture **and cwd assets** layout. Use focused existing Scene/RuntimeSession/Font/Editor checks appropriate to the changed owner; do not rerun all desktop/package cases after each small edit.
3. For A3, run the managed fixture regression in an isolated runtime/storage root. For B1/B2/history, retain failed-replacement, fresh lifetime, interleaved property/structural and no-partial-publication checks. A pure implementation-mirroring unit test is insufficient.

Milestone qualification after the bounded repairs: existing `hazel.py test` (17 sequential regression executables) in Debug/Release on both OSes, existing source-game checks, selected tutorial-authoring hook, existing package and game-package software/GL4.1 profiles, source/SDK/GAC-hidden relocation and license/checksum closure. `test-games` has an additional native example-game executable outside the 17-name general list. Avoid stale “13 tests” totals as current documentation. The public test command presently has no name selector; document direct focused invocations instead of claiming an absent option. A tiny selector is justified only if maintaining the required setup repeatedly causes mistakes; it is not a prerequisite framework project.

Model, scene, managed, renderer and real editor workflow layers catch different faults and should not be collapsed indiscriminately. Minimize repeated assertions that only restate the same helper, retain analytic/reference/behavior tests, and keep bounded owned-window package startup/resize/shutdown checks. Current `game_tests.py:exercise` is lifecycle smoke, not physical full-game playtesting. No new gameplay bot, desktop server or screenshot-report infrastructure is proposed.

## 6. Complete first-party documentation and public GitHub audit

### Method and link results

Inventoried all tracked first-party Markdown/text/license documents (49 files, excluding vendor-owned docs and the ignored local `.vscode/README.md`). Inspected entry points, subjects, current instructions, checkpoint qualifications and source contradictions; long design records were read by relevant sections/headings/contracts rather than treated as proof of implementation. Also inspected the public CLI help, template/pin files, packaging-generated `LAUNCH.txt` text, project provenance JSON, both workflows and migration evidence inventory.

Recursively followed Markdown local links from root README into the first-party document set, checked target existence and heading fragments. **No missing local Markdown target or unmatched checked fragment was found.** This is not validation of every unlinked backtick path or future proposed file. In particular, old completion plans can contain stale instructions while their links still resolve.

Not reachable from README through Markdown document links: maintained `docs/runtime-storage-audio.md`, `tests/fixtures/typography/README.md`, `scripts/dependencies/licenses/typography/NOTICE.md`; historical `docs/design/last-lightkeeper.md`, `runtime-ui-production-audit.md`, migration `KNOWN-LIMITATIONS/PRESERVATION/PROGRESS`, and Lightkeeper Authoring `Captures/README`, `OBSERVATORY-CHECKPOINT`, `TOWER-CHECKPOINT`, `VALIDATION-CHAPTER-ONE`, `VALIDATION`. Legal texts are mostly unlinked; that is not permission to discard them. Runtime-game-ui and Sandbox's guide are technically reachable only through long linked historical records, not discoverable as root-level everyday choices. There is no `docs/README.md` index.

94 distinct external Markdown destinations received unauthenticated HTTP HEAD checks: 76 returned 200, 15 returned 404, three timed out. Fourteen 404s were GitHub artifact URLs requiring authentication; **do not classify them as expired/broken solely from that unauthenticated response**. Authenticated API metadata confirms all six root README artifacts exist and are unexpired, expiring **2027-01-06 09:23:15 UTC**. The eight older editor-authoring artifact IDs likewise exist and are unexpired, expiring **2027-01-01 20:11:16 UTC**. Neither set was download-verified. The remaining 404, Kenney `tiny-wisptle`, is a real wrong asset reference (D7). `hazelengine.com`, its linked screenshot, and the linked `docs.hazelengine.com/WorkingWithinHazel/FirstGameDocumentation` timed out: availability unverified, not confirmed broken. No archive was downloaded during this audit. External plain-text URLs in licenses/provenance are not all included in the Markdown-link count; no blanket external-link validation claim is made.

The GitHub repository's public About description/homepage are empty; no topics appear in the public page. Default branch is master; API identifies Apache-2.0; releases list is empty. No workflow badges occur in current README, so there is no wrong badge branch/workflow to repair. If adding a badge later, point to the maintained workflow and master explicitly and distinguish last successful build from current feature qualification. No settings or public metadata were changed here.

### Documentation classification and disposition

A = public getting-started/everyday reference. B = maintainer/developer reference. C = historical design/migration/verification evidence. Legal/notice entries are retained with the owning A/B subject; their text is authoritative attribution, not editorial cleanup material. “Extract” below means move current instructions into the proposed canonical home while keeping history clearly dated and navigable; do not silently rewrite old evidence into a claim about new HEAD.

| Existing document | Class / current status / proposed disposition |
| --- | --- |
| `README.md` | A, mostly valid introduction but incomplete discovery and build/package/CI claims; compact landing page with direct guides/tutorial and truthful platform/evidence status |
| `docs/runtime-game-ui.md` | A, primary current UI reference; retain canonical home, fix repeated stale structural-history and font/dependency statements, add a compact subject index |
| `docs/runtime-storage-audio.md` | A/B, mostly current storage/audio contract, orphaned; link from index/scripting/UI reference, document actual validated string boundary with A3 repair |
| `docs/authoring-reliability.md` | B, durable ownership/atomic-save contracts mixed with old verification; retain current invariants, label historical verification, link from maintainer architecture/testing |
| `docs/editor-authoring.md` | C with useful A/B material; old one-entity/no-hierarchy/New Project/renderer-deferred claims conflict with later implementation; extract current editor/scripting workflow and mark remainder historical |
| `docs/nutella-runtime.md` | C with useful B contracts; extract current runtime/package reference, keep milestone/13-test/Sandbox history dated |
| `docs/example-games.md` | C, already historical at top; retain milestone evidence; everyday example list belongs in index/project READMEs |
| `docs/design/editor-usability-audit.md` | C, baseline proposals include absent-then features and nonlocal commercial Hazel references; archive/index as initial audit, not “remaining scope” entry point |
| `docs/design/editor-usability-a1-a2.md` | C with B workflow contracts, 958 lines; preserve A–H/source/acceptance evidence, extract actual everyday instructions, stop presenting old verified-source/unchanged-implementation paragraphs as current guarantees |
| `docs/design/sprite-sheet-authoring.md` | B/C, valuable format/identity/sampling contracts but old feature/deferred matrix; extract concise A sprite guide, label baseline/qualification and update current cross-links |
| `docs/design/last-lightkeeper.md` | C, feasibility/content plan and old camera-UI model; orphaned, preserve as history with current game/UI links |
| `docs/design/last-lightkeeper-completion.md` | C, correctly identifies historical content/UI plan at top; keep dated and linked to current UI guide, no renewed feature gates |
| `docs/design/runtime-ui-model-checkpoint.md` | C, explicitly partial historical checkpoint; retain focused design evidence, make current guide the navigation target |
| `docs/design/runtime-ui-production-audit.md` | C, old broad P1–P7 roadmap, explicitly superseded by practical endpoint; archive/index, do not revive virtualization/IME/accessibility/world-space as pre-macOS requirements |
| `docs/design/runtime-ui-integration.md` | C plus B implemented contracts; 3,657 lines of successive checkpoints; preserve evidence, place a latest-implemented-boundary/status pointer at entry, keep everyday API in runtime-game-ui |
| `docs/migration/KNOWN-LIMITATIONS.md` | C, historically accurate limits now misleading as current (Latin-1, New Project TODO, old API); retain as migration snapshot, current limitations get their own reference |
| `docs/migration/PRESERVATION.md` | B/C, ownership/vendor requirements remain valuable; retain explicit supersession of old branch restrictions and link through maintainer/history index |
| `docs/migration/PROGRESS.md` | C, historical clean-build/platform evidence; preserve checkpoints, not current support guarantees |
| `docs/migration/PROVENANCE.md` | B/C, canonical upstream/import attribution; retain immutable import evidence and link to current dependency ledger for post-migration additions |
| `examples/Sandbox/README.md` | A, current five-page tutorial; link directly from root/index, add precise source launch/prebuilt location and SDK prerequisite distinction; replace repository-owner-specific draft wording with general startup instructions |
| `examples/MeadowRun/README.md` | A, commands broadly valid; opening says restart “the meadow” while current behavior restarts current stage; modernize UI/input explanation and shared prerequisite links |
| `examples/Skybound/README.md` | A, commands broadly valid; obsolete world-space mouse-hit/offscreen-control prose after native UI migration; update presentation section from current scripts/assets |
| `examples/LastLightkeeper/README.md` | A, useful play/save/settings guide with contradictory later-UI paragraph and wrong asset pack/link; repair D6/D7 and link current UI/storage/platform guides |
| `examples/LastLightkeeper/Authoring/Captures/README.md` | C, useful real capture versus reference distinction; retain dates/checkpoint context, connect from history index |
| `examples/LastLightkeeper/Authoring/OBSERVATORY-CHECKPOINT.md` | C, says tower/UI not yet available at that checkpoint; preserve as dated historical evidence, not current chapter status |
| `examples/LastLightkeeper/Authoring/REVISION-NOTES.md` | C, loading/dirty/presentation diagnosis and measured evidence; preserve and link from maintainer performance history |
| `examples/LastLightkeeper/Authoring/TOWER-CHECKPOINT.md` | C, completed content with explicitly incomplete-then runtime UI; retain historical scope |
| `examples/LastLightkeeper/Authoring/VALIDATION-CHAPTER-ONE.md` | C, first chapter/platform evidence and old remaining chapters; retain as history |
| `examples/LastLightkeeper/Authoring/VALIDATION.md` | C, revision baseline/CI/physical-acceptance distinctions; retain checkpoint evidence, not current build instructions |
| `tests/fixtures/typography/README.md` | B, good corpus/license limitations; standalone invocation omits new cwd Open Sans requirement; correct D12 and link from testing |
| `scripts/dependencies/licenses/typography/NOTICE.md` | B legal/dependency ledger, matches current linked typography pins/notice copying; retain, link from current dependencies page |
| `LICENSE` | A/B legal, Apache-2.0 retained upstream; keep verbatim and add landing-page license/provenance link |
| `Hazel/Resources/fonts/opensans/LICENSE.txt` | A/B legal, actual engine font notice copied to exports; preserve verbatim |
| `examples/LastLightkeeper/LICENSE.txt` | A legal, Apache-2.0 game source/content license; preserve with project |
| `examples/LastLightkeeper/NOTICE.md` | A legal/provenance, original game plus Kenney attribution; preserve with packaged game |
| `examples/MeadowRun/LICENSE.txt` | A legal/provenance, original art/source; preserve, no third-party rewrite |
| `examples/Skybound/LICENSE.txt` | A legal/provenance, original art/source; preserve |
| `examples/SceneTransitions/LICENSE.txt` | A legal/provenance, generated checker and explicit Kenney frame; preserve; missing project README is D8 |
| `examples/Sandbox/Assets/Fonts/Corpus-NOTICE.txt` | A legal, full pinned DejaVu notice copied with explicit tutorial font; preserve |
| `tests/fixtures/typography/LICENSE-DejaVu.txt` | B legal, identical pinned test-face notice; preserve independently of game import |
| `scripts/dependencies/licenses/typography/LICENSE-Unicode.txt` | B legal, Unicode V3 data notice; preserve and ship with linked libraries |
| `examples/LastLightkeeper/Assets/Licenses/interface-sounds/License.txt` | A legal, imported Kenney original; preserve |
| `examples/LastLightkeeper/Assets/Licenses/tiny-battle/License.txt` | A legal, actual Tiny Battle provenance contradicts Tiny Wisptle prose; preserve original notice |
| `examples/LastLightkeeper/Assets/Licenses/tiny-dungeon/License.txt` | A legal, imported original; preserve |
| `examples/LastLightkeeper/Assets/Licenses/tiny-factory/License.txt` | A legal, imported original; preserve |
| `examples/LastLightkeeper/Assets/Licenses/tiny-town/License.txt` | A legal, imported original; preserve |
| `examples/LastLightkeeper/Assets/Licenses/ui-pack-pixel-adventure/License.txt` | A legal, imported original; preserve included version versus page-version distinction |
| `examples/Sandbox/Assets/Licenses/ui-pack-pixel-adventure/License.txt` | A legal, explicit tutorial art dependency; preserve |
| `examples/SceneTransitions/Assets/Licenses/ui-pack-pixel-adventure/License.txt` | A legal, explicit menu frame dependency; preserve |

Additional maintained guidance/data inspected: the two `.github/workflows/*.yml`; six tracked VS Code template JSON files; `.gitignore`, `.gitattributes`, `.gitmodules`; root/project/tool/test/dependency Premake files; project templates; CLI help/setup messages; packaging `LAUNCH.txt` generator; miniaudio `PROVENANCE.json`; example `Assets/Licenses/PROVENANCE.json` files; dependency manifests. Vendor documentation and Mono's imported material are outside editorial scope. No missing first-party README was hidden by counting dependency READMEs as user guidance.

The 14 tracked migration evidence files are `comparison-summary.json`, `source-comparison.tsv`, `stage2-imports.json`, `stage3-imports.json`, `stage4a-imports.json`, `stage4b-imports.json`, `stage4b-font-pins.json`, `stage4c-imports.json`, `stage4d-imports.json`, `stage4e-imports.json`, `stage4f-imports.json`, `stage7-imports.json`, `stage8-imports.json`, `stage9-example-imports.json`. Retain these as C/B provenance, not generated junk. Seven design-reference images and Lightkeeper reference/capture assets are historical visual evidence, not screenshots of this audited build. Package output/readmes/checksums under ignored dist/build are generated products, not alternative maintained guide sources.

### Actionable documentation findings

Each row identifies affected files, misleading/missing instruction, authoritative implementation and correction. Repeated historical problems are grouped with the complete affected file list above.

| ID / priority | Affected document and claim/missing instruction | Authority / proposed correction |
| --- | --- | --- |
| D1 / C | `README.md:27–29` omits pip/PyYAML readiness, Mono compiler/profile and concrete Linux package instructions; `hazel.py:diagnose` tells users to see README Ubuntu/Arch commands that do not exist | `hazel.py:diagnose, yaml_tools, mono_prefix`, workflow apt list and `mono.json`. Add a platform setup guide with checked package lists, VS2022/.NET 4.7.2, Python 3.9+, git submodules, optional Bear/debugger, exact isolated SDK behavior and actionable diagnostics. Setup never installs system packages; downloading checkout-owned pinned tools/PyYAML is distinct |
| D2 / C | `README.md:55–62`: Debug-only setup then Release script-build/plain package; export prose implies package builds scripts | `hazel.py:main`, `packaging.py:package`, `authoring.py`. Show explicit Release native build → Release script-build → package, or use `editor-export` with required `--name/--output`. Keep run config explicit. Explain setup/build/run/script-build/package separately. MeadowRun/Skybound/Lightkeeper READMEs already include a Release build step; link shared canonical instructions |
| D3 / C | `README.md:38` says generated builds live in `build/` | Premake targetdir/objdir and `hazel.py:binaries/script_build`. State native products `bin/<Config>-<OS>-x86_64`, native intermediates `bin-int`, tools/dependency installs/generated projects under `build`, project assemblies deployed to Assets/Scripts/Binaries, distributions `dist`. Do not move working outputs merely to make old prose true |
| D4 / C | `README.md:65–73` routes users through audits and says “CI checks…” without current failure status; tutorial/UI/storage lack direct navigation | Actual workflow/run evidence, current `runtime-game-ui.md`, Sandbox guide. Lead docs links with current guides; label intended CI gates and link latest completed results. Keep old success evidence attached to its SHA. No production/CI-restored claim until repair gates pass |
| D5 / C | `README.md:5–17`, `docs/editor-authoring.md` downloads, `docs/nutella-runtime.md` “direct verified downloads”; old artifacts treated as ongoing distribution route | GitHub API: README's six artifacts exist, unexpired until Jan 6; old `a31187d` excludes later runtime UI. Say exactly what snapshot includes. Link successful whole runs with login/retention/checksum instructions; optionally publish manually selected durable releases after qualification. Do not imply an automatic latest release exists or delete valid links as expired |
| D6 / C | `examples/LastLightkeeper/README.md`, “Authoring responsiveness, verification and next chapter”: UI remains a later integration milestone, contradicting its opening and Settings section | Current authored screens/TitleUI/SettingsUI and Presentation/Settings C#, integration commits. Replace with current UI guide/tutorial link; retain human pacing/physical acceptance caveats and historical records separately |
| D7 / C | `examples/LastLightkeeper/README.md:140` uses “Tiny Wisptle” / `https://kenney.nl/assets/tiny-wisptle` (404) | Project `Assets/Licenses/PROVENANCE.json`, the actual `tiny-battle/License.txt`, Art assets and `docs/design/last-lightkeeper.md:40–49` identify **Tiny Battle**. Correct current README name/link to actual source without altering imported notice text or licensed source bytes |
| D8 / C | No `examples/SceneTransitions/README.md`; Sandbox guide assumes locating/opening/building the project with prior repository knowledge | Descriptors, `hazel.py run/script-build`, Nutella CLI and existing Sandbox guide. Add a short SceneTransitions project guide; give Sandbox an explicit prepared-checkout launch and prebuilt-package location explanation. Root/index should list five examples and identify tutorial versus gameplay versus focused lifecycle example |
| D9 / C | `docs/editor-authoring.md`: “no hierarchy,” single-entity prefabs, New Project build failure leaves destination unchanged, project renderer persistence deferred; historical commands mixed with contracts | `ProjectCreationController`, native `ProjectCreation`, `authoring.py:create_project` and `tests.py:authoring_checks` deliberately retain editable creation on later compile failure; current Prefab v2/Hierarchy/RendererRequests. Extract current instructions; label the original baseline/design as C. Document native creation without tools, later optional script build and preserved project on build failure |
| D10 / C | `docs/migration/KNOWN-LIMITATIONS.md`: Latin-1/font persistence/NewProject/upstream API limitations read as current; `docs/design/sprite-sheet-authoring.md` defers nine-slice and generic Undo while later UI supports bounded cases | Current FontFamily/Text services, native NewProject, Scene UI/nine-slice/history; existing legacy document policy in `DocumentSchema`/UI config. Preserve historical scope and link current supported limits; distinguish UI nine-slice/property+structural history from unimplemented general animation/document features |
| D11 / C | `docs/runtime-game-ui.md`, repeated screen/value/dropdown/tab/style sections still say structural authoring/history is separate or checklist-pending; Font Family section says milestone adds no fonts/dependencies | `DocumentHistory`, `StructuralCommand`, current tutorial; `typography.lua`, actual gitlinks and Corpus notice. Replace stale repeated caveats with one link to the exact supported-history scope; state real pinned typography deps and explicit Sandbox DejaVu import. Preserve no-general-property-Undo/no-asset-file-Undo boundaries |
| D12 / B | `tests/fixtures/typography/README.md` says supply a fixture directory to run CPU check, omitting cwd-relative Open Sans in `TextEmphasisChecks.h` | Reproduced missing-fixture failure after core conformance passes; private `assets/fonts/opensans` layout then passes. Document both fixture and working-directory resources or give one safe runner recipe; no installed/system-font fallback |
| D13 / C | Root, `docs/editor-authoring.md`, `docs/nutella-runtime.md`, `docs/design/editor-usability-a1-a2.md`, `docs/example-games.md` mingle older 13-test/verified-build assertions, old Sandbox names and old branch status with current navigation | Current 17-name general runner, additional native game suite, master/feature SHAs and run table. Keep milestone counts and old Sandbox evidence as dated C; use actual Hazelnut/Nutella/current examples and current test suite in A/B. No need to erase accurate historical observations |
| D14 / C | Settings/cache paths and ownership are scattered; old `docs/editor-authoring.md` says no workspace taxonomy while current workspaces exist | `Resources.cpp`, `EditorPreferences.cpp`, `EditorState.cpp`, `ImGuiLayer.cpp`, `RuntimeStorage.cpp`. One current reference for resources/Mono roots, HAZEL_DATA, platform user-data/Hazel/app, preferences/session/workspaces/instances/layout/recovery/cache, games namespace and HAZEL_SAVE_ROOT. Document project defaults versus player overrides versus user/workspace state; preserve existing files |
| D15 / C | `examples/MeadowRun/README.md`, `examples/Skybound/README.md` retain camera/world mouse-hit descriptions after native UI migration; MeadowRun first paragraph restart wording lags multilevel behavior | Current project C#, authored Canvas/Screen/Rect assets and RuntimeSession input. Describe gameplay world-coordinate queries separately from screen-space control routing; correct stage restart/result-screen descriptions without changing gameplay |
| D16 / C | No current documentation index, platform/dependencies/troubleshooting/maintainer-testing home, contribution/support entry; root has no license/provenance section | Existing source/tooling/contracts and LICENSE. Add the bounded navigation below, with a short contribution/testing/bug-report path. Include OS/GPU/driver/config/SHA/log/isolated reproduction expectations; do not invent a support commitment or copy vendor contribution rules |
| D17 / C | First-party guides mention licenses but don't provide one obvious dependency/export-notice map | `packaging.py:licenses/copy_project/validate_sprites/validate_engine_fonts`, actual typography/miniaudio/example manifests. Link engine Apache, imported dependency licenses, Open Sans, explicit project DejaVu/Kenney and package `licenses/`/Assets notices. The package copies original notices and project README; no contradictory notice omission was established. Dependency source attribution is not a legal assurance about arbitrary user imports |
| D18 / D | About/website/topics empty, no current workflow badge; external commercial Hazel design links unavailable | Public page/API and link outcomes above. Set a factual description/link after choosing the docs home if desired; badge only the maintained branch/workflow. Mark nonlocal commercial references as historical inspiration, never this fork's setup/API docs; timeouts require a later check, not a broken-link claim |
| D19 / D | Native HazelProject usage advertises only `create`; `measure-scene` has no maintained documented caller | `tools/HazelProject/main.cpp`, current storage/audio sheet-command reference. Document maintained native commands and distinguish diagnostic metadata timing from graphical load timing; retain or retire the unused probe deliberately as C5 describes |

### Proposed README and guide navigation

Keep the landing page concise, with these sections in order:

1. **What this fork is**: upstream attribution; Hazel engine, Hazelnut editor, Nutella player; current 2D and bounded screen-space UI scope. No unsupported quality adjectives.
2. **Run a download**: actual successful snapshot, OS/GL4.1/x86_64/deployment baseline, checksum/extraction/launch, login/retention; prepared games need no SDK/Python/compiler. Link full package guide.
3. **Build from source**: recursive clone, platform prerequisites guide, setup, explicit run project. Explain script compilation separately. Show one canonical command path, not redundant wrappers as separate workflows.
4. **Learn and author**: direct Sandbox UI Tutorial, current editor/sprites/UI/scripting guides; five example projects with purpose and controls links. Native project creation is possible without SDK; optional compilation/export needs a prepared source SDK/toolchain.
5. **Export**: saved-content boundary and correct Release ladder or editor Export; link package/SDK guide.
6. **Support status and reference**: current known limitations/human acceptance/CI links, docs index, contribution/bug reporting, license/provenance. macOS not yet implemented; forced Mesa GL4.1 is not Mac validation.

Proposed canonical homes (new files are later implementation work, not files created by this audit):

| Subject | Canonical home / links |
| --- | --- |
| Navigation | `docs/README.md`, split A everyday / B maintaining / C history; link every maintained guide and provenance reference |
| Clone/prerequisites/setup/toolchain/VS Code | `docs/getting-started.md`; Windows/Linux sections and precise output paths, prepared source SDK, Mono/pip/Bear optionality |
| Editor workflows | `docs/editor-guide.md`; current native creation, content/save/conflicts, bounded Undo, Play/Stop, project/defaults/preferences/workspaces; extract from historical authoring/A–H |
| Scripting | `docs/scripting.md`; source project/build/configuration, callbacks/lifetimes/Entity ID validity, owner events, storage/game API links |
| Sprites | `docs/sprite-authoring.md`; current slice/clip/assign/preview/save/export; link historical format design |
| Runtime UI, styles/fonts/text/history limits | Keep `docs/runtime-game-ui.md` as everyday/reference home with a short index; direct Sandbox page for hands-on tutorial |
| Saves/audio/player settings | Keep `docs/runtime-storage-audio.md`; UI bindings/style reference links back rather than duplicate path/ownership rules |
| Packages/downloads/export/baselines | `docs/packaging.md`; plain package versus editor-export, exact contents, runtime versus SDK, launch from unrelated cwd, checksums/notices; extract current Nutella contracts |
| Platforms/dependency pins/known limits | `docs/platforms-and-dependencies.md`; one current matrix, linked manifests/legal ledger, explicit Linux X11/Xwayland, Windows unverified hardware and future Mac status |
| Testing/CI/fast iteration/troubleshooting | `docs/development.md`; actual jobs/commands/environment selectors, source-hidden profiles, focused executables, latest SHA evidence and limits of automation |
| Contributing/support | Short `CONTRIBUTING.md` pointing to development guide and issue evidence; templates optional and small |
| Architecture | Current ownership/invariants in a concise B reference (can be a section in `docs/development.md` initially), linked to authoring-reliability and this audit; avoid another status diary |
| History | An indexed C section under docs, with explicit baseline/superseded banners on existing design/migration/Authoring records; moving files is optional, all inbound links must be updated if moved |

No giant historical changelog, exhaustive limits table or prior conversation narrative belongs in root README. Historical design proposals are not a current feature roadmap. Documentation updates are deliverables of every implementation slice, with command/menu/link checks alongside code review.

## 7. macOS readiness: concrete later-port blockers

This audit does not implement macOS or verify Apple hardware. GL4.1 portability work already present is useful, but Linux forced-version tests are not a Mac port.

| Blocker / evidence | Cleanup justified now versus later implementation |
| --- | --- |
| `Core/PlatformDetection.h` explicitly accepts only Win64/Linux; debugbreak has the same restriction | Preserve explicit unsupported-platform diagnostics now. Add macOS detection/debugbreak only with an actual port |
| `premake5.lua` globs Platform C++ and removes only the opposite Linux/Windows tree; dependency rules and CLI often treat every non-Windows host as Linux | Document supported targets and existing source-selection obligations. Later add explicit macOS filters/source/link selection and unsupported-host rejection; do not compile both existing OS factories. Keep separate adapters and Premake |
| GLFW window/context/input and native adapters | Later Cocoa window lifecycle, Retina framebuffer/logical sizes, focus/gamepad/clipboard/text and placement; `OpenGLContext::ConfigureWindowHints` currently lacks a Mac forward-compatible hint. `RuntimeSession::SubmitUIEvent` editing uses Ctrl and rejects Super, so Command shortcuts need deliberate OS input policy. No global Windows shortcut rewrite now |
| Filesystem/user data/leases/watchers/time/process/tool discovery | Implement corresponding native macOS paths, executable discovery, dialogs/open-folder, safe saves/file leases, watcher shutdown and child-process behavior. Current Linux `/proc`, X11/GTK, Windows shell/registry/UTF-16 implementations are not portable substitutes |
| Mono/native runtime closure | `mono.lua`, `mono_prefix`, staging, Windows static SDK/Linux .so roots, assembly/config profiles, PackageAudit and SDK readiness need a chosen actual macOS Mono/toolchain/architecture. Existing CLI is x86_64-only; do not claim arm64 or silently use Linux bootstrap there |
| Audio and typography/shader deps | Miniaudio contains platform backends but Hazel's link/build/package integration still needs CoreAudio validation. HarfBuzz/UniBreak/SheenBidi static sources and existing private symbol visibility are promising, not a successful Mac build. FreeType/msdf/Mono/shaderc/glslang/Tools/Cross need actual compiler/source/CRT/link/package checks with unchanged pins first |
| GL4.1 functions/capabilities | Keep common renderer specifications and separate OpenGL backend. Current bind-based resources and generated GLSL410 path avoid a wholesale renderer rewrite. Verify loader/build linkage, Apple driver shaders/limits, line-width behavior, FBO/integer/MSAA/readback and query-counter availability on the actual device. Keep optional GPU timers/capabilities nonblocking and capability-gated |
| Installation/writable roots and packaging | Existing Resources/UserData separation is valuable. Later .app resource/executable/Mono/dylib roots, `install_name`/rpath closure, writable Application Support/cache locations, selected deployment baseline and signing/notarization policy require actual native package decisions. Preserve explicit SDK discovery; do not add cwd scans |
| Test platform | Current desktop helpers are X11/Win32 and software Mesa profiles are not Apple acceptance. First get CPU/service/native builds, then small real app/relocation checks on Mac. Do not introduce a desktop-control server or speculative Metal adapter framework |

Pre-port cleanup should therefore fix current cross-compiler declarations, remove redundant authority, preserve resource/settings ownership, and publish truthful platform/toolchain documentation. Mac adapters, build filters, ABI/deployment choices and hardware checks belong to the next milestone. World-space UI integration belongs with actual 3D prerequisites later; no fake projected UI substitute or Metal/backend framework is needed now.

## 8. Bounded implementation order and exit criteria

Each slice includes corresponding documentation corrections and meaningful focused verification. Preserve the baseline manifest/draft/stash/layout/pins throughout. Reinspect HEAD/status before beginning; do not overwrite newer work. Branch integration is a later explicit step.

| Order | Scope / prerequisites | Completion evidence |
| --- | --- | --- |
| 1 — A correctness and compilation | A1 PCH ordering; A2 declared GLM include; A3 complete managed-string validation; D1–D5 current setup/package/CI correction. No dependency updates or workflow weakening | Focused managed rejection/unchanged-save/path tests; generated MSVC and clean Ubuntu Debug/Release builds; current CI step outcomes and new SHA recorded. Newly exposed downstream failures are investigated on logs |
| 2 — B ownership simplification | After compiling baseline: B1 raw-font removal with tests migrated; B2 one native entity-reference visitor and reduced Python schema duplication. C4 resolved-prefix preflight. Resolve B4 only if its focused probes establish a supported failing contract | Failed replacement/cache pin retirement, all reference remaps/restored lifetimes, prefab closure, custom SDK readiness and existing interleaved history assertions. Update font/asset/history/SDK docs in this slice |
| 3 — C workflow/test restoration | Consolidate C2 CI coverage/triggers without losing Release GL4.1 or required checks; C3 select private-copy tutorial authoring. Keep canonical CLI/OS wrappers/templates and package isolation | Existing whole jobs complete at one implementation SHA; artifact metadata refers to successful whole runs; tutorial hook PASS visible. No polling campaigns/retries/test suppression |
| 4 — C documentation/repository consolidation | Complete D6–D18, docs index/current guides/history classification, stale authoring generator warnings and narrow output-ignore guidance. Can be developed alongside slices 1–3; not an optional final afterthought | Recursively check local paths/anchors; command help/source/menu comparison; external availability reported accurately; package README/LAUNCH/license provenance matches products. No historical instruction presented as current |
| 5 — B/D small measured polish | B3 only demonstrably redundant scans/queries; D1 terminology/navigation and small example wrapper reuse where useful. Do not turn this into general optimization/API redesign | Existing source/layout/render profiles only for touched hot paths; same output/order/input/dirty/history behavior, no broad performance claims |
| 6 — final qualification and acceptance | Completed repairs/docs/CI; run milestone suite once, repair evidence-based failures; physical acceptance with owner before later integration/macOS | SHA/config/OS/renderer/results table, source/SDK-free relocated packages and notices, preserved unrelated state, explicit outstanding human results. Integrate only as a separate authorized action |

**Fix before macOS:** A1–A3; canonical setup/build/export guidance; actual CI coverage and current qualification; current/historical documentation separation and tutorial discovery; resolved-prefix preflight. B1/B2 are high-value small consolidation candidates for this cleanup and should accompany their tests/docs. Do not hold the port for speculative B4 rewrites if focused checks do not expose a supported failure. Performance work is limited to accidental redundant work demonstrated while touching a path.

**Can safely wait:** general Scene decomposition, generic reflection/event/widget systems, universal property Undo, large-list virtualization, native IME preedit/candidate protocol, broader language coverage, accessibility bridges, touch, linked prefab variants, animation features, world-space UI, Mac implementation/hardware verification, additional graphics APIs and all 3D/PBR/IBL development. Optional public metadata polish and durable release publication need not block code cleanup; do not claim a release before producing it.

**Human acceptance still needed:** physical Windows hardware and normal desktop use; actual controller mapping/disconnect/remap/neutral behavior; monitor/DPI/framebuffer/detached viewport behavior; real keyboard layout/dead-key/clipboard and committed-text behavior; competent-reader review of the declared Arabic/Hebrew and other typography; any wider end-to-end authoring/pacing acceptance not covered by the owner's successful tutorial. Unsupported IME preedit is a limitation, not an acceptance task that can be passed by committed-character callbacks.

The audit's only maintained output is this document. Its publication does not mean the recommendations have been implemented or that CI is green.

## First cleanup slice implementation status

Implementation began on 2026-10-28 from `0a631e67fc0221f7356274e6fa60b338c8792480` on `feature/runtime-ui-hardening`. The original audited baseline, findings and historical evidence above remain unchanged. This appendix records only A1–A3 and D1–D5; it does not authorize or claim subsequent slices.

- **A1:** `hzpch.h` now precedes all other includes in the three identified TUs. All 119 engine C++ files using it pass the first-include scan. Each affected header also compiles on its own without PCH. PCH, platform selection, UTF-8 and dynamic CRT policies remain enabled.
- **A2:** TypographySmoke explicitly declares the pinned GLM include root alongside its direct HarfBuzz, libunibreak, SheenBidi and spdlog requirements. Generated GNU Make and isolated VS2022 projects include it. The actual generated GNU include set also compiled the complete typography source with header tracing selecting `Hazel/vendor/glm`, never `/usr/include/glm`. VS generation additionally checked PCH Use, UTF-8, dynamic Debug/Release CRT and exclusion of Linux OS sources; generation is not Windows compilation.
- **A3:** a scripting-local `ManagedStrings::Read` reads the explicit UTF-16 length, checks surrogate pairs/NUL policy and counts UTF-8 bytes before allocating output. It emits exact Unicode directly, so there is no Mono conversion buffer to leak or failed conversion to dereference. All direct `mono_string_to_utf8` calls in first-party scripting were replaced, including live asset snapshots and exception messages. Exceptions are translated after native temporary values have left scope; Scene.LoadScene retains its existing log-and-reject native behavior and C# null/empty argument guard.

| Input | Enforced boundary / retained semantics |
| --- | --- |
| Save slot | 96 UTF-16 units / 96 UTF-8 bytes, then native portable ASCII name validation; NUL/surrogates rejected |
| Save payload | 65536 UTF-16 units and 65536 UTF-8 bytes, nonempty, no NUL; native storage also validates UTF-8 before persistence/after disk decoding |
| Scene/prefab/sprite paths and live asset snapshots | 32768 UTF-16 units / 131072 UTF-8 bytes, no NUL; native path/schema/ownership validation remains authoritative |
| Display text/emphasis | Existing 32768 UTF-16 bound plus 65536 UTF-8 bytes; native scalar/span/layout limits unchanged |
| Text entry | Existing 16384-unit boundary plus 16384 bytes; native configured byte/grapheme/line limits unchanged |
| Family / style selection | Existing UTF-16 bound 1024; native 1024-byte family/asset and 128-byte style-variant limits checked before publication |
| Entity names / log and exception messages | 32768 UTF-16 units / 131072 bytes; names reject NUL, diagnostics preserve it by explicit length. Invalid exception messages produce a bounded diagnostic instead of recursive managed formatting |

Null retains the empty-string convention at the native reader; operation-specific empty-input rules still apply. Data/path malformed input rejects before filesystem access, mutation or request admission. This does not turn unrelated APIs into one string policy or expand supported typography.

The existing managed fixture now exercises real internal calls with disposable persistent storage: valid Unicode/supplementary payloads, exact UTF-8 boundary, malformed/oversized replacements and slots, unchanged accepted/persisted values, no prefix-slot aliasing, malformed scene/prefab paths against actual valid prefix targets, Unicode path success, live-domain preservation on rejected asset snapshots, and retained plain/rich text behavior. Sprite tests also check rejection preserves paused animation. Diagnostic NUL preservation is checked independently. No test writes to the owner's profile or saves.

**Implementation checkpoint:** `7bc5f2307efc30a6ad26d1cdedd82e520a0f8f65` contains A1–A3 and their regressions. All completed-source local checks below use that native/managed source. Local platform: CachyOS x86_64, GCC 16.2.1, pinned isolated Mono 6.12, physical Intel HD4000 / Mesa 26.2.4 through Xwayland. No local Ubuntu or Windows compilation is claimed.

| Verification | Result / ignored evidence |
| --- | --- |
| Debug and Release builds, two compiler jobs | PASS; canonical Premake build path (Debug `hazel.build('Debug', tests=True)` under build lock; Release `python3 scripts/hazel.py build --config Release --tests`). `build/testing/cleanup-slice/final-{debug,release}-build.log` |
| Debug `test --config Debug --profile native` | PASS, all 17 sequential executables; physical HD4000 GL4.2. `final-debug-tests.log` and `build/testing/logs/linux-Debug-native-*.log` |
| Release `test --config Release --profile gl41` | PASS, all 17 sequential executables; physical HD4000 forced GL4.1. `final-release-tests.log` and `build/testing/logs/linux-Release-gl41-*.log` |
| GNU/VS project declarations, pinned GLM and standalone headers | PASS; `generation-checks.log`, `pinned-glm-syntax.log`, `header-self-containment.log`, under `build/testing/cleanup-slice/`. Generation is not Windows compilation |
| Disposable Release packaging / extracted CLI | PASS: private SceneTransitions Release script-build → package both applications; native/managed/font closure, archive and all-file checksums, notices, Nutella help/discovery/error contracts from unrelated cwd. `final-package-cli.log`; graphical relocation remains CI work |

Focused Debug RuntimeSessionSmoke, SceneSmoke and TypographySmoke also passed during repair; the table records the full final-source gates rather than counting those iterations as additional qualification. CI results will be recorded separately. Full source-game/playthrough and graphical package relocation gates are left to the existing CI cycle; no unrelated local game campaign was repeated and Xvfb is unavailable locally. Neither generated VS configuration nor Linux automation establishes physical Windows/controller/DPI or competent-reader acceptance.

**CI qualification:** the tested repair and D1–D5 documentation were pushed together at `a124295e5de12a5011f332a10242470fee70d172`, starting [C/C++ run 40701454325](https://github.com/cringlekaden/Hazel/actions/runs/40701454325). At the single inspection on 2026-10-28 13:59 UTC, both jobs were **in progress**: [Ubuntu](https://github.com/cringlekaden/Hazel/actions/runs/40701454325/job/122862075725) was installing prerequisites; [Windows](https://github.com/cringlekaden/Hazel/actions/runs/40701454325/job/122862076454) was checking out the repository. Compilation, Release/runtime/package gates and whole-job outcomes were not yet available. No new completed failure could be diagnosed and CI restoration is not established. Stop here rather than poll/wait; inspect these completed results at the next continuation. This evidence-only follow-up skips CI to avoid a duplicate qualification run; engine, tests and public instructions are identical to the qualified checkpoint.

Preservation was recorded before edits and rechecked after Debug/Release qualification: 305 authored/user-state manifest hashes match, including Sandbox `Start.hazel` (`da6c537dfb525efd0e70fa993a4081323af4c6c336030c658e59d388ae285219`), saves, preferences/session/layouts and ignored VS Code files. Stash `2948648a2332d8bb809fe893cd8aa554265bd6b4`, all 14 recursive submodule pins and their clean working trees are unchanged; there are no new/missing user-profile or save files. The remaining manifest entry is the ignored autogenerated `examples/SceneTransitions/Assets/Scripts/SceneTransitions.make`, normally regenerated for Release by the canonical build. No authored file was changed by qualification. No dependency installation/update, workflow/settings change, example-content edit or merge was performed.

**D1–D5:** README now links directly to the UI Tutorial/UI/storage references and a new current `docs/getting-started.md`. The guide distinguishes prerequisites from checkout-owned acquisition, setup/build/run/script-build/package/editor-export, actual output paths, correct Release export commands, snapshot downloads/login/retention/checksums and successful whole-run selection. Historical editor/runtime milestone records point to the current guide; their old results are not promoted to this repair SHA. The six retained README artifact IDs were rechecked as present/unexpired (2027-01-06 expiry); no archive download was claimed. Storage documentation records the enforced Unicode contract.

Still pending: B1/B2 ownership consolidation, C4 custom-prefix editor preflight, workflow/coverage consolidation, D6–D19 and the wider documentation navigation/history work, plus physical Windows/controller/DPI and competent-reader acceptance. Those are not silently included in this first slice. The next bounded implementation slice remains audit order 2.

## Second cleanup slice implementation status

Started from `6990721be38b3922d450941f9b99aba1c57662d8` on `feature/runtime-ui-hardening`, 2026-10-28. Initial status was only the unrelated Sandbox Start draft. A new pre-edit manifest records 296 authored/user-state files, excluding generated project makefiles/compiler products; the exact draft SHA, stash and all 14 clean recursive dependency pins match the preserved baseline. Tests use disposable content, profiles and saves. Historical findings and the first-slice checkpoint above remain unchanged.

**Existing CI inspected once:** run [40701454325](https://github.com/cringlekaden/Hazel/actions/runs/40701454325) was still in progress overall; Linux remained running. The completed [Windows job](https://github.com/cringlekaden/Hazel/actions/runs/40701454325/job/122862076454) failed during Debug compilation. Its logs identify `UISliderChecks.h:23–27` and `UIScrollSessionChecks.h:30–31` (`near`/`far` locals collide with Windows macros; C2513/C2337 and cascades) and `EditorFontFamilyWorkflowChecks.h:92` (filesystem path passed to `Serialize(const std::string&)`; C2664). This slice renames those local variables and converts the path with `generic_u8string()`. No checks are disabled. That job did not reach Release/runtime/package qualification; Linux's later outcome was not polled.

- **B1:** removed `FontAsset`, `FontStatus`/`FontResolution`, `ReloadFont`, `FontEntry`, the raw-face map and its refresh/shutdown work. Compositing uses low-level `Font::Load`; measured/project resource tests use actual `.hfont` families. Face-dependency reload still retries family admission, with native owned-path validation and asset epochs. Family maps, component/prepared/submitted pins, immutable accepted bytes/pages, fallback, pressure accounting and renderer-thread teardown remain intact.
- **B2:** `Scene/AuthoredEntityReferences.{h,cpp}` enumerates exactly the 24 current authored UI reference fields and stored script Entity fields. Clone remapping and prefab ownership use it; graph relationships, explicit serialization/snapshots and structural history remain separate. Duplicate retains allowed external targets; detached prefab/extraction rejects nonzero escapes atomically; zero/required/role semantics remain with the operation/control validators. History retains repairable incoming references and restores UUIDs with fresh lifetimes. Python no longer duplicates scene/prefab schema, IDs, UI-reference ownership or native-audited asset dependency checks; it still owns inclusion/exclusion, deployment/filesystem and managed assembly/class closure. Native dependency output now includes script Prefab assets, including missing-file rejection, before Python checks exact package inclusion. The current Python baseline already included UITab.InitialFocus despite the original audit's omission note; its coverage is explicitly retained in the shared visitor and regressions.
- **C4:** `native_contract` resolves Mono once through `hazel.py:mono_prefix`, uses that prefix for native/managed probes and returns it to preflight. Compiler/profile readiness uses the selected prefix, never unrelated default locations. Invalid explicit selection remains an actionable error. Windows retains MSBuild/.NET 4.7.2 targeting checks; runtime-only Play is distinct from compilation readiness.

Focused checks so far pass: real custom Mono SDK with spaces/Unicode and default candidates unavailable; incomplete selected compiler/profile rejection; SceneFoundationSmoke's independent per-field duplicate/ownership cases and native auditor closure/escape tests; FontSmoke pressure/reload/retry/root-release/context lifecycle; GameUIIntegrationSmoke family owner replacement, failed replacement and repaired reload, measurement/preparation reuse and composed controls. The low-level compositing migration's initial ambiguous `Font` identifier was qualified as `Hazel::Font`; the repaired target compiles. Both full Debug and Release builds passed through the canonical Premake CLI with two compiler jobs. Custom-prefix readiness also compiled a disposable project through the real script-build path. All 17 maintained regressions passed in Debug/native (physical HD4000 GL4.2) and Release/gl41 (physical HD4000 forced GL4.1) at source checkpoint `09b593398a55d1f31eb5e9c1f20f7d974a8231de`. This includes native editor/history/prefab, resource pressure/reload/shutdown and both selected-SDK readiness/project-compilation checks. Representative package qualification also passed as detailed below.

Current documentation updates accompany the implementation: [font ownership/resources](../runtime-game-ui.md#font-families-and-declared-coverage), [reference/history/package contracts](../authoring-reliability.md#authored-entity-references-and-detached-prefabs), and [resolved SDK readiness/troubleshooting](../getting-started.md#mono-selection-and-missing-prerequisites). No Windows compilation success, physical acceptance or CI restoration is inferred from local Linux results.

| Completed local qualification at `09b5933` | Evidence (ignored `build/testing/cleanup-slice2/`, plus per-executable `build/testing/logs/`) |
| --- | --- |
| Debug/Release builds through Premake, two compiler jobs | PASS; `final-debug-build.log`, `final-release-build.log` |
| Full Debug/native and Release/gl41 suites | PASS, 17 executables each; `final-{debug,release}-tests.log`, `final-suites.log`. Physical Intel HD4000 / Mesa 26.2.4 through Xwayland; GL4.2 versus forced GL4.1 respectively |
| Native references, interleaved history and fonts | PASS in both full suites: 24 UI fields plus script Entity/number distinction, internal/external/zero policies, atomic prefab/extraction rejection, native auditor escapes/closure, actual duplicate/instantiation, tab initial focus, incoming script reference retention with fresh restore lifetime, exact native-write guards; family pressure/eviction, accepted pages after failed replacement, missing-first-load fallback/retry, owner/root invalidation and shutdown |
| Resolved SDK readiness and real script compilation | PASS; `sdk-focused-complete.log` and both full suites. Controlled custom-prefix links with spaces/Unicode, default candidate file probes unavailable, incomplete compiler/profile rejected while runtime remains usable. Host SDKs were not removed/renamed. Windows targeting checks remain covered by the Windows branch of the maintained runner; not executed locally |
| Windows-macro and header syntax | PASS; `macro-syntax.log`: complete GameUIModelSmoke/RuntimeSessionSmoke TUs with empty near/far macros under GCC. New visitor header also compiles standalone without PCH. This is not MSVC compilation |
| Representative Release package and failure preservation | PASS; `final-packages.log`: tracked disposable Sandbox copy, Tutorial selected only in its private descriptor; real Release scripts, native/managed/font-family closure, checksums/notices. Malformed Tab.InitialFocus and script Entity prefab escapes, excluded prefab dependency and missing prefab dependency all reject before changing accepted archives/checksums |
| Extracted applications with source/resources/SDK unavailable | PASS; existing `test-packages --profile gl41` runner in a private filesystem namespace. Nutella and Hazelnut startup/render/resize/graceful close, unrelated cwd, spaces/Unicode, checksums/inventory and no source-library use. `relocated-packages/*-gl41.log` confirms llvmpipe GL4.1; this is software evidence. Checkout/SDK roots were masked/copied, never renamed on the host |

Final preservation: all 296 recorded authored/user-state hashes match; there are no new/missing real profile/save files. Sandbox draft SHA `da6c537dfb525efd0e70fa993a4081323af4c6c336030c658e59d388ae285219`, stash `2948648a2332d8bb809fe893cd8aa554265bd6b4`, all 14 clean recursive vendor trees/pins and dependency manifests are unchanged. The original audit and first-slice appendix remain byte-for-byte intact. Generated build outputs changed normally; no user examples/settings were rewritten. Affected guide/audit local links and anchors pass (36 checked). No workflow/coverage policy was changed, no dependency installed/updated, and no branch merged or release published.

Implementation checkpoints: `7e522f2` bounded Windows fixture compilation; `07241ee` B1/B2 ownership/reference/package consolidation and tests/docs; `09b5933` C4 SDK readiness/compilation and setup guidance. The new CI run and its actual inspected state will be recorded separately after the deliberate qualification push.

Remaining planned work is C2/C3 workflow/coverage restoration and the wider public documentation/repository consolidation, then final qualification/acceptance. No workflow/trigger change, new UI feature, dependency pin update, macOS implementation or integration is included here.

**Slice-two CI qualification:** pushed `2f4228bcc7a428c35b45f6aff552eb374433f45c` once after local qualification, starting [run 40748759451](https://github.com/cringlekaden/Hazel/actions/runs/40748759451). At the single inspection on 2026-10-28 18:50 UTC, both [Windows](https://github.com/cringlekaden/Hazel/actions/runs/40748759451/job/123029472970) and [Ubuntu](https://github.com/cringlekaden/Hazel/actions/runs/40748759451/job/123029473422) were **in progress**, in checkout. No completed new compilation/test/package outcome or failure log was available. CI restoration remains unestablished; inspect these completed results at the next continuation, without a polling campaign. This evidence-only follow-up skips CI; engine, tests, tooling and public instructions are identical to the submitted checkpoint. No additional local blocker was exposed. Physical Windows/controller/DPI and competent-reader typography acceptance remain outstanding, as do the planned workflow/coverage and broader documentation/repository slices.

## Third cleanup milestone — CI, public guides and repository presentation

Implementation starts from **`207b5445936b44a9c26f25ddf9b63c0f2530e14f`**, `feature/runtime-ui-hardening`, on 2026-10-28. The original `650d09e` audit and earlier implementation records above remain historical evidence. No merge, release, dependency change or UI feature expansion belongs to this milestone.

### CI and tooling

- **C2:** `c-cpp.yml` is canonical; `editor-usability.yml` is retired. Preserve both main job IDs/names. Policy: master pushes, all PRs, manual dispatch for topic qualification. This removes feature-name allowlists and duplicate PR builds. OS/configuration dependency caches remain separate; each native build retains two compiler jobs, Windows UTF-8/dynamic CRT and GL4.1/platform separation.
- Coverage retained: both OS Debug/software and Release/software general suites; Release/GL4.1 general coverage moved from the editor workflow; matching five-project scripts, managed/native/source-game checks, editor/history/prefab checks, software/GL4.1 extracted applications and five game packages with native/managed/resource/notice closure. [Before/after coverage map](../development.md#coverage-retained-and-added) is canonical. All five games now have downloadable artifacts; product uploads follow all platform qualification gates, while failure evidence remains unconditional. Whole-run success is still required for download selection.
- **C3:** Release/GL4.1 sets `HAZEL_FINAL_UI_PROJECT` after scripts are built. The existing native private-copy check is unchanged; Python additionally requires its exact tutorial PASS marker when selected. A skipped hook cannot qualify. No bot, desktop server, capture dependency or new authoring route is added.
- Requested baseline run [40748759451](https://github.com/cringlekaden/Hazel/actions/runs/40748759451), SHA `2f4228b`, was inspected once: overall/Linux still running; completed Windows job `123029472970` failed Debug compilation with `EditorSmoke.cpp: C1128`, COFF section-count limit. `/bigobj` is scoped to **MigrationEditorSmoke on Windows/VS**. No checks/warnings were disabled. Its later Release/runtime/package gates were skipped and are not credited.
- Read-only protection inspection: master and this feature branch returned “Branch not protected”; repository rulesets returned `[]`. No settings changed. Main checks retain their identities. Administrators with other/external policies requiring the retired **Linux/Windows Debug / Release, bounded editor checks** must replace those with the corresponding retained main job, as listed in [development](../development.md#canonical-ci).
- **C5/C6/D19:** Historical composition scripts require explicit `--historical-rebuild --write` and refuse writes in a maintained checkout/worktree; reproduction remains disposable-only. `compare-upstream.py` requires a new scratch `--output`, preserving recorded evidence. Native HazelProject `--help` lists create/validate/sheet commands and labels measure-scene as a CPU diagnostic. Only the root optional clangd cache gets a new ignore rule; nothing is cleaned or deleted from vendor/fixtures/evidence.

### Current documentation homes and disposition

| Findings | Current home / correction |
| --- | --- |
| D6–D8 | [Lightkeeper](../../examples/LastLightkeeper/README.md): completed native UI, correct Tiny Battle/CC0 provenance; new [SceneTransitions guide](../../examples/SceneTransitions/README.md); [Sandbox](../../examples/Sandbox/README.md): complete launch, tutorial discovery, copied startup/export selection and prebuilt-versus-SDK distinction |
| D9–D10, D13 | [Editor guide](../editor-guide.md) supplies current hierarchy, detached subtree, native project creation, script/build/export and renderer-setting workflows. Dated banners preserve historical “not implemented”, branch/count/download evidence rather than presenting it as current. [History index](../history.md) accounts for every retained design/migration/Authoring record |
| D11 | [Runtime UI](../runtime-game-ui.md) has a subject index, visual examples and consistent property/structural history scope; standalone script-field/unrelated asset history remains bounded |
| D12 | [Typography fixtures](../../tests/fixtures/typography/README.md#run-the-cpu-check) specify private cwd `assets/fonts/opensans`, corpus/pinned Unicode preparation and a complete CPU invocation |
| D14 | [Settings/resources](../settings-and-resources.md) owns defaults, profiles/session/workspaces/layout, save roots, shader caches and accepted family/style lifetimes; existing UI/storage/reliability references link to it |
| D15 | [MeadowRun](../../examples/MeadowRun/README.md) describes current-stage restart; [Skybound](../../examples/Skybound/README.md) describes native screens/HUD instead of offscreen/world-hit buttons. Both have complete configuration-specific source/export commands |
| D16 | Concise root [README](../../README.md), [docs index](../README.md), task-oriented editor/scripting/sprite/packaging guides, [development/troubleshooting](../development.md) and [Contributing](../../CONTRIBUTING.md); examples linked by purpose. Historical evidence is a separate navigation route |
| D17 | [Platforms/dependencies/licensing](../platforms-and-dependencies.md) links actual pins and engine/Open Sans/Mono/typography/Unicode/DejaVu/Kenney/project notices and exported closure. Descriptive typography ledger now distinguishes test fixtures from Sandbox's explicit imported face; original legal texts are unchanged |
| D18 | One workflow-wide branch-labelled badge, platform result navigation and snapshot guidance. All six valid old `a31187d` artifact links retained in [Packaging](../packaging.md#retained-older-downloads), explicitly before later UI/Lightkeeper. Authentication, retention, SHA, checksums and whole-run selection are explained |

Current task guides are class A; settings/resource/contracts, dependency ledger, fixture and development references are class B; dated records in the history index are class C. Preservation/provenance records retain their still-applicable invariants/import identities. No wholesale file moves or history rewriting are needed. The old “broader consolidation later” text in prior appendices records those earlier checkpoints, rather than remaining navigation work after this milestone.

### Verification, visuals and preservation

Tooling checkpoints: **`4e7e596`** (canonical CI, tutorial evidence, scoped Windows big objects) and **`aef053f`** (historical safeguards, native help, clangd ignore). Local verification at that completed tooling state:

- Premake **Debug and Release** builds of affected HazelProject/EditorSmoke targets passed, two compiler jobs. No engine/UI product source changed. Generated MSVC projects declare `/bigobj` only for EditorSmoke, retain `/utf-8` and dynamic Debug/Release CRT; GNU Make generation was restored. Generation is not Windows compilation.
- General suites passed **17/17 Debug/native and 17/17 Release/forced GL4.1**, sequentially, on accelerated HD4000/Xwayland. Both emitted the exact native tutorial PASS marker, then the Python suite PASS. Existing resource/font-pressure, managed strings, SDK discovery, history/prefab/lifecycle checks remain included. Logs: `build/testing/logs/linux-{Debug-native,Release-gl41}-*.log`; no repeated full campaign for later prose/images.
- Native `HazelProject --help`, public CLI help/preflight, Python compilation, workflow parsing/command ordering/profile checks and isolated historical-guard checks passed. The historical generators themselves were never run against maintained examples. The documented CPU typography recipe executed successfully in its private cwd, including conformance/editing/emphasis checks.
- One representative **Sandbox tutorial Release archive** passed native family/style/image/reference and notice closure, archive/file checksums, then existing owned-window startup/render/resize/graceful-close checks in forced GL4.1. The entire checkout/isolated SDK was hidden in a private filesystem namespace; actual child library maps contained no checkout libraries. The first scratch adapter used bwrap's parent PID instead of its application child and could not locate the window; correcting only that disposable adapter and reusing the same archive passed. No maintained test assertion/retry was changed. Local archive metadata correctly records a dirty working tree; it is not a clean published release.
- All **47 first-party Markdown documents** are reachable from README/index; recursive local path/anchor checking reports no errors. Legal/data references are linked through the dependency/provenance guide. GitHub rendering/published-path and final CI observations are recorded below after publication.
- External HEAD checks: **113 destinations: 96 HTTP 200, 14 artifact URLs HTTP 404 anonymously, three timeouts**. Authenticated metadata confirms all fourteen historical artifacts exist/unexpired; no downloads claimed. Kenney Tiny Battle resolves. The three historical commercial Hazel destinations remain unavailable to this check, not classified as broken. Valid old links remain intact.
- **12 selected current screenshots**, losslessly optimized, cover Hazelnut menu/hierarchy/Rect authoring, native Style/Sprite Sheet/export panels, Lightkeeper gameplay/title/settings, MeadowRun/Skybound, and tutorial layout/controls/typography. [Capture notes](../images/README.md) distinguish fresh private-copy editor captures from reused current renderer captures and retained historical images. Original authored assets/scripts match the UI endpoint; images contain no personal desktop/user paths. Export's machine-specific tool paths are outside the published crop. No fabricated Windows/macOS capture or screenshot framework was added.

The pre-edit manifest records 296 authored/state files, exact draft/stash and recursive dependency identity. Authorized changes to fourteen pre-existing example README/historical-record/tool files are accounted separately; all other **282 protected files matched**, including the exact Sandbox Start draft `da6c537dfb525efd0e70fa993a4081323af4c6c336030c658e59d388ae285219`. Final comparison confirmed stash `2948648a2332d8bb809fe893cd8aa554265bd6b4`, preferences/sessions/layout seed/ignored VS Code, original legal text, all dependency pins and fourteen clean recursive vendor trees. Captures/tests use only disposable projects/profiles/saves; ignored user state is not cleaned.

**Remaining before final integration/macOS:** complete the new canonical CI cycle at its exact source checkpoint and repair any evidence-backed downstream failures; perform final qualification/acceptance. B4 remains suspected failure-atomicity follow-up, to be decided by small probes rather than speculative rewrites. B3 permits only demonstrated redundant work; this milestone makes no performance improvement claim. No newly reproduced engine correctness defect expands the agreed UI endpoint. Physical Windows GPU/controller/monitor/DPI and competent-reader typography acceptance remain outstanding; the owner's prior successful tutorial test is preserved as separate human evidence.


### Published checkpoint and CI observation

Public documentation/screenshots checkpoint **`556e0a0a9fb99f770f96fe90d91f0f21bfbd48f2`** includes tooling `4e7e596` / `aef053f` and was pushed normally. No open PR existed, so exactly one manual canonical qualification was dispatched: [run 40760243767](https://github.com/cringlekaden/Hazel/actions/runs/40760243767). At the single result inspection, both [Windows](https://github.com/cringlekaden/Hazel/actions/runs/40760243767/job/123069902863) and [Linux](https://github.com/cringlekaden/Hazel/actions/runs/40760243767/job/123069903378) were **in progress** (checkout and prerequisite installation respectively). Compilation, regressions and package gates have no completed result from this cycle yet. No polling/waiting, duplicate dispatch, merge or release follows this observation. CI restoration is **not certified**; final results must be reviewed before qualification/integration.

GitHub's Markdown rendering API accepted the landing page with the intended headings/tables/images and descriptive alt text. All twelve published PNG paths at `556e0a0` returned HTTP 200 with `image/png`; recursive local path/anchor and reachability checks remain clean. Screenshots total approximately 904 KiB. The final follow-up only adds platform result links and this evidence record; product/tool/workflow source remains identical to the dispatched checkpoint. Final preservation comparison passed: exact draft/stash, 282 unchanged protected files, fourteen accounted authorized example documentation/tool changes, no legal-text/pin/vendor changes. Owned capture/test windows are closed. No branch-protection/ruleset or repository metadata setting was modified.

## Final cleanup disposition

Final bounded pass, 2026-10-28 local, starting from `9e10bbd0c11b349b19ac1780bd37a385ebcad8de`. The original audited baseline remains `650d09e`. This section supersedes earlier next-slice statements; no further UI milestone is proposed. Implementation: [`8f8efa1`](https://github.com/cringlekaden/Hazel/commit/8f8efa1e8591e2bb0a8a2c5cb83e58eb23aa7a51). This audit-only follow-up does not alter its product/test/configuration sources.

### B3/B4 conclusions

- **B4 / flags — reproduced and repaired.** [UI_SetValue](../../Hazel/src/Hazel/Scripting/ScriptGlue.cpp) restores the boolean on failed retirement/refresh, following existing guarded setters. A negative-control build with the old setter fails the unchanged-authored-state assertion. [RuntimeFailureChecks](../../tests/migration/RuntimeFailureChecks.h) injects invalid host framebuffer metrics after establishing capture and queued input, invokes all six flags through Mono, and checks managed rejection, exact authored restoration, invalid published geometry, cancelled capture/events and explicit surface repair. Input cancellation is irreversible.
- **B4 / target startup — reproduced and repaired.** [RuntimeSession](../../Hazel/src/Hazel/Scene/RuntimeSession.cpp) unbinds and marks stopping before partially started target teardown, clears its pending transition/exit requests, restores accepted session gain to the previous scene and stops cleanly if that scene also cannot restart. A test-owned managed callback removes a private WAV after loading accepts it, with target scripts and physics already started. The pre-repair probe fails teardown rejection. The repaired probe checks same previous scene/session identity, fresh script instances/default fields and bodies, native destruction before physics retirement, stale target identity, cancelled buffered input, unchanged prior storage plus accepted target storage side effects, repaired retry and failed-fallback cleanup. Restart retains the previous scene's component values; it does not reverse arbitrary script/file/storage side effects. Invalid authored gains already reject during loading and need no new recovery policy.
- **B4 / sprites — existing contract verified.** [SpriteSmoke](../../tests/migration/SpriteSmoke.cpp) exercises accepted → invalid → repaired replacement in Edit and running Play. Pixel readback verifies accepted art/magenta fallback; checks cover epochs, cached diagnostics, live pinned texture/clip owners, unchanged authored state and explicit repaired reload. Disk repair alone does not retry. Strict validation still rejects broken assets before a new Play/export. No sprite implementation change or immutable-font-style retention policy was imposed.
- **B3 — one proven duplicate removed.** [Normalized](../../Hazel-ScriptCore/Source/Hazel/Scene/GameUI.cs) uses the existing combined `UI_ScrollInfo` call. [UIScrollSessionChecks](../../tests/migration/UIScrollSessionChecks.h) measures two source checks for separate Offset/Range reads versus one for Normalized, with the same values, zero ranges and direct native extent edits in three Play/Stop cycles. The combined read observes each raw native edit before any comparison query resolves it. No frame cache, dirty-flag authority or candidate-delivery guard changed. This is CPU boundary preparation evidence, not GPU or whole-frame timing.

The other inspected refreshes are distinct boundaries: `Scene::UpdateUIInput` refreshes again only after `CommitScreens` mutates state; rendering follows callbacks that can write components; `ConsumeEvent` skips an empty queue but revalidates each candidate after possible intervening script/native writes. Coalescing those checks would need assumptions the current mutable-native contract does not supply.

### Finding disposition

| Finding | Disposition | Commit / verification or bounded rationale |
| --- | --- | --- |
| A1 Windows PCH | Fixed | `7bc5f23`; PCH-first source, Linux builds and successful Windows Debug/Release compilation steps at `556e0a0`; final CI is separately recorded below |
| A2 declared typography GLM | Fixed | `7bc5f23`; explicit pinned include in GNU/VS configuration, maintained typography builds/checks; no installed-GLM claim |
| A3 complete managed strings | Fixed | `7bc5f23`; explicit-length checked UTF-16, operation-specific policies and state-preserving storage/path/text regressions |
| B1 raw-font project owner | Fixed | `07241ee`; family ownership, low-level `Font::Load`, reload/fallback/pressure/pins and renderer-lifetime regressions retained |
| B2 authored references / Python duplication | Fixed | `07241ee`; one narrow native enumeration, explicit operation policies/serialization; internal/external remap, tab initial focus, script Entity fields, prefab escapes, repairable incoming refs and guarded history/package tests |
| B3 normalized query | Fixed | `8f8efa1`; two → one measured source checks, native-write/zero-range checks above |
| B3 broader repeated linear comparisons/layout preparation | Deferred | Exact checks remain necessary while mutable native components are supported. No additional safely removable operation was demonstrated; large-interface CPU profiling is nonblocking future performance work, not a new invalidation framework |
| B4 UI flag failure | Fixed | `8f8efa1`; focused negative control and six managed flags above; no resurrection of input/events |
| B4 target-start failure | Fixed, existing restart contract verified | `8f8efa1`; controlled post-admission native failure, retired requests/handles, coherent recovery/retry and stopped failed-fallback path above; arbitrary script side effects are outside rollback |
| B4 sprite replacement | Existing behavior verified | `8f8efa1` regression; Edit/Play render/readback, epochs, cached failure, pins and explicit retry above; current sprite/resource references now state it |
| C1 command semantics | Fixed | `a124295`, `556e0a0`; setup/build/run/scripts/package/editor-export have distinct canonical instructions and actual output roots |
| C2 canonical CI | Fixed configuration | `4e7e596`; one workflow, retained platform job identities, deliberate master-push/all-PR/manual triggers, separate configurations and post-gate artifacts. Completed qualification is a separate gate below |
| C3 tutorial selection | Fixed | `4e7e596`; both platform Release GL4.1 suites select the private tutorial hook and require its exact PASS marker |
| C4 SDK readiness | Fixed | `09b5933`; one resolved prefix, strict explicit selection, custom space/Unicode prefix/compiler/profile tests and runtime-versus-SDK distinction |
| C5 historical tools | Fixed | `aef053f`; historical purpose and overwrite safeguards, no current generator recipes; native commands/help and diagnostic timing documented |
| C6 narrow hygiene | Fixed | `aef053f`; scoped generated-output ignores, no vendor/cache/configuration cleanup or useful-fixture removal |
| D1 authoring friction (section 3) | Fixed documentation | `556e0a0`, `8f8efa1`; current editor/scripting/UI navigation and explicit ownership/failure semantics; no generic framework introduced |
| Documentation D1–D5 | Fixed | `a124295`; platform/tool/compiler/profile prerequisites, complete Release ladder, correct paths, current-guide navigation and SHA-specific authenticated/retained/checksummed downloads |
| Documentation D6–D8 | Fixed | `556e0a0`; current Lightkeeper UI and Tiny Battle provenance, SceneTransitions guide, Sandbox tutorial discovery/run/export and five examples by purpose |
| Documentation D9–D10, D13 | Fixed | `556e0a0`; current hierarchy/prefab/New Project/renderer instructions and dated historical limits, counts, branches and artifacts separated from current guidance |
| Documentation D11–D12 | Fixed | `556e0a0`; exact current UI/history/font scope and self-contained typography fixture/cwd recipe |
| Documentation D14–D15 | Fixed | `556e0a0`, `8f8efa1`; canonical settings/resources/cache ownership and current MeadowRun/Skybound UI descriptions |
| Documentation D16–D17 | Fixed | `556e0a0`; README/index/task/reference/maintainer/history navigation, contribution/bug reporting and dependency/exported-notice map; original legal material preserved |
| Documentation D18 status/navigation | Fixed | `556e0a0`, `9e10bbd`; one workflow-wide branch badge, accurate platform result links and historical commercial references; no readiness guarantee |
| Documentation D18 optional About/topics/settings | Deferred | Repository metadata/protection changes were expressly excluded. Empty optional metadata does not block use, qualification or integration; inaccessible external destinations were not declared broken |
| Documentation D19 native help/diagnostics | Fixed | `aef053f`; maintained commands listed, `measure-scene` clearly retained as CPU metadata maintainer diagnostic |
| macOS blockers (section 7) | Deferred to authorized port | OS definitions/source selection, context/input/dialog/path/process/library services, dependency/SDK and packaging work remain the next platform milestone. Linux GL4.1 is not macOS evidence; no speculative backend added |
| Final canonical Linux/Windows qualification | Blocked pending completed results | [Run 40798826828](https://github.com/cringlekaden/Hazel/actions/runs/40798826828) at `8f8efa1`, both jobs in progress; early compilation and artifacts do not substitute for runtime/package gates |
| Physical Windows/controller/DPI and typography reader acceptance | Deferred to owner acceptance | Automated/software device coverage and owner tutorial success do not supply these acceptances |

### Final verification and CI

Local qualification passed for implementation `8f8efa1`; logs are under ignored `build/testing/final-cleanup/`:

- Full Debug and Release Premake builds (`build --config Debug/Release --tests`), two compiler jobs; affected runtime fixtures rebuilt in both configurations after test refinement.
- `test --config Debug --profile native` and `test --config Release --profile gl41`: all 17 maintained executables each, including font admission/reload/pressure, Unicode/rich text, native-write/input ownership, prefab/reference/history and B3/B4 checks. Both EditorSmoke logs contain **`PASS: native tutorial Inspector, composed creation/duplicate/delete/property undo, prefab instantiate, save/reopen and repeated Play/Stop in a private project`**; the runner also requires that exact marker.
- All five example script modules built in private project copies in both configurations; existing managed game models and native example contracts passed in both. No desktop game playthrough campaign or new capture infrastructure was used.
- Representative Tutorial Release `script-build` → plain `package` produced Hazelnut and Nutella archives. Archive/file checksums and family/style/Corpus/project/typography notices and managed/runtime closure passed. The existing two-app extracted-package runner passed CLI, startup, nonblank rendering, resize and shutdown using llvmpipe GL4.1. A private filesystem namespace hid the complete source checkout/resources/SDK and system Mono/compiler paths; no real source/SDK directory was renamed.
- Physical HD4000 Debug GL4.2 and forced Release GL4.1 through Xwayland are distinct from that software package evidence. No local Windows compilation or physical Windows/controller/DPI/reader acceptance is claimed. The earlier Windows build-step results below belong to `556e0a0`.
- All 47 first-party Markdown documents are reachable from README/index; all local path/anchor checks pass, including 13 referenced images. Current visual/navigation work from `556e0a0`/`9e10bbd` remains intact; no screenshots were regenerated. No new blanket external-link validation is claimed.

Canonical CI completion remains a separate gate; this local pass alone does not establish CI restoration.

The requested [prior canonical run](https://github.com/cringlekaden/Hazel/actions/runs/40760243767), SHA `556e0a0a9fb99f770f96fe90d91f0f21bfbd48f2`, was inspected once at the start: both jobs were in progress. Windows Debug/Release compilation and all five script modules in both configurations had succeeded, with distribution construction in progress; Linux Debug compilation had succeeded, with Release compilation in progress. No completed job failure was available to diagnose. These are step results, not completed runtime/package qualification. No workflow, check identity, repository setting or dependency was changed in this pass.

The repair was pushed and the workflow manually dispatched **once** at [`8f8efa1e8591e2bb0a8a2c5cb83e58eb23aa7a51`](https://github.com/cringlekaden/Hazel/commit/8f8efa1e8591e2bb0a8a2c5cb83e58eb23aa7a51): [canonical run 40798826828](https://github.com/cringlekaden/Hazel/actions/runs/40798826828). The final inspection found both jobs **in progress**: [Linux](https://github.com/cringlekaden/Hazel/actions/runs/40798826828/job/123201735370) installing explicit prerequisites; [Windows](https://github.com/cringlekaden/Hazel/actions/runs/40798826828/job/123201736750) running Setup/build Debug. No completed failure was available. Release/runtime/tutorial/relocated-package gates are not established by this pending run. No waiting, duplicate dispatch, empty commit or further polling is needed; completed job outcomes are the outstanding automated dependency before integration confidence. This audit-only follow-up leaves that run's product, tests, current guides, workflows and dependency sources identical.

### Preservation and next step

The pre-edit manifest covers 303 authored-example/legal/pin/profile/layout/ignored-VS-Code files plus exact stash and recursive gitlinks. Final post-qualification comparison passed: all 303 hashes unchanged, exact stash `2948648a2332d8bb809fe893cd8aa554265bd6b4` retained, recursive gitlinks/pin manifests unchanged and all 14 vendor working trees clean. Sandbox draft SHA-256 remains `da6c537dfb525efd0e70fa993a4081323af4c6c336030c658e59d388ae285219`; its descriptor/startup choice remains `78d80fe4d2749aa7b7b94c806e90d59d8100428b896cc2231ced7efc85369140`. Tests use private projects/profiles/storage. No user save or example rewrite, vendor cleanup, stash operation, merge or release is authorized/performed.

The UI endpoint and bounded cleanup implementation are complete within documented feature bounds, with no known local implementation blocker. Canonical CI completion and human acceptance remain gates; no production-readiness claim is made. After completed automated gates, the next step is owner acceptance and separately authorized integration, then macOS. Human checks are representative tutorial editing/navigation/controls, interleaved property/structural Undo with save/reopen, Lightkeeper menus/settings/confirmations, physical Windows/controller/DPI where available, and competent-reader typography review. Existing IME/large-list/world-space exclusions are unchanged bounds, not new endpoint blockers.
