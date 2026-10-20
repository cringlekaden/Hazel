# Authoring runtime game UI

Runtime UI is ordinary scene content; it runs in Nutella without ImGui. World text and sprites keep their existing rendering paths; text shares the strict decoder described below. Gameplay decisions belong in C#.

## Build a menu

1. In Hazelnut's Hierarchy, choose **Add Entity → Runtime UI → Canvas**. Set reference pixels (default 1280×720), safe insets and sort order in the Inspector. Enable **Block gameplay** for a menu, pause or dialogue canvas.
2. Select the canvas and create **Panel**, **Text**, **Button**, **Stack**, **Grid** or **Wrap** through the same menu. All presets use the canonical measured model (Rect schema 3). A Stack defaults to Fixed width and Content height. Reselect the group before adding each sibling button; vertical-stack children use Fill width and buttons measure Content height. Rects use top-left logical pixels; Transform does not position UI.
3. Set Width/Height to **Fixed**, **Fill** or **Content** in UI Rect. Fixed authors a border-box size; Fill uses parent space; Content measures visuals/children plus padding. Minimum and optional finite maximum constrain each axis. Choosing free Fill explicitly gives that axis to the parent and clears its anchor/offset/pivot; margins set the insets. Content text width is natural unless capped, so use Fixed/Fill width or an enabled Content maximum to wrap labels. Stacks own child positions; use sizing, margins and Fill weight. Resize handles edit only Fixed dimensions; container children resize from right/bottom. Computed dimensions/positions stay read-only, and gestures never switch modes. If parent reflow/alignment would move the opposite edge, that resize sample is rejected; edit Fixed size in Properties instead. Escape cancels.
4. Text uses the existing **Text Component** for content, color, kerning and line spacing. **UI Text Style** supplies size, wrapping and horizontal/vertical alignment. Size is logical pixels per font ascender-to-descender height. Kerning and line spacing remain font-height units, as in world text. Drag a project TTF/OTF onto Font asset, or leave it empty for the packaged default font. Separate styled text children provide mixed emphasis; text has no markup.
5. Images use **Sprite Renderer** with the normal texture/sheet picker and UI Rect Image Fit. For framed panels add **UI Nine Slice**: source borders are left/top/right/bottom pixels within the selected source region, with a separate border scale. Borders must fit; small targets proportionally clamp their display borders. Corners keep their shape, edges stretch in one direction, and the center fills. Layout owns the UI pivot; sprite pivots remain unchanged for world sprites.
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

UI currently uses rectangular inherited clipping, uniform reference scaling, free/stack/grid/wrap layout and sibling text emphasis. It does not yet provide scrolling lists, rich-text parsing or editable text boxes. The native layout/model and renderer abstraction are independent of editor ImGui and backend-specific graphics calls.

## Canonical sizing and containers

Rect schema 3 is the single supported layout model. Older or unversioned Rects produce a clear unsupported-schema error; the old resolver and conversion UI have been removed. Repository scenes/prefabs have been reauthored; Git checkpoints retain their earlier form. No automatic conversion or save occurs.

Free positioning uses one point Anchor, Offset and Pivot on the final border-box size. Margins are outside; padding is inside. Fixed authors a border-box dimension. Fill owns its free position and uses available space after margins; anchor/offset/pivot must be zero on that axis. Content measures visuals/children plus padding, then applies minimum and optional finite maximum. A Content maximum or Fixed/Fill width supplies the text wrapping constraint before Content height. Maximum must be at least minimum; overflow never silently shrinks minima. Clipping affects painting/input, not measurement.

Stack positions are computed in hierarchy order. Fixed/Content children and Fill minimums/margins/gaps reserve space first, then remaining space is shared by Fill weight (`Flex=0` means 1), redistributing at maximums. Main/Cross alignment place spare space. Hidden stack children collapse; disabled children retain layout. Empty Content measures padding; empty/zero-width text measures zero. Free Content children require zero anchor/pivot on the Content axis. Indefinite Content/Fill dependencies reject with entity/axis paths. Nested canvases are independent surfaces; attach sizing to a child Rect rather than the Canvas itself.

