# Source comparison

Baseline: `b030be7` (local `master`, clean tracked tree and submodules at inspection).
Target: [TheCherno/Hazel at 1feb70572fa87fa1c4ba784a2cfeada5b4a500db](https://github.com/TheCherno/Hazel/tree/1feb70572fa87fa1c4ba784a2cfeada5b4a500db).
The target was downloaded as actual Git source and checked out detached in
`/tmp/hazel-upstream-1feb705`. Its commit subject is “Fix for cloning issues with
msdf-atlas-gen (#617)”, dated 2023-10-27. Do not follow a moving master during migration.

`evidence/source-comparison.tsv` compares every tracked entry by mode, type and
Git object hash, including checked-in vendor files and submodule gitlinks.
`evidence/comparison-summary.json` records totals and both sets of dependency pins.
These compare the baseline commits, not generated files or submodule contents.
Reproduce from the repository root:

```sh
python3 scripts/migration/compare-upstream.py /tmp/hazel-upstream-1feb705
```

There are 90 local and 829 upstream entries: 748 upstream-only, nine local-only,
77 changed, four identical. Engine source has 51 upstream-only, seven local-only
and 59 changed entries. A changed hash indicates a review requirement, not an
instruction to overwrite local code. The comparison is a baseline snapshot;
implementation provenance is recorded separately in `PROGRESS.md`.

| Area | Baseline | Upstream target and migration implications |
| --- | --- | --- |
| Applications | Sandbox, static Hazel | Add Hazelnut editor and Hazel-ScriptCore C# library; preserve Sandbox |
| Core | Scope-owned windows, layers and input singleton | Base/Assert/PlatformDetection, application specification and arguments, main-thread queue, UUID, buffers, filesystem, timer; adapt call sites to local ownership |
| Platform | Separate Linux/Windows windows and input; shared OpenGL | Target has Windows window/input/utils, Win32 dialogs, no Linux implementation; port utilities and input API to both OS directories |
| Renderer | Orthographic camera, batched rotated/textured quads and stats | Generic and editor cameras, uniform buffers, framebuffers, integer picking, circles, lines, rectangles, fonts/MSDF text, texture specifications, scene cameras |
| Shader pipeline | Direct GLSL compile with error handling and resource cleanup | shaderc Vulkan SPIR-V compilation, SPIRV-Cross reflection/conversion, OpenGL SPIR-V caching/loading; preserve this architecture while adding GLSL 4.20 program loading |
| Scene | No ECS/scene module | EnTT entities/components, scene copy/duplicate, runtime/simulation/editor updates, YAML serialization, native and managed scripts |
| Physics | None | Box2D rigid bodies/colliders and runtime lifecycle |
| Editor | Sandbox ImGui controls | Docking, viewport/gizmo, hierarchy/component inspector, content browser, drag/drop, selection/picking, scene/project save/load, play/simulate/stop, toolbar and assets |
| Projects/scripting | None | Project configuration/serializer, Mono engine/glue, managed components/input/math, assembly reload/filewatch, example project/scripts |
| ImGui | Official GLFW/OpenGL3 backend translation units; docking/viewports | Target uses TheCherno fork and ImGuiBuild.cpp; preserve official backend build and one definition per backend |
| Build | GCC/gmake and VS2022, dynamic CRT, UTF-8, current Premake shim | Upstream uses legacy flags, Windows .lib paths, Vulkan SDK and bundled Mono; translate platform dependencies rather than dropping features |
| Automation | Linux/Windows Debug+Release Actions | Target has no equivalent local workflow; retain and expand ours for new applications/dependencies |

Local-only source includes Core.h, MouseButtonCodes.h, Linux window/input,
WindowsInput.h. Local-only tooling includes Linux generation and GitHub Actions.
MouseCodes/Base/input API renames require coordinated adapters, not deletion of
the deliberate implementations.

## Dependency decisions

No pin changes in stage 1. Exact hashes are in the JSON/TSV; the following
decisions are provisional until the consuming feature is built and tested.

| Dependency | Decision and required evaluation |
| --- | --- |
| spdlog | Keep local `57cb5fb` instead of target `1aace95`; modern bundled fmt already builds. Update upstream logging adapters for GLM/events/filesystem explicitly |
| GLFW | Keep local `026a148` instead of target `9bed794`; separate X11/Win32 source selection already works. Preserve clean submodule and external Premake overrides |
| ImGui | Keep local docking `e25e452` instead of target fork `3cf61f6` initially. Evaluate editor APIs, font/icon setup, ImGuizmo compatibility and viewport behavior before any replacement |
| GLM | Keep local `8d1fd52` instead of target `7590260`; check experimental GTX requirements, quaternion/camera/math behavior and formatting with newer GLM |
| GLAD/stb_image | Keep local copies in stage 1. Target blob hashes are recorded; evaluate loader extension/function availability and texture formats before updates. Do not hand-edit generated GLAD |
| EnTT/filewatch | Import target checked-in source and attribution when scenes/reload are introduced. Check GCC headers and Linux watcher implementation |
| YAML/Box2D/ImGuizmo | Start from target gitlinks `25be1f2`/`80e17be`/`218d60b`; inspect their actual source/build scripts and prove both OS builds before choosing alternative versions |
| msdf-atlas-gen | Target `b50e101`; inspect nested msdfgen/freetype pins and compiler/build compatibility; preserve text renderer and font assets |
| shaderc/SPIRV-Cross | Target links Vulkan SDK binaries rather than pinning portable builds. Select/document compatible source/toolchain revisions in stage 3; preserve compilation, reflection and cache semantics on Linux and Windows |
| Mono | Target includes Windows static libraries/headers. Evaluate a Linux native Mono package and Windows runtime/CRT deployment, with documented versions and managed compiler selection; no scripting stub |
| Premake | Keep installed 5.0.0-dev for local builds and existing CI pin `71f2d33946947e9cf704f00c24200381e360f593`. Keep and review the narrow `flags == nil` compatibility shim |

## OpenGL 4.2 adaptation map

The actual desktop driver reports Intel HD 4000, Mesa 26.2.3, accelerated core
OpenGL 4.2 / GLSL 4.20. Target source requests/asserts 4.5 and uses features above
4.2. Lowering the assertion alone cannot provide compatibility.

| Target calls/source | Adaptation preserving the feature |
| --- | --- |
| glCreateBuffers/glNamedBufferData/glNamedBufferSubData | glGenBuffers + bind/data/subdata; restore binding state when implementing named-operation equivalents |
| glCreateVertexArrays | glGenVertexArrays, retain integer attributes, matrix columns and instancing divisors |
| glCreateTextures/glTexture*/glBindTextureUnit | glGenTextures + bind-based storage/upload/parameters and active unit; retain formats, alignment and slot behavior |
| glCreateFramebuffers and glClearTexImage | Bind-based framebuffer setup; clear integer color attachments via glClearBufferiv on the correct FBO/draw-buffer index; preserve readback, MSAA and resize |
| glDebugMessageCallback/control | Check extension and loaded function availability; supply compatible diagnostics rather than requiring unsupported core debug output |
| glShaderBinary/glSpecializeShader (SPIR-V) | Retain shaderc/Cross pipeline and reflection/cache; generate GLSL 4.20 and compile/link on 4.2. Capability-gate SPIR-V loading where actually supported; cache hits must still provide GLSL for fallback |
| Renderer2D GLSL 450 | Audit interfaces, explicit locations, UBO and sampler bindings, integer entity outputs and text math; use 420-compatible stage interfaces, then compile/link on the driver |
| 32 texture slots | Query fragment and combined limits. If required, select a supported batch capacity and flush at that boundary; retain textured batching rather than removing texture features |

Stage gates require actual compile/link and framebuffer readback where applicable.
Hardware/runtime evidence must stay distinct from generation-only Windows checks.
