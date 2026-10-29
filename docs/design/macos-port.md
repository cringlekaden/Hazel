# Native Apple Silicon macOS port

## Current owner decisions and Xcode continuation — 2026-10-29

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