**Grid** uses explicit Columns and Rows, each with Fixed logical pixels or positive Fill weight, minimum and optional maximum. There are 1–64 tracks per axis; no implicit/content-sized tracks or packing. Column/row gaps and panel padding reserve space. Fixed empty tracks keep their size. Fill tracks share remaining space with capped redistribution and may overflow at their minima. A Content container axis uses the larger of its visual intrinsic size and the sum of Fixed tracks/gaps, plus padding; Fill tracks on that axis are indefinite and rejected.

Grid children occupy one cell automatically unless row/column and spans are authored in UI Rect. Explicit placements reserve first, then automatic children find the first fitting row-major cell in hierarchy order. Hidden children retain their cell reservation, keeping later icons stable; they do not draw or hit. Overlap, out-of-range placement, exhausted capacity and excessive placement work (16M row-mask checks per grid) reject before publishing. Use explicit cells for dense spanned grids. When switching an existing group, prepare **Grid settings (inactive)** before selecting Grid if its children need more tracks. Explicit Cell selection updates both coordinates atomically; turning off Automatic freezes the currently resolved cell. Positions/anchors/offsets/pivots are owned by the cell; margins inset it and child Fixed/Fill/Content sizing plus cell horizontal/vertical alignment arrange within it. Oversized minima overflow and are diagnosed. Cells add no scissor: overflow may overlap neighboring cells in painter order, subject to inherited clipping. Add tracks before adding more children; a static icon grid is not a complete inventory/data view.

**Wrap** requires a Fixed/Fill main axis: width for horizontal rows, height for vertical columns. Content main size is rejected even with a maximum. Visible children break greedily in hierarchy order using Fixed/Content sizes or Fill minimums, with margins/item gaps. An oversized item retains its own overflowing line. Each line then shares its remaining main space between Fill children with weights/caps; it does not rebalance earlier breaks. Cross thickness is the largest non-Fill outer size or Fill minimum; Cross Fill fills that line, not the whole panel. Cross alignment places children within a line; line block alignment positions the collection in a definite cross size. Line gap separates lines. Content cross size is summed line thickness/gaps/padding. Hidden children collapse; disabled retain space. A vertical item whose Content height needs a cross-Fill width before its column thickness is known is indefinite: give it a definite width or minimum/Fixed height instead. Diagnostics include the entity/axis dependency path.

Images measure base source-region pixels as logical content units, independently per axis; padding, text and child content can enlarge that box. Button-state images and animation frames do not reflow layout. Nine-slice uses the arranged border box. Image fitting is a separate paint policy.

Image Fit in UI Rect separates the arranged box from art presentation. **Stretch** fills the border box, retaining solid-color panels and existing nine-slice behavior. **Contain** preserves aspect inside the padded content box and leaves aligned letterbox space. **Cover** fills that content box and crops normalized source UVs. **Native size** uses one source pixel per logical unit, aligns smaller art and crops larger art; canvas scaling still applies. Alignment ranges from left/top (0) to right/bottom (1). A missing source has no fitted art outside Stretch. Atlas measurement uses authored region dimensions, not the whole texture or the half-texel-inset sampling extent; crops remain within the existing safe region UVs and retain the asset's sampling. No image-fit setting changes world sprites.

Combined text/images share the control's content area; text alignment/wrapping measures independently, so use sibling elements when you want separate image and label regions. State images use their own source extent for painting but the base image for intrinsic layout. Nine-slice owns border fitting and requires Stretch; choose Stretch before adding the component. Incompatible edits reject. Button input/focus uses the arranged control box and effective clip, including padding/letterbox space. Editor framebuffer picking follows painted art (select an empty fitted area through the Hierarchy); layout handles/debugger still show the full box.

For an icon group, create a Grid, set its row/column count before adding buttons, and use Fill-width children with Contain images. Assign sprite regions through the normal sheet picker. Use the debugger's track/cell outlines to inspect spans and overflow. For responsive chips, create a horizontal Wrap with Fill width and Content height, give children Fixed or Content widths, and narrow the runtime preview to check line breaks. A vertical Wrap needs definite height instead. Changing container mode never converts child sizing automatically; prepare inactive grid settings and choose meaningful child constraints first.

For a HUD, add a Content text child to a Canvas, set its free Anchor/Pivot to the desired corner and use logical offsets. For a menu, use a stack with constrained width, Content height, padding/gap and Fill-width, Content-height buttons. Change a label to verify wrapped reflow, preview another aspect, Undo/Redo, save/reopen and Play/Stop. Broader controls remain later milestones.

For measured runtime content, cache the normal component wrapper and change sizing when values change:

