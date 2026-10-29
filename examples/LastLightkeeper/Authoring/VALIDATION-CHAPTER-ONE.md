# Validation record

> **Historical scope — marked 2026-10-28.** This record describes its named checkpoints, including features absent or pending then. It is not current setup, support status or a new feature roadmap. Use the [current guides](../../../docs/README.md) and [history index](../../../docs/history.md).

Baseline master: `463cbdd`. Dedicated branch: `feature/last-lightkeeper`; no merge.
All existing vendor submodule pins and the existing stash were retained. Builds
used the public Premake workflow, two compiler jobs, and sequential regressions.
Test/editor/player data used isolated profiles.

## Local Linux

- Corrected Debug and Release native builds completed, and all 14 regressions
  passed in each configuration. Logs: `/tmp/lightkeeper-profile-{debug,release}-build.log`
  and `/tmp/lightkeeper-profile-{debug,release}-tests.log`.
- Source game model/native tests passed in both configurations, preserving the
  existing MeadowRun and Skybound checks. They cover ordered/idempotent repairs,
  payload version/invalid data, all mirror states, obstruction/cycle bounds, tide
  warning/safe alcove, actual bindings, bounded prefab pool, physics checkpoint,
  authored field isolation, transitions, retired owners and editor Stop isolation.
- Nutella and Hazelnut startup/render/resize/graceful close passed for all three
  projects in Debug and Release. No authored scene or project file changed.
- Relocated Release packages passed the same native gameplay checks and Nutella
  lifecycle tests with source assets, SDK and compiler unavailable. Final profile
  acceptance passed while hiding the bundled GAC to require normal BCL profile resolution.
- Strict native export audits validated sprite/animation/WAV dependency closure
  and managed assemblies. Actual archives contain all six Kenney licenses,
  provenance, project notices, Mono/engine/font/native redistribution material
  and miniaudio's original dual-license text (MIT selected).

## Render and interaction review

Actual Intel HD4000 / Mesa 26.2.4 / OpenGL 4.2 display was available. Renderer used
its existing OpenGL 4.1 resource/GLSL path. Captures in `Authoring/Captures` are
actual Release frames: title, restored relay, tide recovery and chapter ending.
Additional local evidence is under `build/lightkeeper/manual` and
`build/testing/games/{Debug,Release}-native`.

An agent-controlled, keyboard-driven review traversed harbor → grove → pump →
lens → relay → lighthouse → Dawn. This was interactive inspection, not a
screen-coordinate gameplay bot or a claim of human pacing. A tide surge restored
the safe checkpoint and retained the lens; closing/relaunching restored milestone
progress. Final isolated payload was `LK1|127|3|2|1`. Both runs closed normally.
Inspection corrected water/dock selection, body enum serialization, HUD/dialogue
size, empty panel selection, complete 16×32 tree regions and beam composition.

## Cross-platform evidence

Final runtime-fix CI: [run 38137170835](https://github.com/cringlekaden/Hazel/actions/runs/38137170835),
commit `ca1ec7c`. Windows passed the complete Debug/Release, source game and
relocated game workflow, including software OpenGL and the GL 4.1 profile. Linux
CI also passed the complete workflow, including Debug/Release source games and
relocated games under software OpenGL and the GL 4.1 profile. The entire run is green.
An earlier full Linux run passed; Windows Debug/Release's 14 regressions and
both general relocated package graphics profiles passed, then the new game
crashed entering Breakwater. Windows-executable reproduction under an isolated
Wine prefix and Mono assembly logs identified missing System.Core/System search:
`mono_set_assemblies_path` incorrectly selected `lib`, while Windows has no GAC.
The engine now searches the actual bundled `lib/mono/4.5` profile and rejects null
entity allocations with a dependency diagnostic. The diagnostic Wine clone was
only for cause isolation; no duplicated DLL workaround is shipped.

Verified local archives and checksums are in `dist/games`: Linux `.tar.gz`
(runtime-fix/evidence commit `4aabeee`) and Windows `.zip` (CI commit `ca1ec7c`).
The later commits change test evidence/references, not runtime game behavior.
Both archive checksums were checked locally. Local Linux exports use the build
host glibc baseline; the CI Linux archive targets Ubuntu 24.04 as described in
LAUNCH.txt. The CI Windows archive is downloaded locally, not cross-compiled here. Source and relocated game logs are
`/tmp/lightkeeper-profile-{debug,release}-games.log` and
`/tmp/lightkeeper-profile-packaged-games.log`; detailed native evidence is under
`build/testing/games`.

Physical Windows GPU/audio quality remains unverified by CI/software rendering.
Wine diagnostics are not Windows acceptance. Sound cues have matching visual
feedback, including the supported silent fallback when no device is available.

## Scope and next milestone

The 30–45-minute game is not complete. The first connected location has a complete
restoration arc and chapter endpoint. Its 8–12-minute pacing target still requires
timed human playtesting. Character art uses static Kenney poses and movement bob;
there is no directional walk cycle, enemy combat or ambient music in this chapter.
The tide is its meaningful hazard. Later upgrades, enemies, final encounter and
full rescue ending remain authored content milestones, not implemented claims.

Next: human pacing/readability review of Breakwater, then the cliff cistern/sea
caves, lantern upgrade, movable shutters, one distinct enemy and Mara's reunion.
Commands and platform requirements are in the project README; engine/storage/
audio contracts are in `docs/runtime-storage-audio.md` and the bounded audit in
`docs/design/last-lightkeeper.md`.
