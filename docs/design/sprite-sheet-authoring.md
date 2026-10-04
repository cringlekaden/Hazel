# Sprite sheets and frame animation in Hazelnut

Implementation design, 2026-10-03. This supersedes the initial static-only recommendations: sampling extends Hazel's existing `TextureSpecification`; reusable frame animation is included. All sprite loading uses common `Texture2D` decoding/upload. This branch preserves the unmerged editor-authoring baseline and does not merge master.

## Audited baseline

The complete implementation baseline is **8f07a1ea1e33143da9121540aa82be91dee710f0** on `feature/editor-authoring`. This feature starts from its documentation tip **86c36c1c927b60016466fe0bd8fce38a1c5b846f**, whose implementation directories match that commit. Master was **7a0eec21a869a8521a5560cf8c5156343aa747e6** when audited.

| Checkpoint | Source facts at the baseline |
| --- | --- |
| Master / merged Nutella milestone | Hazelnut, Nutella, shared RuntimeSession, managed transitions, portable paths, canonical script builds/packaging and prior authoring reliability fixes. |
| `feature/example-games`, 45cac43e8b599af33cff4082ce5dd6d36e0e0db4 | Unmerged MeadowRun/Skybound, gravity/motion/camera APIs. |
| `feature/editor-authoring`, 8f07a1e | Unmerged detached one-entity prefabs, managed instantiate/destroy, ImGui project creation/build/export, separate preferences and improved inspectors. |

Baseline inspection found the following:

- File Texture2D creates a resource per call; stb flips images vertically. File sampling has no options; defaults are mipmapped linear minification, nearest magnification and repeat. Two-channel files are rejected. Generated textures already have a small specification.
- Renderer2D batches full-UV centered unit quads, carrying UVs, tiling and entity IDs. Its quad shader samples `TexCoord * TilingFactor`. Up to 32 device-clamped texture slots and 20,000 quads are supported.
- SpriteRendererComponent stores color, GPU texture and tiling. Editor/runtime rendering are separate loops; Simulate uses editor rendering. Scene copying clears physics/native owners; Play retains UUIDs, while duplication/instantiation allocate new IDs.
- SceneSerializer stages an entire candidate and loads textures before committing. Prefab v1 is one detached entity, rejects external entity references/native factories, and validates/save-stages through shared scene serialization.
- Managed Prefab is a typed path value. Script-field storage owns a special asset string, never Mono pointers. No managed sprite wrapper exists.
- Content Browser uses UTF-8 filesystem drag payloads and generic icons; AuthoringPanel dispatches script/prefab selection. SceneHierarchyPanel also draws embedded prefab properties. Dirty scene/prefab snapshots feed Save/Discard/Cancel guards. No undo framework exists.
- Project provides explicit asset roots and separator normalization, preserving legacy external textures in scenes. Prefab adds its own containment check. Atomic writes use exclusive sibling temporaries and replacement, without exclusive destination publication.
- Fonts/shaders/Mono have specialized caching/reload facilities, but no project texture or sheet cache exists.
- Canonical Python packaging inventories all eligible assets, validates all scenes/prefabs, legacy TexturePath, prefab/script/native dependencies, excludes compiler intermediates, and copies attribution. It does not prune by reachability.
- Project templates create Scenes/Scripts/Textures/Prefabs and a camera/text starter. Examples use separate PNGs; Meadow's grass/path already tile. GTK/Win32 dialogs, docking, UI scale, and separate user layouts exist. Existing service/GPU/Mono/editor/package regressions provide the verification foundation.

