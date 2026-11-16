# Direction and milestone boundaries

**Ambition:** a well-engineered, production-quality, performant C++ game engine with modern rendering and authoring workflows; eventual broad feature parity with Filament's rendering capabilities is a **north star**, not a compatibility promise or an excuse for speculative abstractions. Match visual/physical correctness and performance through measured tests. Preserve Hazel/TheCherno readability.

## Current prioritized track: Metal 4 backend

- **M1 — PASS, pushed:** Metal 4 device/queue, native presentation, frame/resource lifetime, compute/render/transfer smoke, regression evidence. See `docs/design/metal-m1.md`. Production still OpenGL.
- **M2 — Proposed, not automatically authorized:** shader portability and explicit pass contracts. First bound checkpoint: pinned GLSL → SPIR-V → generated MSL, `MTL4Compiler` pipeline, reflected layout/bindings, orientation/depth, signed integer targets and packaged shader feasibility. Prove representative engine shaders before committing to a universal translation assumption.
- **M3 — Planned:** Renderer2D and Nutella parity, including text/UI, batching, texture/sampler access and display output.
- **M4 — Planned:** Renderer3D, cooked GLB/materials, depth/selection, PBR/IBL, environment prep and frozen visual/numerical parity.
- **M5 — Planned:** full Hazelnut ImGui docking/detached viewports, previews/outlines, GPU-consumer lifetime, native compute dependency proof.
- **M6 — Planned:** full platform/package validation, native metrics and conscious choice of macOS default; OpenGL stays a supported option.

Do not automatically advance to the next milestone, change default backend, merge or ship. Explicit authorization is required. Record PASS only against measured, reproducible acceptance gates. Fixed reference cases must not be adjusted to fit a new renderer.

## Post-parity renderer work — candidate priorities, not authorized tasks

- High-quality indirect AO (GTAO/SSAO) with temporal/noise strategy and correct application to indirect light.
- Multi-bounce GGX energy compensation, particularly rough metals; don't confuse this with the distinct split-sum directional-error investigation.
- Forward+ or clustered culling, point/spot lights, shadow maps and PCSS-style soft shadows.
- Local reflection probes, probe management/blending, SSR with sensible probe fallback.
- Area lights (e.g., LTC), robust emissive behavior, parallax occlusion mapping (normal mapping already exists).
- Bloom, exposure/color-management refinements, optional additional BRDF/material features as justified by real scenes and Filament comparisons.
- Later runtime/mixed-scene/export Stage 5 integration (separate older stage); its execution was paused during IBL research and is not implicitly restarted by Metal work.

The feature order may change with measured blockers, dependency proofs and owner direction. Metal compute supports many of these, but does not inherently make them faster. OpenGL feature backports are separate scope decisions, not mandatory before Metal implementations.

## Quality bar that affects design now

Aim for explicit resource/pass lifetimes, bounded memory, deterministic clean shutdown, actionable errors, runtime/editor parity, portable assets and source-free export. Favor small interfaces with real consumers; allow expansion after the first verified use. Test under native M4 GPU, GL4.1 and the actual Intel HD 4000 path when required. Record baseline and per-feature CPU/GPU costs before claiming wins.

Filament reference: https://google.github.io/filament/Filament.html — a useful comparison target, not a source to copy or a statement of achieved parity.

## Public-facing readiness across milestones

Keep the public `README.md` and linked documentation usable on the actively published branch. Feature and onboarding changes require targeted public guide updates; integrated milestones merit a public-docs/CI/clean-clone checkpoint. Experimental work may be incomplete between checkpoints without forcing rebuilds or inaccurate claims. The release-quality goal is reproducible Hazelnut and Nutella builds and example workflows across **documented supported** platforms, with honest evidence gaps. See `docs/agents/PUBLIC_DOCS.md`.
