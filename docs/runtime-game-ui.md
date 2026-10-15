# Authoring runtime game UI

Runtime UI is ordinary scene content; it runs in Nutella without ImGui. World text and sprites keep their existing rendering paths; text shares the strict decoder described below. Gameplay decisions belong in C#.

## Build a menu

1. In Hazelnut's Hierarchy, choose **Add Entity → Runtime UI → Canvas**. Set reference pixels (default 1280×720), safe insets and sort order in the Inspector. Enable **Block gameplay** for a menu, pause or dialogue canvas.
2. Select the canvas and create **Panel**, **Text**, **Button** or **Flow group** through the same menu. New elements become children. Rects use top-left logical pixels. Edit position and size in the Inspector, or drag a selected element in the viewport. Edge/corner handles resize from resolved bounds; Escape cancels. Negative stretch deltas and the opposite edge are preserved. Flow-owned children use Inspector preferred size/margins; their positions are computed and viewport gestures are unavailable. Transform remains a world component and does not position UI.
3. Use anchor presets for corners, center, bottom captions or stretch. Anchor coordinates are normalized; offsets, size deltas, minimum sizes, margins and padding are logical pixels. Stretch size deltas add/subtract from the anchor span. A flow group owns child positions; child size/minimum/margin/flex control its horizontal or vertical arrangement. Hidden children consume no flow space.
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

UI currently uses rectangular inherited clipping, uniform reference scaling, bounded flows and sibling text emphasis. It does not provide scrolling lists, rich-text parsing, editable text boxes or automatic content-size fitting. The native layout/model and renderer abstraction are independent of editor ImGui and backend-specific graphics calls.

## Resolved state and mutation boundaries

The scene publishes indexed, read-only geometry with a monotonic generation. Borrowed snapshot/element references last until the next resolution or reset; retain IDs/generation rather than pointers across those boundaries. Canvas logical units, viewport pixels and framebuffer pixels are distinct. RuntimeUI accepts viewport-coordinate input and maps it once to the router list’s framebuffer geometry. Hit eligibility and rendering share inward-rounded framebuffer clipping (`[ceil(min), floor(max))`), with half-open edges; zero-size or subpixel regions with no covered framebuffer area cannot focus or hit. Disabled buttons block lower pointer targets; ordinary panels/images/text pass through. Modal canvases restrict controls to their canvas. A pointer gesture owns its release/cancel frame, so simultaneous keyboard Accept cannot turn a cancelled click into an activation.

`Scene::RefreshUI` compares exact layout/input dependency values, component membership and relevant ancestor links. It uses reusable binary buffers, without hashing, YAML or full-document snapshots. Input, rendering and pending-action delivery are boundaries: unchanged values reuse the generation; native field writes are detected at the next boundary. Multiple writes coalesce to the final observed values. Text/color/font/state-image changes are paint dependencies and do not reflow fixed geometry. Text and image caches observe their own values/asset epochs. Structural APIs and managed state setters cancel affected gestures immediately. Native callers needing fresh bounds/ownership between normal boundaries call `RefreshUI`; `GetUISnapshot`/`GetUIBounds` otherwise read the published generation. Component references retain the existing EnTT lifetime: reacquire them after structural component/entity edits. Raw C++ `Input` still polls devices; session/scene gating must be honored by native gameplay callers.

Hosts supply viewport size and framebuffer extent before input/render (`RuntimeSession::ResizeFramebuffer` / `Scene::SetUIFramebufferSize`). Changing geometry after input may publish a newer render generation; it never replays that input. Limits are 4096 declared UI rects including hidden ones, 4096 flow items and scene hierarchy depth 256. Invalid derived values reject resolution and cancel interaction; Edit retains the document and displays the error.

Text uses strict UTF-8 scalar decoding for measurement, wrapping and drawing. One leading BOM is ignored; U+FEFF elsewhere is a retained format scalar with zero visual advance. Embedded NUL, malformed/truncated sequences, overlong encodings, surrogates and values above U+10FFFF are rejected. Managed setters also reject invalid UTF-16 before conversion, preserving the previous value. UI text remains limited to 64 KiB. Encoding support does not expand the current Latin-1 atlas or add shaping/fallback fonts.

## Inspect and edit UI

Open **View → Runtime UI Debugger**. Select a canvas/rect to inspect logical/pixel/framebuffer bounds, effective clip, canvas, generation, eligibility and flow ownership. Focus/capture/modal and gameplay blocking are transient runtime state. Bounds, clip and hit-order overlays are optional. Rebuild/reuse/source-check counters and last resolve/routing timings are CPU diagnostics; they are not GPU duration. Interrupted rect chains, overflow, nested independent canvases and unsupported flow gestures have warnings. Debugger inspection never dispatches game actions or changes authored values.

UI Inspector configuration and viewport gestures share the scene/prefab document history. Use **Edit → Undo/Redo**, **Ctrl+Z**, **Ctrl+Shift+Z** or **Ctrl+Y**. Each completed property interaction or move/resize gesture creates one named entry; anchor presets and asset assignments commit immediately. Sprite-sheet Save and Assign also records the source change when its captured scene target is UI; component creation itself remains outside history. Text fields keep their own ImGui undo while active. History never edits Play/Simulate copies; Stop retains the authored history and valid selection.

Invalid samples keep the last valid authored values. Escape, window focus loss/minimization, selection/document changes and interrupted targets cancel unfinished edits. Flow positions stay computed. Save finishes valid Inspector edits and cancels an unreleased viewport gesture; the accepted save point changes only after the write succeeds. Exact binary authored-content comparison determines dirty state, including native edits outside history and an evicted save point.

Scene and prefab histories are independent and close/reset with their documents. Limits are 100 committed operations and 64 MiB of conservatively accounted retained command data; one in-flight command is separately capped at 64 MiB. Oldest entries are evicted with feedback. The existing accepted authored-content baseline and temporary validation/storage are outside that command budget. History stores values/IDs/lifetime guards, never render caches, fonts/GPU objects or managed delegates. Vectors/colors are atomic authored fields for conflict checks; unrelated fields survive Undo/Redo, and a field takes its before-value when first edited. Conflicting native writes, retired components/entities or changed ancestor chains reject Undo/Redo without partial mutation; **Edit → Clear this document's history** removes obsolete entries.

Unassigned texture/region picker modes remain editable and serialize as the existing `None` source; assign an asset before saving to retain its source mode. Generated native images keep their resource during color edits; changing their source is rejected because history cannot retain a GPU object.

This slice covers Canvas, Rect, Flow, Text Style, Nine Slice and Button configuration, plus their text/image presentation. Creation/deletion, component addition/removal, hierarchy operations, world transforms, script fields and other document types are not yet undoable; their changes still affect exact dirty detection. Content sizing and the wider production control system remain later milestones.
