# Metal 4 M2-B1: production shader resources and cache

Scope: shader/compiler/artifact/library/pipeline ownership only. Baseline
`119678a00361e53c483d55a454a8886b64fcafa9` on `feature/3d-foundations`, synchronized
with origin; four unrelated untracked .DS_Store files are preserved. No rendering
parity, pass redesign, Renderer3D port, ImGui, effects or M2-B2 work is authorized.

Predeclared acceptance:

- Move M2-A translation and native reflection/pipeline construction into production
  Metal resources. M2-A calls these same owners; all nine shaders and both quad
  variants pass Debug/Release translation/offline/native qualification and readback.
- Immutable GLSL, unchanged OpenGL compiler/cache path. Production factory routes
  Metal to packaged libraries with explicit unsupported inline/uniform/draw errors.
- Version/profile/pins/source/variant/producer identities and SHA-256 checked,
  bounded artifact decoding. Atomic publication, deterministic repeated production,
  stale/corrupt producer output recovery, source changes and toolchain invalidation.
- Runtime packaged consumption without GLSL/MSL files, xcrun or developer compiler
  invocation; relocate through spaces/Unicode, repair truncated/wrong user-cache
  entries from validated package, reject bad packages, wrong stages, mismatched
  native layouts and invalid libraries with actionable diagnostics. Concurrent
  loads publish one fully validated resource; failed loads do not poison ownership.
- Preserve all M2-A binding/CPU ABI and tier-2 32-slot descriptor contracts. Keep
  shader/library/pipeline resources alive through exercised GPU completion and
  drain existing device work before backend cache teardown. No new submission path.
- Relevant native OpenGL regressions Debug/Release, C++ header/warning checks,
  public docs health pass and available portability checks. Unavailable native
  Windows/Linux/Intel GPU coverage is reported, not inferred.

The runtime cache is disposable and separate from OpenGL. The offline producer
can rebuild invalid packages from GLSL. Runtime repairs cache entries using the
validated shipped package; it never recompiles source or falls back to OpenGL.
A missing/stale/corrupt shipped package requires offline regeneration. This is
shader resource support, not end-user Metal rendering support.

## Production owners and artifact contract

- `MetalShaderCompiler.cpp` owns the moved M2-A GLSL stage extraction,
  declaration-only EntityOutline adaptation, shaderc Vulkan 1.2 optimization,
  SPIR-V validation/reflection and SPIRV-Cross MSL 3.0 generation. Qualification
  and `MetalShaderTools` call this one path. GLSL sources and OpenGL code are
  unchanged. The compiler rejects unsupported resources and vertex layouts.
- `MetalShaderArtifact` owns SHA-256 identities and the `HZMETAL1` container.
  The checked payload has length-prefixed profile, name, variant, source identity,
  Apple producer fingerprint, and vertex/fragment SPIR-V + metallib. Names and
  variants are bounded to the qualified corpus. Decode bounds every field and the
  complete file to 64 MiB, verifies SHA-256, schema/profile/pins, valid SPIR-V,
  stage/entry point, resource bounds and CPU uniform layouts. SPIR-V and metallib
  are runtime artifacts; GLSL/MSL files and producer executables are unnecessary.
  SPIR-V retains reflection names/debug information, so this is not an IP-stripping
  or shader obfuscation format.
- `MetalShader` owns immutable libraries, one native MTL4 pipeline and reflected
  ABI information, retaining its `MetalDevice`. The pipeline is the qualified
  single-sample RGBA32Float + optional R32Sint profile, packed vertex stream 16,
  M2-A clip/Y conventions and clockwise front-face expectation. Other target
  formats/sample counts and draw/uniform commands remain unavailable. Native
  library/pipeline failures include shader, variant and stage/context diagnostics.
- `MetalRendererAPI` owns a mutex-protected, bounded ten-variant in-memory resource
  cache. `Shader::Create(filepath)` routes Metal to the configured resource root
  by canonical engine GLSL resource path (which may be absent in a package);
  unrelated paths with the same basename are rejected; the default quad variant is 32. Explicit backend
  loading also supports 16. Inline GLSL, loose uniforms and implicit GL-style bind
  commands fail clearly. No capabilities/defaults or scene submission change.

