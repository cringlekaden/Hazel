# Work in Hazelnut

[Set up or download Hazelnut](getting-started.md) first. Open a `.hproj` through **File > Open Project…**, or pass it to the source launcher. Hazelnut restores the last successful project when no explicit project is supplied.

![Hazelnut editing The Last Lightkeeper](images/hazelnut-lightkeeper.png)
*The title menu is scene content: select a button in Hierarchy, edit its Rect in Properties and open shared styles from Content Browser. [Full resolution](images/hazelnut-lightkeeper.png). Linux capture.*

## Create and open content

**File > New Project…** asks for a name, build identifier and destination. Creation produces editable content immediately; a compiler is needed when you build scripts, rather than to create the project. An existing destination is rejected. A later failed script build keeps the created project and source files available for repair.

Open scenes by double-clicking them in Content Browser or using **File > Open Scene…**. Normal Open reports repairable missing resources while retaining their authored references. Invalid/unsupported documents keep the previous session available.

## Arrange entities and prefabs

Use **Scene Hierarchy > Add Entity** to create a root. Drag onto an entity to parent it; drag onto **Scene Root** to detach it. Reparenting preserves world pose and rejects nonrepresentable transforms. Properties edits local transforms; rigidbody/collider owners must remain roots.

Select an entity to edit its components and typed script fields in **Properties**. Referenced entities are selected by name within the current scene. The viewport offers transform gizmos; Runtime UI Rect entities also expose move/resize editing and anchor presets.

**Create Prefab** saves the selected subtree. Include its referenced owners and local visual/focus targets: detached prefabs reject references outside their subtree. Open a `.hprefab` in Content Browser, edit/save in **Prefab Inspector**, then **Instantiate and Select**. Instances receive independent identities and values; editing the source does not propagate to existing instances. UI styles remain shared assets.

## Save, Undo and runtime copies

**Ctrl+S / File > Save** saves the active scene, prefab or sheet. **Save As…** selects a new scene destination; **Save All…** handles eligible dirty documents. Replacement/close/export prompts distinguish saving, using saved content, cancellation and discard.

UI property edits and structural creation, duplicate, delete, reparent/reorder, component edits and prefab instantiation share scene/prefab history. Use **Edit > Undo/Redo**, **Ctrl+Z**, **Ctrl+Shift+Z** or **Ctrl+Y**. Text fields retain their local editing undo. History is bounded; [the UI reference](runtime-game-ui.md#inspect-and-edit-ui) describes its coverage and exact conflict guards.

The viewport toolbar or **Scene** menu controls **Play, Simulate, Stop, Pause / Resume and Step**. Play runs a scene copy with scripts; Simulate runs physics without game scripts. Stop returns to the authored document. Runtime edits are not saved into it. Editor Play uses isolated in-memory game saves, cleared on Stop.

## Build scripts and export

**Project > Create Script…** creates C# source; **Build Scripts…** compiles Debug project scripts and **Reload Scripts** refreshes reflected classes/fields. See [Scripting](scripting.md).

**Project > Export Game…** uses saved content and performs the targeted Release build/script/package sequence. Choose a name and output directory; operation progress and results appear in **View > Console**. See [Packaging](packaging.md#export-from-hazelnut).

![Export Game dialog](images/hazelnut-export.png)
*Export uses the saved project; this upper portion shows the destination, name and readiness checks. The build/export action is below.*

## Settings and useful panels

**Project > Project Settings…** stores portable startup scene, asset/module paths, rendering requests and UI defaults in the `.hproj`. **Edit > Editor Preferences…** stores user tools, scale, session behavior and optional Windows custom caption. Renderer device/effective values are read-only; project requests are distinct from device capability. Linux uses native window decorations.

**View** restores hidden Hierarchy, Properties, Content Browser, Console and Runtime UI Debugger panels. Open `.hsprites`, `.hstyle` and `.hfont` assets from Content Browser to edit sprites, styles and font families. Follow [Sprite authoring](sprite-authoring.md) or [Runtime UI](runtime-game-ui.md).

Preferences, sessions, project workspaces, docking layouts and resource caches have different owners. Their locations and recovery behavior are in [Settings and resources](settings-and-resources.md); preserve them when diagnosing problems.
