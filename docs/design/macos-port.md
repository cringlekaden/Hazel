# Native Apple Silicon macOS port

## Qualification continuation — 2026-11-03

Target: native arm64/macOS 27.0+, pinned GLFW/shared OpenGL 4.1,
Premake/Xcode and classic Mono 6.14.1. Merge remains gated on successful desktop
and cross-platform CI acceptance. The dated sections below retain historical
evidence; this is not a completed distribution claim.

Current host: Apple M4/arm64, macOS 27.2, full Xcode 27.2 (27B48), SDK 27.2,
Apple Clang 21.0.0 (`clang-2100.3.34.1`); first-launch readiness passes.
Command-scoped selection leaves global `xcode-select` on the installed CLT.

| Gate | Evidence / remaining work |
| --- | --- |
| Apple M4 regressions | Debug/Release builds and both full 17-test suites pass at `631af3b`, including positive native tutorial authoring, bounded-loop failure cases and 2x framebuffer/picking checks. Full builds also prepare SceneTransitions scripts in both configurations |
| SDK/native services | Restricted-PATH SDK readiness, actual disposable script compilation, private-profile restart, process bounds/cancellation, leases and watcher replacement/bursts/rename/delete/recreate/shutdown pass |
| Examples | Five committed disposable projects pass actual scripts/rendering, UI Tutorial, Lightkeeper menu/settings/journey/save failure paths and scene transitions; user LastLightkeeper drafts excluded |
| Relocation | At `631af3b`, both pristine extracted products pass source/SDK/Homebrew-denied startup, 60 rendered frames, ordinary close and full teardown, mapped native closure and unchanged signatures. Both app archives and all five game archives pass signatures/checksums, arm64/closure checks, Apple pixel/picking, RuntimeSession and Mono/helper probes. Four-domain Mono tests invoke runtime-loaded helpers and record native images |
| Desktop | Source Xcode launch rendered a private project; native Open cancellation and spaces/Unicode project selection observed. Relocated Hazelnut rendered text/UI/sprites, Play/Level1/Stop, resize and native minimize/restore. Physical Command+Shift+S opened Save As; saving `Qualification ü.hazel` into the private project succeeded, repeated Save As was cancelled, and Command+Q exited normally. Relocated Nutella rendered MainMenu/Level1, resized, returned to MainMenu and quit normally. Both normal closures passed mapped-native-closure/signature checks. Clipboard user-gesture acceptance remains unexecuted |
| Hosted CI | [Run 41660986550](https://github.com/cringlekaden/Hazel/actions/runs/41660986550), `131b552`: full Linux and Windows jobs, and Mac Debug/Release CPU/service/managed job all pass. Replacement [run 41705072658](https://github.com/cringlekaden/Hazel/actions/runs/41705072658) at `631af3b` is pending for the shared application-loop changes |

After replacement CI and remaining acceptance pass, merge normally into
master using an isolated worktree to preserve the owner's current checkout,
then create the separately reviewed 3D audit branch from that exact merged SHA.

Computer Use intermittently reports `cgWindowNotFound`, including for Xcode.
Foregrounding Hazelnut restored access long enough for the observed Play/transition/
Stop checks above. The owner was away during an earlier attempt and may have
checked after timeout; that is not evidence of a missing-window defect. Timeouts
leave normal closure unverified, never a pass. No privacy/security settings,
real SDKs, user projects/profiles or unsaved Xcode buffers were changed to work
around it. Completed observed desktop launches were normal unsandboxed runs;
source/SDK-denied product acceptance is separately identified below.

Desktop attempts are stopped again at the owner's request while unavailable.
Do not start another interactive run until the owner is available. `test-packages
--interactive --interactive-app Hazelnut` or `Nutella` selects one product and
explicitly reports the other desktop check unexecuted. Per-attempt logs are kept
under `build/testing/packages/macos/desktop-*/`. Completed normal runs are
`desktop-3ndfviyt` (Hazelnut) and `desktop-gyvg0tu3` (Nutella). A later isolated
interactive attempt timed out without owner input; it is not a pass.

The maintained `--smoke-test` product diagnostic closes the remaining unattended
relocation gap: require absolute private data/save roots, render 60 ordinary
application frames with engine draws, request closure through the existing window/
document guard and report only after teardown. No gameplay input, validation
bypass or desktop-control service is added. CoreSmoke checks real quad readback,
normal close dispatch, no-draw failure and blocked-close preservation. Package
qualification launches both pristine products with source/SDK/Homebrew reads
denied, checks mapped native closure and verifies signatures before adding
test-only probes. Local results pass at `631af3b`: Hazelnut submitted 236 engine
draws, Nutella 180; both report full teardown and valid final signatures. Separate
pixel tests supply rendering correctness evidence. Products report
`Apple / Apple M4 / 4.1 Metal - 91.7`, GLSL 410 and 16 effective texture slots.
The five example archives were rebuilt from descriptors and LastLightkeeper menu
bytes verified against committed source, excluding the owner's drafts. Archive
metadata truthfully records a dirty checkout due to those unrelated files.

Temporary extracted product logs include an OS
`sandbox_extension_issue_file_to_process ... Operation not permitted` diagnostic
in both observed normal and restricted launches. Rendering, Unicode native Save
As, scripting/transitions and normal closure succeeded in their recorded runs;
the precise OS origin is unresolved. It is retained in logs, not suppressed or
treated as Developer ID/Gatekeeper evidence.

Clipboard read/restore was not automated: current macOS can ask for permission
to read the general pasteboard. No clipboard contents or security preferences
were changed. Physical user-gesture copy/paste remains a manual acceptance item,
alongside physical Retina/mixed-display/controller checks. Command+Shift+S and
Command+Q have actual owner-operated evidence.

Build/workflow repairs: `open` preserves generated test targets and local schemes;
Mac game dispatch prepares the shared pinned project reader. Full Mac builds now
prepare default SceneTransitions scripts like Windows/Linux. Xcode `ARCHS` comes
from the existing target contract rather than pinned Premake's
`NATIVE_ARCH_ACTUAL` fallback. The template retains Windows/Linux x64 solution
identity (restoring `Debug|x64`/`Release|x64`) and portable managed IL on Mac;
actual pinned-Premake Windows generation verifies those configurations.
Vendors/pins remain unchanged.

The interpreter fixture now emits a resolved path like the real Python probe and
checks version/order and physical executable identity,
rather than comparing path spellings from `GetModuleFileName` and `canonical()`.
Separate diagnostics distinguish failed probes, wrong versions and wrong files;
the focused Mac editor regression and both CPU-only resolved-path fixture modes
pass. The replacement Windows CI at `131b552` also passes.
Xcode intermediate paths also restore Apple's [configuration-specific default](https://developer.apple.com/documentation/xcode/build-settings-reference):
pinned Premake had set `CONFIGURATION_TEMP_DIR=$(OBJROOT)`, making Debug/Release
object directories identical. The maintained Premake override uses
`$(PROJECT_TEMP_DIR)/$(CONFIGURATION)`, preserving local DerivedData choices.
Debug and Release builds each compiled 417 C/C++ files in their new directories;
returning to Debug compiled zero. This verifies preservation across configuration
switches without changing global Xcode settings or cleaning existing caches.
A subsequent one-file interpreter-fixture edit compiled exactly one C++ file.

Earlier [CI at `3aae0ba`](https://github.com/cringlekaden/Hazel/actions/runs/41559537302)
exposed the Windows `interface` COM macro collision and Linux wide-line use in
a forward-compatible context. Repairs rename the variable, retain previous
Linux/Windows core-context requests and derive usable width from actual context
flags. The [OpenGL core specification](https://registry.khronos.org/OpenGL/specs/gl/glspec43.core.pdf)
removes wide lines from forward-compatible contexts despite a wider advertised
device range. Bundle versions use valid development `0.1.0`/`1`; exact identity
stays in `build.json`. Pinned Glad still probes absent legacy `glVertexP*` packed
fixed-function symbols in Xcode logs; exercised core draws succeed. No diagnostic
was suppressed.

`InstallationLayout` now owns loose-product versus bundle resource, Mono,
project and metadata paths. Nutella discovers `Contents/Resources/Project` in
a bundle. Hazelnut seeds its bundled example into a versioned private profile
workspace before editing, preserving signed bundle contents. Source SDK
discovery remains separate and never falls back to cwd or bundled runtime files.
Native dylibs live in `Contents/Frameworks`; Mono profiles/config live in
`Contents/Resources/Mono`. Runtime dllmap entries resolve helpers through rpath.
The macOS GLFW adapter disables Cocoa's default change to the Resources cwd.
Native SDK tools stage their runtime independently of editor post-build timing.

The canonical test runner accepts `--suite cpu` and repeatable `--only NAME`;
both explicitly report excluded coverage. The full Mac runner uses the real
Apple driver and committed disposable tutorial content. Xcode's generated
`MacOSQualification` target builds all maintained regressions, native services,
SDK probes and managed fixtures. Linux/Windows defaults retain their full suite.
The Mac CI job uses GitHub's explicit [`xcode-27` preview image](https://github.com/actions/runner-images/blob/main/images/macos/xcode-27-arm64-Readme.md),
which documents macOS 27.0.1/arm64. Its CPU/service/managed subset makes no GPU
or interactive-desktop claim; physical Apple rendering stays a local gate.

Concrete repairs: both Renderer2D batching paths bind complete fallback textures
for unused active samplers; switching back from ordered text restores its sampler.
Apple's combined shader archive namespaces object members so dsymutil can resolve
their debug maps. Darwin watchers tolerate stat/enumeration races and distinguish
atomic replacements by inode. Restricted GUI PATH preparation includes standard
OS recipe tools. Tests count Darwin descriptors through libproc and compare
validated Python locations canonically (macOS `/var` aliases `/private/var`).
Offscreen viewport pixels follow the containing GLFW window's framebuffer scale;
UI input stays logical and picking converts once. Detached ImGui draw data gets
its native window scale at the existing integration boundary.

Bundle production includes runtime profiles/config/native helpers, ScriptCore,
project content, notices/provenance, Info.plist, metadata and checksums. It audits
arm64/minimum versions/load commands and signs inside-out after all mutations.
Relocation tests use a process sandbox denying checkout/Homebrew reads, without
renaming the owner's SDK or resources; test-only rendering/managed/helper probes
are distinct from actual product interaction. No Developer ID, notarization,
Gatekeeper-download acceptance or SDK publication is implied.

Current qualification commands (build configurations sequentially):

```sh
python3 scripts/hazel.py build --config Debug --tests
python3 scripts/hazel.py test --config Debug
python3 scripts/hazel.py build --config Release --tests
python3 scripts/hazel.py test --config Release
python3 scripts/hazel.py test-games --config Release
python3 scripts/hazel.py package --project /absolute/prepared/Game.hproj --output dist
python3 scripts/hazel.py test-packages --output dist
python3 scripts/hazel.py test-packages --output dist --interactive
```

Automated mode launches pristine relocated products with private profiles and
no resource/Mono overrides, then runs distinct pixel/managed/helper probes.
Interactive mode adds observed human input and waits for normal user closure;
neither mode treats timeout/killed processes as acceptance.
Current display evidence is 1x; physical Retina/mixed-display transitions and
controller acceptance remain manual. Project P/Invoke export remains rejected;
optional Mono APIs requiring additional native software are not a bundled SDK promise.

## Historical application foundation — 2026-10-29

The current minimum is **macOS 27.0**, native arm64. Full Xcode is the preferred
development environment, with pinned Premake's `xcode4` application generator.
Mono 6.14.1's 27.0 minimum is accepted. The earlier macOS 13 producer gate is
retired; no source rebuild to support older macOS is required.

Full Xcode at `/Volumes/External/Applications/Xcode.app` reports version 27.2,
build 27B48, macOS SDK 27.2, Apple Clang 21.0.0 (`clang-2100.3.34.1`). License
check passes. First-launch status returns 69 without a diagnostic, but ordinary
macOS target compilation proceeds. No license or first-launch action was run.
Canonical tooling uses a command-scoped `DEVELOPER_DIR`, honoring explicit
`HAZEL_DEVELOPER_DIR` or `DEVELOPER_DIR`, then discovering the external Xcode
and standard installed Xcode locations. Global selection remains unchanged.

```sh
python3 scripts/hazel.py diagnose
python3 scripts/hazel.py dependencies
python3 scripts/hazel.py generate
python3 scripts/hazel.py build --config Debug
python3 scripts/hazel.py build --config Release
python3 scripts/hazel.py script-build examples/SceneTransitions/SceneTransitions.hproj --config Debug
python3 scripts/hazel.py open
```

Workspace: `Hazel.xcworkspace`; generated shared schemes: Hazelnut and Nutella.
Products: `bin/<configuration>-macos-arm64/<target>`. Build concurrency: two.
Premake and the maintained scheme producer own generated project files.
Managed build phases use the pinned checkout-owned Mono/mcs because Premake's
Xcode action does not implement C#. Shader production retains its internal
Premake/make path. Application resources and Mono use the existing executable-
adjacent development layout; `.app` layout remains later work.

Do not run simultaneous Xcode builds against the same workspace/DerivedData:
Xcode's build database rejects the second writer. Canonical builds serialize
the two application schemes under the existing checkout build lock. Native
builds select the generic macOS destination because this Xcode reports the
physical host destination as arm64e; compilation targets remain native arm64.

Verified continuation results:

- Debug and Release Hazelnut and Nutella compile/link through `xcodebuild`;
  the canonical serialized Release command passes both schemes. All four native
  executables are arm64, with minimum 27.0 and SDK 27.2 load commands.
- SceneTransitions compiles through the canonical `script-build` command;
  both applications load its managed assembly and reach their ready state.
  Both observed launches completed normal Mono/GLFW shutdown with exit status 0.
- Real driver: Apple / Apple M4 / OpenGL `4.1 Metal - 91.7`. Capabilities select
  16 sampler slots and shaderc/Cross GLSL 410. Apple rejected mismatched varying
  names, so the shared shader producer now canonicalizes location-linked names.
  ImGui requests GLSL 410; unused active sampler elements receive white textures.
- The existing Renderer2D GPU regression passes default and two-slot settings,
  sprites/text/UI, color/integer picking, clips, rich typography/bidi/compositing,
  batching/capacity, failed-start recovery and repeated context teardown.
- `tests/macos/PlatformServices.cpp` passes structured process argv/cwd/streams,
  descriptor closure, flood bounds, timeout/cancellation, exclusive directory
  publication and lease ownership/release.
- The Mono embedding/domain/relocation fixture passes again at target 27.0.

Hazelnut was also observed running beneath Xcode's LLDB/debugserver with the
generated scheme's expanded SceneTransitions project argument. Visual inspection
of that GUI session remains unverified: Computer Use Accessibility/Screen
Recording permissions were unavailable. Direct executable launches provide the
logged ready/shutdown evidence above. Human Retina/drag/picking interaction, native dialog
interaction, watcher replacement/shutdown stress and Play/Stop/reload remain
qualification work. No full distribution claim is made.

Focused checks are available after `generate --tests`, through Xcode schemes
`MacOSPlatformServices` and `MigrationRenderer2DSmoke`. The latter uses the
existing harness fixture layout: copy engine Resources plus tests/fixtures/assets
to an ignored working directory's `assets`, and use `typography_inputs` from
`scripts/internal/tests.py`. Set explicit HAZEL_RESOURCES, HAZEL_MONO, HAZEL_DATA
and, for the unstaged test executable, DYLD_LIBRARY_PATH to the local Mono SDK.
The full test/export/package CLI deliberately reports its unqualified status on
macOS rather than running the Linux harness implicitly.

Build logs still contain existing source/vendor warnings and `dsymutil` warnings
for some combined shader archive symbols. Native linking and the exercised
runtime paths pass; complete debug-symbol quality remains a follow-up. Logs and
local manifests are retained under ignored `build/macos-*` and `build/testing`.
The final GPU suite still prints a log-once Apple `GLD_TEXTURE_INDEX_2D`
unloadable-texture diagnostic while all reference pixels/picking assertions pass;
triage its originating draw during the next graphics/editor qualification.
Linux/Windows CI definitions and vendor gitlinks were unchanged; those operating
systems were not executed during this native Mac run.

Mono acquisition now downloads the checksum-pinned bottle directly from GHCR
when absent and validates existing SDK hashes when reused. Consumers require
neither Homebrew nor global Mono. All 21 candidate native files declare arm64
and minimum 27.0. Original notices are retained. No SDK artifact is published.

The next work is actual application compile/launch, Apple OpenGL sprites/text/UI
and framebuffer/picking acceptance, managed Play/Stop, native-service checks,
then relocatable bundles and Debug/Release qualification. Existing GLFW, shared
OpenGL and separate Windows/Linux/macOS adapters remain the architecture.

## Source SDK integration repair — 2026-10-29

The owner confirmed Xcode build/launch. SDK selection then exposed hardcoded
Linux/x86_64/Debug paths in `HazelSDK.cpp`. These are now replaced by the shared
target records in `scripts/internal/toolchain.json`: Premake supplies a compiled
`BuildIdentity`, and Python's target selector reads the same records. Installed
native consumers never query the host to guess their target or product paths.
SDK validation distinguishes missing/invalid selections, incompatible metadata,
compatible checkouts needing preparation, and prepared configuration-specific
products. Managed/native contract probes and compiler/profile readiness remain
separate from file discovery. An explicit invalid selection never falls back.

Automatic development discovery recognizes only the bounded executable layout
`bin/<configuration>-<system>-<architecture>/Hazelnut`, or an explicit application
metadata locator. It does not use cwd or walk arbitrary ancestors. Source SDK
means the checkout root containing `premake5.lua` and `scripts/hazel.py`.
Hazelnut's generated Xcode dependencies include HazelProject, PackageAudit and
ScriptCore. Build that scheme in the editor's configuration; Release/Dist editors
use Release tooling, Debug editors use Debug. No Linux path or Debug fallback is
used for a requested script configuration.

For a fresh source SDK use the dependency/generate/build ladder above. For an
already prepared checkout, regenerate and build Hazelnut in Xcode, or run
`python3 scripts/hazel.py build --config Debug` (substitute Release as needed).
Then open **Project > Build Scripts**: script readiness probes the selected SDK,
Python, native authoring contract, ScriptCore contract, mcs and 4.7.2 profile.
Build Scripts publishes/reloads the project's assembly. Mac export readiness
explicitly reports unimplemented support without blocking script readiness.

Python discovery belongs to `Toolchain`: explicit preferences and SDK-owned
interpreters retain precedence, then macOS considers stable Homebrew/python.org
locations and the system interpreter, validating complete executable paths.
No Terminal PATH is required. Native Xcode selection belongs to the Python
producer boundary: explicit developer-directory overrides, remembered local
selection, selected full Xcode, then bounded standard/external Applications
locations. Generation records Xcode and Mono selections in ignored
`build/tools/macos-toolchain.json`; GUI phases need not inherit Terminal's
environment. Script-only operations do not require Xcode discovery at import.

Generated launch arguments are quoted for spaces/Unicode. Regeneration preserves
existing scheme arguments, environment and launch configuration while refreshing
generated target IDs. The optional verification scheme uses a private profile
and a test-only LaunchServices wrapper, not a distributable app package.

Focused verification:

- `python3 tests/macos/sdk_workflow.py`: Windows/Linux/Mac target selection,
  unsupported combinations, explicit Xcode failure, remembered producer roots
  and preservation of customized schemes.
- Generate with `--tests`, build `MacOSSDKIntegration` Debug, then run it with
  checkout and a fresh private directory followed by `initial`. It tests SDK
  classification, explicit precedence, missing configuration/foreign target
  metadata, Unicode paths, missing Python, real editor tool-service readiness,
  native project creation, script compilation, export rejection and persisted
  selection after process restart. Run with `PATH=/usr/bin:/bin` and unset
  developer-directory variables. The service suite passed on this Mac.
- `python3 tests/macos/sdk_workflow.py --gui-scheme` prepares **Hazelnut SDK
  Verification** without changing the owner's normal scheme. Actual Xcode GUI
  verification showed automatic SDK Ready at the checkout, restricted-PATH
  Python, successful Check Readiness, and Build Scripts: Succeeded with assembly
  reload, using a disposable Unicode-path project/profile.

The platform review retained existing resource/settings/save ownership,
validated absolute child tools, explicit OS source selection, and renderer
capability-based feature selection. Editor shortcuts already accept Control or
Super; runtime bindings remain separately owned. Logical/framebuffer dimensions
already have distinct GLFW/common services. No generic window/input replacement
or new service locator was introduced.

Specific remaining work: full Retina/drag/picking interaction; native watcher
replacement/coalescing/shutdown stress; complete Debug/Release authoring and
Windows/Linux runtime qualification; production bundle/native closure/signing;
full Mac test/package runners. Their current entry points remain explicit about
unsupported scope. Linux/Windows fixture contracts were updated to use compiled
identity, but no Windows/Linux runtime result is claimed from these Mac checks.

## Historical feasibility evidence — 2026-10-29

The initial run used CLT, Apple Clang 17.0.0 and SDK 26.2. Pinned Premake
`71f2d33946947e9cf704f00c24200381e360f593` bootstrapped natively and drove a
C++/Objective-C++/Foundation probe using gmake, with a recorded experimental
13.0 minimum. Xcode project generation also passed. That probe remains historical
evidence; it does not set the current application deployment floor.

The existing shader pins in `scripts/dependencies/shader-tools.json` remain
unchanged. Debug/Release consumers validated Vulkan 1.2 SPIR-V, reflection and
GLSL 410 emission. The continuation rebuilt both archives with SDK 27.2 and
target 27.0. Apple uses `libtool -static`; Linux retains GNU MRI archive merging.

The official Mono 6.12.0.206 macOS package has only i386/x86_64 runtime slices
(package SHA-256
`80b0dbfa59ba9ed76dbf1393998e6a2ed2d1ccc8f5850c7a46fbe31a2aea88d8`).
The macOS-only successor is classic Mono 6.14.1, source
https://dl.winehq.org/mono/sources/mono/mono-6.14.1.tar.xz, source SHA-256
`3024c97c0bc8cbcd611c401d5f994528704108ceb31f31b28dea4783004d0820`.
Exact producer: formula revision
`5251e778a43128b71a6223d3d8c39e1ff3d37b04`, arm64_golden_gate bottle SHA-256
`bb3ced09c44ad6a265f35fbc4538151f340fe44c915042862b7557cd84f9d29d`.
Original LICENSE, COPYING.LIB and shipped notices stay with the SDK.

The maintained embedding fixture passed explicit roots/config, mcs compilation
against 4.7.2, real ScriptCore loading, internal calls, four AppDomain
create/invoke/unload cycles, cleanup, and relocation into a path containing
spaces/Unicode with dynamic closure inspection. It passed again under the
current 27.0 target. Run `python3 scripts/macos/test-mono-embedding.py`.
No global Mono/Homebrew runtime or published Hazel SDK artifact is required.

## Qualification and resume

Public support remains experimental until native authoring, Retina/picking,
service behavior, packages and both configurations complete qualification.
Generation, native compilation, process startup, Apple GPU readback and an
actual Xcode GUI scheme launch are separate evidence levels.

The intended architecture remains common Hazel APIs over separate OS adapters,
with shared OpenGL graphics ownership. Native window decorations stay enabled.
Eventual bundle identifiers are `com.kadencringle.hazel.hazelnut` and
`com.kadencringle.hazel.nutella`. Bundles/signing/notarization, Intel/universal,
Metal/Vulkan and 3D/PBR/IBL remain future work.
