# Sprite-sheet authoring in Hazelnut

Audit date: 2026-10-03. The original static-sprite audit below records the baseline. Implementation is now authorized on `feature/sprite-sheet-authoring`, with the following scope amendments taking precedence over the original recommendations. No master merge or release is authorized.

## Implementation scope amendments

1. All file/generated textures use Hazel's shared `TextureSpecification` and `Texture2D` implementation. Common enums specify min/mag filters, wrap S/T, mip generation, dimensions and format. Legacy defaults remain linear trilinear minification, nearest magnification, repeat and generated mips. File defaults infer dimensions/format; generated textures require explicit valid dimensions/format. No sprite loader, sampler-object system or sRGB toggle is introduced. Native CPU inspection/decode is part of Texture2D and is shared with headless asset validation. Decoded files have top-left rows; OpenGL upload reverses rows once. SetData uses existing GPU-oriented rows and regenerates requested mips. Framebuffer attachment allocation is unchanged.
2. Include reusable single-sheet frame clips **inside `.hsprites` v1**: stable clip ID/name, ordered region-ID frames with positive finite durations, and Loop. Retired clip IDs prevent identity reuse. Empty clips are valid authoring drafts but invalid playback references. Region deletion leaves broken frame IDs diagnosable; clip/region renames preserve references.
3. `SpriteAnimationComponent` owns default `(sheet,clipID)`, autoplay and finite nonnegative speed; transient playback time/status/current frame are per entity and excluded from serialization/copy. Zero speed holds the frame. Scripts run before animation advances by the accepted scene timestep, then render; paused scenes advance only during Step. Play-once retains its final frame and reports Finished; Stop resets to frame zero. New Play starts authored defaults. An active animation supplies a transient rendered sprite override; it never overwrites SpriteRenderer's authored static Source. Removing the animation restores that Source.
4. Hazelnut's sheet panel adds one Clips section: create/rename/delete, frame region picker, add/remove/up/down, duration, Loop, independent Play/Pause/Stop/scrub preview/current frame/total. Entity/prefab inspector assigns clip/autoplay/speed; managed typed SpriteAnimation references and Play/Pause/Resume/Stop/status calls are bounded playback controls.
5. Implement in the user's order: shared textures/path/safe writes; sprite services/render/persistence; sheet editor; animation/component/bindings/UI; native export audit/examples/platform verification. Use original representative MeadowRun content, preserve unrelated Skybound edits, and verify through short service/render/package checks rather than game-playing drivers.

The animation deferral, separate ImageReader suggestion and fixed sheet-only filtering recommendation below are superseded: the asset persists effective shared sampling options. Atlas mip generation is explicitly exposed with a bleed warning; no per-region mip isolation is promised. Default atlas sampling remains nearest, clamp, no mips. ImGui preview/state-machine logic delegates to reusable native playback functions.

## 1. Audited baseline and branch placement

Design against the completed authoring implementation **`8f07a1ea1e33143da9121540aa82be91dee710f0`**, on `feature/editor-authoring`. Checked-out HEAD is **`86c36c1c927b60016466fe0bd8fce38a1c5b846f`**: its changes are documentation only, and its implementation directories match `8f07a1e`. This is the latest complete authoring implementation in the local branch history, rather than master.

| Local branch/checkpoint | What is present |
| --- | --- |
| `master`, `7a0eec21a869a8521a5560cf8c5156343aa747e6` | Hazelnut, Nutella, shared `RuntimeSession`, managed scene transitions, portable resource/project paths, canonical script builds and packaging. Includes the earlier authoring reliability fixes via `b0fb5fc`. |
| `feature/nutella-runtime`, `dbd840ef51815ebcf106410ab676e37576f7f94b` | Runtime milestone already merged into master. |
| `feature/example-games`, `45cac43e8b599af33cff4082ce5dd6d36e0e0db4` | Descends from master; adds MeadowRun and Skybound, gravity/motion/camera scripting controls. Not merged into master in this history. |
| `feature/editor-authoring`, implementation `8f07a1e`, documented tip `86c36c1` | Descends from the example branch; adds detached single-entity prefabs, managed instantiate/destroy and cleanup, ImGui project creation/build/export, preferences separate from project settings, improved inspectors and example authoring. Not merged into master. |
| `fix/authoring-reliability`, `11259a28ac8f26f09223011dfa6c733ae61b6f48` | Repairs merged into master before Nutella. Not a newer alternative baseline. |

Local and remote-tracking tips agree; no remote freshness is claimed. Preserve the modified `examples/Skybound/Assets/Prefabs/LowerPipe.hprefab` and `examples/Skybound/Skybound.hproj`, layout stash, ignored `.vscode`, user preferences/layout and vendors. These local edits are outside the committed baseline. This audit changes only this document.

### Current architecture, verified in source

