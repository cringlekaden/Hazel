# Multiscattering_v2 reference family

Additive rendering-model version: reciprocal Kulla–Conty, scalar complementary
diffuse and converged RuntimeBRDF_v2 A/B. Historical IBL_v1/Direct_v1/Surface_v1
assets, bake identities and outputs remain immutable. Multiscattering_v1 records
the rejected legacy-LUT experiment; it is not silently promoted or overwritten.

[Conditions](conditions.json) specify the frozen fixture revision, materials,
roughness rows and exposure. [Evidence and limits](../../../../../../docs/design/ggx-multiscattering.md)
include linear FP32 direct/diffuse/specular components and native moving-light
captures. PNGs are clamped SDR previews, not numerical references or product
screenshots. Owner visual acceptance remains required.

Reproduce using `scripts/macos/ggx_multiscattering.py --config Release --project
FROZEN/RenderingReference-IBL-Photo.hproj --output FRESH`. `--single` pairs the
historical shader and original A/B inside private qualification resources only.
Production has one current model and no runtime quality toggle. See the design
record for the exact frozen project revision and commands.
