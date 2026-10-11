# Runtime progress and audio

These additions retain RuntimeSession/Scene ownership, Premake, Mono and the
OpenGL renderer. No vendor submodule revision was changed.

## Durable progress

A project may set an optional `Project.SaveNamespace` in `.hproj` or Hazelnut's
Project Settings. Choose a stable unique identity such as
`cringlekaden.TheLastLightkeeper`, independent of the project display name,
installation folder or build configuration. Empty disables save slots. Changing
it intentionally selects another save history. Portable names reject traversal,
Windows device names, trailing periods and more than 96 characters.

`Hazel.SaveData.Read("journey")` returns an empty string for no slot.
`SaveData.Write("journey", payload)` writes 1–65536 UTF-8 bytes without NUL.
The game owns its payload schema, version/migration and recovery UI. The native
YAML envelope is separately versioned (`Version: 1`, owner, slot and payload).
Wrong ownership, malformed/future versions, oversize files and I/O failures throw
managed InvalidOperationException; they never silently reset the file.

Nutella opts into persistence before starting its RuntimeSession. Default editor
Play uses an empty, session-owned overlay, persists across its scene transitions,
and clears on Stop. It never reads or writes player saves. Session hosts can
explicitly choose persistence before Start; no editor-specific dependency exists
in engine storage. Main-thread active runtime required.

Player slots live at the platform user-data root / `Hazel/Games/<namespace>/`.
Linux: `${XDG_DATA_HOME:-~/.local/share}/Hazel/Games/...`; Windows:
`%LOCALAPPDATA%/Hazel/Games/...`. Files are named `slot-<name>.save` and contain no
authored scene data. `HAZEL_SAVE_ROOT` explicitly replaces the games root for
isolated tests or portable profiles; it does not alter Assets or editor settings.

Writes create a checked exclusive sibling temporary and atomically replace the
slot using the existing Linux/Windows FileSystem implementation. Failed writes
leave the prior file and successful overlay intact. This prevents partial-file
publication; it does not promise fsync/power-loss durability or concurrent-window
conflict resolution. Game code should save at meaningful progress boundaries,
report failure visibly and require explicit confirmation before replacing an
unreadable/newer journey. Version and corrupt-file tests use private directories.

## Bounded audio

Add an **Audio Source** component in Hazelnut. Set its project-relative WAV Clip,
Gain (0–1), Loop and Play on start. The Inspector exposes these authored settings;
scenes and detached prefabs preserve them. Managed
`entity.GetComponent<AudioSourceComponent>().Play()` restarts that source;
`Stop()` releases it. A scene owns at most 32 live source voices, collects ended
voices before new playback, and releases all voices/device ownership on Stop or
transition. Destroy/remove source also releases its voice. Audio callbacks never
enter Hazel/Mono; all controls stay on the runtime thread. No audio plays in Edit
or physics-only Simulate. Pause/dialogue behavior is game-owned; brief game cues
are intentionally allowed to finish.

The first implementation accepts PCM WAV files. Asset ownership/schema and
native CPU decode/length are checked during export, including after relocation.
Missing/bad clips raise errors; unavailable devices log a warning and return
false so a game with equivalent visual cues can continue silently. A future
streaming/spatial/mixer system is outside this milestone.

miniaudio 0.11.22 is pinned to commit
`350784a9467a79d0fa65802132668e5afbcf3777` with source/license SHA-256 in
`Hazel/vendor/miniaudio/PROVENANCE.json`. The unmodified header is compiled once
in the existing Hazel library, uses native Windows/Linux device backends and
introduces no package DLL dependency. Both offered license texts are preserved;
this project uses the MIT alternative. Canonical packaging copies the vendor
license. Kenney Interface Sounds' originals and licenses remain in the game;
selected cues were converted with FFmpeg into mono 22050Hz PCM s16le WAV, without
synthesis. The FFmpeg executable is an authoring tool and is not distributed.

## Large authored scenes

Sprite-region reads now validate the existing Source mapping directly. Assigning
its fields into temporary YAML maps caused yaml-cpp to merge the whole source
arena once per sprite, with quadratic loading cost. The fix keeps the same strict
keys, references and format; vendor code/pins remain untouched.

Local transform edits validate the affected subtree's world matrices, physics
and primary-camera inverse before commit, instead of checking every unrelated
terrain tile for every camera/actor/effect change. Load, copy and topology edits
still run full graph validation. Descendant-camera singularity and unchanged
failed-edit regression coverage protect the original transactional contract.

## Native sheet authoring commands

`HazelProject slice-sheet Assets-root Art/name.hsprites Art/image.png 16` creates
an editable nearest-filtered grid sheet using the same GenerateGridPreview,
AddGridRegions and SaveSpriteSheet services as Hazelnut. It refuses existing
files, retaining authored region identity.

`HazelProject add-clip Assets-root Art/name.hsprites "Clip name" 0.65 0,1` adds a
loop using zero-based region ordinals, a common positive frame duration and a
new stable clip identity. It rejects an existing clip name. Further timing,
slicing, renaming and reference edits use Hazelnut's Sprite Sheet panel. These
commands perform native ownership/image/schema validation without a graphics
context. They are authoring aids; packaged games do not execute them.
