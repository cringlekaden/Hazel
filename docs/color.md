# Runtime color

Hazel scene, text and UI RGB values are **linear RGB with sRGB/Rec.709 primaries
and D65 white**. Alpha is linear coverage/opacity. SceneVersion 3, PrefabVersion 3
and UIStyle.Version 2 identify this contract. Ambiguous historical color documents
are rejected with an actionable diagnostic; loading never rewrites source colors.

| Entry point | Meaning |
| --- | --- |
| Sprite/circle/world-text component colors; text emphasis | Linear RGB, linear alpha |
| Managed component/text/emphasis APIs | Same; `Hazel.Color.FromSRGB` converts a display literal explicitly |
| Native colors | Same; `ColorSpace::FromSRGB` / `ToSRGB` transfer RGB only |
| Inspector and style color pickers | Display sRGB; convert at the picker boundary |
| UI style foreground/background/focus/selection/caret; runtime UI images/text | Linear RGB, linear alpha |
| UI control state colors | Linear multipliers, including values above one; alpha remains an opacity multiplier |
| Ordinary file color textures | Unchanged top-left decoded bytes, sRGB storage; hardware decodes RGB before sampling/filtering |
| Data textures, masks, generated glyph/MSDF atlases and float textures | Linear data; no transfer function |
| Texture cache | Path, format, dimensions, encoding, sampling and mip policy all participate in identity |

Native `.hmat` base-color/emissive bindings interpret images as sRGB; normal,
metallic/roughness and occlusion bindings interpret linear data. glTF factors and
vertex colors are already linear and are copied without another transfer. Native
material pickers use the same sRGB display boundary. Imported PNG/JPEG channel
and alpha bytes survive cooking unchanged; slot usage supplies interpretation,
with no conflicting encoding field on the shared image. Static meshes use direct metallic/roughness shading and optional prepared image-based lighting. AO affects indirect terms only; no ambient multiplier is added.

`TextureSpecification::Encoding` distinguishes `SRGB` from `Linear`. Generated
textures default to Linear; ordinary RGB(A) file textures default to SRGB.
Sprite-sheet Sampling can explicitly request either interpretation. A shared file
used for color and data receives distinct GPU resources. R8, RG32F and RGBA32F require
Linear. Float pixels are never passed through an LDR transfer function.

Ordinary image rows and file texture uploads are top-left. Sprite UVs pair the
bottom vertices with the larger V coordinate. Generated glyph pages retain their
own explicit atlas UVs. Runtime UI fit, crop and nine-slice use those same sprite
UVs. Texture bytes, alpha, geometry and glyph distance data are not gamma corrected.

World presentation is independent of camera projection and editor navigation.
**DisplayReferred** composes unlit world sprites/circles/text, unlit meshes and
ordered screen UI into one RGBA16F linear target with depth and integer picking.
It needs no lighting target or tone-map pass.

**SceneReferred** renders world radiance into RGBA32F, applies exposure gain and
fixed Khronos PBR Neutral tone mapping into the display-linear RGBA16F composite,
then composes screen UI independently. Scene compensation adds signed stops to
the camera gain. Lit metallic/roughness meshes require this
mode. Unlit world content receives the same world exposure/presentation. Both
orthographic and perspective cameras work in either supported presentation.
The opaque linear background follows world presentation; transparent scene output
is rejected. Prepared Radiance HDR environments remain linear Rec.709/D65 float data;
background, diffuse and specular lookups share inverse yaw and radiance gain.
Cubemaps have explicit prepared mips; the BRDF LUT is linear A/B data. No source
image, sRGB transfer or convolution runs during runtime loading. See
[environment preparation](design/3d-environment-stage4.md).

The primary runtime Camera Inspector offers optional physical aperture, shutter
duration and ISO. These set EV100. The reference camera is f/4, 1/64 s and
ISO 100 (EV100 10). At that reference, zero compensation gives unity gain and
each positive compensation stop doubles it. This mapping uses Hazel's relative
scene-light units. The perspective camera can also use focal length and sensor
dimensions to frame the shot.

New native HDR projects use automatic exposure. Scene Settings supplies the
editor navigation policy and the default for runtime cameras. To use a different
policy in Play or Nutella, select **Override scene exposure** on the camera.
Automatic exposure meters the HDR world before bloom. **Lock adaptation**
holds the current gain until a cut or view reset, and compensation remains
editable. **Manual physical** uses EV100 from the active camera. A camera
without authored lens settings uses the reference combination above. Existing
scene files with the earlier signed exposure field retain their manual gain.
[Camera and image formation](design/camera-image-formation.md)

For a perspective shot, enable **Depth of field** on its Camera Inspector.
Use the lens focus distance for manual focus or choose a scene entity as the
**Focus subject**. The camera tracks the entity's world pivot, with the
authored lens distance as its fallback. **Balanced** uses narrower blur
support and fewer samples than **Cinematic**. The optional aperture polygon
controls highlight shape. The effect uses HDR world color before bloom and
leaves screen UI sharp. [Depth of field](design/camera-depth-of-field.md)

**Motion blur** is an independent Camera Inspector control. **Physical
duration** uses the lens shutter in seconds. **Angle** uses a fraction of
the elapsed frame time for blur and keeps the lens exposure settings.
Balanced and Cinematic bound the shutter gather and its maximum spread.
Motion blur resets on camera cuts and precedes depth of field and bloom.
[Camera motion blur](design/camera-motion-blur.md)

**Lens effects** offers visible-highlight ghosts, halos, starbursts and
anamorphic streaks. An optional image from the project’s Assets folder
modulates only glare, including bloom. Vignette, chromatic separation,
radial distortion and grain start at zero. Lens effects apply to the
camera’s world image before screen UI.
[Lens effects and qualification](design/camera-lens-effects.md)

A final pass applies the piecewise sRGB transfer into RGBA8, or into Nutella's
window with `GL_FRAMEBUFFER_SRGB` disabled. Alpha is copied. The editor's output
texture has RGBA8 storage, so ImGui samples the encoded values unchanged. Prefab,
style and font previews use the same output boundary. ImGui's own color/renderer
contract is unchanged: its file icons/image previews use explicit encoded-byte
sampling, separate from runtime color resources. No output conversion is added
to ImGui's backend.

Repository-owned palettes were deliberately converted by RGB field. Alpha,
transforms, physics, scalar data and linear control-state gain multipliers remain
unchanged. The two scripted palettes use explicit sRGB literal conversion.
Linear blending changes translucent overlaps and edges; palette intent does not
imply pixel identity with the former encoded-space blending.

CPU authoring/picker helpers and the final output shader use the exact piecewise
sRGB equation. Hardware SRGB8 sampling has implementation precision: M4 and
Mesa need not produce identical float bits. Mesa's [decoder](https://gitlab.freedesktop.org/mesa/mesa/-/blob/main/src/gallium/auxiliary/gallivm/lp_bld_format_srgb.c) decoded byte 128
to .216553 after half storage in Linux/Windows qualification (exact IEC value
.2158605). The independent texture readback oracle requires recovery within half
an sRGB byte; authored linear/data/alpha/compositing checks retain their 6e-4
half-float bound and final output retains its one-byte bound. This is documented
sampling error, not a second color path or a change to Core_v1 conditions.
