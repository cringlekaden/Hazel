# Authoring runtime game UI

Runtime UI is ordinary scene content; it runs in Nutella without ImGui. World text and sprites keep their existing rendering paths; text shares the strict decoder described below. Gameplay decisions belong in C#.

## Build a menu

1. In Hazelnut's Hierarchy, choose **Add Entity → Runtime UI → Canvas**. Set reference pixels (default 1280×720), safe insets and sort order in the Inspector. Enable **Block gameplay** for a menu, pause or dialogue canvas.
2. Select the canvas and create **Panel**, **Text**, **Button** or **Flow group** through the same menu. Presets now use measured layout version 2; existing content stays legacy. A Flow group defaults to Fixed width and Content height. Reselect the group before adding each sibling button; vertical-stack children use Fill width and buttons measure Content height. Rects use top-left logical pixels; Transform does not position UI.
3. Set Width/Height to **Fixed**, **Fill** or **Content** in UI Rect. Fixed authors a border-box size; Fill uses parent space; Content measures visuals/children plus padding. Minimum and optional finite maximum constrain each axis. Choosing free Fill explicitly gives that axis to the parent and clears its anchor/offset/pivot; margins set the insets. Content text width is natural unless capped, so use Fixed/Fill width or an enabled Content maximum to wrap labels. Stacks own child positions; use sizing, margins and Fill weight. Resize handles edit only Fixed dimensions; stack children resize from right/bottom. Computed dimensions/positions stay read-only, and gestures never switch modes. If parent reflow/alignment would move the opposite edge, that resize sample is rejected; edit Fixed size in Properties instead. Escape cancels.
4. Text uses the existing **Text Component** for content, color, kerning and line spacing. **UI Text Style** supplies size, wrapping and horizontal/vertical alignment. Size is logical pixels per font ascender-to-descender height. Kerning and line spacing remain font-height units, as in world text. Drag a project TTF/OTF onto Font asset, or leave it empty for the packaged default font. Separate styled text children provide mixed emphasis; text has no markup.
5. Images use **Sprite Renderer** with the normal texture/sheet picker. For framed panels add **UI Nine Slice**: source borders are left/top/right/bottom pixels within the selected source region, with a separate border scale. Borders must fit; small targets proportionally clamp their display borders. Corners keep their shape, edges stretch in one direction, and the center fills. Layout owns the UI pivot; sprite pivots remain unchanged for world sprites.
6. Buttons have an action identity, tab order, interactable state and five editable tint/image states. An unassigned state image inherits Sprite Renderer. Keyboard focus has a persistent outline, including hover/press. Controls activate on pointer release inside their live clipped bounds with framebuffer coverage, or Enter/Space while keyboard focus belongs to UI. Tab starts navigation; Tab/Shift-Tab and arrows traverse. Escape or a click outside controls releases nonmodal keyboard ownership. A modal canvas blocks gameplay and buttons in other canvases. Higher canvas sort order wins; ties use stable entity identities; child hierarchy order supplies painter order within a canvas.
7. Use the viewport's Window/Small/Widescreen/Classic aspect selector to preview the actual runtime layout. Editing does not dispatch actions. In Play, pointer coordinates map to the displayed image, including its letterboxing. Stop before changing authored values. **Validate UI** checks configuration/assets. Save/reopen as a normal scene. Use the existing **Create Prefab** workflow on the canvas/group to save a reusable subtree. Detached rect prefabs have a static default-canvas editor preview; runtime instances need a canvas ancestor.

The examples contain editable **TitleUI.hprefab** assets. Each keeps its game's own textures/palette. All UI assets and button-state references use project ownership and export closure.

## Connect C# decisions

Attach an ordinary Entity script to a controller; author Entity references in the Inspector. Consume actions before combining them with alternate shortcuts, so a shortcut cannot leave a queued duplicate.

```csharp
using Hazel;
public class MainMenu : Entity {
    public Entity Play;
    void OnUpdate(float dt) {
        string action;
        if (Play.GetComponent<UIButtonComponent>().TryConsumeAction(out action))
            Scene.LoadScene("Scenes/Level1.hazel");
    }
}
```

For a central controller:

```csharp
Entity button;
string action;
while (GameUI.TryConsumeAction(out button, out action)) {
    if (action == "resume")
        FindEntityByName("Pause canvas").GetComponent<UICanvasComponent>().Visible = false;
}
```

Use `UIRectComponent.Visible/Enabled`, `UICanvasComponent.Visible/Enabled/Modal`, `UIButtonComponent.Interactable`, and existing `TextComponent.Text/Color/Kerning/LineSpacing` for dynamic presentation. Entity references are checked against the current scene. Actions contain identities/strings, never delegates. Delivery requires membership and eligibility in the current resolved tree; destruction, ancestor removal/replacement/reparent, and observed hide/disable changes retire pending work. Scene transition, Stop, script-domain reload, focus loss and viewport deactivation cancel both action queues, focus and capture. Restoring input samples held keys before arming new activation. Action identities allow at most 128 bytes and no embedded NUL. Queues hold 64 activations; overflow drops newest with a counter and rate-limited warning. Actions are consumed once; do not have multiple controllers consume the same global queue.

