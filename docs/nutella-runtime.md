# Nutella milestone

## Audit and design (2026-10-02)

Baseline: verified authoring branch `11259a2` (CI 36984752108, passed) merged
without force into master at `b0fb5fc`. Feature work stays on
`feature/nutella-runtime`; it will not be merged automatically. Vendor pins and
sources remain pristine. No overlapping builds; compiler parallelism is two.
The named migration-layout stash and local development backup were inspected;
the stash contains only Hazelnut/imgui.ini and differs from the older backup.
It remains preserved; its exact layout is restored as Hazelnut's initial resource
template, without replacing existing writable user settings.

Actual boundaries inspected: Scope-owned Application/window/layers and failure
cleanup, Scene physics/script shutdown, per-scene authored fields, transactional
Project/domain loading and reload generations, official ImGui backends,
renderer capabilities and content-addressed validated shader cache, platform
filesystem utilities, Premake shim/dependency generators, desktop regressions,
CI, ignored VS Code configuration and dependency SDKs.

1. Project remains Hazel's Ref<Project> descriptor/asset root. An engine
   RuntimeSession owns the independent Play/player scene and retains the project
   for that session. Editor retains the authored scene. Panel/active scene Refs
   are deliberate shared observations and are cleared before replacement.
2. RuntimeSession starts/updates/stops physics and scripts, owns viewport input,
   and accepts a single pending transition. First accepted request wins until
   the next boundary; duplicates coalesce. Requests are cancelled on Stop.
   Loading stages a scene before retiring the current scene. Commit happens at
   the beginning of session Update, outside every callback and registry walk;
   OnCreate requests wait until the following Update. ScriptEngine borrows the
   live session only between Start and Stop. Ordinary transitions retain its
   same-project domain, assembly metadata and watcher.
3. Engine shaders/fonts and editor resources are installation Resources;
   project assets and compiled scripts remain descriptor-relative. Writable
   shader cache and editor settings use platform user data, never installation
   resources. No working-directory changes or resource search heuristics.
4. ApplicationSpecification selects resources and ImGui explicitly. Default
   roots are executable-adjacent Resources and mono. Development tooling stages
   the same layout. Explicit CLI project paths use invocation cwd; discovery is
   executable-root-only, sorted, and requires an unambiguous selection.
5. Mono assembly/config roots are runtime configuration, not compiled SDK paths.
   Native linking stays platform-specific. Packages carry runtime assemblies,
   config, ScriptCore, project scripts and native dependency closure. Compiling
   new scripts is a separate SDK operation.
6. scripts/setup.sh and setup.ps1 call one canonical Python command tool for
   setup/build/run/script-build/package/test/database. Premake is the only build
   generator. Builds are incremental and serialized with two jobs. Setup checks
   prerequisites before bootstrapping pinned tools; it never installs system
   packages or elevates privileges.
7. Source: Hazel/{src,Resources,vendor}, Hazel-ScriptCore, Hazelnut/{src,Resources},
   Nutella/src, examples/SceneTransitions, tests/{migration,fixtures}, scripts
   (public commands plus internal dependency/test helpers), docs. Distribution:
   app executable, Resources/{shaders,fonts,Icons,Scripts}, mono/{lib,etc},
   project.hproj + Assets (Nutella), Example (Hazelnut), native libraries,
   licenses and launch instructions. Generated packages stay under ignored dist/.

Prerequisites: remove cwd/compiled Mono resource dependencies; make Application
ImGui optional with consistent guards; move ImGuizmo BeginFrame into editor;
share validated project-scene loading; implement session ownership before its
managed API and application consumers. Existing YAML/managed/ownership/reload,
UTF-8/CRT/shader-cache contracts and Linux/Windows separation are obligations.
Existing sample-dependent tests will use intentional regression fixtures rather
than disappearing with Sandbox. Historical migration provenance stays historical.

## Checkpoints and verification

