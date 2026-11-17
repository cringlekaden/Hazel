# M3 legacy line coverage investigation — historical INCOMPLETE

This investigation predates the user's explicit approval to change OpenGL line
rasterization. Its measurements and legacy captures are preserved. Current
implementation and qualification are in the
[shared deterministic line record](metal-m3-deterministic-lines.md). The earlier
"unlocked" conclusion below used an incomplete console summary; its null session
lock field is not proof of an unlocked intended session. Current window status
comes from the maintained runner's full per-session lock check.

Continuation from `808e31d5eecbd3cc51d3efcae5d46aa5e3afe798` on
`feature/3d-foundations`. This is a diagnostic working-tree result, not a new
qualified commit. Production renderer/shader code, generated artifacts, frozen
references, OpenGL smoothing and acceptance tolerances are unchanged.

## Decision required

The smallest conventional screen-space expansion/analytical coverage alternatives
do **not** reproduce the maintained Apple OpenGL smoothed-line output. Stop M3
implementation here under the requested architectural stop condition. Do not
calibrate a permanent Metal algorithm to undocumented driver interpolation.

Recommended next contract: explicitly authorize a **shared deterministic line
representation**, including an intentional OpenGL output change. Keep DrawLine,
the existing Renderer2D streams/runs/capacity/statistics, SceneRenderer ownership,
and canonical GLSL → generated Metal shader pipeline. Expand segments into
triangles with common endpoint/coverage data; define physical framebuffer-pixel
width, butt endpoints, homogeneous clipping before perspective division, depth
interpolation, primitive clipping, and an explicit picking footprint. Prefer
pixel-area coverage over an orientation-dependent brightness approximation.
The picking decision must explicitly address legacy zero-alpha fringe IDs;
discarding zero coverage changes current picking. Default one-pixel physical
width and logical UI scaling must be distinguished at native 1×/2×.

That proposal needs new independent contract tests and a reviewed migration of
OpenGL visual expectations after authorization. It is **not implemented** and
does not justify resetting current references. Triangle expansion alone does
not establish depth, clipping, gradient-color interpolation or lifetime parity.

Alternatively preserve the legacy OpenGL output and leave M3 incomplete while
investigating driver-specific rasterization. The closer major-axis approximation
below still fails; adopting it would require additional interpolation/tie-rule
emulation, with no demonstrated Windows/Linux/Intel portability. Neither option
is silently selected by this investigation. No M4 work is authorized by it.

## Reproducible measurements

Host: Apple M4, native Apple OpenGL `4.1 Metal - 91.7`. Both Debug and Release
builds of MigrationRenderer2DSmoke pass. All 50 captures and their metadata are
byte-identical across configurations. The capture keeps GL_LINE_SMOOTH enabled
and uses the actual Renderer2D line shader, ordered runs, source-over blending,
RGBA32F color and R32Sint IDs. Depth is disabled to isolate raster coverage;
these captures do not qualify depth behavior or native Metal rendering.

The diagnostic includes eight orientations at four subpixel phases, reversed
endpoints, short/degenerate segments, requested widths 0.5/1/2/4, transparent
overlap, passive overlap, primitive clipping, viewport clipping, perspective,
near-plane clipping and a doubled physical render target. The latter is a
synthetic pixel-density case, **not** actual native 2× window qualification.
Both reported OpenGL width ranges are [1,1]; all requested widths become one.
Reversed endpoints produce identical RGBA and ID footprints for the three
tested pairs. Every captured ID is exactly the expected large negative signed
ID or background -1, including preservation of the first ID under passive
overlap. No unsigned conversion or alpha-based ID tolerance is used.

The FP64 CPU analyzer clips the homogeneous centerline against all six clip
planes, projects to physical pixels, applies the primitive clip, and evaluates
three explicitly defined candidates. Its independent area self-checks cover a
unit box, diagonal intersection, area conservation and homogeneous clipping.
It compares all RGBA channels at the existing 2e-5 color gate. Candidate picking
writes the signed ID on positive visible support; numerical polygon-zero
classification does not alter captured values or the acceptance gate.

| Candidate (CPU model, not Metal implementation) | Maximum RGBA error | ID footprint mismatches | Failing cases / 50 |
| --- | ---: | ---: | ---: |
| Exact pixel intersection with butt-ended rectangle | 0.5418449425 | 1298 | 46 |
| Signed-distance rectangle, one-pixel linear coverage ramp | 0.5 | 797 | 46 |
| Ideal major-axis linear coverage ramp, half-open endpoints | 0.00304603872 | 293 | 26 |

