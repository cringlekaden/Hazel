# Editor usability A1 / A2 implementation

Baseline: `5de70c93a89e1c8df3fd37553569d9499f793c7a` on
`feature/sprite-sheet-authoring`, including the complete audit and descended from
`9a48282f59b4cbc188615ec66763c448b70a2e83`. Implementation branch:
`feature/editor-usability`. This record supplements
[the audit](editor-usability-audit.md); later stages remain proposals.

## Workflows

- **Properties:** select an entity in the flat Scene Hierarchy. Labels sit left of
  controls, vector axis buttons reset owner-supplied defaults, and Tab / keyboard
  editing use ImGui navigation. Angles display degrees but remain radians in
  components. Script fields without overrides explicitly show an unknown C#
  default; creating a zero/unassigned override is deliberate. “Use C# default”
  removes the override, without pretending the constructor was evaluated.
- **Assets:** Content Browser always exposes Import Texture, Search / Clear,
  type filter, breadcrumbs and Refresh. Search includes descendant assets;
  unfiltered browsing follows the current folder. Entries sort folders first,
  then case-insensitive paths with a deterministic tie-break. Click selects;
  double-click or Enter opens. Asset inspection retains the scene entity as the
  assignment target. Existing validated drag payloads continue to work.
- **Import:** choose the image, project destination and optional sheet path.
  If copying succeeds but sheet creation fails, the dialog says the image was
  imported, preserves its destination, and offers Retry Sheet Creation or
  Finish with Imported Texture. Retrying does not copy the image again.
- **Sprites:** open a sheet, select/name a region, edit bounds/pivot and sampling.
  Regions and animations have separate searchable lists. On Animations, choose
  the region in “Add frame”; each frame has its own region chooser, duration
  in seconds and reorder/remove actions. Preview transport and scrub operate
  independently of gameplay. Select Lantern in Scene Hierarchy: assignment
  says “Assign Region to Lantern” / “Assign Clip to Lantern”, or “Save Sheet and
  Assign …” when dirty. Saving must succeed before asset resolution and assignment;
  the captured scene/entity identity is revalidated. Sprite/clip inspectors offer
  Open Sheet and Reveal Asset. Invalid path text remains a draft; resolution is
  triggered by commit, explicit refresh/selection, browse or drop.
- **Save:** focus Viewport / Scene Hierarchy / Properties for scene Save, Prefab
  Inspector for prefab Save, or the sheet window for sheet Save. Ctrl+S and File
  > Save use the retained active document; the shell displays its name and `*`.
  Ctrl+Shift+S / Save As is scene-only under existing serialization contracts.
  Prefabs and sheets save in place; copying with new identities is deferred.
  Ctrl+Alt+S / Save All lists dirty documents and independently saves them.
  Partial failures and cancelled Save As identify each outcome, preserve failed
  drafts, and retry only documents that remain dirty.
- **Play / Simulate:** the current scene draft runs in a copy. Open prefab/sheet
  drafts are conservatively listed because scripts can select assets dynamically.
  Save Assets and Play / Simulate saves them first; Use Saved Assets keeps their
  drafts intact. Authored mutations are disabled during runtime. Saving the
  retained editor scene remains available. Stop returns to its retained selection.
- **Export:** Save All and Export or Export Saved Files (Keep Drafts) is explicit.
  Export never calls a discard path. Tool jobs protect scene and asset mutation,
  saving, replacement, script editing/reload and new tool jobs. Browsing and
  independent previews remain available. Apply-time checks share the same policy
  as menus, buttons, shortcuts and drop handlers.
- **Open / Close:** guards list only affected document types. Discard applies
  after successful replacement/close; failed or cancelled Open retains the old
  valid session and drafts. Closing a document actually releases it. Hiding a
  sheet window preserves its document; Window > Sprite Sheet reveals it again.
  Existing repair/schema behavior is retained for the later recovery milestone.

## Boundaries and ownership

