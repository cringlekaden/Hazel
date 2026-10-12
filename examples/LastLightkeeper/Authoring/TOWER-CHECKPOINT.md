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
