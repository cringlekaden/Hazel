# Studio surface investigation (2026-11-07)

**Correction:** the subsequent [native silhouette investigation](silhouette-normal-fix.md)
shows that rejecting negative shading N·V blacked out visible geometric samples.
The zero-radiance sliver described below was not an acceptable visibility repair.
The native fix resolves shading normals against geometric visibility; this record
retains the earlier measurements and the separate split-sum model limitations.

Baseline: `d4c4330`, Apple M4, OpenGL 4.1 Metal–91.7. Stage 5 has not begun.
**The implementation defects below are repaired; the requested full-BRDF visual
acceptance is not established.** The remaining rough-copper dark patch needs a
separate, measured view-dependent IBL model change before calling it solved.

## Fixed conditions and method

Use the existing `RenderingShowcase-SurfaceDetail.hproj` camera at 1280×720,
EV 0, yaw .349999994, gain 1, original materials, normals and AO. No reference,
showcase scene, material, HDR or accepted lighting bytes changed. No image was
attached in this session; SurfaceDetail supplies the repeatable close view.
Studio Small 09 source SHA-256 is
`e7cfda5f4e98e623db12b8bfd0184e048488e4855d9c83e2751fb44a32e80c45`;
accepted High generation is `d030bea247f14dd3`. Existing provenance is in the
[showcase guide](../../examples/RenderingShowcase/README.md).

`IBLSurfaceChecks.h` captures isolated FP32 terms, prefilter/LUT inputs, actual
position and normals, IDs, dark-background controls, no-normal-map wood,
untextured dielectric/conductor and constant-lighting controls. Thirteen views
over a 6.9° controller orbit retain the same exposure and lighting. Five views
also retain numeric probes. The existing editor scene entry and player session
are compared on the same authored view; runtime handles are matched by UUID.

The independent streaming FP64 oracle integrates the original **bilinear HDR
field**, using exact source-cell solid angles, two/four subdivisions and the
full single-scattering GGX/Schlick/correlated-Smith BRDF. It shares no production
CDF, MIS, convolution or GPU sampling helpers. Results include the separate
normalized prefilter and geometric-horizon integral. Evaluate RGB RMS/max in
linear HDR, plus relative error with a .02 radiance floor. Do not divide dark
error by an unrelated bright peak. Sharp r=.05 source quadrature did **not**
converge and is excluded from claims of full-BRDF accuracy.

The maintained GPU gate requires off-center/seam reconstruction within .6% of
the independent prefilter, an independent LUT bound of .01, zero reflection for
N·V<0, and retained positive grazing response. Existing constant/furnace, direct,
fractional-LOD, exposure/UI/picking gates remain unchanged.

At five orbit poses, eight actual copper directions per pose have accepted
prefilter RMS .000202–.000485, versus .017673–.022200 before; maximum accepted
outlier is .000763. Full-BRDF RMS remains .045373–.080712. Reference convergence
is below .000000761 maximum RGB RMS. Ordered keyframes show smoother
interpolation transitions but retain the broad moving dark region. That remaining
region is **not** justified by the full reference. This is an automated native
controller/GPU sequence and inspected keyframes, not a physical mouse test.

## Rough copper: representation defect and model limit

The roughest specular mip is 4×4 per face. Its stored texel values are accurate,
but bilinear reconstruction introduces bands/kinks between them. The actual
uploaded chain is RGB32F, explicitly supplied, byte-checked before/after draws;
no later pass replaces it. This is not texture quantization, final encoding,
alpha compositing or the separately tested M4 fractional-LOD problem.

At r=1 the normalized V=N GGX prefilter is exactly E/π. ForwardPBR now samples
that same signal from the existing 64×64 irradiance cube, in the **existing
reflection direction**, at the terminal level. Integer-level FP32 interpolation
remains explicit. Earlier levels, the BRDF LUT, exposure and HDR energy are
unchanged. No final-frame blur, new bake, format/cache revision or extra resource
is needed. Diffuse illumination is not added to metals by this reuse.

At 36 actual rough-copper pixel directions, including between texel centers:

| Linear RGB error | Before RMS / max | Accepted RMS / max |
| --- | --- | --- |
| Sampled prefilter vs independent convolution | .017850 / .051593 | .000253 / .000701 |
| Final specular vs full BRDF integral | .055250 / .116246 | .058613 / .121700 |
| Full BRDF relative error, .02 floor | .254397 / .578791 | .269111 / .615313 |

Full-BRDF quadrature RMS convergence is .00000023. The cleaner reconstruction
**does not fix** the larger split-sum error and removes some accidental error
cancellation. In particular, the broad dark region still differs from the
reference. A cleaner crop or the prefilter improvement alone cannot pass the
owner's rough-sphere acceptance condition.

In the dark region at pixel (902,567), N·V=.7927 and roughness=1, the independent
RGB is (.239626,.152485,.127632). Baseline output is (.150996,.095336,.078387);
accepted output is (.144212,.090921,.074604). The discrepancy first appears in
split-sum reconstruction, before exposure, tone mapping or display encoding.
It cannot be described as removed mottling relative to the full reference.

