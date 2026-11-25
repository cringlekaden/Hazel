# TAA v1 qualification record

Owner subsequently **rejected geometric-edge quality at `4c25138`**. The initial
records below describe that checkpoint; they do not establish visual acceptance.
See the [focused investigation and correction](taa-edge-investigation.md).

Baseline HEAD `3a0f71cb13728a6802ad1c9eaadf6ed384d8e4a0` on
`feature/3d-foundations`; implementation was qualified as a dirty tree. The commit
containing this record identifies the delivered source. Apple M4, macOS 27.3,
native Metal 4 and native Apple OpenGL; no software GL substituted. Authored
examples, frozen PBR/IBL references and the XeGTAO historical measurements remain
unchanged. Windows historical full-suite timeout remains deferred. Windows/Linux
execution was NOT RUN in this macOS checkpoint.

Frozen-fixture provenance: full regression probes use immutable
`86c581c13fd5c944c2a06d720314a753d7607d8f` via
`HAZEL_TEST_EXAMPLE_REVISION`. At baseline HEAD, the existing
`Reference/IBLQuality_v2/Materials/M0R3.hmat` hashes to
`cd46e0bdeed723b167fa8da46654748b4c71a51d12e8a16813da7c42b6c8510a`,
while its conditions require
`da104e331b0cb02bdc17d215903903d0463b69b464ed4613936950145ebb6623`.
The latter matches the frozen snapshot. The initial HEAD pristine-export probe
failed this check (`final-pristine-products/run.log`); the failure is retained.
No hashes were reset and no authored files changed. Current-HEAD M6 products
were tested separately; exported products contain the current project with a
private TAA-enabled start scene, while expanded frozen probes use that snapshot.

Owner visual acceptance: **INCOMPLETE — NOT REVIEWED**. Automated checks and the
small captures below cannot establish final production visual quality. Use the
[manual procedure](taa-manual.md) and record acceptance or defects separately.

## Evidence and gates

Automated feature gates: **PASS**, using the frozen fixture provenance above.
Final visual-quality acceptance: **INCOMPLETE**.

Raw commands, logs, counters and captures are under `build/taa-evidence/`.
[The gate manifest](taa-qualification.json) retains commands, statuses and the delivered source digest. Raw `final-gates.json` records the broad acceptance commands and exit status. Earlier failed
experiments remain there: the initial 2x storage budget failure, HDR precision
probe, test queue gate sequencing, and clipped Scene Settings workflow. Their
corrections were requalified; they are not counted as passing executions.
`cleanup-gates.json` records the final encoder-failure cleanup builds and fresh
source-free exports, including the correctly pinned frozen regression probes.

| Gate | Evidence / scope |
| --- | --- |
| Debug/Release build | `compact-build-debug.log`, `compact-build-release.log`, `acceptance-build-release.log`, final M1 Release build; no new production first-party warnings. Existing deprecated-copy, unused test helpers and build phase/libtool diagnostics remain. |
| Native final resolve | `detail-reference-debug/`, `optimized-native-release/evidence/`; finite HDR, motion, cuts, disocclusion, fine/masked/specular motion, AO Off/Low/Medium/High/Ultra, exact IDs/depth/UI and cancellation. |
| Native velocity oracle | `velocity.csv`: perspective 4,394 / ortho 7,265 pixels; maximum reprojection error .001965 / .001588 pixel, depth error 2.385e-7. Independent CPU depth reconstruction, actual camera and model motion. |
| Three pending temporal submissions | Warmed history, queue gate, distinct colors versus independent FP64 Neutral oracle, busy fourth CPU packet, view destroyed before completion; completion-owned resources retire. |
| Lifecycle and scale | Debug `compact-lifecycle-debug-{1x,2x}` and Release `final-lifecycle-Release-{1x,2x}`; actual physical extent, resize/minimize/invalid drawable, three slots, bounded allocations and retirement. |
| Two independent views | Debug 1x `two-views-debug-main`, Debug 2x and Release 1x/2x `final-two-views-*`; two moving cameras/models in one native drawable, verified colors and positive presentation. |
| Disabled rendering parity | `m4-debug/`, `m4-release/`: 135-case native parity and frozen independent PBR/IBL numerical coverage in each configuration. |
| Shader artifacts/cache | `shader-cache-debug/`, `shader-cache-release/`: cold/warm/corrupt/source-free and 17 invalid shader rejection cases. Final compute reflection and packaging additionally execute current TemporalAA library. |
| OpenGL regression | `opengl-debug.log` plus corrected `editor-scroll-owner-debug.log`: all 18 Debug tests collectively pass; `opengl-release.log`: full 18/18 Release pass. Includes TAA setting Apply/Undo/Redo/Save/Play with unsupported OpenGL output retained. |
| Existing Metal lifetime | `m3-release-retest/` and final Debug M3. Existing active-frame accounting semantics preserved; idle history/attachments separately included in admission budget. |
| Native failure/product/export | Final M5 Debug/Release; M6 product profiles Debug/Release; fixed XeGTAO regression; fresh signed bundles, source-free TAA relocation and pristine exported editor/player. Final status recorded in `final-gates.json`. |
| Public docs | README, editor and packaging guides updated; local public link audit required after final records. No CI/platform certification inferred from local runs. |

