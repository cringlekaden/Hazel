# Chapter revision and continuation

> **Historical scope — marked 2026-10-28.** This record describes its named checkpoints, including features absent or pending then. It is not current setup, support status or a new feature roadmap. Use the [current guides](../../../docs/README.md) and [history index](../../../docs/history.md).

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

Continuation boundary: Cliff Cistern and Sea Caves are complete connected authored
locations. The sleeve focuses Iona's portable lantern, enabling distant receivers
and repelling the storm wisp. Three notched shutter banks have physical slats and
optically matching openings. Drain/bridge/gallery/refuge latches persist. Opening
the refuge is separate from meeting Mara; her winch key opens a dry return route.
Mara follows its authored safe waypoints. The final landing points toward the
observatory and missing keeper; chapter three remains future content.

LK2 adds shutter positions and a separate cave recovery marker. LK1 preserves
all seven repairs, optional discoveries, mirror orientation and checkpoint; a
completed old relay gets the already-earned service-door default. Completed old
beacons Continue at the cliff workshop. Future/corrupt payloads remain intact.
The native save envelope/namespace/path are unchanged. No audio/AI/dialogue/save
engine rewrite was needed for this continuation.

Art corrections use the existing six CC0 packs. Native 8-pixel grid imports crop
Tiny Wisptle and Tiny Town into editable quarter regions; no new artwork was
painted or generated. Correct Tiny Wisptle atlas size: 288 by 176 (18 columns),
not the old design document's 304 by 176. Shore tiles are water-side boundaries,
with cardinal and diagonal adjacency. Town gravel is inset and uses its matching
corner regions. Military boat art is removed; Iona wears the ordinary worker
sprite instead of armor. Tiny Dungeon mirror faces replace enlarged UI squares.
Tiny Wisptle's dark-edged energy strip/arrows, white endpoint and obstruction
cross distinguish route shapes independently of color. Warning boards move
before a tide surge; cave rush marks and recoil remain usable without audio.

## Whole-game polish (from e92f1c5)

Scope stays within the two existing chapters. Menu labels use a shared baseline;
HUD insets, hierarchy and warning rows match across locations. Dialogue advances
with Next and ends with Close or Continue where a transition follows. New Game
confirmation is cancellable without a timer. Failed writes retain the previous
menu journey and cannot generate a successful checkpoint message.

Trees use the existing native 16x32 pine region at its original aspect ratio. Joined gravel follows the
actual route footprint. Quiet slate and directional masonry describe the cistern's
working paths and pool; the cave seep no longer uses door/tank tiles as edging.
Rubble is concentrated at falls, with salvage at storage areas. No atlas pixels,
existing sheet IDs or save payload fields change. Authored scene identities are
retained where an existing object keeps its tag. Composition helpers reproduce
the layout; scenes remain normal Hazelnut documents.

Orrin identifies his sister on the survey ledge. Iona's repair observations stay
separate from explicit control instructions. Mara's reunion is shorter and her
key's consequence is stated directly. Notes, objectives, prompts, errors and
endpoint text follow the same terminology. Adjustment, warning, repair and
recovery cues use the existing licensed sounds at distinct authored gains.

The pine correction uses the existing `pine_tall` region (ID
`be7a5bc884ec131c`, Rect 64,0,16,32) once per tree at 1x2 world units.
That region already includes its trunk: the attempted 4/16 composition duplicated
it. All duplicate bases are removed, sibling orders reindexed, and the source
check verifies crop, pivot, counts, aspect ratio and native sampling. Colliders,
texture pixels and existing sheet IDs are unchanged. Dialogue controls now have
a fixed authored footer. Chapter exits require a successful progress write.
