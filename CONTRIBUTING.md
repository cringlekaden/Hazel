# Contributing to Hazel

Start with [getting started](docs/getting-started.md), the [documentation index](docs/README.md) and [development/testing](docs/development.md). Active 2D/3D development uses `feature/3d-foundations` in the permanent checkout `/Volumes/External/kadencringle/Projects/Hazel`. Open `Hazel.xcworkspace` there. Small intentional content changes should be committed separately before branch switches; do not create another long-lived worktree to preserve them. Historical worktree paths in qualification records describe their dated runs. Master integration follows final qualification and acceptance.

## Report a useful issue

Use [GitHub Issues](https://github.com/cringlekaden/Hazel/issues) for reproducible bugs or documentation corrections. Include:

- Commit SHA or package `build.json`, application, build configuration and exact command.
- OS/version, GPU/driver/OpenGL version, display path (X11/Xwayland on Linux) and relevant DPI/controller details.
- Small reproduction, expected/actual result, and whether it occurs in Edit, Play or a relocated package.
- Console/compiler log excerpt and a minimal licensed project when needed. Remove tokens, usernames/private paths and personal saves before sharing.

Distinguish automated/software checks from physical hardware acceptance. Preserve the original project and player data when reproducing a failure. There is no promised response time or commercial support service for this fork.

## Propose a change

Keep the change scoped and explain its behavior and meaningful verification. Use Premake and the public Python CLI, preserve Linux/Windows implementations, OpenGL 4.1, Windows UTF-8/dynamic CRT and existing ownership conventions. Keep pinned dependencies and vendor trees unchanged unless the task explicitly requires a reviewed dependency change.

Update current guides alongside code/tool/workflow changes. Put dated investigation evidence in the historical records, rather than adding checkpoint diaries to the landing page. Run focused existing checks first; complete relevant milestone gates once before submission. Do not weaken assertions or substitute screenshots for correctness tests.

Submit a pull request with the problem, resulting behavior, tests run and any outstanding platform/manual checks. CI runs on PRs; topic-branch maintainers can dispatch the canonical workflow explicitly. Do not clean another developer's ignored files, rewrite example drafts, publish a release or change repository protection settings as incidental cleanup.

Retain [upstream attribution and third-party notices](docs/platforms-and-dependencies.md#licensing-and-provenance). Screenshots must come from the implemented application and use disposable projects/profiles; capture notes live in [docs/images](docs/images/README.md).

## Camera and viewport ownership

Projection math lives in `Renderer/Cameras/PerspectiveProjection.h` and
`OrthographicProjection.h`; both authored `Scene/SceneCamera` and transient
`NavigationCamera` use it. SceneCamera owns serialized projection settings, while
entity world transforms supply runtime pose. `PerspectiveCameraController` owns
reusable orbit/pan/dolly/fly pose math with supplied point deltas and seconds.
`PlanarCameraController` owns the simple 2D pan/zoom policy used by navigation.
The unused input-polling OrthographicCameraController was removed.
`NavigationCamera` combines transient planar/perspective view poses; it has no
ImGui, GLFW or input bindings.

Change editor gesture ownership in `Hazelnut/src/Camera/ViewportNavigation`,
not in projection math or SceneRenderer. The editor owns workspace persistence,
focus/widget/gizmo arbitration and gesture cancellation. Runtime inspection is
explicit and transient; normal game cameras remain scene/script owned. Keep
navigation separate from document history and preserve both workspace poses.
Runtime view selection/lifecycle lives in `Scene::OnUpdateRuntime` and
`Scene/RuntimeSession`; opt-in movement routing is in
`Scene/RuntimeCameraInspection.cpp`, with host UI/launch choices in Hazelnut and
Nutella. Game-specific authored camera motion remains in game scripts.

## Use MetalCup as a reference

The local `~/Projects/MetalCupWorkspace` is a useful **read-only** reference for
features and past experiments. Evaluate each implementation on its own merits;
do not transplant its Metal passes or camera architecture into Hazel. Preserve
Hazel's improved high-roughness IBL. MetalCup's root motion and SSAO have known
quality problems; its bloom is a working comparison point. When those features
are scoped, define a better result, compare approaches, and verify the chosen
implementation in motion. This guidance does not authorize those features,
Metal work, or changes to the MetalCup checkout.
