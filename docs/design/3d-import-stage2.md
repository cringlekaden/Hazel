# Stage 2: native static asset import

Stage-2 code and native authoring are implemented on `feature/3d-foundations`.
Local Debug/Release and relocated-package qualification passed. Current canonical
CI status and remaining platform limits are recorded below. Core_v1 remains
byte-for-byte unchanged. Mesh drawing, PBR, IBL, Metal and other adapters are excluded.

## Boundaries and documents

`HazelImport` is an engine-owned producer module linked by `HazelProject`, not
Hazel or Nutella. The GLB adapter owns framing, semantic preflight and TinyGLTF;
it copies into `ImportedSceneData` and releases parser memory. Common validation,
Mikk cooking, stable mapping and publication consume owned values. Runtime CPU
codecs read `.hmesh`, `.hmat`, `.himport` and `.hgeom` without GLB/parser access.
[Dependency gate](import-dependencies.md) records exact source/notices and limits.

Geometry is mesh-local, right handed, +Y up, in metres. Nodes retain local signed
TRS; shears, singular transforms and nonfinite input reject. Images/UV0 use top-left
origin. Embedded 8-bit PNG/JPEG is decoded through the existing shared decoder;
cooking preserves encoded channel and alpha bytes in top-left RGBA TGA. Base-color
and emissive bindings interpret images as sRGB; metallic/roughness, normal and
occlusion bindings interpret linear data. Factors and vertex colors are linear;
alpha is unchanged. Sampler state belongs to each binding.

`.hgeom` version 1 encodes little-endian IEEE754 scalar fields, checked contiguous
offsets/counts/stride, mesh/slot IDs, local bounds and SHA256 payload integrity.
Original source copyright/attribution, when present, is bounded UTF-8
`SourceNotice` in the accepted manifest and visible in native import metadata;
it travels with cooked assets without the original source. Separate source
license files belong with the project asset notices.

No native structs, pointers or GL identities are serialized. Mesh slots have
independent persistent IDs. `.hmat` version 1 is a fixed typed metallic/roughness
or explicit unlit vocabulary with Opaque/Mask alpha. These are authored material
values, not a claim that lighting or mesh drawing exists.

`MeshRendererComponent.Version: 1` stores only a mesh reference and whole-material
slot replacements. Existing scene/prefab version 3 remains the canonical color
container; unknown component/asset versions reject. Copy, detached prefab and
exact authored/history paths include these references. Runtime readiness rejects
mesh components until stage 3; metadata authoring remains available.

## Acceptance and reimport

First import reserves a new destination and publishes its validated complete
folder exclusively. Later geometry generations are immutable; one atomic
`Import.himport` replacement is the acceptance point. Source bytes, document
conflicts and cancellation are checked before acceptance. The persistent native
lease serializes publication. Marked temporary stages live in excluded
`Assets/Intermediates/NativeImports`, and retry removes only owned stages.
A killed operation can leave unaccepted immutable output for inspection; it is
never selected by the accepted manifest. Previously accepted generation files
remain available. There is no automatic garbage collection or linked-prefab merge.

Source keys/indices are recorded as scoped provenance. Persistent IDs are assigned
independently. Reimport matches content evidence, reconciles unique material/mesh/
primitive/node mappings and rejects ambiguity/removal. Unique geometry changes
with unchanged material semantics/hierarchy can update existing mesh resources.
Source material or node-semantic changes conflict, with existing authored files
retained. Import into a new destination to inspect/reconcile those changes.
Existing edited materials, mesh defaults and detached prefabs are never overwritten.

Limits include 64 MiB source, 64 MiB JSON/arena budgets under a 128 MiB C allocation
cap, depth 64 JSON, 1 MiB strings and one million semantic visits. The subset caps
one million source/cooked vertices, three million indices, 1,024 meshes, 4,096
materials/mesh primitives and 9,999 selected nodes plus the native prefab root.
Owned DTOs cap at 128 MiB, corner/reindex storage at 192 MiB, Mikk allocations at
64 MiB, embedded decoded images together at 64 MiB and decoder allocations at
96 MiB (including PNG inflation). Working sets that exceed these bounds reject.

Triangles, supported float and normalized unsigned UV/color accessors, index
widths, strides, multiple primitives and repeated mesh instances are converted.
Missing normals become flat corner normals. Missing tangents use Mikk with
per-corner splitting/reindexing; normal-mapped degenerate UVs and zero-area
triangles reject. Skins, morphs, animation, sparse accessors, external URIs,
compression, extra UV sets, alpha Blend, cameras/lights and material extensions
other than explicitly translated `KHR_materials_unlit` reject. Optional extensions
cannot silently alter material meaning.

## Qualification

