# Authoring runtime game UI

Runtime UI is ordinary scene content; it runs in Nutella without ImGui. World and UI text share shaped CPU layout and glyph-ID pages; sprites keep their existing rendering path. Gameplay decisions belong in C#.

## Build a menu

1. In Hazelnut's Hierarchy, choose **Add Entity → Runtime UI → Screen** for a menu, pause or dialogue. A Screen is a Canvas with Managed screen enabled and Block gameplay on by default. Use **Canvas** for an ordinary HUD. Set reference pixels (default 1280×720) and safe insets. Ordinary canvases use sort order; open screens draw above them in runtime open order.
2. Select the canvas and create **Panel**, **Text**, **Button**, **Stack**, **Grid** or **Wrap** through the same menu. All presets use the canonical measured model (Rect schema 3). A Stack defaults to Fixed width and Content height. Reselect the group before adding each sibling button; vertical-stack children use Fill width and buttons measure Content height. Rects use top-left logical pixels; Transform does not position UI.
3. Set Width/Height to **Fixed**, **Fill** or **Content** in UI Rect. Fixed authors a border-box size; Fill uses parent space; Content measures visuals/children plus padding. Minimum and optional finite maximum constrain each axis. Choosing free Fill explicitly gives that axis to the parent and clears its anchor/offset/pivot; margins set the insets. Content text width is natural unless capped, so use Fixed/Fill width or an enabled Content maximum to wrap labels. Stacks own child positions; use sizing, margins and Fill weight. Resize handles edit only Fixed dimensions; container children resize from right/bottom. Computed dimensions/positions stay read-only, and gestures never switch modes. If parent reflow/alignment would move the opposite edge, that resize sample is rejected; edit Fixed size in Properties instead. Escape cancels.
4. Text uses the existing **Text Component** for content, color, kerning and line spacing. **UI Text Style** supplies size, wrapping, Clip/Ellipsis overflow, Max lines and horizontal/vertical alignment. Size is logical pixels per font ascender-to-descender height. Kerning and line spacing remain font-height units, as in world text. Choose a project .hfont family and real face style in Text Renderer, or leave the family empty for engine Open Sans. Separate styled text children provide mixed emphasis; text has no markup.
5. Images use **Sprite Renderer** with the normal texture/sheet picker and UI Rect Image Fit. For framed panels add **UI Nine Slice**: source borders are left/top/right/bottom pixels within the selected source region, with a separate border scale. Borders must fit; small targets proportionally clamp their display borders. Corners keep their shape, edges stretch in one direction, and the center fills. Layout owns the UI pivot; sprite pivots remain unchanged for world sprites.
6. Buttons have an action identity, tab order, interactable state and five editable tint/image states. An unassigned state image inherits Sprite Renderer. Keyboard focus has a persistent outline, including hover/press. Controls activate on pointer release inside their live clipped bounds with framebuffer coverage, or the configured Accept action while focus belongs to UI. Tab/Shift-Tab traverses tab order; arrows and gamepad navigation use spatial neighbors. Cancel or a click outside controls releases nonmodal keyboard ownership. A modal canvas blocks gameplay and buttons in other canvases. Higher canvas sort order wins; ties use stable entity identities; child hierarchy order supplies painter order within a canvas.
7. Use the viewport's Window/Small/Widescreen/Classic aspect selector to preview the actual runtime layout. Editing does not dispatch actions. In Play, pointer coordinates map to the displayed image, including its letterboxing. Stop before changing authored values. **Validate UI** checks configuration/assets. Save/reopen as a normal scene. Use the existing **Create Prefab** workflow on the canvas/group to save a reusable subtree. Detached rect prefabs have a static default-canvas editor preview; runtime instances need a canvas ancestor.

The examples contain editable **TitleUI.hprefab** assets. Each keeps its game's own textures/palette. All UI assets and button-state references use project ownership and export closure.

## Connect C# decisions

Attach an ordinary Entity script to the screen/controller and select it in
**UI Canvas → Event owner**. Each managed scope has one declared recipient. The
engine derives the consuming entity from its executing managed lifecycle callback;
an unrelated script cannot pass another owner's ID to steal its events.

