# Lean Codex workflow (quality gates unchanged)

**Default working choice:** GPT-6.1 Sol Medium for bounded implementation; choose High only for genuinely difficult architecture, synchronization, regression or failure analysis. The model picker is user-controlled; no repository file can change account billing or Codex reasoning effort automatically.

## Day-to-day loop

1. Read the task, current `git status` and narrow entry point. One `rg` search for symbols/callers, then directly relevant files and tests. Use `docs/agents/` or a specific skill only when the task needs them; don't load every document automatically.
2. State a short approach only if there is a real tradeoff. Solve one coherent slice; don't combine unrelated cleanup. Prefer existing helpers/ownership.
3. Run smallest compile/test that can disprove the change. If failing, form a concrete hypothesis before rerunning; avoid speculative loops.
4. Keep raw logs/evidence in ordinary repo test locations. Return a short summary, not pages of terminal output. Use exact paths to evidence.
5. At the specified acceptance boundary run full required tests; missing hardware/env → report INCOMPLETE and blocker, not PASS.
6. For public-facing changes, refresh affected README-linked docs at the next stable checkpoint. For experimental slices, record deferred docs drift; do not trigger full link audits or rebuilds after every edit (`docs/agents/PUBLIC_DOCS.md`).

Avoid needless web search for established repo decisions, duplicate architecture documents, repeatedly explaining project history, full-repo rescans, premature performance microbenchmarking, and compulsory long reports after trivial fixes.

If two distinct debugging hypotheses fail without producing new evidence, stop and ask for guidance or a new diagnostic direction. This is not a blanket two-try cap on genuine progress.

## Reporting budget

Routine: **Change**, **Tests**, **Docs status if relevant**, **Risks/next** (short sections). Milestone: **SHA**, **files**, **measured gates**, **warnings**, **missing gates**, **verdict**. If implementation is uncommitted, report HEAD and uncommitted status, not a fabricated new SHA.

Prompt from the reviewer should usually name only task, scope, exact acceptance boundary, and relevant skill/doc. Avoid restating `AGENTS.md`. Extra instructions are for changed requirements only.
