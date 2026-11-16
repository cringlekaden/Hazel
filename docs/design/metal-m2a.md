# Metal 4 M2-A: bounded shader qualification

**M2-A PASS:** Debug/Release shader translation, Apple compiler/native Metal 4
pipelines, binding/layout checks, deterministic readback, focused OpenGL
regressions and M1 presentation/lifetime regressions all pass. This qualifies the
bounded shader checkpoint only; OpenGL remains the production default.

Scope: GLSL translation, reflection and a minimal offscreen native Metal 4 pipeline
proof only. OpenGL remains the production default. No Renderer2D/3D port, material
model changes, ImGui integration, scene pass redesign, M2-B or M3 is authorized.

Baseline: `feature/3d-foundations`, HEAD
`79ee83e1d8dd2ecac7e8fdb48f43073e79eeb330`, synchronized with origin after fetch.
M1 is its parent `9030dd62b0e486a369d39de35fb24d43389b022d`.
Four unrelated untracked `.DS_Store` files were present and are preserved.

## Acceptance gates (declared before native execution)

- All nine current raster shaders, vertex and fragment, plus both 16/32-slot quad
  variants: shaderc Vulkan 1.2 performance optimization with debug names, SPIR-V
  validation, generated MSL 3.0, installed Apple offline Metal compilation/link,
  native MTL4 compiler graphics pipeline creation. No shader warning ignored.
- Stage-local resource namespaces: set 0, UBO slots 0..7, sampled-image/sampler
  slots matching GLSL binding and array extent; vertex stream reserved at buffer
  16. Reject collisions, unbounded arrays and unsupported resource classes.
  Validate reflected uniform sizes/member offsets/matrix strides against CPU ABI;
  inspect and validate native pipeline reflection as well. The 32-slot fragment
  variant may use tier-2 descriptor-buffer indirection to preserve its full extent.
- Deterministic offscreen draw using generated authoritative Renderer2D_Line
  stages, M1's device/MTL4 queue, explicit residency and ordered GPU completion.
  Test identity/orthographic and perspective projection, winding/back culling,
  near-plane clipping, depth ordering and readback (absolute depth error <1e-6),
  RGBA32Float HDR output (exact 2, .25, .5, 1), R32Sint output (-123456789) and
  untouched clear (-1). Test stable interiors/exteriors independently of edges.
- Debug and Release focused runs with Metal API/shader validation, no new
  first-party compiler warnings; relevant OpenGL shader/renderer regression tests.
  M1 device/presentation/lifetime regression smoke. Unavailable Windows/Linux and
  physical Intel GPU coverage must be identified honestly.

## Inputs and adaptation boundaries

The existing pinned shaderc/SPIRV-Tools/SPIRV-Cross revisions in
`scripts/dependencies/shader-tools.json` are unchanged. Premake builds/installs
`spirv-cross-msl` from that same Cross revision. Generated MSL is an artifact,
never an independently maintained shader. Qualification uses separate fresh
artifact directories, not the production OpenGL cache. Each run retains source,
SPIR-V, MSL, AIR, metallib, reflection, compiler logs and SHA-256 identities.
The production cache stays unchanged: its existing version/source/stage identity,
container checksums, corruption rejection and SPIR-V validation remain authoritative
for OpenGL. This checkpoint does not ship a Metal runtime cache or export format.

Eight shaders already use the Vulkan pipeline. EntityOutline is GLSL 410 with
loose uniforms and gl_VertexID. Its qualification-only declaration adaptation
raises the version, names Vulkan's gl_VertexIndex builtin and lifts selected,
radius and encoded color into std140 Outline at buffer 7; its sampler is slot 0.
No function-body equations are rewritten. Production EntityOutline remains GLSL
410 and retains its existing OpenGL uniform upload path. Integrating this adapter
into a production Metal asset/compiler path is a subsequent dependency.

