# Migration progress

Last updated: 2026-09-30. Branch: `migration/upstream-1feb705`.
Baseline: `b030be7`. Fixed target: `1feb70572fa87fa1c4ba784a2cfeada5b4a500db`.

## Resume here

Read PLAN.md, PRESERVATION.md and COMPARISON.md. Check `git status` and submodule
cleanliness before edits. The upstream source checkout is in
`/tmp/hazel-upstream-1feb705`; if absent, clone TheCherno/Hazel and detach at the
fixed hash. Do not use a moving master. Do not push or merge. Build sequentially
with at most two jobs.

## Preparation status

- Inspected local tree before any write: clean master `b030be7`, four clean
  initialized submodules, existing ignored artifacts/configuration preserved.
- Dedicated branch created. Git metadata/display/network are restricted by the
  workspace permission profile, so those operations used explicit tool escalation.
- Downloaded real upstream Git source and verified its exact commit.
- Recorded full tracked-tree comparison and dependency pins, preservation
  inventory, staged plan and reproducible comparison script.
- Baseline `premake5 gmake`: passed with installed Premake 5.0.0-dev and the
  existing legacy-flags shim. Compiler GCC 16.2.1; GNU Make 4.4.1.
- Baseline `make config=debug -j2`: incremental pass.
- Baseline `make config=debug -j2 -B`: full compile/link pass, evidence in
  evidence/baseline-debug-full.log.
- Baseline `make config=release -j2 -B`: full compile/link pass (0), evidence in
  evidence/baseline-release-full.log.
- Desktop baseline Sandbox: startup/GLSL initialization completed on Intel HD
  4000, ran for ten seconds until timeout (124), no logged shader/startup errors.
  This is a startup smoke test, not visual validation or a clean shutdown test.
  Assets/imgui.ini were copied to /tmp/hazel-baseline-run; original files retained.
- A Release invocation briefly overlapped the final part of Debug; it was
  immediately interrupted (130) and restarted sequentially after Debug exit 0.
  All subsequent builds must remain sequential.
- Sandbox runtime inside the sandbox failed GLFW initialization (SIGTRAP, 133).
  `glxinfo -B` was also blocked there; outside sandbox it successfully identified
  accelerated Intel HD 4000 / Mesa 26.2.3 core 4.2, GLSL 4.20. Desktop rerun is
  necessary to distinguish display isolation from an engine failure.

## Implementation stages

Stage 1 planned, not yet started. Stages 2–9 pending. Windows MSVC builds and
runtime have not been run. Existing remote Actions coverage is retained; no
remote workflow was triggered and no results are assumed.

## Evidence policy

Logs under evidence/ are actual command output. Distinguish full and incremental
builds, timeout-limited startup and clean shutdown, project generation and MSVC
execution, and synthetic checks from real GPU rendering/readback. Logs and tree
comparison are a snapshot; update this document as each gate completes.
