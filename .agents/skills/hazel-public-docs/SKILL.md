---
name: hazel-public-docs
description: Audit or update Hazel public GitHub documentation, build/run guides, linked examples, screenshots, README CI badges, and clean-clone onboarding at integrated checkpoints. Use for explicit docs work or completed user-visible milestones; skip full audit for experimental intermediate edits.
---

1. Identify checked-out **branch/SHA**, task scope and last verified CI state. Read `docs/agents/PUBLIC_DOCS.md` and only the public entry-point documents relevant to this checkpoint. Treat source/scripts/workflows/examples as authority, not older claims.
2. At integrated checkpoints, run `python3 tools/agents/check_public_docs.py --root .` to catch local broken links and images. Check any parser edge cases manually. Traverse `README.md` outward to active guides and examples, following relevant downstream links. Check local links/anchors, file casing, images and source commands. The current development branch may differ from default `master`; show correct branch-specific clone instructions and never pass off master CI as branch CI.
3. For each supported platform (Windows, maintained Linux, Apple Silicon macOS), check the actual prerequisite/setup/build steps, Hazelnut and Nutella launch paths, example projects/scripts, packaging/export distinctions, and explicit limitations. Use source evidence or scoped execution; mark unrun platform checks clearly.
4. Verify GitHub Actions workflow names/triggers/jobs and badge URLs/labels. Preserve established CI behavior; do not dispatch workflows or rewrite CI solely to make a badge appear green without authorization.
5. Update only incorrect or stale public-facing text/images necessary for accurate onboarding. Use real representative captures for screenshots; do not synthesize screenshots or claim unverified native runs. Preserve historical design/evidence docs as revision-specific records.
6. Test links and selected instructions proportionately. For experimental or incomplete work, record drift and next public checkpoint instead of forcing a broad green build or infinite troubleshooting loop.
7. Return a compact table of corrected paths, checked commands/links, CI and platform evidence, and outstanding NOT RUN/BLOCKED items. No implicit commit, push, release, stage advancement or platform-certification claim.