The actual LUT convention remains x=N·V, y=perceptual roughness, channels A/B.
At r=1 it clamps to the last stored row center, .99609375; its finite endpoint
representation contributes about 1.07% scale difference at N·V=.8. The prefilter
regression independently filters the accepted CPU LUT to separate that admitted
table error from angular reconstruction; full-BRDF comparisons use the exact
authored roughness and no LUT. Neither LUT contents nor its identity changed.

A normal-directed rough endpoint visually removed the dark region and reduced
this copper full-BRDF RMS to .035938. It was **rejected**: at positive N·V in the
controlled rough-dielectric sweep it increased r=1 RMS from .007117 to .014768.
A wider dominant-direction correction also increased the r=.45 ideal-prefilter
model RMS from .069766 to .107543. Neither alternative ships. Changing the full
view-dependent lobe representation requires a coherent model investigation,
not more bake samples or a metallic-dependent cosmetic tint.

## Wood: valid Fresnel, invalid-domain sliver, residual approximation

Changing only the displayed background to black leaves sphere lighting
unchanged. Covered mesh pixels are opaque, depth/ID resolved; background color
does not leak into the HDR specular term. Removing the wood normal map barely
changes the mean edge reflection. A constant environment retains the neutral
grazing response: dielectric F0=.04 approaches neutral F90=1, while diffuse
falls at grazing angles. This part of the pale rim is expected.

At this checkpoint, smooth/interpolated normals, even without a normal
map, cross N·V=0 at some silhouette samples. Clamping the LUT coordinate to zero
still emitted reflection for **negative** shading N·V. The checkpoint rejected
that reflection without first repairing the shading frame. Native geometric
readbacks later proved that this created black foreground pixels; the correction
linked above supersedes that interpretation and implementation.
At that checkpoint's close view 23 wood pixels and 27 rough-copper pixels were
affected; their specular output was forced to zero. Wood edge mean specular
luminance changed .318495 → .303384. Geometry/IDs and positive-N·V wood values
were unchanged. Those black foreground samples are repaired by the subsequent
geometric-normal correction, not retained as valid zero-radiance samples.

Representative raw wood pixels (top-down coordinates):

| Pixel / N·V | Before RGB | Independent RGB | Accepted RGB |
| --- | --- | --- | --- |
| (572,309) / −.0441 | (.5259,.5147,.5159) | (0,0,0) | (0,0,0) |
| (612,200) / .0646 | (.4536,.4476,.4549) | (.4627,.4586,.4701) | unchanged |
| (619,194) / .0957 | (.4027,.3985,.4068) | (.4019,.3994,.4101) | unchanged |
| (585,267) / .3898 | (.1791,.1797,.1860) | (.0829,.0833,.0873) | unchanged |

The .0646 sample has .0120 RGB RMS error; its neutral color is predicted by the
reference. Geometric-horizon clipping changes it by only .0000025 RGB RMS.
The last row exposes significant existing high-frequency split-sum error away
from the strict silhouette. It is not fixed by the domain guard or by removing
the normal map. The controlled positive-N·V dielectric sweep at r=.2/.45/.75
has .122756 RMS error at N·V=.02 and .079990 at .1, unchanged by this repair.
Those are model limitations, not a claim of unexplained halo acceptance.

## Reproduce and inspect

From `/Volumes/External/kadencringle/Projects/Hazel`:

```sh
python3 scripts/hazel.py run Nutella --config Release --project examples/RenderingShowcase/RenderingShowcase-SurfaceDetail.hproj --inspect-camera
python3 scripts/hazel.py run Hazelnut --config Release --project examples/RenderingShowcase/RenderingShowcase-SurfaceDetail.hproj
```

Use Play in Hazelnut for the authored view. Home restores it during inspection;
the existing in-viewport hint describes orbit/fly/release. Look for remaining
broad rough-metal variation as well as the removed interpolation bands. Preserve
the wood's neutral grazing reflectance; do not judge correctness by making it brown.

For numerical capture, build the maintained tests and create a disposable
committed Showcase copy using `internal.testing.projects.snapshot`, as the normal
runner does. Invoke `MigrationRuntimeSessionSmoke Core.dll Fixture.dll --ibl-surface`
with absolute `HAZEL_RENDERING_SHOWCASE_PROJECT`, `HAZEL_IBL_SURFACE_CAPTURE`,
private `HAZEL_DATA`/`HAZEL_SAVE_ROOT`, normal `HAZEL_RESOURCES` and the platform
Mono library path. This explicit diagnostic mode reports that other session
checks were not executed. It saves `capture.yaml`, raw bottom-up RGBA32F/ID32,
encoded PPMs, and a 384-row grazing/roughness input/result sweep.

An exact M4 capture invocation, after building Release tests:

