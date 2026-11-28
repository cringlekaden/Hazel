# Pavilion glTF fidelity audit

The checked-in architecture and Flight Helmet are the only glTF-family inputs.
The helmet's [download ledger](Assets/Pavilion/asset-downloads.json) pins the
Khronos source to `4995a638`. I inspected both original JSON and the GLB JSON/BIN
chunks, and followed the compiled mesh slots into their native material files.
The authoring contract is [GLBSemantics.h](../../tools/HazelImport/GLBSemantics.h),
the adapter is [GLB.cpp](../../tools/HazelImport/GLB.cpp), and shading is owned
by [ForwardPBR.glsl](../../Hazel/Resources/shaders/ForwardPBR.glsl).
TinyGLTF's ability to decode a field does not confer admission or render support.

## Asset inventory

| Source | Geometry and hierarchy | Image, sampler and material mapping |
| --- | --- | --- |
| Original Flight Helmet | Six named roots, six indexed triangle meshes, identity node transforms. POSITION, NORMAL, TANGENT and TEXCOORD_0. No skins, morphs, animations, cameras or compression. `extensionsUsed` contains only `KHR_materials_transmission`. `extensionsRequired` is absent. | Six opaque materials. The hose is double-sided. Hose and stand share three textures. The four remaining sets each have base color, tangent-space normal and ORM (shared occlusion and metallic/roughness). One sampler uses linear magnification and trilinear minification, default repeat. Fifteen external PNGs, twelve 2048² and three 1024². The lens base image is RGBA. Transmission factor is 1 on the lens, with no dedicated transmission map. |
| Checked-in Flight Helmet GLB | Six roots and original geometry attributes. Both extension arrays are absent. Native prefab adds a detached parent. Scene placement adds a metre-scale world transform. | Embedded 8-bit JPEG at 1024². Five textured materials retain UV0 semantics and sampler policy. The lens uses opaque metallic bronze with roughness .16 and metallic .82, without its original three maps. Native authored references retain twelve RGB PNGs. The GLB retains the three unused lens images. RGBA was converted to RGB at the conversion step. The original lens was opaque for alpha coverage, with transmission carried independently by the extension. |
| Pavilion architecture GLB | 21 indexed triangle mesh nodes, positioned in shared architectural coordinates except the local ring parts. POSITION, NORMAL, TEXCOORD_0. Hazel cooks Mikk tangents. No extension lists or deformation, cameras, animations or compression. | The GLB contains no images or explicit materials. The authoring script supplies native material overrides, photographs and generated signage by slot. The courtyard grass mask is a native alpha-cutout override. The title fascia repair resides in scene transforms and an extra native cube binding. |

The JPEG conversion is lossy even for normal and ORM maps. Comparing Pillow's
decoded JPEG to a Lanczos-resized original gave **0.98–2.37 RGB code values
of mean absolute error** across the twelve referenced images. Decoder rounding
can differ. This comparison measures conversion loss and does not compare full
shading outputs. Native RGB storage and bindings separate sRGB base color from
linear normal and ORM reads. The existing
[import qualification](../../docs/design/3d-import-stage2.md) documents
geometry and tangent, signed transform and image-channel tests.

## Capability matrix

“Rejects” means the native GLB semantic gate explicitly declines the input,
including an unsupported optional extension. “Native only” identifies an
engine feature with no current glTF translation.

| Feature | Present in Pavilion source? | Hazel importer | Runtime | Fidelity/loss | Priority |
| --- | --- | --- | --- | --- | --- |
| Core metallic/roughness, base, ORM, normal, emissive | Helmet base/ORM/normal | Admitted static subset, UV0 | GGX, correlated visibility, multiscattering, diffuse/specular IBL | Main helmet finish retained with texture conversion loss | Maintain |
| Color, alpha, UV0, mip/wrap | Helmet RGB maps and one RGBA lens map | Embedded PNG/JPEG. Opaque and mask, UV0, repeat/clamp/mirror | Slot-specific linear/sRGB sampling and mip generation | Opaque alpha ignored for coverage. Texture transforms and other UV sets require separate admission | Maintain |
| Tangents and geometry | Helmet supplied, architecture missing | Preserved or generated with Mikk. Float/static and supported normalized attributes | Signed transforms, mirrored winding, tangent frames | Native representation, no deformation | Maintain |
| Node hierarchy and transforms | Independent source roots | Signed TRS and decomposable affine matrices | Native hierarchy | Detached root. Shear and singular inputs rejected | Maintain |
| Double-sided and alpha mask | Hose double-sided. Grass native mask | Admitted for core materials | Raster, masked depth/shadows and picking | Retained | Maintain |
| Alpha blend | Absent | Rejects | No transparent PBR submission/sorting | No depth/coverage transparency | P2 |
| `KHR_materials_unlit` | Absent | Admitted | Unlit material | Core static support | Maintain |
| `KHR_materials_emissive_strength` | Absent | Rejects | Native `.hmat` strength in [0,1000] | Translation gap. Pavilion practicals use native strength | P0 |
| `KHR_texture_transform` and UV1+ | Absent | Rejects | Material paths sample UV0 | No per-slot transform or UV selection | P1 |
| `KHR_materials_variants` | Absent | Rejects | Native slot replacements, no named glTF switch set | Variant identity and mappings unavailable | P1 |
| `KHR_lights_punctual` | Absent | Rejects | Native directional, point and spot lights | Need glTF units/range/intensity mapping | P1 |
| Embedded cameras | Absent | Rejects | Perspective/orthographic and physical camera | Projection and hierarchy need adapter | P1 |
| `KHR_materials_ior`, `KHR_materials_specular` | Absent | Rejects | Fixed dielectric Fresnel response in the qualified base model | Variable dielectric F0 and specular color/weight unavailable | P2 |
| `KHR_materials_transmission` | Lens in original | Rejects | No BTDF or refractive PBR pass | Intentional opaque substitute. Largest visible source loss | P2 |
| `KHR_materials_volume` | Absent | Rejects | No thickness/absorption path | No Beer–Lambert attenuated transmission | P2, after thin transmission |
| `KHR_materials_clearcoat` | Absent | Rejects | No independently rough upper lobe or coat normal | Layering unavailable | P2 |
| `KHR_materials_anisotropy` | Absent | Rejects | Isotropic GGX | No directional coat or brushed lobe | P3 |
| `KHR_materials_sheen` | Absent | Rejects | No Charlie fabric lobe | Fabric receives core roughness only | P3 |
| `KHR_materials_iridescence` | Absent | Rejects | No thin-film Fresnel | No angle-dependent interference color | P3 |
| `KHR_texture_basisu` | Absent in selected source | Rejects | No KTX2 transcode path in native project loader | Upstream alternate helmet is a separate asset | P1 |
| `KHR_draco_mesh_compression` | Absent | Rejects | Uses cooked geometry | Producer decoder gap | P1, after corpus |
| `EXT_meshopt_compression` | Absent | Rejects | Uses cooked geometry | Producer decoder gap | P1, after corpus |
| Animation, skinning, morphs | Absent | Rejects | Runtime transforms and sprite animation exist. No imported 3D clips, skin palettes or morph buffers | Rigid motion supported. No deformation or historical deformed vertices | Separate milestone |

