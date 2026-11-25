# TAA geometric-edge investigation

Baseline: `4c25138c2c75189ee024cc7d1fdef6e5fc693acf`,
`feature/3d-foundations`, Apple M4 / macOS 27.3. The owner rejected that
revision's geometric antialiasing: stationary edges jittered and camera motion
made aliasing severe. The earlier interior-pixel and AO tests missed this.
Final visual acceptance requires owner re-review; this record does not grant it.

## Reproduction and causes

`TemporalEdgeQualification.h` renders an unlit diagonal cuboid with **AO off**,
using the production SceneRenderer, Metal raster, temporal resolve and tone map.
Thirty-two 64-frame sequences cover front-facing and steeply sloped (60-degree)
geometry, ordinary and small-near/long-far lenses, perspective/orthographic
projection, a stationary
camera, camera translation, camera rotation and independent object motion.

The failures were history validity/contribution at silhouettes, not jitter sign:

- The original camera-cut heuristic tested near-plane/mid-depth clip corners.
  With the default .01 perspective near clip, normal camera movement reset
  history to phase 1 on the second moving frame. Long-far orthographic rotation
  could trigger the same problem. `CameraCut` now compares camera rays at depth
  estimated from submitted mesh bounds, independent of near/far range and scene
  units; per-pixel disocclusion handles visibility changes. Every sequence also
  asserts that a large camera jump resets history.
- The original resolve compared current and historical luminance as though any
  difference were a lighting discontinuity. Subpixel coverage changes naturally
  change luminance at an edge. That test repeatedly drove history contribution
  to zero at precisely the pixels needing accumulation. The contribution capture
  shows the [old black outline](images/taa-edges/weight-before.png) versus the
  [corrected stationary contribution](images/taa-edges/weight-after.png).
- A mixed edge pixel's color could include foreground while its unjittered
  centre/depth hit background. Following that background's motion/sky rejection
  discarded or misregistered foreground coverage. Orthographic rotation rejected
  roughly half the edge band in the baseline. Closest-depth 3x3 motion dilation
  now follows foreground coverage on both sides of the silhouette.
- The fixed normalized-depth tolerance also rejected valid neighboring samples
  on steep perspective faces. A held-out 60-degree face exposed only 28–46%
  history acceptance during motion even after the first corrections. Tolerance
  now follows same-ID local depth variation around the chosen foreground sample
  over the two-pixel cubic footprint. Background/other IDs cannot inflate it.
  This restores roughly 95% acceptance without changing authoritative depth.
- Repeated bilinear history interpolation then exposed recursive edge diffusion.
  In the intermediate dilation experiment, moving-edge soft width reached 2.07
  pixels against a .93-pixel analytic reference. Bounded Catmull-Rom history
  reconstruction reduces that to 1.54 pixels. Current reconstruction was already
  cubic and is unchanged. No FXAA, wider spatial blur or sharpening was added.
- The old mixed-coverage weight penalty also applied to stationary edges. It now
  fades in with motion over 1/16 physical pixel per frame, allowing full .92
  accumulation at rest while preserving a moving-edge response. Pixel motion also shortens feedback more
  strongly to bound moving-edge width once valid sloped history is restored.

`TemporalAA.metal` makes luminance responsiveness relative to neighborhood
contrast, retaining flat-HDR change response, per-tap ID/depth rejection,
variance clipping and current RGB bounds. History alone stores closest-surface
proxy depth and exact integer proxy ID. Final picking/depth still copy the
original unjittered motion-pass attachments without filtering. Resource count,
ping-pong ownership, queue barriers and submission tokens are unchanged.

## Coordinate and frame audit

`SceneRenderer::Render` offsets clip x/y by `2*jitter/physicalExtent`, where the
Halton(2,3) jitter is bounded within half a pixel. Raster artifacts flip vertex Y
and convert clip depth once in `MetalShaderCompiler`; offscreen rows retain the
engine's lower-left convention. Resolve samples current radiance at `p+jitter`.

`MotionPBR` writes absolute **previous unjittered UV/depth**, not a signed screen
velocity. History is also stored on the unjittered grid, so an additional previous
jitter subtraction would be wrong. For dilated motion at neighboring pixel q,
resolve uses `previousUV(p) = uv(p) + previousUV(q) - uv(q)`. Perspective sky rays
retain rotation reprojection. Input, world 2D, UI and picking stay unjittered.