MSL generation explicitly remaps each stage's GLSL binding into distinct Metal
buffer, texture and sampler namespaces. All stages except the 32-slot quad
fragment use discrete parameters supplied by native MTL4 argument tables. Apple
rejects the 32-slot discrete sampler array. That fragment instead uses a tier-2
argument buffer at fragment buffer 0: texture IDs 0..31, sampler IDs 32..63.
Native reflection validates 512 bytes, 8-byte alignment, member offsets 0/256,
32-element arrays and argument-index stride 1. The opaque arrays report byte
stride 0 in native reflection; the GPU execution proof validates the 8-byte
`MTL::ResourceID` CPU encoding by sampling slot 31, with black decoys in slots
0..30. Vertex buffer 0 still independently supplies Camera; stage-local tables
prevent aliasing. No classic Metal command submission is used.
Clip fixup maps z from [-w,w] to [0,w]; vertex Y is inverted. Readback coordinates,
framebuffer-dependent gl_FragCoord/UV/derivative signs and future CPU clip rectangles
must be interpreted consistently; qualification is not scene-level parity.
The proof preserves numeric GL framebuffer coordinates in offscreen rows:
NDC (-.75,-.75) maps near readback (4,4), with +Y increasing row index. Metal's
physical top-left origin means final display composition must account for this
choice once. With generated Y inversion the tested front winding is clockwise.
No projection matrix or shader lighting equation is changed. Existing MSL
`dfdy`, implicit sampling, explicit LOD, integer fetch and discard operations
compile; textured UV patterns, normal/tangent derivative parity, sRGB display,
cubemap seams and scene/PBR/IBL numerical/visual execution remain M2 dependencies.
The accepted IBL model and all nine source shader bytes remain unchanged.

## Reproduction

From the canonical checkout, on native arm64 macOS 27+ with Metal 4 hardware and
the optional Apple Metal Toolchain installed:

```sh
python3 scripts/macos/metal_m2a.py --config Debug --build --output build/metal-m2a-evidence/debug-final
python3 scripts/macos/metal_m2a.py --config Release --build --output build/metal-m2a-evidence/release-final
```

Use a fresh output directory. The runner uses the established pinned producer,
Premake and Xcode selection. Raw evidence belongs in the ignored build directory.
Results, limitations and actual regression commands are recorded below after
execution. No scene/PBR numerical parity or presentation-performance claim follows
from an offscreen shader checkpoint.

## Validated layout contract

| GLSL block | Buffer slot | Bytes | CPU/native/SPIR-V member offsets |
|---|---:|---:|---|
| Camera | 0, vertex | 64 | 0 |
| View | 2 | 128 | 0,64,80,96,112 |
| Object | 3 | 144 | 0,64,128 |
| Material | 4, fragment | 64 | 0,16,32,48 |
| Presentation | 5, fragment | 16 | 0 |
| Environment | 6 | 96 | 0,64,80 |
| Outline (declaration adaptation) | 7, fragment | 32 | 0,4,16 |

Matrices are column-major with 16-byte column stride. Existing Renderer3D CPU
struct definitions were moved unchanged into `Renderer3DUniforms.h` so tests use
the same types and `offsetof` as uploads, rather than a second material layout.
The packed vertex stream is buffer 16: PBR 64, Circle 68, Line 48, Quad 64,
Text 56 bytes. Native vertex layouts compile with reflected float/int attributes;
Line and Quad offsets/strides also have CPU static assertions and GPU execution.
Sampled resources retain 2D/cube/signed-int types and GLSL extents; PBR uses
textures/samplers 0..7, including cube slots 5/6, without changing sampling math.
There are no cube-array inputs in this corpus; cube-array runtime support is not
claimed. There are no UBO arrays; matrix strides are checked explicitly. The
harness rejects resource classes outside this contract.

## Experimental findings retained

- `debug-initial`: launcher failed before shader execution because Mono's dylib
  search path was missing; runner now uses the existing producer SDK's lib path.
- `debug-translation`: Cross's `enable_decoration_binding` shortcut does not set
  its automatic-binding query metadata. Explicit `MSLResourceBinding` mappings
  now supply and validate both indices.