```csharp
rect.Width = new UISizing(UISizeMode.Fixed, 320, minimum: 160, maximum: 480);
rect.Height = new UISizing(UISizeMode.Content);
```

`LayoutVersion` is the read-only current schema identifier in C#. C++ can use `Scene::SetUISizing`, `SetUILayout`, `SetUIGridCell`, `SetUIImageFit` or existing direct component writes. Validated sizing operations reject bad values/dependencies and retain the old authored values. They do not enter authoring history while Play uses its isolated scene copy.

For grids, cache `UILayoutComponent` and call `SetGrid(columns, rows, gap)` with `UITrack` arrays; `UITrackMode.Fixed` values are pixels and `Fill` values are weights. `Settings` reads/writes container settings while retaining tracks; `Apply(settings, columns, rows)` updates them atomically. Use `UILayoutSettings.Wrap(direction, gap, lineGap)` for wrapping. `Columns`/`Rows` return value copies, and a Rect's `SetImageFit(mode, alignment)` updates presentation atomically. `ImageFit` and `ImageAlignment` also expose those values. Its `Cell` accepts `UIGridCell.Automatic` or explicit zero-based row/column/spans. Invalid operations throw and preserve accepted values. Change these values on events rather than rebuilding arrays every frame.

A two-column runtime grid can be updated as one validated operation:

```csharp
layout.SetGrid(
    new[] { new UITrack(UITrackMode.Fixed, 96), new UITrack(UITrackMode.Fill, 1) },
    new[] { new UITrack(UITrackMode.Fixed, 64) }, new Vector2(8, 8));
rect.Cell = new UIGridCell(0, 1);
rect.SetImageFit(UIImageFit.Contain, new Vector2(0.5f, 0.5f));
```

## Resolved state and mutation boundaries

The scene publishes indexed, read-only geometry with a monotonic generation. Borrowed snapshot/element references last until the next resolution or reset; retain IDs/generation rather than pointers across those boundaries. Canvas logical units, viewport pixels and framebuffer pixels are distinct. RuntimeUI accepts viewport-coordinate input and maps it once to the router list’s framebuffer geometry. Hit eligibility and rendering share inward-rounded framebuffer clipping (`[ceil(min), floor(max))`), with half-open edges; zero-size or subpixel regions with no covered framebuffer area cannot focus or hit. Disabled buttons block lower pointer targets; ordinary panels/images/text pass through. Modal canvases restrict controls to their canvas. A pointer gesture owns its release/cancel frame, so simultaneous keyboard Accept cannot turn a cancelled click into an activation.

`Scene::RefreshUI` compares exact layout/input dependency values, component membership and relevant ancestor links. It uses reusable binary buffers, without hashing, YAML or full-document snapshots. Input, rendering and pending-action delivery are boundaries: unchanged values reuse the generation; native field writes are detected at the next boundary. Multiple writes coalesce to the final observed values. Content additionally observes text/font/base-image metrics and asset epochs; color/state-image changes remain paint dependencies. Wrapped text lines and intrinsic image metrics are cached across unrelated layout changes. Asset freshness uses explicit reload/refresh epochs, without per-frame filesystem checks. Structural APIs and managed state setters cancel affected gestures immediately. Native callers needing fresh bounds/ownership between normal boundaries call `RefreshUI`; `GetUISnapshot`/`GetUIBounds` otherwise read the published generation. Component references retain the existing EnTT lifetime: reacquire them after structural component/entity edits. Raw C++ `Input` still polls devices; session/scene gating must be honored by native gameplay callers.

Hosts supply viewport size and framebuffer extent before input/render (`RuntimeSession::ResizeFramebuffer` / `Scene::SetUIFramebufferSize`). Changing geometry after input may publish a newer render generation; it never replays that input. Limits are 4096 declared UI rects including hidden ones, 4096 container children and scene hierarchy depth 256. Invalid derived values reject resolution and cancel interaction; Edit retains the document and displays the error.

Text uses strict UTF-8 scalar decoding for measurement, wrapping and drawing. One leading BOM is ignored; U+FEFF elsewhere is a retained format scalar with zero visual advance. Embedded NUL, malformed/truncated sequences, overlong encodings, surrogates and values above U+10FFFF are rejected. Managed setters also reject invalid UTF-16 before conversion, preserving the previous value. UI text remains limited to 64 KiB. Encoding support does not expand the current Latin-1 atlas or add shaping/fallback fonts.