| Area | Existing behavior and consequence |
| --- | --- |
| [Texture API](../../Hazel/src/Hazel/Renderer/Texture.h), [OpenGL loading](../../Hazel/src/Platform/OpenGL/OpenGLTexture.cpp) | `Texture2D::Create(path)` creates a new resource each time. File textures retain their resolved UTF-8 path. stb loads with vertical flipping enabled; 1/3/4-channel images are accepted, two-channel images rejected. File loading offers no sampling options. Defaults generate mipmaps, use linear mipmapped minification, nearest magnification and repeat wrapping. |
| `SubTexture2D` | No definition, implementation, include or use exists in this checkout; no matching file appears in reachable local history. It cannot currently be extended or wrapped. |
| [Renderer2D](../../Hazel/src/Hazel/Renderer/Renderer2D.cpp), [quad shader](../../Hazel/Resources/shaders/Renderer2D_Quad.glsl) | Quads already carry per-vertex UVs and entity IDs, but public textured submission always supplies the full `[0,1]` rectangle. Shader sampling multiplies those UVs by `TilingFactor`. Batching groups by actual texture equality, with a device-clamped limit up to 32 slots and a 20,000-quad limit. Regions could share a texture slot. |
| [Components](../../Hazel/src/Hazel/Scene/Components.h), [Scene](../../Hazel/src/Hazel/Scene/Scene.cpp) | A sprite stores `Color`, `Ref<Texture2D>` and `TilingFactor`. Runtime and editor each call `DrawSprite`; Simulate uses the editor rendering path. Geometry is a centered unit quad transformed by entity translation/rotation/scale. |
| Scene values/lifecycle | Copy keeps entity UUIDs for Play; duplicate/instantiate allocate new entity UUIDs. Component copying shares texture resources, clears physics/native runtime owners and copies authored script fields independently. Managed creation/destruction is deferred at safe callback boundaries, with initial physics ready. Sprite references must remain ordinary component values, independent of entity IDs. |
| [SceneSerializer](../../Hazel/src/Hazel/Scene/SceneSerializer.cpp) | Writes `Color`, optional `TexturePath`, and `TilingFactor`; derives texture paths from GPU resource `GetPath()`. Loads textures while staging a candidate scene. Parsing or resource failure leaves the existing scene unchanged. Stored script fields survive absent classes and own no Mono pointers. |
| [Prefab](../../Hazel/src/Hazel/Scene/Prefab.cpp) | `.hprefab` version 1 wraps one serialized entity. Load/save validate a detached scene; save is atomic. External entity references and native scripts are rejected; self references remap on instantiation. Texture containment and typed prefab fields are validated. Instances have independent component values; there is no linked-prefab propagation. |
| Managed API | [Components.cs](../../Hazel-ScriptCore/Source/Hazel/Scene/Components.cs) has no managed sprite renderer wrapper. [Prefab.cs](../../Hazel-ScriptCore/Source/Hazel/Scene/Prefab.cs) is a typed path value. Native script-field storage has a special string `AssetReference` for Prefab; reflection/reload/serialization handle it explicitly. There is no general managed asset handle. |
| Editor navigation | [Content Browser](../../Hazelnut/src/Panels/ContentBrowserPanel.cpp) lists files using generic icons; selection dispatches to `AuthoringPanel::SelectAsset` for prefabs/scripts. It is not a general asset inspector or import pipeline. `SceneHierarchyPanel` supplies Properties and the detached Prefab Inspector's embedded component controls. Texture assignment is currently drag/drop onto a button. |
| Drag/drop | `CONTENT_BROWSER_ITEM` carries a null-terminated UTF-8 filesystem path; [ContentBrowserPayload.h](../../Hazelnut/src/ContentBrowserPayload.h) validates its bytes. The serialized path is separately derived relative to Assets. Viewport drops currently dispatch prefabs or scenes. |
| Dirty state and guards | [AuthoringPanel](../../Hazelnut/src/Authoring/AuthoringPanel.cpp) compares serialized scene/prefab snapshots with saved text. `Guard` offers Save/Discard/Cancel before replacement/close/export. Ctrl+S currently saves the scene. Tool jobs keep the UI responsive and block conflicting operations. There is no undo framework. |
| Paths and persistence | [Project](../../Hazel/src/Hazel/Project/Project.cpp) normalizes separators and resolves explicit asset roots. Legacy external absolute textures remain readable, but export rejects them. `Prefab::Resolve` adds containment checks; it is prefab-specific, not a reusable strict path boundary. [FileSystem](../../Hazel/src/Hazel/Core/FileSystem.cpp) writes an exclusive sibling temporary then replaces via native Linux/Windows operations. It checks write/flush/close failures; it does not guarantee power-loss durability or exclusive publication of a new destination. |
| Cache/reload | There is a shader disk cache and Mono assembly watcher/reload. `Font` generates an atlas; its `CreateAndCacheAtlas` name does not establish a project texture/sheet cache. No project texture deduplication, sprite metadata cache or texture hot reload exists. |
| Packaging | [packaging.py](../../scripts/internal/packaging.py) validates every included scene/prefab, `TexturePath`, prefab fields and compiled script/native dependencies. `project_files` ships eligible project files, including source, rather than pruning to reachable assets. Compiler intermediates are excluded and escaping paths/symlinks rejected. There is no sheet parser or region dependency validation. |
| New projects and examples | [authoring.py](../../scripts/internal/authoring.py) generates Scenes/Scripts/Textures/Prefabs, a minimal camera/text scene, prefab lifecycle sample, and the canonical [Premake template](../../scripts/internal/templates/project.lua). Examples use separate PNGs, not sprite sheets: Explorer is 64×64, Tree 64×96, Skybound Column 64×512. Meadow's grass uses tiling 24 and path uses 3. Existing scripts instantiate prefabs and adjust transforms, without sprite selection bindings. |
| UI/platform/tests | Docking, keyboard navigation and platform windows are enabled. Layout lives in user-data `imgui.ini`; UI scale is an editor preference. Native GTK/Win32 dialogs expose UTF-8 paths. Existing focused scene/editor/Mono/render/package regressions cover atomic saves, relocation, picking, lifetimes, batching and whole-texture tiling. Historical verification includes long game/UI drivers; these are not the model for new acceptance. |

### Pinned public upstream comparison

