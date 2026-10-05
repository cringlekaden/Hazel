# Editor usability implementation: A1 / A2 and Stage B

Baseline: `5de70c93a89e1c8df3fd37553569d9499f793c7a` on
`feature/sprite-sheet-authoring`, including the complete audit and descended from
`9a48282f59b4cbc188615ec66763c448b70a2e83`. Implementation branch:
`feature/editor-usability`. This record supplements
[the audit](editor-usability-audit.md). A1/A2 and their corrections are complete;
Stage B is implemented below. C–H remain separate proposals.

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
retained; no new default arrangement overwrites user layouts. Automatic diagnostics do not steal keyboard focus; shortcut ownership is queried
after all panels draw. The viewport toolbar contains only runtime icons; File, Scene
and Project menus retain saving, entity creation, script builds and export. Colors and spacing are restrained;
renderer facts are read-only and disclosed under an expandable section.

## Verification and human acceptance

The branch adds bounded Linux/Windows Debug and Release CI, using two compiler
jobs and existing ownership, serialization, renderer and runtime regression
executables. Release runs the OpenGL 4.1 profile. The editor smoke checks failed
assignment saves, stale targets, rejected Open, active saves, tool-busy mutation
rejection and invalid physics simulation before Box2D initialization. Pure document
checks cover scopes, save failure/exception/cancel, partial success/retry, explicit
Use Saved retention, true close/discard and action availability. The existing short
render/startup/shutdown test exercises focused ImGui shortcut delivery and fresh-row
control width. The legacy standalone renderer fixture now requests the engine
minimum (4.1), uses GLSL 410 with equivalent explicit UBO binding, and retains all
render/readback assertions. Optional
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

![Selected-region authoring in the isolated smoke](images/editor-usability-a1-a2.png)

