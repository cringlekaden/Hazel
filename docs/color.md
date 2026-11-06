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
with no conflicting encoding field on the shared image. Mesh material values are
metadata at stage 2 and do not imply implemented mesh/PBR rendering.

`TextureSpecification::Encoding` distinguishes `SRGB` from `Linear`. Generated
textures default to Linear; ordinary RGB(A) file textures default to SRGB.
Sprite-sheet Sampling can explicitly request either interpretation. A shared file
used for color and data receives distinct GPU resources. R8 and RGBA32F require
Linear. Float pixels are never passed through an LDR transfer function.

Ordinary image rows and file texture uploads are top-left. Sprite UVs pair the
bottom vertices with the larger V coordinate. Generated glyph pages retain their
own explicit atlas UVs. Runtime UI fit, crop and nine-slice use those same sprite
UVs. Texture bytes, alpha, geometry and glyph distance data are not gamma corrected.

The supported world presentation is **DisplayReferred** unlit, independent of
camera projection and editor navigation. The display canvas has an opaque scene background; transparent scene output is
currently rejected, so ImGui never composites a partly transparent encoded viewport.
World sprites/circles/text and ordered
screen UI compose into one RGBA16F linear target with ordinary world depth and
separate integer picking. This needs no light evaluation, HDR world lighting
target, environment resources or tone-map pass. Scene-referred presentation is
rejected until its renderer exists.

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
