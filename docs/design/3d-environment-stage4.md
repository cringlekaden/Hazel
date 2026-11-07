# Stage 4 — offline HDR environments and image-based lighting

Implementation branch: `feature/3d-foundations`, canonical checkout
`/Volumes/External/kadencringle/Projects/Hazel`. This record is under qualification;
it does not close Stage 4 or claim Stage 5 runtime/export qualification yet.

The Windows Stage-3 entry failure was frozen fixture extraction, repaired in
`8666bcb`. [Run 42291096669](https://github.com/cringlekaden/Hazel/actions/runs/42291096669)
passed Linux/hosted macOS and Windows's three complete native profiles. Windows
then reproduced the separate Debug gameplay 480-second timeout, reaching
LastLightkeeper Observatory without a failing assertion. See the Stage-3 record;
no assertions, limits or timeouts were weakened.

`.henv` owns an image source reference, explicit linear Rec.709/D65 interpretation,
preparation dimensions/sample count and attribution. Scene presentation owns the
selected environment instance, inverse-lookup yaw in radians, linear gain and
independent lighting/background enablement. Project defaults seed new scenes;
they never override existing scenes. Exposure remains a scene presentation value.
An image background needs a primary runtime camera; camera-free UI can use the
solid background. Editor navigation remains independent.

The producer first decodes bounded Radiance RGBE using the existing pristine
stb_image 2.23 implementation. Framing is checked before decode because its raw
RGBE path does not diagnose every short read. Supported layouts are raw RGBE or
scanline RLE, `-Y height +X width`; explicit alternate primaries/color correction/
gamma, XYZE, legacy repeat encoding, mixed scanline modes and trailing/truncated
payloads reject. Source limits are 64 MiB, 8192×4096 / eight million pixels,
128 MiB decoder allocation and finite nonnegative RGB <=1e10. Pixels remain
linear float RGB; there is no 8-bit conversion, alpha or sRGB transfer.

`RadianceImage` is the engine-owned normalized radiance boundary. The common CPU
preparer consumes it without filenames/parser objects. Only the image adapter
and offline policy exist. No atmosphere, probe, scheduler or backend registry was
added. `PreparedEnvironment` contains accepted portable resources and provenance,
independent of image decoding, source files and graphics bindings.

The versioned IBL recipe matches direct single-scattering GGX: perceptual roughness
floor .045, alpha=r², height-correlated Smith visibility and Schlick Fresnel.
Diffuse stores irradiance E and shading divides by pi once. Specular mips use
normalized GGX/NoL convolution with V=N, explicit unblurred mip zero and roughness
k/(levels-1); no generated mipmaps. Runtime uses evaluated r*(levels-1).
The shared 128² RG32F LUT uses 4096 deterministic samples; x=NoV, y=roughness,
channels A/B, reconstruction F0*A+B. Diffuse uses `(1-metallic)*(1-Fview)`.
AO attenuates indirect terms only. No ambient multiplier or energy compensation
conceals single-scattering loss or split-sum/diffuse approximation error.

Default preparation requests 256 background faces, 128 specular faces with all
eight mips, 32 irradiance faces and 1024 convolution samples. Cubes use RGB32F;
linear FP32 avoids undocumented half-float range conversion and costs about
6.2 MiB of portable pixels per default generation including the shared LUT. Quality alternatives
are explicitly prepared power-of-two sizes/samples within bounded limits.
The small constant-radiance starter uses 16/16/8 faces because its exact uniform
meaning needs no high spatial resolution. Native project creation copies and
CPU-validates bundled prepared data without a baker, importer, GPU or script SDK.

`.hlight` encodes explicit little-endian IEEE754 fields, version/kind/dimensions/
channels/faces/mip count/payload length and SHA-256. The decoder checks lengths,
complete chains, finite representable values and bounds before accepting data.
A `.henv.accepted` descriptor atomically selects one coherent immutable generation.
Generation UUIDs are distinct from source/recipe cache identities, so preparation
can repair damaged data by publishing new files while retaining previous versions.
Prefilter identity includes source/intent, decoder/orientation, profile/algorithm
and producer/FP fingerprint; the LUT identity is independent of HDR source.
Artifact byte hashes are checked independently. Producer fingerprints reuse
canonical build identity. Accepted dependencies exclude the original HDR.

Preparation uses the existing native process, progress/cancel, lease and atomic
filesystem services. It snapshots source/settings/acceptance and rechecks before
publication. Failed, conflicting, cancelled or stale candidates retain the prior
accepted descriptor/resources. Killed jobs' marker-owned flat temporary stages
are cleaned on retry under the same lease. Generated data is outside authored
file/scene history. Explicit Save/Reload/Refresh is the freshness boundary.
Runtime never bakes or opens producer inputs. Changes to source bytes are reported
by explicit native authoring inspection; unavailable source is a rebuild limitation,
not a runtime dependency. Changed authored intent is stale and rejects new Play.

Hazelnut's Content Browser creates/opens environment documents and can import a
validated HDR into project ownership. The environment panel uses file-owned
undo/redo, guarded Save/Reopen and native Prepare/Cancel with Console progress.
It reports accepted quality, identity, bake time, peak resident memory and stale/
missing diagnostics. Scene Settings selects an asset and edits yaw/gain/background/
lighting through existing scene history. Surface inspection adds direct, diffuse
IBL and specular IBL terms. No fake preview renderer was added.

Additive `IBL_v1` Reference scenes cover DirectOnly, IBLOnly, Combined, Photo and
yaw +90 degrees. Existing Core_v1, Import_v1 and Direct_v1 bytes/conditions remain
frozen. Photo uses original Studio Small 09 1k HDR, Sergej Majboroda / Poly Haven,
CC0-1.0; the source URL/hash/notice are recorded with the assets. Diagnostic inputs
are engine-authored. Numerical, actual GPU, editor and CI qualification results
will be recorded here when completed; added tests are not themselves pass evidence.

From the canonical checkout (matching build configuration):

```sh
python3 scripts/hazel.py run Hazelnut --config Release --project examples/RenderingReference/RenderingReference-IBL-Combined.hproj
python3 scripts/hazel.py run Nutella --config Release --project examples/RenderingReference/RenderingReference-IBL-Photo.hproj
```

Native preparation: `HazelProject prepare-environment PROJECT.hproj ASSET.henv`.
The development helper is staged beside Hazelnut; neither command uses Python to
author assets. Stage 5's broader mixed-scene transitions/export campaign remains
deferred, along with dynamic environments, probes, shadows, EXR/other formats,
Metal and other render paths.

Focused Release CPU qualification passed the maintained SceneFoundationSmoke, including
HDR framing/float vectors, constant piC, normalized mips, analytic axes/lobe endpoints,
portable decoding, independent LUT/furnace integration and native document/publication
checks. Maximum sampled LUT error was .00435928 with quadrature delta .000526171.
The recorded Fview diffuse furnace deficit at grazing reached .719418 for a unit
white dielectric; this is the specified approximation, not a tolerance adjustment
or compensated energy. Actual GPU/editor and full Debug/Release remain pending.