The new **Jittered input (no resolve)** debug view exposes actual raster samples.
Analytic projected geometry classifies every tested pixel after applying the
recorded jitter: zero raster mismatches in all 512 static geometry/lens/projection/phase
frames, excluding a .002-pixel boundary precision margin. The existing independent
camera/object motion oracle, including rotated and translated cameras, remains
below .002 pixel correspondence error. Both projection types are exercised.

History phase advances with successfully committed packets. `TemporalLifetime`
now compares **every RGBA pixel, exact signed ID and depth** in three gated,
independently moving diagonal-edge submissions against the same serialized
sequence. Results agree within 1e-6 color with bit-exact IDs/depth; it also tests
a busy fourth CPU packet and destroys the view before retirement. The strengthened check passes with the correction; shader-only ablations
reduce the failure without changing frame ownership. Three in-flight frames
were not the cause, and no CPU wait was introduced into the renderer.

The normal-speed lens check uses .01 near / 1000 far for perspective and a long
orthographic range, with bounded camera translation up to about 2.8 pixels per
frame. Before the camera-cut correction, the phase-advance assertion fails at
frame 1; after it, all 64 phases advance normally and the large-jump assertion
still resets to phase 1. [Corrected normal-speed camera](images/taa-edges/fast-camera-after.gif).

## Edge evidence

The independent reference clips the double-precision projected cuboid silhouette
against each pixel square. Unlit colors occupy the tone mapper's linear segment,
so errors can be expressed as normalized opaque coverage. Metrics use an analytic
1.5-pixel edge band; residual change subtracts the true per-frame coverage change.
Soft width counts 5–95% coverage pixels divided by projected perimeter. Frames
16–63 are measured after warmup. These metrics distinguish stability from blur.

Representative perspective results (orthographic counterparts also pass):

| Path | TAA-off error | Old TAA error | Corrected error | Old residual change | Corrected residual change |
| --- | ---: | ---: | ---: | ---: | ---: |
| Stationary | .1615 | .1193 | .0450 | .1736 | .0146 |
| Camera translation | .1620 | .1234 | .0925 | .1752 | .0433 |
| Camera rotation | .1619 | .1231 | .0803 | .1850 | .0511 |
| Independent object | .1609 | .1193 | .0894 | .1679 | .0477 |

Stationary jitter falls about 92%; geometric error falls 72% versus TAA off.
Moving geometric error falls 43–50% versus TAA off, with substantially smaller
residual changes. Corrected mean history weights in the edge band are about .919
at rest with smaller adaptive weights during motion. Orthographic rotation
accepts about 91% of the band instead of 48%. Debug weight images themselves pass
through tone mapping; CSV `mean_history_weight` inverts that mapping explicitly.

Native offscreen GIFs sample every eighth scene step, slowed for inspection:

| Path | Rejected baseline | Corrected |
| --- | --- | --- |
| Stationary | [before](images/taa-edges/static-before.gif) | [after](images/taa-edges/static-after.gif) |
| Camera translation | [before](images/taa-edges/camera-before.gif) | [after](images/taa-edges/camera-after.gif) |
| Independent object | [before](images/taa-edges/object-before.gif) | [after](images/taa-edges/object-after.gif) |

These are 320x180 native fixture readbacks, not editor screenshots. Full FP32
frames, phase/jitter CSV, diagnostic ablations and logs are retained under
`build/taa-edge-investigation/`. The old shader fails the new edge gates.
Successive ablations identify contrast responsiveness, motion dilation, cubic
history and stationary feedback separately; none is labelled a final acceptance.

The held-out sloped perspective translation improves from .1424 error/.1691
residual change before the depth correction to .1139/.0674 after it; independent
object motion improves from .1403/.1942 to .0995/.0756. All sloped sequences pass
the same unrelaxed gates, including soft width (maximum 1.58 pixels). Compare
[before the slope correction](images/taa-edges/sloped-before.gif) and
[fully corrected](images/taa-edges/sloped-after.gif); that before clip is the
intermediate candidate, not the original baseline.

