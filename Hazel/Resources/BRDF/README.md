# Runtime BRDF data

`GGX-MS_v2.hlight` is the active, engine-owned RuntimeBRDF_v2 table.
`GGX-MS_v2.json` records its recipe, coordinates, integrator, source hash,
producer and payload identity. The native maintainer command is
`HazelProject prepare-runtime-brdf OUTPUT.hlight 256`; it refuses existing outputs.
Runtime does not bake. EnvironmentResources packs A/B and the exact hemispherical
averages of the endpoint-grid interpolant into RGBA32F.

`GGX-correlated-AB.hlight` is an immutable historical qualification copy of
RenderingReference's accepted IBL_v1 128x128 table, from
`Assets/Prepared/BRDF/8916c405c4da491ac3abaccd5f4da974be8b228135e47a81ac41ff1856d5ce94/bb4673afa8c745fb.hlight`.
It is not the active production table. Historical shaders/data are paired only
inside qualification fixtures; production has one current BRDF implementation.
