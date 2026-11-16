---
name: hazel-qualification
description: Qualify a Hazel implementation milestone or regression fix with reproducible Debug/Release, Metal/OpenGL, GPU lifetime, numerical, editor/player, portability, and packaging evidence; report PASS versus INCOMPLETE honestly.
---

1. Identify the **predeclared** acceptance gates, exact commit/tree, platform and baseline. See the milestone's `docs/design/` record and `docs/agents/QUALITY.md`; never invent gates after seeing results.
2. Run narrow sanity checks first, then only the full mandated suites. Distinguish command submissions, GPU-completed frames and positively presented frames; exclude missing/zero presentation timestamps.
3. Track build warnings (new first-party versus existing/vendor), resource leaks/in-flight retirement, unsupported features, numerical reference cases, rendering/parity, resize/failure recovery, export relocation and target/hardware availability.
4. Capture exact commands, configurations, sample counts and summaries; retain raw traces/logs in the existing evidence location, not in the chat response. Avoid repeatedly running unchanged broad suites.
5. Report matrix of PASS / FAIL / NOT RUN / BLOCKED with exact evidence paths, commit SHA only if committed, any regressions and remaining blockers. Declare milestone PASS only if every required gate passed; otherwise INCOMPLETE.
6. At integrated/public checkpoints, include README-linked documentation, example commands, screenshots and workflow/badge correctness using `hazel-public-docs` as needed. Documentation assertions must match actual branch/SHA and verified platform coverage; incomplete intermediate experiments can defer this.
7. Respect authorization: a passing test does **not** authorize a merge, release, backend-default switch or the next stage. Commit/push only under task-specific permission.
