# Stage 2: native static asset import

Implementation continues on `feature/3d-foundations`; stage-3 drawing is excluded.
This record is in progress until editor/package and Debug/Release qualification
are recorded below. The frozen Rendering Reference Core_v1 is unchanged.

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

## Current evidence

- Content-only packaging repair: actual relocated Reference Nutella, source/SDK
  denied, 60 frames and normal teardown; assigned module/class, managed closure,
  corrupt/missing dependencies and accepted archive preservation checks pass.
- Apple Clang 21 dependency Debug/Release and ASan/UBSan corpus pass. Initial
  canonical MSVC admission exposed the engine wrapper's missing C `max_align_t`;
  the wrapper now explicitly preserves MSVC x64 malloc alignment. No vendor edit.
- Focused Release CPU tests exercise portable decoding, strict schemas, identities,
  reimport conflicts/cancellation/source changes, retained accepted versions,
  source-free runtime reads and explicit unsupported Play.
- Native editor interaction, additive reference fixtures, relocated imported-asset
  closure and final Debug/Release/CI qualification remain to be completed.