## Qualification and limits

[The correction manifest](taa-edge-investigation.json) records final gates,
source/artifact identity and full metrics. `scripts/macos/taa.py --edges` applies
local regression criteria declared after diagnostic ablations and before final
Debug/Release acceptance: improved geometric error, stationary convergence and
residual change, improved moving residuals, and bounded soft width. The separate
ordinary TAA fixture retains HDR responsiveness, fine texture, masks, photographic
specular motion, cuts, disocclusion, all AO presets, alpha/world-2D composition,
exact IDs/depth/UI, cancellation, resize and retirement gates.

Fine-texture RMSE is .009781 versus .014621 in the rejected revision and .029909
with TAA off; contrast is 91.4% of the independent 4x-resolution reference.
Registered moving-floor RMS ratios on/off are .796 Low, .765 Medium and .888
Ultra, compared with .830/.805/.911 previously. This preserves the AO benefit;
physical occlusion changes remain in that metric, so it is not an AO fidelity
oracle. XeGTAO sampling and diffuse-only integration are unchanged.

The edge remains somewhat wider than ideal: approximately 1.14 pixels stationary
and 1.41–1.58 moving, versus .92–.93 analytically. Stationary residual variation is
small but nonzero. Very thin subpixel features, fast disocclusions and specular
trails need owner re-review on real scenes; finite/interior tests do not establish
that every such artifact is absent. The quantitative and native-frame evidence
shows geometric antialiasing genuinely improves in the exercised cases. Final
visual acceptance remains **INCOMPLETE** pending that review.

The owner's contact scene and project edits are preserved byte-for-byte. Native
tests use an owned snapshot of `4c25138`; expanded frozen OpenGL probes retain the
existing `86c581c` fixture provenance and its documented baseline hash drift.
Windows/Linux execution and the historical Windows full-suite timeout remain
outside this Metal-specific correction. No merge, release or backend change.

## Executed acceptance and cost

The costs below describe the correction checkpoint. The subsequent
[performance follow-up](taa-performance.md) records an image-preserving
optimization, newer measurements and its completed automated qualification.

Debug/Release builds, front-facing/sloped edge fixtures and HDR/AO fixtures pass. Native API/GPU validation
reports zero errors; 1x/2x lifecycle and independent two-view presentation pass
in both configurations. Three-slot serialized-image equivalence and complete
retirement pass. Focused native OpenGL RuntimeSession probes pass with the
established immutable fixtures. Fresh signed source-free editor/player bundles,
TAA relocation/missing-library rejection and pristine Metal product launches pass.
No new first-party compiler warnings were emitted. Public documentation links
are audited at this correction checkpoint.

The accuracy correction costs GPU time: 1080p Medium+TAA whole-submission median/
p95 is **11.932/12.055 ms**, versus 10.154/10.255 previously. Its resolve interval
is **5.827/5.890 ms**, versus 4.043/4.120. At 720p, whole GPU is 5.381/5.438 ms
and resolve 2.596/2.632 ms. These are sequential native presented Release runs
with 120 warmup and 20 tail frames excluded; only positive presentation feedback
is used. Completion intervals may overlap and must not be summed. Full CPU/GPU,
memory and pacing distributions for AO Off/Low/Medium/High/Ultra are in the
manifest. Resource allocation counts/storage remain bounded, with no new history
textures or CPU waits. Ultra+TAA remains expensive (19.381 ms median at 1080p);
this correction prioritizes correct reconstruction over hiding defects with blur.

The full five-preset GPU matrix was measured after the final shader correction.
The subsequent CPU camera-cut correction leaves that static GPU workload and
shader bytes unchanged; Medium was additionally remeasured on the final CPU
implementation. Scene CPU median/p95 is .072/.087 ms at 720p and .071/.085 ms
at 1080p. The manifest distinguishes both checkpoints. All 32 edge sequences,
HDR/AO fixtures, current native OpenGL probes, 2x lifecycle, independent views
and source-free products passed after the CPU correction. A missing generated
Xcode test scheme was restored using tracked `hazel.py generate --tests`; its
failed build attempt is retained, not counted as a feature failure or hidden.
