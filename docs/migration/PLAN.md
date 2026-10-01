# Staged migration plan

Target complete public functionality and architecture of
`1feb70572fa87fa1c4ba784a2cfeada5b4a500db`, with deliberate local improvements,
native Linux and Windows support, and a working OpenGL 4.2 path. Stages 2–9 are authorized for this resumed session; continue in order through
buildable checkpoints. Use actual target source, recording imports and adaptations.

Each stage ends with a local Git checkpoint on `migration/upstream-1feb705`.
Retain Sandbox as a runnable integration target throughout. Branch pushes are
authorized for CI after local checkpoints; never merge into master.
Use at most two concurrent compiler jobs, one build invocation at a time.

| Stage | Implementation boundary | Exit gate |
| --- | --- | --- |
| 0: baseline and preparation | Working-tree/pin inventory, baseline build/runtime, tree comparison, preservation rules, this plan | Baseline Debug and Release compile/link; runtime failures explained; preparation committed before implementation |
| 1: renderer foundation | Import Camera and UniformBuffer factory/backend; bind-based 4.2 UBO operations, target integer/matrix VAO behavior and matrix component counts; no pin changes | Linux Debug/Release; focused real-context UBO readback, integer/matrix attribute checks and GL error checks; baseline Sandbox startup; VS generation/source/CRT inspection; record unrun Windows checks |
| 2: core/platform architecture | Target Base/Assert/platform detection, application specification/arguments, queue, timers, filesystem/buffers/UUID, typed input/events, editor camera/math; preserve Scope LayerStack. Separate Linux/Windows Time/FileDialogs utilities and GraphicsContext factory | Ownership/layer partition and shutdown tests, application args/queue/input smoke, Linux builds/runtime and Windows builds via retained workflow |
| 3: shader/texture/framebuffer architecture | Evaluate shaderc/Cross versions and portable builds; preserve SPIR-V reflection/cache with a tested GLSL 410 fallback where viable (retain 4.2 functionality); texture specifications, binding-based GL operations, framebuffer integer picking/MSAA/readback/clearing; capability-gated diagnostics | Cold/warm shader caches, compile/link on real 4.2, texture format/alignment cases, integer picking/clear/resize/MSAA tests; both OS build checks |
| 4: scene/ECS/serialization | Import exact EnTT/filewatch/YAML source/pins after inspection; scenes/entities/components, SceneCamera, serialization/copy/duplicate and native scripts | YAML save/load/copy round trips, entity lifetimes, editor/runtime camera and offscreen scene rendering; Sandbox and both OS builds |
| 5: complete 2D rendering/text | Target transform/entity-aware quads, circles, lines, rectangles, sprites, stats; MSDF atlas/font dependency and assets; all four target shaders adapted through the tested 410/420 backend path | Render and read back color/entity attachments; all primitives/text/rotation/tiling/batch overflow and hardware texture-limit behavior; font load failure diagnostics; both OS builds |
| 6: physics | Exact target Box2D API/pin and scene runtime/simulation physics integration | Start/stop/restart, rigid bodies and both collider kinds, collision and transform updates; both OS builds |
| 7: managed scripting/projects | Mono native Linux/Windows dependency/deployment, Hazel-ScriptCore and example scripts, ScriptEngine/Glue, component/input internal calls, filewatch and assembly reload, Project/serializer | Compile/load managed script, field persistence, update/collision-related scene behavior as exposed upstream, reload, project open/save and runtime restart; both OS builds |
| 8: Hazelnut/editor | Exact target editor/panels/assets, ImGuizmo pin evaluation, content browser, hierarchy/properties, docked viewport and picking, gizmos, play/simulate/stop, scene/project workflows and shortcuts | Launch on both OS, dock/viewports/resize/input capture, create/edit/save/reopen project+scene, drag/drop textures, selection/gizmos, run/simulate scripts/physics |
| 9: parity and hardening | Reconcile all target source/features/assets/dependencies, audit adaptations, expand CI/reproducible setup and user docs | Feature-by-feature parity inventory, full Linux/Windows Debug/Release, real 4.2 rendering/editor checks, clean vendor status, final local checkpoint |

Dependencies may require smaller buildable sub-checkpoints within these stages.
For example, isolate shader tools before framebuffer integration and font tools
before replacing Renderer2D. Any reorder must be recorded in PROGRESS.md; do not
disable upstream features to make an integration compile.

## Per-stage procedure

1. Inspect status/submodule status and PROGRESS.md; preserve newly arrived user changes.
2. Read target files and their consumers. Record original blob IDs and exact
   imported paths; retain attribution and license.
3. Apply the smallest coherent API/backend/call-site change. Keep vendor sources
   clean; resolve Premake changes in project-owned scripts where possible.
4. Generate gmake with the existing shim, build Debug then Release with `-j2`.
   Generated artifacts can be overwritten by the generator; never use recursive
   vendor Makefile deletion. For a full rebuild use `make -B`, not broad deletion.
5. Run relevant runtime/readback checks in an isolated temporary working directory.
   Investigate errors before advancing. An unavailable host/tool is an unverified
   gate, never a pass. Record compiler/platform and exact commands/results.
6. Generate VS2022 for Windows and inspect source exclusion, CRT and UTF-8 when
   those change. Generation on Linux does not count as an MSVC build or Windows
   runtime. Retain CI coverage and arrange Windows execution before final parity.
7. Update PROGRESS.md and provenance; commit only task-owned files, then report
   checkpoint hashes and remaining gates. Push only the migration branch for CI;
   never merge into master.

Stage 1 can be a Linux-verified checkpoint with Windows execution explicitly
pending. Future cross-platform build-system/dependency integration must not be
described as fully verified without Windows results.