```csharp
using Hazel;
public class MainMenu : Entity {
    public Entity Settings; // assign the child screen in Properties
    void OnUpdate(float dt) {
        UIEvent value;
        while (GameUI.TryConsumeEvent(out value)) {
            if (value.Kind == UIEventKind.Activated && value.Action == "settings")
                Settings.GetComponent<UICanvasComponent>().Open();
            else if (value.Kind == UIEventKind.Cancel)
                value.Screen.GetComponent<UICanvasComponent>().Close();
        }
    }
}
```

Use **Cancel → Close screen** for dismissal that needs no script decision. Custom
Cancel uses **Notify owner**; the optional Cancel context identifies a button in
that semantic event, without activating it. Initial focus defaults to the first
eligible TabOrder/painter target. A configured initial target is tried first;
missing/ineligible targets diagnose and fall back. Default action is selected
only when no eligible control has focus, so Accept never bypasses a focused
button. `GameUI.Focus(entity)` selects an eligible current-scope control; null
clears focus. Explicit nonzero focus requires a live input owner; use the authored
initial target for opening. Requests to focus a different/inactive scope reject.

`UICanvasComponent.Open()`, `Close()`, `ReplaceTop()` and `GameUI.PopScreen()`
queue operations. Their bool means enqueued, not already committed. Commit occurs
before input and after script updates, outside control iteration/callbacks.
Initially open is authored; runtime order never edits Visible or the document.
Open/Close are idempotent; reopening an open screen does not raise/reset it.
ReplaceTop requires a closed target or the existing top; an already-open lower
target is rejected without closing anything. Pop closes the last open screen.
Closing a non-top screen leaves higher screens open. Hidden/disabled roots remain
remembered but unavailable; opening a closed unavailable root rejects at commit.
Independent nested canvases retain their independent geometry and runtime order.

The foreground available managed screen owns UI focus/hits, including a nonmodal
overlay. Ordinary canvases beneath it cannot receive UI actions. Nonmodal does not
impose unconditional gameplay blocking: the existing keyboard/pointer gate applies.
Any available modal screen still blocks gameplay, including one without buttons.
Without a foreground managed screen, ordinary highest-modal/scene-zero scopes
continue through the same published geometry/hit pipeline.

Closing restores eligible remembered underlying focus, then the closing screen's
Return fallback, underlying initial focus and first eligible target. Eligibility
includes scene identity, scope membership and effective framebuffer clipping.
Underlying keyboard ownership is restored too: a pointer-opened modal does not
unexpectedly acquire gameplay keys when closed. Transitions cancel capture/events
and require an enabled neutral sample before routing fresh gestures. Held inputs
remain gated by RuntimeSession until neutral; no opening/closing gesture clicks
through. Requests are FIFO, at most 64 pending, with at most 64 declared screens;
newest overflow rejects with diagnostics. Focus-return memory holds 64 records,
evicting oldest. A failed/retired request does not partially perform that operation.

Managed button activation and Notify-owner Cancel share one bounded delivery queue
(64 records, drop newest). Events carry source/screen entities, kind, sequence and
the existing action identifier (128 bytes, no NUL). No lifecycle notifications or
retained delegates are needed. Input events become visible to that frame's script
updates; screen changes requested there commit after all scripts return. Screen
changes, ownership loss, retired sources/owners, scope changes, Stop and domain
replacement retire stale records. Owner/class/lifetime changes cannot transfer an
old record. Queued managed requests also validate their issuer before commit.

Unmanaged canvases retain `UIButtonComponent.TryConsumeAction`,
`GameUI.TryConsumeAction` and scoped `TryConsumeCancel` conveniences. Those methods
cannot consume a managed screen's records; there is one delivery stream, without
duplicate activation. Use a managed screen for a menu with a declared owner.
Native consumers use the equivalent Scene request/focus/owner-consume APIs on the
scene thread. Native field writes are reconciled before candidate delivery.