This capture replays the existing ImGui draw data into a test-owned framebuffer;
its controlled failed operation is intentional. It excludes OS decorations and
is not evidence of physical input or Windows DPI acceptance. No matched baseline
capture was made. Assignment now precedes detailed properties; region identity is
an expandable section. The UI font retains its existing glyph coverage (the
fixture filename's emoji uses a fallback glyph); UTF-8 file identity is preserved.

## Focused correction after Linux/Windows feedback

This correction starts from `9868b17` on `feature/editor-usability`, including
the completed guarded recovery-retry fix and its regression coverage.

The runtime toolbar restores Hazel's existing Play/Simulate/Stop/Pause/Step icons,
with Play as Resume while paused. It retains `##toolbar` and the current docks;
authoring actions remain in File/Scene/Project menus, without a More dropdown.
Icon and menu commands share `InvokeRuntime`, existing availability checks,
dirty-asset guards and runtime lifecycle. Active document/dirty/tool status remains
in the status row and native title.

Shared vector rows restore attached red X, green Y, blue Z reset buttons, with
purple W for generic four-component vectors. Rows use all axes horizontally when
there is room, then fewer axes per line to preserve readable numeric fields.
Narrow scaled controls remain left-labeled. Defaults come from owners; reset commits
intentionally, `Changed` reflects a value change, and no supplied default means
reset unavailable. Engine rotations remain radians; inspector values are degrees.
Sprite pivots use the same vector helper with their known center default.

**Hazel source SDK** means a prepared checkout containing `premake5.lua`,
`scripts/hazel.py`, the canonical internal authoring/packaging/child-tools/template
files and `scripts/internal/toolchain.json`, pinned Premake under
`build/tools/premake-core/bin/release`, and host Debug `Hazel-ScriptCore.dll` under
`bin/Debug-<platform>-x86_64/Hazel-ScriptCore`. It is used by current New Project,
Build Scripts and Export; native Create Script, content editing/saving and a
prepared project's Play do not need it. Open still has its existing asset/assembly
requirements; this correction does not implement uncompiled-project loading.

Native `HazelSDK` discovery has a fixed order:

1. Explicit absolute override; invalid/moved/incompatible overrides report an error
   and remain persisted until deliberately corrected or reset.
2. Optional `hazel_sdk` string in `build.json` beside the executable. Absolute paths
   stay absolute; relative locators resolve against that metadata directory. A
   malformed/stale locator reports a problem. Current runtime packages **omit** it
   and contain no SDK; metadata without a locator stops development-layout inference.
3. The exact development `bin/{Debug,Release,Dist}-<host>-x86_64/{Hazelnut,MigrationEditorSmoke}`
   executable layout. No cwd dependency, ancestor search or drive scan.

`SDKSelection` keeps Not configured/Missing/Incompatible/Ready, effective root,
source and diagnostic outside preferences. Premake supplies expected tool pins
from the existing canonical manifest; validation checks required files and matching
Premake/PyYAML pins. This is SDK-file readiness, not comprehensive managed ABI or
host compiler validation: existing loaders and canonical preflight retain those
checks. A new comprehensive compatibility stamp remains stage E. Workers revalidate
SDK inputs before commands. SDK discovery starts no Python process; legitimate
Python discovery/preflight/build/export keep their existing contracts.

Preferences explain the SDK, show effective root/source/status and offer Browse,
Validate / Refresh SDK, and Use Automatic. Reset changes only the draft; Apply and
Save explicitly persists a blank override, never an automatic machine path. Invalid
text stays editable; checks run on commit/browse/refresh/apply/action, not each frame.
If discovery fails, select a compatible source checkout (not the executable or
Resources folder): clone with `--recurse-submodules --branch feature/editor-usability`
(the current compatible authoring branch; master lacks these tools), install README prerequisites,
then run `scripts/setup.sh` or `scripts/setup.ps1`. Check Readiness verifies Python,
Mono/.NET targeting packs and, for export, native compiler prerequisites.

The editor regression covers native discovery with spaces/Unicode/unrelated cwd,
prepared/unprepared and incompatible files, explicit precedence, package locators,
no-SDK packages, reset/persistence scope and owner-defined axis resets. Runtime
commands retain lifecycle/paused-Step and dirty-document Cancel/Use Saved assertions.
The recovery retry regressions remain intact. A bounded ImGui contract draws wide
and 300-pixel, 125% vector forms; capture is inspection evidence, not physical
mouse, Windows DPI or hardware acceptance.

Local validation passed: Premake Debug/Release builds with two jobs; all fourteen
sequential regressions in Debug/software and Release/OpenGL 4.1; native HD4000
editor startup/render/shutdown, including the 1024×640 capture. Capture fixtures
use viewport-relative positions inside isolated resources/data; existing layout
and preview-cache assertions remain intact. Physical input/DPI acceptance is pending.

**Pending tooltip cleanup:** audit redundant label/value tooltips across panels in
a later presentation pass. Keep explanations of behavior, units, shortcuts,
consequences and disabled reasons. Show the full value/path only when its visible
presentation is clipped; do not repeat already readable text. Touched runtime/reset
controls follow this policy, and status identity now gets a tooltip only when clipped.
No project-wide tooltip rewrite is included here.

## Stage B: Console and tool-operation reporting

Baseline `871028315416385975459bbefdc05b607707447c` on the existing
`feature/editor-usability` branch includes the completed recovery retry fix,
SDK discovery, compact runtime toolbar, colored vectors and the newer Skybound
pipe correction. The unrelated local MeadowRun Lanterns sheet is preserved.
No dependency, layout template, example, SDK generation or schema changes are
part of Stage B.

- **View > Console** replaces Output. Only the old panel's window settings are
  migrated in memory if Console has no existing settings; dock geometry/order
  and all other layout entries remain intact. Initial placement uses the former
  Output/Stats dock. Errors never open/focus the Console automatically; the
  document/status row has a visible error/result link. Explicit View/status clicks
  reveal the Console tab; background failures do not request focus. Runtime icons
  are unchanged.
- **Messages:** Core, App, Managed, Authoring, tool stdout/stderr and tool status;
  severity/source filters, substring search, Reset filters, visible/hidden/drop
  counts, Ctrl-select/keyboard selection, context Copy message, Ctrl+C,
  Copy > selected/visible, Clear, Auto-scroll and Latest. Summary rows
  are clipped; selected details wrap full multiline text/paths. A narrow panel
  uses a compact source/severity/message row. Settings explains native capture
  versus display filtering; capture defaults to Info and saves only on request.
- **Operations:** immutable ID/project-generation/request snapshot, actual
  Python selection, SDK/command, real CLI child-command stages, elapsed/start/end
  times, outcome/exit code and accepted artifact location. Operation details offers
  message filtering, Copy result and Open output folder through the existing
  native abstraction. A build records its module; export records its destination.
  The latest failed/cancelled/timed-out result remains pinned until Dismiss failure,
  even after clearing messages, history eviction or later successes.
- **Limits:** 1 MiB/1,024 incoming records; 8 MiB/10,000 retained records;
  64 KiB UTF-8-safe individual records and partial lines; 20 operation records
  plus one pinned failure. Pump moves at most 256 records/256 KiB per frame.
  Oldest messages drop with explicit counters. Clear atomically removes accepted
  queued and retained messages and resets counters; new messages may arrive
  afterward. It does not cancel work, reset sequence IDs or erase operation results.
  Process final reports retain at most 2 MiB; result details preserve the final
  64 KiB so compiler failures at the end remain useful.
- **Lifetimes:** Hazel owns a permanent distribution sink, preserves stdout and
  any separately attached sinks, and offers scoped weak observers. Hazelnut's
  data-only session precedes Application construction and outlives its teardown.
  Observers detach after tool/watcher/script/renderer shutdown; Reset waits for
  in-flight bounded ingestion. No worker/sink calls ImGui or owns editor/scene
  pointers. Sink recursion is suppressed and collector locks never call logging.
- **Tools:** one worker, separate fair stdout/stderr reads, bounded drain passes,
  original interpreter/SDK/preflight contracts and argv execution. The canonical
  Python invocation explicitly enables UTF-8 text output on both platforms. Poll validates
  project path and generation before typed main-thread reload/open actions. A narrow
  Application main-thread callback keeps polling while minimized, without advancing
  scene/runtime/render updates; editor detach and Application teardown clear it. Stale
  completion is labeled Previous project. Normal close waits and offers Keep editor
  open. Internal forced teardown cancels, terminates the owned Linux process group /
  Windows job and joins; it never applies an editor action. Windows children inherit
  only their three standard handles. Python discovery's existing probe is bounded
  by five seconds; normal jobs retain the sixty-minute deadline. No unsafe
  interactive compiler/export cancellation button is introduced.

Focused checks exercise the actual relay, mixed sources/order, filtering/copy,
UTF-8 truncation, bounded inbox/Pump/retention/Clear, pinned completion across
truncation/history eviction, observer recursion/concurrent teardown, stdout/stderr,
continuous-output deadlines, consumer exceptions, failed/cancelled workers,
project/generation rejection and descendant cleanup on cancellation/shutdown.
Existing failed Open, Save-and-Assign, document-operation and runtime guard checks
remain intact. The real editor smoke checks Output-to-Console dock migration and completion
while minimized without advancing scene/render frames.
Local validation: Premake Linux Debug and Release builds with two compiler jobs;
all 14 canonical regressions in Debug/software and Release/gl41 passed. An isolated
software-rendered editor capture was inspected at 1024×640 with a 300-pixel Console
at 125% scale; controls wrap and messages use compact summaries with full details
on selection. This is not physical mouse/monitor or Windows DPI acceptance.
The existing Linux/Windows CI workflow is required for the pushed revision.

Hands-on Linux/Windows acceptance: open Console from View; filter/search and copy
multiline diagnostics; scroll upward during a build and use Latest; make a script
compile fail while Console is hidden and verify the status link does not steal a
text field's focus; inspect the failure, Clear messages and build successfully,
then confirm the old failure remains until dismissed. Export and open its output
folder; request close during a job and choose Keep editor open. Check a narrow
Console and scaled Windows UI. These are acceptance tasks, not claims of physical
mouse/DPI verification.

**Stage B limits:** existing NativeLog calls and managed exception logs are tagged;
arbitrary C# Console.Out/Error remains stdout and is not advertised as captured.
A public managed logging facade/domain-scoped redirection, optional rotating native
files and validated compiler file/line navigation remain later Console refinements.
All messages/result details can be copied, and artifact folders can be opened now.
Broad redundant-tooltip cleanup remains explicitly pending as recorded above.

## Deferred findings

Unified recovery/schema preservation, workspace restoration,
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
