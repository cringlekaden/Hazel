# Hazel — repository guidance for Codex

Build a fast, readable, production-quality C++ engine descended from TheCherno's public Hazel. Filament-level rendering capability is a long-term **goal**, not current functionality or license to invent frameworks. Match the adjacent Hazel code, naming and ownership style. Prefer concrete, source-discoverable responsibilities, small APIs, RAII, and measured optimization over generic compatibility layers or duplicated paths.

- **Repo:** use the existing checkout, Premake, and tracked scripts; never create a second checkout, CMake replacement, or edit generated IDE files as authoritative source. Keep Hazel/Hazelnut/Nutella's single scene submission path, existing Renderer2D, OpenGL 4.1, and Windows/Linux/macOS compatibility. Metal 4 is an additional macOS backend, not permission to regress OpenGL.
- **Start:** inspect branch, HEAD, worktree status and the narrow source/test path for this task. Preserve unrelated changes. Current source + applicable `docs/design/` evidence outrank agent summaries. Read only needed docs/skills; do **not** preload all of them.
- **Scope:** do only the authorized slice. No unrequested stage advancement, changing backend defaults, resetting reference data, mass formatting, dependency upgrades, commit/push, merge or release. A milestone passing doesn't authorize the next one.
- **Quality:** fix **new first-party compiler warnings** at the cause, not via blanket suppression. Check boundaries, headers/includes, ownership, CPU/GPU races, failure cleanup, Debug/Release and relevant platform differences. Preserve stable tests and existing semantics. Don't silently hide failures or feature gaps.
- **Verification:** iterate with focused builds/tests; run complete relevant gates at approved checkpoints. Label PASS/FAIL/NOT RUN/BLOCKED accurately and separate simulated/software GL from native GPU, CI from local execution, and tests from visual observation. Never fabricate performance or validation results. After repeated evidence-free debugging or an environmental blocker, report instead of looping.
- **Public readiness:** update affected README-linked user docs for stable changes to commands, examples, features, platforms or CI. At integrated milestones/checkpoints run the public-docs audit; during experiments record deferred documentation drift without forcing full builds. See `docs/agents/PUBLIC_DOCS.md`.
- **Report briefly:** changed files, warnings, relevant tests/evidence, docs impact, unresolved gates and status. Quote a new SHA only if actually committed; follow explicit authorization for writing/pushing.

## Load only when relevant

- `docs/agents/PROJECT.md` history/status; `ARCHITECTURE.md` owners; `ROADMAP.md` plans.
- `docs/agents/QUALITY.md` tests/warnings/performance; `STYLE.md` C++ design; `USAGE.md` lean workflow.
- `docs/agents/PUBLIC_DOCS.md` public README/examples/CI; `AGENT_MAINTENANCE.md` maintaining these instructions.
- `.agents/skills/`: `hazel-implementation`, `hazel-qualification`, `hazel-gpu-review`, `hazel-shader-parity`, `hazel-cpp-integrity`, `hazel-code-review`, `hazel-public-docs`. Select only skills that fit the requested task.