Use `UIRectComponent.Visible/Enabled`, `UICanvasComponent.Visible/Enabled/Modal`,
`UIButtonComponent.Interactable` and existing Text properties for dynamic values.
References remain checked against the current scene. Visibility/structural changes
retire work before delivery. Use Open/Close for managed screen lifetime rather
than manually reproducing focus and modal cleanup with visibility toggles.

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

Text uses strict UTF-8 scalar decoding for measurement, wrapping and drawing. One leading BOM is ignored; U+FEFF elsewhere is a retained format scalar with zero visual advance. Embedded NUL, malformed/truncated sequences, overlong encodings, surrogates and values above U+10FFFF are rejected. Managed setters also reject invalid UTF-16 before conversion, preserving the previous value. UI layout input is limited to 64 KiB / 16,384 scalars; output is capped at 65,536 glyphs and 4096 lines/runs. Shaping, bidi and grapheme-safe line breaks use the shared Unicode 17 CPU service. The tested corpus covers Latin accents/ligatures, Greek, Cyrillic, Arabic joining and mixed Hebrew/Arabic/Latin/numbers; this is not an all-Unicode guarantee. A selected face must cover the complete text: there is no cross-family fallback.

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

Text antialiasing uses straight tint RGB and coverage × authored opacity as source
alpha. World/UI text shares the correction; brighter edges are expected. Text
writes source-over alpha and restores blend state before images. The engine has
no automatic sRGB conversion; mixed transparent targets still inherit the older
sprite-alpha semantics and are not general opacity layers.

**Text Renderer → Font details** shows the actual atlas and has **Reload family** for explicit
freshness. Failed reload keeps the accepted font; first-load failures use shipped
Open Sans. The authored family selection remains intact and the Inspector/debugger explains
the error/fallback. Repair the file and Reload, or use the normal asset Refresh
before Play. A failure is cached rather than retried every frame. Missing-font
scenes can be saved/reopened for repair; export still rejects invalid closure.
Undo/Redo restores authored selections even if a file subsequently disappears.

**Runtime UI Debugger → Font resources** distinguishes the eight-entry face cache,
reusable pages (12 MiB) and live pages (24 / 24 MiB), including prepared-text and
pending-draw pins. Per page: at most 128 glyph IDs, 1024² / 3 MiB RGB8. Source
input remains 4 MiB per face; live CPU sources have their separate 24 / 32 MiB
budget. These estimates exclude driver/dependency allocations. Resource details
show page count/bytes, shaping reuse and page failures.

Pages group CPU-shaped printable ASCII together, then use glyph-ID blocks for
other requests; this is packing, not another typography path. Pages are immutable and prepared synchronously, so first use can pause while
outlines are generated. Graphical startup with a nonzero viewport prewarms world text and its families' declared Required/Sample text. UI prepares only its final constrained text at the actual paint boundary, so omitted text does not request pages. Unchanged UI reuses positioned
results and prepared glyphs; world text retains its CPU layout. Drawing never
rereads a font. Missing source glyphs show that face's `.notdef` (or a visible
metric placeholder if it has no outline), with a bounded warning. Page pressure
shows magenta rectangles at shaped positions without changing advances; release
unused screens/resources and **Reload family** to retry preparation. Authoring
keeps the family reference. Required coverage checks source/shaping support,
not GPU residency or a guarantee that every screen fits resource limits.

Size remains logical pixels per accepted font ascender-to-descender height.
The shared service converts that to em units; UI aligns whole shaped lines and
world text starts at the first baseline. `Kerning` is additional cluster tracking
in font-height units; OpenType kerning is already applied exactly once by
HarfBuzz. Start/End alignment remains physical left/right; UI layouts do not
automatically mirror for RTL paragraphs. Tracking never splits ligature/combining clusters and is suppressed
inside Arabic joining runs except whitespace. Line spacing adds font-height units
to the accepted face's line advance. Tabs advance four spaces; trailing spaces
are retained. Wrapping reshapes each chosen line with its own joining context.
CJK, color emoji, vertical/variable typography, discretionary soft hyphens,
cross-family fallback, rich text and editing/IME remain outside this scope.

