# Upper tower and full-game endpoint

Separate editable UpperTower, HarborHome, Ending and Credits scenes complete the
remaining route. Existing polished chapters and asset pixels are retained.

Observatory Dawn → Enter → exposed tower walk → fixed-mirror entrance latch →
three independently latched drive contacts → main lantern → morning landing →
boat farewell → ending/credits. Completed Continue revisits the landing. The
boat can replay the ending; New Game retains its replacement confirmation.

The tower's warm stone, iron columns and enclosed glass contacts contrast with
the cold, flooded observatory. The lamp's established range, aim and reflection
are sufficient. A locked rush mark, focused-light scatter, stone cover and safe
repair posts give counterplay. The wisp stops when all drive contacts hold.
Contacts can be tackled in any order; recovery retains them. Gusts on the exposed
approach use the established warning/unsafe/clear language, with visual feedback
independent of sound. No new engine feature or dependency was needed.

At the landing, Orrin and Nessa settle the compass/boat thread; Cal leaves a
reliable mooring signal, Mara gets a dated repair commitment, and Iona orders
the missing steps and handrail. Short optional conversations precede the boat
farewell. Ending and credits remain on the current presentation layer until the
runtime UI milestone; no additional hand-coded mouse regions were introduced.

LK4 extends the retained LK3 fields with TowerCheckpoint (0..2) and Contacts
(bitmask 0..7). Old payloads default these to zero and retain all old settings
and optional discoveries. Entry, drive, main-light and boat-departure flags are
separate. Drive requires all three contacts. Failed departure writes roll back
the completion flag, preserve the active scene, and allow checkpoint retry.
Finished Continue selects HarborHome, never a replay of the final encounter.

All new composition, colliders, cue sources, actors and references are native
scene entities. The one-time recipe protects existing files; it is never used
by Nutella. Six original Kenney CC0 licenses are unchanged; provenance records
new uses of existing assets. No human duration or artistic certification is
claimed by automated mechanics and collision tests.

Validation results are recorded after the checkpoint's final local/package/CI
checks. Runtime UI implementation and Hazelnut authoring/migration remain separate
integration milestones.

## Verified game checkpoint: e579469

On Grandpa, Debug/native and Release/GL4.1 game suites passed the complete native
New Game-to-ending route, old saves, partial contacts, recovery, transitions,
failure-safe exits, completed Continue and unreadable-save retention. Both
build configurations and all 14 existing engine regressions passed. All three
example packages passed extracted gameplay/startup with source SDK and GAC
unavailable. Lightkeeper's native sprite/animation/audio closure and included
license materials passed export checks. The Linux archive is under
`dist/lightkeeper-finished/LastLightkeeper-linux-x86_64-Release.tar.gz`.

Linux and Windows CI both passed for the gameplay commit:
https://github.com/cringlekaden/Hazel/actions/runs/38236272078

Actual Nutella input exercised the old-LK3 departure, gust recovery, contacts,
partial-contact quit/relaunch, focused counterplay, main light, reunion, boat,
ending, credits and completed Continue at 1024x600/1280x720. Internal captures
are under `build/lightkeeper-completion/live/tower-manual`. An isolated Hazelnut
copy opened UpperTower in 0.974 seconds with 33 visible progress frames and
completed two Play/Stop cycles. Its captures are under the `tower-editor-final`
profile. Source settings, authored scenes and real player saves were not used
as test scratch data. Human pacing and physical Windows/Linux acceptance remain
required; the runtime UI model is a separate incomplete integration milestone.
