---
name: hazel-code-review
description: Review Hazel commits or diffs adversarially for correctness, lifetime, synchronization, regressions and architecture. Use for explicit code review; do not implement unless asked.
---

1. Record exact branch, SHA/range and diff. Inspect relevant callers/callees and prior invariant/test records; keep review read-only by default.
2. Search for failures before style nits: incorrect ownership and teardown, partial initialization, exceptions/errors, queue/command completion, CPU/GPU races, lock ordering, visibility barriers, ABA/generation reuse, slot overwrite, dangling callbacks.
3. Check numerical and API boundaries: zero/maximum sizes, alignment, narrowing, integer overflow, format/coordinate/precision assumptions, invalid inputs, platform conditionals, GL4.1 functionality and packaging.
4. Trace at least one success and one failure/abort path through the full state machine. Confirm tests actually exercise the claimed condition, not only the happy path. Differentiate static reasoning from executed evidence.
5. **Disprove suspected findings** by checking compensation elsewhere (ownership, synchronization, validation or a test) before reporting them. Avoid speculative issue lists and broad redesign proposals.
6. Output findings first, ordered by severity, each with file:line/range, failure scenario, evidence and smallest correction. Then gaps/questions and verdict. If no findings, say so with remaining risks. No commits or feature stages without permission.

For detailed shader/GPU numerical qualification invoke `hazel-shader-parity`; for architecture-only planning invoke `hazel-gpu-review` rather than duplicating reviews.
