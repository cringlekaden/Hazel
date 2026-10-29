# Native Apple Silicon macOS port

This is the single implementation plan and progress record for the native macOS
port. It does not claim public macOS support. The maintained applications still
target Windows x64 and Linux x86_64 until the remaining stages pass.

## Target and decisions

- Native arm64 only for this milestone, without Rosetta. The initial deployment
  target is macOS 13.0, subject to every shipped native dependency actually
  meeting it.
- Retain the pinned GLFW and shared OpenGL backend. A later stage will add
  separate macOS platform adapters and exercise a real forward-compatible
  OpenGL 4.1 core context on Apple hardware.
- Retain Premake and `scripts/hazel.py` as the canonical build architecture.
  Use the pinned `gmake` generator for normal command-line builds; Xcode project
  generation is available but is not a reason to require the full IDE.
- Retain classic Mono embedding and the existing managed API/profile contract.
  A narrowly macOS-specific version is allowed because the existing official
  Mono 6.12 macOS package has no arm64 runtime.
- Eventual bundle identifiers are `com.kadencringle.hazel.hazelnut` and
  `com.kadencringle.hazel.nutella`. Bundles, signing and notarization are later
  stages, not results of the current probes.

Source compatibility, successful compilation, runtime behavior, real Apple GPU
rendering, relocatable packaging, and signed/notarized distribution are separate
qualification levels. Passing an earlier level never implies a later one.

## Staged plan

| Stage | Scope and acceptance boundary | Status |
| --- | --- | --- |
| 0 | Explicit host/target diagnostics; native pinned Premake; C++ and Objective-C++ generation/build; Foundation linking; deployment-target evidence | Implemented and verified on the host below |
| 1 | Native pinned shader libraries and consumer pipeline; choose/prove classic Mono embedding, managed compiler/profile, reload domains and relocated native closure | Feasibility proven; production Mono deployment floor remains blocked |
| 2 | Platform/build foundation: macOS detection/debug break, Premake source filters, PCH/Objective-C++, framework links, dependency identities and common path/resource ownership | Not started |
| 3 | Separate macOS window/input and native-service adapters: GLFW lifecycle/input/Retina/clipboard, Cocoa committed text/dialogs/menus, user roots, watchers, processes and dylib services | Not started |
| 4 | Real Apple OpenGL 4.1: forward-compatible context, Glad capabilities, shader/cache path, framebuffers/picking/queries, Renderer2D/text/runtime UI and ImGui viewport policy | Not started |
| 5 | Hazelnut/Nutella integration: application/session/scene lifecycle, script build/reload, Play/Stop/transitions/shutdown, authoring/history, audio and export readiness | Not started |
| 6 | Relocatable `.app` bundles: plist/layout/resources, Mono and dylib closure/rpaths, Application Support/cache/save roots, notices and launch outside the checkout | Not started |
| 7 | Debug/Release qualification, real editor/game and Apple-GPU acceptance, relocated packages, Windows/Linux regression CI, then optional signing/notarization work | Not started |

Stages 2-3 should be one bounded platform-foundation milestone. Stage 4 is a
separate graphics/hardware milestone. Stages 5-6 can follow as runtime/editor and
packaging milestones; stage 7 is final qualification. Metal, Vulkan, universal
binaries, Intel Mac, 3D, PBR and IBL remain outside this port.

## Verified stage 0 baseline

Audited checkout: `/Volumes/External/kadencringle/Projects/Hazel`, branch
`feature/macos-support`, based on integrated master
`ca4d94d581df4917b31f3c98221cc33e07257815`. Submodule gitlinks and vendor
trees were not changed.

Host evidence on 2026-10-29:

| Concern | Observed value |
| --- | --- |
| Machine | Mac16,10; arm64 host and process; 16 GiB RAM |
| OS | macOS 27.2, build 26B123 |
| Selected developer directory | `/Library/Developer/CommandLineTools` (Command Line Tools, not full Xcode) |
| Compiler / SDK | Apple clang 17.0.0 (`clang-1700.6.3.2`); macOS SDK 26.2 |
| Python / Make | Python 3.14.6; GNU Make 3.81 |
| Checkout volume | external APFS, case-insensitive; symlinks and executable bits work |

`python3 scripts/hazel.py diagnose` reports the host and target separately,
rejects non-arm64 macOS execution, records the selected tools/compiler/SDK and
labels the path as dependency feasibility rather than application support. It
does not install tools or require full Xcode.