The profile includes artifact/binding/translation policy version, pinned compiler
revision digest, Vulkan/MSL modes, conventions and macOS target floor. Source
identity hashes length-delimited profile/name/variant/canonical GLSL bytes. The
producer fingerprint adds Apple Metal version, SDK version and compile flags.
Full artifact SHA-256 is the runtime content key, including compiled library bytes.
Changes to translation/binding policy require a profile-version bump. Native GPU
pipeline state is per-device memory, never reused as a portable disk binary.

Offline `metal_shaders.py` builds packages using Premake's `MetalShaderTools`,
then installed `metal`/`metallib` with `-std=metal3.0 -fno-fast-math` and deployment
floor 27.0. Input basenames are stable, without checkout paths. A valid unchanged
artifact is a HIT and is not rewritten; changed source/profile/pins/producer or
corruption rebuilds it. `--force` explicitly regenerates checksum-valid packages
that fail native library loading, rather than trusting a prior producer HIT. The producer validates the translation's source key again
before packing. Temporary build output is private and final packages publish by
same-filesystem rename.

Runtime loads `Resources/shaders/metal/<variant>.hzmetal`. If GLSL is present,
its source key must match; a stale shipped artifact fails with the producer command.
Without GLSL, the shipped package is authoritative. User cache entries live at
`UserData/cache/shader/metal4/<artifact-sha256>.hzmetal`, independently of OpenGL's
unchanged cache. Invalid cache bytes are rejected and atomically reseeded from the
validated package, after native pipeline/reflection success. Unique temporary
files prevent independent cache writers from truncating one another. An unwritable
cache warns and uses the validated package. Invalid/missing shipped packages fail;
there is no runtime source compiler invocation, silent stale acceptance or OpenGL
fallback. Old content keys are ignored; disk garbage collection/hot reload are
not implemented in this slice.

## Binding and lifetime

All M2-A uniform sizes/member offsets/column strides are checked against the same
CPU definitions, then against native pipeline reflection. Vertex locations/types
and packed sizes are checked explicitly, including signed entity attributes.
Textures preserve 2D/cube and float/signed-int types and sampler extents. Resource
classes outside the qualified contract fail. The 32-slot fragment retains tier-2
buffer 0, texture IDs 0..31, sampler IDs 32..63, 512 bytes/alignment 8, offsets
0/256 and argument-index stride 1. Vertex Camera buffer 0 is stage-local; it does
not alias the fragment descriptor buffer.

Shaders submit no work. A command owner must retain the shader through GPU
completion, alongside its allocator, argument tables and referenced allocations.
M2-A's retained proof resources exercise that rule using M1's device/MTL4 queue,
explicit residency/barriers/shared events and completion-gated CPU readback.
The backend holds loaded resources for its lifetime, drains before clearing them,
and rejects new loads after shutdown. External shader references safely keep the
device alive; shared ownership is not treated as evidence of GPU completion.
No resource eviction, transient command ownership or new submission system was
introduced. The next drawing slice must retain shader and table resources in
completion-gated frame ownership before encoding production draws.

## Reproduction and scope

```sh
python3 scripts/macos/metal_m2b1.py --config Debug --build --output build/metal-m2b1-evidence/debug-final
python3 scripts/macos/metal_m2b1.py --config Release --build --output build/metal-m2b1-evidence/release-final
```

Fresh output directories are required. The runner qualifies all shaders through
the production owners, tests cold/warm/deterministic production and stale/corrupt
rebuilds, then copies only shader packages, the test executable and its runtime
Mono dylib dependency into a relocated path with spaces/Unicode. The child has
`PATH=/nonexistent`, `DEVELOPER_DIR=/nonexistent` and no shader sources or SDK tools
in the package. This qualifies source-free shader consumption, not a full game
export or end-user Metal renderer. Ordinary staging/export remain OpenGL and do
not automatically select or produce Metal packages.

The new developer producer command is documented in `docs/development.md`.
Windows/Linux continue excluding all macOS implementation files; no Apple types
enter portable Shader headers. Local Linux execution is unavailable because the
Docker daemon is absent; Windows and physical Intel GPU coverage are unavailable.
Native Metal pipeline/rendering evidence is from the Apple M4 host, not CI or
software GL. M2-A's scene/UV/derivative/color-space and final-composition limitations
remain; this checkpoint adds no rendering parity claim.

## Acceptance results — PASS

