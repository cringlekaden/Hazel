# The Last Lightkeeper

An original top-down adventure in development for Hazel. Keeper Iona returns to a
storm-damaged island to restore its lower lighthouse signal and bring mechanic
Mara home. This project begins the planned 30–45-minute game; it contains the
four playable chapters: **The Breakwater**, **The Cliff Cistern / Sea Caves**,
**The Observatory**, and **The Upper Tower**, followed by the morning landing,
ending and credits. Runtime UI authoring and presentation migration remain separate
milestones; the current game still uses its existing presentation layer.

The author played the original Breakwater chapter in about **five minutes** once
its mechanics were understood. That is the pacing evidence; the old 8–12-minute
estimate is withdrawn. The revised and new chapters need timed human playtesting.
Breakwater now has a latched service receiver that opens the third mirror room,
repaired coast/gravel joins, proper Tiny mirror/worker sprites, outlined beams,
and shorter character writing. Its tide hazard retains safe recovery.

From the restored lower beacon, Iona reaches Mara's cliff workshop. A focus sleeve
turns the lantern into a portable beam: hold Space in the direction last moved.
The cistern's notched shutter and fixed mirrors release the upper inspection walk
and cave bridge. The caves add two shutter banks, a rushing storm wisp with a visible
fixed attack mark and focus counterplay, Mara's locked refuge, and her winch key's
dry return route. Mara follows that route; the landing resolves her rescue and
points toward the observatory. Opening her door and meeting her are separate
milestones. All repairs, openings and safe recovery points survive Continue.

From Cliff Dawn, Enter takes Mara's ledger up to the observatory. The survey
hall's floor has fallen through. Fit the long-focus lens at the west repair
bench, light the span drive across the gap, then use the east carriage winch to
clear the return sightline. The opposed latch opens the north instrument room.
Nessa and Cal have the tower calibration; talk to them, power the winch and meet
them at the west stair. Observatory Dawn is a saved departure checkpoint; Enter
continues to the upper tower. Light the entrance latch through its fixed mirror,
cross the exposed walk, then power three glass contacts in any order. Stone cover,
a locked rush mark and focused light provide counterplay against the tower wisp.
All three contacts steady the drive and open the main lantern bay. Starting the
light returns Iona to the morning landing. Talk to the rescued crew, Mara and
Orrin, then use the southern mooring to take the boat. Ending and credits return
to title; completed Continue revisits the landing.
The survey hall uses authored continuous solid rectangles and Iona's existing
engine circle collider for a rounded footprint. This avoids catching on joined
wall corners. Visual tiles remain selectable; gates and carriage keep separate
physics ownership. The earlier chapters' collision authoring is preserved.

## Play and edit

From a prepared checkout, with the native applications built:

```sh
python3 scripts/hazel.py script-build examples/LastLightkeeper/LastLightkeeper.hproj --config Debug
python3 scripts/hazel.py run Nutella --config Debug --project examples/LastLightkeeper/LastLightkeeper.hproj
python3 scripts/hazel.py run Hazelnut --config Debug --project examples/LastLightkeeper/LastLightkeeper.hproj
```

