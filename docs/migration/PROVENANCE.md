# Source and dependency provenance

Baseline: `b030be7` (initial clean master, four clean submodules). Fixed source: [TheCherno/Hazel 1feb70572fa87fa1c4ba784a2cfeada5b4a500db](https://github.com/TheCherno/Hazel/tree/1feb70572fa87fa1c4ba784a2cfeada5b4a500db), “Fix for cloning issues with msdf-atlas-gen (#617)”, 2023-10-27. Imported actual Git source; upstream licensing is retained in UPSTREAM-LICENSE. Source target remains pinned throughout.

Baseline tracked-entry comparison remains in `evidence/source-comparison.tsv` and `comparison-summary.json`: local90/upstream829 entries;748 upstream-only,9 local-only,77 changed,4 identical. These are baseline snapshots, not a final diff. Reproduce using `scripts/migration/compare-upstream.py UPSTREAM_CHECKOUT`. Per-stage `evidence/*-imports.json` records exact upstream file/blob IDs; `stage4b-font-pins.json` records nested font pins. Routine logs/CI metadata are kept outside tracked source.

| Actual source foundation | Import records | Local adaptations |
| --- | --- | --- |
| Camera/UniformBuffer and integer/matrix vertex attributes | Target files; stage1 checkpoint81b3861 | Bind-based allocation/update, Scope factories, matrix-column/divisor fixes |
| Core, application specification/args/queue, UUID/buffer/filesystem/timer, typed input/events, editor camera/math | stage2-imports | Scope LayerStack, ordered teardown, separate Linux/Windows utilities, release validation, modern fmt |
| Shader/texture/framebuffer APIs and OpenGL source | stage3-imports | Full shaderc/Cross optimizer/reflection/cache, GLSL410 loading, bind-based storage, integer picking/MSAA/resolve, state restoration and errors |
| SceneCamera, components, EnTT/filewatch, YAML | stage4a/4b-imports | Finite minimized projection, current compiler integration, native UTF-8 filesystem paths |
| Font/MSDF and complete Renderer2D/shaders | stage4b/4c-imports | Two atlas workers, ownership/shutdown, driver-constrained batching, UTF-8 decoder, primitive/texture/font flushes |
| Box2D and Project/serializer | stage4d-imports | Exact2.4.0 API, scoped physics ownership, native portable paths, transactional/release validation |
| ScriptEngine/Glue, Hazel-ScriptCore, Windows Mono SDK/assemblies | stage4e-imports | Native Linux SDK, matching CRT, metadata/GC lifetime, transactional preflight reload, OS watcher adapters/main-thread queue |
| Complete Scene/Entity/SceneSerializer/native scripts | stage4f-imports | Ownership/copy/duplicate/removal/physics synchronization, lossless typed YAML, staged deserialization, retained fields after missing script classes |
| Actual Player/Camera scripts and SandboxProject assets | stage7-imports | Portable Premake/deployment, component API regression probe, token-based nested metadata discovery |
| Actual Hazelnut editor/panels/fonts/icons/layout, ImGuizmo, ImGui theme/helper/UI | stage8-imports | Official ImGui API/backends, Scope layers/font, UTF-8 payload/CLI, safe scene/project workflows, all three body types, teardown |

Final source-path audit accounts for target engine/editor/managed sources. `Hazel/Core/Window.cpp` is intentionally replaced by separate OS window factories; `ImGuiBuild.cpp` is replaced by official backend translation units. Preserve local Core/MouseButtonCodes compatibility includes and separate OS inputs. Engine features are not removed for compatibility.

## Deliberate dependency choices

| Dependency | Retained/imported pin and reason |
| --- | --- |
| GLFW | `026a148d7dd78d597de380c4e77ca0869f0ceaab`; retained working Linux/Windows integration |
| GLM | `8d1fd52e5ab5590e2c81768ace50c72bae28f2ed`; retained, explicit experimental-header setting |
| Official ImGui docking | `e25e4526cd41cd6536194de098540d54244f54e9`; retained official GLFW/OpenGL3 and stdlib TUs, docking/viewport callback/context behavior |
| spdlog/fmt | `57cb5fb7a8ff30079751728234623230535a5c92`; retained modern bundled fmt, portable logging adapters |
| YAML | `25be1f208665b9732ba40aa7b05ec2da97104192`; exact target, root-owned cstdint include compatibility |
| Box2D | `80e17bef53f217fa8b1696718a047e29e9f25def`; exact target2.4.0 API |
| ImGuizmo | `218d60bde7d22061ac525d0d71e05360b4dcf978`; exact target, official ImGui compatibility, root-owned settings |
| Font generators | msdf-atlas-gen `b50e101d24b1f6009841ce3a386e1bc9365dc66a`; nested msdfgen/FreeType/dlg hashes in stage4b-font-pins.json; exact source, two atlas workers |
| shaderc/glslang/Headers/Tools/Cross | Complete hashes/URLs in scripts/dependencies/shader-tools.json; shaderc2023.6 + Cross VulkanSDK1.3.268-compatible source; same validated pins, no Windows Vulkan SDK binary requirement |
| Mono | Exact upstream Windows6.12 SDK/assemblies and licenses. Linux native mono-devel/mono, or isolated6.12.0.206-1 SDK with checksum/URL in scripts/dependencies/mono.json |
| GLAD/stb_image | Retained baseline checked-in source, generated loader kept intact; capability checks use actually loaded functions |
| Premake | Local current development version; CI `71f2d33946947e9cf704f00c24200381e360f593`; narrow legacy-flags shim preserved |

Shader dependencies build exclusively through root-owned shader-workspace.lua and build-shader-tools.py. shader-sources.json contains actual complete dependency source sets (including HLSL/OGLCompiler, Tools core/validation/optimizer, Cross core/GLSL). generate-shader-inputs.py runs the pinned grammar/registry/version generators into ignored output, uses the actual glslang template/version extraction, and publishes only changed files. Windows selects its actual OS source and MDd/MD. The source pins stay clean; the compiler compatibility header is outside vendor source. Generated build-version timestamps use the shaderc pin's SOURCE_DATE_EPOCH. Installed manifests record pins/source-manifest hash/build system/CRT and licenses.

## Local capability/settings extension

Inspected target Renderer.h/.cpp, RendererAPI, TextureSpecification, FramebufferSpecification and Renderer2D's `TODO: RenderCaps`. Refreshed public master/text still equal the target. Available asset-manager `cca493f4abbbfd759fc624b3cc58d338cd316b5b`, projects `e4b0493999206bd2c3ff9d30fa333bcf81f313c8` and scripting `f8f8e3089b47c8371dc61448c3d47d43cad31079` contain no equivalent implementation. RendererCapabilities/RendererSettings are **local extensions**, following those specification structs and Renderer::Init -> RenderCommand -> RendererAPI flow. They are not attributed to Cherno or unavailable Hazel-dev source.

OpenGLContext refreshes one backend detection record after GLAD; standalone resources initialize lazily. Common APIs expose descriptive device/resource limits and selected settings. API versions/functions stay in Platform/OpenGL. Renderer2D and shader capacity use one selected texture count; texture/framebuffer validation shares detected limits. Default paths preserve native HD4000 functionality: bind-based4.1 resources, full shaderc/Cross -> GLSL410; native specialization requires loaded core4.6 functions, debug output loaded core4.3 functions. Requested settings can choose GLSL loading and smaller batches without changing scene/project formats. Current wider lines clamp to actual smooth-line limits. Private cache containers bind source/stage/contract identity and payload checksum; SPIRV-Tools validates the target environment before reflection/driver use.

Windows editor cleanup investigation found Mono's process-lifetime debugger logfile lock. Actual [Mono6.12.0.206 debugger-agent.c](https://github.com/mono/mono/blob/mono-6.12.0.206/mono/mini/debugger-agent.c) opens that file without closing it in debugger cleanup. Use its documented stdout sink, preserving debugging, loglevel3 and soft breakpoints; strict cleanup/runtime checks then passed. Vendor source was not patched.

Final native cache probe also exposed integer0 on an uncleared R32I target with
blending enabled, despite correct draw/output locations and masks; disabling
blending yielded73. OpenGLFramebuffer now explicitly disables indexed blending
for integer attachments while bound and restores caller state on unbind/release/
resize. Color blending is preserved. This addresses a current Linux portability
obstacle, rather than adding speculative Mac code; the regression tests uncleared
overwritten color/entity pixels and restored state. Final Debug no-clear color/entity readback and blend-state restoration passed on native4.2, forced4.1 and software4.6; Release/clean-checkout proof remains pending.

Final source audit also found dormant target Sandbox/src/ExampleLayer.h/.cpp,
now imported directly (stage9-example-imports.json). Its required Texture.glsl
is missing at the target checkpoint; use the matching actual basic-texture shader
from public Hazel commit7d120fb4250ecdebdcdc93137bbec7413a9bb7e2 as
Sandbox/assets/shaders/ExampleTexture.glsl, leaving retained batch assets intact.
Official ImGui include and scoped application selection adapt the example;
Sandbox --example-layer makes it runnable for regression checks. The target pin
is unchanged. Other absent target entries are replaced Windows/Vulkan/bundled
Premake build tooling, intentional OS/backend TU placement, or repository-specific
branding/contribution templates, not engine/editor/managed functionality.