- `debug-native-first`: all 20 stages translate, but Apple rejects Quad-32's
  discrete sampler array (`cannot reserve sampler resource locations at index 0`).
  Tier-2 generated MSL resolves this without reducing capacity or merging samplers.
- `debug-reflection`: native opaque-resource arrays report byte stride 0, not 8.
  Member offsets, count, argument-index stride and executed CPU resource handles
  establish the actual descriptor-buffer ABI instead of assuming scalar reflection.

These logs remain under `build/metal-m2a-evidence/`; they are experiments, not
passing acceptance runs. Final runs are `debug-final` and `release-final`.

## Final checkpoint evidence

Debug and Release each pass translation/validation, all 20 offline stage
compiles/links and all ten unique native MTL4 graphics pipeline combinations.
All nine original raster shader files remain byte-for-byte unchanged.
Each configuration executes three completed offscreen submissions: Line
orthographic, Line perspective and Quad-32 slot 31. Each independently checked
readback has 210 stable interior samples and 618 exterior/clear samples; exact
color and ID error is zero, maximum absolute depth error is zero (gate <1e-6).
Depths are .25/.5/.25, interior ID -123456789 and clear ID -1. Debug/Release
readback SHA-256 values match. Raw readbacks and the independent Python analysis
are retained at `build/metal-m2a-evidence/readback-analysis.json`.
API and GPU shader validation were enabled; there are zero diagnostics in these
exercised draws. This is offscreen execution, not drawable presentation evidence.

| Shader | Stages | GLSL/SPIR-V/MSL | Apple offline | Native MTL4 pipeline |
|---|---:|---|---|---|
| DisplayOutput | 2 | PASS | PASS | PASS |
| EntityOutline | 2 | PASS, explicit declaration/builtin adaptation | PASS | PASS |
| EnvironmentSky | 2 | PASS | PASS | PASS |
| ForwardPBR | 2 | PASS | PASS | PASS |
| Renderer2D_Circle | 2 | PASS | PASS | PASS |
| Renderer2D_Line | 2 | PASS | PASS | PASS + draw |
| Renderer2D_Quad | 4 (16/32 slots) | PASS | PASS, tier-2 for 32 | PASS + 32-slot draw |
| Renderer2D_Text | 2 | PASS | PASS | PASS |
| WorldPresentation | 2 | PASS | PASS | PASS |

The final toolchain reports Apple metal 32023.925 (metalfe-32023.925.6), target
`air64-apple-darwin27.3.0`; Xcode 27.3, macOS 27.3 SDK, Apple M4. MSL is 3.0
with `-fno-fast-math`, linked with the installed `metallib`; the native pipeline
compiler is `MTL4::Compiler`. MSL language version and Metal submission API
version are independent. The pinned Cross backend supports the required corpus
without a dependency revision upgrade.

### Warnings and source integrity

Focused final Debug build has zero warnings. Release's dependency/engine rebuild
emits existing warnings in untouched first-party/vendor code (deprecated implicit
copy assignments, unused scene-component parameters, UI aggregate initialization,
FreeType narrowing and archive symbol-table/empty-object warnings). No warnings
originate in changed source files or generated shaders. The new proof exposed
an existing macOS platform macro redefinition; its header now uses the same guard
as Windows/Linux, fixing the cause without suppression. Build logs retain the
full diagnostics; no global warning policy changed.

The C++ integrity check uses the shared CPU uniform header directly from the
translation test, with no PCH dependency. Apple headers/types remain confined to
macOS tests and existing backend sources. Premake exclusions preserve the
Windows/Linux engine path; the portable dependency producer also builds the
pinned MSL static library. Native Windows/Linux builds, physical Intel HD4000,
full scene/PBR visual parity, source-free Metal export/relocation and 2× display
qualification are NOT RUN in M2-A. They are not inferred from this offscreen proof.

### Public documentation checkpoint