| Checkpoint | Result |
| --- | --- |
| `b0fb5fc` | Completed authoring fixes merged into master; baseline CI passed. |
| `8b7dac0` | Explicit resources, optional ImGui, platform discovery and cleanup prerequisites. |
| `dbfbb1e` | Shared RuntimeSession, managed transitions, Nutella, example and retained regressions. |
| `d9f04ae` | Relocatable package closure and canonical project script builds. |
| `59c8fe5` | One shipped asset inventory, resource defaults and final ownership/tooling review. |
| `e575f4f` | Window dimensions reflect the actual native client area, including constrained desktops. |
| `b18057c` | Windows extracted archives passed both software and OpenGL 4.1 acceptance profiles. |
| `5d972f3` | Rooted/drive-relative references rejected consistently on Windows and Linux. |

Final implementation CI: [37100603151](https://github.com/cringlekaden/Hazel/actions/runs/37100603151),
commit `5d972f323eb35904b27a994a5fc5292e01a5c7d1`, fully passed on Windows 2022
and Ubuntu 24.04: Debug/Release builds and all 13 regressions per configuration,
script-build, complete dependency-closure packaging, and extracted-archive tests
under isolated software graphics and the OpenGL 4.1 path. Direct verified downloads
are in README. Downloaded archives are independently checked against archive and
all-file checksums, clean commit metadata, and absence of compiler intermediates
or test drivers. The final documentation-only checkpoint skips redundant CI;
its implementation is identical to this verified commit.

Local native HD4000 verification covers all 13 Debug and Release regression
executables: constructor cleanup and layer ownership, authored/Play/Stop field
isolation, assembly reload, physics, rendering/font/shader cache identity and
corruption recovery, and runtime transitions. RuntimeSession checks OnCreate and
OnUpdate deferral, first-wins conflicts, cancelled requests, 12 repeated scene
changes with cleared physics bodies/managed handles, failed target validation
preserving the current session, viewport mapping/resizing, domain reuse and live
reload with a pending transition. No expected result was weakened for this work.

Extracted-archive acceptance uses an unrelated working directory and paths with
spaces/Unicode, verifies checksums, hides source resources and Mono SDK roots,
and clicks MainMenu -> Level1 -> MainMenu twice in both applications. It resizes
Nutella, stops editor Play and closes both applications cleanly. Linux additionally
rejects loaded libraries from the checkout. Windows CI uses a private pinned
software driver and temporarily selects/restores a supported virtual desktop
mode; neither driver nor testing display configuration enters production archives.
The Windows acceptance investigation also corrected the engine's initial native
window-size reporting rather than compensating for incorrect viewport dimensions.

Setup was exercised from fresh CI checkouts on Windows 2022 and Ubuntu 24.04.
Grandpa reuses its isolated Mono SDK and unchanged pinned dependencies. Unchanged
local setup preserved 13 native/managed/deployed timestamps (4.27 seconds for the
default workflow; 5.38 seconds with tests). Bear refreshed 250 engine/editor/player
compiler commands. Independent Unicode project builds verified configuration
separation, ScriptCore-content invalidation, unchanged outputs, symbol cleanup
and deployed permissions. Development staging preserves matching SDK links and
unexpected files. Managed package validation excludes compiler intermediates but
still rejects conflicting shipped assemblies and unresolved runtime references.

The named user-layout stash remains intact, its exact bytes are the editor's
initial resource template, and local VS Code settings match the saved backup.
F5 defaults to Hazelnut; Nutella has a separate configuration. All recursive vendor
working trees remain pristine. Generated output and routine logs are ignored.

Automated mouse input and inspected screenshots are evidence of desktop behavior,
not human manual testing. Remaining manual checks are Windows physical graphics
hardware, high-DPI/detached editor viewports, native file-dialog interaction and
human use of the game controls/F5. Linux distributions from official CI require
Ubuntu 24.04 / glibc 2.39 or newer, X11/GLX and OpenGL 4.1; Windows packages require
Windows 10 x64 or newer and OpenGL 4.1. Running precompiled scripts requires no
compiler; compiling new scripts requires the separately documented source SDK.