Implementation checkpoints: `32f0164` (content-only packaging), `7040fc8`/`aacce9c`
(pinned C dependencies/MSVC alignment), `ed17cc6` (owned pipeline/codecs/reference),
`49ddbad` (native authoring), `e579e9c` (attribution/retained metadata), `623b90b`
(Windows manifest/diagnostics) and `ffe3904` (completed-teardown observation).

- Apple M4, Apple OpenGL 4.1: maintained Debug and Release builds and **all 18**
  sequential regressions passed at `e579e9c`. Both configurations ran the actual
  Core_v1 orthographic/perspective player for 60 frames without a project module.
  Native editor tests exercised import with unavailable Python/SDK, actual ImGui
  material create/property/texture selection/save/duplicate, independent history,
  mesh-slot/transform history interleaving, detached instantiation/save/reopen,
  explicit unsupported Play, reimport conflict and retained metadata reload.
- Apple Clang 21 dependency corpus and full C++ CPU foundation pass ASan/UBSan,
  including stack-use-after-return detection. The CPU invocation first lacked the
  runner's staged font assets; supplying those assets let the same instrumented
  binary finish. LeakSanitizer is unavailable here; counted live C allocations
  are asserted. No skipped graphics check is counted as passed.
- Reimport checks preserve edited material, prefab bytes, mesh names/slot labels/
  default assignments and detached scene edits. Unique primitive reorders keep
  stable slots; ambiguous/removal conflicts, cancellation before/after immutable
  publication, source changes, failed destinations and retry retain acceptance.
- Relocated Hazelnut/Nutella pass checksums/signatures, native/managed closure,
  actual frames/normal teardown and rendering/picking/session regressions under
  source/SDK denial. The shipped native helper creates/imports/reimports without
  GPU initialization or project compilation. Content-only Reference packaging
  tests required module/class, managed closure, missing/corrupt geometry/images
  and accepted archive/checksum preservation.
- A separate native-created cooked-only package omits **all GLBs and its project
  assembly**. Its extracted parser-free native auditor validates mesh/material/
  prefab/artifact closure and retained source attribution under source/SDK denial;
  its ordinary 2D startup completes 60 frames. `nm` also confirms parser/tangent
  symbols are absent from Hazelnut, Nutella and the native runtime auditor.
- Import_v1 has eleven valid/rejected diagnostic sources, exact provenance and
  frozen native outputs. CLI import and source-free closure checks pass. All eight
  pre-stage-2 Reference assets compare unchanged with `ab033b7`.

Evidence is retained in ignored `build/stage2-evidence`, `build/testing/logs` and
`build/testing/packages`. The normal Reference archive is under
`build/stage2-evidence/packages/reference`; the cooked-only variant has separate
qualification metadata and deliberately cannot reimport without restoring source.

## CI and remaining limits

[Canonical run at `623b90b`](https://github.com/cringlekaden/Hazel/actions/runs/42158669043)
completed: Linux full qualification and hosted macOS CPU/service/managed checks
passed. Windows built Debug/Release and passed both extracted-archive profiles,
including relocated native helper import/reimport and content-only contracts.
The path/UTF-8 repair therefore ran successfully. Windows then failed before the
native Debug regressions because checkout CRLF conversion changed frozen
`Native/ProvidedTangents/Import.himport` bytes. The consolidation checkpoint adds
explicit byte-preservation attributes for accepted native assets and Reference.
This is separate from the historical gameplay timeout; that fixture was not reached.
No fixture hash, tolerance or timeout is changed.

The macOS product probe also observed an exit race after the 60-frame teardown
marker and earlier positive native mappings. It now accepts disappearance of
mappings only in that completed state; exit, frames, native closure and signature
checks remain mandatory. Relocated checks passed after this focused repair.

The historical Windows Debug game timeout remains unresolved; these Windows
attempts have not reached that fixture, so no recurrence or full Windows gameplay
qualification is claimed. Bounds, retries and reference conditions are unchanged.
Physical OS dialogs/desktop input remain separate acceptance: this session could
not attach to the private app's native window; real ImGui authoring was exercised
by the maintained native editor regression.

The original checkout's two LastLightkeeper drafts and two Finder files retain
their recorded hashes. Preferences/saves/layouts and original Xcode customizations
were not used for tests. Conflicting owner drafts remain unreconciled alongside
the stage-1 committed counterparts. No master merge or release was performed.
The next implementation stage is shared scene submission and direct PBR drawing,
followed by HDR/IBL; none was begun here.


Subsequent consolidation reconciled the owner drafts in 310a4d3 and returned
active development to the original Hazel checkout. The preservation statements
above describe the Stage-2 closeout, not a remaining reconciliation task. Current
checkout, direct-rendering progress and qualification are in
[the Stage-3 record](3d-direct-stage3.md).