## Text overflow

**UI Text Style → Overflow** defaults to **Clip**, with **Max lines = 0**
(unlimited within the 4096-line engine limit). Wrap remains independent. Clip
keeps the established element/inherited border-box pixel clip; padding positions
and wraps text without adding another clip. A positive Max lines removes whole
subsequent lines without a marker. Existing scenes keep these defaults.

**Ellipsis** fits complete font-height line boxes inside the padded content area,
then replaces the logical tail of each overwide line and/or the last retained
line with shaped U+2026 (`…`). Max lines also applies. A line fits vertically when
one font height plus its preceding line advances fits; authored line spacing is
included. Height edges allow four float ULPs for measure/arrange rounding; zero
remains empty. No complete line means no glyphs. If the marker alone is too wide, that
line paints nothing and the debugger explains why. Ink overhang clips to the
padded content box and inherited clip. Left/center/right remain physical alignment;
the jointly reshaped marker follows paragraph direction (visual left for RTL).

Source text is never edited. Explicit newlines and trailing empty lines count;
omitting even a trailing empty line warrants a marker. Empty source has no omitted
content. Content sizing measures width-constrained text and Max lines without
using its own eventual height as a constraint. Fixed/Fill and Content maxima keep
the existing layout ownership; padding is removed before text fitting. Painting
uses the final arranged content height. These intrinsic and final constraints may
differ without a circular dependency. Clipping ancestors do not change line layout.

Short-line ellipsis tests every grapheme-safe prefix, longest first, by reshaping
retained text **with** the marker. For more than 128 boundaries it tests a bounded
advance-estimated neighborhood plus geometric checkpoints; it may omit additional
clusters and reports conservative truncation. It never assumes prefix widths are
monotonic. Existing cumulative shaping/output limits still apply and reject the
whole result on exhaustion. The selected face supplies the marker; missing U+2026
uses its explicit `.notdef`/normal placeholder policy, never another family.

**Runtime UI Debugger** shows final/original line counts, omission, a marker that
cannot fit, conservative selection and missing marker coverage after painting.
Inspector edits participate in scene/prefab Undo/Redo. Managed scripts can use
`style.SetOverflow(UITextOverflow.Ellipsis, 2)`, `style.Overflow` and `style.MaxLines`;
invalid values reject without changing the accepted fields. Full strings survive
save/reopen, duplication, prefab reuse and Play/Stop.

## Font families and declared coverage

Text Renderer owns **Font family** and **Face style**, shared by world and UI text.
Empty family selects engine Open Sans (the four shipped real faces). UI Text Style
owns size, alignment, wrapping and overflow/line limits. Project families are `.hfont` assets; no face is
synthetically emboldened/slanted. An absent optional style explicitly uses Regular,
with a message in Properties. Family errors keep the live accepted face, or use
engine Regular if none survives; authored selections are never rewritten.

In Content Browser choose **New Font Family…**, select/import a Regular TTF/OTF
and its notice, and choose a project destination. The Font Family asset editor
opens on creation or double-click. Add real Bold/Italic/Bold Italic slots and their
associated notices only where supplied. **Choose / import…** selects project files
or copies an external file to a destination you choose inside Assets; it never
overwrites an existing file. Associate the correct notices yourself: an association
is not evidence that a font can legally be redistributed. Existing Open Sans
fixtures use its shipped Apache license; this milestone adds no fonts/dependencies.

Save, Reopen/discard, Reload saved and normal Save All/unsaved-document guards
apply to the family draft, like sprite-sheet assets. Failed writes/conflicts keep
the draft and accepted file; Save Copy/Reopen/Cancel use the existing conflict UI.
Family draft editing does not have document Undo; text fields keep their local
ImGui Undo. Text component family/style assignments do use scene/prefab history.
Drag a `.hfont` from Content Browser onto Text Renderer's family field (or type its
asset-relative path), choose a style, then Undo/Redo, save/reopen and Play/Stop.
**Font details → Edit family** opens the assigned asset; Reload family explicitly
rechecks its descriptor/dependencies. Runtime uses saved assets, never an unsaved
asset-editor draft. Native field changes and managed `text.SetFont("Fonts/UI.hfont",
FontStyle.Bold)` invalidate Content/wrapping and prepared glyphs together.

