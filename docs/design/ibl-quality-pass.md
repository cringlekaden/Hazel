# Renderer organization and IBL quality pass

Implementation in progress on `feature/3d-foundations`; Stage 5 has not begun.

Pre-tuning criteria (2026-11-06, ddeb1ef/bb1fdf7 baseline)

All errors are linear HDR, same base RGB (.18,.4,.7), metallic 0/.5/1,
geometric normals, AO=1, no direct key, fixed camera/EV0/yaw0. Test Photo,
constant C and a one-pixel 10,000-radiance emitter, plus yaw/moving-view offsets.
Separate diffuse/specular/combined output and inspect every prepared level.

Constant piC/C <=1e-5. No diffuse contribution for metal=1 (GPU <=1e-6).
At actual convolved levels r>=.25: independent source quadrature must converge
within .2% peak; target RMS <=1%, 99th percentile <=3%, maximum <=5% of the
reference peak, separately per level/direction set. Energy <=1% relative.
For sharper levels measure integration convergence, peak/location and half-height
lobe width; target <=one source-cell/one output-texel angular sampling error.
Measure off-lobe spikes, not only averages: no error >5% peak or isolated second
maximum above the converged lobe. Compare rotation-adjacent samples against the
same independent integral. Cube face/edge samples use a common direction oracle.
Fractional GPU LOD must interpolate distinctive known levels within FP32 .0001;
CPU artifact -> GPU face/level readback exact FP32. Exposure/UI/picking and direct
paths retain their existing bounds. No tone-curve/exposure/material tuning.

If a proposed remedy misses these targets, investigate and report it; do not
silently relax them. Quantify roughness-level/spatial representation error and
split-sum model error independently from estimator error.

## Baseline evidence

At `ddeb1ef`, Photo diffuse had 7.77% RMS / 27.67% maximum error relative to
the sampled reference peak; roughness-1 specular had 9.08% RMS / 15.50% maximum.
The 256×128 tiny-emitter diffuse case had 49.7% RMS / 183.2% maximum error.
These are bake-data errors before GPU sampling or tone mapping. Independent
FP64 source-cell quadrature uses the bilinear radiance field and exact cell solid
angles; convergence and all roughness levels are being qualified separately.

The organization-only checkpoint is `bb1fdf7`, with portable includes corrected
in `32d5ecd`. Existing focused 2D/direct/IBL checks passed before and after the
split. [Follow a frame](../follow-a-frame.md) describes the current entry points.

Canonical CI 42318229171 passed Linux and hosted macOS. Windows passed Debug/Release
software and Release GL4.1 (18 native executables per profile), then hit the
unchanged 480-second Debug gameplay timeout. Its last log reached MainMenu at
01:19:51 UTC, after ObservatoryDawn at 01:19:42; no failing assertion preceded
the timeout. This is separate duration debt, not a Stage-4 renderer failure.
