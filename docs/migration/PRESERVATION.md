# Preservation inventory

Inspected before changes: local branch `master`, HEAD `b030be7`, no staged,
unstaged or untracked changes; all four submodules clean and at recorded pins.
Ignored artifacts were present: bin/, bin-int/, generated Makefiles/build/,
compile_commands.json, .vscode/, .cache/ and profiling traces. Preserve local
configuration and vendor source files. Builds regenerate only generated build
outputs; runtime experiments use copies of Sandbox assets/settings under /tmp.

| Invariant | Source/evidence | How to preserve and verify |
| --- | --- | --- |
| OS/API separation | Platform/Linux, Platform/Windows, Platform/OpenGL; system removefiles filters | Keep both OS implementations; place rendering only in OpenGL; inspect generated OS source lists every build-system change |
| Ref/Scope factories | Core/Core.h and resource factories | Retain shared/unique ownership aliases and forwarding factories; adapt upstream Base.h/call sites without raw owning pointers |
| Layer lifetime/index | LayerStack.h/.cpp, history `1b2fa06`, `b6d0d27` | Preserve vector<Scope<Layer>>, initialized insertion boundary, partitioned pops, detach/destruction exactly once. Push owns attachment; upstream Application must not attach twice |
| Application lifetime | Scope window/application, ImGui observer, EntryPoint reset | Preserve owners; review renderer/layer/context destruction order when importing upstream lifecycle |
| Official ImGui docking | ImGuiLayer.cpp, premake explicit backends, ocornut docking pin | Keep official backend compilation, GLFW callback chaining, docking and viewport context backup/restore; avoid duplicate ImGuiBuild backend inclusion |
| Modern spdlog/fmt | Log.h, modern pin, explicit casts/to-string at log sites | Use compatible formatters/streamed/string conversions for new event/GLM/path types; retain MSVC /utf-8 |
| Portable events | Event.h macros and dispatcher | Preserve EventType::type (no token-pasting extension), virtual destructor, Handled OR accumulation and generic callbacks |
| Linux & Windows support | Separate window/input .cpp/.h and CI jobs | Preserve platform-specific definitions and linking; add separate implementations for upstream platform utilities and dialogs |
| Windows CRT | premake staticruntime Off and runtime Debug/Release filters on Hazel, Sandbox, GLFW, ImGui, Glad | Keep /MDd for Debug, /MD for Release/Dist consistently; apply same settings to each new native project |
| UTF-8 | VS action filters on Hazel and Sandbox, history `434f745` | Keep /utf-8 wherever native projects include public spdlog/fmt headers |
| Premake shim | premake lines implementing flags only if absent | Handles only NoRuntimeChecks -> runtimechecks Off and NoIncrementalLink -> incrementallink Off; unexpected flags fail explicitly. Extend deliberately outside vendor if new dependencies require it |
| Vendor cleanliness | clean git submodule status and status foreach | Retain pins until evaluated, use root-owned project overrides; never recursively delete vendor Makefiles (some are source-controlled) |
| OpenGL 4.2 texture fixes | OpenGLTexture.cpp | Preserve binding-based calls, release-mode error handling, positive dimensions, RGB/RGBA validation, unpack alignment restoration and full-upload size checks |
| Shader failures | OpenGLShader.cpp | Preserve readable compiler/linker errors, cleanup and exceptions in non-Debug builds while adding upstream reflection/cache |
| Batching | Renderer2D and Sandbox2D, latest `b030be7` | Preserve rotated/textured quads, tiling, stats and capacity flush while adding target primitives/picking/text |
| CI | .github/workflows/c-cpp.yml | Keep both jobs and Debug/Release, maximum two build jobs; expand outputs/dependency setup with each new project |

Inspection also found follow-up issues: WindowsWindow calls glfwInit twice, and
Application shuts down Renderer in its destructor body before LayerStack member
destruction. Investigate these with the core lifecycle stage; do not silently
copy either behavior into new architecture. README's GLSL 330 statement describes
shader syntax, but baseline windows and context already require OpenGL 4.2.

Stage 1 leaves the core/ownership, OS, ImGui, dependency and build-setting
invariants above unchanged except for opt-in verification tooling/CI branch
coverage. The resumed user instruction authorizes migration-branch pushes for CI; merging
into master remains prohibited.

Stage 2 resolved both recorded lifecycle defects: Windows initializes GLFW once;
application shutdown releases queued captures and owned layers/ImGui before
renderer/window teardown, resetting the singleton and insertion boundary for
repeated applications. Debug/Release Linux lifecycle and graceful-close evidence
is recorded in PROGRESS.md; Windows compilation passed, engine runtime is untested.

Stage 3 retains the texture's deliberate nearest magnification filter, validates
full uploads in both configurations and extends state restoration to unpack
row/skip/PBO state. Target reflection/cache/SPIR-V paths remain available alongside
the tested GLSL 410 fallback. Backend capability checks keep supported 4.2 features
and reject unsupported MSAA formats explicitly. A process-local Mesa 4.1 test is
evidence for this backend path, not a macOS platform/runtime claim.
