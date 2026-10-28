# Authoring reliability contracts

Current reference for ownership and failure behavior, including later prefab/history cleanup. [Editor guide](editor-guide.md), [Scripting](scripting.md), [Settings/resources](settings-and-resources.md) and [Packaging](packaging.md) explain the workflows. Historical verification below describes its named fixtures/checkpoints rather than physical acceptance.

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

## Authored entity references and detached prefabs

One narrow native visitor enumerates authored cross-entity fields for subtree
remapping and prefab ownership checks: screen focus/actions/owners; control
indicators/groups/owners; progress, slider and scroll parts; dropdown trigger/popup;
tab page **and initial focus**; tooltip body; and stored script Entity fields.
Hierarchy relationships keep their own graph validation. Runtime state, queued
events/captures, resources and numeric script fields are not authored references.

Duplicate remaps targets inside its subtree once and retains allowed same-scene
external targets. Detached extraction/prefabs reject nonzero external references
before publication; they never clear them or bind them to unrelated destination
entities. Zero is ownership-neutral; required/optional and component/role rules
remain in control validation. Native metadata audit owns scene/prefab schema,
hierarchy and prefab-reference ownership. Python packaging consumes its dependency
closure and retains package inventory/exclusions, filesystem/deployment checks and
managed assembly/class closure; it does not carry a parallel UI reference list.

Deletion/history retains repairable incoming references while targets are absent.
Undo restores the authored UUIDs with fresh lifetimes and preserves surviving
owners' values. Interleaved property/structural history still uses explicit exact
snapshots, payloads and native-write guards; the visitor does not serialize history,
retarget incoming references or weaken conflict detection. Authored formats and
detached-instance semantics are unchanged.

## Save behavior

Scene and project saves reserve an exclusive sibling temporary, check writing,
flushing and closing, then replace the destination with Linux rename or Windows
`MoveFileExW(MOVEFILE_REPLACE_EXISTING)`. Failures leave the prior destination in
place and attempt to remove only the temporary owned by that save. The
destination is never deleted first. No fsync/FlushFileBuffers or power-loss
durability guarantee is implemented. Concurrent edits by another writer are not
detected; saving is still a single editor command.

## Verification scope

The existing SceneSmoke, ProjectPhysicsSmoke and actual EditorLayer EditorSmoke
contain the regressions. EditorSmoke starts with the F5 project argument
`tests/fixtures/AuthoringProject/Authoring.hproj` in an isolated test environment, calls the same
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

Current runtime/project deployment is covered by [Packaging](packaging.md); the [Nutella milestone](nutella-runtime.md) retains historical evidence.
Current `.hfont` ownership/reload behavior is documented in [runtime UI resources](runtime-game-ui.md#font-families-and-declared-coverage).
macOS/ARM64, Metal and larger feature systems remain deferred.
