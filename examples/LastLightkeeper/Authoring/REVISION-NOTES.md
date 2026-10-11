# Chapter revision and continuation

Player evidence: the author completed the first chapter in about **five minutes**
after learning its mechanics. This replaces the unverified 8–12-minute estimate.
No automated run is pacing evidence. Additional content must earn its time through
new decisions, discovery and consequences, rather than longer errands or text.

Phase one gate: reproduce large-scene authoring, measure actual load/idle costs,
present loading feedback before/during work, retain complete validation and verify
selection, hierarchy, save, sheet editing, Play/Stop and reopening. Do not author
a second large chapter until the editor is responsive after loading.

Initial reproduction on Grandpa (i3-3227U, 1.9 GHz, 8 GB RAM, Intel HD4000):
Release opened a copied project with Breakwater as startup scene in approximately
5.3 seconds (11:40:38 window/renderer to 11:40:43 ready). RSS was about 184 MB,
then sustained near 100% CPU while idle. Repeated document/status checks emitted
and validated the entire scene as YAML. These calls continued after ready, so the
issue could appear to be loading that never finished. Frame-level measurements
and the corrected editor results will follow after the build.

Loading design: staged, measured operation/phase labels; entity counts where a
finite total is available, indeterminate animation for other phases. Immutable
YAML parsing may use one worker while the main thread presents progress and polls
window events. Registry, graphics and Mono operations remain on the owner thread.
Authored dirty state uses exact field bytes (not a collision-prone checksum), with
runtime handles/caches excluded. Save, load and Play keep their full validation.

Writing direction: Iona knows this machinery and notices physical evidence;
Orrin knows the landing/current and needs the light to get his boat through;
Mara's notes record repairs and disagree with unsafe keeper habits. Their history
comes from practical choices and things left behind, not inspirational slogans.

Terrain/visual direction: inspect the actual Tiny tiles and their edges before
choosing an adjacency table. Treat terrain boundaries and corners consistently.
Replace stretched UI tiles used as world props with suitable Tiny-family art.
Beam routes need a dark casing, bright core and distinct termination/blocked
markers so route shape remains readable without depending on hue alone.

Phase-one observations (Release, during a two-job native build): project work
finished in 2.486 seconds and presented 67 progress frames. Parse 1.265 seconds,
schema 0.52 seconds, components 0.25 seconds, hierarchy/physics 0.018 seconds,
sprite previews 0.289 seconds, folder index 0.031 seconds, managed scripts 0.030
seconds. Full launch also includes renderer/font startup (about 7.5 seconds in
this concurrent run). Ten old YAML snapshots cost 5.957 seconds; one hundred
exact authored snapshots cost 0.763 seconds. These are local measurements, not
cross-machine guarantees. The window continues to render frequently after load;
selection, expanding the coast group, asset search, native Tiny Town sheet
selection/edit/save, and Play/Stop were exercised in an isolated project copy.
Capture evidence is under `build/lightkeeper-next/editor-fixed/`.

An additional browser defect restored `Assets/.` and compared it literally
against entry parents. This hid root folders until a breadcrumb was clicked.
Normalize navigation to canonical owned folders, preserving containment checks.

Authoring gate passed: all 14 Release native regression executables passed,
including exact authored comparison, strict invalid-save validation, sheet edit/
assignment, guarded document replacement, hierarchy, save/reopen and runtime
lifecycle. The real isolated Breakwater copy reopened in 2.087 seconds of project
work with 60 loading frames (full window-to-ready about 3 seconds with cached
font). Representative real editor captures are committed in `Captures/Editor-*`.
