# Hazel Codex setup — v3 (repo-local drop-in)

This bundle adds **only new agent/docs/checker files**. It does not edit renderer code, GitHub Actions, README, Premake, asset files, or existing design records, and it does not authorize M2.

## Install (new setup)

From macOS Terminal, in your **one existing checkout**:

```sh
cd /Volumes/External/kadencringle/Projects/Hazel
git branch --show-current && git status --short
unzip -n ~/Downloads/Hazel-Codex-Setup-v3.zip -d .
python3 tools/agents/validate_setup.py --root .
git status --short
```

The ZIP has no enclosing folder: put `AGENTS.md` beside your existing root `premake5.lua`; `.agents/skills/` is a hidden folder. If a ZIP utility creates an extra directory, move the **contents** of it, including hidden directories, to the repo root. `unzip -n` is safe for first install: it skips existing files.

## Upgrade if v1/v2 was already installed

**Important:** `unzip -n` does NOT update an old `AGENTS.md` or skills. Stage and compare before selectively copying; do not overwrite edits you made to local instructions.

```sh
mkdir -p /tmp/hazel-codex-v3
unzip -q ~/Downloads/Hazel-Codex-Setup-v3.zip -d /tmp/hazel-codex-v3
diff -u AGENTS.md /tmp/hazel-codex-v3/AGENTS.md || true
# After reviewing, selectively copy updated files or use a merge tool.
# If you haven't customized these docs/skills, a full copy is fine:
cp -R /tmp/hazel-codex-v3/.agents /tmp/hazel-codex-v3/docs /tmp/hazel-codex-v3/tools .
cp /tmp/hazel-codex-v3/AGENTS.md /tmp/hazel-codex-v3/CODEX_SETUP.md .
python3 tools/agents/validate_setup.py --root .
```

`cp -R` merges into those directories, not replace them wholesale. This upgrade assumes you review the diff first. It changes **no engine/CI/public README code**.

## How Codex uses this

Codex discovers the root `AGENTS.md` as repo guidance and may select `.agents/skills/<skill>/SKILL.md` when appropriate. Descriptions are intentionally short. The longer material is read on demand, not automatically at every turn. **Start a new Codex conversation** after installation if a running session is holding old context.

| Task | Useful skill | Read when appropriate |
|---|---|---|
| Implement small feature or milestone slice | `hazel-implementation` | Narrow source + design gate |
| Metal/OpenGL architecture audit | `hazel-gpu-review` | `ARCHITECTURE.md`, design record |
| GLSL/SPIR-V/MSL translation and cross-API parity | `hazel-shader-parity` | Shader/toolchain source, numerical gates |
| C++ headers/includes/warnings | `hazel-cpp-integrity` | `STYLE.md`, `QUALITY.md` |
| Review pushed code/commit | `hazel-code-review` | Diff, tests, call paths |
| Milestone qualification | `hazel-qualification` | Explicit acceptance gates |
| README/linked docs/examples/CI health | `hazel-public-docs` | `PUBLIC_DOCS.md`, README link chain |

Example read-only next task (M2 **not yet authorized**):

> Use `hazel-gpu-review`. Inspect the M1 record and current shader tooling; recommend the smallest M2 GLSL-to-MSL qualification slice and its acceptance gates. Read-only. Do not implement or commit.

After authorization:

> Implement the approved M2 shader qualification slice using `hazel-shader-parity` and `hazel-implementation`; preserve OpenGL, run scoped verification and stop after reporting evidence.

Example public-docs task:

> Use `hazel-public-docs` to audit README click-throughs, branch-aware clean-clone/build/run examples, screenshots and CI badge claims. Fix only verified documentation drift. Report untested OS/hardware explicitly; do not change CI triggers or dispatch workflows without approval.

## Public docs and CI expectations

User-facing docs begin at root `README.md`, not at `docs/agents/`. The current branch's README historically describes a 2D-oriented master onboarding path while development contains newer 3D work. At an authorized **stable** public checkpoint, make branch/status distinctions explicit rather than advertising unfinished Metal as production. Verify both Hazelnut and Nutella startup and tracked examples on documented supported platforms using available evidence. Distinguish actual CI runs (specific SHA) from badges showing master history. Don't invent screenshots: use real, representative captures with permission and artifacts.

For fast **local-file** checks use:

```sh
python3 tools/agents/check_public_docs.py --root .
# Optional: include best-effort README link anchor checks
python3 tools/agents/check_public_docs.py --root . --check-anchors
```

This script is non-mutating and checks reachable relative paths/images, not external URLs, CI green status, screenshots' correctness or executable build instructions. Link-parser false positives require human review. Full build/example/package qualification remains a separate explicit task.

## Maintain and commit

Update `docs/agents/PROJECT.md` for milestone changes, `ROADMAP.md` for priority changes, `PUBLIC_DOCS.md` for documentation policy and canonical `docs/design/` evidence for tests/experiments. Avoid duplicating large logs. See `docs/agents/AGENT_MAINTENANCE.md` for concrete evaluation cases and pruning guidance.

Once reviewed, **you** may version only these instructions/tools:

```sh
git add AGENTS.md CODEX_SETUP.md docs/agents .agents/skills tools/agents
git diff --cached --stat
# Commit/push only when you intend; this package itself does not commit.
```

Current recorded checkpoint: Metal 4 M1 PASS at `9030dd62b0e486a369d39de35fb24d43389b022d` on `feature/3d-foundations` (Codex-reported native results; see `docs/design/metal-m1.md`). M2 was not started by that commit. Refresh from the actual branch every time the project moves forward.
