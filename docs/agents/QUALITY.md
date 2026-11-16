# Quality and verification — load for risky changes and milestone gates

## Compiler and code hygiene

- For changed first-party code, investigate and fix **new warnings** in supported configurations; do not handwave warnings away or silence them with broad flags. Preserve clean Debug and Release builds and report warning differences from the baseline. Don't turn on global `-Werror` without an approved policy or apply blanket diagnostics suppressions to hide defects.
- Distinguish introduced first-party warnings, pre-existing first-party warnings and third-party/vendor warnings. Remedy the root cause of warnings you touch; unrelated cleanup is a separate, scoped task.
- Check errors, types, limits, integer narrowing/overflow, ownership, RAII, shutdown paths, move/copy behavior, temporary lifetimes, thread/GPU synchronization, release-only behavior and deterministic failure recovery.
- Match surrounding formatting and code conventions rather than mass-reformatting whole files. Preserve readable names and ownership discoverability.

## Test ladder — optimize iteration, never final acceptance

1. **During edit:** focused compile and nearest deterministic unit/smoke, ideally Debug then Release for critical changes.
2. **At local integration:** relevant renderer tests, frozen images/numerical gates, GL paths and player/editor parity affected by the change.
3. **At an authorized milestone:** full specified native 1×/2×, Debug/Release, OpenGL/Metal, portability/build, export/relocation, long-session, warnings and GPU validation gates. Report unavailable hardware honestly.

A green selective subset is not a fully qualified milestone. Maintain precise categories: compilation, software renderer, native GPU validation, numerical reference, visual observation, package relocation and actual Intel HD4000. Host-screen lock / zero `presentedTime` is NOT successful presentation. Avoid full suite reruns on unchanged code during ordinary iteration.

## Rendering invariants

- Frozen IBL cases remain frozen. The split-sum specular directional approximation is accepted as a deferred limitation, **not** fixed. Protect prior roughness-1 cubemap and AA-off silhouette/geometric-normal fixes.
- Measure color-space, sRGB formats, depth/clip conventions, winding/handedness, normal transforms, radiance HDR output, integer selection ID/depth parity, alpha discard, 2D/UI overlay and editor/player consistency.
- On Metal 4, explicitly validate resource residency/visibility, in-flight lifetime, per-draw update versions, command submission errors, drawable availability, buffer/texture formats, async readback and detached viewport consumers. A zero validation-error count applies only to exercised code.
- When comparing OpenGL and Metal, use identical physical resolution, camera/assets/settings; exact IDs in stable interiors, bounded tolerance at edges, explicit numeric tolerances for color/depth. No universal pixel-identity requirement, no blanket image tolerance masking bugs.

## Performance and shipping

- Optimize **measured hot spots** (CPU/GPU frame and pass time, allocation, memory traffic, shader/pipeline counts, cache behavior), not guesses. Distinguish GPU time, CPU encoding time, frame pacing and presentation waits; provide warmup, sample count, median/p95 and settings when quoting improvements.
- Resource budgets include pending-retirement/in-flight objects; do not hide usage behind cache eviction. Confirm no frame-growth/leaks, reload and shutdown correctness.
- Distribution/export: validate shader artifact packaging, source-free relocated runs, Unicode/space paths, no dependence on developer checkout/toolchains. Preserve Premake and Windows/Linux availability. New Metal-only functions must report unsupported capability cleanly on OpenGL rather than silently substitute behavior.

Check task-specific gate documents first; don't invent success criteria after seeing test output. End with tested versus untested gates and PASS/INCOMPLETE.

## Public documentation and CI gates

When source changes affect end-user features, onboarding, examples, toolchain/CLI, supported platforms or CI, update the affected public guide or explicitly defer during an unfinished experiment. At an integrated milestone/release checkpoint, follow `docs/agents/PUBLIC_DOCS.md` and `hazel-public-docs` to check README link chains, branch-aware clean-clone paths, actual Hazelnut/Nutella/example commands, screenshot accuracy and CI badge/workflow status. Match claims to the verified SHA and platform; report missing checks without falsifying a green status. A docs checkpoint does not require full test loops on unchanged code.