```sh
python3 - <<'PY'
import os, subprocess, sys
sys.path.insert(0, 'scripts')
import hazel as hz
from internal.testing.projects import snapshot
out = hz.ROOT / 'build/ibl-surface-v1'
project = snapshot('RenderingShowcase', out/'project', 'RenderingShowcase.hproj')
base = hz.binaries('Release')
env = os.environ.copy()
env.update(HAZEL_RENDERING_SHOWCASE_PROJECT=str(project),
           HAZEL_IBL_SURFACE_CAPTURE=str(out/'capture'),
           HAZEL_DATA=str(out/'profile'), HAZEL_SAVE_ROOT=str(out/'saves'),
           HAZEL_RESOURCES=str(hz.ROOT/'Hazel/Resources'),
           DYLD_LIBRARY_PATH=str(hz.mono_prefix()/'lib'))
subprocess.run([str(base/'MigrationRuntimeSessionSmoke/MigrationRuntimeSessionSmoke'),
                str(base/'Hazel-ScriptCore/Hazel-ScriptCore.dll'),
                str(base/'MigrationManagedFixture/MigrationManagedFixture.dll'),
                '--ibl-surface'], env=env, check=True, timeout=240)
PY
```

The reference-only entry, in the same build, is:

```sh
DYLD_LIBRARY_PATH="$PWD/build/dependencies/mono/macos-arm64/sdk/lib" bin/Release-macos-arm64/MigrationRenderer2DSmoke/MigrationRenderer2DSmoke --ibl-reference examples/RenderingShowcase/Assets/Environments/Studio.hdr /absolute/capture/sweep-inputs.txt 4 /absolute/capture/reference4.txt
```

Repeat at subdivision 2 to measure convergence. Inputs are N.xyz, V.xyz,
geometric-N.xyz, base.rgb, roughness, metallic, AO, in environment coordinates.
Output columns are full specular RGB, geometric-horizon specular RGB, normalized
GGX prefilter RGB, current diffuse RGB, E(N)/π RGB. The source, input rows, working
set and sample product are bounded; existing output files are retained. This CPU
entry does not execute graphics regressions or load a producer in the player.

Local evidence lives under ignored `build/ibl-surface-evidence`: `before-final`
is the instrumented baseline, `accepted-final` is the shipped repair; `endpoint` and
`after-final` are explicitly **rejected normal-directed prototypes**. Keep these
labels when comparing with a future Metal implementation. No second renderer or
historical runtime selector ships.

## Qualification and remaining gate

Apple M4 native **Debug and Release Renderer2DSmoke and RuntimeSessionSmoke
passed**, two selected executables per configuration. These retain direct,
2D/color/text, fractional LOD, all uploaded faces/mips, constant/furnace energy,
AO, exposure-independent UI, masks/depth/picking, saved views, opt-in Gallery
orbit/approaches/reset and Play/Stop coverage. The full 18-executable campaign
was not repeated. No skipped graphics check is counted as a pass.

The production endpoint seam/off-center reconstruction maximum relative error
is .1271%, within its .6% regression bound. Private counterfactual runs fail the
intended assertions when restoring the old coarse lookup or removing the
shading-hemisphere guard. The final explicit integer-zero irradiance LOD and the
preceding equivalent non-mipped lookup produce **232 byte-identical raw/ID/PPM
capture files**; final captures record the shipped shader hash. The largest
editor/player interior display difference over all seven spheres is .002441;
copper/wood coverage is identical. A .005371 whole-image boundary difference
is retained separately, not hidden by the interior statistic.

Actual Release Hazelnut, fixed Nutella and inspection-enabled Nutella each
rendered 60 frames and shut down normally on the source-free SurfaceDetail copy.
A relocated content-only package passed the existing checkout/SDK denial,
loaded-library closure and signature checks, with no HDR/GLB/project DLL.
Final archive SHA-256:
`1a2c85489e20e569e46aad8835280ffb7a3d680ba11ebe9216b71aa9a911a541`.
No bake was run; accepted artifact bytes/size, preparation cost and cache
identities are unchanged. The 384-surface FP64 source integrations took 5.37s
(subdivision 2) and 20.21s (4), not environment-preparation time.

No canonical CI was polled or dispatched. Linux/Windows qualification and the
separately documented historical Windows gameplay timeout are not resolved or
claimed green here. Owner content, all frozen reference/showcase assets and
vendor pins are preserved. Of 354 initial file hashes, only the root `.DS_Store`
changed during the session; it was left untouched and uncommitted. Recovery
commit `bfbce81` remains available. Private profiles/saves and ignored evidence
were used throughout.

**Do not treat this as full visual acceptance or proceed to Stage 5 on that
basis.** The implementation regressions are fixed and reproducible; the rough
copper's remaining full-BRDF discrepancy and positive-grazing split-sum errors
need a separate coherent view-dependent IBL model checkpoint. More bake samples,
lower exposure, an arbitrary edge tint or the rejected direction correction
would not close that gate. Stage 5, Metal and new rendering paths were not begun.
