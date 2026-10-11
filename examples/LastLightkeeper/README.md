# The Last Lightkeeper

An original top-down adventure in development for Hazel. Keeper Iona returns to a
storm-damaged island to restore its lower lighthouse signal and bring mechanic
Mara home. This project begins the planned 30–45-minute game; it contains the
first connected location, **The Breakwater**, rather than the full adventure.

The slice's 8–12-minute pacing target needs timed human playtesting. It includes
an illustrated title/continue/new journey menu, harbor reflection tutorial and
bridge, salt grove exploration, Orrin's conversations and Mara's notes, a wrench
and sluice repair, a telegraphed tide crossing with a safe alcove, recovered lens,
three-mirror relay puzzle with visible beam/stone occlusion, lighthouse ignition
and a chapter endpoint. An optional archive and sea-glass cache reward exploring.
There is no enemy combat in this chapter; the tide is its meaningful hazard.

## Play and edit

From a prepared checkout, with the native applications built:

```sh
python3 scripts/hazel.py script-build examples/LastLightkeeper/LastLightkeeper.hproj --config Debug
python3 scripts/hazel.py run Nutella --config Debug --project examples/LastLightkeeper/LastLightkeeper.hproj
python3 scripts/hazel.py run Hazelnut --config Debug --project examples/LastLightkeeper/LastLightkeeper.hproj
```

Use `python` on Windows. In Hazelnut, open Assets/Scenes/Breakwater.hazel and use
Play; all terrain, paths, buildings, occluders, interaction landmarks, HUD and
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
| Q | Journal / close journal |
| Escape | Close dialogue or pause |
| Enter while paused | Resume |
| M while paused | Return to title and saved progress |
| Enter / left click on title | Begin / continue |
| N / title New button | New journey; repeat to confirm replacing existing data |
| C / title Credits button | Credits and asset thanks |

Stay clear of the tide causeway until the pump is repaired. Blue means clear;
amber warns two seconds before the three-second surge. Higher ground at the
causeway's eastern alcove is always safe. A surge returns Iona to the safe
checkpoint and retains completed repairs. The journal records the current goal
and directions. The beam itself explains reflection; turn one mirror at a time.

## Progress and audio

Nutella saves milestone progress and mirror orientation under the stable project
namespace `cringlekaden.TheLastLightkeeper`. It restores safe checkpoints, rather
than arbitrary positions that could trap the player. Editor Play starts a separate
practice journey and clears it on Stop, without touching player data. Missing,
corrupt and newer saves are distinguished; the original file is retained until
an explicit confirmed new journey. Save failures are visible. Keep the active
session open after a failure, and retry saving by interacting with the pump log.

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
- [Tiny Battle 1.0](https://kenney.nl/assets/tiny-battle): water and shore subset only.
- [UI Pack - Pixel Adventure](https://kenney.nl/assets/ui-pack-pixel-adventure): small thin-outline panels, mirrors, light and icons. Included license labels version 2.0; the source page's update listing says 1.0.
- [Interface Sounds 1.0](https://kenney.nl/assets/interface-sounds): selected cues, originals preserved plus PCM WAV conversions.

The Tiny family uses matching 16×16 sprites and palette. Sheets use nearest
minification/magnification, clamp and no mipmaps. One tile is one world unit. Tall pines use a native 16×32 region and matching scene aspect ratio.
Storm colors are renderer tints; warm restoration light remains readable against
blue-gray stone and sea. Kenney character poses are static, with game movement
bob; no directional walk animation is claimed. The lantern pulse is a real
sprite animation authored with native sheet services. No environment art was
procedurally drawn or replaced with checkerboard/primitive placeholders.

[Authoring/Asset-reference.png](Authoring/Asset-reference.png) assembles selected
actual assets; [Authoring/Island-reference.png](Authoring/Island-reference.png)
shows their placement. These are source reference collages, not runtime captures.
`Authoring/compose.py` records the initial authored composition and refuses to
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

## Verification and next chapter

See the final validation record in Authoring/VALIDATION.md. Shared game tests
cover restoration order/idempotence, progress schema, all eight mirror states,
beam obstruction/cycles, tide warnings and the safe alcove. Native regressions
cover actual storage atomic replacement/corruption/isolation, hierarchy and
lifecycle. Export audits decode referenced sprites/WAVs and validate closure.
Application tests cover startup, rendered frames, resizing and shutdown, rather
than a coordinate-guessing full-game bot.

Next content milestone: the cliff cistern and sea caves, a lantern upgrade,
movable shutters, one distinct enemy encounter, and Mara's reunion. Then the
observatory, lighthouse ascent, storm-heart encounter and rescue ending. Full
playtime, human pacing and physical Windows GPU/audio acceptance remain required
before claiming the finished adventure.
