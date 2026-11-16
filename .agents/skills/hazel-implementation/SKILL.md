---
name: hazel-implementation
description: Implement a scoped feature, fix, or backend milestone slice in the Hazel C++ game engine while respecting existing architecture, Premake, compiler warnings, and targeted verification. Use for authorized source changes; not for read-only audits.
---

1. Confirm current branch/HEAD/status and authorization boundary. Preserve unrelated work; don't switch branch, commit or advance milestones unless asked.
2. Identify the source owner, shortest call path and nearest test. Read `docs/agents/ARCHITECTURE.md` only if ownership is unclear; read `docs/agents/QUALITY.md` for GPU/lifetime/packaging changes.
3. Describe one small implementation approach when needed; use existing Hazel style and concrete ownership. Avoid unrelated refactors and abstraction scaffolding.
4. Implement with error paths, stable lifetimes, bounds and portability. For GPU/backend work, validate hazards and format/coordinate contracts explicitly.
5. Compile/test incrementally; investigate and fix introduced first-party warnings. Reproduce failures before changing code. Don't rerun unrelated full suites on every iteration.
6. If the slice changes user-visible workflows, commands, examples, packaging or CI, update affected public docs at a stable checkpoint, or explicitly record deferred drift for WIP. Avoid full docs audits after routine edits; use `hazel-public-docs` at integrated milestones.
7. At the requested handoff, run mandated gates, document evidence in the existing record and report files, warnings, public-docs status if relevant, measured results and incomplete checks. Stop at authorized scope.

For C++ interface or warning work use `hazel-cpp-integrity` when useful; for Metal shader translation use `hazel-shader-parity`, for architecture only use `hazel-gpu-review`; for final milestone gates, use `hazel-qualification`.