`Input` supplies gameplay input and respects UI ownership. `GameUI.IsKeyDown` supplies viewport-gated keys for UI-specific shortcuts such as Escape to close a modal or N to confirm starting over. `GameUI.IsPointerCaptured` lets alternate shortcuts defer to an active pointer gesture while retaining their held-key history. Keep gameplay movement and interaction on `Input`; do not bypass modal blocking with this UI API. Visibility or enabled changes update native layout/routing, with no per-frame positioning script.

UI currently uses rectangular inherited clipping, uniform reference scaling, free/stack layout and sibling text emphasis. It does not provide grids, wrapping containers, scrolling lists, rich-text parsing or editable text boxes. The native layout/model and renderer abstraction are independent of editor ImGui and backend-specific graphics calls.

## Sizing and legacy conversion

Version 2 free positioning uses point anchors, Offset and a Pivot on the final size. Margins are outside the border box; padding is inside it. Fill starts at the leading margin and can leave space at an enabled maximum. A stack allocates Fixed/Content children first, reserves Fill minimums and gaps, then distributes remaining space by Fill weight (`Flex=0` means 1), redistributing at maximums. It never shrinks minimums to fit; overflow is diagnosed and clipped according to the existing clip policy. Cross alignment positions children; legacy Cross Stretch means Start here, with child Fill controlling expansion.

Content containers measure stack sums/maxima or free child extents. On a Content axis, free children need zero anchor/pivot; Fill depending on that same Content parent is indefinite and rejected with an entity/axis path. Hidden nodes collapse space/gaps; disabled nodes retain layout. Clipping does not change measurement. Empty Content measures padding, empty/zero-width text measures zero, and zero surfaces suspend drawing/hits. Images contribute base source-region pixels as logical content units, then padding and independent axis constraints apply. A standalone, unconstrained Content image with zero padding uses native dimensions. The sprite/background fills the arranged border box, so padding, combined text, constraints or other sizing modes may stretch it; there is no implicit aspect lock. Button-state images and animation frames do not change intrinsic size.

Missing `LayoutVersion` means version 1. Its stretch anchors, negative SizeDelta, minimums and legacy pivot interpretation remain unchanged. Its Inspector retains anchor presets and preferred flow sizes; its viewport gestures keep the D6 correction. A measured free subtree may live under a legacy free parent; legacy children inside measured groups and mixed legacy/measured stacks are unsupported. Convert a uniform legacy subtree before attaching it. An already invalid mixed tree must first be detached/repaired because it has no supported geometry for conversion preview. Nested canvases remain independent surfaces.

Select a visible legacy subtree or Canvas and click **Preview legacy conversion…**. The native preview validates a disposable candidate scene and compares resolved bounds/coverage. Apply creates one **Convert UI layout** entry in the current scene/prefab history; Cancel changes nothing and neither action auto-saves. Fixed, minimum-constrained and representable full stretches preserve appearance and responsiveness. Legacy flex bases become Fill minimums. Partial stretches or negative insets require an explicit **Freeze ambiguous stretches as Fixed** choice, which preserves the preview size while losing stretch responsiveness. Unresolved/hidden/interrupted nodes, isolated children of legacy stacks and coverage mismatches report why conversion cannot proceed. Restore/reveal the viewport/subtree or select its containing stack, then preview again. A changed layout invalidates the preview. Conversion Undo/Redo checks the selected Rect configurations and lifetimes atomically.

For a HUD, add a Content text child to a Canvas, set its free Anchor/Pivot to the desired corner and use logical offsets. For a menu, use a stack with constrained width, Content height, padding/gap and Fill-width, Content-height buttons. Change a label to verify wrapped reflow, preview another aspect, Undo/Redo, save/reopen and Play/Stop. Broader controls remain later milestones.

For measured runtime content, cache the normal component wrapper and change sizing when values change:

```csharp
rect.Width = new UISizing(UISizeMode.Fixed, 320, minimum: 160, maximum: 480);
rect.Height = new UISizing(UISizeMode.Content);
```

`LayoutVersion` is read-only in C#; sizing operations reject legacy components instead of converting them. C++ can use `Scene::SetUISizing` or existing direct component writes. Validated sizing operations reject bad values/dependencies and retain the old authored values. They do not enter authoring history while Play uses its isolated scene copy.

## Resolved state and mutation boundaries

The scene publishes indexed, read-only geometry with a monotonic generation. Borrowed snapshot/element references last until the next resolution or reset; retain IDs/generation rather than pointers across those boundaries. Canvas logical units, viewport pixels and framebuffer pixels are distinct. RuntimeUI accepts viewport-coordinate input and maps it once to the router list’s framebuffer geometry. Hit eligibility and rendering share inward-rounded framebuffer clipping (`[ceil(min), floor(max))`), with half-open edges; zero-size or subpixel regions with no covered framebuffer area cannot focus or hit. Disabled buttons block lower pointer targets; ordinary panels/images/text pass through. Modal canvases restrict controls to their canvas. A pointer gesture owns its release/cancel frame, so simultaneous keyboard Accept cannot turn a cancelled click into an activation.