The exact pinned Premake commit is
`71f2d33946947e9cf704f00c24200381e360f593`. The checkout-owned bootstrap uses
`PLATFORM=AARCH64`, `MACOSX_DEPLOYMENT_TARGET=13.0` and two jobs. Its output is
a native arm64 Premake 5.0.0-dev executable. The maintained probe at
`tests/macos/feasibility/premake` generated and built C++17 plus Objective-C++,
linked Foundation, ran successfully, and has `LC_BUILD_VERSION` minimum 13.0.
The pin's `gmake` generator omits `systemversion` from the final link command, so
the probe records the necessary explicit link option. The `xcode4` generator
also generated a project with the expected architecture, source and framework;
it was not built because full Xcode is not installed and is unnecessary here.

Recommendation: keep `gmake` for canonical command-line builds. Require full
Xcode later only if a verified stage needs Xcode-only tooling (for example,
archive/notarization workflows or GUI debugging), not for compilation merely
because the target is macOS. Current stages work with the selected CLT.

## Verified pinned shader toolchain

The existing shader pins remain unchanged: shaderc v2023.6 commit
`39aa522785f130130927cd4766a37e8813af6d66`, glslang
`76b52ebf77833908dc4c0dd6c70a9c357ac720bd`, SPIRV-Headers
`124a9665e464ef98b8b718d572d5f329311061eb`, SPIRV-Tools
`e553b884c7c9febaa4e52334f683641fb5f196a0`, and SPIRV-Cross
`2de1265fca722929785d9acdec4ab728c47a0254`.

The Premake dependency workspace now selects macOS/arm64 explicitly, uses Apple
Clang, applies the SDK/deployment target and writes architecture-specific cache
and installation identities. macOS uses `xcrun libtool -static` to combine the
shader archives; Linux retains GNU `ar -M`, and Windows retains `lib.exe`.

The Release libraries were built natively with two jobs. All installed archives
contain arm64 objects. The maintained consumer at
`tests/macos/feasibility/shader` linked the resulting libraries, compiled a
representative GLSL 450 vertex shader to Vulkan 1.2 SPIR-V, validated it,
reflected its uniform binding with SPIRV-Cross, and emitted desktop GLSL 410
without the 420-pack extension. This proves the native producer/link pipeline;
it does not prove acceptance by Apple's OpenGL driver.

## Classic Mono feasibility

### Version decision and provenance

The official signed/notarized Mono 6.12.0.206 macOS package was inspected
directly from `archive/6.12.0/macos-10-universal` (package SHA-256
`80b0dbfa59ba9ed76dbf1393998e6a2ed2d1ccc8f5850c7a46fbe31a2aea88d8`). Its
`libmonosgen-2.0.1.dylib` has only i386 and x86_64 slices, so the current macOS
distribution cannot load into a native arm64 Hazel process. The existing
Windows/Linux 6.12 choices remain unchanged.

The bounded macOS candidate is classic Mono 6.14.1 from
`https://dl.winehq.org/mono/sources/mono/mono-6.14.1.tar.xz`, source SHA-256
`3024c97c0bc8cbcd611c401d5f994528704108ceb31f31b28dea4783004d0820`.
For this feasibility run, the exact arm64 candidate producer is Homebrew formula
revision `5251e778a43128b71a6223d3d8c39e1ff3d37b04`, bottle SHA-256
`bb3ced09c44ad6a265f35fbc4538151f340fe44c915042862b7557cd84f9d29d`.
Homebrew was acquisition evidence only; it is neither a consumer requirement nor
the final Hazel SDK source. Preserve the candidate's `LICENSE`, `COPYING.LIB`
and component notices; complete a notice review for the eventual source-built
SDK before redistribution.

`scripts/macos/prepare-mono-sdk.py` checksum-validates that local bottle, stages
it under ignored `build/dependencies/mono/macos-arm64/sdk`, replaces producer
install names with `@rpath`, ad-hoc signs modified Mach-O files, verifies every
native file is arm64, rejects Homebrew/Cellar dependencies and emits an ignored
candidate manifest with hashes, architectures, minimum OS versions, load
commands, provenance, notices and producer/consumer requirements.

Important limitation: all 21 native files in that bottle declare macOS 27.0.
It works on this macOS 27.2 host but is not a macOS 13 SDK. The next producer
gate is a source build of the same recorded 6.14.1 source with an arm64 compiler
combination known to build a stable JIT, explicitly targeting 13.0, followed by
the same manifest and fixture. The current Homebrew recipe uses a separate LLVM
toolchain because its recorded Apple-Clang configuration is not considered
runtime-safe. Those producer tools may be checkout-owned or deliberately
installed by the owner; ordinary Hazel consumers must not require Homebrew,
LLVM, autotools or a Mono installation. Do not promote or distribute the current
bottle-derived candidate as the Hazel SDK.

