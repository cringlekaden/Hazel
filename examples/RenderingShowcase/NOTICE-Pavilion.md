# Pavilion asset credits

Downloaded November 27, 2026. The [download manifest](Assets/Pavilion/asset-downloads.json)
records exact URLs and SHA-256 digests for the original files. All the third-party
inputs below are under [CC0 1.0 Universal](https://creativecommons.org/publicdomain/zero/1.0/legalcode).
CC0 permits redistribution and modification without mandatory attribution.
The credits here identify the original authors and distinguish the adaptations.

| Asset and creator | Local destination and changes |
| --- | --- |
| [Flight Helmet](https://github.com/KhronosGroup/glTF-Sample-Assets/tree/4995a638c96dbc94376d3d6164e70fee3ac7f7a2/Models/FlightHelmet), Public, Gary Hsu (conversion from Maya) | `Assets/Pavilion/FlightHelmet`. Compacted the source into an embedded GLB, resized maps to at most 1024 px with Lanczos and stored them as quality-94 RGB JPEG without chroma subsampling before Hazel import. Replaced the transmissive lenses with an opaque smoked-bronze metallic/roughness finish. The 12 referenced native TGA maps were losslessly repacked to RGB PNG. The display uses the source geometry at metre scale. |
| [Dikhololo Sunset](https://polyhaven.com/a/dikhololo_sunset), Greg Zaal | `Assets/Pavilion/Sunset.{hdr,henv}` and the manifest-selected generation under `Assets/Pavilion/Sunset.henv.prepared`. Original 2048×1024 RGBE scanlines, with a unit gamma line and an all-zero Photoshop primaries placeholder removed from the header. The descriptor explicitly declares linear Rec.709/D65. |
| [Marble 01](https://polyhaven.com/a/marble_01), Rob Tuytel | `Assets/Pavilion/Surfaces/marble_01-*.jpg`. Original 1K color, ARM and +Y normals. Hazel lowers roughness and normal amplitude for the polished hall floor. |
| [Concrete Floor 02](https://polyhaven.com/a/concrete_floor_02), Rob Tuytel | `Assets/Pavilion/Surfaces/concrete_floor_02-*.jpg`. Original 1K color, ARM and +Y normals. Approximately 2 m repeats in the courtyard. |
| [Concrete Wall 007](https://polyhaven.com/a/concrete_wall_007), Charlotte Baglioni, Dario Barresi and Rico Cilliers | `Assets/Pavilion/Surfaces/concrete_wall_007-*.jpg`. Original 1K color, ARM and +Y normals. Low normal amplitude for the board surfaces. |
| [Wood Table 001](https://polyhaven.com/a/wood_table_001), Dimitrios Savva and Rico Cilliers | Reuses the studio gallery's `Assets/Materials/wood_table_001` images with a warmer factor for the oak. The original download records are in [PolyHaven-original-downloads.json](Assets/Materials/PolyHaven-original-downloads.json). |

Poly Haven's [license statement](https://polyhaven.com/license) covers the listed
HDRI and material inputs. Hazel's original geometry, graphics, optical dust,
Night Flight LUT, scene and authoring recipes use the repository's Apache-2.0
license. The authoring script rasterizes signage with the checkout's packaged
Open Sans fonts under their [bundled license](../../Hazel/Resources/fonts/opensans/LICENSE.txt).
Static reflection captures contain rendered views of these same assets.
