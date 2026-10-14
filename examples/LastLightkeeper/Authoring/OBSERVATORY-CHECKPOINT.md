# Observatory playable checkpoint

Continuation of clean `13cbbdd`, with the previous chapters' art, trees,
dialogue, sound, HUD and collision authoring retained. MainMenu's chapter label
and the Cliff Dawn departure action now include the observatory.

West lens bench → long sightline to span drive → southern bridge → carriage
winch/east sightline → north instrument room → Nessa and Cal → tower drive →
west stair farewell → Observatory Dawn. The next chapter is explicitly unavailable.
All world composition, collision rectangles, actors, source clips, cue sources,
HUD and references are normal editable scene entities. The composition recipe
is never invoked by the runtime and protects existing files without --overwrite.

LK3 adds an observatory checkpoint and carriage setting to the retained LK2
fields. LK1/LK2 defaults do not invent survey progress. Winch power is distinct
from departure: Continue cannot skip the farewell. Failed departure writes
roll back the completion flag and leave the active chapter available for retry.
The native save namespace/envelope and optional discoveries are unchanged.

Normal input exposed a snag when Iona hugged joined masonry fixtures. The new
chapter uses existing native circle physics for rounded feet and authored fixed
solid rectangles. The regression steps real Box2D fixtures along the passage
center and wall contact, alongside authored connectivity and optical obstruction
checks. Gates/carriage retain their own fixtures. Earlier chapters are unchanged.

Nutella was inspected at 1024x600 and 1280x720; renderer-state regressions also
capture 4:3. Old-LK2 departure, lens/carriage/crew/winch, winch quit/relaunch,
farewell, endpoint and title return were exercised through actual keyboard input.
An isolated copy opened in Hazelnut, with hierarchy search/selection, resize and
two Play/Stop cycles. Temporary positional logging was removed.

Human timing and Linux/Windows hands-on acceptance remain required. Static
character poses and the existing brief sound cues remain; no new artwork,
dependency or broad engine feature was introduced. Runtime UI components and
menu migration are designed in docs/design/last-lightkeeper-completion.md and
are not implemented at this checkpoint. Upper tower/final encounter/ending follow.