Use `python` on Windows. In Hazelnut, open Assets/Scenes/Breakwater.hazel, CliffCistern.hazel or SeaCaves.hazel and use
Play (Observatory.hazel, UpperTower.hazel and HarborHome.hazel are also available); all terrain, paths, buildings, occluders, interaction landmarks, HUD and
camera are authored, ordinary scene entities. Controller fields reference their
actual scene entities and can be selected/tuned in the Inspector. Rigidbody
owners remain roots; terrain/decorative districts and HUD use hierarchy. Only
bounded textured beam segments instantiate from a detached prefab at runtime.
Open Art/*.hsprites in the real Sprite Sheet panel to edit named regions, pivots,
clips and sampling; region IDs must remain stable.

| Control | Action |
| --- | --- |
| WASD / arrow keys | Move; diagonals normalized |
| E | Contextual interact, talk, turn mirror, repair or collect |
| Enter / Space / E | Advance authored dialogue |
| Q | Notes / close notes |
| Hold Space outside dialogue | Focus the upgraded lantern; movement changes its aim |
| Escape | Close dialogue or pause |
| Enter while paused | Resume |
| M while paused | Return to title and saved progress |
| Enter / left click on title | Begin / continue |
| N / title New button | New journey; repeat to confirm replacing existing data |
| C / title Credits button | Credits and asset thanks |

Stay clear of the tide causeway until the pump is repaired. Blue means clear;
moving warning boards and the countdown warn two seconds before the three-second surge. Higher ground at the
causeway's eastern alcove is always safe. A surge returns Iona to the safe
checkpoint and retains completed repairs. The journal records the current goal
and directions. The beam itself explains reflection; turn one mirror at a time.

## Progress and audio

Nutella saves milestone progress, mirror orientation and shutter positions under the stable project
namespace `cringlekaden.TheLastLightkeeper`. It restores safe checkpoints, rather
than arbitrary positions that could trap the player. Editor Play starts a separate
practice journey and clears it on Stop, without touching player data. Missing,
corrupt and newer saves are distinguished; the original file is retained until
an explicit confirmed new journey. LK1 payloads migrate to LK2 without losing
repairs, optional discoveries, mirror settings or Breakwater checkpoints. A
completed LK1 beacon resumes at the cistern workshop; cave checkpoints remain
separate. LK2 keeps its original repairs, optional discoveries and settings;
observatory progress writes LK3 with carriage position and a separate safe
terrace marker. Prior chapters still write LK2 until extended data is needed.
Completed cave saves retain their Cliff Dawn departure screen; Enter proceeds
to the observatory. Partial observatory saves resume there; completed ones
resume at Observatory Dawn. LK3 defaults tower progress to zero without changing
its settings. LK4 adds a tower checkpoint and a three-contact bitmask. Entry,
drive, main-light and boat departure are distinct milestones. A failed departure
write rolls back its completion flag and keeps the active scene available.
Partial tower saves resume there; main-light saves resume at the landing.
Completed Continue also revisits the landing. Save ownership, native envelope version and storage paths are unchanged. Save failures are visible. Keep the active
session open after a failure, and retry saving by interacting with the pump log or dry-ledge marker.

Linux: `${XDG_DATA_HOME:-~/.local/share}/Hazel/Games/cringlekaden.TheLastLightkeeper/slot-journey.save`.
Windows: `%LOCALAPPDATA%/Hazel/Games/cringlekaden.TheLastLightkeeper/slot-journey.save`.
`HAZEL_SAVE_ROOT` explicitly replaces the games root for isolated test profiles.
Brief authored turn, restoration and hazard cues supplement all visual feedback;
play remains possible when no audio device is available. These are real Kenney
sounds, not synthesized placeholder audio. Ambient/music direction remains future
content work.

## Release and packaging

```sh
python3 scripts/hazel.py build --config Release --tests
python3 scripts/hazel.py script-build examples/LastLightkeeper/LastLightkeeper.hproj --config Release
python3 scripts/hazel.py package --app Nutella --project examples/LastLightkeeper/LastLightkeeper.hproj --name LastLightkeeper --output dist/games
```

The archive is `dist/games/LastLightkeeper-linux-x86_64-Release.tar.gz` or
`dist/games/LastLightkeeper-windows-x86_64-Release.zip`. Extract it and run
`./Nutella` on Linux or `Nutella.exe` on Windows. The single adjacent .hproj is
automatically discovered. Its Assets, engine Resources, Mono runtime, native
closure, licenses, provenance, README and checksums travel with the game; no
source checkout, compiler, Python or SDK is needed to play. Require OpenGL 4.1,
Windows 10+ or an X11 Linux desktop; this work adds no macOS/Metal support.

## Art, provenance and authoring source

Art/sound: **Kenney**, CC0-1.0. Included pack licenses were read and preserved
under Assets/Licenses, with exact source pages and archive SHA-256 in
Assets/Licenses/PROVENANCE.json. No logo is used and no endorsement is implied.

- [Tiny Town 1.1](https://kenney.nl/assets/tiny-town): island, cottage, tower, tools.
- [Tiny Dungeon 1.0](https://kenney.nl/assets/tiny-dungeon): keeper/NPC poses, stone, lens and archive.
- [Tiny Factory 1.0](https://kenney.nl/assets/tiny-factory): connected pumps, pipes, relay machinery.
- [Tiny Wisptle 1.0](https://kenney.nl/assets/tiny-wisptle): water, shore and energy-path/endpoint feedback; no military props.
- [UI Pack - Pixel Adventure](https://kenney.nl/assets/ui-pack-pixel-adventure): small thin-outline HUD panels and lantern pulse. World mirrors use Tiny Dungeon art. Included license labels version 2.0; the source page's update listing says 1.0.
- [Interface Sounds 1.0](https://kenney.nl/assets/interface-sounds): selected cues, originals preserved plus PCM WAV conversions.

The Tiny family uses matching 16×16 sprites and palette. Sheets use nearest
minification/magnification, clamp and no mipmaps. One tile is one world unit. Shore/gravel use native 8×8 quadrant crops at half a world unit.
Tall pines use a native 16×32 region and matching scene aspect ratio.
Storm colors are renderer tints; warm restoration light remains readable against
blue-gray stone and sea. Kenney character poses are static, with game movement
bob; no directional walk animation is claimed. The lantern pulse is a real
sprite animation authored with native sheet services. No environment art was
procedurally drawn or replaced with checkerboard/primitive placeholders.

[Authoring/Asset-reference.png](Authoring/Asset-reference.png) assembles selected
actual assets; [Authoring/Island-reference.png](Authoring/Island-reference.png)
shows their placement. These are source reference collages, not runtime captures.
`Authoring/compose.py`, `Authoring/cliff_chapter.py` and `Authoring/observatory.py` record the authored composition and refuse to
replace existing scenes without `--overwrite`. It is never run by the game.
Further scene changes belong in Hazelnut. There is no runtime tile-map generator.

The initial sheets were created with the same native grid and save services used
by Hazelnut, exposed in the built HazelProject authoring tool:

```sh
bin/Debug-linux-x86_64/HazelProject/HazelProject slice-sheet examples/LastLightkeeper/Assets Art/tiny-town.hsprites Art/tiny-town.png 16
```

This refuses the existing sheet; the command documents how the initial file was
created. Equivalent imports cover the other Tiny atlases. The UI panel is sliced
at 4px for nine authored frame pieces. Two unmodified UI tiles are packed into
lantern-pulse.png; native `add-clip` records their animation. Do not regenerate
sheets or their identities during normal scene editing.

## Authoring responsiveness, verification and next chapter

Hazelnut used to format/validate the full scene as YAML during status/dirty checks
at least twice per frame. Exact authored-state comparisons replace that work;
save/load/Play validation remains intact. Project and scene opening present actual
loading frames, operation labels, entity totals where measurable and indeterminate
feedback during YAML parsing. The browser also normalizes restored folder paths.
See [revision findings](Authoring/REVISION-NOTES.md) and the validation record.

Focused tests cover LK1/LK2/LK3 migration and LK4 roundtrips, invalid/future retention, all
mirror and shutter states, fixed routes, tide warning/recovery, native chapter
transitions, wisp telegraph/dodge/repel, Mara's consequence, New Game/Continue and
Stop isolation. Packages audit sprites, audio, assemblies and license closure;
relocated startup runs without the source tree. Captures come from the actual
renderer/editor, not a drawn mockup or full-game screen-coordinate bot.

The upper tower, final storm encounter, morning landing, ending and credits are now authored.
Completed observatory saves depart from Observatory Dawn with Enter. Finished
Continue revisits the landing; the mooring can replay the ending.
The reusable runtime UI and Hazelnut workflow are designed in
[the staged completion plan](../../docs/design/last-lightkeeper-completion.md)
and remain a later integration milestone. Human pacing, control feel, repeated-play enjoyment and
physical Windows GPU/audio acceptance remain required. No full-game playtime is
claimed.

## Whole-game presentation pass

The existing routes, puzzle settings and LK1/LK2 save payloads are unchanged.
All gameplay locations share left-aligned HUD baselines, consistent panel insets
and a dedicated warning row. Menu button labels sit on the same vertical baseline.
Dialogue and pause controls use a fixed authored footer.
New Game confirmation waits for another N or click; Esc cancels, and selecting
Credits or Continue cancels the pending replacement. Save failure remains visible
until a successful retry; checkpoints only confirm an actual write. Chapter exits remain in the current
scene if writing fails, so session repairs can be saved on retry.

Joined gravel replaces the isolated road sprites. Court and cistern surfaces use
quiet slate instead of wall brick; trees use the complete native 16x32 pine region. Cistern and
cave water have directional stone kerbs. Cave rubble and salvage describe specific
falls and storage areas. Existing Kenney atlas pixels, sheet IDs and licenses are
preserved. The composition scripts retain these choices; the shipped scenes remain
ordinary Hazelnut documents. The observatory adds separate authored scenes without
regenerating these polished chapters or their trees.
