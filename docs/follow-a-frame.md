# Follow a frame

Hazel keeps one scene pipeline for Hazelnut and Nutella. Start here rather than
with the implementation milestone records.

1. **Host and view.** [EditorLayer::OnUpdate](../Hazelnut/src/EditorLayer.cpp)
   updates Edit/Play and owns viewport composite/output targets and its private
   navigation camera. [Nutella's RuntimeLayer](../Nutella/src/NutellaApp.cpp)
   updates [RuntimeSession](../Hazel/src/Hazel/Scene/RuntimeSession.cpp), which owns
   its render view. Both supply actual framebuffer dimensions.
2. **Scene and collection.** [Scene::DrawView](../Hazel/src/Hazel/Scene/Scene.cpp)
   constructs the camera input and calls
   [SceneRenderer::Collect](../Hazel/src/Hazel/Renderer/Scene/SceneCollection.cpp).
   Collection resolves accepted CPU assets, world/normal transforms, stable slots,
   directional light and environment into
   [SceneSubmission](../Hazel/src/Hazel/Renderer/Scene/SceneSubmission.h).
   No GL names or importer types enter this submission.
3. **Pass order.** [SceneRenderer::Render](../Hazel/src/Hazel/Renderer/Scene/SceneRenderer.cpp)
   owns target selection, clear/depth/ID state and ordering: background/meshes,
   world 2D, world exposure/Neutral presentation, then screen UI. The optional
   FP32 world framebuffer belongs to SceneRenderView. Display-referred content
   uses the lightweight composite directly.
4. **Draw owners.** [Renderer3D::Render](../Hazel/src/Hazel/Renderer/Drawing/Renderer3D.cpp)
   owns the forward mesh/material/light bindings, culling state, indexed ranges
   and environment background draw, using
   [ForwardPBR](../Hazel/Resources/shaders/ForwardPBR.glsl). Background goes first without depth or ID
   writes. [Renderer2D](../Hazel/src/Hazel/Renderer/Drawing/Renderer2D.cpp) retains
   its established sprite/circle/text batches. Scene's world-2D callback uses it;
   [SceneUI](../Hazel/src/Hazel/Scene/SceneUI.cpp) uses the ordered runtime UI path.
5. **Resources and backend.** [ProjectAssets](../Hazel/src/Hazel/Assets/ProjectAssets.h)
   owns accepted CPU generations and bounded renderer realizations. Typed
   [mesh geometry](../Hazel/src/Hazel/Renderer/Drawing/MeshGeometry.h),
   [environment resources](../Hazel/src/Hazel/Renderer/Resources/EnvironmentResources.h)
   and shared [resources](../Hazel/src/Hazel/Renderer/Resources) use the existing
   factories. [RenderCommand](../Hazel/src/Hazel/Renderer/RenderCommand.h) delegates
   state/draws to RendererAPI. Allocation, GL bindings, cube face/mip upload and
   attachment copies live under [Platform/OpenGL](../Hazel/src/Platform/OpenGL).
6. **Output.** Hosts call Framebuffer::EncodeOutput once. Hazelnut displays the
   encoded RGBA8 viewport through ImGui; Nutella encodes to the window. UI has
   already composited in display-linear space independently of world exposure.

Cameras, Drawing, Scene, Resources and Text are shallow reading groups under
Renderer. Renderer, RenderCommand, RendererAPI, capabilities/policy and Color
remain at its root. Material documents and lights are semantic scene/asset inputs,
not a forward binding layout. Other first-party areas were inspected; the adjacent
offline environment producer was grouped under
[HazelImport/Environment](../tools/HazelImport/Environment). Broad editor/asset
moves are deferred.

Environment ownership has three boundaries: authored `.henv` intent; immutable
accepted CPU `.hlight` data; and runtime EnvironmentResources. The native
EnvironmentBake producer owns decoding/filtering/publication, never the player.
Its [radiance sampler](../tools/HazelImport/Environment/RadianceSampler.cpp) owns
solid-angle proposals; the renderer consumes only accepted data. See the
[quality contract](design/ibl-quality-pass.md) for recipes and measured limits.

**Shutdown:** Layers stop Scene/RuntimeSession and release per-view targets while
Mono is alive. Application stops ScriptEngine, releases project realizations, then
Renderer shuts down Renderer3D, Renderer2D batches/font resources and RenderCommand
before destroying the GLFW context/window. See
[Application::~Application](../Hazel/src/Hazel/Core/Application.cpp) and
[Renderer::Shutdown](../Hazel/src/Hazel/Renderer/Renderer.cpp). History contains no
GPU resources. Frame scopes restore state, including exceptional paths.