`SubTexture2D` is absent from this checkout and reachable history. The public pinned upstream **1feb70572fa87fa1c4ba784a2cfeada5b4a500db** also has no SubTexture2D in its [complete tree](https://api.github.com/repos/TheCherno/Hazel/git/trees/1feb70572fa87fa1c4ba784a2cfeada5b4a500db?recursive=1). Its [Renderer2D](https://github.com/TheCherno/Hazel/blob/1feb70572fa87fa1c4ba784a2cfeada5b4a500db/Hazel/src/Hazel/Renderer/Renderer2D.cpp) submits full UVs; its texture/component contracts are the baseline described above. The following asset/editor/animation services are local additions, not claims about Cherno's design.

## Responsibilities and types

The implemented path is project image → `.hsprites` → stable regions/clips → typed component/script references → resolved draws → Hazelnut/Nutella → portable export.

| Owner | Responsibility |
| --- | --- |
| `TextureSpecification`, `Texture2D` / graphics backend | Common dimensions/format/sampling validation, native CPU image decoding, graphics allocation/upload, mip generation and resource destruction. |
| `SpriteSheetDefinition` / SpriteSheet services | Versioned portable data, identity, lookup, validation, grid generation, UV/corner conversion and atomic persistence. No ImGui dependencies. |
| `ProjectAssets` | Project-scoped sheet/texture/resolved-sprite/clip ownership and caching, explicit invalidation, native import and CPU export audit. No global database/handle system. |
| `SpriteSheetDocument` | Native editable draft, disk-conflict detection, safe create/save/discard, texture repair, retired identities and reference scans. No ImGui dependencies. |
| `SpriteSheetPanel` / shared SpriteWidgets | Docking, navigation, selection, zoom/pan, gestures, overlays, inspector/drag payloads, dirty/error feedback and independent preview playback. |
| Scene / components | Authored source/settings plus per-entity transient resolution/playback, copy/lifecycle safety, timestep updates and runtime validation. |
| Renderer2D | Consume resolved texture/UV/corners/color/tiling/entity ID; share existing batching, shaders and picking. No asset loads or metadata parsing. |
| ScriptEngine / ScriptGlue | Typed managed references/fields, checked selection/playback operations and normal Mono domain ownership. |
| `SpriteAssetAudit` / canonical packaging.py | Validate metadata/images/references without a scene, GPU context or Mono initialization; check dependency inventory before copying and again after relocation. |

A narrow resolved draw type is appropriate because a texture-plus-UV subtexture object alone would omit pivots, validation, identities and reload ownership. No second sprite renderer or decoder is introduced.

Representative native contracts (complete definitions live in the linked source files):

```cpp
using SpriteID = uint64_t; // nonzero, scoped to sheet AND region/clip kind
struct PixelRect { uint32_t X, Y, Width, Height; };
struct SpriteRegion { SpriteID ID; std::string Name; PixelRect Rect; glm::vec2 Pivot; };
struct SpriteFrame { SpriteID Region; double Duration; };
struct SpriteClip { SpriteID ID; std::string Name; bool Loop; std::vector<SpriteFrame> Frames; };
struct SpriteReference { std::filesystem::path Sheet; SpriteID Region; };
struct AnimationReference { std::filesystem::path Sheet; SpriteID Clip; };
using SpriteSource = std::variant<std::monostate, TextureSpriteSource, SpriteReference>;
struct ResolvedSprite { Ref<Texture2D> Texture; std::array<glm::vec2,4> UV, Corners; float TilingFactor; };
struct SpriteAnimationSettings { AnimationReference DefaultClip; bool Autoplay; double Speed; };
```

`SpriteRendererComponent` owns Color and one Source. Whole-texture source owns path/tiling; its optional native Resource supports existing programmatic callers. Generated textures without paths cannot serialize. `SetTexture` is a native migration bridge. There is no editable texture field competing with a region field. Resolved caches are transient.

`SpriteAnimationComponent` extends authored settings with Current, SpritePlayback, a resolved immutable clip and error/epoch state. Animation supplies a rendering override while assigned; it never writes the authored static Source. Clearing/removing the animation restores the static source. Managed SetSprite changes that static source; it does not implicitly cancel animation. There is one playback owner per entity.

## Texture specification

Common enums: TextureFilter supports Nearest, Linear, NearestMipmapNearest, LinearMipmapNearest, NearestMipmapLinear and LinearMipmapLinear. Magnification accepts only Nearest/Linear. TextureWrap supports Repeat, ClampToEdge and MirroredRepeat independently for S/T.

| Property | Existing-compatible whole/generated defaults | New sheet defaults |
| --- | --- | --- |
| Min / mag | LinearMipmapLinear / Nearest | Nearest / Nearest |
| Wrap S/T | Repeat / Repeat | ClampToEdge / ClampToEdge |
| GenerateMips | true | false |
| Format | Generated RGBA8; files infer source channels | Explicit RGBA8 |
| Dimensions | Generated 1×1; file defaults 0×0 infer actual size | Recorded positive source dimensions |

Mipmapped minification without generated mipmaps is invalid. Generated textures require positive dimensions and R8/RGB8/RGBA8/RGBA32F. File decoding supports inferred or explicit R8/RGB8/RGBA8; two-channel input expands to RGBA8. RGBA32F is explicit generated float data, not an 8-bit file option. All formats are linear UNORM/float; no sRGB conversion/toggle is claimed. R8 samples red, not grayscale color.

Orientation is a common loading convention, not a per-loader toggle. CPU decoded rows start at top-left. OpenGL reverses rows once at upload so logical bottom-left UVs remain conventional. Generated `SetData` retains the existing bottom-left/GPU-oriented rows and replaces the complete level zero. Requested mipmaps regenerate after initial upload and every SetData. Framebuffer attachment allocation/filtering stays separate and unchanged. Metadata contains common enum names, not GL constants/IDs. This boundary remains suitable for future Metal implementations.

Cache identity includes canonical source path plus dimensions, format, min/mag, S/T wrapping and mip policy. OpenGL initially owns distinct complete texture resources for different specifications; there is no sampler-object system/shared decoded-image framework. Assigning a sheet never changes another whole-texture sprite's sampling.

## Format and identity

`.hsprites` YAML v1 lives anywhere inside project Assets, commonly beside its texture. Paths are Assets-relative, normalized UTF-8, and checked through `Project::ResolveOwnedAsset`, including existing symlinks. Parent traversal, rooted paths, drive paths and invalid references are rejected. Scene whole-texture compatibility still accepts legacy external paths; export requires owned paths.

```yaml
SpriteSheet:
  Version: 1
  Texture: Textures/Hero.png
  TextureSize: [128, 64]
  Sampling:
    MinFilter: Nearest
    MagFilter: Nearest
    WrapS: ClampToEdge
    WrapT: ClampToEdge
    GenerateMips: false
    Format: RGBA8
  Regions:
    - {ID: "0000000000000101", Name: Idle, Rect: [0, 0, 32, 32], Pivot: [0.5, 1]}
    - {ID: "0000000000000102", Name: Step, Rect: [32, 0, 32, 32], Pivot: [0.5, 1]}
  RetiredRegionIDs: []
  Clips:
    - ID: "0000000000000201"
      Name: Walk
      Loop: true
      Frames:
        - {RegionID: "0000000000000101", Duration: 0.12}
        - {RegionID: "0000000000000102", Duration: 0.12}
  RetiredClipIDs: []
```

Defaults apply when optional pivot/sampling/loop fields are omitted. Names are nonempty and unique within region/clip kind. Unknown versions, duplicate/unknown metadata keys, invalid IDs, rectangles, pivots, durations and texture-size mismatches produce errors. Positive finite frame durations are required even for drafts; empty clips and frames referencing retired regions remain recoverable authored data, but cannot play/export.

IDs are stored as quoted 16-digit hexadecimal strings, separate from editable names. Renaming changes no references. Deletion records a retired ID and leaves scene/prefab/script/frame references intact and visibly broken. Grid Apply only appends new rectangles, skips exact existing rectangles and allocates fresh IDs; it never replaces identities based on index/name. Overlap is allowed. A sheet's own path is its bounded asset identity: moving/renaming the file requires explicit reference repair; no global identity database is justified.

Native saves validate metadata and decode/compare source dimensions before atomic replacement. Creation/import use exclusive CreateNew publication. Invalid saves leave disk and draft intact. Document Save detects content changed on disk since Open/last Save and refuses overwrite; reconcile by preserving draft details and reopening. This is a foreground conflict check, not a concurrent-writer lock or power-loss durability guarantee.

Canonical scene/prefab excerpt:

```yaml
SpriteRendererComponent:
  Color: [1, 1, 1, 1]
  Source: {Type: Region, Sheet: Textures/Hero.hsprites, RegionID: "0000000000000101"}
SpriteAnimationComponent:
  DefaultClip: {Sheet: Textures/Hero.hsprites, ClipID: "0000000000000201"}
  Autoplay: true
  Speed: 1
```

Other sources are `{Type: None}` and `{Type: Texture, Texture: Textures/Grass.png, TilingFactor: 24}`. Legacy TexturePath/TilingFactor is still read and saves canonically. Mixed legacy/canonical source fields are rejected. Unassigned clips/typed fields serialize null. Playback time/current frame/finished state/GPU resources are never serialized. Shared animation settings parsing/writing is used by SceneSerializer and the native package audit; prefab content uses the same serializer.

## Coordinates, placement and atlas limits

Rectangles are integer `(x,y,width,height)` from the decoded image's top-left, half-open, positive, fully contained. Grid/manual tools always snap to pixels. Bounds use widened arithmetic; zero/incomplete rectangles are rejected. Grid omits incomplete edge cells rather than rounding/cropping them.

UVs are ordered bottom-left, bottom-right, top-right, top-left. Conversion reverses pixel Y and uses pixel-center endpoints: `(x+0.5)/W` through `(x+w-0.5)/W`, and analogous inverted Y. A one-pixel extent has equal endpoints. This half-texel inset reduces base-level bleeding; it cannot provide region-isolated mipmaps or anisotropic seamless repetition.

Pivots are normalized top-left `(px,py)` in [0,1], default (0.5,0.5). Quad corners are `(-px,py-1), (1-px,py-1), (1-px,py), (-px,py)`. Entity translation is the pivot anchor; entity rotation/scale operate on those local corners. Center defaults preserve existing placement. World width/height remain explicit entity scale, independent of source pixel aspect/dimensions. No pixel-to-world service is included. Picking and selection outlines follow the same corners. Colliders keep their authored offsets and are not silently moved/resized; adjust them using existing collider controls/overlays.

Whole textures retain tiling. Regions always submit tiling 1; repeating atlas UVs into neighbors is unsupported. Sheets expose all supported sampling settings, with explicit bleeding feedback. Nearest/no mips suits unpadded pixel art. Linear sampling needs authored gutters; generated mips operate on the whole texture and can mix neighboring regions. Texture sampling is saved game data. Zoom, selection, grid proposal and preview playback are transient editor state; DPI/UI scale/layout are existing user preferences.

## Ownership, refresh and animation order

Project owns a lazily created `ProjectAssets`; scenes/prefab staging share it where they use the same root. Cache keys are canonical paths/specifications and stable sheet+ID references. Immutable sheet/clip/draw objects and GPU resources are Ref-owned. Copies may share those immutable resources, never playback state. Scene copy/duplicate/instantiate reset resolution/transient animation state and copy authored defaults/typed fields.

Preparation occurs on source changes or cache epoch changes. Draw submission and animation frames do not parse metadata/decode/upload images. Cached failures remain diagnosable until explicit invalidation. Save/Reload clears derived resolutions and texture resources, publishes the sheet revision/error and increments the epoch. Existing Ref owners stay valid while new resolution is prepared. Refresh before Play/Simulate reloads cached sheets and clears texture/derived caches; unrelated cached broken assets do not prevent valid scenes from running, but referenced broken content fails strict runtime validation.

Authoring edits are disabled during Play/Simulate. External disk edits are applied by explicit Reload or the next Play freshness boundary, not automatic per-frame polling. A native invalidation during a session safely re-resolves by epoch, retains the current typed reference and clamps playback time to the new clip duration; an invalid reference renders a magenta diagnostic placeholder. No stale pointer or silent substitute is used.

Runtime order: deferred lifecycle start/managed/native scripts → animation timestep → physics → sprite preparation/render. Script Play/SetSprite is visible that frame; a script's finished query sees completion from the preceding animation update. Scene pause holds animation; Step advances one supplied engine timestep exactly when physics advances. Simulate advances the same timing without scripts. New Play/restart uses frame zero, autoplay and authored speed. Speed is finite/nonnegative; zero holds, negative/reverse playback is unsupported.

Playback uses cumulative clip time and bounded frame lookup. Looping retains fractional remainder; very large finite timestep/speed products use overflow-safe modular reduction, including MSVC's double-sized long double. Non-looping clips hold the last frame and report Finished/IsPlaying=false. Pause holds; Resume restarts a finished clip; Stop resets to frame zero and clears Finished. Stopping the editor Play session restores the untouched authored editor scene. Preview owns a separate SpritePlayback, advances from editor timestep, and never mutates gameplay.

Minimal managed API lives in `Hazel/Scene/Sprite.cs`: immutable Sprite(sheet,regionID) and SpriteAnimation(sheet,clipID), authored public fields selectable in the shared Inspector, SpriteRendererComponent.SetSprite, SpriteAnimationComponent.Play/Pause/Resume/Stop/IsPlaying/IsFinished. Fields serialize typed maps and survive reflection/copy/reload without native/Mono pointers. Missing components/invalid clips produce checked managed errors, not invalid dereferences. No event tracks, graphs or speculative animation bindings.

## Exact ImGui workflows

One open sheet document is supported. Opening another uses the existing save guard. Closing the dockable window hides its document; dirty state remains guarded. Reopen by selecting the same asset. Controls use normal ImGui widgets/scrollable children and existing UI scale; a narrow dock reduces canvas space, so undock/enlarge for detailed work. No general multiple-document/undo framework is introduced.

**A — regular grid:** right-click Content Browser background → Import Texture… → native file dialog → Assets-relative destination → Import and create sheet…, or right-click an existing project image → Create Sprite Sheet…. Choose a unique `.hsprites` destination → Create. Select its asset to open. Regions → Grid slicing: cell width/height, left/top offset, right/bottom margin, X/Y spacing, counts (0 derives), prefix/start number → Preview grid. Inspect green proposal → Apply grid. Exact existing rectangles are skipped; names use prefix_000 and collision suffixes. Select regions and edit names → Save sheet / focused Ctrl+S. Select an entity → Assign to selected entity, or drag a region row onto Properties → Sprite Renderer → Region. Save the scene.

**B — irregular sheet:** create/open the asset → Create rectangle tool → left-drag. Rectangles snap to pixels. Select/move tool → left-click region and drag; drag selected bottom-right square to resize. Wheel zooms around cursor, middle drag pans, Fit resets placement. Numeric X/Y/Width/Height correct any gesture. Names have readable dark overlays at useful zoom; selected outline is yellow, pivot orange. Save.

**C — pivot:** select region → normalized Pivot sliders, Center pivot or Feet pivot, or enable Pivot tool and click/drag in the region. The crop preview shows the entity-origin cross and unit-quad placement; canvas marks the pivot. Save and inspect entity transform/picking/selection outline. Adjust collider offset separately with existing collider properties/overlay.

**D — referenced edits:** open the sheet, edit its draft, preview locally. Saved scene/prefab references remain IDs; previews do not alter scene component data. Save validates/publishes and invalidates caches; edit-mode instances refresh. Unsaved sheets trigger Save/Discard/Cancel before project/scene replacement, export, Play/Simulate or close. Assets are read-only during sessions. Reload from disk refuses dirty drafts; explicit Discard sheet changes… asks before reopening.

**E — rename/delete:** rename the display name and Save; existing references and frames retain their IDs. Delete region… / Delete clip… lists saved scene/prefab/script uses and clip-frame uses, warns about unsaved references, then requires explicit Delete. Retired IDs are persisted. Broken references display their ID/error; choose a replacement explicitly in component/frame/script pickers. Missing regions are never replaced by the first region.

**Animation authoring:** Animation tab → Create clip → name / Loop. Select a region in Regions, then Add selected region; each frame also has a region combo, positive Duration seconds, Remove/Up/Down. Play preview / Pause preview / Stop preview, scrub, current-frame number and total duration share this tab. Save before assignment. Assign animation to selected entity adds the renderer/animation components as needed. Properties → Sprite Animation selects Default animation, Autoplay and Playback speed. Drag clip rows onto clip fields; prefab Inspector and managed Sprite/SpriteAnimation fields use the same pickers. Clearing/removing animation restores the static source. No duplicate clip editor is embedded in entity properties.

**Sampling:** Texture tab → Minification, Magnification, Wrap X/Y, Generate mipmaps, byte format. Effective settings persist and apply in preview/runtime. Invalid mip-filter/mip-policy combinations block save and report errors; choose a base filter or enable mips explicitly.

**F — package/run:** save sheet and scene/prefab → Project → Export Game, using existing SDK/Python readiness/output controls. Canonical export builds Release/scripts, invokes native CPU asset audit before copying, checks included dependency membership and repeats validation on the relocated copy. Sheet plus source texture, prefab/component/frame references and licenses ship without requiring prior editor loading. Extract and run Nutella from another directory; no source/SDK/importer is needed.

**G — recovery:** strict scene/project opening retains the previous scene on asset failure. File → Open Project for Repair… / Open Scene for Repair… preserves typed missing references with placeholders. Inspect a parseable sheet → Texture → Select / repair texture… (or drop one), explicitly accept its dimensions and repair out-of-bounds rectangles → Save / Retry texture. Malformed/future metadata opens Sprite Sheet Recovery with diagnostic and Open file/Open folder/Copy path/Retry open. Original bytes and current document remain intact; repair with a text editor. Save conflicts retain the draft and disk; reconcile explicitly. Repair-loaded prefab inspectors can save recoverable broken references; instantiation/Play/export remain strict.

## Changed files and dependency-ordered stages

| Stage / exact files | Purpose |
| --- | --- |
| 1: `Hazel/src/Hazel/Renderer/Texture.{h,cpp}`, `Platform/OpenGL/OpenGLTexture.{h,cpp}`, `Renderer/Font.cpp` | Common texture configuration/decoding/upload; explicit nonmipmapped font sampling. Framebuffers untouched. |
| 1: `Core/FileSystem.{h,cpp}`, `Platform/{Linux,Windows}/*FileSystem.cpp`, `Project/Project.{h,cpp}` | Exclusive creation, reusable owned paths and project-scoped typed services. |
| 2: `Assets/SpriteSheet.{h,cpp}`, `ProjectAssets.{h,cpp}`, `SpriteSheetDocument.{h,cpp}` | Portable schema/identity/conversion, caching/native audit/import and reusable documents. |
| 2/4: `Scene/Components.h`, `Scene.{h,cpp}`, `SceneSerializer.{h,cpp}`, `Prefab.{h,cpp}`, `RuntimeSession.cpp`, `Renderer/Renderer2D.{h,cpp}` | Canonical sources, pivot/UV rendering, static/animation resolution, lifecycle/copy, strict/repair staging and shared serialization. |
| 3/4: `Hazelnut/src/Panels/SpriteSheetPanel.{h,cpp}`, `SpriteWidgets.{h,cpp}`, `ContentBrowserPanel.{h,cpp}`, `SceneHierarchyPanel.cpp`, `Authoring/AuthoringPanel.{h,cpp}`, `EditorLayer.{h,cpp}` | Full native authoring UI, assignment, preview, save guards/recovery and correct outline. |
| 4: `Hazel-ScriptCore/Source/Hazel/Scene/Sprite.cs`, `InternalCalls.cs`, `Scripting/ScriptField.h`, `ScriptEngine.{h,cpp}`, `ScriptGlue.cpp` | Typed references/fields, managed renderer and playback controls. |
| 5: `tools/SpriteAssetAudit/{main.cpp,premake5.lua}`, root `premake5.lua`, `scripts/dependencies/{premake5,scene-foundation,fonts,physics,mono}.lua`, `scripts/internal/{packaging,tests}.py`, `.github/workflows/c-cpp.yml` | Canonical native package validation and existing pinned Premake/test/CI integration. No pins change. |
| 5: MeadowRun `Textures/Lanterns.{tga,hsprites}`, `Scenes/Meadow.hazel`, `Prefabs/LanternSeed.hprefab`, README/LICENSE | Static explorer region plus reusable seed pulse; original existing art composed natively. No gameplay rewrite. |
| Verification: `tests/migration/SpriteSmoke.cpp`, `ManagedFixture.cs`, `premake5.lua` and existing sprite consumer smoke files | Service/timing/serialization/cache/export and small real rendering/managed checks, plus coherent old-API migration. |

Templates require no new pipeline or compulsory sprite assets: existing camera/text/whole-texture projects remain compatible. New sheet assets can be created in any eligible Assets directory. Windows CRT/UTF-8, Linux/Windows OS splits, Scope/Ref, OpenGL 4.1/HD4000 4.2 and pinned vendors are preserved.

## Verification and manual acceptance

New focused SpriteSmoke covers defaults/invalid sampling, common decoder origin, UVs/zero/bounds, pivot corners and rendered picking/placement, grid identity, rename/delete/retired IDs, malformed data/atomic safe saves, legacy sources, cached distinct sampling, mips/SetData, scene/prefab compatibility, independent timing/copies, pause/step, large timesteps, managed typed references/controls/update order, invalidation, strict/repair failure isolation, dependency closure and relocation. Existing renderer/editor/scene/runtime tests verify retained workflows; no new whole-game driver or pixel-click acceptance program is introduced.

Verification evidence is recorded after execution. Linux/Windows Debug/Release builds and short native/software/4.1 smoke checks are appropriate. Human acceptance remains necessary:

- Import a grid sheet, preview/apply twice, confirm IDs/names/pivots survive, then save and assign by button/drag.
- Draw/move/resize irregular regions and correct numeric bounds at multiple zoom levels; verify pan/zoom/tool discovery and small-screen/DPI docking.
- Preview centered/feet pivots; verify entity placement, picking/outline and explicitly authored collider offset in Edit/Play.
- Author/reorder/per-frame-duration clips; preview, scrub and assign to a scene entity/prefab/script field. Check pause/step/autoplay/speed-zero/play-once completion and Stop returning to authored content.
- Rename referenced items; delete after the usage warning and repair the exact broken IDs explicitly. Check Save/Discard/Cancel and external-edit conflict handling.
- Remove/restore a texture and corrupt/repair metadata; confirm previous scene/draft/disk stay recoverable and Play/export reject unresolved content.
- Run exported MeadowRun from an extracted relocated directory; inspect static explorer region and pulsing seeds, license inclusion and shutdown.

## Bounded limitations and decisions

One sheet per clip, forward playback, normalized in-bounds pivots, unit world quads, one open document, one resize handle plus numeric editing, explicit refresh and full eligible-asset exports are deliberate first contracts. The legacy API names/color pipeline are preserved where possible; native component callers migrate to Source/SetTexture. Retired IDs grow monotonically. Disk conflict detection does not lock other applications. Linear/mip bleeding needs authored padding and visual review.

Deferred: animation controllers/graphs/transitions/events, skeletal animation/blends, automatic packing, polygon trimming, rotated packed regions, nine-slicing, tile-map authoring, multi-resolution atlases, a global asset database/import framework, sampler-object architecture, pixels-per-unit scaling, multi-document infrastructure, general undo/redo, macOS/Metal implementation. Existing font atlases do not justify expanding game-sprite packing.

No architectural preference remains required before using this milestone. Implement/review in bounded stages, retaining native service/UI separation; subjective editor usability and art sampling still need human acceptance.
