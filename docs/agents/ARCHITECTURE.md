# Architecture navigation and ownership

This is a map, not a substitute for checking current source. Search narrowly with `rg -n` and follow call sites instead of reading entire directories.

| Responsibility | Current places to inspect |
|---|---|
| App startup/lifecycle, backend selection | `Hazel/src/Hazel/Core/Application.cpp`, `Hazel/src/Hazel/Renderer/RendererAPI.*`, `RenderCommand.*`, `Renderer.*` |
| Window/context, macOS/native presentation | `Hazel/src/Platform/macOS/`, `Hazel/src/Hazel/Renderer/Resources/GraphicsContext.*` |
| Metal 4 native implementation | `Hazel/src/Platform/macOS/Metal/`, `docs/design/metal-m1.md` |
| Scene to GPU boundary | `Hazel/src/Hazel/Scene/Scene.cpp` (`DrawView`), `Hazel/src/Hazel/Renderer/Scene/SceneSubmission.h`, `SceneRenderer.*` |
| Drawing | `Hazel/src/Hazel/Renderer/Drawing/Renderer2D.*`, `Renderer3D.*` |
| Resources | `Hazel/src/Hazel/Renderer/Resources/` (buffers, textures, shaders, framebuffers, timers) |
| OpenGL backend | `Hazel/src/Platform/OpenGL/` |
| 3D PBR and environment prep | `Hazel/Resources/shaders/ForwardPBR.glsl`, environment bake / asset code under renderer and assets |
| Editor and player hosts | `Hazelnut/src/EditorLayer.cpp`, `Nutella/src/NutellaApp.cpp` |
| GPU asset realizations | `Hazel/src/Hazel/Assets/ProjectMeshes.cpp`, `ProjectEnvironments.cpp` |
| Builds/toolchains | `premake5.lua`, `scripts/dependencies/`, `scripts/internal/toolchain.json` |
| Regression and visual fixtures | `tests/migration/`, `docs/design/` |

## Boundary rules

1. Scene/camera/material/runtime authoring and visibility collection should not know Metal objects, GL handles, GPU queues or drawable implementation details.
2. `SceneRenderer` owns shared render ordering: world HDR/radiance + ID/depth, tone mapping/display, screen UI, selection/overlay considerations. Hosts own their target views. Native backends implement commands/resources, not a second scene traversal.
3. Keep raster, compute and transfer passes explicit for Metal 4; don't rush to a generic render graph. Avoid emulating GL state queries in Metal. Preserve platform-specific behavior at well-defined boundaries.
4. Buffer updates consumed by different draws must have stable GPU-visible versions, including **within the same frame**; reuse only after completion. Track retained/retiring resources across frames and editor detached-window consumers.
5. Existing GLSL is authoritative for raster math. M2-A qualifies translation/native pipelines; M2-B1 production shader ownership lives in `Platform/macOS/Metal/MetalShader*`, with dated gates in `docs/design/metal-m2b1.md`. Production drawing/scene parity remains separately scoped. Don't invent a second unreviewed PBR lighting model.
6. Preserve signed picking IDs and AA-off depth/ID coverage; keep coordinate, origin, depth convention, sRGB/HDR encoding, object transforms and mirrored winding behavior explicit.
7. Reuse current project/asset-generation contracts. No device pointers in scene files/material formats. Resource lifetime must survive reload, eviction, resize, swap and export.
8. If a source path has moved, discover the new owner; do not create a duplicate just to make this map accurate.

For a rendering change, additionally use the `hazel-gpu-review` skill and relevant `docs/design/` record. Only open this map on ownership decisions or initial orientation.