`UI/PropertyUI` provides responsive two-column tables, stable explicit property
keys under owner identity scopes, tooltips, inline validation, disabled reasons,
read-only/reference presentation, resets and changed/committed results. Controls
contain no services, serialization, logging or writes. Discrete selections/reset
commit immediately; continuous/text edits commit on deactivation. Vector controls
stack axes in narrow columns. Owners hold picker drafts/caches and reset them on
entity, project, document, reference or asset-epoch changes.

`Authoring/EditorActions` owns action availability. `EditorDocuments` owns bounded
Scene/Prefab/Sheet guard intent, identities and per-file results; it has no ImGui
or I/O. `DocumentController`, `DocumentUI` and `EditorShell` contain controller,
modal and shell responsibilities respectively. Existing asset, prefab, tool and
atomic-file services retain their contracts.

In-memory dirty snapshots preserve authored path spelling without filesystem
canonicalization; saved-file serialization still uses its existing portable-path
contract. Project file-choice scans and recent-project existence checks occur on
explicit refresh/open, rather than every draw. No asset database was introduced.

Native decorations and OS behavior remain. A narrow Window title API publishes
project/scene identity on Linux/Windows. Existing docking IDs and layout files are
retained; no new default arrangement overwrites user layouts. Secondary toolbar
actions overflow under More and remain in menus. Colors and spacing are restrained;
renderer facts are read-only and disclosed under an expandable section.

## Verification and human acceptance

The branch adds bounded Linux/Windows Debug and Release CI, using two compiler
jobs and existing ownership, serialization, renderer and runtime regression
executables. Release runs the OpenGL 4.1 profile. The editor smoke checks failed
assignment saves, stale targets, rejected Open, active saves, tool-busy mutation
rejection and invalid physics simulation before Box2D initialization. Pure document
checks cover scopes, save failure/exception/cancel, partial success/retry, explicit
Use Saved retention, true close/discard and action availability. The existing short
render/startup/shutdown test exercises focused ImGui shortcut delivery. Optional
`HAZEL_EDITOR_CAPTURE` writes a screenshot from that smoke without image assertions.
The milestone workflow does not invoke automated game playthroughs or pixel-click
scripts. It retains existing service assertions.

Human acceptance remains necessary; automated evidence does not establish physical
mouse/keyboard usability, Windows DPI or native dialogs:

- Edit each component/script field with mouse and keyboard; reset defaults; type
  invalid reference drafts, then correct them. Check radians and C# default semantics.
- Select rows, clear by clicking only blank list space, type W/E/R/Delete in fields
  without invoking commands. Inspect an asset while retaining the scene target.
- Search / clear / filter assets, navigate breadcrumbs, import a texture, open it;
  exercise partial sheet creation and retry.
- Slice/name/pivot the bundled Lanterns sheet; add frames using their own chooser,
  change duration, reorder/remove, preview and scrub. Assign to a named entity.
- Ctrl+S independently saves scene, prefab and sheet. Scene Save As cancellation
  retains the draft. Save All partial failure preserves unsaved documents and reports
  exactly which saves succeeded.
- Play / Stop and Export with dirty documents: Use Saved keeps drafts; Cancel does
  nothing; destructive Open discards only after a valid replacement. Failed Open
  preserves the prior valid scene and asset drafts.
- Attempt scene/prefab/sheet edits, assignment, reload and saves during a tool job;
  verify reasons and unchanged inputs. Browse/preview and completion remain discoverable.
- Resize narrow panels and test Windows DPI / native snapping; restart to verify
  the existing layout has survived. No new layout persistence behavior is claimed.

## Deferred findings

Console ingestion, unified recovery/schema preservation, workspace restoration,
native project generation, renderer policies and entity/prefab hierarchy remain
outside A1/A2. Existing prefab save conflict detection/exclusive creation limitations
belong to the recovery/asset-service stage; this milestone preserves those contracts
and rejects existing creation destinations at the action boundary. Save All is a
series of existing atomic file writes, not a multi-file transaction. Preferences
and project forms retain explicit Apply/Save rather than becoming document types.
Existing Python project creation still requires its current tool readiness.

Unrelated Skybound files, stashes, ignored VS Code configuration, user/resource
layouts and all recursively pinned vendor repositories must match their recorded
pre-task state. No merge into master or force push is part of this milestone.