**Coverage declaration** holds Required text and informational Sample text, each
at most 8192 strict UTF-8 bytes / 4096 unique scalars. **Inspect coverage** is CPU
only; it does not generate/upload GPU pages. Results distinguish missing nominal
source glyphs from unsupported script/feature/segmentation behavior. The actual
CPU service checks shaping of the declaration; cmap presence alone does not
certify a language. Every provided style must satisfy Required text before export;
Sample remains informational. Newline, CR, tab, BOM and supported format controls
follow the text contract. Details are bounded to 32 code points; unsupported
layout reports its cause. Page residency/admission is separate: **Update preview**
uses the actual renderer and reports accepted pages, `.notdef` count and failures.
This is representative rendering, not competent-reader language acceptance.

Descriptor Version 1 uses `FontFamily: {Version, Name, Faces, Required, Sample}`.
Faces is a mapping keyed Regular/Bold/Italic/**Bold Italic**; each provided slot has
`File` and 1–8 `Notices` paths relative to **Assets**, not the descriptor directory.
Regular is required. Descriptors are capped at 64 KiB, names at 128 bytes, paths
at 1024 bytes; absolute/traversal paths and duplicate keys/notices reject. Normal
export audits all `.hfont` files and referenced families, every provided face and
its notices, then verifies that the dependencies are included. Packages preserve
relative structure and also ship engine Open Sans/resources/licenses. No system
font or source-checkout fallback is used. Nonempty obsolete UI FontPath fields
are unsupported; writers only emit Text FontFamily/FontStyle. An empty historical
marker carries no selection and is ignored (including the preserved Sandbox draft).

Measurement, wrapping, family previews, ordinary world text and runtime UI now
consume the same immutable CPU results and accepted face versions. No old/new
renderer mode or fixed Latin-1 path remains. Test-only DejaVu/corpus resources
stay outside game packages unless explicitly imported into a project; packages
include the project's declared faces/notices and engine defaults, without system
font or SDK fallback. Review real Arabic/Hebrew content with competent readers.

## UI input and focus

**Project Settings → Runtime UI bindings** edits the project's one UI map. Expand
an action to choose/capture a primary or alternate key, match Shift and select a
standard gamepad button. Pointer Press uses its separate mouse button. Choose a
left/right stick or None, engage/neutral thresholds and navigation repeat timings.
Escape cancels capture; select Escape from the key list to bind it. Release keys
before capture. Conflicts/invalid values block Save; Revert discards the form and
Reset chooses engine defaults. Save uses normal project conflict/atomic-write
handling. It does not alter editor-wide shortcuts or game save data.

Defaults are arrows for **spatial** navigation, Tab/Shift+Tab for tab order,
Enter/Space or gamepad South for Accept, Escape or East for Cancel, D-pad/left
stick for navigation and left mouse for Press. Keyboard codes are physical keys,
not text input. Ctrl/Alt/Super chords belong to the host, cannot be assigned here
and require rearming before UI resumes. The first standardized mapped controller
is used; unmapped joysticks are not UI gamepads. Physical Windows/controller
acceptance remains separate from synthetic regression coverage.

Initial navigation selects the first eligible tab target (last for Previous).
Subsequent directions choose a visible neighbor, using effective clipped centers:
forward distance squared + four times perpendicular distance squared. Ties use
TabOrder/painter rank; no candidate leaves focus in place, without directional
wrap. Tab wraps. Fully clipped, zero-covered-pixel, hidden and disabled controls
cannot focus; partial clips use their visible region. Pointer presses focus their
hit target. Motion alone never steals focus. A pointer press suppresses held
navigation repeats until those directions rearm.

Stick engage/neutral defaults are .55/.25. A selected direction stays latched
until neutral, including when reversing the stick. Opposing directions cancel;
a diagonal chooses horizontal. Navigation repeats after .35 s, every .10 s,
at most one repeat per action/update and at most .1 s of repeat clock per update.
Accept and Cancel never repeat. Multiple Accept bindings form one gesture until
all sources release. Held input on connect, restore or rebind cannot activate;
disconnect/connection revision retires input ownership and requires rearming.

The scene UI service owns screen order, interaction scope and modal blocking;
the router only consumes its published eligible controls. Managed-screen policies
and ordinary Canvas behavior are described under Connect C# decisions above.
Tab order is local to the published active scope.

Cancel cancels gestures/pending activations once per sampled update (multiple
Cancel edges coalesce). Managed screens use Ignore, Close screen or Notify owner;
unmanaged scopes expose `canvas.TryConsumeCancel()` or scene-zero
`GameUI.TryConsumeCancel()`. It never invents a button click or unloads a scene.
Scene/domain/Stop, ownership loss and scope replacement retire records. A consumed
UI gesture remains gated from gameplay until held inputs in the effective UI map return neutral,
including across a scene transition. Native raw `Input` is still physical polling;
native gameplay callers must honor RuntimeSession's established gate.

`GameUI.GetBinding(action)` / `SetBinding(action, binding)`, `InputSettings` and
`ResetBindings()` expose validated player overrides. Digital actions are Up,
Down, Left, Right, Next, Previous, Accept and Cancel; Press uses InputSettings'
PointerButton. Stick is -1 None, 0 Left or 1 Right. Setters cancel/rearm input and
reject conflicts without changing effective values. For example:

```csharp
var accept = GameUI.GetBinding(UIAction.Accept);
accept.Alternate = (int)KeyCode.Z;
GameUI.SetBinding(UIAction.Accept, accept);
```

Nutella persists a complete override in RuntimeStorage's reserved
`engine-ui-bindings` slot under the project's SaveNamespace. A reset follows
project defaults again. A project without a SaveNamespace can use defaults;
persistent override writes reject with a clear error. Editor Play overrides stay
in memory and end with the session. Corrupt player data is left intact, warns once
at Start and falls back to usable project defaults. Successful writes precede
publication. No packaged asset or editor preference is rewritten.

**Runtime UI Debugger → Effective bindings / Play override** shows the actual map,
last action device, pad rearming, scope/focus/capture, navigation misses and edge
limits. Its override draft is separate until Apply; Read discards draft changes.
The input buffer holds 64 ordered edges; overflow cancels the frame and rearms
rather than delivering a partial gesture. Existing button queues remain bounded
and destructive. Scope-return memory holds 64 scope/target records, evicting oldest entries.

## Author a composed screen flow

Create a Menu Screen and attach its owner script. Create Settings and Confirmation
screens as children so the connected subtree can be saved as one prefab; turn
Initially open off on those two. Add current Panel/Text/Button presets, give buttons
meaningful actions and assign the same controller as each screen's Event owner.
Choose initial focus with the named local entity selector. Settings/Confirmation
can use built-in Close on Cancel; choose Notify owner when a game decision is
needed. Assign the controller's Entity fields through normal script properties.
No manual focus stack or gameplay blocking code is needed.

Selecting a managed screen or its children previews that screen in Edit, including
a closed screen, without changing initial state. With no managed selection,
initially-open screens compose above ordinary canvases. Prefab preview follows its
selection too. Screen policy edits and target assignments use shared scene/prefab
Undo/Redo; structural creation and script fields retain their existing separate
limitations. Save/reopen, Play, open Settings, open Confirmation, Cancel twice and
check focus returns to each invoking control. Stop preserves authored policy and
history. Prefab references must be internal to the saved subtree (or cleared).

**Runtime UI Debugger → Screens and scoped events** shows open/available order,
foreground scope, owner, pending requests/events, retired delivery and the most
recent issue. Target warnings are attached to resolved Canvas nodes. This is
transient inspection, not a second editor/runtime screen manager.
