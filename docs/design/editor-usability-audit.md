# Hazelnut usability and authoring audit

Audit date: 2026-10-04. **Recommendation: first make the existing authoring workflow coherent; retain native window decorations; deliver hierarchy as a separate architectural milestone.** Everything below is a proposal unless explicitly identified as current source behavior. No features were implemented.

## 1. Baseline and preservation

Repository: [cringlekaden/Hazel](https://github.com/cringlekaden/Hazel). Audited checkout: `feature/sprite-sheet-authoring`, **`9a48282f59b4cbc188615ec66763c448b70a2e83`**, matching the locally recorded origin branch. The last implementation commit is `7e233d5`; the tip changes documentation only. No fetch was performed, so remote refs describe the local repository's recorded state.

| Recorded checkpoint | Relationship to audited HEAD / master |
| --- | --- |
| `master`, `7a0eec21a869a8521a5560cf8c5156343aa747e6` | Merged Nutella/runtime and earlier authoring reliability work. |
| `feature/example-games`, `45cac43e8b599af33cff4082ce5dd6d36e0e0db4` | Eight commits beyond master: MeadowRun/Skybound and supporting gameplay APIs; unmerged into master. |
| `feature/editor-authoring`, `86c36c1c927b60016466fe0bd8fce38a1c5b846f` | Twelve further commits: detached prefabs, creation/build/export tooling, preferences and lifecycle improvements; unmerged into master. This is HEAD's merge base with that branch. |
| `feature/sprite-sheet-authoring`, `9a48282…` | Seventeen further commits: native sheets, stable regions/clips, sampling, cache ownership, managed references, authoring UI and native export audit; unmerged into master. HEAD contains all preceding work: 37 commits beyond master. |

Pre-existing working changes, neither rewritten nor included in the design:

- `examples/Skybound/Skybound.hproj`: version/build identifier added and formatting changed.
- `examples/Skybound/Assets/Prefabs/LowerPipe.hprefab`: scene label/formatting and rotation changed. The rotation difference is substantive, not merely serialization formatting.
- `stash@{0}`: “Preserved user Hazelnut layout before authorized master migration merge”; retained without applying or dropping it.

Recursive vendor working trees were clean and remain untouched. Existing resource/user ImGui layouts, ignored `.vscode` files, preferences, branches, dependencies and commits are preserved. The only repository write is this document. Read-only inspection and reference downloads to `/tmp` were used; no editor launch, build, setup, GUI automation, merge or push was performed. This is source-based UX inspection, not a new human acceptance run. Prior validation in the design records is historical evidence, not a result rerun by this audit.

Read before proposing changes: [editor authoring](../editor-authoring.md), [authoring reliability](../authoring-reliability.md), [Nutella runtime](../nutella-runtime.md), [sprite-sheet design](sprite-sheet-authoring.md), and migration preservation/limitations records. Source takes precedence where those records describe intent or older behavior.

Historical local captures `build/testing/authoring-ui/preferences-small-screen-125.png` and `prefab-dirty.png` were also viewed without modification. They visibly show wrapped settings-path prose crowding the small form and prefab controls mixing left/right labels. These corroborate unchanged drawing patterns in source, but are earlier authoring evidence, not newly captured sprite-milestone screenshots.

## 2. Public references: observations and interpretation

Research inspected Hazel's public site and directly viewed its editor screenshot, plus official Epic documentation and downloaded public illustrations. Epic currently labels the retrieved documentation **Unreal Engine 5.8**; some illustrations clearly depict older versions. A current documentation URL does not establish that every screenshot is newly captured. No private Big/3D Hazel source was accessed. Its renderer, scripting and graph architecture are not evidence for this checkout.

| Reference actually used | Direct observation / documented behavior | Interpretation for this Hazel fork |
| --- | --- | --- |
| [Hazel home](https://hazelengine.com/) and [public Hazelnut 2023.2 screenshot](https://hazelengine.com/images/Hazel-2023.2-Screenshot_huda087e3f95812a96d2373c8ea820d639_197486_753x548_resize_q90_h2_box.webp) | Visible compact application/menu strip, named scene, nested entity tree, grouped inspector with labels left of values, orange viewport selection, docked Content Browser/Log, animation graph beside viewport. The graph has colored connections, named nodes, aligned inputs and its own tools. | A coordinated visual language and nearby contextual actions reduce attention switching. This is an inference from composition. The image proves neither OS hit testing nor graph execution/serialization, save semantics or ownership. |
| [Unreal Editor Interface](https://dev.epicgames.com/documentation/en-us/unreal-engine/unreal-editor-interface), [Level Editor](https://dev.epicgames.com/documentation/en-us/unreal-engine/level-editor-in-unreal-engine), [viewed editor composition](https://d1iv7db44yhgxn.cloudfront.net/documentation/images/fa122be2-b098-4ff7-87e8-32ab634916d9/hero-image.png), [Toolbar](https://dev.epicgames.com/documentation/unreal-engine/level-editor-toolbar-in-unreal-engine) | Viewed composition shows viewport tools, selected chair outline, aligned Details and searched Outliner. Documentation distinguishes application menu, common actions, viewport tools and bottom diagnostics; it describes a temporary Content Drawer that can be docked. The settings-menu image below also shows application menus and Play controls. | Separate global, document and viewport actions. Keep Hazel's docked browser initially; a second drawer is unnecessary. Do not import Unreal's many editor modes or platform deployment menus. |
| [Content Browser interface](https://dev.epicgames.com/documentation/en-us/unreal-engine/content-browser-interface-in-unreal-engine), [viewed annotated screenshot](https://d1iv7db44yhgxn.cloudfront.net/documentation/images/70c4db53-ac9f-44f0-845e-33464b63940a/ue5_1-content-browser-areas.png) | Visible Add/Import/Save All, breadcrumbs, folder tree, filters/search above assets, clear selected tile and item count; documentation explains navigation and view settings. | Give discovery and import persistent locations. Use a searchable list/tile browser over existing files; collections and an asset database are not prerequisites. |
| [Outliner](https://dev.epicgames.com/documentation/en-us/unreal-engine/outliner-in-unreal-engine), [viewed outliner image](https://d1iv7db44yhgxn.cloudfront.net/documentation/images/d4b9937d-3a48-4aac-b76c-19f2b3f3cf43/hero-image.png) | Image shows search, name/type columns and indented folder/entity rows. Documentation explicitly describes context menus, drag attachment, customizable columns and focus/selection navigation. | Keep search and scene selection simple now. Actual parenting must follow a scene relationship contract, rather than merely drawing indentations. |
| [Level Details](https://dev.epicgames.com/documentation/unreal-engine/level-editor-details-panel-in-unreal-engine), [Blueprint Details](https://dev.epicgames.com/documentation/en-us/unreal-engine/details-panel-in-the-blueprints-visual-scriting-editor-for-unreal-engine), [viewed Details illustration](https://d1iv7db44yhgxn.cloudfront.net/documentation/images/789bcfe1-9faa-49d9-9603-6ba57afa681e/blueprintdetails2.png) | Visible aligned labels/controls, search and collapsible categories. Level documentation describes default indicators/reset and conditional disabled properties. | Consistent rows matter more than decorative chrome. Provide reset and explain disabled actions; retain Hazel component terminology rather than adopting Actors/Blueprints. |
| [Project Settings](https://dev.epicgames.com/documentation/en-us/unreal-engine/project-settings-in-unreal-engine), [Editor Preferences](https://dev.epicgames.com/documentation/en-us/unreal-engine/unreal-editor-preferences), [viewed settings menu](https://d1iv7db44yhgxn.cloudfront.net/documentation/images/67ded7cf-dc69-47b5-91e0-6a372126360a/001-edit-menu.png) | Distinct project/editor settings entries are visible. Documentation describes searchable project categories and editor behavior preferences. | Show scope in every settings page. Use drafts with Apply/Save in Hazel rather than mechanically reproducing Unreal's persistence behavior. |
| [Animation Sequence Editor](https://dev.epicgames.com/documentation/en-us/unreal-engine/animation-sequence-editor-in-unreal-engine), [viewed annotated screenshot](https://d1iv7db44yhgxn.cloudfront.net/documentation/images/86e5de0c-12ee-472e-99f9-a8c301795aa6/animationsequenceeditoroverview.png) | Visible asset toolbar, property groups, central preview, timeline/transport and filtered asset browser. Documentation distinguishes asset properties from preview settings. | Put clip frames, timing and transport near the preview. Borrow organization, not skeletal animation features. A sprite frame strip is sufficient; a graph has no immediate workflow justification. |
| [Unreal output/bottom toolbar](https://dev.epicgames.com/documentation/unreal-engine/level-editor-toolbar-in-unreal-engine), [OutputLog settings API](https://dev.epicgames.com/documentation/unreal-engine/API/Developer/OutputLog/UOutputLogSettings), [Frontend documentation](https://dev.epicgames.com/documentation/en-us/unreal-engine/using-the-unreal-frontend-tool), [viewed older Frontend console](https://d1iv7db44yhgxn.cloudfront.net/documentation/images/7b1d02d2-ec5a-4710-a535-81fcfb525721/unrealfrontend_ui.png) | Current docs distinguish log access from command entry; API exposes timestamp/filter preferences. The older console image shows filtered records, source/time columns and copy/clear/save controls. | A single Hazel Console should collect diagnostics and tool results. The old image is visual evidence only; it is not proof of current Unreal ingestion internals. Hazel needs no command interpreter to deliver this panel. |

Coherence comes from repeated alignment, restrained density, persistent orientation, visible selection and tool placement by scope. Small icons become learnable when tooltips, text and menus expose the same action. Search reduces hunting, but an active filter must remain obvious so a beginner does not mistake hidden content for missing content. Dense expert workflows should share actions with discoverable buttons, not require a separate interaction model. Error/progress behavior is documented for some references; screenshots alone cannot establish it. These are original recommendations for this fork: no logo, proprietary art, copied graph skin or mechanically duplicated full layout.

## 3. Evidence-backed findings

Source links below are relative to this document; names/line numbers describe the audited commit.

| Evidence | Current behavior and concrete consequence |
| --- | --- |
| [EditorLayer](../../Hazelnut/src/EditorLayer.cpp), `OnImGuiRender` / `UI_Toolbar` (148/339) | Native application window plus fullscreen ImGui dock host; minimum dock width temporarily 370; unconditional Stats/Hierarchy/Properties/Viewport windows; separate icon-only Play/Simulate toolbar. Project/scene identity and dirty state are not integrated into the shell. Action errors also appear in Stats. |
| [SceneHierarchyPanel](../../Hazelnut/src/Panels/SceneHierarchyPanel.cpp), `OnImGuiRender` / `DrawEntityNode` (36/108) | Flat registry list; opening a node draws a duplicate dummy child with constant ID `9817239`. This visually implies a relationship that does not exist. Selection-clearing checks mouse-down/window-hover instead of a verified blank-space click: it can conflict with row selection. Confirm physically during implementation; service selection tests do not establish this click behavior. |
| [ContentBrowserPanel](../../Hazelnut/src/Panels/ContentBrowserPanel.cpp), `OnImGuiRender` (23) | Unsorted filesystem grid, generic icons, back/up button, no search/type filter/breadcrumbs. Import exists only in the background context menu. Clicking sheets/prefabs opens inspectors and clicking C# launches an external application; there is no coherent browser selection model. |
| [SpriteSheetPanel](../../Hazelnut/src/Panels/SpriteSheetPanel.cpp), `Regions` / `Clips` / `Render` | Functional grid/manual/pivot/clip tools, stable IDs, independent preview and guarded documents. Small fixed-height children and a 240–380 pixel controls column crowd timing/fields. Assignment uses the independently retained scene entity and requires saving the sheet; the button does not name its target. Region selection lives on one tab while “Add selected region” lives on another. |
| [AuthoringPanel](../../Hazelnut/src/Authoring/AuthoringPanel.cpp), `Shortcuts` / `Guard` / `Render` (275/78/938) | Ctrl+S saves a focused sheet, otherwise the scene; prefab focus is not handled. “Save and Continue” covers several documents but does not list them. “Discard and Continue” explicitly reloads only the sheet; retained scene/prefab drafts are not generally discarded. Export consequently can use disk while retained drafts still differ. The guard needs operation-specific document scope. |
| Same file, `Render`, `Prefabs`, `BindProject` | Sheet edit controls are disabled during tools, but prefab saves/properties and scene properties remain reachable. Assignment buttons sit outside some `editable` guards and callbacks check Edit mode rather than job state. Build/export therefore lacks one authoritative mutation policy despite the sheet-specific guard fixes. |
| [UI helper](../../Hazel/src/Hazel/UI/UI.h), [vector helper](../../Hazelnut/src/Panels/SceneHierarchyPanel.cpp), [SpriteWidgets](../../Hazelnut/src/Panels/SpriteWidgets.cpp) | Only scoped color in UI.h; vector rows use legacy Columns, other controls mostly show labels right or above. Sprite pickers own a static map keyed by ImGui ID, seed paths only when empty, and do asset resolution during drawing. Draft/error state can outlive panels or display a previous reference. |
| [EditorLayer open/save](../../Hazelnut/src/EditorLayer.cpp), `OpenProject` / `OpenScene` (568/641); [SceneSerializer](../../Hazel/src/Hazel/Scene/SceneSerializer.cpp), `DeserializeText` (439) | Candidate staging protects the previous session on normal file/resource/assembly failure. Repair only changes `PrepareSprites(!m_Repair)` to tolerate resolution errors. It does not parse corrupt YAML differently. Repair state/report is not retained as a first-class document condition. |
| [SceneSerializer](../../Hazel/src/Hazel/Scene/SceneSerializer.cpp), `SerializeText` / `DeserializeText`; [ProjectSerializer](../../Hazel/src/Hazel/Project/ProjectSerializer.cpp); [Prefab](../../Hazel/src/Hazel/Scene/Prefab.cpp) | Scene serialization emits `Untitled`; scene format has no general version gate and unrecognized component keys can be ignored. Prefab v1 checks allowed components and exactly one entity. Project accepts omitted Version as legacy, rejects explicit non-1 versions. Canonical reserialization can lose unrecognized scene/project data: automatic recovery must not build on that behavior unchecked. |
| [ProjectAssets](../../Hazel/src/Hazel/Assets/ProjectAssets.cpp), [SpriteSheetDocument](../../Hazel/src/Hazel/Assets/SpriteSheetDocument.cpp) | Project-owned immutable resolution resources, full sampling cache keys, epochs and explicit freshness boundaries are sound. Document detects disk changes; native import/create uses exclusive publication. Refresh invalidates globally, and source scans traverse all saved scenes/prefabs synchronously. These are bounded choices, not justification for a new asset database. |
| [EditorPreferences](../../Hazelnut/src/Authoring/EditorPreferences.cpp), [ImGuiLayer](../../Hazel/src/Hazel/ImGui/ImGuiLayer.cpp) | User preferences and 12 recents persist atomically. Corrupt/future preferences stay intact until explicit replacement. Existing user `imgui.ini` is loaded before the bundled seed and saved at detach. Window geometry, last scene/project restoration, panel visibility and browser preferences are absent as explicit session state. |
| [ProjectTools](../../Hazelnut/src/Authoring/ProjectTools.cpp), [process implementations](../../Hazel/src/Hazel/Utils/Process.h) | Workers receive paths/argv, use futures/shared mutex-protected progress, and join at destruction. Process trees are owned; output is bounded (256 KiB progress, 2 MiB result). Stdout/stderr currently share one pipe, losing source attribution. Continuous drain loops can starve timeout checks under sustained output; no such failure was reproduced here. |
| [Log](../../Hazel/src/Hazel/Core/Log.cpp), [ScriptGlue](../../Hazel/src/Hazel/Scripting/ScriptGlue.cpp), [ScriptEngine](../../Hazel/src/Hazel/Scripting/ScriptEngine.cpp) | Core/client spdlog loggers write stdout. Output is a separate tool/error string, not a log sink. Managed failures log through the engine; `NativeLog` uses `std::cout`, and general C# `Console.WriteLine` is not redirected to a panel. There is no public managed logging facade. No existing native rotating file sink was found. |
| [Python authoring](../../scripts/internal/authoring.py), [hazel.py](../../scripts/hazel.py) (160/240/378), [packaging](../../scripts/internal/packaging.py) | New Project stages and compiles before exclusive directory publication: missing tools prevent even content creation. Editor export calls broad `build('Release')`, which builds the solution/default make targets, stages both applications and explicitly builds SceneTransitions scripts before the selected project. |
| [Scene](../../Hazel/src/Hazel/Scene/Scene.cpp), [components](../../Hazel/src/Hazel/Scene/Components.h), [RuntimeSession](../../Hazel/src/Hazel/Scene/RuntimeSession.cpp), [managed Entity](../../Hazel-ScriptCore/Source/Hazel/Scene/Entity.cs) | Scene UUID maps, callback snapshots, deferred lifecycle, reset transient copies and runtime/editor isolation are foundations to retain. Transform is independent TRS; rendering/cameras/physics use it as world. Prefabs remap self references only. Hierarchical transform semantics cannot be added solely to the panel. |

## 4. Recommended visual and navigation direction

Use Hazel's existing dark theme and OpenSans, with a modest accent for selection/focus, consistent section headers and readable secondary text. Use text plus icons for primary actions; reserve warning/error colors for actual conditions. Use approximately one font-height row plus vertical padding; derive spacing from scaled font/style rather than scattered pixels. Favor legibility over increasing the already dense field count.

Keep the dock host, stable existing panel IDs and current user layout. Supply a coherent default only on first launch or **Window > Layout > Reset to Default** after confirmation. Reopening hidden panels restores their prior docks. Window menu uses checked `MenuItem`s for Hierarchy, Inspector, Content Browser, Viewport, Sprite Sheet, Prefab, Console and Statistics. Preserve `Properties`/`Output` layout sections through explicit one-time ID migration if renamed; do not overwrite `imgui.ini` with a new template.

Recommended default: narrow hierarchy left, viewport/asset editor center, inspector right, Content Browser/Console tabs below. This arrangement follows Hazel's current 2D authoring tasks and is freely dockable. Statistics moves to an optional panel; it stops acting as the error inbox.

| Scope | Explicit ImGui access / behavior |
| --- | --- |
| Application | Menu bar: File (New/Open/Recent, active Save/Save As, Save All, Exit), Edit (Preferences), Project (Settings, Create Script, Build/Reload Scripts, Export, Open Folder), Window and Help. Help menu shows keyboard reference and links to project documentation using the existing path/browser facility. Do not advertise Undo until implemented. |
| Main toolbar | Ordinary dock-host child row: Save with named active document, Save All, Add Entity dropdown, labeled Play/Stop, Simulate dropdown, Pause/Step when relevant, Build Scripts and Export. Narrow widths move secondary actions into `BeginPopup` overflow; menus remain available. Tooltips include shortcuts and prerequisites. |
| Viewport | Compact local row for Select/Move/Rotate/Scale, snap enabled/values and collider overlay; controls route to existing gizmo/overlay functions. Q/W/E/R work only in viewport context, outside text input/modal popups. Play captures gameplay input only with viewport focus. |
| Selection | Scene selection is `(scene identity, entity UUID)`; browser selection is an asset reference; sheet selection is `(sheet, kind, stable ID)`. Focus decides active document and Ctrl+S. Asset selection never silently changes the scene target. Inspector header states “Scene Entity: Lantern” or “Prefab Asset: LanternSeed”. Do not allow two contexts to masquerade as one selection. |
| Asset editing | Single-click selects an asset; Enter/double-click or context Open edits it. Keep one sheet and one detached prefab draft initially, using existing guards. Asset editors are named dockable windows, not a mandatory new multi-document framework. |
| Search | Hierarchy name/type filter, browser filename/type filter, inspector label filter, sheet region/clip name filter: `InputText` with clear button and result count. Keep filtered state visible; empty result says “No matches” and offers Clear. Initially search browser assets under Assets with a UI-owned index refreshed after operations or explicit Refresh, not recursive scanning every frame. |
| Feedback | Status row displays Edit/Play/Simulate, dirty-document count and latest operation. Clicking error count or operation opens Console with matching filter. Field errors remain inline and also become structured Console records; successful actions do not replace all earlier diagnostics. |

## 5. Main authoring workflows and layouts

| Task | Current friction | Proposed explicit ImGui workflow |
| --- | --- | --- |
| Create project | Tool preflight and initial compilation required before publication. | File > New Project: left-label Name, Identifier, Destination/Browse, starter-file summary; Create and Open generates content. Optional “Build scripts after creation” checkbox starts a separate job. Readiness is descriptive; generation is not disabled by missing compiler/Python. Native service is a later stage (§12). |
| Import texture | Hidden background context action followed by a second create popup. | Content Browser toolbar **Import Texture** opens native file picker; one modal has destination and “Create sprite sheet” checkbox (default on), proposed sheet path and Import/Open. Reuse native import/create validation. If sheet creation fails after image import, show that exact partial result and Retry/Create Sheet; do not claim both actions were atomic. Existing texture context Create Sheet remains. |
| Slice/name/pivot | Long controls, hidden selected region across tabs, arbitrary generated labels. | Sheet toolbar: Select, Rectangle, Pivot, Grid, Fit; Regions tab: searchable region list and left-label Name/Rect/Pivot rows, Center/Feet presets. Grid button opens settings child/popup with Preview, cell count and Apply. Keep append-only IDs and pixel snapping. Canvas selection mirrors region list; keyboard/numeric rectangle and pivot editing is an equivalent path. |
| Author animation | “Add selected region” depends on another tab; per-frame groups are tall. | Animation tab: clip list + Create; adjacent searchable region chooser and Add Frame; `BeginTable` frames with index, region, duration, Up/Down/Remove. Transport sits below preview with Play/Pause/Stop and Scrub. Selected frame is visibly highlighted. “Add current region” is optional and names that region; never a prerequisite. |
| Assign to entity | Retained target unspecified; sheet must be saved first; static and animation are easy to confuse. | Header action **Assign Region/Clip to Lantern** names retained scene target. When sheet dirty, **Save Sheet and Assign** calls save, resolves the ID, then applies to the still-valid captured target. Failure leaves draft/scene intact. No target: inline “Select an entity” plus hierarchy focus action. Typed drag/drop remains the expert path. Entity Inspector has Source type, Texture/Region chooser, Clip, Autoplay, Speed and Open Sheet/Reveal Asset buttons. |
| Configure scene | Inspector and project startup selection are scattered; no scene overview. | Viewport document menu **Scene Settings** opens a dockable page listing primary camera, script/asset readiness and validation results; primary camera chooser uses existing camera flags, ensuring one selected primary rather than adding a conflicting scene field. Entity properties remain in Inspector; Project Settings > General selects startup scene. |
| Play/Stop | Unlabeled icons; sheets are disk-backed while scene runs a copy. | Labeled Play explains “Runs current scene draft; referenced assets use saved content”. Prompt lists only relevant dirty asset drafts and offers Save and Play, Use Saved Assets, Cancel. Use Saved Assets leaves drafts untouched. Stop restores editor scene; no automatic runtime writes. Missing camera/script/asset blocks launch with named problems in Console. Simulate shares asset/physics validation but does not require managed classes. |
| Save | Ctrl+S varies by panel and omits prefabs; generic guard. | Ctrl+S/File Save targets active scene/sheet/prefab and reports its name. Ctrl+Shift+S/File Save As uses that document's supported copy/save operation, with unsupported cases explained. Save All lists scene/sheet/prefab/project drafts, saves only dirty documents, retains failed dirty states and reports partial completion. No claim of an atomic multi-file save. |
| Export | Unclear saved/draft boundary, tool details dominate UI; unrelated builds. | Project > Export modal: Project, Startup Scene (read-only/reveal), host target, Release, Destination, Name; named dirty-document guard **Save All and Export / Export Saved Files / Cancel**. Readiness shows actionable missing requirements. Start pins operation summary in Console; success offers Open Folder, Copy Artifact Path and run instructions. Export always validates saved dependency closure. |
| Run Nutella | Must know the distribution/descriptor relationship. | Completed export card explicitly says extract package, run Nutella; multiple projects require `--project`. Help/Copy Command uses the actual exported descriptor path. An optional later **Run Exported Game** button is enabled only for an extracted directory, launches absolute Nutella with structured arguments, and records launch result; archive extraction is never silently implied. |

Representative original mockups (documented intent, not screenshots of implemented UI):

```text
Native caption: MeadowRun — Meadow * — Hazelnut
File  Edit  Project  Window  Help
[Save Meadow*] [Save All] [Add Entity v]  [Play] [Simulate v]  [Build] [Export]
┌ Hierarchy ─────────┬ Meadow* / Lanterns.hsprites* ─────┬ Inspector ────────┐
│ Search [lantern x] │ [Move] [Rotate] [Scale] [Snap v]  │ Scene: Lantern    │
│ Lantern           │                                  │ Search properties │
│ Camera            │     viewport OR sheet editor     │ ▾ Transform       │
│ [Add Entity v]    │                                  │ Position [X Y Z] ↺│
│                   │                                  │ ▾ Sprite          │
│ flat until §13    │                                  │ Source   [Region] │
│                   │                                  │ Region   [Seed ▾] │
├───────────────────┴──────────────────────────────────┴───────────────────┤
│ Content Browser | Console       [Import] [Create v] Assets > Textures     │
│ Search [                 x] [Type v]   files / filtered diagnostic rows   │
└ Edit · 2 unsaved documents · Export completed [View result] ─────────────┘
```

```text
Sprite Sheet: Lanterns.hsprites *   [Save] [Reload] [Assign Clip to Lantern]
[Regions] [Animation] [Texture]      Saved asset required for scene assignment
Clip [Pulse ▾] [New]                Preview + transport, independent of Play
Name       [Pulse                 ] [Play] [Pause] [Stop]
Loop       [✓]                      Scrub [======|-----------] Frame 2/3
Add frame  [SeedBright ▾] [Add]     [Select] [Rectangle] [Pivot] [Grid] [Fit]
#  Region       Seconds  Actions    sheet canvas with selection/pivot overlay
1  SeedDim      [0.10]   ↑ ↓ ×
2  SeedBright   [0.20]   ↑ ↓ ×      Target: scene Lantern · entity UUID …
```

Closing a draft editor prompts Save/Discard/Cancel; hiding via Window menu leaves it in the dirty-document list. Show only task-relevant prerequisites. C# lifecycle explanations belong in Help/tooltips and generated source, not paragraphs occupying every creation panel.

## 6. Reusable ImGui property contracts

Existing ImGui header reports **1.89.4 WIP** and already provides tables, disabled scopes, keyboard navigation and edit-deactivation queries. Existing GLFW header reports **3.4.0**, regardless of older-looking submodule describe strings. Neither dependency needs changing for this work.

Place a small editor-specific drawing layer in proposed `Hazelnut/src/UI/PropertyUI.{h,cpp}`. Keep general scoped styling in `Hazel/UI/UI.h`; do not move editor models into Hazel's runtime API. Migrate the vector helper, then component/script fields, sprite sheet, prefab placement, project/preferences forms and browser view controls. Preserve standard ImGui widgets and existing payloads.

Suggested contract, illustrative rather than an implemented API:

```cpp
PropertyTable(id, availableWidth); // RAII BeginTable/EndTable, two columns
PropertyRow(propertyKey, visibleLabel, help, disabledReason);
EditResult { bool Changed; bool Committed; bool ResetRequested; };
// Wrappers: DragFloat/Int, Slider, Checkbox, Combo, Text, Vec2/3/4, ReadOnly.
// ReferenceRow draws presentation/chooser actions; returns an action to its owner.
```

Rules:

- **Columns:** fixed/resizable label column initially ~35%, clamped to 8–14 font-height units while reserving a usable control width; control column stretches. `AlignTextToFramePadding`, full-width ordinary controls, shared cell padding. Reserve trailing width for reset/browse/reveal before sizing the value. Labels remain left for checkbox, combo, numeric, vector, path, reference and read-only rows.
- **IDs:** outer document/project identity, entity UUID or stable asset ID, component key and explicit property key via `PushID`; widgets use `##value`, `##reset`, axis IDs, etc. Visible names/dirty markers never define identity. Draft frame rows get editor-only stable keys across reorder, not a new serialized frame schema. Scope IDs by document to avoid script/component collisions.
- **Small widths:** wrap long labels with full tooltip; stack vector axes vertically inside the control cell, then collapse optional button text into a popup. Use vertical scrolling, no horizontal hunt for Save. If fewer than ~20 font-height units are available, allow explicit compact form with label above only as the documented narrow-screen exception; keep left labels as normal behavior. Remove the blanket 370-pixel minimum. Do not squeeze values to unreadability.
- **Typography/grouping:** normal body font for rows, semibold section headings, secondary read-only info; unit suffixes such as degrees/seconds/pixels. Use `CollapsingHeader` and consistent spacing, not a separator for each scalar. Angles present degrees with explicit conversion in the owner; engine storage remains radians.
- **Defaults:** owner supplies value/default availability, never guessed by drawing code. Trailing reset button and row context Reset. Vector axis reset is supported with named tooltip. Script “Use C# default” clears the authored override; unset must read “C# default (value not evaluated)”, not display zero as the constructor value.
- **Validation:** owner supplies field severity/message. Inline wrapped message below control; tooltip explains units/range and unavailable action. `BeginDisabled` with adjacent readable reason and disabled-hover tooltip. Invalid text drafts remain editable without mutating accepted values. Helpers neither parse YAML nor resolve assets nor log errors while drawing.
- **Edits:** `Changed` fires on value changes for responsive previews; `Committed` fires on end of drag/text edit (`IsItemDeactivatedAfterEdit`/Enter), or immediately for accepted checkbox/combo/reset. Owner validates and accepts/rejects, marks dirty and performs invalidation at an appropriate boundary. Do not reload textures, write files or compile per keystroke. An invalid commit keeps draft text and error, not a substituted default. This contract can support undo later without requiring it now.
- **Keyboard/DPI:** Tab follows rows/axes/actions; Enter activates chooser, Escape closes popup or cancels its draft; preserve standard Ctrl-click numeric entry and focus highlight. Avoid stealing Q/W/E/R/Delete in text fields. Base metrics on font/style scale, rebuild font atlas at safe DPI changes, then rescale from an unscaled style baseline. Do not repeatedly compound scaling. Screen-reader support is not established by ImGui keyboard navigation; document that limitation rather than claiming full accessibility.

Reference widgets accept display data and return Browse/Clear/Reveal/Assign requests. Their panel/controller performs service calls and caches validated results. Panel-owned picker drafts reset on document/selection/reference changes and are pruned on close; replace the static `SpriteWidgets::States` lifetime without building a widget framework.

## 7. Custom title bar decision

**Retain native decorations for the first milestone.** Add project/scene/dirty identity to native title via a narrow Window API extension and mirror it inside the menu/toolbar header. Keep application menus and frequent actions in ImGui below it. This provides most of the orientation benefit with much less platform risk.

| Option | Benefit | Cost / implications |
| --- | --- | --- |
| Native frame + coordinated ImGui header | OS drag/resize/caption buttons; familiar maximize/system menu; clear title and tool grouping. | Extra vertical row; native appearance varies. Verify system menu because the retained GLFW exposes `GLFW_WIN32_KEYBOARD_MENU` and Hazel does not explicitly enable it. |
| Custom caption retaining appropriate native frame integration | More continuous presentation and room to combine identity/actions. | Dedicated platform integration, DPI/hit testing, keyboard/accessibility and detached-window behavior. ImGui buttons and drag deltas do not reproduce OS non-client behavior. Higher effort than property/browser cleanup. |

If reconsidered later, require these acceptance gates:

- **Windows:** retain resize borders, caption double-click, drag-to-restore, minimize/maximize/close, Alt+F4, Alt+Space/right-click system menu, taskbar interaction, Win+arrow/Win+Z and maximize-hover snap layouts. Distinguish interactive controls from draggable caption and resize hit regions. Use platform message integration, not per-frame window-position updates. Microsoft documents custom-frame hit testing and Windows 11 maximize hit regions: [DWM custom frames](https://learn.microsoft.com/en-us/windows/win32/dwm/customframe), [Snap layouts](https://learn.microsoft.com/en-us/windows/apps/desktop/modernize/ui/apply-snap-layout-menu).
- **Linux:** present build selects X11. Preserve WM move/resize, modifier drag, caption context menu, placement and decorations conventions across GNOME/KDE/tiling WMs. Future Wayland delegates placement and move/resize to the compositor; do not promise arbitrary global coordinates. Keep native/default decoration fallback. [GLFW window guide](https://www.glfw.org/docs/3.4/window_guide.html) describes platform differences; removing decoration does not supply a replacement resize implementation.
- **DPI/monitors:** hit testing uses correct screen/client coordinates and per-monitor scale, including negative origins. Preserve minimum/restore geometry and system work areas. Distinguish window units from framebuffer pixels. Detached ImGui windows must remain consistent and operable.
- **Accessibility/keyboard:** caption actions require focusable labeled equivalents; OS close routes through existing unsaved guard. Menus need a documented keyboard entry (for example F10) and standard shortcuts. Do not claim native accessibility when controls are custom-drawn.
- **Future macOS:** native traffic lights/titlebar, global menu and Cmd shortcuts should use a future Cocoa integration; do not bake Windows caption assumptions into editor panels. macOS build/runtime support remains deferred.

No custom-titlebar work is justified merely by the Hazel screenshot. Reassess after a cross-platform prototype proves the OS behaviors, without patching vendor repositories.

## 8. Unified normal Open and recovery

Keep normal Project/Scene file types, but eliminate “for Repair” commands. Both use one staged loading policy: parse known schema → validate authored data → resolve resources → assess run readiness → publish candidate. Return a structured result (`Ready`, `EditableWithProblems`, `NeedsDecision`, `Rejected`) with path/entity/property diagnostics. A bool plus a repair flag cannot express the required outcome.

| Problem | Normal Open behavior | Save/run behavior |
| --- | --- | --- |
| Missing texture/sheet/region/clip, parseable owning scene | Automatically open editable scene with exact unresolved references, valid entities/components and diagnostic placeholder. Show a nonmodal “Opened with problems” banner linking Console. | Known authored references may remain unresolved on save. Play/Simulate/export validate relevant assets strictly; no replacement guessing. |
| Malformed sheet metadata referenced by a valid scene | Preserve scene reference and original sheet bytes; show sheet diagnostic, but do not invent a blank sheet. | Open File/Open Folder/Copy Path/Retry in Console problem detail. Edit metadata externally or explicitly choose a valid reference. Rejected sheet save cannot rewrite it. |
| Corrupt scene/project YAML or invalid required structure/duplicate UUIDs | Reject candidate, retain previous session including current Play where staging has not committed; show precise location. | No partial scene interpretation or overwrite. Retry after external correction. Do not strip bad fields to obtain a load. |
| Unknown project/prefab/sheet version or unknown scene component/schema | Reject editable interpretation. Keep original bytes and prior session. Offer path/retry/help; raw read-only diagnostic view is acceptable. | No save through an older serializer. Add scene version/unknown-field handling before claiming safe automatic recovery. |
| Known omitted optional field / supported legacy encoding | Apply documented defaults or exact migration in memory; record changes. Existing whole-texture legacy paths and omitted project Version have known contracts. | Saving migration is explicit and visibly marked. Defaults never replace invalid required values. Preserve original before first migrated overwrite. |
| Missing script assembly/class | Allow content editing with “Scripts unavailable”; preserve authored class/fields even without reflection. No reuse of previous project's classes. | Build/Locate assembly normal actions. Scripted Play disabled until ready; content-only preview/Simulate stays available after appropriate validation. Nutella/export remain strict until their supported contract is met. |
| Missing startup scene / Assets directory | NeedsDecision during Open: Locate directory/scene or Open Project Workspace Without Scene when descriptor/root can be validated. | Never pick an arbitrary scene or modify descriptor automatically. Explicit choices update a draft; previous session remains if cancelled or unworkable. |

Safe recovery is automatic only when interpretation is unambiguous: retaining an unresolved UUID/path, choosing a specified schema default, displaying a placeholder, or applying a registered migration. Filename similarity, directory searches and matching names do not justify replacement.

ImGui workflow: File Open/Recent → ordinary native dialog → candidate evaluation. Editable problems open a banner and Console filtered to this document. A problem detail offers Select Affected Entity/Reveal Asset, Locate/Choose Reference, Retry, Copy Diagnostic or Open File. Locate opens a type-constrained picker, shows the proposed old→new reference and affected uses, and requires **Apply Reference Change**. Corrupt/future files never get an Apply that rewrites them. No repair-mode choice is required beforehand.

Attach load report, original-byte fingerprint, migration/recovery condition and dirty state to editor document ownership. Loading with missing assets alone is not a user edit. Known parseable repaired scenes save without discarding unresolved references; original snapshot is preserved before first repair/migration overwrite in machine-local recovery storage. Saving unknown/unrepresentable fields is blocked. Save As offers a new file; actual replacements are explicit and conflict-checked. Keep bounded recovery history with original path/hash and visible location.

Extend sheet-style disk conflict detection to scene/prefab/project documents. On conflict retain both draft and disk; modal offers Save Copy, Reopen after dirty guard, Cancel. No automatic YAML merge. Existing atomic sibling writes remain; they are not a power-loss durability guarantee. File/session staging must include diagnostics, browser, project assets and optional managed domain before the old session stops. Missing assembly is a readiness condition, while a present invalid assembly is reported explicitly; content opening must safely retire the previous project domain rather than borrowing it.

## 9. Renderer capability and settings matrix

Actual APIs: [RendererCapabilities/Settings](../../Hazel/src/Hazel/Renderer/RendererCapabilities.h), `Renderer::Init`, `Renderer/RenderCommand::GetCapabilities/GetSettings`, backend `Configure`, `Window::SetVSync/IsVSync`, and `Renderer2D::SetLineWidth`. OpenGL is the only implemented backend; None is not a user choice. Capabilities contain vendor/device/GL version string, texture/binding/size/attachment/draw-buffer/sample limits, line-width range, shader binary and debug-output flags. RendererSettings contains TextureSlots, PreferShaderBinaries and EnableDebugOutput.

| Proposed presentation/control | Location / scope / persistence | Default / supported range | Application and unsupported behavior |
| --- | --- | --- | --- |
| Backend, vendor/device, driver/GL version and all above limits | Preferences > Rendering > Detected Device; read-only, no persisted hardware facts | Actual queried values; `Driver` is GL_VERSION, not a separate OS driver package number | Update for current context. Copy Device Report button copies capabilities and effective policy to clipboard/Console. |
| Editor VSync checkbox | Preferences > Rendering; user `preferences.yaml` | On; bool | Live, on appropriate current window/context. `IsVSync` reports stored requested interval state, not proof of compositor/driver behavior; label that limitation. Detached ImGui windows need consistent swap policy and verification before claiming global application. |
| Runtime VSync request (later) | Project Settings > Runtime Rendering; portable descriptor | On; bool | Apply on Nutella launch; editor Play shares editor window and displays editor override. Show Runtime requested vs Editor current. Driver/compositor may override; no false measured-effective claim. |
| Shader loading combo: Automatic / GLSL compatibility | Preferences > Rendering > Advanced initially; later portable Runtime request | Automatic = PreferShaderBinaries true; false forces GLSL | Initialization only; restart required. Actual path = capability AND preference; 4.1/HD4000 4.2 uses shaderc/Cross→GLSL410. Automatic isn't a forced SPIR-V promise. |
| GL debug output checkbox | Preferences > Rendering > Advanced; editor user policy | Debug build on, Release off; bool | Current API initialization only; restart required. Effective = request AND core 4.3 loaded functions. Unsupported reads “Unavailable on this context; request retained”. For runtime, defer extra project debug toggle unless a shipping diagnostic use justifies it. |
| Texture batch slots integer (advanced) | Later Project Settings > Runtime Rendering; portable request, user editor override optional only if needed | 32 requested; valid request 2–32, including white slot | Initialization only. Effective min(request, 32, device texture units). Display requested/effective side by side and explain clamping. Reject below 2. Do not offer MaxQuads: 20,000 is compile-time, not a settings API. |
| Collider overlay checkbox | Viewport toolbar; user preference, mirrors existing ShowColliders | Off; bool | Live; editor visualization only. No project/runtime renderer field. |

Use left-label property rows; Advanced sections use `CollapsingHeader`. Pending restart/relaunch appears next to every initialization policy. Effective shader path is calculated from capabilities and preference because `GetSettings().PreferShaderBinaries` itself remains the preference; debug/slots have clamped values. VSync uses “Requested interval” rather than pretending hardware timing has been measured.

**Initialization dependency:** Application creates window/context and initializes renderer/Renderer2D before EditorLayer loads preferences/project. Hazelnut's specification currently supplies defaults; Nutella parses descriptor before constructing Application but does not populate Rendering. There is no general settings setter. Therefore do not simply persist project flags and claim they apply on project switching. Later introduce CPU prelaunch parsing and one shared policy validator: editor preferences before Application; Nutella project requests before Application; track requested and effective separately. Switching projects stages content with the existing renderer, shows pending runtime requests, and does not tear down GPU resources live. If applying project policy to the editor becomes desirable, require an explicit guarded relaunch. Avoid an implicit renderer restart during Open.

Framebuffer has Samples and implemented multisample allocation/resolve, but editor explicitly uses its default **1 sample**, Nutella uses its window path, and integer picking limits differ from color/depth limits. This is not a complete project MSAA policy or reason to expose a global MSAA slider. Defer end-to-end MSAA controls, HDR, color-management promises, backend switching and other unsupported quality knobs. Preserve bind-based OpenGL 4.1 and native HD4000 OpenGL 4.2 with the existing fallback paths.

## 10. One Console and its ownership

Replace Output's presentation with **Console**, retaining a layout migration. One diagnostic workflow: source producers → bounded records → Console filters/details → action. Stats remains performance-only. Recovery reports use the same Console with typed actions; persistent operation cards are part of it, not another Output pane.

ImGui: top row Severity multi-select (Trace/Debug/Info/Warning/Error/Critical), Source multi-select (Core/App/Managed/Tool stdout/Tool stderr/Authoring), Search/Clear Filter; next row Copy Selected, Copy All Filtered, Clear and Auto-scroll checkbox. Use a table for time/source/severity/summary with selectable record IDs, keyboard selection and context Copy. Expanding a row/detail child shows complete multiline text. Auto-scroll follows only if already at bottom; manual scroll pauses following and a **Jump to Latest** button restores it. Filters never hide the pinned latest operation result; error badge includes hidden-error indication.

Store sequence ID, wall timestamp with milliseconds, monotonic elapsed/session time, source/severity, message, optional project/document/operation identity and validated file/line location. Default clock display `HH:MM:SS.mmm`; full date/timezone and elapsed time in details/copy. Preserve multiline as one entry, not independent misleading rows. Clip summary rows with `ImGuiListClipper`; details handle expanded variable height. Plain substring search and filters suffice initially. Current logger thresholds are Info: filtering cannot recover suppressed Trace/Debug messages. A Console settings **Capture Level** combo can adjust editor logger admission live, default Info, persisted as a user preference; distinguish admission from display filtering and restore normal thresholds on capture shutdown.

Suggested fixed budgets: incoming queue 1 MiB; retained messages 10,000 or 8 MiB, whichever first; individual message 64 KiB with truncation marker; bounded partial process lines too. Drop oldest with explicit dropped-count indication. Clear clears visible retained history, not file/stdout logs or a running operation's completion record. Keep a small bounded operation history (for example 20), latest failed result pinned until dismissed. Progress is stage + elapsed time + indeterminate activity when tools provide no percentage; do not fabricate percentages.

Architecture:

1. Attach a custom thread-safe spdlog sink to both existing core/client loggers before editor Application initialization so startup failures are captured. Keep stdout sinks and any externally redirected file logs. Capture raw level/logger/time/message; optional rotating session files can be added under user data with a fixed disk budget. No change to dependency pins.
2. Sink owns/shared-owns a non-ImGui queue, never panel or Scene pointers. Producers copy bounded records under a short mutex, with no ImGui, filesystem/service work or logging under that mutex. Main thread swaps/drains a bounded batch, releases lock, updates model, then draws. Record overflow counters directly; reporting dropped logs must not recursively call spdlog from its sink. Do not hold collector locks while taking logger locks or processing callbacks.
3. Route native authoring errors once through structured diagnostics; avoid duplicating ActionFailed as both a string and a second log entry. Managed exceptions already reaching core logs can be tagged with managed origin. Add a small public `Hazel.Log` managed facade in a later Console increment. C# Console.Out/Error redirection, if desired, must be domain-scoped, bounded and restored on reload/shutdown; arbitrary managed stdout is not currently integrated and must not be advertised as such beforehand.
4. Extend Process streaming callback to carry stdout/stderr separately, draining both concurrently/fairly on both platforms; retain structured argv, process-group/Windows job ownership and bounded final report. Assemble UTF-8/chunked multiline diagnostics safely; do not infer every stderr line is an error. Use compiler severity when recognized, source “stderr” otherwise.
5. Operation model owns request snapshot, ID/project generation, stage, start/end time, exit/timeout and artifact paths. Completion updates this model independently of incoming log retention. Workers do not invoke editor closures; main-thread Poll validates owner/project identity before reloading assembly or opening results.
6. Shutdown stops admission of jobs, keeps queue/sink alive through worker joins, scene/script watcher shutdown and renderer teardown logs, then detaches its shared sink safely and releases collector. UI may detach earlier because sink has no UI pointer. Avoid mutating logger sink vectors concurrently with producers: use an owned forwarding/distribution sink with synchronized attachment or a logging quiescence boundary. Never globally shut down logging just to close a panel.

Current compiler/export jobs deliberately lack cancellation and can run for up to 60 minutes. Retain explicit **Exit when job finishes / Keep editor open** handling and an indeterminate status; do not introduce unsafe force-stop UI. Bound output drain work per iteration so deadlines/exit checks execute even during a log flood. File navigation is offered only for validated compiler locations inside appropriate source roots: Open File/Open Folder/Copy Location works with current facilities; configured editor line arguments are a later opt-in protocol, not shell concatenation or a universal promise.

**Stage B delivery:** see the [implementation record](editor-usability-a1-a2.md#stage-b-console-and-tool-operation-reporting).
The Console collector, core/app/managed bridge/tool streams, scoped observers,
bounded operation history and process ownership are implemented. The existing
Output dock settings migrate only when Console has no saved settings; failures
use the status link without stealing focus. Native stdout remains functional
without Hazelnut. Clear preserves operation results and the pinned latest failure.
Actual child-command stages replace guessed percentages. Internal teardown is
cancellable; normal compiler/export close waits. Managed Console.Out/Error,
optional rotating files and validated compiler file/line navigation remain
separate refinements. C–H below are still pending; broad redundant-tooltip cleanup
remains a later presentation pass, not a Stage B rewrite.

## 11. Preferences/workspace persistence

Use the existing `Resources::Get().UserData` root, honoring HAZEL_DATA. Normal roots: Linux `$XDG_DATA_HOME/Hazel/Hazelnut` or `~/.local/share/Hazel/Hazelnut`; Windows `%LOCALAPPDATA%/Hazel/Hazelnut`. Do not relocate existing preferences/layout merely to create a new taxonomy.

| Scope | Data / existing coverage | Proposed location and write trigger |
| --- | --- | --- |
| User/editor preferences | Existing Python/SDK/editor paths, UI scale 0.8–2, collider toggle, recents. Add editor VSync, startup restore toggle, console filters/time/follow policy, browser view/thumbnail preference. | Existing versioned `preferences.yaml`; explicit Apply/Save, or discrete preference command with debounced flush. Tool paths stay machine-local. |
| Machine session | Main window normal position/size, maximized state; last successfully opened project; panel visibility. Currently absent. | New versioned `session.yaml`; successful Open/close and settled move/resize debounce, never every frame. Only normal/windowed + maximized initially; do not restore minimized/fullscreen/Play. |
| Dock layout | Existing user ImGui layout including detached panels. | Keep `imgui.ini`, seed only when absent, atomic save on existing detach path; explicit reset takes backup. Renaming panels needs targeted migration preserving docks. |
| Project-specific machine workspace | Last successfully opened scene, editor camera, browser folder, selected entity UUID, sheet/clip selection, expanded hierarchy/component sections, export destination/name. Currently mostly transient. | `workspaces/<hash-of-canonical-descriptor-path>.yaml` under user data; paths relative to project where possible; debounced accepted actions/close. Validate IDs on restore. A moved project offers Reset/Associate Previous Workspace explicitly; don't guess identity by display name. |
| Portable project configuration | Existing Name/ScriptProject/Assets/StartScene/ScriptModulePath. Later supported runtime rendering requests and generation/template compatibility version. | `.hproj`; explicit Project Settings Save, atomic, schema-versioned. No window bounds, tools, absolute workspace paths or dock placement. |
| Portable authored content | Scene components/script defaults; sheet regions/clips/sampling; detached prefab content. | Existing content files through document Save. Browser/preview state is not authored data. |
| Recovery copies | Unsaved recovery snapshots if later enabled; original bytes before repairs/migrations. | Machine user-data recovery folder, bounded history, explicit restore prompt with path/hash/time. Never replace the source automatically. Full periodic draft recovery is a separate extension. |
| Transient runtime | Physics owners, animation playback/current frame, script instances/live fields, runtime scene transitions, input capture, GPU resources, jobs. | Never persisted/restored as workspace. Start in Edit with fresh runtime defaults. A crashed job becomes an interrupted historical status, not an automatic rerun. |

Restoration contract:

- Explicit launch project/scene arguments take precedence. Add proper argument parsing to Hazelnut (currently only `argv[1]` as project), including optional `--project`, `--scene`, `--no-restore`; bad explicit input reports failure without silently opening a different remembered project. After successful project stage, try last scene; if missing, show Locate/Forget and fall back to that project's validated startup scene. Missing project shows Recent/Locate/New; never delete assets or stale settings.
- Validate finite geometry and size bounds; choose intersecting monitor work area, clamp reachable title/edges, and center/shrink on current primary monitor if no intersection. Store normal restore rect independently from maximized rect and restore normal rect before maximizing. Recompute from current DPI; monitor IDs are hints. Under future Wayland let compositor place the window. Adjust unreachable detached docks while preserving layout structure.
- Preference UI: categories list and search on left, rows on right; **Apply and Save**, Revert and Reset Defaults. Workspace options offer “Reopen last project/scene” and **Forget workspace for this project** without touching content. Panel visibility uses Window checked menu items; browser settings popup controls view/tile size; Console settings popup controls timestamps/filters/follow.
- Schema versions/defaults/migrations validate before use. Corrupt/future settings use safe in-memory defaults and report file path; preserve originals until explicit Reset/Save with backup. Atomic writes prevent partial replacement, but not lost concurrent edits.
- Multiple instances use separate session/workspace/layout snapshots identified by an instance token. A primary ownership lease controls shared session/layout writes; secondary instances read baseline and write their own recovery/snapshot, rather than last-close overwriting another instance's layout. Shared preferences use lock + reread/generation conflict check; UI offers Reload or explicit overwrite after conflict. Atomic rename alone is insufficient. If locking fails, keep in-memory changes and report persistence failure.
- Unsaved content is never reconstructed from session fields. Closing runs the named document guard first, then records last successfully opened paths/state. Future crash-recovery snapshots prompt Recover Draft/Ignore with original kept intact. Layout reset or monitor recovery must not clear unsaved documents.

## 12. Native project generation and tooling boundary

Current canonical generation is `scripts/internal/authoring.py::create_project`, called by editor and CLI. Its transactional publication is valuable; its compulsory Debug build is the unnecessary prerequisite. C++ `ScriptSource::Create` and Python starter source duplicate related template/identifier behavior; UI instructions say “start with a letter” while both validators allow underscore. Choose and document one contract, using current `[A-Za-z_][A-Za-z0-9_]*` plus reserved names.

Proposed shared native `Hazel/Project/ProjectCreation` service accepts `ProjectCreateRequest {Name, Identifier, Destination, TemplateVersion}` and returns descriptor path, created-file list and structured diagnostics. It generates descriptor, directories, primary-camera/text starter, script source and canonical Premake template; no ImGui, Python discovery, compiler or active-session mutation. Use existing serializers/ScriptSource validation. Package one versioned template set in engine resources; generate editor Create Script and starter Example from that same ownership. Existing `project.lua` build semantics remain authoritative during migration.

Create in an exclusively reserved sibling staging directory, validate its complete native content, then publish with no-replace directory semantics (existing Python Linux renameat2/Windows non-replacing rename behavior). Own-only cleanup on failure; never replace an existing destination, including an empty directory that appears concurrently. Distinguish atomic visibility from durable power-loss guarantees. Missing tools must not block this service.

Editor invokes service directly; a small native CLI frontend (possibly extending the existing CPU authoring/audit tool where responsibility remains clear) exposes the same generation contract. Python `hazel.py new-project` delegates to it, then optionally invokes existing `script-build`. Do not maintain a second Python template/validator. Transitional SDKs missing this generator report that specific contract mismatch, not silent fallback to competing templates. Existing projects need no template rewrite.

Uncompiled projects keep ScriptModulePath as the intended relative artifact, but loader supports “not built yet” editing state. Content Browser, sheets, scene configuration, prefab editing and saving work. Inspector retains unavailable classes/fields; compiled class chooser has **Build Scripts** and readiness explanation. Play of non-scripted content can run without a project domain after appropriate initialization changes; scripted Play/export require readiness. Once compilation succeeds, staged reflection/domain replacement updates readiness; failures retain valid assembly/session. No SDK/Python dependency for merely creating/editing content.

Keep deterministic interpreter discovery, explicit-invalid-path errors, absolute argv/cwd, bounded probes, build locks and canonical Python packaging. Add a small SDK/native generator/ScriptCore API contract stamp checked in preflight/build/open, separate from full engine commit identity: compatible patch commits need not be rejected. The present preflight mainly checks tool/files, and reflection checks classes/components/constructor; this is not comprehensive editor-SDK compatibility validation. Show mismatch with Fix SDK/Rebuild Scripts actions in the normal readiness UI.

Export's later targeted-build correction should build Nutella, ScriptCore, PackageAudit, SpriteAssetAudit and their dependency closure, then the selected project's Release scripts; preserve resource/native-library staging and packaging validation. Leave full developer `build`/CI behavior available. Verify exported closure from a relocated directory. Do not rewrite packaging to eliminate Python, and do not skip rebuild solely because a binary file exists.

## 13. Entity hierarchy and detached hierarchical prefabs

**Separate milestone after UX cleanup.** Current scene transforms are treated as world by all render loops/cameras/gizmos/overlays, managed Translation/Scale and Box2D synchronization. `Math::DecomposeTransform` is not a shear-safe inverse and its caller does not check success. Existing prefab v1 loads exactly one entity; Scene::Copy preserves UUIDs for Play, Duplicate allocates one ID without a subtree map, and Instantiate remaps self fields. Each assumption needs an explicit replacement contract.

Bounded proposal:

| Area | Contract |
| --- | --- |
| Relationship ownership | Scene owns `RelationshipComponent {Parent UUID or 0, sibling order}`; entity UUIDs are scene-local references qualified by scene identity at APIs. Serialize only authoritative parent/order; derive child adjacency in Scene. No owned Entity pointers or separate contradictory persisted child list. |
| Validation | Reject self-parent, ancestor cycles, cross-scene targets, duplicate IDs, missing parents, invalid sibling orders and non-finite transforms before mutation/runtime. Use iterative bounded traversal/depth/entity limits with diagnostics; don't silently detach broken edges. A malformed graph may be shown as a diagnostic list with no hierarchy evaluation; running/saving interpreted content is blocked until explicit correction. |
| Transform | Existing TRS becomes **local**, root local = world; `World = ParentWorld * Local`. Reject non-finite composed matrices, including multiplication overflow. Cached world matrices/dirty propagation are transient. Parenting does not change UUID. Renderers, cameras, picking and overlays consume world matrices consistently. Use explicit local/world getters rather than changing GetTransform's meaning invisibly across callers. |
| Nonuniform scale/shear | Visual descendants can render the exact composed matrix, including shear; local authored transform remains TRS. Preserve-world reparent computes inverse(new parent world) × old world and accepts only if finite, invertible and reconstructable as TRS within a documented relative tolerance. Reject singular/nonrepresentable/sheared local results; never approximate. Report shear in world information rather than invented Euler/scale values. Negative/reflected scales need tested decomposition or rejection for that operation. |
| Reparent options | Default Preserve World. Explicit Preserve Local permits a deliberate world-position change. Validate candidate subtree and all physics restrictions first. No mutation on failure. Detaching uses same rules and may also fail TRS representability. |
| Physics bound | Initially **all Rigidbody2D/collider owners must be roots**; dynamic bodies under parents are rejected, not simulated approximately. Root body may have visual-only children that follow its final world transform. No child colliders silently compound into parent's body. Require physics planar rotation and positive valid dimensions; circle nonuniform XY scale cannot silently become an ellipse. Parent/physics component edits validate this restriction immediately and before Play/Simulate/export. Supporting static/kinematic descendants requires a later explicit body/fixture synchronization design. |
| Update order | Safe lifecycle/reparent boundary → parent-first world propagation for script reads → script snapshot callbacks → flush mutations/recompute world → animation → root physics synchronization/step → write root transforms → propagate descendants → render/picking/overlays. Dirty ancestor paths are updated on demand for world queries during callbacks; never serve an old matrix. |
| Callback mutation | Runtime parenting commands queue UUID/scene-identity operations; getters keep committed graph until safe boundary, with documented completion/failure. Validate on enqueue and again at commit after destroys. Subtree destroy marks every member invalid immediately and runs exactly-once cleanup at safe boundary, child-before-parent, with no callbacks editing live traversal. Preserve existing snapshot/deferred lifecycle foundation. |
| Parent destruction | Default destroys subtree, matching containment. Editor confirmation lists descendant count; separate “Delete parent, keep children” detaches all children preserve-world transactionally, then deletes only parent. If any transform is unrepresentable, whole operation fails; user may explicitly choose preserve-local detach. Runtime exposes explicit policy, not an accidental orphaning rule. |
| Duplicate / copy | Duplicate subtree allocates all new UUIDs first, copies authored components/fields, remaps all internal entity refs and relationships, inserts as sibling beside source; external same-scene refs stay explicit. Scene copy for Play preserves UUID/edges but clears physics/animation/world-cache owners. Define entity-only duplicate separately, never half-copy children implicitly. |
| Prefab v2 | One explicit root and connected set of local entities, no parent outside asset. Stage/validate entire source before destination mutation; allocate every instance UUID, remap internal fields/edges, attach root placement, then initialize physics/scripts after complete insertion. Roll back whole insertion on failure. Root position-only override preserves authored root rotation/scale, as today. |
| External references | Internal entity refs remap; external scene entity refs rejected on prefab creation. UI shows offending field and offers explicit Clear or include the entity in the authored subtree; no automatic name lookup. Project prefab/sprite references remain typed paths/IDs and require closure validation. No implicit recursive nested-prefab instantiation. |
| Compatibility | Legacy unversioned scenes migrate to a versioned root-only graph in memory; prefab v1 imports as one root. Write new scene/prefab versions only on explicit save with original preservation. Old readers must reject new hierarchy versions; native/package validators and managed APIs update together. No down-save that flattens relationships. |
| Managed APIs | Add Parent/Children (snapshot), SetParent/Detach with explicit transform policy and queued result, LocalTranslation/Rotation/Scale and World matrix/position operations with representability checks. Preserve existing Translation/Scale meaning as world-compatible wrappers or introduce an explicit version migration; don't silently redefine old scripts. Existing Entity scene identity/CheckedID prevents cross-scene access. |

Hierarchy ImGui workflow: true `TreeNodeEx` rows keyed by scene/UUID with arrow only when children exist; name/type search keeps ancestor context. Select syncs viewport/Inspector; F focuses/reveals selected entity. Drag UUID + scene identity onto entity to reparent, onto root area to detach. Hover preview states **Parent under X · Preserve World**; invalid drop explains cycle/physics/shear. Optional modifier/menu chooses Preserve Local explicitly. Context menu Create Child, Duplicate Subtree, Create Prefab from Subtree, Unparent, Delete Subtree and Delete Parent/Keep Children; destructive operations use the described count/policy modal. Inspector exposes Parent chooser, Local rows and read-only World information; world-space gizmo applies inverse-parent conversion with exact validation and visible rejection. Retain single selection initially.

Gizmos use world matrices; picking IDs still identify individual child UUIDs rather than parent. Selection outline uses resolved sprite corners through world matrix. Collider overlays depict actual root fixture offset/rotation/scale conventions, not an approximate transformed bounding box. Root rigidbody motion updates visual descendants after physics. Hierarchical prefab editor shows isolated tree/Inspector/preview, saves the detached subtree, and Instantiate selects its root. No linked instance update is implied.

Stage internally: relationships/validation/serialization and matrix propagation first; all render/script/physics consumers second; editor tree/reparent/gizmo third; prefab v2/remapping and export closure last. Each stage has service/lifecycle acceptance before adding UI interactions. Deferred linked overrides, variants and propagation would require separate identity/change tracking and are not required for detached hierarchy.

## 14. Targeted corrections, without broad rewrites

| Priority | Evidence-backed correction | Boundary to preserve |
| --- | --- | --- |
| P0 | Centralize operation availability/mutation policy: tool jobs block every file mutation participating in build/export, including prefab/script creation and scene/sheet assignment. Make labels explain the reason. Snapshot saved-file inputs or hold the existing operation guard consistently. | Read-only browsing/previews continue; external edits are detected/revalidated rather than presumed locked out. |
| P0 | Replace generic discard guard with explicit affected-document list and operation intent. Export Saved/Use Saved Assets does not discard drafts. A destructive close/open's Discard really resets/closes those drafts. Stage new Open before committing loss of the previous session. | Save errors/cancel leave pending intent and dirty drafts available; partial saves are reported. |
| P0 | Enforce schema/version/unknown-field safety before automatic recovery. Current unversioned scene reader cannot promise unknown-data preservation. Add structured load diagnostics rather than concatenated generic messages. | Retain known legacy sources and exact missing typed references; no YAML “repair” guessing. |
| P1 | Remove dummy hierarchy child; fix blank-click selection clearing and focus-gated shortcuts. Make runtime property edits explicitly temporary and gate authored edits consistently. | UUID/scene identity checks and runtime-copy isolation. |
| P1 | Split AuthoringPanel's 1,036-line responsibilities into thin orchestration plus existing/new bounded document guards, operation/readiness model and settings forms. Avoid a plugin/service-locator framework. | Existing ProjectTools, SpriteSheetDocument, SceneSerializer and Prefab services remain authoritative. |
| P1 | Panel-owned picker state; typed reference presentation separated from validation. Cache prefab lists/readiness results outside per-frame drawing and refresh after accepted changes. | ProjectAssets owns GPU caches on graphics thread; workers receive CPU/path snapshots, never GL/scene pointers. |
| P1 | Shared physics validation for Play **and Simulate before OnPhysics2DStart**. Simulate currently creates bodies before `PrepareSprites(true)` and bypasses RuntimeSession's physics preconditions. | No weakening Box2D assertions or substituting dimensions to make malformed scenes run. |
| P1 | Scene/prefab/project conflict fingerprints and exclusive new-prefab publication. UI exists-check plus Prefab::Save's replacing atomic write has a destination race; sheets/import already have CreateNew semantics. | Existing overwrite saves stay atomic; only user-approved overwrite replaces data. |
| P1 | Typed readiness report keyed by operation/tool paths/SDK contract/configuration; currently one `m_Report` is reused for Python validation, preflight/build/export. `Ready` checks prior argument shape and success rather than a persistent validated contract. | Revalidate at operation start; deterministic Python discovery remains. |
| P1 | Process drain budgets, separate streams, handle inheritance whitelist on Windows (`CreateProcess` currently inherits handles broadly), retained deadlines and join order. | Linux close-from descriptor hygiene; Windows process job ownership. Detached external editor reaper has no Scene/UI pointers; do not bring long-lived external editors into tool shutdown join. |
| P2 | Targeted export build/staging; native generation; SDK compatibility stamp; settings draft validation shared with services. Python packaging's project reader does not enforce the same explicit Version gate as native ProjectSerializer. | One canonical build/package closure and contract tests across entry points, no duplicate templates or dependency overhaul. |

Do not add fine-grained cache infrastructure without measured reload stalls. Current global epoch invalidation is simple and tested; correct picker lifetime and owner thread constraints first. Keep authored sprite Source/default clip separate from resolved resource/current clip/playback; reset transient state in every copy/duplicate/instantiate path. Diagnostics should expose stable IDs on demand, not use hex IDs as the primary beginner-facing name.

## 15. Verification simplification and acceptance

Retain Linux/Windows Debug/Release builds, strong logical assertions and short real rendering/startup/shutdown checks. Existing 14 migration executables include useful ownership/serialization/lifecycle/service coverage; especially retain SpriteSmoke, SceneSmoke, ProjectPhysicsSmoke, RuntimeSessionSmoke, MonoSmoke, CoreSmoke and EditorSmoke's direct authoring/selection/save/reload checks. Rendering checks for UV/pivot/picking/cache invalidation and GL errors verify actual contracts, not UI pixel style.

Retire as default blocking acceptance the lengthy desktop playthroughs in `scripts/internal/game_tests.py`: fixed editor area `(372,58,570,394)`, Play pixel `(657,40)`, framebuffer color searches, route timing and repeated full-game completion in several configs/profiles. `testing/desktop.py` additionally changes Windows display mode to achieve test geometry. These couple layout/art/DPI/software frame rate to correctness and will churn under UX cleanup. Package tests' pixel-click transitions should similarly become short lifecycle/relocation smoke plus logical transition tests.

Keep `tests/examples/FlightTests.cs` gameplay model assertions and `tests/examples/RuntimeSmoke.cpp` appropriate real startup/render/lifecycle checks. Keep archive checksums, source/SDK-unavailable relocation, complete managed/native/resource closure and strict dependency validation. Move actual gameplay completion/restart/menu and UI workflow acceptance to human release checks; old capture drivers may remain an explicitly optional evidence job until replacement coverage is in place. Inspect each automation assertion and map it to service, smoke or manual coverage before removal. Do not lower expected scores, lengthen waits indefinitely or ignore lifecycle errors to obtain green CI.

New tests should be narrow logical tests for document guard outcomes, missing/unknown-data preservation, generation without tools/exclusive publication, settings migration/conflicts/geometry clamp, Console overflow/concurrent shutdown, dual-stream flood/deadlines and hierarchy transform/remap/callback rules. Use isolated user-data fixtures. One short real ImGui smoke for migrated property/asset panels is sufficient; no extensive GUI automation framework.

Human acceptance checklist, using both example games and a new project:

1. Import texture using visible button; slice grid twice without replacing IDs; draw/resize/pivot a region numerically and with mouse; rename and locate it by search.
2. Build a clip without remembering another tab's selection; reorder/time frames, scrub, assign by named target and typed drag; confirm static source is preserved beneath animation.
3. Ctrl+S from scene/prefab/sheet; Save All with one failing/conflicting file; close/Open/export with Save/Discard/Use Saved/Cancel; verify dirty states and original files.
4. Normal Open with missing texture, malformed sheet, future descriptor, unavailable assembly and missing scene; preserve prior valid session on rejected open and exact unresolved references on editable open; restore/retry and save with original backup.
5. Play/Pause/Step/Stop and Simulate; authored data returns intact. Attempt file edits during export. Console filters/copy/multiline/manual scrolling/log flood retain discoverable completion.
6. Restart with last project/scene, moved paths, new monitor arrangement, maximized window, explicit arguments, corrupted settings and two instances; existing dock layout and VS Code files remain intact. No Play restoration.
7. Windows native drag/resize/snap/system menu/close guard; Linux WM interactions; 100/150/200% DPI and small docks; detached windows, keyboard-only property editing, focused gameplay input. Verify native OpenGL 4.1 path and HD4000 4.2 without raising renderer requirements.
8. Export only selected project, extract to spaces/Unicode path, hide source/SDK resources, run Nutella from unrelated cwd, exercise both games manually and close cleanly.

Hierarchy acceptance later adds cycle/cross-scene failures, world-preserving reparent and rejection of shear/singular cases, parent destruction policies, exact visual world matrices, root-only physics enforcement, subtree duplication/ref remapping, prefab v1/v2 compatibility and mutation during OnCreate/Update/Destroy.

## 16. Dependency-ordered implementation plan

Effort is a relative planning estimate for an engineer familiar with this checkout, not an overnight promise. H/M/L denote user benefit, effort and architectural risk; stages require their own review/acceptance.

| Order | Deliverable and dependency | Benefit | Effort | Risk / exit gate |
| --- | --- | --- | --- | --- |
| A1 | Shared property rows + explicit IDs; fix fake tree/blank click/focus; one mutation availability policy. | High | Medium | Low–medium. Existing authoring/services still pass; physical selection and keyboard editing work. |
| A2 | Visible browser Import/search/type/breadcrumbs; coherent selection headers; sheet frame table, region chooser, named Save-and-Assign; active-document save and operation-specific guard. Depends A1. | High | Medium | Medium. Complete existing create→sheet→clip→entity→Play→save→export workflow without guessing target/save scope. Preserve layout. |
| B | Console collector, core/client/tool streams, bounded operation model, safe shutdown/fair process drain. Depends A1 availability rules; can follow A2 independently of schema work. | High | Medium | Medium. Concurrent flood/exit tests; completion remains visible; no duplicate error inbox. |
| C | Structured staged Open, known-schema/unknown-data gate, automatic editable unresolved resources, save conflicts/original backups, optional scripts readiness. Depends guard/model work; do not remove Repair menus until parity exists. | High | Medium–high | High for data integrity. Failed Open/Save retains prior session/disk; missing assets round-trip exactly. |
| D | Session/workspace restoration, visibility/layout migration, DPI/geometry clamp, multi-instance persistence; live editor VSync and read-only device report. Depends stable panel/document identities. | High | Medium | Medium. Two instances and changed monitors cannot overwrite layouts/content. |
| E | Native generation contract and uncompiled-project editing; separate build; SDK contract; targeted export closure. Depends C readiness and B operations. | High | High | Medium–high. New project needs no tools; editor/CLI produce same content; portable exports contain all dependencies. |
| F | Portable runtime renderer requests, prelaunch validator/spec plumbing and requested/effective UI. Depends D/E schemas/preflight. | Medium | Medium | Medium. Project switch shows pending policy accurately; Nutella applies it before init; 4.1/4.2 unchanged. |
| G | Scene relationships/world transforms + consumer changes; then hierarchy UI; then detached prefab v2/remapping. Depends C schema safety and stable A selection. | High for larger scenes | High | High. Separate service/lifecycle/physics/version gates in §13. |
| H | Optional custom caption prototype and OS acceptance. Depends D DPI/geometry and demonstrated demand. | Low–medium versus A–E | High | High platform risk; native fallback mandatory. |

**Bounded first milestone: A1 + A2 only**, likely several working days plus Linux/Windows/human review, split into reviewable property/selection and workflow increments. Include the focused unsafe-edit/save-guard corrections; keep native decorations and current renderer/runtime/schema/tool contracts. Do not bundle Console ingestion, recovery format changes, persistence, native generation or hierarchy into this first implementation. Its success measure is that the already working sprite/animation workflow becomes understandable, aligned and predictably saved. B–F are separately shippable stages; G is a separate feature effort.

Testing simplification accompanies each stage: retain logical coverage before retiring its corresponding pixel driver. Existing branch milestones should be reviewed/merged by a separate authorized task; this plan is written against their combined current checkout, not master alone.

## 17. Deferrals and user decisions

Explicitly defer animation graphs/state machines/blending/event tracks; linked prefab overrides/variants/propagation; global asset database and automatic rename repair; skeletal/3D authoring; backend switching/HDR/global MSAA policy; live renderer teardown/reinit; broad undo/redo; automatic destructive recovery; new GUI test framework; periodic crash-draft recovery beyond first-repair original backups; cross-platform packaging rewrites; macOS/ARM64/Metal delivery. None is needed to make current authoring coherent.

Decisions needed before the relevant implementation, with recommended defaults:

- **First milestone:** A1/A2 and their corrections are delivered; native decorations remain. No dependency updates are proposed.
- **Selection/navigation:** retain scene target while editing assets, name it on assignment, and keep one sheet/prefab draft initially. Use the freely dockable proposed default only for first use/reset.
- **Generation:** E implements the authorized content-first creation and separate optional build contract; hands-on acceptance remains.
- **Hierarchy:** subtree destruction default, Preserve World reparent, root-only physics owners initially, detached prefab v2, and explicit managed world/local compatibility policy. Confirm bounds before G; linked prefabs remain deferred.
- **Renderer:** native editor VSync preference first; portable runtime requests later. Decide whether advanced batch/shader policies need user controls at all after capability/readiness presentation is usable.

The current ownership, stable sprite/clip IDs, authored/transient separation, common texture pipeline, safe sheet documents, runtime copies, staged loads and transactional packaging are sound foundations. The most valuable fixes are consistent left-label property rows, visible asset actions/search, explicit selection/assignment, predictable document saves and one diagnostic workflow. The visual direction is a coordinated dark docked workspace with native OS framing. Implement A first, then independently stage Console/recovery/persistence/native tooling; hierarchy belongs in its own milestone.

## Implementation follow-up (A1/A2 correction)

Linux/Windows user feedback revised the first milestone's presentation: retain a
minimal runtime-icon toolbar; authoring commands belong in existing menus/panels.
Restore Hazel's colored axis reset design inside shared left-label property rows.
Native bounded source-SDK discovery/configuration is a focused correction, not
native project generation or a new packaging/ABI contract. See
[the implementation record](editor-usability-a1-a2.md#focused-correction-after-linuxwindows-feedback).

Track redundant tooltip cleanup as a later presentation task: behavior, units,
shortcuts, consequences and disabled reasons are useful; repeating readable labels
or values is generally unnecessary. Full paths/values are useful when clipped.
The current correction applies this to touched controls only. B is delivered;
C–E delivery is recorded below and in the implementation record. F–H and broader
tooltip cleanup remain separate stages.


## Stage C implementation follow-up

Stage C is verified locally with Linux Debug/Release builds, focused CPU checks
and software-rendered editor regressions. Physical acceptance and CI are separate. Normal Open now uses staged known-schema recovery, retained unresolved
references, original backups and disk-conflict handling. Read the current contracts and check
status in [the implementation record](editor-usability-a1-a2.md#stage-c-normal-open-and-preservation).
A1/A2/B remain intact. After reboot the user authorized D/E to proceed following
C verification; F/G/H and broad tooltip cleanup remain deferred. The original findings above describe the audited baseline, not later code.

## Stage D implementation follow-up

D now implements machine-local preferences/session/workspaces, native window/DPI
bounds, explicit launch precedence, lease/conflict protection and read-only device
information. Existing ImGui docking remains authoritative. See the implementation
record for scopes, write triggers and actual verification. F/G/H and
broader tooltip cleanup remain separate.

## Stage E implementation follow-up

E implements one native content-first generation service and template contract,
shared by Hazelnut and the CLI. Creation does not launch Python or compile;
script tooling and export remain explicit operations with their existing Python,
SDK and packaging contracts. Readiness distinguishes editing, script tools,
script-free/assigned-script Play and saved-content export. Export selects required
runtime/validator targets and the chosen project's scripts, rather than unrelated
examples. See the implementation record for verification and remaining limits.
The next architectural stages are F (portable renderer requests), then G (bounded
hierarchy/detached prefabs); H remains an optional native-caption alternative.
Broader redundant-tooltip cleanup is still pending. Stop after E for physical
Linux/Windows acceptance before any of those stages.
