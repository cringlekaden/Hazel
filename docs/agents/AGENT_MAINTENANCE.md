# Maintain the Codex setup without agent-context bloat

This package is repo-local guidance, not an autonomous governance system. Source and current test records always win over `docs/agents` summaries. Review these instructions at architecture/milestone changes, not on every source edit.

## Ownership and activation

- Root `AGENTS.md`: the few rules every task needs (scope, style, safety, evidence, public readiness). Keep it short; do not embed feature inventories, build logs, or a mandatory sequence of documents.
- `docs/agents/`: *reference* facts and quality/roadmap decisions, accessed only when the task needs them. `docs/design/` remains canonical for experiments and frozen acceptance records.
- `.agents/skills/<name>/SKILL.md`: *procedures* with specific triggers, concise YAML `name` + `description`, steps and outputs. A skill should solve a repeated task better than ordinary instructions. Do not add a skill for every feature or for a one-off question.
- `tools/agents/check_public_docs.py`: a **local filesystem** link/image check, not an executable build, CI run, visual comparison, or platform qualification. Run when appropriate to avoid manual scanning. It cannot validate external websites or missing semantic feature claims.

## How to improve reliably

At a representative checkpoint, compare a few real tasks on Sol Medium with and without a proposed guidance change. Record (a) first-pass correctness, (b) targeted/full tests, (c) reviewer findings, (d) retries and total usage if exposed, (e) time to qualified outcome. A shorter prompt is not a win if it increases mistakes and rework. Keep examples/records short; discard guidance that doesn't improve outcomes.

Suggested recurring evaluation cases:
1. One small header/API change: correct includes, no new first-party warnings, platform guards.
2. A Metal 4 lifetime bug: completion-gated upload reuse, failure and shutdown checks.
3. One GLSL→MSL parity issue: bindings, integer output, geometry/depth, frozen lighting.
4. A README/public docs checkpoint: valid links and images, accurate branch clone and CI statements, no false platform claims.
5. An environmental test blocker: stop with INCOMPLETE and exact evidence, no speculative retries.

Don't automatically trigger full suites, CI dispatch, external network traffic or commit/push. A reviewer can request a `hazel-code-review` pass or `hazel-public-docs` checkpoint; these do not grant write authorization unless the task does.

## External references (patterns, not transplanted project rules)

- Filament `AGENTS.md`, `CODE_STYLE.md`, `skills/code_review`, `skills/verification_protocols` and `skills/cpp_header_inclusion`: https://github.com/google/filament
- OpenAI's repo-local skill pattern: https://developers.openai.com/blog/skills-agents-sdk
- OpenAI guidance on keeping instructions/skill descriptions lean: https://developers.openai.com/blog/rethinking-skills-and-prompts-for-gpt-6-astra
- OpenAI guidance on evaluating skills: https://developers.openai.com/blog/eval-skills