The major-axis model is an explanatory approximation, not a claim about the
driver's internal algorithm. Its maximum error is over 152 times the existing
gate. Even eliminating its ID differences would leave color failures.
An orientation-independent area model cannot preserve this driver's output:
for length-40 segments at phase zero, observed coverage sums are approximately
40 (horizontal), 37.933 (15°), 33.994 (30°), and 27.958 (45°); the rectangle's
area is 40 for each. Some horizontal/vertical half-phase cases write 40 IDs at
exactly zero alpha, and a 45° case writes 28. Thus a visible-support picking
rule also changes legacy behavior even where blended color matches.

OpenGL 4.1 permits implementation-dependent antialiasing approximations; the
ideal area formulation is not a guarantee of the driver's precise output.
See sections 3.3 and 3.5.2 of the
[official specification](https://registry.khronos.org/OpenGL/specs/gl/glspec41.core.pdf).
These measurements demonstrate failure of the tested maintainable models,
not a proof that all possible emulations are impossible.

Reproduce with fresh output directories:

```sh
python3 scripts/macos/metal_m3_lines.py --config Debug --build --output build/metal-m3-lines-debug-fresh
python3 scripts/macos/metal_m3_lines.py --config Release --build --output build/metal-m3-lines-release-fresh
```

Raw evidence is in `build/metal-m3-evidence/line-investigation/`: final-debug
and final-release contain capture logs, 50 raw RGBA/ID files, cases.json and
analysis.json. Successful diagnostic execution does not mean a candidate or M3
passed. The normal `metal_m3.py` acceptance runner is unchanged; its previously
recorded normal-line failure (308 IDs, maximum color error 0.353) remains
unresolved. No aliased diagnostic is counted as acceptance.

## Ownership and resource review

Reviewed shared Renderer2D initialization, ordered/grouped FlushOrdered, common
line vertex packing, native batch capture, MetalDevice submission/buffer reuse,
and shutdown. The Metal run adapter consumes the shared CPU streams; it is not
a second primitive packer. Native encoding/residency/retirement remain backend
owned. Existing queue/drawable/startup failure hooks still have active lifetime
tests; removing them would discard valid qualification coverage. No production
cleanup is justified by this coverage experiment.

MetalDevice reuses buffers only in retired frame slots, drops unused buffer
entries after preparation and reallocates buffers over four times the requested
size. Full-frame readback storage is omitted for window submissions. Shutdown
waits for GPU completion and feedback, then clears slot pools. These are source
findings, not new leak/performance results. The original M3 record's 720p/1080p
attachment and retained-buffer measurements remain historical evidence; no new
native Metal CPU/GPU or memory measurement was made in this continuation.

## Remaining qualification

No new Metal live presentation, physical input, camera interaction, native
1×/2×, resize/minimize/restore, positive presentation timestamps, extended
session or resource-retirement gate is claimed. The console check for this
continuation was unlocked (`console.json`); these gates are **NOT RUN** because
the line contract remains unresolved, rather than reported as currently locked.
The earlier M3 record's locked-console result is historical. If the console is
locked when qualification resumes, window gates must be BLOCKED without display
workaround loops. Windows, Linux and Intel execution are NOT RUN here.

There are no new first-party compiler warnings in the focused builds. Existing
ScriptableEntity unused-parameter and TextCompositingChecks missing-field
warnings remain. Full native Metal shader/cache/lifetime acceptance should run
after the line decision and implementation; it has not been rerun unchanged to
turn this diagnostic into an acceptance claim. OpenGL regression results and
machine-readable evidence are recorded in the companion
[continuation results](metal-m3-lines-results.json).

The maintained native OpenGL Renderer2DSmoke passes in both Debug and Release,
including its normal and two-slot/GLSL settings, primitive capacities, ordered
runs, signed picking, Unicode/atlas changes and failed-constructor recovery.
An initial direct invocation lacked the required disposable assets and failed;
the maintained `scripts/hazel.py test --config Debug --profile native --only
Renderer2DSmoke` runner (also with Release) supplies them and passes. This is
native Apple OpenGL evidence, not software GL or Metal acceptance. The public
link check reports 72 Markdown files, 591 local links/images and zero findings.

Verdict: **INCOMPLETE — architectural line coverage/picking decision required**.
No commit/push: authorization for this continuation is conditional on all M3
gates passing. M4 remains outside scope.