`Scene::RefreshUI` compares exact layout/input dependency values, component membership and relevant ancestor links. It uses reusable binary buffers, without hashing, YAML or full-document snapshots. Input, rendering and pending-action delivery are boundaries: unchanged values reuse the generation; native field writes are detected at the next boundary. Multiple writes coalesce to the final observed values. Content additionally observes text/font/base-image metrics and asset epochs; color/state-image changes remain paint dependencies. Wrapped lines and intrinsic image metrics are cached across unrelated layout changes. Asset freshness uses explicit reload/refresh epochs, without per-frame filesystem checks. Structural APIs and managed state setters cancel affected gestures immediately. Native callers needing fresh bounds/ownership between normal boundaries call `RefreshUI`; `GetUISnapshot`/`GetUIBounds` otherwise read the published generation. Component references retain the existing EnTT lifetime: reacquire them after structural component/entity edits. Raw C++ `Input` still polls devices; session/scene gating must be honored by native gameplay callers.

Hosts supply viewport size and framebuffer extent before input/render (`RuntimeSession::ResizeFramebuffer` / `Scene::SetUIFramebufferSize`). Changing geometry after input may publish a newer render generation; it never replays that input. Limits are 4096 declared UI rects including hidden ones, 4096 flow items and scene hierarchy depth 256. Invalid derived values reject resolution and cancel interaction; Edit retains the document and displays the error.

Text uses strict UTF-8 scalar decoding for measurement, wrapping and drawing. One leading BOM is ignored; U+FEFF elsewhere is a retained format scalar with zero visual advance. Embedded NUL, malformed/truncated sequences, overlong encodings, surrogates and values above U+10FFFF are rejected. Managed setters also reject invalid UTF-16 before conversion, preserving the previous value. UI text remains limited to 64 KiB. Encoding support does not expand the current Latin-1 atlas or add shaping/fallback fonts.

## Inspect and edit UI

Open **View → Runtime UI Debugger**. Inspect available constraints, measured/arranged size, sizing mode, controlling parent, logical/pixel/framebuffer bounds, effective clip, canvas, generation, eligibility and flow ownership. Focus/capture/modal and gameplay blocking are transient runtime state. Bounds, clip and hit-order overlays are optional. Rebuild/reuse/source-check, text/image measurement cache and arrangement counters plus last resolve/routing timings are CPU diagnostics; they are not GPU duration. Interrupted rect chains, overflow, nested independent canvases, dependency errors and unsupported gestures have diagnostics. Debugger inspection never dispatches game actions or changes authored values.

UI Inspector configuration and viewport gestures share the scene/prefab document history. Use **Edit → Undo/Redo**, **Ctrl+Z**, **Ctrl+Shift+Z** or **Ctrl+Y**. Each completed property interaction or move/resize gesture creates one named entry; anchor presets and asset assignments commit immediately. Sprite-sheet Save and Assign also records the source change when its captured scene target is UI; component creation itself remains outside history. Text fields keep their own ImGui undo while active. History never edits Play/Simulate copies; Stop retains the authored history and valid selection.

Invalid samples keep the last valid authored values. Escape, window focus loss/minimization, selection/document changes and interrupted targets cancel unfinished edits. Flow positions stay computed. Save finishes valid Inspector edits and cancels an unreleased viewport gesture; the accepted save point changes only after the write succeeds. Exact binary authored-content comparison determines dirty state, including native edits outside history and an evicted save point.

Scene and prefab histories are independent and close/reset with their documents. Limits are 100 committed operations and 64 MiB of conservatively accounted retained command data; one in-flight command is separately capped at 64 MiB. Oldest entries are evicted with feedback. The existing accepted authored-content baseline and temporary validation/storage are outside that command budget. History stores values/IDs/lifetime guards, never render caches, fonts/GPU objects or managed delegates. Vectors/colors are atomic authored fields for conflict checks; unrelated fields survive Undo/Redo, and a field takes its before-value when first edited. Conflicting native writes, retired components/entities or changed ancestor chains reject Undo/Redo without partial mutation; **Edit → Clear this document's history** removes obsolete entries.

Unassigned texture/region picker modes remain editable and serialize as the existing `None` source; assign an asset before saving to retain its source mode. Generated native images keep their resource during color edits; changing their source is rejected because history cannot retain a GPU object.

History covers Canvas, Rect, Flow, Text Style, Nine Slice and Button configuration, their text/image presentation, measured sizing and explicit conversion. Creation/deletion, component addition/removal, hierarchy operations, world transforms, script fields and other document types are not yet undoable; their changes still affect exact dirty detection. The wider production control system remains later work.