The public pin is **`1feb70572fa87fa1c4ba784a2cfeada5b4a500db`**. Its [Renderer2D](https://github.com/TheCherno/Hazel/blob/1feb70572fa87fa1c4ba784a2cfeada5b4a500db/Hazel/src/Hazel/Renderer/Renderer2D.cpp) has batched full-UV quads and tiling; its [component](https://github.com/TheCherno/Hazel/blob/1feb70572fa87fa1c4ba784a2cfeada5b4a500db/Hazel/src/Hazel/Scene/Components.h) stores texture/color/tiling, and [texture specification](https://github.com/TheCherno/Hazel/blob/1feb70572fa87fa1c4ba784a2cfeada5b4a500db/Hazel/src/Hazel/Renderer/Texture.h) defaults to mip generation. The complete [pinned tree](https://api.github.com/repos/TheCherno/Hazel/git/trees/1feb70572fa87fa1c4ba784a2cfeada5b4a500db?recursive=1), inspected directly, contains no `SubTexture2D`. Earlier tutorials are not evidence of a facility here. Local runtime/authoring services and all proposals below are local work, not claims about Cherno's implementation or plans.

## 2. Recommended milestone

Ship **one texture-backed sprite-sheet asset with stable rectangular regions**, native asset services, a dockable authoring panel, static component selection, minimal managed selection, and verified project export. Use a small project-scoped typed service; a global asset database or asset identity service is unnecessary for this contract.

The dependency chain is `component/script reference → sheet definition → project texture → resolved sprite → renderer`. The editor edits definitions through services. Drawing never parses YAML or opens files.

| Responsibility | Owner |
| --- | --- |
| Sheet definition, region identity, format, rectangle/pivot validation, UV/geometry math | Hazel `Assets` types and pure native functions. No ImGui, graphics IDs or active-project dependency. |
| Texture decode/dimensions and GPU creation | Native image reader plus `Texture2D`/graphics backend. CPU image data and GPU resources have separate lifetimes. |
| Sheet/texture caches, lookup, dependency validation and invalidation | `ProjectAssets`, constructed with an explicit asset root; owned by Project and borrowed/shared by its scenes. |
| Runtime draw data | Immutable `ResolvedSprite`: texture resource, logical UVs, local quad corners and effective tiling. No authored labels or persistence logic. |
| Scene/prefab reference values | `SpriteRendererComponent::Source`, serialized independently of resolved resources. Prefabs reuse the component contract. |
| Draft/save/conflict tracking | Native Hazelnut `SpriteSheetDocument`, independent of ImGui; uses engine serializers/services. |
| Preview/selection/gestures/layout | `SpriteSheetPanel`; shared inspector widget for entity, prefab and script fields. |
| Managed selection | Small typed `Hazel.Sprite` reference and checked sprite renderer calls; normal Mono domain lifetime. |
| Export closure | Existing canonical Python packaging, invoking a narrow native sprite audit adapter that reuses the engine parsers/validators. |

### Why a resolved sprite, rather than SubTexture2D

Introduce `ResolvedSprite`. A texture-plus-UV object would still lack rectangles, IDs, pivots, paths, validation and reload semantics. The draw value supplies UVs **and local corners**; sheet metadata owns authoring information. Extend textured submission while retaining batching. Asset services resolve; Renderer2D submits.

### Canonical component source

| Representation | Tradeoff |
| --- | --- |
| Texture plus rectangle/pivot copied into every component | Easy rendering, but sheet edits cannot update authored uses consistently; packaging and identity become duplicated. |
| Sheet path plus region name | Small, but names become fragile links. A display rename would break content. |
| Global asset handles | Could support arbitrary moves, but require persistent global identity, indexing and migration absent from Hazel. |
| **Discriminated source: None / whole texture / sheet + stable region ID** | Explicit, compatible with existing behaviors, and sufficient for this milestone. Recommended. |

Keep color on the component and one source alternative. Tiling belongs only to whole textures. Rectangles/pivots belong to sheets; resolved data is transient, without competing editable fields.

Proposed interfaces below illustrate boundaries; implementation can refine signatures without changing these contracts:

```cpp
struct PixelRect { uint32_t X, Y, Width, Height; };
using SpriteRegionID = uint64_t; // nonzero; scoped to one sheet
struct SpriteRegion {
    SpriteRegionID ID;
    std::string Name;
    PixelRect Rect;
    glm::vec2 Pivot = { 0.5f, 0.5f }; // normalized from top-left
};
enum class SpriteFilter { Nearest, Linear };
struct SpriteSheetDefinition {
    uint32_t Version = 1;
    std::filesystem::path Texture;   // relative to project asset root
    glm::uvec2 TextureSize;           // dimensions validated at last save
    SpriteFilter Filter = SpriteFilter::Nearest;
    std::vector<SpriteRegion> Regions;
    std::vector<SpriteRegionID> RetiredRegionIDs;
};
struct TextureSpriteSource { std::filesystem::path Texture; float TilingFactor = 1; };
struct SheetSpriteSource { std::filesystem::path Sheet; SpriteRegionID Region; };
using SpriteSource = std::variant<std::monostate, TextureSpriteSource, SheetSpriteSource>;
struct SpriteRendererComponent {
    glm::vec4 Color{1};
    SpriteSource Source;
    SpriteResolution Cache;          // transient; reset on component/scene copy
};
struct ResolvedSprite {
    Ref<Texture2D> Texture;           // null for a color-only quad
    std::array<glm::vec2, 4> UV;
    std::array<glm::vec2, 4> LocalCorners;
    float TilingFactor = 1;
};
class SpriteSheetSerializer {         // definitions only; no GPU dependency
public:
    static SheetReadResult Read(const std::filesystem::path& file);
    static void Save(const std::filesystem::path& file,
                     const SpriteSheetDefinition&, WriteMode);
};
class ProjectAssets {                // no ImGui; main-thread GPU operations
public:
    explicit ProjectAssets(std::filesystem::path assetRoot);
    SheetResult GetSheet(const std::filesystem::path& relative);
    SpriteResolution Resolve(const SpriteSource&);
    ReloadResult Reload(const std::filesystem::path& relative);
    ValidationReport ValidateSprites(const Scene&); // includes typed script fields
};
// Pure functions: ValidateSheet, FindRegionByID, GenerateGridPreview,
// PixelRectToUV, PivotToLocalCorners, DiscoverSpriteDependencies.
// Renderer input: DrawSprite(transform, resolved, color, entityID).
```

Use existing `Scope`/`Ref` helpers. ProjectAssets holds its root without a Project back-reference. Scene loading accepts the explicit service even while another project is active. Copy/duplicate/instantiate copy sources by value and reset resolutions; resources may be shared. Destroy releases references without modifying assets/region IDs. Keep procedural textures available through existing low-level `DrawQuad(texture)`; they acquire no serializable project identity implicitly. Component tests use authored texture fixtures for the new source contract.

## 3. Format, identities and compatibility

Use UTF-8 YAML **`.hsprites`** under Assets, conventionally `Assets/Sprites/<name>.hsprites`; creation defaults beside the texture. References must select this extension. `Texture` is relative to **the project asset root**, never the sheet directory or cwd.

```yaml
SpriteSheet:
  Version: 1
  Texture: Textures/characters.png
  TextureSize: [128, 64]
  Filter: Nearest
  Regions:
    - ID: "6f219052ab4d8310"
      Name: hero_idle
      Rect: [16, 8, 32, 16]          # x, y, width, height; top-left pixels
      Pivot: [0.5, 1.0]             # bottom-center of this rectangle
    - ID: "a61ec9138d72504b"
      Name: hero_jump
      Rect: [50, 8, 32, 16]
      Pivot: [0.5, 1.0]
  RetiredRegionIDs: []
```

Require Version/Texture/TextureSize and each region's ID/name/rectangle. Defaults: Nearest filter, center pivot, empty Regions/RetiredRegionIDs. Empty sheets are valid, but cannot resolve a region. Write explicit defaults deterministically, preserve order and float precision. IDs use quoted 16-digit lowercase hex and native/managed 64-bit integers, avoiding numeric interpretation/precision hazards.

Reject duplicate YAML keys/IDs/names, invalid/zero IDs, active IDs also retired, unknown v1 properties, invalid enums/types, non-finite pivots and invalid rectangles. Names are trimmed nonempty UTF-8, unique case-sensitively per sheet. Overlap is legal with a warning. Reject rooted/drive-relative/UNC paths, traversal, empty references and escaping symlinks for new sheet assets. Factor strict project-path resolution out of prefab-specific logic; preserve legacy external-texture reading.

Verify the texture decodes and matches TextureSize before normal Save/runtime/export. Changed dimensions require **Accept New Dimensions** and revalidation without scaling/clamping rectangles. Authors repair out-of-bounds regions explicitly, preventing silent normalization changes.

Validate before writing; publish cache revisions only after success. Add `WriteMode::CreateNew` to native atomic publication: new assets must fail if a destination appears; normal Save retains replacement. A pre-save existence check is insufficient. Preserve temporary cleanup/durability limits. A content fingerprint detects ordinary external-edit conflicts, without claiming concurrent-writer locking.

### Stable reference policy

Generate nonzero random IDs with Hazel UUIDs, checking active/retired collisions. **Names never identify references.** Rename/rectangle/pivot/order/save preserve ID; Delete retires it. New/generated regions never reuse retired IDs. Recreating a label gets a new ID; backup restoration can restore the old definition.

Scenes/prefabs/managed fields and future frames use `(sheet path, region ID)`. Later clips can add frame durations in their own versioned asset; v1 needs no clip/controller fields. **Copy Sprite Reference** produces ID-based C#; no runtime name-lookup binding is added.

Sheet paths define asset identity: external sheet moves require reassignment/repair. Content Browser move/rename and automatic rewriting are deferred. A sheet copied to another path is separate even with identical region IDs; no global GUID is needed.

### Scene/prefab serialization

New canonical writes use one tagged `Source`, for example:

```yaml
SpriteRendererComponent:
  Color: [1, 1, 1, 1]
  Source:
    Type: Region
    Sheet: Sprites/characters.hsprites
    RegionID: "6f219052ab4d8310"
# Other alternatives:
# Source: { Type: Texture, Texture: Textures/Grass.png, TilingFactor: 24 }
# Source: { Type: None }
```

Read legacy TexturePath/TilingFactor as Texture, absent TexturePath as None. Preserve color/transforms/centered placement/external paths/tiling; do not convert games automatically. Mixed canonical/legacy keys and Region tiling/texture keys are errors. Omit meaningless color-only tiling on save. Persist authored paths even on resolution failure, independent of GPU GetPath. Existing C++ component callers migrate to source assignment/resolution.

New files require matching new engine/ScriptCore binaries; compatibility means backward reading. Older readers cannot reliably interpret canonical sources. Prefab outer v1/detached semantics remain; share sprite parsing/validation. Sheet edits affect all asset uses, including instantiated prefabs, without changing independent component values or adding linked-prefab propagation.

Keep strict transactional loads by default. **Open for Repair** adopts structurally valid candidates with broken sprite references/diagnostics; malformed component/scene data still fails. Authoring saves may preserve those references with warnings; prefab entity/native-script/containment safeguards remain. Runtime load/instantiate, Play and export require valid dependencies. Existing strict load-failure tests retain their contract.

## 4. Coordinates, placement and sampling

### One pixel convention

Rectangles use unflipped pixels: **top-left origin, X right, Y down**, unsigned integer edges and half-open `[x,x+w) × [y,y+h)`. Require positive sizes and widened-arithmetic bounds `x+w<=W`, `y+h<=H`. Snap drag endpoints to nearest integer edges, ties toward larger coordinates; normalize reversed drags and validate. Reject zero/fractional/clipped regions. Invalid numeric edits retain the last valid rectangle.

Native image data uses top-left rows. Centralize stb decode/info with explicit orientation; reverse rows for OpenGL upload, preserving current storage/whole-texture appearance without dependence on global flip history. Expand two-channel grayscale/alpha to RGBA, retaining other formats and generated texture/SetData conventions.

Renderer logical UVs retain the existing lower-left convention. For rectangle `(x,y,w,h)` in `(W,H)`, edge limits are:

```text
u0 = x/W                    u1 = (x+w)/W
v0 = 1 - (y+h)/H            v1 = 1 - y/H
corner order: bottom-left, bottom-right, top-right, top-left
```

Region sampling uses a fixed half-texel inset to keep the footprint within its texel centers: `u0=(x+.5)/W`, `u1=(x+w-.5)/W`, `v0=1-(y+h-.5)/H`, `v1=1-(y+.5)/H`. One-pixel dimensions validly collapse that UV axis. Keep the edge rectangle separate from the sampling rectangle: preview overlays and pivot math use pixel edges. Example above has edge BL `(0.125,0.625)` and TR `(0.375,0.875)`; sampled BL `(0.12890625,0.6328125)` and TR `(0.37109375,0.8671875)`. Whole textures still use `[0,1]` with no inset.

The ImGui adapter retains display V flipping; overlays use top-left pixels transformed to logical UI coordinates. DPI/framebuffer scale never enters saved bounds. Future backends map logical UVs/storage to native sampling; assets store no GL IDs, native handles or baked UVs.

### Pivots and world placement

Pivot `(px,py)` is normalized from the region's top-left edge, bounded to `[0,1]²`; fractional pivots are allowed. Default `(0.5,0.5)` reproduces today's centered unit quad. The local corners are:

```text
BL=(-px, py-1)   BR=(1-px, py-1)   TR=(1-px, py)   TL=(-px, py)
worldCorner = entityTransform * vec4(localCorner, 0, 1)
```

Offset the centered quad by `(0.5-px, py-0.5)` **before** entity scale/rotation/translation. Bottom-center `(0.5,1)` puts the lower edge at origin. Scale remains full world width/height. No pixels-per-unit, automatic aspect/transform compensation or collider regeneration; show pixel dimensions/aspect beside existing Scale controls.

Pivots move visual geometry, preserving entity/physics/script/gizmo origins. Shared pivot edits intentionally change all uses' placement; state this before Save. Center leaves old content unchanged. Share corner math across render/outline/preview and retain entity IDs so picking follows geometry. Keep existing alpha/picking behavior. Collider offsets/sizes remain independent, adjusted through Properties and the collider overlay.

### Bounded atlas filtering and tiling

Regions submit **tiling 1**: today's UV multiplication samples other regions, and texture wrapping cannot repeat one region. Replace the tiling control with “Atlas regions do not repeat; use a whole texture for tiling.” Changing source drops whole-texture tiling explicitly; switching back starts at 1. Region tiling keys are invalid.

Saved sheet Filter is Nearest by default or Linear, for minification/magnification. Use clamp wrapping, **no mipmaps**, and the inset above. Nearest suits pixel art; Linear can slightly stretch edges. This bounds neighboring-region bleed but sacrifices distant minification quality: ordinary mipmaps mix sprites. Per-region padded/extruded mip chains and seamless repetition are deferred.

Add backend-neutral file load options, preserving legacy defaults. Different sampling options require distinct cache resources; never mutate shared sampler state. Validate hardware size limits in the backend. Zoom/checkerboard/overlays/UI scale are editor state, never game filtering options.

## 5. Ownership, caching and refresh

`ProjectAssets` strongly caches committed definitions by normalized sheet reference and textures by resolved path **plus all sampling options**. Project-scoped roots prevent relocation/switch collisions. Whole textures also support retained legacy external paths; new pickers/import produce owned references. Deduplicate equivalent filesystem identities without lowercasing case-sensitive paths. Resolutions use `(sheet reference, ID, sheet revision, texture revision)` and immutable data, never draft-vector pointers.

Resolve on load/assignment/runtime setter; prepare invalidated caches before both scene render loops. Unchanged components require memory-only source/revision checks, without per-frame stats/parsing/loading. A newly selected runtime asset can load once synchronously on the context thread. Reset copied component caches and reuse shared resources; no async importer/preload API is needed.

Save stages metadata/resources, writes atomically, then publishes revisions. Invalidate affected scene/prefab resolutions at a frame boundary; labels can reuse geometry/textures. Sources and scene/prefab dirty state remain unchanged. Drafts affect only panel preview until Save; Discard abandons the draft.

External changes use **Reload** and freshness checks on open/assignment/before Play, without a new watcher. Reload validates/decodes before frame-boundary publication. Dirty conflicts offer Reload and Discard Draft / Keep Draft / Cancel; keeping requires explicit overwrite on Save. Check every referenced dependency before Play. Disable sheet editing/save/reload during Play/Simulate and freeze committed resolutions; Nutella needs no hot reload. Transitions validate candidates through the same service before retiring the previous scene.

Distinguish missing sheet/texture, malformed metadata, size mismatch and missing/retired ID. Preserve paths/IDs without fallback. Hazelnut draws a missing marker carrying the entity ID and shows Inspector diagnostics. Last-good resources may survive failed reload for recovery, but broken status still blocks Play/export. Runtime startup/transition rejects broken sources; failed assignment returns false/diagnostic and preserves the prior source.

Stage service/scene/browser/domain before project switch. Stop releases runtime caches without changing editor sources. Release all panel/scene/service GPU references before context teardown. Project-bounded ownership needs no LRU/persistent GPU/process-global cache.

## 6. Managed scripting and packaging

Expose only a typed sheet-region value and selection operations:

```csharp
public sealed class Sprite {
    public readonly string Sheet;    // project asset-relative .hsprites
    public readonly ulong RegionID;
    public bool IsAssigned { get; }  // nonempty path and nonzero ID, not load success
    public Sprite(string sheet, ulong regionID);
}
public class SpriteRendererComponent : Component {
    public Sprite Sprite { get; }    // null for None/whole-texture sources
    public bool TrySetSprite(Sprite sprite); // null explicitly clears to None
}
// Typical authored use: public Sprite Idle; then renderer.TrySetSprite(Idle).
```

Use checked entity/component calls and the runtime scene's service. Sprite fields need structured native sheet/ID storage outside Mono metadata and the 16-byte primitive buffer; Data serializes as `{Sheet: ..., RegionID: ...}`. Extend reflection/defaults/reload/marshalling following Prefab lifetime rules; copies preserve values. Reuse the component picker/drop payload for fields. Expose no editing, names, animation, samplers or general asset bindings.

Extend canonical `scripts/internal/packaging.py` and retain `project_files`/all-eligible-assets shipping. Validate every included sheet and every canonical/legacy component or typed Sprite field in included scenes/prefabs: `sheet → texture`, plus referenced region existence. Shipping all eligible assets supports runtime-computed references; bad IDs still fail assignment. No reachability pruning is added.

Add a small SDK-only Premake **SpriteAssetAudit** executable using the same new native parsers/image validators. Packaging supplies asset root and exact inventory; the adapter returns dependencies/location-qualified diagnostics in YAML. It reads references without constructing Scene, Texture2D, graphics, Mono or editor objects. Python checks shipping membership and uses canonical copying, avoiding duplicate sheet validators. The adapter is tooling only; ordinary sprite authoring invokes native services directly.

Validate decoding/dimensions/bounds/IDs/source alternatives/versions/path containment. Preserve relative structure and checksum sheet metadata/textures. Revalidate staged copies before archive publication against copy-time changes. Errors identify scene/prefab/entity/field/sheet/region. Retain assembly/native/license checks and texture-adjacent notices through current inventory behavior.

Nutella resolves relocated assets from its package root with precompiled scripts: no checkout, editor cache, audit executable, compiler, Python or new asset host paths.

## 7. Complete ImGui authoring experience

### Navigation and document policy

Content Browser adds **Import Texture…** on the current folder and **Create Sprite Sheet…** on a texture context menu. Existing native dialogs select source/destination; default destination is the browsed folder with an editable filename. Validate/copy atomically without modifying the source. Collision offers Choose Another Name / Cancel. Owned textures need no copy. Create shows dimensions/new path/Create and Open, with inline extension/containment errors.

Click a sheet to open dockable **Sprite Sheet**, with stable window identity, path/status, Save/Reload/Close and Controls popover. Support **one active sheet**, alongside scene/Prefab Inspector. Switching/closing/hiding a dirty sheet prompts Save/Discard/Cancel; reopening it preserves state. Project changes/exit also guard it. No tabbed document framework.

Texture/Regions/Grid/Preview sections contain a resizable list, region properties (read-only ID/name/integer bounds/pivot), canvas/handles and origin-axis placement preview using renderer geometry. Texture shows the path/dimensions, **Choose Texture… / Import Texture…**, and a **Filter: Nearest / Linear** combo; changes affect the draft until Save and revalidate dimensions. Preview Scale is labelled temporary. Entity Properties/selection remain independent. **Edit Sheet** from component/field opens the referenced region, including missing IDs.

At narrow widths stack/scroll children and collapse Grid/Preview; keep status/Save accessible. Use logical coordinates and existing UI scale/layout, including detached windows/Windows DPI. Handles have minimum logical hit sizes. Draw visible overlays only, with selected/hovered labels at low zoom, contrasting outlines and transparency checkerboard.

| State category | Contents |
| --- | --- |
| Saved sheet properties | Texture path/validated dimensions, filter, region IDs/names/rectangles/pivots, retired IDs and schema version. |
| Saved user preferences/layout | Existing global UI scale and window docking/layout. No new preference schema is required initially. |
| Temporary document/panel state | Draft and saved fingerprint, selected region, zoom/pan, tool, grid recipe and candidates, overlay/checkerboard toggles, preview scale. None enters game metadata. |

Component picker offers **Color / Whole Texture / Sheet Region**, path/name/thumbnail/errors, searchable project lists, Browse and drops. Dropping a sheet opens a region chooser. Region-list dragging uses validated UTF-8 `SPRITE_REGION` with sheet/hex ID, never pointers/indices/GL IDs. Resolve before changing component/field values; Clear/replace is explicit. Prefabs reuse it. No viewport entity-creation gesture is added.

### Discoverable controls

| Canvas action | Control |
| --- | --- |
| Zoom about pointer; pan | Mouse wheel while canvas hovered; middle-button drag, or the visible Pan tool with left drag. Toolbar includes −, +, 1:1 and Fit. |
| Select; move/resize | Select tool: left click rectangle/list; drag selected body or corner/edge handle. Overlaps follow current selection, otherwise smallest containing rectangle; list is authoritative. |
| New manual rectangle | Rectangle tool: left drag empty/start area; outline during drag, commit only positive in-bounds snapped rectangle. Esc cancels gesture. |
| Pivot | Pivot tool: left click/drag selected region's marker; numeric normalized/pixel readout plus center/corner/edge presets. |
| Save/delete | Ctrl+S saves the focused sheet, outside text entry; toolbar Save always available. Delete acts on selected region only with panel focus and no text input, via the reference-aware confirmation below. |

Rectangles snap to pixel edges; pivots can be fractional. Snapshot gestures for Esc cancellation, clamp move/resize to bounds, retain valid values on bad numeric input, and capture drags through release outside the child. Focus routing prevents viewport/camera/gizmo conflicts. Buttons/tooltips/Controls expose gestures and numeric alternatives; no hidden space/Alt controls.

### A. Import, grid-slice, name, save, assign

1. In Content Browser, Import Texture… selects a supported PNG/JPEG/BMP/TGA through the native dialog, chooses a new project destination, and copies it. Alternatively select an existing texture. Right-click it → Create Sprite Sheet…, choose `.hsprites` path → Create and Open.
2. Canvas starts at Fit with texture dimensions and checkerboard. Expand Grid: enter cell width/height, left/top/right/bottom margins, horizontal/vertical spacing and optional column/row counts. Zero counts mean derive; values must fit the usable image area. Nonnegative margins/spacing and positive cells are required.
3. For usable width `U=W-left-right`, derive columns as `floor((U+spacingX)/(cellWidth+spacingX))` when `U>=cellWidth`, similarly rows. Explicit counts select the leading rows/columns and cannot exceed the full-cell capacity. **Skip incomplete edge cells** is the fixed initial policy; show uncovered edge pixels and skipped-cell count. No partial/trimmed cells are generated.
4. Enter a naming prefix and start index. Generate names in top-to-bottom, left-to-right order (`hero_000`, `hero_001`, …), adding displayed suffixes for existing name collisions. Preview Grid shows dashed candidates and a count; it does not mutate the draft or allocate IDs.
5. **Add Regions** assigns IDs and appends candidates, skipping exact rectangles already present and preserving their IDs, labels and pivots. Preview shows those skips. Overlapping nonidentical rectangles warn. There is no Replace All or positional/name-based ID remapping; changing the recipe then adding cannot destroy old identities. Select rows and edit names individually; center pivots are defaults.
6. Save validates/writes and clears the dirty badge. Select an entity → Properties → Sprite Renderer → Sheet Region; drop the sheet and choose a named region, or drag that region from the panel onto the source. The component stores its ID; adjust existing Transform Scale if desired. Save the scene, or use the same control and Save Prefab in the detached inspector.

### B. Author an irregular sheet

1. Create/open a sheet from the texture. Choose Rectangle; zoom with the wheel/buttons and pan with middle drag/Pan tool.
2. Drag each source rectangle on pixel edges. A valid release adds a new ID and default name/pivot; invalid/zero-area releases show why and add nothing.
3. Switch to Select, click the list or canvas, then move the body or resize handles. Integer X/Y/Width/Height fields offer precise correction; selected overlays and a zoomed placement preview remain readable.
4. Name regions, set pivots as needed, Save, then assign using the same picker/drop as A. No packing, transparency trimming or rotated rectangles are implied.

### C. Adjust and preview a pivot

1. Select the region; open Preview. Compare centered sprite and origin axes at temporary scale, with normalized and pixel pivot readouts.
2. Choose Pivot tool or a preset, or edit numeric values. Preview immediately uses draft corners; entity transforms/colliders remain untouched. The panel displays “Saving changes placement of every use of this region.”
3. Save to update committed scene rendering. In Properties inspect Translation/Scale; enable the existing Show Physics Colliders control and adjust collider Offset/Size intentionally if needed. Play uses the same geometry and origin, and Stop returns to authored values.

### D. Edit a referenced region safely

1. Select the entity/prefab's sprite and click Edit Sheet. The matching ID is selected and current uses are identified.
2. Move/resize or edit numeric bounds. Only the sheet draft changes; scene/prefab render their saved region until Save. Component paths/IDs, transforms, script defaults and entity selections are preserved.
3. Save updates service revisions and visible sprites at the next safe frame boundary; scenes/prefabs gain no dirty flag from an asset change. Invalid edits/save failures retain the draft and previous committed definition. Editing and Save are disabled in Play/Simulate; Stop restores authoring access.

### E. Rename/delete with references

1. Rename the selected region in its Name field. Show the unchanged ID; Save changes display labels without rewriting references. Copy Sprite Reference generates an ID-based C# constructor expression for code authors.
2. Delete opens a confirmation listing known uses: current unsaved scene/prefab plus a native on-disk scan of scene/prefab components and typed Sprite fields. Show precise locations and scan failures; counts are incomplete if files cannot parse, and hard-coded/script-computed references cannot be proven absent.
3. Cancel preserves the draft. Delete retires the ID in the draft; Save commits it. Known uses then show **Missing Region [ID]**, and Play/export fail until repaired. They remain serializable/recoverable. No automatic clear, replacement or matching-name fallback occurs.
4. Open each affected scene/prefab, using Open for Repair if necessary, select the broken component/field and explicitly choose a replacement or Clear, then save. A new region with the old name does not heal those links. Restoring a backed-up sheet is the way to restore the original ID without reassigning.

### F. Package and run

1. Save sheet edits and scene/prefab assignments. Project → Export Game… uses the current host target, existing readiness/output controls and Save/Discard/Cancel guard, now including the active sheet draft. Invalid saves keep export pending.
2. Export performs canonical Release builds and native sprite/dependency validation. Output lists malformed/missing references by source location; select/open the affected asset and repair it before retrying. Discard publishes the last saved definitions, never the unsaved panel preview.
3. On success use Open Output Folder, extract the game to another location, and launch Nutella. The sheet and texture resolve there; the static sprite and pivot match Hazelnut Play. Closing either application releases resources normally.

### G. Recover missing texture or malformed metadata

1. Open/select the sheet. For a missing texture retain region data, list and last validated dimensions, with a clear error and disabled normal Save. **Locate Texture…** offers a project texture picker or Import Texture…; no external path is silently embedded.
2. Preview the replacement. If dimensions differ, Accept New Dimensions only updates the recorded dimensions after bounds revalidation; repair invalid rectangles explicitly. Save and then Reload affected references. ID and label data survive the repair.
3. A malformed/unsupported sheet displays filename, version/parser diagnostics, Reload and Open Containing Folder/Open File Externally; do not build an editable default sheet over its bytes. Restore/fix the file and Reload. If a valid unsaved draft already exists, retain it and use the external-change conflict choice before replacing the disk file.
4. If the broken sprite prevents strict scene/project/prefab opening, the failure UI offers **Open for Repair** for that candidate. Only valid scene/component data is adopted, with conspicuous missing markers and Inspector path/ID errors. An ordinary failed Open still retains the current session. Repair the reference, save, and retry Play/export; missing assembly or malformed scene structure remains a separate load error.

### Dirty/save integration

The document owns saved fingerprint and draft dirty state, independent of scene text comparisons. Extend `AuthoringPanel::Guard` with a bounded sheet participant, not a general document bus. Guards apply to switching/closing the sheet, project/root changes, exit, export and Play dependency preparation. Play prompts for dirty sheets only and still supports today's unsaved authored scene copy. Save failure prevents the pending operation; Discard resets the applicable draft; Cancel leaves all state intact. Global/project guards list dirty scene/prefab/sheet items; sequential saves are individually atomic, not an advertised multi-file transaction. Focused Ctrl+S saves the sheet; otherwise preserve current scene shortcut behavior. Prefab retains its visible Save button.

## 8. Exact likely file changes

These are proposed paths, not work performed by this audit.

| Files | Purpose |
| --- | --- |
| **New** `Hazel/src/Hazel/Assets/SpriteReference.h`, `SpriteSheet.h/.cpp`, `SpriteSheetSerializer.h/.cpp` | Source variants, definitions/IDs, pure geometry/grid/validation, shared YAML source/sheet parsing and dependency discovery. |
| **New** `Hazel/src/Hazel/Assets/ProjectAssets.h/.cpp` | Typed project caches, resolution/status/revision/invalidation. |
| **New** `Hazel/src/Hazel/Core/ImageReader.h/.cpp` | Native CPU decode/info for editor/runtime/audit, explicit orientation and two-channel conversion. Reuse pinned stb; no new dependency. |
| `Hazel/src/Hazel/Core/FileSystem.h/.cpp`, `Platform/Linux/LinuxFileSystem.cpp`, `Platform/Windows/WindowsFileSystem.cpp` | Exclusive atomic create/publication option and binary import reuse. |
| `Hazel/src/Hazel/Project/Project.h/.cpp` | Explicit service/root ownership, reusable strict owned-path resolution, policy-aware scene loading. Preserve legacy external paths. |
| `Hazel/src/Hazel/Renderer/Texture.h/.cpp`, `Platform/OpenGL/OpenGLTexture.h/.cpp` | Backend-neutral file sampling options and shared decode/upload orientation. Existing API defaults preserved. |
| **New** `Hazel/src/Hazel/Renderer/ResolvedSprite.h`; `Renderer2D.h/.cpp` | Region UV/local-corner submission, color/texture compatibility, existing batch and entity IDs. Current quad shader already supports the needed UVs; no atlas repeat shader is required. |
| `Hazel/src/Hazel/Scene/Components.h`, `Scene.h/.cpp`, `SceneSerializer.h/.cpp`, `Prefab.h/.cpp`, `RuntimeSession.h/.cpp` | Canonical source, reset caches on copy, both rendering paths, strict/recovery loads, source persistence and dependency checks before runtime mutation. |
| `Hazel/src/Hazel/Scripting/ScriptField.h`, `ScriptEngine.h/.cpp`, `ScriptGlue.cpp`; **new** `Hazel-ScriptCore/Source/Hazel/Scene/Sprite.cs`; `Scene/Components.cs`, `InternalCalls.cs` | Typed Sprite fields, defaults/reload/marshalling and checked managed selection. |
| **New** `Hazelnut/src/Authoring/SpriteSheetDocument.h/.cpp`, `Panels/SpriteSheetPanel.h/.cpp`, `Panels/SpriteSourceWidget.h/.cpp` | Native draft/import/save/conflict services, ImGui panel and reusable selectors. All model/validation code stays outside ImGui. |
| `Hazelnut/src/Panels/ContentBrowserPanel.h/.cpp`, `ContentBrowserPayload.h`, `Panels/SceneHierarchyPanel.h/.cpp` | Create/import/open callbacks, portable region payload, entity/prefab/script source pickers and error presentation. |
| `Hazelnut/src/EditorLayer.h/.cpp`, `Authoring/AuthoringPanel.h/.cpp` | Panel/service lifetimes, focus/shortcut routing, missing markers/selection geometry, repair-open and expanded dirty guards. |
| `Nutella/src/NutellaApp.cpp` | Explicit project service lifetime and strict dependency preparation/release. |
| **New** `scripts/internal/native/SpriteAssetAudit.cpp`; `premake5.lua`, `scripts/hazel.py`, `scripts/internal/packaging.py` | Small native audit target/invocation and canonical package dependency/staged-inventory validation. C++ TUs honor existing PCH order; Premake is the only build generator. |
| `scripts/internal/authoring.py` | Add conventional Sprites folder to new-project creation; no authored sheet/animation example forced into starter projects. `templates/project.lua` requires no semantic change. |
| **New** small `tests/fixtures/SpriteSheets/` fixture set and CPU `tests/migration/SpriteSheetSmoke.cpp`; `tests/migration/premake5.lua`, `Renderer2DSmoke.cpp`, `SceneGPUSmoke.cpp`, `EditorSmoke.cpp`, `MonoSmoke.cpp`, `ManagedFixture.cs`; `scripts/internal/tests.py`, `package_tests.py` | Focused service, integration and short package checks. Update existing whole-texture source API use; reuse render/Mono contexts. |
| `.github/workflows/c-cpp.yml` if necessary | Invoke focused sprite checks within Linux/Windows builds; do not add full-game or pixel-click walkthroughs. |

No vendor, dependency pin, CMake project, shipped user layout, `.vscode`, example-game source migration or renderer baseline upgrade is required. Prefer the tiny intentional fixture over converting MeadowRun/Skybound wholesale.

## 9. Dependency-ordered implementation stages

1. **Contracts and native persistence.** Implement source/definition/schema/identity, strict paths, image reader/orientation, rectangle/pivot/grid functions and exclusive atomic create. Exit: CPU service checks, format example and legacy parse fixtures pass; native document services can create/read/save without ImGui or a graphics context.
2. **Static rendering and resources.** Add file sampling options, resolved quad submission, typed caches and revision lifecycle. Exit: asymmetric regions and pivots render/pick correctly in short editor/runtime smokes; whole-texture orientation/tiling and batch limits remain intact on 4.1.
3. **Authored/runtime references.** Integrate scenes, detached prefabs, both render paths, strict/recovery policies, minimal managed Sprite fields/selection and runtime preparation. Exit: copy/duplicate/instantiate/reload/Stop preserve authored values and reject invalid runtime changes transactionally.
4. **Complete editor workflows.** Deliver native import/create, one sheet document/panel, grid/manual/pivot tools, shared selectors/drag payload, dirty/conflict/error guards and all A–G recovery/navigation paths. Exit: human acceptance of save/assign/edit/rename/delete, including small-screen and DPI behavior; no asset rules hidden in ImGui callbacks.
5. **Export and acceptance.** Connect the native audit adapter to canonical packaging, add the Sprites starter folder and short extracted-package regression. Exit: sheet/texture validation without editor load, relocation/source-free Nutella, Linux/Windows Debug/Release builds and short 4.1/native HD4000 smoke evidence, plus final human visual acceptance.

Use one bounded milestone tracked through these reviewable stages. Do not land a panel-only “finished” task with runtime/export work deferred; those are part of the first milestone. No branch integration is authorized by this document.

## 10. Verification and manual acceptance

Tests should assert externally meaningful contracts, not reimplement the conversion algorithms. Use one small asymmetric texture with distinct labelled corners, a one-pixel strip, transparent edges and adjacent contrasting regions; include an irregular region with a noncentral pivot. CPU checks do not start Application/Mono/OpenGL.

| Coverage | Minimal meaningful evidence |
| --- | --- |
| Rectangles/origin/UVs | Known top/bottom corner results, one-pixel regions, reversed/snap drags, offsets/spacing/derived grid counts, incomplete edges and overflow/zero/out-of-bounds rejection. |
| Pivots | Known world corners for center and bottom-center under translation/nonuniform scale/rotation; transform unchanged. One short rendered displacement/picking check uses existing framebuffer machinery. |
| Stable identity | Rename/order/save preserve IDs; delete retires ID; recreate and grid append cannot heal or overwrite prior identities. References still resolve after rename and fail after committed deletion. |
| Metadata/safe persistence | Version/type/duplicate/path/dimension failures; deterministic round-trip; partial writer/publication failure preserves previous bytes/draft; CreateNew collision preserves destination. Reuse current platform safe-write tests. |
| Scene/prefab compatibility | Legacy whole-texture/color fixtures, Windows separators/external semantics; canonical source round-trip; reject mixed source fields; copy/duplicate/prefab instances preserve region refs and independent fields. Strict failure preserves old scene; explicit repair retains source and blocks Play until fixed. |
| Managed selection | Authored Sprite field marshalling/reload, missing ID setter preserves old source, valid switch changes only runtime copy, null clears, Stop restores authored selection. |
| Cache refresh | Shared texture for same file/options, distinct sampling resource for different options, no repeated loads on unchanged preparation, revisions refresh all users without dirtying scenes, failed reload remains visibly broken, new root cannot reuse old paths. |
| Rendering compatibility | Correct orientation/UVs, neighbor bleed bounds, center and noncentral pivots, tint, entity ID, multiple regions sharing a slot, slot/quad flush, retained whole-texture tiling. Extend the current short Renderer2D smoke. |
| Packaging | Invoke audit/packager on never-editor-loaded saved fixtures; sheet/texture and component/field closure, missing/malformed/excluded/escaping dependencies, staged validation, extracted relocation with unrelated cwd and source/SDK/cache unavailable. One static startup/display/shutdown per host, not game completion. |

Run appropriate existing logical regressions and Linux/Windows Debug/Release Premake builds with current bounded job counts. Use short OpenGL 4.1 baseline and native Intel HD4000/OpenGL 4.2 rendering/startup/shutdown checks; Windows hardware visual/DPI acceptance remains human. No automated game-playing programs, fragile mouse-coordinate scripts or long server-driven editor tours are added. This audit ran no feature tests/builds and makes no new hardware verification claim.

Human checklist:

- Import/cancel/collision paths work with spaces/Unicode; grid candidates and incomplete-edge counts match the image; Add never destroys existing IDs.
- Manual create/select/move/resize/numeric correction, zoom/pan and controls are usable; overlays/handles remain readable at low zoom and high DPI.
- Name/pivot edits, preview placement, Save/Discard/Cancel and external conflicts work; center preserves whole-texture placement and colliders remain independent.
- Entity, detached prefab and Sprite script field select the same region through picker/drop; scene outline/picking follows displaced geometry in Edit and Play.
- Rename keeps all authored uses; delete leaves visible recoverable errors; missing texture and malformed metadata can be repaired without overwriting unrelated content.
- Switch sheets, dock/undock, close/exit, change projects and export without bypassing dirty guards; use a small window and Windows 125–150% DPI/monitor transition.
- Export and relocate the package, visually compare with Play, then close Hazelnut/Nutella cleanly. Check existing tiled grass/whole-texture content for compatibility.

## 11. Risks, decisions and deferrals

Main risks are accidental coordinate double-flipping, sampler leakage through shared textures, stale resolutions after edits, incomplete serializer/Mono switch coverage, and guards treating asset edits as scene mutations. Address them through the narrow data/resource boundary and focused tests above. Linear inset/no-mip sampling trades distant-image quality for bounded atlas isolation. No pixels-per-unit means irregular sprites need deliberate entity Scale; this is explicit and avoids changing existing transform meaning. Fingerprints detect ordinary external edits, not a hostile/concurrent writer race between check and replace. Reference scans cannot prove arbitrary C# code has no uses.

Decisions adopted here: path-scoped sheets, stable numeric region IDs, YAML `.hsprites` v1, top-left integer rectangles, normalized bounded pivots, Nearest default/Linear optional without mips or repetition, one active sheet document, explicit repair loading, minimal typed scripting, and canonical packaging backed by native validation. No architecture-changing user preference is required before implementation. Naming/UI text can be refined during human acceptance without changing the contract. If automatic sheet-file moves or pixels-per-unit are desired, revisit those requirements before implementation because they change reference/placement policy.

Explicit deferrals:

- **Animation clips/controllers:** next candidate milestone; references already support stable frames, but no time/playback/clip UI or speculative bindings now.
- **Automatic image packing, polygon trimming, rotated packed regions, multi-resolution atlases:** need new image generation/geometry/sampling contracts. Existing MSDF atlas generation is font-specific and is not a free general sprite packer.
- **Nine-slicing and tile-map editor:** require different geometry/placement and authoring models; existing whole-texture tiling remains available.
- **General asset database/import framework/global handles:** unnecessary while sheets are project-path assets; reconsider only for automatic asset moves/dependency rewriting at larger scale.
- **Undo/redo framework:** gesture cancellation and Discard/Reload are bounded recovery; no editor-wide command system.
- **Pixel-to-world scaling, out-of-bounds pivots, alpha-aware picking, per-region mip chains, seamless atlas repetition, live edits during Play, multiple sheet tabs:** explicit later work, not implied capabilities of this milestone.
- **macOS/Metal:** keep asset/renderer contracts backend-neutral and preserve Scope/Ref and pinned Premake vendors; implement neither platform/backend now.

Sprite tooling therefore fits as Hazel asset services and renderer support, surfaced through Hazelnut authoring and consumed identically by Nutella. The first milestone is the complete static import/slice/pivot/select/save/package workflow. Its prerequisites are narrow corrections to file texture options/orientation, owned paths and exclusive atomic creation, plus authored references independent of GPU objects. Implement it in several dependency-ordered stages under one bounded milestone; animation follows separately.
