# Runtime UI production audit and implementation design

This is an audit and proposed design, not an implementation. **Verdict: a useful, integrated first runtime UI milestone; not yet a general production menu system.** Hazel already owns UI in scenes, renders it without editor ImGui, authors it natively, and packages its resources. Preserve those foundations. Ordinary settings, inventories, scrolling journals, text entry, shared themes, controller navigation, and world-space interaction still require substantial engine work or game-specific reconstruction.

“Production” below means passing a bounded support contract and its acceptance gates. It does not mean correctness for arbitrary scenes, every language/font, every display configuration, or platforms that have not been tested.

## 1. Baseline and evidence boundaries

### Repository selection

- Repository: [cringlekaden/Hazel](https://github.com/cringlekaden/Hazel).
- Audited source: **`f50c5559d015086573a1c3848d6c9490d23e3073`**, `master`, merge “Merge The Last Lightkeeper and runtime UI into master [skip ci]”. Parents: `463cbdd` and `753c77a`.
- `git fetch origin` succeeded. Initial `master`, `origin/master`, and `origin/HEAD` agreed; the initial working tree was clean. `feature/last-lightkeeper` and its remote tip were **`753c77a25b86a2af56f8fa8e589be9c6b179ef65`**. It is an ancestor of master; `git diff feature/last-lightkeeper..master --stat` was empty. Selection therefore follows discovered history, not an assumption that the feature was merged.
- Created **`audit/runtime-ui-production`** from the merge baseline. No production source, example, vendor, dependency pin, or build-system change belongs to this audit commit.
- Other discovered branches: `feature/editor-authoring` (`86c36c1`), `feature/editor-usability` (`463cbdd`), `feature/example-games` (local `45cac43`, remote `f08c93e`), `feature/nutella-runtime` (`dbd840e`), `feature/sprite-sheet-authoring` (`5de70c9`), `fix/authoring-reliability` (`11259a2`), and `migration/upstream-1feb705` (`bc1f8b1`). None was substituted for the current runtime baseline.
- No applicable `AGENTS.md` was found in the workspace or its ancestor directories. Recursive gitlinks were recorded; none was changed by the audit.

### Inspected and exercised

Source review covered `RuntimeUI`, `RuntimeUIModel`, `RuntimeUIConfig`; components, hierarchy, scene copy/subtree duplication, prefab and serializer/authored-state code; Renderer2D, MSDF font generation and ProjectAssets; native input/window callbacks, RuntimeSession, C#/Mono bindings and reload; Hazelnut presets, Inspector, viewport edits/preview, prefab/document workflows; Nutella; every applicable example's authored scenes/prefabs and presentation scripts; asset/export/license closure; test implementations and CI configuration/history. Source locations throughout this report are relative to the repository root and refer to the exact baseline above.

Local host: Grandpa, CachyOS Linux/X11, i3-3227U, approximately 8 GB RAM, GCC 16.2.1. `glxinfo -B` reported accelerated Intel HD4000, Mesa 26.2.4, OpenGL core 4.2. Tests and scratch data use `build/testing/runtime-ui-audit` and disposable directories, with private `HAZEL_DATA`, resources and caches. There was a separate Debug build in the shared workspace; the audit stopped its own redundant Release engine build and did not stop that build. No performance acceptance is inferred from measurements under this load.

Local probe results and their exact build boundary are recorded in section 7. Existing UI model and integration tests were inspected for their assertions, not credited with broader coverage from their names. The native authoring walk-through in section 9 follows actual editor commands and script APIs; it is a source-traced workflow assessment, not a claim that a human completed fresh menu/HUD/journal editing during this audit. Scrolling-journal authoring stops at an absent control.

GitHub's API reported [CI run 38382294908](https://github.com/cringlekaden/Hazel/actions/runs/38382294908) successful for `753c77a`, with both [Windows](https://github.com/cringlekaden/Hazel/actions/runs/38382294908/job/115245437339) and [Ubuntu](https://github.com/cringlekaden/Hazel/actions/runs/38382294908/job/115245437760) jobs successful. This is the same source tree as the audited merge, **not a CI run on the merge SHA**. The workflow builds Debug/Release, runs the regression executables, exercises examples, and tests extracted application/game packages in software/GL4.1 profiles. The audit checked job status and workflow source, not downloaded every artifact or repeated every job. Physical Windows hardware, DPI/monitor transitions, human usability, and macOS remain unverified here.

### Documentation versus implementation

Read [runtime-game-ui.md](../runtime-game-ui.md), [runtime-ui-integration.md](runtime-ui-integration.md), [runtime-ui-model-checkpoint.md](runtime-ui-model-checkpoint.md), and relevant authoring/runtime/migration/example documentation. The checkpoint is historical at `0213e48`; its integration TODOs are not current absences. History subsequently adds scene/render/assets (`8aa699c`), editor authoring (`816bbd5`), managed registration (`789cbf0`), example migration (`1fcc2f4`), viewport capture fixes (`4bec36d`), final presentation/assets (`dd12455`), and independent managed Canvas/Rect state (`753c77a`).

The current guide correctly discloses no scroll lists, editable text, markup, or automatic content fitting. Its “disabled/hidden cannot deliver stale work” and viewport retirement claims need qualification for the outer scene queue and structural changes described below. “Arrows traverse” means sequential tab-order traversal, not spatial navigation. Canvas fitting scales uniformly while **extending logical space** at different aspects; it does not letterbox the canvas to its reference rectangle. Hazelnut's aspect preview separately letterboxes its displayed runtime image. Test claims about UTF-8 font paths and Latin-1 do not establish international typography.

### Actual migrated example inventory

Read-only YAML inspection counted scene-authored components below; prefab contents are excluded from these totals. All scene UI font paths are empty, selecting the shipped default. These are small trees: successful example migration does not establish large-interface performance.

| Project | Scenes | Canvas / Rect / Flow / Buttons | Largest UI rect count in one scene | Real presentation/authoring path |
| --- | --- | --- | --- | --- |
| LastLightkeeper | 12 | 18 / 156 / 6 / 42 | 14 | TitleUI prefab; C# Menu and Controls consume start/new/credits/advance/pause/journal/menu; chapter controllers set Text and Canvas visibility; journal uses dialogue pages; native nine-slice and licensed sprites |
| MeadowRun | 5 | 5 / 27 / 0 / 3 | 9 | Title/completion UI plus corner HUD/objectives; Presentation.SceneButton caches UIButton and loads scene; Explorer owns values/story; FitCamera sizes gameplay view rather than HUD |
| Skybound | 2 | 3 / 21 / 0 / 3 | 11 | TitleUI prefab, score/control ribbon and result modal; SceneButton/Game consume play/restart/menu and update score/visibility; FitCamera still moves gameplay ground, not screen HUD |
| SceneTransitions | 2 | 2 / 8 / 0 / 2 | 5 | TitleUI prefab, Play/Menu scripts consume UIButton; explicit Kenney frame/nine-slice; scripts still use per-frame GetComponent |
| Sandbox | 1 | 0 / 0 / 0 / 0 | 0 | Intentional world text/physics/prefab demo; no applicable runtime menu to migrate |

The four applicable examples each include an editable `Assets/Prefabs/TitleUI.hprefab`. Scenes contain authored snapshots; a shared prefab edit does not automatically update existing scene copies. Lightkeeper's dialogue/pause/journal state and shortcut edge history remain game scripts; none establishes a reusable engine dialogue/menu controller. No examples exercise custom-font fallback, text entry, scrolling, settings sliders, drag/drop, gamepad or world UI. Keep gameplay camera fitting and story/save code when migrating presentation further.

### Source evidence index

| Area | Primary files/functions |
| --- | --- |
| Runtime layout/draw/queue | [RuntimeUI.cpp](../../Hazel/src/Hazel/UI/RuntimeUI.cpp): Resolve, Visit, UpdateInput, Live, Consume, Draw |
| Geometry/router limits | [RuntimeUIModel.cpp](../../Hazel/src/Hazel/UI/RuntimeUIModel.cpp): FitCanvas, ResolveRect, ArrangeFlow, InputRouter::Update/Retire |
| Authored validation | [RuntimeUIConfig.cpp](../../Hazel/src/Hazel/UI/RuntimeUIConfig.cpp), [SceneUI.cpp](../../Hazel/src/Hazel/Scene/SceneUI.cpp): readers, ValidateUI, RefreshUI, RenderUI |
| Persistence and cloning | [SceneSerializer.cpp](../../Hazel/src/Hazel/Scene/SceneSerializer.cpp), [AuthoredState.cpp](../../Hazel/src/Hazel/Scene/AuthoredState.cpp), [Scene.cpp](../../Hazel/src/Hazel/Scene/Scene.cpp): CaptureAuthoredState, Copy, CloneSubtree |
| Hierarchy/prefabs/retirement | [Hierarchy.cpp](../../Hazel/src/Hazel/Scene/Hierarchy.cpp), [Prefab.cpp](../../Hazel/src/Hazel/Scene/Prefab.cpp), [Entity.h](../../Hazel/src/Hazel/Scene/Entity.h): ApplyReparent, Validate, OnComponentRemoving |
| Text/font/assets | [Renderer2D.cpp](../../Hazel/src/Hazel/Renderer/Renderer2D.cpp): MeasureText, LayoutText, DrawString, Flush; [Font.cpp](../../Hazel/src/Hazel/Renderer/Font.cpp); [ProjectAssets.cpp](../../Hazel/src/Hazel/Assets/ProjectAssets.cpp): FontAsset, Resolve, Refresh, AuditSpriteAssets |
| Session/managed contracts | [RuntimeSession.cpp](../../Hazel/src/Hazel/Scene/RuntimeSession.cpp), [ScriptGlue.cpp](../../Hazel/src/Hazel/Scripting/ScriptGlue.cpp), [ScriptEngine.cpp](../../Hazel/src/Hazel/Scripting/ScriptEngine.cpp), [GameUI.cs](../../Hazel-ScriptCore/Source/Hazel/Scene/GameUI.cs), [Entity.cs](../../Hazel-ScriptCore/Source/Hazel/Scene/Entity.cs) |
| Editor/UI documents | [SceneHierarchyPanel.cpp](../../Hazelnut/src/Panels/SceneHierarchyPanel.cpp): AddUI, DrawComponents; [EditorLayer.cpp](../../Hazelnut/src/EditorLayer.cpp): viewport handling; [PrefabPreview.cpp](../../Hazelnut/src/Authoring/PrefabPreview.cpp); [EditorDocuments.cpp](../../Hazelnut/src/Authoring/EditorDocuments.cpp), [DocumentController.cpp](../../Hazelnut/src/Authoring/DocumentController.cpp) |
| Runtime/platform | [NutellaApp.cpp](../../Nutella/src/NutellaApp.cpp), [LinuxWindow.cpp](../../Hazel/src/Platform/Linux/LinuxWindow.cpp), [WindowsWindow.cpp](../../Hazel/src/Platform/Windows/WindowsWindow.cpp), [Input.h](../../Hazel/src/Hazel/Core/Input.h) |
| Export and evidence | [packaging.py](../../scripts/internal/packaging.py), [tests.py](../../scripts/internal/tests.py), [CI workflow](../../.github/workflows/c-cpp.yml); focused test sources named in section 7 |

## 2. Strengths and present support matrix

Keep `Scene`'s `Scope<RuntimeUI>`, plain authored components, transient resolved/input state, `Ref` asset sharing, stable UUIDs plus scene identity, and existing project-relative asset ownership. UI reuses sprites/text rather than duplicating those asset systems. `GameUI` layout/input has no graphics or ImGui calls. Screen overlays can render without a world camera. Draw/hit order comes from canvas order and child hierarchy; clipping and raster restoration are deliberate. C# wrappers check entity lifetime, and actions contain identities/data rather than retained managed delegates. Serializer staging, authored-state comparisons, and prefab remapping are valuable existing infrastructure.

Legend: **I** implemented/source verified; **T** focused existing assertions or reported CI evidence; **L** limited/incomplete; **A** absent; **P** proposed. T is not a blanket verification label. All runtime/editor columns below describe the audited implementation, not the target design.

| Capability | Runtime/native | C# | Hazelnut | Evidence/limit |
| --- | --- | --- | --- | --- |
| Screen canvas, reference fit, safe insets | I/T | L: visibility/enabled/modal | I/T: preset/Inspector/aspect preview | `FitCanvas`; extends aspect space |
| Anchors, offset, size delta, pivot, min, margin/padding | I/T | A: layout setters | I: numeric fields/presets/basic drag | No max/content/aspect sizing |
| Horizontal/vertical flow, positive flex, alignment | I/T | L: marker wrapper | I: group preset/Inspector | No wrap/grid/shrink/measure |
| Rectangular clipping | I/T | A: clip API | I: Clip children | No scrolling; own drawable always bounds-clipped |
| Text wrap/alignment, explicit lines | I/T | L: text/color/spacing | I: text/style/font fields | Code-point/ASCII-space wrapping |
| Font selection and ownership | I/T | A: font/style setters | I: project font drop/path | Latin-1 atlas; unbounded font-count cache |
| Sprite/nine-slice/button state images | I/T | L: normal sprite APIs | I: pickers/borders/five states | No shared theme or state preview |
| Button release/capture, basic tab focus | I/T/L | I: consume/interactable | I: preset/tab order | Clipped-focus and queue gaps |
| Gameplay input gating | I/T/L | I: Input versus GameUI | I/T: Play viewport guards | Native raw Input bypasses gating |
| Modal canvas filtering | I/T/L | I: Modal bool | I: Block gameplay | Highest eligible canvas; no stack/return-focus |
| Toggle/radio/slider/progress | A | A | A | Games would compose state manually |
| Scroll/list/grid/dropdown/tab/text field | A | A | A | Required for general menus/inventory/journal |
| Tooltip/popup/context menu/confirmation composition | A | A | A | Game-specific Canvas/Text/Button only |
| Action/event connections and dynamic binding | L: bounded string queue | L: polling/entity refs | L: script fields/action text | No typed events/designer connection picker |
| Copy/duplicate/serialization/prefab | I/T | I: instantiate/entity refs | I/T: subtree/document workflows | Detached copies; no shared-instance updates |
| Undo/redo, clipboard subtree, multi-selection | A for scene authoring | — | A | Recovery originals are not edit undo |
| Styles/tokens/animation/audio cues | A as UI services | L: generic audio/sprite APIs | A: theme/timeline/state preview | Five button tint arrays are local styles |
| Gamepad/remapping/touch/drag-drop | A as UI services | A | A | GLFW mouse/keys hard-coded in session |
| Grapheme/shaping/bidi/fallback/IME | A | A | A for runtime authoring | UTF-8 decode alone is insufficient |
| UI semantics/accessibility bridge | A | A | A | Focus outline is a useful starting point |
| World-space/projected UI/render-to-texture routing | A | A | A | World sprites/text are not interactive UI |
| UI debugger/overflow/layout costs | A | A | L: Validate UI/error text | No resolved tree/focus/capture inspection |

| Platform/path | Established evidence | Production status |
| --- | --- | --- |
| CachyOS GCC/Premake/GNU Make, Intel HD4000 GL4.2 | Existing local records plus focused audit checks | Initial screen UI; general production gate open |
| Linux GL4.1 | Historical local suite and same-tree CI profile | Portable path exists; no modern GL dependency should be introduced |
| Windows x64 | Same-tree Debug/Release and packaged software/GL4.1 CI | Physical GPU, IME, DPI and monitor cases still manual |
| Hazelnut detached viewports/mixed DPI | Screen-origin mapping exists | No dedicated UI acceptance found; main-window native polling needs verification |
| Apple Silicon/macOS GL4.1 | No build/input/UI evidence | P: platform work; do not advertise current support |
| Metal/Vulkan or 3D rendering | No UI implementation/evidence | P: adapter contracts; separate renderer prerequisites |

## 3. Findings: layout, rendering, typography and resources

### Layout and sizing

`Hazel/src/Hazel/UI/RuntimeUIModel.cpp` (`FitCanvas`, `ResolveRect`, `ArrangeFlow`) validates finite dimensions, normalized ordered anchors/pivots and nonnegative margins/padding/minimums. Rect position uses `parent.Position + AnchorMin*parent.Size + Offset - Pivot*SizeDelta`; size uses anchor span plus `SizeDelta`, then margins and minimums. **Pivot therefore acts on size delta, not the final stretched/minimum-constrained size.** Preserve that legacy meaning in migration rather than silently replacing it with another engine's formula.

`RuntimeUI::Resolve/Visit` traverses visible canvases, propagates visible/enabled state, insets child content by padding, and clips children to the parent's bounds when requested. Hidden nodes skip layout space; disabled nodes still draw and occupy flow space while their buttons become noninteractable. Flow assigns child positions and sizes from authored preferred `SizeDelta`, minimum, margin and flex. It never measures text/images/children; it distributes positive free space, does not shrink overflow, and validates hidden flow items too. A negative stretch size delta valid in free layout becomes an invalid preferred size when moved under a flow.

There is no measure/arrange dependency solver, content size, max constraint, wrapping flow, grid, aspect preservation, scroll offset or intrinsic image sizing. Consequently there is no current content-size cycle to solve, but introducing content fit without a dependency contract would create one. Defaults for the target are specified in section 8.

Nested canvases are independently fitted to the entire viewport and sorted globally; ancestors influence visibility/enabled, but parent geometry and clipping do not constrain the nested canvas. A non-rect intermediate entity stops a rect subtree's traversal. A root orphan rect is previewed in Edit with a synthetic default canvas; runtime requires a canvas path. Reparenting currently follows the general hierarchy/world-transform policy (`Hierarchy.cpp::ApplyReparent`), not a logical-UI-position policy. These are explicit behaviors to preserve or migrate, not undocumented promises of nesting/keep-position.

Zero viewport dimensions suspend resolve/draw; zero rectangles cannot hit, and excess padding clamps content size to zero. `LayoutText` treats width <= 0 as no wrap, then the drawable is clipped. There is no early validation that all reachable runtime UI fits the node/depth limit. Canvas/rect scaling is fractional; nine-slice retains source-border proportions but does not select integer scaling or snap geometry. Scissor uses inward ceil/floor framebuffer edges while hit testing uses unsnapped floats. Tiny fractional regions can hit while producing no framebuffer coverage. A pixel-art contract must address geometry, sampling, clipping and input together.

### Rendering and performance candidates

`SceneUI.cpp::UpdateUIInput` calls `RefreshUI/Resolve`; `SceneUI.cpp::RenderUI` resolves again. Scripts can mutate visibility/text between these phases, so removing the second resolve without a mutation boundary would produce stale display. The target should reuse versions, not assume that input and render always see identical authored state.

`RuntimeUI::UpdateInput` linearly searches `m_Nodes` for every button to convert bounds/clips to pixels. `Draw` linearly searches `m_Buttons` for button state, again for focus. These introduce quadratic work for larger button trees. Resolve sorts canvases, builds nodes/buttons, validates numbers and walks ancestors each time. Vectors retain some capacity, but each `Button` copies its action string. Nine-slices emit up to nine quads. Whole-texture state overrides call `ProjectAssets::Resolve`, which allocates a `ResolvedSprite` even when the underlying texture is cached.

`RuntimeUI::Draw` sets raster state and performs **two BeginScene/EndScene pairs per drawable node**: image pass followed by text/focus pass. `Renderer2D::EndScene/Flush` submits nonempty batches; empty passes do not imply draw calls. This intentionally preserves painter order across Renderer2D's separate primitive batches, but prevents batching adjacent compatible UI nodes. Proposed optimization must preserve order, premultiplied/straight-alpha policy, clip and entity picking; globally sorting by texture would be incorrect.

Text line cache keys include string, font path/asset epoch, width divided by font size, kerning and wrap. Line spacing/alignment change placement rather than line breaks. Cached lines are strings/widths, **not shaped glyph runs or vertex geometry**. `DrawString` decodes each cached line into a newly allocated code-point vector every draw. `m_Cache` retains entries for hidden/changed components until retirement/reset; `ProjectAssets::m_Fonts` strongly owns every requested font until refresh/project teardown, with no byte budget or LRU. `CreateAndCacheAtlas` is a misleading name: it generates/uploads synchronously, not a persistent atlas cache. Two generation threads and one fixed Latin-1 atlas keep each current font modest; loading many distinct fonts remains unbounded.

Source-only alpha finding: `Hazel/Resources/shaders/Renderer2D_Text.glsl` multiplies both RGB and alpha by glyph coverage via `mix(0, Input.Color, opacity)`, while `OpenGLRendererAPI::Init` uses `GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA`. Over black, coverage `c` and input alpha `a` therefore contribute `RGB*c*c*a`, rather than the straight-alpha `RGB*c*a`. Text edges are darkened by coverage twice. Typography qualification must establish one alpha convention and pixel-reference tests for text, images and fades on light/dark/translucent backgrounds; this audit did not make a framebuffer color measurement for this finding.

### Text and font contract

`Renderer2D.cpp::MeasureText/LayoutText/DrawString` share the same MSDF geometry and font-height normalization. Measurement supports newlines/tabs and pair advances; rendering handles the same basic model. Wrap recognizes spaces/tabs and falls back to code-point breaks for long words. Alignment is left/center/right and top/center/bottom; every drawable is clipped to its rect. No ellipsis, overflow choice, rich spans, selection, editable text, fallback chain, language/direction metadata, shaping, bidi, or grapheme mapping exists. An icon can be a separate sprite child; it is not an inline text run.

`Font.cpp::Font` requests **U+0020–U+00FF only**, even if a selected TTF contains much more. Missing glyphs substitute `?`. Curly quotes, em dash, ellipsis, combining accents, Cyrillic, Greek, Arabic, CJK and emoji are outside that requested atlas. Font validation verifies a readable FreeType face with space and fallback glyph, not required localization coverage. Font weight/italic can be authored as different files, but there is no family/style asset or inline style model. Kerning is an extra authored advance in font-height units, not a shaping substitute.

`LayoutText` enforces 64 KiB and compares its byte-boundary count to the pinned `msdf_atlas::utf8Decode` output. That decoder tolerates invalid scalar encodings and suppresses a leading BOM; it is not a strict UTF-8 validator. See confirmed defects. Managed string conversion uses a NUL-terminated UTF-8 buffer, so embedded NUL requires an explicit future reject/replace contract rather than silent truncation. TextComponent setters validate finite spacing but do not enforce the UI string limit at assignment; invalid/oversized text can fail on rendering later.

A text input system additionally needs committed text separate from physical keys, grapheme-safe caret/selection/deletion, undo transactions, clipboard, scrolling-to-caret, composition/preedit and candidate-position reporting. Linux/Windows windows already forward GLFW character callbacks, but RuntimeSession only collects pointer and Enter/Space edges. No runtime clipboard/composition service exists. **Single-line and multiline fields are production scope; password fields are optional** until a game actually has an authentication/secret-entry requirement. A masked glyph alone must never be marketed as secure storage.

### Validation, authored state and packaging

`RuntimeUIConfig.cpp` has strict component key sets, duplicate/unknown rejection, finite vectors, enum checks, positive reference/font dimensions, font size <= 512, and action <= 128 bytes/no NUL. `Scene::ValidateUI` checks dependent components and fonts/state images/nine-slices; `DocumentSchema::Scene` calls config validation. `SceneSerializer` stages parsing/hierarchy/resource checks before replacing the active scene and resets transient UI on commit. `AuthoredState.cpp::CaptureAuthoredState` includes every UI field, each state source/color and Text fields, excluding caches/capture/actions. `AllComponents` includes UI for copy/duplicate; `CloneSubtree` remaps stored Entity script fields, and prefab validation rejects references outside its saved subtree. These support reliable reuse of **detached instances**, not linked prefab inheritance/shared updates.

Validation is uneven across phases. `ValidateUI(false)` still reads/validates font files, but state image resolution and nine-slice source size checks require resource validation. `UILayoutComponent` without a rect and orphan runtime rect paths are not fully diagnosed. The 4096-node limit is thrown during visible traversal; hidden over-limit content may pass until shown. Runtime draw/input exceptions propagate to the application entry point and can terminate Nutella; Edit draw catches one error and exposes `GetUIError`, preserving the document. Bad scene loading retains the previous draft, but there is no integrated repair view for all malformed UI fields. Resource failure needs a deliberate per-subtree fallback policy, not an accidental “all UI disappears or application closes”.

Export uses `scripts/internal/packaging.py::validate_project/validate_sprites/project_files/copy_project`, native `AuditSpriteAssets` and assembly validation. Closure includes UI font paths, whole-texture state images, sprite-sheet states/regions, nine-slice sources, prefabs and audio. Project ownership rejects escape paths/external symlinks for export. Empty font means shipped Open Sans; its Apache-2.0 notice is copied separately. Engine/vendor/native notices and project top-level notices are copied; assets are broadly included except compiler intermediates. Lightkeeper contains six Kenney license directories; SceneTransitions includes Pixel Adventure CC0 provenance and its frame license. SceneTransitions' top-level license description still says no external artwork; that claim conflicts with its current included frame/provenance and should be corrected during migration. No new package or license mutation was performed here.

Closure is not font coverage validation or license provenance enforcement for arbitrary custom fonts. Add font-family/theme/template/localization references to the same native closure graph when introduced, and a reviewed resource manifest linking redistributed files to notices. Missing notices/unknown provenance should block release export with a fixable diagnostic; routine editing should remain possible. Preserve notices with relocated packages. UI asset editing stays native in Hazelnut even though existing build/export orchestration uses Python.

## 4. Findings: interaction, scripting and lifecycle

`RuntimeUIModel.cpp::InputRouter::Update` samples one frame, suppresses held input on scene/enable entry, filters duplicate button identities, captures topmost usable buttons on press, and activates on release inside the current clipped target. Half-open bounds prevent adjacent-edge overlap. Optional host press/release coordinates retain one quick click between updates. Keyboard accept activates on **press**; held Accept does not repeat. Pointer gestures generally take priority, with the cancelled-release exception in section 7.

Input order equals painter order for **buttons only**. Panels/images/text do not participate as blockers. A visual cover, disabled button, or decorative popup can therefore allow an underlying usable button to receive a click. This is a missing hit-test/occlusion policy, not evidence that all visible UI is interactive. Visibility and alpha are separate: transparent button tints do not remove rectangular hits. `BlocksPointer` considers intersections with any button, including disabled ones, so gameplay suppression and usable UI targeting have different policies.

`InputRouter::State` collapses hover, press and focus into one of five presentation states. Hover takes precedence over Focused; `Draw` only outlines the Focused state, so a hovered keyboard-focused button loses that outline. Production state composition should keep focus/checked/invalid indicators orthogonal to hover/press, with a persistent visible focus cue.

Tab/Shift-Tab enters nonmodal keyboard ownership. Down/Right mean “next”; Up/Left mean “previous” in sorted TabOrder. There is no geometric neighbor scoring, explicit neighbors, focus scopes, initial/return-focus API in C#, or navigation repeat timer. Native model `SetFocus` exists, but Scene/GameUI do not expose it. Escape releases nonmodal keyboard ownership; scripts must decide what closes a modal. Mouse focus does not automatically establish persistent nonmodal keyboard ownership. Modal behavior picks the last visible enabled modal canvas by sort/UUID order and makes only buttons in that canvas usable. It is not a popup stack, does not dim other canvases, and does not restore the previously focused control. Nested child canvases belong to a different modal canvas identity.

`RuntimeSession.cpp::Update` hard-codes Tab, arrows, Shift, Escape, Enter, Space and left mouse; neither native nor managed Input supplies UI gamepad actions. GLFW callbacks preserve non-repeat Accept and pointer edges, but multiple presses/releases coalesce into one pair and one accept bit per frame. This is an explicit current throughput limit, not a general ordered input-event stream. There is no touch identity, multiple pointers, scroll consumption, drag/drop or device-change/remapping layer. Future touch should enter the same gesture router with pointer IDs rather than branch every control by device.

Nutella uses window-coordinate mouse/layout dimensions, framebuffer dimensions for GL viewport and a draw clip ratio. This is a coherent separation for screen UI; it is not evidence of mixed-monitor DPI acceptance. Hazelnut subtracts the displayed image's global origin via `GetMouseScreenPosition`, gates by main-window focus and ImGui widget state, and maps its preview letterbox to the runtime viewport. Detached editor windows still use main-window GLFW polling; acceptance must check pointer/key ownership there. There are no content-scale events or a declared logical/window/framebuffer unit type to prevent accidental mixing.

`ScriptGlue.cpp::Input_IsKeyDown/IsMouseButtonDown` honors session UI gating. **C++ `Input::IsKeyPressed/IsMouseButtonPressed` remains raw GLFW polling.** Native game scripts can bypass modal ownership accidentally; expose raw device input only as a clearly named platform-level API and provide session action queries for gameplay. UI-specific `GameUI.IsKeyDown` intentionally bypasses UI gameplay blocking, with viewport gating only. Lightkeeper's own edge-history/shortcut code is necessary today and can bypass a modal if used carelessly. A reusable screen controller/action context should replace this convention.

Scene destruction marks entities invalid, retires UI and clears resources; scene Stop and script-domain reload reset UI. Scene identity changes and C# `Entity.CheckedID` protect against old-scene objects; actions hold no delegates. Component removal retires the component entity but not automatically its UI descendants. Hiding/disabling through managed UI setters retires the subtree and eagerly resolves. Native direct component writes do not use that gateway. Actions in the outer RuntimeUI deque are live-checked at consume; that check tests component/state/ancestor canvas, not membership in a current visible resolved tree. See the inactive-frame and removed-layout-parent defects.

Queues are bounded to 64 with silent drop of new work; only one activation is emitted per router update. Global consumption is destructive and single-owner by convention. No editor validation selects the global consumer, no subscription/multiple-consumer contract exists, and hierarchy callback order can determine who receives an action. Preserve at-most-once activation, but make intended target/owner and overflow diagnostics explicit.

### API/data flow

Native authored types are simple and discoverable within components, but inheritance from layout structs and mutable component references permit edits without invalidation/transaction validation. C# `GameUI.cs` exposes only consume, raw UI key polling, capture observation, Canvas visible/enabled/modal, Rect visible/enabled and Button interactable. Layout/text-style/nine-slice wrappers are registration markers without properties. Text content/color/kerning/spacing and sprite APIs are separate generic components. There is no ordinary API for focus, scrolling, slider value, theme, initial state, control change notifications, or localization binding.

Per-button entity references are safer than name lookups/string switches, but repeated `GetComponent` allocates managed wrappers. Examples cache some wrappers while SceneTransitions polls `GetComponent` each update. Lightkeeper still centralizes string actions and finds named presentation entities; these are real migration constraints, not a reason to add a second UI tree outside Scene. Script field UI supports Entity references; it cannot currently author typed UI property bindings or validated event endpoints. Recommended APIs and ownership are in section 8.

## 5. Controls, styles and accessibility scope

| Control/service | Production core | Composition/acceptance intent |
| --- | --- | --- |
| Button, toggle/checkbox, radio group | Yes | Shared focus/gesture/disabled behavior; checked state independent from focus; radio group exclusivity |
| Slider and progress/busy indicator | Yes | Value clamp/step/keyboard/controller adjustment; progress read-only; determinate plus reduced-motion busy feedback |
| Scroll view/scrollbar | Yes | Content extent, inherited clip, wheel/page/drag/controller scroll, ensure-focused-visible, nested consumption |
| List and grid, selection | Yes | Stable item keys; reusable item template; fixed-size virtualization first; inventory grid and journal rows |
| Dropdown and tabs | Yes | Popup host/reuse of list and selection; focus/close/return semantics; tabs select content without discarding state |
| Single/multiline text field | Yes | Clipboard/selection/caret/undo/composition/limits; read-only selectable text reuses same text geometry |
| Tooltip, popup/context menu | Yes | Shared placement/viewport clamp, delay, focus/hover rules, dismissal policy; controller-accessible equivalent |
| Confirmation dialog and modal screen | Yes | Screen/modal stack, declared initial/default/cancel button, return focus; game owns destructive decision |
| Dialogue, journal, HUD, interaction prompt | Templates/services | Text/localization + panels/lists/buttons; game owns story and values, UI owns layout/input/menu mechanics |
| Inventory drag/drop | Yes, bounded | Item-key payload and accepted target; keyboard/controller move alternative; cancel/retire semantics |
| Tree/table, date picker, docking, full rich editor | Optional | Not prerequisites for ordinary game menus; add only with a concrete use case |
| Password, color emoji, arbitrary shader masks | Optional | Separate security/font/render contracts; do not quietly include in basic field/image support |

Controls should compose existing rect/layout/text/image entities with one behavior owner, not implement independent focus/layout/render systems. A button label may be a child; a dropdown uses a button, popup and list; a dialog uses a modal host and ordinary controls. Internal decoration is nonfocusable by default. Templates may hide implementation children in the hierarchy while allowing an explicit expand/edit-parts command. Stable named parts are validated asset metadata, not unchecked game strings.

Five local button colors/images exist; theme/font/color/spacing assets, inheritance, state preview, shared updates and local-override reset do not. Sprite animation and AudioSource do not constitute UI animation/audio feedback. Add tokens and small state transitions before a timeline or graph editor: opacity, tint and visual translation/scale, with explicit interruption/cancel and optional layout effects. Accessibility preferences must apply to the screen service and theme, not require every game to recolor/fade panels.

Minimum production accessibility: user UI/text scale, keyboard/controller alternatives, readable persistent focus, adequate theme contrast, non-color selected/error/disabled cues, explicit names/roles/value/state/actions, tooltip equivalence and reduced motion. Screen-reader bridges are staged platform work: Linux AT-SPI and Windows UI Automation need focus/value announcements and safe action dispatch; macOS NSAccessibility follows the platform target. A semantic tree/test adapter belongs in the core milestone. **OS screen-reader support is not claimed until bridge-specific manual acceptance passes.** A release needing screen-reader support must take the bridge stage as a prerequisite rather than interpret semantics alone as accessibility completion.

## 6. Confirmed defects versus missing behavior and decisions

Confirmed source defects are distinct from absent product features. Severity reflects ordinary menus, not speculative abuse. The local probe table in section 7 marks what was reproduced; source-only items below explicitly say so.

| ID | Defect and source-backed trigger | Consequence | Repair/acceptance |
| --- | --- | --- | --- |
| D1 | `RuntimeUI::UpdateInput`: InputRouter clears on disabled frame, outer `m_Actions` only erases `!Live` | An already queued action for a still-live button survives viewport input deactivation | Clear/cancel both queues on ownership loss; activate, disable input before consume, assert no delivery after restore |
| D2 | `InputRouter`'s `Usable` checks independent nonempty bounds/clip, not their nonempty intersection | Completely clipped button can gain focus and activate by keyboard | Base navigation usability on effective visible region; offscreen scroll children handled by an explicit ensure-visible policy |
| D3 | `LayoutText` uses permissive decoder plus count check | Overlong/surrogate encodings can pass; valid leading UTF-8 BOM can be rejected | Strict scalar decoding, documented BOM/NUL policy; malformed corpus plus valid Unicode tests |
| D4 | `OnComponentRemoving` retires only that entity; `RuntimeUI::Live` does not require a drawable rect chain/current membership | Removing a rect from a layout ancestor can leave a descendant's queued action deliverable after it leaves the runtime tree | Structural invalidation and subtree retirement; remove/reparent ancestors between dispatch/consume |
| D5 | In `InputRouter::Update`, release outside clears `m_Capture` before keyboard fallback on the same frame | A cancelled pointer gesture plus fresh Accept can activate its focused control | Gesture ownership lasts through release/cancel processing; regression for simultaneous press/release/accept combinations |
| D6 | Source-only: viewport resize handle writes `max(MinimumSize, SizeDelta + delta)` | A negative stretched SizeDelta can jump to nonnegative on first resize; pivot/minimum math can move the opposite edge; moving flow children changes ignored Offset | Layout-aware edit commands operate on resolved edges, invert legacy anchor math, expose flow-owned fields as computed; compare before/after for fixed/stretch/pivot/flow cases |
| D7 | Source/document discrepancy: SceneTransitions license says no external artwork while a Kenney frame/provenance exists | Misleading asset provenance description in shipped project notice | Update that example's notice in the implementation/migration stage; verify copied package notices |
| D8 | Source-only: MSDF text output scales RGB and alpha by coverage, with straight-alpha blending | Coverage is applied twice to text RGB at antialiased edges | Align shader/blend convention in P3; pixel-reference tests on contrasting backgrounds and partial opacity; include existing world text regressions |

Missing production features include the controls and services in section 5, true spatial/controller navigation, content sizing, shared styles, undo/redo, semantic/platform accessibility, text composition and world-space UI. These are not defects in a guide that already excludes them.

Design questions/defaults: retain legacy anchor semantics behind versioned migration; choose explicit hit behavior (decorative/pass-through versus blocking); choose content-size rules and cycle rejection; decide text-language coverage and package manifests; distinguish semantic accessibility from OS support; gate actual world-space release on the real 3D renderer. Recommended defaults below resolve routine choices. Licensing/dependency additions and a narrower localization/accessibility shipping requirement are the material scope decisions.

## 7. Verification record and performance plan

The audit does not turn prior logs into new acceptance. Same-tree CI is the strongest complete build/platform evidence. Existing local binaries predate the final feature tip, so an unqualified “all current suites passed locally” would be incorrect.

The local model smoke main was rebuilt using its existing generated Premake/GNU Make target and passed, linked to the cached engine archive. An isolated probe separately compiles the baseline `RuntimeUI.cpp`, `RuntimeUIModel.cpp` and `Renderer2D.cpp`, links existing engine/vendor archives, and uses private resources/data. That is focused current-source evidence for those modules, not a clean engine rebuild. Optimization is disabled for those probe objects; concurrent compilation and overlapping stress nodes rule out release frame-time conclusions. No production scene/save/layout/settings is used for probe edits.

| Audit check | Actual result | Evidence boundary |
| --- | --- | --- |
| Existing model smoke linked with freshly compiled baseline RuntimeUIModel | PASS | Native CPU assertions; independent of editor/GL |
| Existing integration smoke linked with freshly compiled baseline RuntimeUI/Model/Renderer2D | PASS, Intel with Mesa GL/GLSL override 4.1/410 | Camera-free framebuffer/picking/clipping, raster restoration, queue/modal, closure/copy/serialization/prefab/text assertions; other engine/vendor archives reused |
| D1: activate; publish disabled input before consume | `INACTIVE retained=1` | Current RuntimeUI outer queue; not a simulated OS-focus-loss test |
| D2: bounds at (100,100), size (50,30); clip at (0,0), size (50,30); Accept | `CLIPPED focus=1 action=1` | Fully disjoint effective region still focused/activated |
| D3: UTF-8 C0 AF / ED A0 80 / EF BB BF + A | Overlong and surrogate accepted; leading BOM rejected | Current LayoutText and pinned decoder; no font/shaping inference |
| D4: queue child activation, remove its parent Rect, consume child | `REMOVED parent rect childAction=1` | Scene component-removal hook plus current RuntimeUI Live/Consume |
| D5: captured button, release outside plus fresh Accept | `POINTER cancelled release + Accept activated=1` | Fresh model; remaining gesture combinations still require acceptance |
| Visual cover panel above button | `PANEL clickThrough=1` | Confirms missing blocker policy; no claim that panels promised blocking |
| Default font requested atlas | 191 loaded of 224 requested; é present, em dash/CJK absent | Exact current glyph-range limit, not a count of all glyphs in the font file |

Focused checks completed with private data/resources and scratch scenes. Compile commands reused generated Premake include/link settings; only the probe/test mains and three audited UI/render translation units were freshly compiled, using cached engine/platform/font/vendor archives. Fresh model and integration logs are `model-current-native.log` and `integration-focused.log` under ignored `build/testing/runtime-ui-audit`; probe evidence is `model-extra-native.log` and `probe-prepared-native.log`. The earlier cached-library model pass is corroboration, not the fresh-source result listed above.

The final diagnostic profile prepared sprite resources through `Scene::PrepareSprites(true)`, then drew solid rectangular buttons, with no text/state textures/nine-slices. Bounds repeat in the 512-node fixture, so it is a submission/routing stress sample rather than a typical large menu. After warmup, five Resolve+UpdateInput iterations yielded:

| Nodes/buttons | Mean resolve + input CPU | Measured image draw calls / quads |
| --- | --- | --- |
| 64 | 0.483 ms | 64 / 64 |
| 512 | 4.785 ms | 512 / 512 |

These nonoptimized diagnostic object timings are **not Release benchmarks, p95 results or GPU timings**. Draw counters confirm the per-element submission behavior; they do not establish a GPU bottleneck. The first combined attempt including 2048 nodes timed out at 180 seconds without usable buffered counters; its phase/cause was not established, and it is not acceptance evidence. A smaller run completed; its initial zero draw counters exposed missing sprite preparation in the probe, so preparation was added before the final draw-count run. No timeout or unprepared draw is counted as a successful renderer profile. Release/allocation/cache/GPU profiling of representative mixed UI remains an implementation acceptance task.

Reviewed test boundaries:

- `GameUIModelSmoke.cpp`: reference/aspect mapping, anchor/flow/clip edges, quick click and explicit release position, capture cancellation, basic tab navigation, disabled state, held-key suppression, scene reset/retire and malformed values. It lacks D1/D4 (scene integration), full-intersection focus and cancelled-release Accept coverage.
- `GameUIIntegrationSmoke.cpp`: camera-free draw/picking/clipping and raster restoration, font/state-image closure, once consumption, hidden control, modal gating, copy/roundtrip/prefab root remap, dirty state, zero viewport and measurement/wrap checks. It does not prove translated shaping, scaled framebuffer precision, all malformed resources or large-tree efficiency.
- `ManagedFixture.cs::UIProbe` checks all six UI component registrations and independent Canvas/Rect booleans plus fresh action/capture state. `MonoSmoke`, `SceneSmoke`, `RuntimeSessionSmoke` cover surrounding domain/scene lifecycle. Broader lifecycle assertions are valuable but do not exercise every UI queue path.
- `EditorSmoke.cpp::HierarchyAuthoringChecks`: actual native Canvas/Panel/Text/Button presets, preview bounds/no Edit actions, authored copy and prefab save/reopen, and ImGui viewport MoveId versus other-widget gating. Direct controller calls are not mouse-handle/undo/user-discoverability acceptance.
- `FontSmoke.cpp`: Latin-1 `A`/`é`, metrics/MSDF GPU contents, Unicode file paths, invalid/missing fonts and context lifetime. **No fallback/shaping/bidi coverage.**

For implementation profiling, use three recorded fixtures: menu/HUD (64 nodes, ~8 controls, several text labels), settings/journal/inventory (512 logical nodes with clipped content and 1000 data items), stress (2048 visible nodes plus mixed images/text, explicitly below 4096). Capture Release native Intel GL4.2 and GL4.1, window/framebuffer sizes, build SHA, scene seed, font cache warm/cold status and allocation counters. Separate resolve/measure/arrange, routing, text shaping, command generation, submission and GPU time. Report median/p95 over 300 frames after warmup with no competing build; CPU timings around draw submission are not GPU duration.

Recommended release budgets on Grandpa at 1280×720/60 Hz: p95 UI CPU <= 1 ms for small fixture, <= 3 ms for larger fixture; UI GPU <= 2 ms for the larger fixture, measured independently. Idle frames allocate no UI layout/routing memory and do not remeasure unchanged text. A stable screen must reuse its resolved generation. Stress is a bounded reliability/scaling test, not a 60-Hz guarantee. Failure to meet a target triggers measured optimization or an explicitly revised published budget. Do not claim these proposed budgets have passed.

## 8. Recommended architecture and supported contract

### Ownership and phased data flow

Keep one scene/entity hierarchy and scene-owned UI service. Components hold serialized authored defaults; ProjectAssets owns `Ref` fonts/images/themes/templates; RuntimeUI owns derived snapshots and transient input/control/event/animation state through `Scope`/plain values. Rendering consumes backend-independent commands. Hazelnut views/edits those exact authored types through ImGui; Nutella needs no ImGui UI renderer or editor document implementation.

```text
Authored components + asset revisions + runtime property overrides
        -> validate / dirty propagation
        -> measure and arrange -> resolved snapshot (generation + ID index)
platform input -> UI actions/pointers -> hit/focus/capture -> typed event queue
        -> owner script decisions -> bounded property transactions
        -> resolve changed dependencies -> ordered draw + semantic snapshots
        -> renderer adapter / accessibility adapter
```

Input targets one published generation. Script transactions that change visibility, enabledness, hierarchy or popup ownership immediately cancel ineligible capture/events and invalidate dependencies; changed geometry is resolved before the next relevant hit/render boundary. Coalesce mutations before resolving. Rendering can publish a newer generation after script changes without replaying input. Index buttons/nodes by stable ID; never rescale the same mutable button geometry twice. Const/read-only snapshots expose diagnostics. Main-thread mutation remains the rule; background font work publishes assets only at safe boundaries and cannot retain scene pointers.

| State category | Examples | Persistence/copy rule |
| --- | --- | --- |
| Authored | Reference size/mode, rect rules, default value, local style overrides, theme/template asset, typed connection/Entity refs | Serialize/version/dirty/undo/copy/remap; no live resource handles |
| Derived | Measured sizes, arranged bounds, effective clip, shaped lines, draw commands, navigation index, semantic tree | Rebuild/version/cache; do not serialize or clone live caches |
| Transient | Hover/focus/capture, popup stack, scroll position unless game opts to save, editing/composition, current value overrides, actions, animation clock | Scene/session owned; clear on retirement; explicit game save adapter if needed |

Do not persist Play values back into authoring implicitly. Clone authored state into Play and preserve editor selection/document dirty state across Stop. Unknown newer schema/unsupported values should retain originals and provide recovery diagnostics instead of lossy rewrite.

### Layout defaults and limits

Retain version-1 legacy anchored behavior and reference-fit mode unchanged for existing scenes. New UI uses explicit per-axis **Fixed / Fill / Content** size rules with finite min/max and logical-pixel spacing; free anchored layout, horizontal/vertical stack, wrapped flow, and fixed/auto grid are production containers. Images offer stretch/contain/cover/native aspect modes; clipping defaults to rect bounds, with explicit visible-overflow for labels/tooltips where intended. `Visible=false` collapses layout; a separate `Hidden` presentation mode reserves layout space; disabled retains layout and cannot interact. Avoid overloading alpha as input eligibility.

Measure bottom-up under declared constraints; arrange top-down. Width-constrained text measures height; measured content never depends on its own final arranged result. Percentage/Fill children under an unconstrained Content parent need an external definite size, or fail validation with the dependency path. Do not “solve” cycles through unlimited convergence. Reflow at most twice per committed boundary if scrollbar visibility changes constraints; additional oscillation fails with a diagnostic and deterministic fallback. Overflow is reported; minimums are not silently shrunk.

Nested canvases default to independent screen surfaces as today; true parent-constrained sublayouts use ordinary rect groups. Warn when authors nest independent canvases, and show surface boundaries. Rect traversal should allow explicitly designated nonvisual grouping entities or diagnose interrupted chains. UI reparent commands default to keeping resolved logical position where representable, with an explicit adopt-parent-layout option; generic world hierarchy behavior remains intact for world entities.

Scaling modes: default uniform reference fit with aspect extension; optional reference letterbox, constant logical size with DPI policy, and integer pixel-art scaling. Keep text scale separate from overall UI scale so containers can reflow. Pixel-art mode snaps final drawable/hit/clip boundaries together in framebuffer space and honors texture sampling; below reference size use an explicit fractional-fit fallback with a warning rather than a zero integer scale. High-resolution mode uses fractional logical layout and stable raster coverage rules. Declare unit conversion APIs between window units, framebuffer pixels, canvas units and surface-local units. Never silently change a legacy reference scale on a DPI event.

Published limits initially retain **4096 resolved rect nodes per scene, 4096 flow items, hierarchy depth 256, 64 queued activations, 128-byte legacy action strings, 64-KiB UTF-8 text per text element and font size <= 512**. The scene's overall limit remains 10000 entities. Count declared UI and possible template expansion at validation; virtualized data items do not each require scene entities. Define queue overflow as drop-newest with a rate-limited diagnostic/counter; do not replay dropped actions later. Large finite numbers that overflow derived math are rejected. Zero-size/minimized surfaces do no draw/hit work and cancel input ownership. These are supported maxima, not performance promises at those maxima.

### Text and asset pipeline

Introduce a project-owned font-family asset: regular/bold/italic face references, fallback chain, expected scripts/locales, coverage policy, atlas/render settings and notice references. Default remains shipped Open Sans for compatibility; additional fonts require declared licensed assets. Validate family cycles/duplicates and font coverage before packaging. Unsupported glyphs get a visible replacement plus one useful diagnostic, not silent assertions of Unicode support.

One CPU text service supplies strict UTF-8 decoding, bidi/script segmentation, grapheme/word/line boundaries, shaping and cluster maps, then line layout; measurement, drawing, caret, selection and accessibility consume that same result. Shape glyph **IDs**, including ligatures not directly mapped from code points. The pinned MSDF code already exposes `FontGeometry::loadGlyphset/getGlyph(GlyphIndex)`, `GlyphGeometry::load(GlyphIndex)` and `msdfgen::loadGlyph(GlyphIndex)`; an engine-owned glyph-ID adapter can use those capabilities without editing the vendor. Use locale/direction/font-family revision/width/size/spacing/spans in bounded cache keys. Preserve source byte/UTF-16 conversion maps for C# and cluster boundaries for editing. Break long runs only at allowed grapheme/line boundaries; ellipsis is itself a shaped run. Rich emphasis/icon spans are structured, validated runs, with an optional small documented markup convenience later; arbitrary HTML is unnecessary.

Stage guaranteed typography around a fixture corpus: English/French/German including longer strings and combining accents, Greek/Cyrillic, Arabic mixed with Latin/numbers, Hebrew mixed-direction text, and Japanese/CJK wrapping with declared fonts. This is a tested subset of scripts/font features, not “all Unicode”. Unsupported color emoji/vertical typography/font variations remain disclosed extensions. Layout pseudolocalization should expand labels by 50–100% and exercise RTL mirroring separately from bidi text. Logical Start/End alignments replace left/right assumptions in new authored documents.

Cache default budgets: 32 MiB CPU text/layout/face-derived data plus 32 MiB GPU atlas data per active project UI, configurable with diagnostics. Count fallback faces and hidden-screen caches. LRU evicts unpinned runs/pages; active render commands hold `Ref` pins until submitted. Avoid unbounded dynamic glyph rasterization: prewarm/package corpus pages; runtime requests use a bounded queue and replacement glyph until the safe upload boundary. Persist derived font data by file hash, family config, generator/Unicode version and render parameters, never pathname alone. Device texture limits and backend lifetime still apply. Resource construction failure preserves the old valid asset while reporting the attempted replacement. Packaging includes baked data/source fonts and every fallback notice.

### Input, controls and events

Add device-neutral UI actions (Navigate, Next/Previous, Accept, Cancel, Point/Press, Scroll, Context, Adjust) supplied by the input/action layer. Keyboard/mouse/gamepad maps are remappable assets with standard defaults; controller dead zones, repeat delay/rate and disconnect behavior are explicit. Pointer events carry pointer ID, ordered timestamp/sequence, position and surface ID; touch reuses them later. Route gameplay/UI once through action contexts; provide a deliberate raw-platform escape hatch for tools.

Hit entries declare **Ignore / PassThrough / Block / Control** and use effective clip plus painter order. Decorative children default Ignore. Disabled controls block pointer by default while not activating, configurable for overlays; transparent color does not decide blocking. Focus scopes restrict modal navigation; tab order is stable, directional navigation uses explicit neighbors then deterministic geometry scoring, and scroll views can reveal a requested child before focusing. Capture has owner/pointer/scene generation; leaving the viewport, focus loss, hide/disable/destruction/reload/disconnect cancels without activation. Repeat applies to navigation/adjustment, never default button Accept.

Screen service owns push/pop, focus return, initial focus, popup placement/dismissal, modal input boundary and optional dimming. Only the top eligible modal scope receives UI actions; gameplay remains blocked even if the modal has no buttons. Cancel closes only when that screen's policy allows it. Opening a popup during a gesture cannot forward its release to a new control. Input queries in multiple consumers cannot create another activation from one physical action. Drag/drop carries a typed item key/owner token, validates the target and supports a controller/keyboard alternative.

Prefer **typed, scene-owned event records with one declared owner** over arbitrary retained delegates. Each event includes scene identity, element ID, kind, value and sequence. Legacy `TryConsumeAction` remains an adapter with exclusive consumption; new control event routes cannot also be consumed through a second queue. Optional C# subscription conveniences are domain-owned `IDisposable` tokens, destroyed before reload/Stop and never invoked after target retirement. Entity references plus a validated endpoint picker connect designers to scripts. A generated enum/ID maps authored action identities to typed code; invalid endpoints/duplicates get actionable editor diagnostics.

Proposed ordinary C# use is illustrative, not an implemented API:

```csharp
// Inspector assigns the actual entities; wrappers are cached at startup.
void OnCreate() {
    volume = Volume.GetComponent<UISliderComponent>();
    menu = GetComponent<UIScreenComponent>();
    menu.SetInitialFocus(Resume);
}
void OnUpdate(float dt) {
    while (volume.TryConsumeValueChanged(out float value))
        AudioSettings.Volume = value;
    if (ResumeButton.TryConsumeActivated()) menu.Close();
}
```

Runtime setters validate values and mark precise layout/paint/semantic/input dirtiness. Expose focus/value/selection/visibility/scroll/theme through consistent C++ and C# component APIs. Property bindings are explicit endpoint-to-property connections with initial sync and change notifications; avoid reflection/name lookup every frame or silently bidirectional feedback loops. Two-way binding is opt-in with equality/reentrancy guards. Game data stays in game scripts; UI can format/localize a value and update the display when it changes.

### Themes, templates and lightweight animation

ProjectAssets loads a theme with named font/color/spacing/radius/focus/audio tokens and control state styles. Resolution order: engine defaults -> canvas theme -> referenced control style -> local overrides -> runtime transient state. Show origin/reset of each override; cycles fail validation. Prefab assets continue to save whole subtrees. Add shared style/template references and revision tracking for shared updates before offering full linked-prefab variants; ordinary prefab instances keep existing independence. Templates have stable part IDs and remapped internal Entity refs; changing a part schema reports migration conflicts instead of erasing overrides.

State transitions specify property, duration, easing and optional sound; panel fade/show/hide is a small API/preset, not an animation graph. Default visual transforms do not trigger layout reflow; transformed interaction geometry follows the displayed control while it is interactable. Interaction can be disabled during exit and visibility collapsed at completion. A new transition replaces from the current value; destruction/Stop/reload cancels handles and releases references. Use an unscaled UI clock for pause menus, bounded dt after focus/minimize, no replay of missed audio. Reduced motion makes nonessential motion immediate while retaining focus/value/error cues. State preview in Edit never dispatches events or changes authored defaults.

### Screen-space and world-space contracts

Define a `UISurface` abstraction now: reference/local coordinate system, logical size, resolved snapshot, render destination, clip representation, ordering and input mapping. Screen-space uses a viewport/window surface and orthographic overlay adapter independent of the game camera; it will work over a real 3D scene without changing layout. Projected world prompts use an explicit camera projection plus viewport conversion and behind-camera/offscreen policy, not a text Transform trick.

World-space later attaches a planar surface to an entity Transform with declared units-per-logical-pixel and a finite local rectangle. A chosen camera renders it with its real view/projection; pointer/controller rays intersect the surface plane, reject backfaces/outside bounds, and map the hit to its local UI units before ordinary routing. Camera selection is explicit Entity/viewport ownership, not whichever camera was last rendered. Depth policy is declared: depth-tested/occluded by world by default, or deliberate overlay; input occlusion must match that policy through the 3D scene query. Multiple surfaces choose eligible nearest intersections with deterministic ties, bounded by modal/screen ownership.

Scissor alone cannot clip rotated perspective content: world UI needs a clip-mask/stencil or offscreen-texture path. Render-to-texture has explicit resolution/format/update policy, UV mapping, mip/sampling policy and project/device budget; a monitor mesh maps a ray's UV to the same local surface. Offscreen targets are renderer-owned `Ref` resources, with opaque handles/generations in UI commands. Backend-neutral commands express ordered quads, glyph runs, geometric focus decoration, rectangular/stacked clip operations, opacity and material/sampling references; no GLuint, GLFW or ImGui type enters authored UI.

Do not implement world UI before the engine provides camera matrices, depth/occlusion queries, a render-pass contract and world picking. Establish interfaces and CPU ray/coordinate acceptance fixtures now; actual 3D render/interaction acceptance belongs to that integration stage. Metal can implement the command/clip/resource adapter later without replacing scene components, typography or controls. No fake 3D canvas mode is recommended.

### Diagnostics and failure behavior

Authored validation reports all practical issues with document/entity/field and severity; inspector edits are transactional and undoable. Runtime mutation rejects invalid input before replacing a valid value. Missing optional visuals use a known fallback and diagnostic; a broken nonmodal subtree is isolated and noninteractive. A broken modal must retain a visible error/cancel boundary and gameplay blocking, not silently release input into gameplay. Missing essential startup assets/unsupported schema may reject startup cleanly with a precise error/exit status. Editor drafts remain editable/recoverable. Queue/layout/cache limits report counts and offending paths with rate limiting. Structural cycles and invalid cross-scene refs are rejected without partial mutation. Tests cover allocation/font/texture construction failures and raster-state restoration, including failure mid-command stream.

## 9. Hazelnut workflows and proposed UX

Authoring is a production prerequisite. Keep the existing editor shell, hierarchy, Inspector, asset picker and document controller; add a UI workspace/view mode rather than a separate incompatible editor or routine Python scene generator.

### Current menu walk-through

1. Add Entity -> Runtime UI -> Canvas. Set reference pixels, safe insets, sort order and Block gameplay. The Canvas outline shows its safe area; transform fields still appear even though they do not position this UI.
2. With Canvas selected, add Panel and edit its Rect or drag it. Add a Flow group under the panel; choose direction/gap/padding. Add a Button while the intended parent remains selected. Creation selects the new child, so repeated additions require reselecting the group; otherwise they become nested descendants. An implicit canvas may be created when starting from a non-UI selection.
3. A Button preset combines Sprite Renderer, Text, Text Style and Button on one entity. Edit caption, font/size, action string, tab order and five state colors/images. It is initially easy to forget whether color belongs to Text, Sprite or Button tint; preview states require Play interaction.
4. Attach the normal controller script and assign Entity fields, or attach a per-button script. Use `UIButtonComponent.TryConsumeAction` to select/load a scene. Global action strings require handwritten routing. Save/reopen, test several preview aspects, then Play/Stop. Save the canvas as a prefab; referenced external controller entities must be cleared or included.

This creates a functional menu with existing editor commands and C#. It lacks content-fit labels, shared style updates, designer event connections, precise alignment, safe undo and controller settings. Numeric TabOrder is discoverable only through Inspector, not visible in context. Existing TitleUI prefabs are a good starting point, but authoring a shared button change independently across scenes is repetitive.

### Current HUD walk-through

Create a nonmodal Canvas, corner-anchor a panel or text, edit logical offsets, add images/text/nine-slices using normal asset pickers, and assign text entities to the game script. Cache Text wrappers and update when values change. Resize/aspect preview evaluates the runtime layout. Hidden/disabled remain distinct. Progress bars currently need game-owned sprite/rect changes, and C# cannot change the UI rect size, so ordinary meters lack a complete public workflow. Selection comes from renderer picking; empty flow/rect groups often require hierarchy selection. There is no canvas isolation, guides, snapping, align/distribute, multi-selection or layout overflow overlay.

### Current scrolling-journal walk-through

Create a modal Canvas, Panel, heading and Text/Flow children; enable inherited clipping and populate text with normal scripts. **The next step cannot be completed in the editor: no Scroll View, content extent, scrollbar, wheel route or ensure-visible operation exists.** Lightkeeper uses `Story.Journal` pages and the dialogue Next/Close mechanism instead. That is a real authored paged journal, not proof of scrolling support. No routine Python edit is needed for current controls; the missing behavior must become a native control rather than a recommended scene-generating workaround.

### Target workflows

Provide creation presets **Menu screen**, **HUD canvas**, **Scrollable journal**, **Settings row**, **Inventory grid**, **Dialogue**, and individual controls. Presets select a usable theme, layout and first focus/cancel defaults, expose game-data placeholders and create no game-specific keys. A palette drops into a clearly highlighted intended parent with a “create sibling” shortcut. The hierarchy badges canvases/containers/controls; collapsed template parts remain explicitly editable. A UI mode isolates a selected canvas, dims the world, and uses screen pan/zoom independently of the world camera.

For a menu: create Menu screen, choose a theme, enter button labels/localization keys, choose events through the script endpoint picker, set default focus, and preview state/aspect/text scale/locale. Use content sizing for labels and stack layout for rows. For a HUD: use anchors and safe-area guides, progress/value bindings and image aspect presets. For a journal: create Scrollable journal, bind a keyed entry list, choose an item template, preview 0/1/1000 entries and long translations, then test pointer/controller scroll in Preview/Play. Save reusable assets and propagate theme changes with local override indicators. Normal scripts supply data and decisions, not geometry/navigation.

Two annotated wireframes specify behavior; these are design examples, not captures of existing tools:

```text
UI workspace: Journal                 [1280×720 ▼] [150% text] [ar ▼] [Preview]
Hierarchy          Canvas (isolated, safe-area guides)      Inspector
Journal [modal]    ┌─────────────────────────────────┐    Selected: Entry list
 Header            │ Journal                 [Close] │    Width: Fill
 Entry list        │ ┌──── viewport clip ────────┐  ▌ │    Height: Fill
  Content [flow]   │ │ Entry title               │  ▌ │    Content: keyed entries
  Scrollbar        │ │ Longer translated text…  │  ▌ │    Template: JournalEntry
 Footer            │ └──────────────────────────┘    │    Scroll: Vertical
                   └─────────────────────────────────┘    [Reveal focused item]
1: size/locale are preview overrides, never authored changes
2: clip and overflow guides share runtime-resolved geometry
3: layout-owned fields explain their source rather than accepting ineffective drag
```

```text
UI debugger [Pick] [Bounds] [Clips] [Hit order] [Navigation]
Surface Journal / generation 42 / logical 1280×720 / framebuffer scale 1.25
Focus: Close    Capture: none    Modal owner: Journal    Gameplay: blocked
Selected Entry#17: measured 420×96; arranged 420×96; clip 420×48
Hit: ignored decoration       Overflow: 48 px below viewport
Layout: 0 dirty roots         Text: 1 miss / 26 hits     Commands: 38
Source: JournalEntry template -> Body style -> local font-size override [Reset]
1: Pick distinguishes a visual element from its control/event owner
2: runtime costs have explicit units and measured scope; never guessed GPU time
```

Add eight resize handles, anchor/pivot medallions and parent links, numeric resolved bounds, snapping toggles/guides, align/distribute, and multi-selection. Use layout-aware resize/move/reparent commands; flow children show drag-to-reorder and editable size/margin, with positions read-only. Anchor changes offer preserve-resolved-rect rather than silently jumping. Empty rects can be selected by bounds and hierarchy; a selection stack resolves overlapping elements. Preview has a deliberate edit-versus-interact mode, no game side effects, and explicit state simulation.

Undo/redo is shared **scene/prefab document infrastructure**, covering Inspector commits, each complete drag gesture, hierarchy reorder/reparent, create/delete, duplication, paste, theme assignment and script connections. Prefer bounded typed commands with authored before/after values/subtree snapshots; 100 operations/64 MiB default, diagnostics on eviction. Cancelled gestures roll back. Selection/runtime state is not serialized into commands. Clipboard IDs remap and external references follow existing prefab checks. Recovery original-file history, autosave and Save As remain useful alongside undo, not substitutes for it.

Use reference-size and actual-size previews, safe-area overlays, portrait/minimum size, pixel sampling, text scale and localization expansion. Explain asset errors inline and in a navigable problems panel; show overflow, unsupported glyphs, interrupted rect chains, clipping/focus, ignored layout offsets and budgets. Add theme/token/state preview and template part/override controls through ImGui. Routine content/font/style/layout editing must require no Python script.

Workflow references are public, with concrete principles rather than an appearance specification:

- [Unreal UMG anchors](https://dev.epicgames.com/documentation/en-us/unreal-engine/umg-anchors-in-unreal-engine-ui): make parent anchors/offsets visible and test selectable screen sizes.
- [Unreal Widget Reflector](https://dev.epicgames.com/documentation/en-us/unreal-engine/widget-reflector?application_version=5.6): expose the actual hierarchy/geometry and pick the interaction target.
- [Unity UI Builder](https://docs.unity3d.com/6000.0/Documentation/Manual/UIBuilder.html) and [layout guidance](https://docs.unity.com/en-us/engine/6000.5/manual/best-practice-guides/bpg-uiad-index/layouts): separate containers from leaves and surface layout/style ownership while editing visually.
- [Unity UI debugger](https://docs.unity.com/en-us/engine/6000.6/manual/uitoolkits/uielements/uie-test-ui/uie-ui-debugger): inspect computed styles and distinguish transient live edits from saved authoring.
- [Public Hazel editor/game tutorial](https://docs.hazelengine.com/WorkingWithinHazel/FirstGameDocumentation): retain familiar hierarchy/Inspector/content/browser context. No proprietary “Big Hazel” source was accessed or inferred.

## 10. Dependencies and compatibility

Keep Premake/GNU Make/Windows integration and all existing vendors/pins pristine in this audit and first stage. Do not introduce an alternate build system or require a newer GL feature. A future dependency addition gets a separately reviewed root-owned pinned source/build project, consistent `Ref`/`Scope` wrappers, Debug/Release/Linux/Windows CI, checksums/provenance, notice copying and package closure.

| Recommendation | Justification and implications |
| --- | --- |
| Keep pinned FreeType + MSDF | Existing geometry/assets/renderer path; extend via additive adapters and explicit coverage/glyph-ID pages. No vendor edits. Bitmap pixel-font rendering may be a later mode if a game needs it |
| Add HarfBuzz for production shaping | Needed for glyph substitution/positioning and cluster maps shared by render/measure/edit. Use a minimal pinned CPU build with unnecessary tools/ICU/GLib integrations disabled; do not rely on incidental GTK/system HarfBuzz in Linux link flags. Its upstream license is permissive MIT-style; include the exact selected pin's notices. [API rationale](https://harfbuzz.github.io/shaping-and-shape-plans.html), [license](https://github.com/harfbuzz/harfbuzz/blob/main/COPYING) |
| Add libunibreak for line/word/grapheme boundaries | Avoid handwritten Unicode segmentation; zlib-style license, C library and generated data integrate through Premake. Pin Unicode tables/version and run conformance corpus. [Project](https://github.com/adah1972/libunibreak), [license](https://github.com/adah1972/libunibreak/blob/master/LICENCE) |
| Prefer SheenBidi for bidi runs | Small CPU UAX#9 service, Apache-2.0; validate the selected pin/conformance before adopting. HarfBuzz does not replace paragraph bidi analysis. [License](https://github.com/Tehreer/SheenBidi/blob/master/LICENSE). ICU is a larger alternative if future locale/formatting requirements justify its footprint; do not add both |
| Native platform text-input adapter | Existing GLFW committed text/clipboard primitives are useful, but runtime composition needs platform services. Start Linux X11 and Windows; expose unsupported composition explicitly. No GLFW fork/pin rewrite just to add an undocumented IME hack. [GLFW input guide](https://www.glfw.org/docs/3.3/input_guide.html) |
| No new layout/control GUI framework by default | Current scene UI model is small and compatible. A bounded measure/arrange extension avoids mirroring Scene into a second widget tree. Yoga is an evaluation alternative only if an agreed CSS/Flexbox compatibility requirement outweighs bridge/versioning costs |

These are design recommendations, not dependency installation or an approval request. Exact pins/build options and Unicode/font corpus are selected and licensed in the typography stage before its code lands. Font licenses are separate from library licenses. Broad localization versus a deliberately limited language release materially changes scope; the recommended target includes the stated multi-script corpus.

## 11. Prioritized bounded implementation roadmap

Each stage produces a reviewable change and evidence on current platforms. Do not bundle controls, a renderer rewrite, new platforms and an editor redesign in one PR. Continue to run relevant existing regressions; expand only where the new behavior demands it.

| Stage | Deliverable/prerequisites | Risks and bounded acceptance |
| --- | --- | --- |
| **P1 — Resolved snapshot and interaction lifetime** | First implementation. Freeze current behavior fixtures; fix D1–D5; index IDs, immutable logical/pixel bounds, resolved generation, precise eligibility/structural invalidation; declare hit policy and diagnose limits. No new dependency or control catalog | Native mutable component writes are the compatibility risk. Scene/thread ownership stays intact. Tests: disabled viewport queue, fully clipped keyboard activation, parent rect/component removal/reparent, hidden modal/native mutation, simultaneous release/Accept, scene/domain replacement, two consumers, zero size, order/clip agreement; unchanged legacy examples/source serialization |
| **P2 — Layout and reversible native editing** | P1; per-axis Fixed/Fill/Content, max/aspect, measure/arrange/stack/grid/wrap; schema migration with legacy mode. Shared scene/prefab undo commands; fix D6; bounds/debug overlay, layout-owned Inspector fields | Cycles/text-width dependency and stretched legacy semantics. Tests: constraints/overflow/zero/padding/disabled/hidden/nested surfaces, cycle path rejection, deterministic arrange, font/text mutation reflow, resize invertibility, single-gesture undo/redo, multi-selection/reorder/paste/remap and save/reopen. Use temporary documents only |
| **P3 — Typography and bounded font assets** | P1 snapshot; P2 measurement hooks; reviewed/pinned HarfBuzz/boundary/bidi adapters, family/fallback/notice assets, glyph-ID pages, strict decoding, shared cluster layout, ellipsis/spans, localization preview | Build/licensing/package footprint, atlas memory and first-use stalls. Tests: declared corpus, mixed bidi/combining/ligature/grapheme breaks, measure/draw/caret agreement, missing coverage/reload failure, cache evict/pin/teardown, relocated fallback notices and resource caps. Keep legacy typography until migrated |
| **P4 — Actions, screen scopes and core controls** | P1; P2 layout; P3 text for localization. Device-neutral remappable actions/gamepad/repeat; screen/modal/popup stack; composed toggles/radio/sliders/progress/tabs/tooltips/dropdown/dialog; typed event owner and designer endpoint picker | Duplicate legacy/new dispatch, native gameplay bypass, return-focus bugs. Tests: press/release/cancel/focus scopes, navigation geometry and remap/device change, disabled blockers, modal no-buttons, no same-gesture click-through, slider step/clamp/repeat, return-focus retirement; normal scripts consume once |
| **P5 — Scroll, data views, inventory and text input** | P2–P4; native platform input adapter. Scroll/scrollbar/nested consumption, keyed list/grid templates and fixed-size virtualization; selection/drag/drop plus non-pointer alternative; single/multiline/read-only-selectable fields, clipboard/caret/undo/IME | Variable-height virtualization and OS composition are difficult; initial variable-height list may remain bounded/nonvirtualized with documented item cap. Tests: 0/1/1000 items, filtering/reorder/key stability, clipped input, ensure-visible, nested wheel, retired drag payload, Unicode paste/selection/deletion, focus-loss composition cancel, X11/Windows IME and candidate placement |
| **P6 — Themes, complete authoring and release qualification** | P1–P5; shared tokens/styles/templates, state/fade/audio APIs, reduced motion/text scale and semantic snapshot; full UI workspace/debugger, warnings, presets, connections and template overrides; migrate examples without gameplay/save changes and correct D7 | Style inheritance and shared updates must not erase local overrides. Tests: references/closure/reload/cycle/error, cancel/interrupt/Stop/reload animations, no Edit actions, menu/HUD/scroll journal authored natively, keyboard/controller alternatives, undo all workflow operations, Release budgets on Grandpa, source-hidden relocated Nutella packages/Linux/Windows. Semantic bridge conformance only; OS reader support separately gated |
| **P7 — Real 3D surface integration** | Surface/command interfaces established in P1 design and renderer adapter extraction; actual engine 3D camera/pass/depth/picking prerequisites supplied by the separate 3D project | Do not unblock by fake projection. Tests: perspective/orthographic views, plane/ray roundtrip, camera selection, near/behind/backface/occlusion, transformed/overlapping surfaces, rotated clipping, screen/world modal routing, render-target UV hits, retirement and budget/resource teardown. Actual world-space acceptance requires renderer evidence |

Platform accessibility bridges follow semantic-tree stabilization and native text/focus infrastructure. Test Linux AT-SPI and Windows UIA individually; make either mandatory for games declaring that platform accessibility support. macOS OpenGL/input/IME/NSAccessibility and Metal adapters are future engine/platform stages, not work authorized by this audit plan.

### Migration strategy

Capture authored snapshots of accepted scenes, title prefabs, script references and packaging inventory before migration. Add explicit schema versions/defaults; old anchored/flow/text behavior remains in a legacy mode so a simple engine update does not move existing menus. Provide a native Hazelnut conversion with preview/diff, one undoable transaction, overflow/reference report and backup. No implicit auto-save or Python mass editor is needed. Keep legacy action/string consumption as an adapter during migration; each control selects one dispatch model so existing controllers cannot also receive typed events. Migrate one TitleUI/menu, one HUD and one dialogue/journal template first, then the remaining examples. Preserve accepted LK1–LK4 save/gameplay behavior and test scene transitions independently of presentation changes.

Style/theme migration extracts repeated values into shared assets but records local overrides. Template instance updates are explicit until stable part IDs/conflict handling exist; existing detached prefabs do not silently become linked. Font/shaping conversion is opt-in while measurement changes are reviewed at each declared locale/reference size. Packaging/managed API contract versions advance with incompatible changes, with diagnostics for stale ScriptCore/modules. Update the existing guide/checkpoint references to match implemented scope after each stage.

## 12. Completion gates and remaining acceptance

**Screen-space production milestone is complete only after P1–P6 pass**, including all listed production controls, useful native authoring, declared typography, lifecycle limits/failure behavior, script connections, performance budgets and relocated Nutella packaging. Screen overlays are camera-independent and suitable over either a 2D or future 3D game; that statement does not certify world-space rendering.

**The full requested 2D/3D production system additionally requires P7 on the actual 3D renderer.** Until then, release notes must say “screen-space production UI; world-space integration pending”. The future 3D renderer is an external prerequisite; the UI architecture and interfaces can be completed without implementing that renderer in this conversation. Do not label projected prompts or world Text components as a substitute for the world-space gate.

Release evidence must identify source/build SHAs, configurations, renderer/hardware and declared platforms; include Linux/Windows automated assertions plus manual acceptance results. Maintain a matrix of supported behavior, known limits and untested targets. Each gate verifies deterministic geometry/hit order, one activation, no retired-scene/domain callbacks, mutation/undo preservation, bounded memory, clear failure diagnostics and asset/license closure. Test with Nutella ImGui disabled, and Hazelnut native ImGui authoring without routine Python edits.

Remaining manual acceptance, not claimed here:

- A person authors and revises a menu, HUD, settings page, scrolling journal and inventory entirely through Hazelnut and normal scripts; undo/redo and reusable style/template changes remain understandable.
- Pointer/keyboard/controller navigation and focus/readability under long labels, large text, disabled controls, multiple modals, scrolling/dragging, focus loss/minimize/restore and disconnected/remapped devices.
- Native Windows hardware and Linux X11 behavior; 100/125/150/200% DPI, window/framebuffer mismatch, resize, portrait/4:3/ultrawide, monitor transitions and detached editor viewport input. Wayland requires its own platform evidence.
- Clipboard/composition with actual Linux/Windows IMEs; declared script/font combinations reviewed by competent readers; locale expansion/RTL layout and missing-glyph diagnostics.
- Measured cold-font/menu startup, idle/change CPU and GPU performance on Grandpa, with no concurrent build; resource limits and interruption/cancellation under low-memory failure injection.
- Relocated packages with source/SDK unavailable, complete theme/template/font/state/localization references and notices; no runtime dependency on editor ImGui.
- Real world surfaces and render targets once the actual 3D renderer exists; each future graphics/OS adapter must earn its own support claim.

Genuinely optional later work: arbitrary variable-height virtualized collections beyond the declared bounded mode; tree/table/docking controls; full document rich-text editing, password/auth controls, color emoji, vertical typography/variable-font features; curved/cylindrical world surfaces, VR accessibility, advanced shader masks; animation graph/timeline; full linked-prefab variant inheritance. Touch and OS accessibility bridges are staged target support, optional only when those platforms/input/accessibility claims are explicitly outside a game's release contract.

**Best first implementation milestone: P1, “resolved UI snapshot and interaction lifetime”.** Begin with D1–D5 reproductions and the existing UI smoke fixtures, make eligibility/retirement/ownership explicit, and reuse indexed resolved geometry with generation-based invalidation. Keep authored files, current C# consumption, accepted example behavior, vendors and GL4.1 compatibility stable. This gives the next conversation a bounded engine change with decisive acceptance before adding controls or expanding the editor.