## Implementation sequence

1. **Translation gaps.** Add emissive strength first, using the native scalar
   to retain scene-linear ratios through the HDR and bloom passes. Validate
   absent/present defaults, texture multiplication, invalid ranges, archive
   survival and parity with equivalent authored material. Test
   [EmissiveStrengthTest](https://github.com/KhronosGroup/glTF-Sample-Assets/tree/main/Models/EmissiveStrengthTest).
   Aim for identical runtime GPU cost because the native multiplier is already
   part of material submission.
2. **Asset inputs.** Version texture bindings for UV set and affine transforms,
   including correct derivatives and tangent basis for transformed normals.
   Test TextureTransformTest and MultiUVTest with CPU UV probes, mirrored charts
   and equivalent rasterized references. Separate named variants and camera/light
   translation from this shader change. Verify punctual photometric units and
   spotlight half-angles before mapping them to Hazel's relative calibration.
   Keep Draco and meshopt decode in the producer with allocation limits,
   malformed corpora, cancellation and native digest equivalence. Qualify
   BasisU separately with linear/sRGB target format selection, normal errors,
   mip isolation and measured transcode, upload and residency.
3. **Dielectric foundation and layering.** Introduce IOR/specular without
   changing default F0 or frozen GGX/Kulla–Conty references. Test FP64 Fresnel,
   reciprocity, smooth/grazing limits and white-furnace energy for direct, global
   and local IBL. Clearcoat then needs Fresnel attenuation of the base, an
   independent coat normal/roughness, and reflection handling for two lobes.
   Benchmark added BRDF evaluations, map traffic and IBL fetches by feature
   and with disabled materials. Use Khronos IORTest and ClearcoatNormalTest.
4. **Thin transmission for the helmet.** Model reflection plus transmission,
   use Snell and dielectric Fresnel with total internal reflection, and
   explicitly own a pre-transmission scene color/depth source and off-screen
   fallback. Validate sorted coverage, picking, shadows, SSR coexistence,
   TAA/motion and probe capture contracts. Test TransmissionTest before
   adding volume thickness and Beer–Lambert attenuation with VolumeTest.
   Profile full-resolution copies/resolve, extra samples and overlapping
   layers at 1080p/4K. Keep alpha blend as a separately qualified coverage
   model with ordering, HDR composition and explicit depth rules.
5. **Further BSDFs and deformation.** Anisotropy needs energy-aware elliptical
   GGX, stable tangent direction and an IBL approximation qualified against
   integrated angle sweeps. Sheen needs a fabric lobe with base attenuation.
   Iridescence needs thin-film Fresnel with verified wavelength/color
   integration. Each gets independent furnace, colored environment, shadow,
   motion and memory gates. For deformation, qualify authored-time
   interpolation, bounded sparse/layered data, normals and previous deformed
   positions across shadow, picking, TAA and shutter passes before expanding
   runtime support.

Filament's current published support includes core glTF and many of these
extensions, with separate material-system contracts for layering, anisotropy
and transmission. Its support list is implementation evidence for Filament's
own pipeline. Use feature-specific Khronos cases to qualify Hazel.
[Filament input support](https://github.com/google/filament/blob/main/README.md) ·
[Filament materials](https://google.github.io/filament/Materials.md.html) ·
[Khronos glTF specification](https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html) ·
[Extension registry](https://github.com/KhronosGroup/glTF/blob/main/extensions/README.md)