Release 2x lifecycle submitted/completed 332 frames, 325 positively presented,
peak three in flight, zero errors, 33 total attachment allocations across resize
and 1,732 reuses. Peak active-frame tracking 915,887,796 bytes, attachment tracking
879,230,976 bytes and allocator 8,924,712 bytes overlap; do not add these numbers.
The initial display size was clamped by the window manager (2560x1126 physical),
then resized to 1920x1080 at 2x. The advertised 1280x720 logical / 2560x1440
physical display mode was selected and the exact original mode restored.

## Motion, detail and limits

Final Release motion fixture uses 64 actual scene steps per AO preset, camera
translation after step 16 and independent occluder motion after step 32. Native
readback GIFs show sampled frames 16–60, four steps apart, slowed for inspection:
[TAA off](images/taa/medium-off.gif), [TAA on](images/taa/medium-on.gif).
These are offscreen 320x180 fixture images, not product screenshots. Registered
floor luminance RMS ratios on/off are .830 Low, .805 Medium, .882 High, .911 Ultra
(.945 AO-off). Counts, per-pair and camera-only/moving-occluder segments are in
[the measurement record](taa-measurements.json). Physical AO changes remain in
this metric; smoothing can lower RMS, so it cannot establish fidelity alone.
For Medium, the camera-only ratio is .757 and the moving-occluder ratio .819;
both paths improve this local metric. AO-off moving-occluder variation is 1.009,
so temporal filtering does not reduce every measured signal.

The independent static fine-texture test compares native TAA and disabled output
against a CPU-averaged native 4x-resolution reference over 12,027 interior pixels.
RMSE is .0146213 with TAA versus .0299088 without; TAA contrast .0395083 versus
reference .0441489 (89.5%). Predeclared bounds require improved RMSE and 85–120%
reference contrast. Captures: [reference](images/taa/detail-reference.png),
[TAA](images/taa/detail-taa.png), [off](images/taa/detail-off.png).
This demonstrates retained detail for this texture, not universal sharpness.

Synthetic perspective/ortho rigid-motion interiors remain below the .015 mean
color-error gate through moving objects, camera motion, cuts and exposed ground.
Exact IDs/depth are compared every frame; screen UI colors match exactly.
A million-unit HDR signal at low exposure and a 120,000→100,000 radiance change
pass immediate-response checks. Persistent interior color trails were not found
by these tests. Arbitrary AO edge trails, bright leaks and specular ghosting still
require owner review of longer real scenes. No sharpening or independent AO
history hides these limitations. Unsupported deformation/material correspondence
is documented in [the design](taa.md).

[Two native temporal views at 2x](images/taa/two-views-2x.png) is an actual complete
drawable readback, frame 90, with positive presentation recorded in the native
trace. Both histories move independently and retire without errors.

## Performance

