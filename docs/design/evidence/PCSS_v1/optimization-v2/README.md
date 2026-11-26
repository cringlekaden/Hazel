# PCSS optimization v2 — INCOMPLETE, unadopted

The owner accepted the existing CSM appearance for continued development. That
production renderer remains PCF; the PCSS experiment failed the declared combined
quality/cost decision. No source, artifact, editor default, reference tolerance,
resource budget or backend default is changed in production by this archive.
No new commit or push is claimed.

Read [the design/decision record](../../../directional-pcss.md) and the original
[frozen criteria](../criteria.json). This directory adds evidence; it does not
replace the earlier failed screens or immutable renderer references.

## Candidates and provenance

- `global416-source.patch`: source-only principal8-search/32-filter candidate,
  including PCF-preserving compile-time aliases, exact contact caches, native
  empty-search refinement and full native depth support.32 planes,36 finite strips,
  motion and lifetime gates passed on native Metal/OpenGL Debug/Release. Cost
  fails in the layered contact stress.
- `bounds448-source.patch`: rejected tighter receiver-plane caster-bound variant.
  It improves the Sun stress but still misses the cost gate and fails four resolved
  penumbra cases at .14365 error versus the frozen .10. Release depth-grid tests
  include48 finite/grazing/layered cases. Debug448 was not qualified.
- Companion `*-qualification-workspace.patch` files and
  `DirectionalPCSSChecks-*.h` preserve the actual test context. Workspace test
  patches include preexisting CSM qualification infrastructure; those are
  reproduction inputs, not adopted/revived production CSM experiments.
- `directional_pcss.py` and `pcss_optimization.py` are qualification-only tools.
  Scalar/gather exact-fallback variants are generated from canonical GLSL; their
  cost regressions are retained, with no claim of full visual qualification.
- `generated-ForwardPBR_PCSS.*.metal` is generated evidence, not another shader
  authority. It shows8-location search,32-location filtering and explicit base-LOD
  hardware comparison. Hardware ISA/register occupancy was not measured.

Artifacts must be regenerated offline from the matching source patch. Do not mix
400-byte production,416-byte principal-prototype and448-byte bound-prototype CPU
binaries/resources. The two experimental PCSS programs were removed from normal
resources and staged app resources when production was restored.

## Local raw evidence retained

All paths below are relative to this existing checkout, ignored by Git. They are
not a second checkout. Raw render readbacks and original logs remain in place.

| Path | Content |
|---|---|
| `build/pcss-v1/final-support/` | Principal global416 Debug/Release plane, strip, motion, authored and lifetime rerun |
| `build/pcss-v1/checkpoint/` | GGX735, local terminator, PCF foundation, shader corpus/cache, AO/TAA regressions; frozen resources and binary hashes |
| `build/pcss-v1/bound-release-planes/` | Rejected448 penumbra FAIL records and raw captures |
| `build/pcss-v1/bound-release-strips/` |48 finite/grazing/layered raw captures and independent depth-grid results |
| `build/pcss-v1/optimization/` | Source-based optimization candidates, earlier negative screens, paired source resources and raw timings |
| `build/pcss-v1/comparisons/` | Final ordinary/overlap/1440p data, actual native1x presentation joins, copied project/resources |
| `build/pcss-v1/layered-*-cost/` | Global-search mixed-depth stress,20 samples after5 warmups |
| `build/pcss-v1/bound-layered-*-cost/` | Tighter-bound stress |
| `build/pcss-v1/fused-exact-*-cost/` | Exact scalar/gather fusion stress |
| `build/pcss-v1/archive/global416/bin/{Release,Debug}/` | Matching principal-prototype native binaries in explicit `--executables` layout |
| `build/pcss-v1/bound-before/resources/` | Principal416 resource snapshot |
| `build/pcss-v1/archive/bounds448/` | Decision-time448 resources and Release binaries |
| `build/pcss-v1/resources-global-control/` | Principal shader algorithm with unused448 ABI fields for final cross-comparison binaries |
| `build/pcss-v1/research/` | Pinned Filament/PKRenderer source snapshots and commit metadata |

The final global-control comparison uses the same8/32 global-search shader
algorithm as the qualified416 candidate, with unused world-bound uniform fields
for the current448 prototype binaries. Both layouts align to512 GPU bytes. This
compatibility control is explicitly recorded rather than presented as a new
production implementation.

## Reproduction

Use the recorded native binaries/resources directly, for example:

```sh
python3 scripts/macos/directional_pcss.py --config Release \
  --resources build/pcss-v1/bound-before/resources \
  --executables build/pcss-v1/archive/global416/bin/Release \
  --output build/pcss-v1/reproduce-global416
```

For source reproduction, the recorded base is `d80bb5e9a3fcfd687b1a995980806c91748e1b8d`
on `feature/3d-foundations`. Preserve unrelated working changes before explicitly
applying an experimental source/workspace patch and copying its matching header
to `tests/macos/DirectionalPCSSChecks.h`. Build `MetalShaderTools`, run the tracked
experimental14-package producer, then build the native helpers. Source-only
patches intentionally omit metallib payloads; generated MSL is not edited.
The current normal build intentionally does not implement PCSS. Tools reject that
configuration unless matching isolated prototype binaries/resources are selected.

Raw command arrays, sample counts, p95 values, gate failures and source hashes are
in the JSON/CSV records. Copied reports/logs replace the checkout prefix with
`<repo>`; original raw logs are retained locally. `captures/` contains unchanged
GPU-derived PNGs at matching exposure/viewpoints. `native-fixtures/` reconstructs
the exact copied scenes; TAA was a CLI override. The actual monitor was1x. Do not
claim that the1440p offscreen run established Retina2x presentation.

The strict CSM stress failure remains FAIL. Authored-frame strict parity failures
are retained in `authored-parity.json`, including zero new stable-PCF RGB failures
in this scoped15-capture comparison. This does not waive any original gate.

Full source-free export/relocation, native2x, Windows/Linux/Intel PCSS execution and
owner PCSS visual acceptance are NOT RUN. The current milestone is INCOMPLETE.
