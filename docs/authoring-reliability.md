# Authoring reliability contracts

These repairs build on master baseline `00aca3c` (migration merge `257bd58`),
on `fix/authoring-reliability`. They are a review checkpoint, not another master
merge. Dependency pins, Sandbox examples and local VS Code configuration stay
unchanged. Original migration verification records remain historical evidence.

## Texture references

File textures expose their resolved UTF-8 load location through `GetPath()`.
Scene serialization derives project-owned references relative to the project's
asset directory, for example `Textures/texture é 🚀.png`. Loading joins that
directory once. The content browser still supplies a usable filesystem path to
the assignment helper; it does not prescribe the serialized reference.

Existing asset-relative Windows separators are normalized on both platforms.
Existing absolute paths remain supported on their native host. Absolute paths
inside the current asset root are rewritten relative on save; external paths
remain absolute and require the external file to remain available after moving
the project. External references are not copied, discarded, or turned into an
asset database. Windows drive paths on Linux report that an explicit local
replacement is needed; no drive mapping is guessed. Previously broken
references containing an extra project/asset-directory prefix need that prefix
removed explicitly; there is no ambiguous cwd fallback.

## Authored fields and script replacement

Each Scene owns authored script fields by entity UUID. Scene copies and entity
duplicates copy the field values independently. Destroying entities erases their
fields; destroying scenes releases the entire map. Serialized field names,
types and data do not own Mono metadata and can be loaded before a domain exists.
Missing classes and removed fields retain their serialized data.

Live reload snapshots are separate from authored defaults. Reload restores
matching runtime fields; Stop clears runtime snapshots and a new Play starts
from authored fields. Old managed handles and reflected metadata are invalidated
at domain replacement and shutdown. The public managed API and scene YAML format
are unchanged.

Project open stages its configuration, start scene, textures, content browser,
script domain/reflection and watcher before replacing the valid session. Once
validation succeeds, the old scene stops while its old domain is still alive.
Scene open stages before stopping Play/Simulate. Expected file, YAML and assembly
errors return failure and appear in editor Stats/log output; unexpected failures
are allowed to propagate. Cancelling a dialog returns false.

Hover/selection and legacy borrowed observations are cleared at scene replacement
and teardown. Selection checks scene identity before inspecting registry validity;
`Entity::operator bool` still requires a live owning Scene.

## Save behavior

Scene and project saves reserve an exclusive sibling temporary, check writing,
flushing and closing, then replace the destination with Linux rename or Windows
`MoveFileExW(MOVEFILE_REPLACE_EXISTING)`. Failures leave the prior destination in
place and attempt to remove only the temporary owned by that save. The
destination is never deleted first. No fsync/FlushFileBuffers or power-loss
durability guarantee is implemented. Concurrent edits by another writer are not
detected; saving is still a single editor command.

## Focused acceptance

The existing SceneSmoke, ProjectPhysicsSmoke and actual EditorLayer EditorSmoke
contain the regressions. EditorSmoke starts with the F5 project argument
`SandboxProject/Sandbox.hproj` in an isolated copy of Hazelnut, calls the same
assignment helper as drag/drop, saves/reopens, and opens a relocated project with
the original project unavailable. It checks external paths, legacy separators,
selection immediately after transitions, authored/live reload separation and
failed scene/project/reload/save recovery in Edit and Play. Safe-save checks
exercise partial writer failures, bad stream state, replacement rejection and
owned-only cleanup; Linux also exercises a kernel file-size-limit write failure,
and Windows a destination opened without delete sharing.

Physical drag/drop, native dialogs and sustained interactive acceptance remain
manual. Check a textured scene with spaces/Unicode in its paths; save/reopen and
relocate it; change a live script value, reload and Stop/restart; select immediately
after New/Open/Play/Simulate/Stop; try a missing/corrupt project, scene and assembly;
and try saving to an inaccessible destination. Prior valid work must remain usable.

Standalone runtime/project deployment, custom font persistence, live component
synchronization, macOS/ARM64, Metal and larger feature systems remain deferred.
