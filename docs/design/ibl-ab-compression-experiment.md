# Bounded A/B angular compression experiment

Pre-fit declaration, 2026-11-13; starting HEAD `7f2885073373874fdd5b2986f6ee5aac65293dc8`.
This is an offline rejection/qualification experiment, not a production change.

The candidate projects the separate transfer kernels K(1-Fc) and K Fc into
orthonormal real spherical harmonics in the local frame Z=effective Ns,
X=normalize(V-Ns*(Ns.V)), Y=Z cross X. At normal incidence use a deterministic
orthogonal X. Isotropy removes sine modes in this frame. Rank means 8, 16 or 32
retained cosine/m=0 modes per kernel, ordered by degree then m=0..degree;
the last band may be partial. These are linear angular subspaces, not SVD ranks
or claims about every possible rank-32 representation. Maximum degree is 7.
Rotation uses complete world-space SH bands (16/36/64 RGB coefficients), so a
partial local band never implies a partial world band. No radiance clipping,
positivity clamp, metallic fitting or energy renormalization is permitted.

Fit only the authored GGX/correlated-Smith/Schlick transfer, r floor .045,
alpha=r*r, with deterministic FP64 NDF-CDF integration. Store FP32 A/B tables
at N.V={.0005,.002,.01,.02,.05,.1,.2,.35,.5,.7,.85,1} and
r={.045,.05,.1,.2,.35,.45,.6,.75,1}. Interpolate four neighbors explicitly in
FP32 in N.V/r; evaluate controls at exact nodes and midpoint nodes, plus direct
coefficient projections to isolate interpolation error. Lighting projections
and rotation evaluation use FP64 as an optimistic screening bound; count FP32
runtime storage. Fit has no Studio, Axes, TinyEmitter, surface or material data.
Axes and TinyEmitter are held-out lighting. Actual/orbit normals are captured
effective normals, all directions in environment coordinates. Gate by Ng.V;
integrate the full shading incoming hemisphere, without Ng.L truncation.

Before fitting: **per-environment ceiling 1 MiB; shared runtime ceiling 1 MiB;
peak process RSS 512 MiB; each heavy child <=900 seconds, one at a time**.
No unlimited orientation/view/roughness texture grid is allowed. At rank 32:
64 RGB FP32 environment coefficients=768 bytes; two 12*9*32 FP32 tables=27,648
bytes; 21 axis FP32 nodes=84 bytes. Harmonics recurrence/rotation is analytic,
no hidden sampled angular bank or source HDR at runtime. Existing background /
diffuse resources are unchanged and additional to this specular representation.
Offline source decode <=64 MiB, <=2048*1024 RGB source pixels; rows <=1024,
chunks <=65536 angular points, max fit order 512. Disk evidence ceiling 64 MiB
excluding the existing capture and isolated Python environment.

Frozen gates: floor-relative RGB RMS <=.02, max <=.05, denominator
max(reference,.02); channels below .02 also absolute RMS <=.0005 and max <=.002.
Independent reference convergence max <=.005 for r<.06 and <=.001 otherwise.
Unconverged required rows block promotion. Retain exact worst inputs, group
errors and convergence flags; no threshold changes after candidates are seen.
First refine TinyEmitter sharp rows 9/15/18 with localized FP64 source quadrature
and an independently checked constant-field term. Then assess every required
row, including stored nodes, view/roughness midpoints, metals/dielectrics/mixed,
grazing/sharp/intermediate/r=1, rotations/seams, 150 actual pixels, 80 orbit
pixels and positive-view saved sweep rows. Negative-view rows are domain controls.

Only a fully converged accuracy/storage pass authorizes a test-only GPU step.
Count all <=64 reconstruction fetches (A/B, coefficients, four-node interpolation
and rotation), ALU/bandwidth, then measure full native M4 GL4.1 1280x720 AA-off
frames: median <=16.67 ms, p95 <=20 ms. A dispatch proxy cannot qualify it.
Failure means no GPU/product/format changes and an unresolved visual gate.

Reproduction tooling and measured results will be recorded below after execution.
