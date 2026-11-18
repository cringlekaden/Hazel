# Project context — read only when history or current direction matters

**Authority:** current source + pushed branch + canonical `docs/design/` records outrank this summary. Last contextual update: 2026-11-15, based on pushed `9030dd6` and owner/Codex reports.

## Identity and origins

Hazel started from TheCherno's public C++ Hazel, retaining the spirit of a small, comprehensible engine rather than adopting a commercial engine's complexity. This fork has evolved its own cross-platform editor/runtime, 2D capabilities and a newer 3D foundation. It is **not** TheCherno's commercial Hazel engine. Main subsystems: `Hazel/` engine library, `Hazelnut/` editor, `Nutella/` player/export runtime, `tests/migration/` regression work, `examples/`, `docs/` and `scripts/`.

The 2D/editor base includes scene authoring, C# scripting, physics, asset/export support and runtime UI. Keep this working while 3D and Metal are developed.

## 3D implementation status (feature/3d-foundations)

- Separate `Renderer2D` and `Renderer3D`; one scene submission and shared editor/player render path.
- Static GLB meshes and cooked assets, inspection camera, depth-aware 3D selection, perspective/orthographic camera ownership, direct GGX metallic/roughness PBR and prepared HDR IBL.
- Implemented PBR concepts include GGX, correlated Smith, Schlick Fresnel, normal maps, diffuse irradiance and split-sum specular IBL. Verify details in current shader sources; don't claim feature completeness.
- `docs/design/ibl-view-dependent-investigation.md` and `docs/design/ibl-ab-compression-experiment.md`: measured directional specular approximation error remains; FP64 full-BRDF reference is too slow, tested SH ranks 8/16/32 failed accuracy. This is a **documented, accepted deferred limitation**, not a passed accuracy gate.
- Earlier environment reconstruction and AA-off geometric-normal/silhouette fixes are accepted work; preserve them and their frozen cases. Separate Hermes showcase work may not be on this branch—verify before assuming it landed.

## Native Metal 4 status

- `docs/design/metal-m1.md` and `docs/design/metal-m1-results.json` are the canonical records.
- Pushed M1 commit `9030dd62b0e486a369d39de35fb24d43389b022d` reports native Metal 4 device, command submission, explicit lifetime, rendering/compute/blit smoke, presentation and GPU qualification PASS in Debug/Release at 1×/2×, with 18/18 selected macOS OpenGL regressions for each configuration. The owner reported these execution results; do not assume independent reproduction.
- M1 includes no production Metal 2D/3D shader rendering. **Hazelnut and Nutella still default to OpenGL.** Hardware qualification not executed on Windows/Linux/physical HD 4000 for that M1 report.
- M2 not yet authorized as of this context update. Proposed first M2 slice: qualify existing GLSL → SPIR-V → MSL with pinned tools, reflected bindings/layouts, depth/orientation, integer target, and a minimal `MTL4Compiler`/Metal pipeline proof.

## Local contracts

- Canonical working directory: `/Volumes/External/kadencringle/Projects/Hazel`, one checkout, Premake. Main dev host: Apple M4 Mac mini with 16 GB unified memory, native arm64, repo toolchain macOS deployment floor 27.0.
- Compatibility targets: macOS OpenGL 4.1, Linux/Windows OpenGL and real Intel HD 4000 GL4.2 on the CachyOS Dell (“Grandpa”). Do not substitute hosted/software GL for actual hardware evidence.
- Other reference code: MetalCup at `~/Projects/MetalCupWorkspace` may be inspected read-only when relevant; its renderer design, SSAO, bloom and root motion are not automatically appropriate for Hazel.

## Sources to read next (only as relevant)

`README.md`, `docs/design/metal-m1.md`, `docs/design/macos-port.md`, `docs/design/ibl-view-dependent-investigation.md`, `docs/design/ibl-ab-compression-experiment.md`, and targeted `tests/migration/` fixtures. Do **not** reread the full project history for routine changes.


## Current follow-up to M6

The default-backend milestone starts at `93d39bd6d71b090a3a931f1a457a9df27347c91c`.
See `docs/design/metal-default-qualification.md` for current source/evidence, rather
than treating the M1 snapshot above as current integration status. Backend
selection/settings/capability and measured cleanup work are implemented, but native
presentation/package continuation requires an unlocked GUI session. OpenGL remains
`DefaultRendererBackend`; explicit Automatic/Metal/OpenGL requests are supported.
M6 candidate CI has hosted macOS PASS, Windows exact unlit picking/depth FAIL and
Linux exact unlit picking/depth FAIL. No automatic-default qualification, merge, release or XeGTAO follows.
