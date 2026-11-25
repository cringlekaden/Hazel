# TAA performance follow-up

Baseline: `dc3a4d1b50e56ce51d46f9f5c67392f1335211c5`,
`feature/3d-foundations`, Apple M4 / macOS 27.3. This is a scoped optimization
of the [geometric-edge correction](taa-edge-investigation.md). The original
owner visual re-review remains **INCOMPLETE**. Automated optimization gates
pass; this record does not grant general TAA visual acceptance.

## Profile and selected changes

An unchanged native Release profile reproduced the reported bottleneck:
1080p Medium XeGTAO + TAA resolve was 5.810 ms median and the whole GPU
submission was 11.917 ms median. The SceneRenderer still owns one shared scene
submission path: jittered radiance and unjittered motion/depth/IDs feed the
Metal temporal pass, then world 2D, tone mapping, UI and presentation.

Controlled sequential experiments selected three small changes:

- Explicitly unroll the fixed current/history cubic and depth-neighborhood loops,
  exposing constant tap weights/tests to the pipeline compiler. Resolve median
  at 1080p fell from 5.810 to 5.499 ms in the first isolated experiment.
- Reuse the already-read exact central ID when motion dilation selects that
  pixel. Skip the slope-depth neighborhood only when the closest 3x3 depth is
  sky. With depth clear 1 and Less testing, that means the selected pixel remains
  the centre and the same neighborhood contains no depth below 1; the skipped
  scan would produce exactly zero. Together these measured 5.307 ms.
- Use a measured 32x4 compute threadgroup instead of 8x8, with a matching
  128-thread pipeline-capacity check. 16x8 measured 5.138 ms; 32x4 measured
  4.897 ms. 32x8 measured 4.906 ms and was discarded. These timings identify
  useful changes, not hardware-counter proof of cache, occupancy or bandwidth
  behavior. No queue barrier, residency, retirement or CPU-wait change was made.

A zero-weight-history-tap experiment added no measurable gain and was discarded.
The source audit found up to 16 current radiance taps, 16 per-tap validated cubic
history taps, and two 3x3 depth scans. Overlapping reads can be cached; nominal
shader tap counts are not measured DRAM traffic. FP32 radiance/history and
R32 signed IDs remain unchanged. History alpha still stores FP32 proxy depth.
Packing to FP16 would change HDR/depth precision and was not justified by these
measurements. Cheaper bilinear reconstruction in smooth/interior regions was
considered but not introduced: current smoothness alone cannot establish that
reprojected history has no silhouette/fine texture, and the prior correction
already demonstrated recursive bilinear diffusion. Exact work reductions were
available without accepting that risk.

Both 4x4 cubic reconstructions, all per-tap ID/depth rejection, foreground motion
dilation, slope-aware tolerance, camera-cut logic, variance clipping and
responsiveness remain intact. There is **no introduced quality tradeoff**,
additional blur, sharpening, FXAA, tolerance increase or history format change.
Performance gains are measured for this fixture/GPU, not guaranteed for other
content or Apple hardware.

## Matched performance

Milliseconds are median/p95, native presented Release, Medium XeGTAO + TAA.
Each extent runs 720 frames; the first 120 and last 20 are excluded. Only
positive presentation feedback and ordered completed GPU intervals count.
Resolve intervals include synchronization and must not be added to the
whole-submission interval. The baseline column is an isolated run; the final
columns come from the fully qualified five-preset run.

| Physical extent | Whole GPU before | Whole GPU after | Resolve before | Resolve after | History bytes after |
| --- | ---: | ---: | ---: | ---: | ---: |
| 1280x720 | 5.388/5.435 | 4.994/5.028 | 2.599/2.635 | 2.188/2.209 | 38,010,880 |
| 1920x1080 | 11.917/12.059 | 11.015/11.108 | 5.810/5.891 | 4.892/4.941 | 83,558,400 |

History bytes, ping-pong allocation count and resource budgets are unchanged.
The [manifest](taa-performance.json) retains all five final AO presets, sample
counts, median/p95 CPU/GPU/pacing distributions, RSS, allocation/reuse counters,
tracked memory peaks and complete retirement. Its initial exploratory Off/Low
baseline briefly overlapped edge capture and is excluded from the primary
comparison. The isolated Medium baseline and selected final measurements were
serialized without other qualification GPU work.

## Quality and acceptance

All **32 geometric sequences per configuration** pass in Debug and Release,
using unchanged independent analytic silhouettes, raster-jitter and motion
oracles, temporal stability criteria and moving-edge soft-width bounds.
All 2,350 retained baseline/candidate frame pairs (135,360,000 pixel records)
are **byte-identical**, including RGBA, exact signed IDs and depth. These cover
all four edge groups and the HDR/AO/detail fixture in both configurations;
they are sampled captures, while the geometric sequence metrics cover all
64 frames. Maximum color error and RMSE against the qualified baseline are zero.

Fine-texture RMSE stays .0097812 versus .0299088 with TAA off; contrast stays
.0403433 against the independent .0441489 reference. Registered moving-floor
RMS on/off ratios remain .795679 Low, .764797 Medium and .887847 Ultra. These
ratios retain the prior XeGTAO benefit but include physical occlusion changes
and are not AO ground truth. XeGTAO shaders/sampling/integration are unchanged.

| Gate | Status and evidence |
| --- | --- |
| Debug/Release editor, player, native test builds | PASS; no new first-party compiler/shader warnings. Existing no-symbol archive and Xcode script-phase warnings retained. |
| All 32 edge sequences and HDR/AO/detail/cuts/disocclusion/UI/ID/depth | PASS in both configurations; exact baseline capture comparison. |
| Three-slot serialized image equivalence, busy cancellation, view destruction, resize and retirement | PASS in both configurations; no lifetime ownership changes. |
| Native API/GPU validation, 1x/2x lifecycle and independent 2x temporal views | PASS in Debug/Release; zero errors in exercised gates. |
| Native Apple OpenGL RuntimeSession regression | PASS Debug/Release with immutable `86c581c` fixture provenance retained. |
| Five-preset 720p/1080p presented performance and bounded memory | PASS; complete submission retirement, zero GPU errors. |
| Fresh signed source-free editor/player, TAA relocation and missing-library rejection, pristine product launches | PASS. |
| Public README link chain and affected instructions/workflow policy | PASS static audit; no public workflow/default/feature change. No new CI run claimed. |
| Windows/Linux execution, other Apple GPUs and physical Intel HD4000 | NOT RUN for this Metal-only slice. |
| Original owner visual re-review | INCOMPLETE, unchanged; fixture equality does not replace that review. |

The first Release 2x lifecycle attempt stopped before test execution because
the console locked. That blocked attempt and its console snapshot are retained;
the unlocked retry and all remaining native gates subsequently passed.

Raw exact commands, logs, phase/analytic CSVs, full FP32 captures and comparison
results are retained under `build/taa-performance/`; the checked-in manifest
records source/artifact identities and raw evidence hashes. Public entry-point
links, branch-specific clone instructions, current launch/package commands and
CI triggers were inspected. Local execution does not certify Windows/Linux or
feature-branch CI. The owner's scene/project modifications remain byte-for-byte
unchanged and are excluded from this optimization. No backend default, merge,
release or unrelated rendering feature is included.