## Inspect and edit UI

Open **View → Runtime UI Debugger**. Inspect available constraints, measured/arranged size, sizing mode, controlling parent, logical/pixel/framebuffer bounds, effective clip, grid cell/track/wrap-line geometry and last painted image bounds/crop, canvas, generation, eligibility and container ownership. Focus/capture/modal and gameplay blocking are transient runtime state. Bounds (including tracks, wrap lines and fitted image outlines), clip and hit-order overlays are optional. Rebuild/reuse/source-check, text/image measurement cache and arrangement counters plus last resolve/routing timings are CPU diagnostics; they are not GPU duration. Interrupted rect chains, overflow, nested independent canvases, dependency errors and unsupported gestures have diagnostics. Debugger inspection never dispatches game actions or changes authored values.

UI Inspector configuration and viewport gestures share the scene/prefab document history. Use **Edit → Undo/Redo**, **Ctrl+Z**, **Ctrl+Shift+Z** or **Ctrl+Y**. Each completed property interaction or move/resize gesture creates one named entry; anchor presets and asset assignments commit immediately. Sprite-sheet Save and Assign also records the source change when its captured scene target is UI; component creation itself remains outside history. Text fields keep their own ImGui undo while active. History never edits Play/Simulate copies; Stop retains the authored history and valid selection.

Invalid samples keep the last valid authored values. Escape, window focus loss/minimization, selection/document changes and interrupted targets cancel unfinished edits. Container positions stay computed. Save finishes valid Inspector edits and cancels an unreleased viewport gesture; the accepted save point changes only after the write succeeds. Exact binary authored-content comparison determines dirty state, including native edits outside history and an evicted save point.

Scene and prefab histories are independent and close/reset with their documents. Limits are 100 committed operations and 64 MiB of conservatively accounted retained command data; one in-flight command is separately capped at 64 MiB. Oldest entries are evicted with feedback. The existing accepted authored-content baseline and temporary validation/storage are outside that command budget. History stores values/IDs/lifetime guards, never render caches, fonts/GPU objects or managed delegates. Vectors/colors are atomic authored fields for conflict checks; unrelated fields survive Undo/Redo, and a field takes its before-value when first edited. Conflicting native writes, retired components/entities or changed ancestor chains reject Undo/Redo without partial mutation; **Edit → Clear this document's history** removes obsolete entries.

Unassigned texture/region picker modes remain editable and serialize as the existing `None` source; assign an asset before saving to retain its source mode. Generated native images keep their resource during color edits; changing their source is rejected because history cannot retain a GPU object.

History covers Canvas, Rect, Flow, Text Style, Nine Slice and Button configuration, their text/image presentation, measured sizing, grid tracks and placement. Creation/deletion, component addition/removal, hierarchy operations, world transforms, script fields and other document types are not yet undoable; their changes still affect exact dirty detection. The wider production control system remains later work.

Runtime UI rendering keeps hierarchy/canvas painter order, including translucent
images, interleaved text, focus outlines and nested clipping. Adjacent compatible
geometry batches automatically; font/primitive changes retain ordered draws.
Changing clips does not reorder elements. Fitted images and nine-slice use the
same arranged bounds and entity IDs as before. There is no author-facing batching
mode or texture sort.

Open **Runtime UI Debugger → Rendering costs** for UI-only submitted elements,
draw runs, upload groups, clip transitions and capacity/texture flush counts.
CPU source/layout, preparation and submission are separate measurements. GPU
intervals arrive later and show their submission age; they can include gaps while
the CPU feeds commands and exclude window presentation. These counters do not
establish a whole-game frame budget. Use Release `MigrationGameUIIntegrationSmoke
--profile-render` for the reproducible bounded pass profile and `--profile-layout`
for changed/unchanged grid/wrap CPU and allocation measurements.

Text preparation reuses normalized glyph geometry when its text, font/asset epoch,
width, spacing, size and alignment are unchanged. Position and tint still repaint
immediately, including native writes that leave layout generation unchanged.
Retained glyph geometry is capped at 16 MiB per scene; labels over 4 KiB and
budget-exhausted labels draw through uncached preparation. This cap does not bound
all font/atlas or document memory. Large alternating image/text interfaces still
need many ordered draws; the implementation record documents the measured limits.
