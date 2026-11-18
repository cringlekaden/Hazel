# Direction and milestone boundaries

**Ambition:** a well-engineered, production-quality, performant C++ game engine with modern rendering and authoring workflows; eventual broad feature parity with Filament's rendering capabilities is a **north star**, not a compatibility promise or an excuse for speculative abstractions. Match visual/physical correctness and performance through measured tests. Preserve Hazel/TheCherno readability.

## Current prioritized track: Metal 4 backend

- **M1 — PASS, pushed:** Metal 4 device/queue, native presentation, frame/resource lifetime, compute/render/transfer smoke, regression evidence. See `docs/design/metal-m1.md`. Production still OpenGL.
- **M2 — PASS (local Apple M4):** shader portability and explicit pass contracts progressed through M2-A/B1–B4/C. Native generated shaders, resource/pass contracts, source-free shader relocation and integrated quad presentation are recorded in `docs/design/metal-m2c.md` and its linked evidence. This supersedes the old proposal summary; it does not certify Windows/Linux or full 3D parity.
- **M3 — PASS (local Apple M4):** Shared deterministic Renderer2D lines, independent/frozen/cross-backend qualification and native Debug/Release 1×/2× player lifecycle/input/extended-session evidence are in `docs/design/metal-m3.md`. Windows/Linux/Intel execution is NOT RUN. OpenGL remains default.
- **M4 — PASS (local Apple M4):** production Renderer3D, cooked GLB/materials, depth/selection and prepared PBR/IBL parity; native Debug/Release 1×/2× lifecycle, actual mixed player input, extended visible stability and matched runtime timings are recorded in `docs/design/metal-m4.md`. Windows/Linux/physical Intel/new CI remain NOT RUN. M5 evidence is tracked separately; no default change, merge or release.
- **M5 — PASS (Apple M4 local qualification):** native Debug/Release Hazelnut workflows, single-display detached-window lifecycle at 1×/2×, unified three-slot retirement, production compute dependencies, GPU/CPU timing and regression evidence. See `docs/design/metal-m5.md`. Windows/Linux/Intel/new CI remain NOT RUN; general Metal export remains unqualified. OpenGL stays default. M6 qualification is tracked separately; no merge or release.
- **M6 — INCOMPLETE, qualification in progress:** production/package/source onboarding, native metrics, branch CI and public documentation are tracked in `docs/design/metal-m6.md` and `metal-m6-results.json`. OpenGL remains default; no merge/release or next stage is authorized.

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