[Full measurements](taa-measurements.json) retain all CPU median/p95, GPU sample
counts, phase intervals, memory/reuse, completion and positive-presentation pacing.
Matched sequential Release runs use 720 frames per extent, excluding 120 warmup
and 20 tail. CPU submission includes slot backpressure. Whole GPU values require
positive presentation feedback. Phase completion intervals include synchronization
and overlap: **do not sum them**. The old [XeGTAO baseline](xegtao-measurements.json)
is preserved; contemporaneous off/on measurements are the controlled comparison.

GPU whole-submission median / p95 milliseconds:

| AO | 720p off | 720p on | 1080p off | 1080p on |
| --- | ---: | ---: | ---: | ---: |
| Off | 0.743 / 0.797 | 2.754 / 2.873 | 1.637 / 1.747 | 6.205 / 6.293 |
| Low | 1.820 / 1.989 | 4.196 / 4.276 | 4.439 / 4.988 | 9.476 / 9.577 |
| Medium | 2.143 / 2.302 | 4.536 / 4.597 | 5.675 / 5.726 | 10.154 / 10.255 |
| High | 2.940 / 3.354 | 5.323 / 5.383 | 7.404 / 7.489 | 11.884 / 12.008 |
| Ultra | 5.870 / 5.899 | 7.839 / 7.894 | 12.988 / 13.089 | 17.464 / 17.573 |

Medium+TAA scene CPU median/p95 is .074/.099 ms at 720p and .071/.119 ms at
1080p; whole submit CPU is 4.155/7.819 and 9.098/16.199 ms respectively, including
backpressure. Positive presentation spacing median/p95 is 4.239/11.135 and
9.201/18.406 ms. These are unpaced fixture loops, not a guaranteed display rate.

The full-resolution 16-tap HDR reconstruction is the measured TAA bottleneck.
Scalar reciprocal reuse, precomputed cubic weights and redundant depth-load
elimination reduced the 1080p resolve from about 4.93 to 4.04 ms (about 18%),
without changing its reconstruction filter; current Medium p95 is 4.120 ms.
720p Medium resolve median/p95 is 1.763/1.797 ms. Further bandwidth reductions
would need separate quality evidence. 1080p Medium+TAA is faster than current
Ultra without TAA (10.154 vs 12.988 ms); local motion variation improves with
Low/Medium, but owner review must determine whether their AO fidelity suffices.
Ultra+TAA exceeds 16.67 ms in this fixture and is not a practical 60 Hz default.

Two histories use 38,010,880 bytes at 720p and 83,558,400 at 1080p, including
exact IDs and depth in HDR alpha. Allocations are two per extent (four cumulative
after resize), with 1,417 reuses by the second extent; no ongoing history growth.
All ten benchmark runs completed/retired without GPU errors or live attachment
generations. No renderer rewrite, extra CPU wait or higher resource budget was
introduced to reach these results.

Final adversarial source review traced `SceneRenderer` cancellation/reset,
`MetalFramebuffer::ResolveTemporal` ping-pong selection, native prepare/encode,
post-commit token publication, completion timing/readback and resource retirement.
A failed temporal compute-encoder creation now ends the command buffer before
throwing, matching the AO encoder's cleanup. The actual driver allocation failure
was reviewed statically, not induced; validation fixtures cover preparation
failure, aborted reservations, busy slots and recovery. Final performance and
export gates include this cleanup. No unresolved source-review finding remains;
visual limitations above remain open.

Final alpha checkpoint: `alpha-before-fix/native.log` reproduces forced opaque
alpha. Current spatial alpha reconstruction preserves .35 background alpha and
.4-alpha world-2D composition (.61 final alpha), exact colors, IDs and depth
against TAA-off, across four frames in Debug/Release. `alpha-native-*`,
`alpha-final-gates.json`, `benchmark-alpha-taa-on/evidence`, `alpha-packages`,
`alpha-relocation` and `alpha-pristine-metal-products` qualify the final shader.
All 123 retained opaque motion/detail reference readbacks match the final
Release fixture byte-for-byte. The preceding full source-free managed/OpenGL
suite passed in `cleanup-pristine-products`; final shader-only checks repeat
the native TAA fixture, source-free TAA probe/missing-library rejection and both
pristine Metal products. No unrelated managed/OpenGL suite was repeated for
this shader-only change.