### Embedding, lifecycle and relocation evidence

The maintained fixture at `tests/macos/feasibility/mono` and runner
`scripts/macos/test-mono-embedding.py` proved on native arm64:

- explicit Mono library/config roots through `mono_set_dirs` and
  `mono_config_parse`, followed by JIT initialization;
- compilation with the candidate's `mcs` and 4.7.2 profile, including the real
  `Hazel-ScriptCore` sources;
- managed assembly load/method invocation and a Hazel-shaped internal call;
- four create, switch, load, invoke, restore and unload AppDomain cycles;
- orderly `mono_jit_cleanup`;
- an arm64 native host loading arm64 Mono through
  `@executable_path/../sdk/lib`, with no non-system checkout, Homebrew, producer
  or developer-SDK libraries observed at runtime;
- compilation and execution after moving the complete layout to an unrelated
  temporary path containing spaces and Unicode, launched from a different
  working directory with Mono/DYLD path variables removed.

The pass line is: `PASS: arm64 Mono JIT, 4.7.2 compiler/profile, internal call,
ScriptCore load, four AppDomain unload/reloads, cleanup`. Evidence and the local
candidate manifest remain ignored build products. This establishes classic-Mono
ABI, profile, lifecycle and relocation feasibility, not the macOS 13 deployment
claim and not integration into Hazelnut/Nutella.

## Reproduction boundaries

All generated repositories, downloads, builds, logs, manifests and relocated
layouts live under ignored `build/` or temporary directories. Maintained probe
source lives under `tests/macos/feasibility`; producer/runner logic lives under
`scripts/macos`. No system package, developer-directory, security setting or
vendor gitlink is changed.

Canonical diagnostics and project-owned Premake bootstrap:

```sh
python3 scripts/hazel.py diagnose
python3 scripts/hazel.py bootstrap-premake
premake=build/tools/premake-core/bin/release/premake5
"$premake" --file="$PWD/tests/macos/feasibility/premake/premake5.lua" gmake
make -C build/testing/macos/premake config=release -j2
build/testing/macos/premake/bin/PremakeProbe
```

The shader producer uses two jobs:

```sh
python3 scripts/dependencies/build-shader-tools.py \
  --config Release --jobs 2 --premake build/tools/premake-core/bin/release/premake5
prefix="$PWD/build/dependencies/install/Release-macos-arm64"
mkdir -p build/testing/macos/shader
xcrun clang++ -std=c++17 -arch arm64 -mmacosx-version-min=13.0 \
  -I"$prefix/include" tests/macos/feasibility/shader/main.cpp \
  -L"$prefix/lib" -lshaderc_combined -lspirv-cross-glsl \
  -lspirv-cross-core -o build/testing/macos/shader/ShaderProbe
build/testing/macos/shader/ShaderProbe
```

The Mono candidate requires the exact checksum-pinned bottle already present in
the ignored download directory; staging never invokes Homebrew:

```sh
python3 scripts/macos/prepare-mono-sdk.py --archive path/to/the-pinned-bottle.tar.gz
python3 scripts/macos/test-mono-embedding.py
```

Producer requirements are source/archive acquisition, an arm64 native compiler
toolchain and install-name/signing inspection tools. Future ordinary setup must
only acquire a published checksum-pinned Hazel Mono SDK into the checkout and
must need no Homebrew or system Mono. Until the macOS-13 source producer passes,
the CLI intentionally refuses to present application setup/build as supported.

## Remaining blockers and next stage

The immediate blocker is a redistributable native Mono SDK built from the pinned
6.14.1 source with a verified macOS 13.0 minimum; rerun the complete maintained
fixture against it and record its full dynamically loaded closure. Signing and
notarization credentials are neither needed nor assumed for that local gate.

After that gate, the next bounded implementation prompt should cover stage 2 and
the non-UI portion of stage 3: explicit platform/build filters, macOS PCH and
Objective-C++ integration, GLFW/Cocoa adapter skeletons, executable/resource and
writable-root ownership, process/watcher/dylib services, and focused CPU/service
tests. It must stop before claiming Apple OpenGL rendering or complete editor
support. The following prompt should then handle real OpenGL 4.1/Retina/ImGui
hardware qualification as stage 4.
