# Revision validation

Work continues from pushed `4e5f6f1` on `feature/last-lightkeeper`. Master remains
`463cbdd`; vendors, settings and the existing stash are preserved. Prior chapter
one history is retained in VALIDATION-CHAPTER-ONE.md. The player's approximately
five-minute original playthrough is the timing evidence. Revised playtime remains
unmeasured by a human; neither native tests nor keyboard inspection establish it.

## Authoring diagnosis and measurements on Grandpa

Intel i3-3227U, 1.9 GHz, two cores/four threads, 8 GB RAM, Intel HD4000,
X11 1600x900, Mesa 26.2.4. The baseline reached ready in approximately 5.3 seconds
then remained near 100% CPU: document/status checks serialized and validated the
whole scene as YAML at least twice per frame. Ten YAML snapshots took 5.957 seconds;
one hundred exact authored snapshots took 0.763 seconds. Exact field comparison
replaces repeated emission, while save/load/Play retain complete validation.

The original scene reopened in 2.087 seconds of project work with 60 progress
frames. The revised 3,591-entity Breakwater opened in 3.103 seconds with 77 progress
frames; full cold window-to-ready was 6.55 seconds. Parsing runs on one CPU worker
while the owner thread presents frames. Schema/components/sprite preparation show
real entity totals; parsing and other unknown-length phases show indeterminate
activity, current operation and elapsed time. Registry/GPU/Mono stay on the owner
thread. Loading presentation polls native events and defers editor mutation.

Isolated copies exercised real hierarchy expansion/selection, scene save/reopen,
Tiny Town sheet region edit/save and Play/Stop. Editing responded after loading.
The root browser also had a separate Assets/. comparison bug, fixed by canonical
owned-folder navigation. Editor-loading, Editor-hierarchy and Editor-sheet are
real captures; final-load logs are in build/lightkeeper-next/editor-final-check.

## Local acceptance

- Canonical Debug and Release builds completed with two compiler jobs.
- All 14 engine regressions passed in Debug native and Release OpenGL 4.1.
- All three source games passed logical/native checks and both Nutella/Hazelnut
  startup, render, resize and shutdown in Debug and Release. Scene/project bytes
  were unchanged by application tests.
- Actual chapter checks cover LK1 migration, service door, focus sleeve, shutter
  occlusion, fixed routes, both transitions, wisp telegraph/dodge/repel/recovery,
  Mara's key consequence, completed Continue, corrupt/future retention, fresh
  New Game and editor Stop isolation. Authored collision checks verify reachable
  landmarks, sealed gates, recovery positions and the opened dry return route.
- Strict exports audit sprites, animation, audio, managed/native dependencies and
  licenses. A matched set of the three games passes relocated OpenGL 4.1 checks
  with source resources/SDK unavailable and the GAC hidden. Existing older example
  archives were retained; matched packages are in dist/lightkeeper-next.

Detailed local logs: /tmp/lightkeeper-next-final-{debug,release}-build.log,
/tmp/lightkeeper-next-engine-debug.log, /tmp/lightkeeper-next-engine-release-gl41.log,
/tmp/lightkeeper-next-final-{debug,release}-games.log. Final art/package logs use
/tmp/lightkeeper-next-{art-regression,final-package,final-relocation}.log.
Continue-fix rebuild/retest/package logs use /tmp/lightkeeper-next-continue-*.log.
Native evidence/captures are in build/testing/games/{Debug-native,Release-gl41}.

## Render, input and save review

Real renderer captures cover title, court, blocked/solved cistern, cave telegraph,
Mara dialogue and endpoint. Actual display inspection used 1280x720 and 1024x600;
FBO inspection uses 960x720. It corrected unsupported punctuation, oversized frame
borders, dialogue/HUD overlap, marker placement under Iona, and viewport-edge gaps.
Native crops supply shore/gravel corner rules. Real Tiny sprites replace military
boat, armor, UI-square mirrors, tombstone occluders and the empty causeway floor.
The causeway uses joined stair/deck tiles; the storm wisp uses the actual ghost
pose. Dark beam casing, arrows, obstruction cross and lens endpoint carry state
without depending only on color. No new artwork was generated.

A subsequent relaunch exposed an empty dialogue overlay on mid-chapter Continue.
Chapter initialization now applies the dialogue visibility state, and native
regressions reload both partially completed cliff locations to verify the overlay
is hidden and the latched repairs remain.

The live Nutella profile continued LK1|127|3|2|1 into the cistern, acquired the
sleeve by keyboard, wrote LK2|1663|3|2|1|0|0 and quit normally. Existing native
storage tests cover atomic replacement, ownership/isolation, malformed/future
native envelopes and failure retention. Game tests cover both payload versions.
All player/editor profiles here are isolated from the user's saves/settings.

## Remaining acceptance

Windows CI is checked at the final meaningful checkpoint; its run/result is
recorded below when complete. Software CI does not establish physical Windows
GPU/audio quality. Human Linux/Windows play must check pacing, puzzle discovery,
small-display readability, wisp feel and Mara's return. Characters use static
Kenney poses/bob; carried flame and beacon use genuine native clips. Ambient music
and directional walking remain future art/audio work. Observatory and upper tower,
final encounter and full rescue ending are not yet authored or claimed complete.

The new SeaCaves scene opened in 1.192 seconds of project work with 37 loading
frames. Its real editor hierarchy search, direct-scene practice Play and Stop
worked in an isolated copy (Captures/Editor-caves-play.png).
