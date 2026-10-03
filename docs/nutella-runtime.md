# Nutella milestone

## Audit and design (2026-10-02)

Baseline: verified authoring branch `11259a2` (CI 36984752108, passed) merged
without force into master at `b0fb5fc`. Feature work stays on
`feature/nutella-runtime`; it will not be merged automatically. Vendor pins and
sources remain pristine. No overlapping builds; compiler parallelism is two.
The named migration-layout stash and local development backup were inspected;
the stash contains only Hazelnut/imgui.ini and differs from the older backup.
It remains preserved, and only its layout will be restored into writable settings.

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

Implementation and measured results are recorded here as checkpoints complete.
No Windows/hardware/interactive or relocation claims are made without evidence.

Prerequisite checkpoint: Linux Debug root build passed (two jobs). CoreSmoke
checks both ImGui-enabled and disabled lifetimes. RendererFeaturesSmoke passed
native HD4000 cache identity/warm/corruption tests with explicit isolated cache
roots. SceneSmoke passed managed/physics/authored/reload/watcher regressions.
EditorSmoke passed actual docked editor authoring/Play/Stop/failure/relocation
regressions. Its initial settings fixture was corrected to use the configured
resource root. ELF inspection confirms `$ORIGIN/lib` RPATH, without SDK paths.

Runtime checkpoint: Nutella replaces the demonstrations, using a single
RuntimeSession shared with editor Play. Linux Debug canonical build and all 13
retained/extended regression executables passed on native HD4000. Runtime checks
cover OnCreate/OnUpdate deferral, first-wins conflicts, cancellation, 12 repeated
transitions with cleared physics/managed handles, invalid/missing scenes and
invalid physics preserving the old session, viewport mapping/resizing, and domain
reuse. EditorSmoke retains authored/live reload/Stop isolation and failure checks.
Real native mouse clicks exercised MainMenu -> Level1 -> MainMenu in both apps;
rendered screenshots were inspected. This is automated desktop input, not a
claim of human manual testing. Local VS Code settings were preserved/backed up;
launch/tasks now use the canonical tool and example instead of removed demos.
Packaging/CI verification is the next checkpoint, not yet a completed result.