The public-docs health pass follows README links to getting-started, development,
platform/dependency and packaging guidance. Existing production commands,
OpenGL defaults, platform prerequisites, screenshots and packaging behavior are
unchanged, so no public guide rewrite is needed. The optional Metal compiler is
a prerequisite only for this developer qualification command, documented above.
`python3 tools/agents/check_public_docs.py --root .` checks 66 Markdown files and
572 local links/images with zero findings. README master badges remain labeled
master; `.github/workflows/c-cpp.yml` runs on master pushes, PRs and dispatch.
No new CI run, clean-clone/platform requalification or workflow dispatch is
claimed for this checkpoint. No public documentation drift is deferred.

### Regression results

Both configurations build all six focused regression targets successfully and
pass five sequential native Apple OpenGL executables: ShaderToolsSmoke,
RendererSmoke, RendererFeaturesSmoke, Renderer2DSmoke and SceneGPUSmoke. Raw logs
are copied into `build/metal-m2a-evidence/opengl-logs/`. Other regression executables
were not rerun. Seven `tests/macos/sdk_workflow.py` checks pass.

Exact regression build uses the existing producer, invoked from the root:

```python
import sys
sys.path.insert(0, 'scripts')
import hazel
for config in ('Debug', 'Release'):
    hazel.build(config, tests=True, targets=[
        'MigrationShaderToolsSmoke', 'MigrationRendererSmoke',
        'MigrationRendererFeaturesSmoke', 'MigrationRenderer2DSmoke',
        'MigrationSceneGPUSmoke', 'MetalM1Smoke'])
```

```sh
python3 scripts/hazel.py test --config Debug --profile native --only ShaderToolsSmoke --only RendererSmoke --only RendererFeaturesSmoke --only Renderer2DSmoke --only SceneGPUSmoke
python3 scripts/hazel.py test --config Release --profile native --only ShaderToolsSmoke --only RendererSmoke --only RendererFeaturesSmoke --only Renderer2DSmoke --only SceneGPUSmoke
python3 tests/macos/sdk_workflow.py
python3 tools/agents/check_public_docs.py --root .
python3 scripts/macos/metal_m1.py --config Debug --output build/metal-m2a-evidence/m1-debug-unlocked
python3 scripts/macos/metal_m1.py --config Release --output build/metal-m2a-evidence/m1-release-unlocked
```

The M1 regression runner passes in both builds at native 1×. Debug completes and
verifies 340 frames; Release completes and verifies 338. Both independently
correlate 325 positive native presentation timestamps with completed/verified GPU
frames. Zero timestamps are excluded. Each run has zero errors, exercises three
in-flight slots and shutdown with three pending submissions, and ends with zero
in-flight work, owned bytes, allocator bytes and residency allocations. Surface
detachment and device release are confirmed. Resize/minimize/recovery and OpenGL
default restoration pass. This is fresh native presentation/lifetime evidence;
M2-A does not repeat M1's native 2× or capture campaign.

The Release rebuild has 565 warning occurrences, including 209 first-party
occurrences covering 22 message kinds. Every first-party message kind appears
in retained M1 build logs; changed-file warnings are zero. See
`build/metal-m2a-evidence/warnings.json` and producer logs for classification.

### Remaining scope and M2-B recommendation

M2-B should integrate the qualified
GLSL declaration adapter/reflection contract into production Metal shader assets
and cache identity, then begin a bounded Renderer2D geometry/texture/ID path.
Use the existing device's completion-gated resource versions and explicit
residency for tier-2 handles and their referenced textures. Establish final
composition orientation, CPU clip rectangles and nonconstant UV/derivative
fixtures before claiming visual parity. Keep one GLSL shading source, retain
OpenGL default behavior and defer full scene/PBR/IBL parity, ImGui and pass
redesign to separately authorized checkpoints. No M2-B implementation was done.

The machine-readable record lists every changed file, qualified implementation
source SHA-256, compiler/matrix, readback measurements, warning classification
and raw evidence locations in [metal-m2a-results.json](metal-m2a-results.json).

All predeclared M2-A gates pass. The qualified implementation hashes in the JSON
record identify the tested source tree before its scoped checkpoint commit; the
only subsequent changes are these qualification records. No merge, backend-default
switch or M2-B advancement is authorized or performed.
