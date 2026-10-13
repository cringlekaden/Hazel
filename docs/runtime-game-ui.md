# Authoring runtime game UI

Runtime UI is ordinary scene content; it runs in Nutella without ImGui. World text and sprites retain their existing behavior. Gameplay decisions belong in C#.

## Build a menu

1. In Hazelnut's Hierarchy, choose **Add Entity → Runtime UI → Canvas**. Set reference pixels (default 1280×720), safe insets and sort order in the Inspector. Enable **Block gameplay** for a menu, pause or dialogue canvas.
2. Select the canvas and create **Panel**, **Text**, **Button** or **Flow group** through the same menu. New elements become children. Rects use top-left logical pixels. Edit position and size in the Inspector, or drag a selected element in the viewport; the lower-right handle resizes it. Transform remains a world component and does not position UI.
3. Use anchor presets for corners, center, bottom captions or stretch. Anchor coordinates are normalized; offsets, size deltas, minimum sizes, margins and padding are logical pixels. Stretch size deltas add/subtract from the anchor span. A flow group owns child positions; child size/minimum/margin/flex control its horizontal or vertical arrangement. Hidden children consume no flow space.
4. Text uses the existing **Text Component** for content, color, kerning and line spacing. **UI Text Style** supplies size, wrapping and horizontal/vertical alignment. Size is logical pixels per font ascender-to-descender height. Kerning and line spacing remain font-height units, as in world text. Drag a project TTF/OTF onto Font asset, or leave it empty for the packaged default font. Separate styled text children provide mixed emphasis; text has no markup.
5. Images use **Sprite Renderer** with the normal texture/sheet picker. For framed panels add **UI Nine Slice**: source borders are left/top/right/bottom pixels within the selected source region, with a separate border scale. Borders must fit; small targets proportionally clamp their display borders. Corners keep their shape, edges stretch in one direction, and the center fills. Layout owns the UI pivot; sprite pivots remain unchanged for world sprites.
6. Buttons have an action identity, tab order, interactable state and five editable tint/image states. An unassigned state image inherits Sprite Renderer. A focused button also has an outline. Controls activate on pointer release inside their live clipped bounds, or Enter/Space while keyboard focus belongs to UI. Tab starts navigation; Tab/Shift-Tab and arrows traverse. Escape or a click outside controls releases nonmodal keyboard ownership. A modal canvas blocks gameplay and buttons in other canvases. Higher canvas sort order wins; ties use stable entity identities; child hierarchy order supplies painter order within a canvas.
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

Use `UIRectComponent.Visible/Enabled`, `UICanvasComponent.Visible/Enabled/Modal`, `UIButtonComponent.Interactable`, and existing `TextComponent.Text/Color/Kerning/LineSpacing` for dynamic presentation. Entity references are checked against the current scene. Actions contain identities/strings, never delegates; destroyed/hidden/disabled controls cannot deliver stale work. Scene transition, Stop, script-domain reload and focus loss retire interaction state. Actions are bounded and consumed once; do not have multiple controllers consume the same global queue.

`Input` supplies gameplay input and respects UI ownership. `GameUI.IsKeyDown` supplies viewport-gated keys for UI-specific shortcuts such as Escape to close a modal or N to confirm starting over. `GameUI.IsPointerCaptured` lets alternate shortcuts defer to an active pointer gesture while retaining their held-key history. Keep gameplay movement and interaction on `Input`; do not bypass modal blocking with this UI API. Visibility or enabled changes update native layout/routing, with no per-frame positioning script.

UI currently uses rectangular inherited clipping, uniform reference scaling, bounded flows and sibling text emphasis. It does not provide scrolling lists, rich-text parsing, editable text boxes or automatic content-size fitting. The native layout/model and renderer abstraction are independent of editor ImGui and backend-specific graphics calls.