Debug and Release each pass all 20 offline raster-stage compilations, ten unique
native MTL4 pipeline combinations and the three retained M2-A GPU draw/readback
fixtures through the production owner. Each readback checks 210 stable interior
and 618 exterior samples; color/depth error is exactly zero, IDs are exactly
-123456789 and clear -1. There are three GPU-completed offscreen submissions per
configuration, not presented frames. API/shader validation produces no unexpected
diagnostics. Debug's final runtime checks were repeated after the constructor's
worker-thread autorelease pool and canonical factory-path guard were added;
readback bytes are unchanged. Translation/offline production was unchanged by
those ownership/factory fixes. Release runs the complete final campaign.

Both artifact campaigns pass cold production, byte-identical production in
separate directories, warm HIT/no rewrite, forced regeneration, truncated output
recovery, valid-checksum stale-profile recovery, producer-fingerprint invalidation,
source invalidation, unchanged-variant reuse and restoration to canonical source.
All ten artifact byte hashes also match across Debug and Release.

The final production-loading tests pass 17 expected rejections, including
truncated/trailing payloads, stale profile with repaired checksum, wrong stage,
SPIR-V corruption, wrong variant/name/path, inline source, corrupt package,
invalid library, native/SPIR-V vertex mismatch, unavailable draw/uniform calls,
stale source and post-shutdown load. Corrupt and wrong-content user cache entries
repair from the package; concurrent factory loads share one resource. No negative
case publishes a failed resource to the in-memory cache. Relocated source-free
execution passes with only `.hzmetal` resource files and runtime dylib dependency,
without the original GLSL/MSL files or developer tools. Packaged SPIR-V retains
reflection/debug metadata as described above.

Five focused OpenGL regressions pass in each configuration: ShaderToolsSmoke,
RendererSmoke, RendererFeaturesSmoke, Renderer2DSmoke and SceneGPUSmoke. Other
regressions were not rerun. Seven SDK workflow checks pass; direct inclusion of
the changed shader/native headers passes C++17 `-Wall -Wextra` without a PCH.
Build logs show zero warnings in changed files. Existing unrelated first-party
and vendor warnings remain visible (including existing OpenGLShader C++20 capture
warnings and archive empty-object warnings); no suppression was introduced.

An additional M1 presentation rerun stopped at its GUI-session preflight before
submission because the desktop was locked; Release presentation was not attempted
with the same unavailable prerequisite. It supplies no new presentation evidence
and is not a predeclared acceptance gate for this offscreen shader/artifact slice.
M1 device/queue/allocator/barrier/residency/event implementation files are unchanged.
No 2× or full scene/export requalification is claimed. Linux's local Docker daemon
is absent; Windows and physical Intel GPU tests are NOT RUN.

The public-docs health check passes; development instructions now cover the
experimental offline producer and clearly retain OpenGL end-user support.
README badges and CI trigger policy are unchanged; no CI run for this checkpoint
is claimed. Raw logs, packages and readbacks are retained in
`build/metal-m2b1-evidence/`; the [machine-readable record](metal-m2b1-results.json)
lists source hashes, every changed file, configuration results and evidence paths.

Commands used for final acceptance:

```sh
python3 scripts/macos/metal_m2b1.py --config Debug --output build/metal-m2b1-evidence/debug-qualified
python3 scripts/macos/metal_m2b1.py --config Release --output build/metal-m2b1-evidence/release-qualified
python3 scripts/hazel.py test --config Debug --profile native --only ShaderToolsSmoke --only RendererSmoke --only RendererFeaturesSmoke --only Renderer2DSmoke --only SceneGPUSmoke
python3 scripts/hazel.py test --config Release --profile native --only ShaderToolsSmoke --only RendererSmoke --only RendererFeaturesSmoke --only Renderer2DSmoke --only SceneGPUSmoke
python3 tests/macos/sdk_workflow.py
python3 tools/agents/check_public_docs.py --root .
```

Focused targets were built through `hazel.build(..., tests=True, targets=[...])`
in both configurations: MetalShaderTools, MetalShaderSmoke, MetalM2ASmoke and the
five selected regression targets. MetalM1Smoke was also built for the additional
presentation check. Exact native refresh commands, environments and readback hash
comparison are retained in the test logs/record; no broad suite was repeated for
the final ownership-only fixes.

## Smallest recommended M2-B2

One offscreen quad submission through the existing renderer submission owner,
using this shader resource and completion-retained per-frame vertex/argument-table
versions, explicit residency and exact color/ID readback. Establish ownership and
coordinate contracts before expanding batches, text/circles, composition or scene
parity. M2-B1 does not implement that slice, merge or change backend defaults.
