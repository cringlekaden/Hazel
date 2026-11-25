# Multiscattering_v1

Additive numerical/visual reference family. Historical Direct_v1, IBL_v1 and
IBLQuality_v2 source data, hashes and results remain unchanged.

`tests/macos/MultiscatteringQualification.h` renders immutable Direct_v1 cube and
sphere geometry through the production scene renderer. Private material definitions
cover gold (.95,.64,.15), copper (.955,.638,.538), aluminum (.91,.92,.92), dark (.02)
and white dielectric, mixed metallic (.2,.4,.7; m=.5), and a perfect reflector.
Colors are linear Rec.709 RGB approximations, not spectral optical-constant fits.
The analytic study sweeps r=0/.045/.046/.25/.5/.75/1 and NoV=.01/.05/.25/.5/1.
It captures direct, diffuse IBL and specular IBL separately in linear FP32,
checks intrinsic GPU reciprocity and perfect-reflector furnace energy, then captures
28 spheres under photographic IBL and eight moving directional-light poses.
No AO/TAA changes or project/material format additions are part of this family.

Run `scripts/macos/ggx_multiscattering.py` with an explicit immutable project snapshot,
configuration and fresh output directory. `--single` selects the historical shader
only inside this qualification runner, using the same current executable and packed
resources. It is not a production BRDF toggle. CSV columns distinguish exact-table
implementation expectations from the continuous FP64 model. FP32 component binaries
are authoritative; PNG component previews clamp to SDR. Shaded previews use the
production Neutral presentation at exposure zero. Sphere columns follow the material
order above; rows progress from polished to rough, top to bottom.

Acceptance and retained measured evidence: [design record](../../../../../../docs/design/ggx-multiscattering.md).
The intrinsic direct-energy gate currently FAILS with the preserved LUT. This
family is uncommitted WIP, not a qualified production reference. Owner visual
acceptance remains distinct from numerical correctness.
