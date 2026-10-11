# Validation record — in progress

Baseline master: `463cbdd`. Dedicated branch: `feature/last-lightkeeper`; no merge.
Initial audit/assets checkpoint: `3501555`; storage/audio/reader checkpoint: `dd16bd5`.

- Linux native Debug build completed; initial 14 existing regressions passed,
  including new storage corruption/version/atomic-write/editor-isolation tests and
  subtree camera safety. Logs: `/tmp/lightkeeper-debug-tests.log`.
- Actual Intel HD4000 / Mesa 26.2.4 / OpenGL 4.2 title → island rendered.
  Title/arrival captures: `build/lightkeeper/captures/`. Source reference images
  are separate files in this Authoring directory.
- Render inspection corrected dock-vs-water selection, scene physics enum
  encoding, HUD/dialogue size, empty UI panels, and lighthouse composition.
- Further graph synchronization and compact beam performance work is being
  rebuilt and checked in Debug/Release. This checkpoint does not claim these
  final checks, relocated packages, Windows CI or human pacing have passed yet.
- Full game is not complete. The 8–12-minute slice target and eventual
  30–45-minute adventure require hands-on timing and later authored chapters.

This record will be replaced with final local/CI/package results after validation.
