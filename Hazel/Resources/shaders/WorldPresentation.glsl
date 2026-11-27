// Hazel scene-referred presentation: fixed Khronos PBR Neutral, linear output.
#type vertex
#version 450 core
layout(location=0) out vec2 v_UV;
void main(){vec2 p=vec2((gl_VertexIndex<<1)&2,gl_VertexIndex&2);v_UV=p;gl_Position=vec4(p*2.-1.,0.,1.);}
#type fragment
#version 450 core
layout(location=0) in vec2 v_UV;
layout(location=0) out vec4 o_Color;
layout(location=1) out int o_Entity;
layout(binding=0) uniform sampler2D u_World;
layout(binding=1) uniform isampler2D u_Picking;
layout(binding=2) uniform sampler2D u_Bloom;
layout(binding=3) uniform sampler2D u_AdaptedGain;
layout(binding=4) uniform sampler2D u_DefocusDepth;
layout(binding=5) uniform sampler2D u_DefocusNear;
layout(binding=6) uniform sampler2D u_DefocusFar;
layout(binding=7) uniform sampler2D u_DefocusBounds;
layout(binding=8) uniform sampler2D u_ShutterLayer;
layout(std140,binding=5) uniform Presentation {vec4 u_Presentation, u_Defocus;};
// SHUTTER RECONSTRUCTION
vec3 reconstructShutter(vec3 sceneColor, ivec2 pixel, float mode)
{
    ivec2 size = textureSize(u_ShutterLayer, 0);
    vec2 point = (vec2(pixel) + .5) * .5;
    vec4 layer;
    if (mode < 1.5) {
        layer = texture(u_ShutterLayer, point / vec2(size));
    } else {
        vec2 p = point - .5;
        ivec2 base = ivec2(floor(p));
        ivec2 last = size - 1;
        vec2 f = fract(p);
        vec4 a = texelFetch(u_ShutterLayer, clamp(base, ivec2(0), last), 0);
        vec4 b = texelFetch(u_ShutterLayer, clamp(base + ivec2(1, 0), ivec2(0), last), 0);
        vec4 c = texelFetch(u_ShutterLayer, clamp(base + ivec2(0, 1), ivec2(0), last), 0);
        vec4 d = texelFetch(u_ShutterLayer, clamp(base + ivec2(1, 1), ivec2(0), last), 0);
        layer = mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
    }
    return sceneColor * (1.0 - layer.a) + layer.rgb;
}
// END SHUTTER RECONSTRUCTION
// DEFOCUS RECONSTRUCTION
vec4 sampleDefocusLayer(sampler2D layer, vec2 uv, bool explicitFiltering)
{
    if (!explicitFiltering) return texture(layer, uv);
    ivec2 size = textureSize(layer, 0);
    vec2 point = uv * vec2(size) - .5;
    ivec2 base = ivec2(floor(point));
    ivec2 last = size - 1;
    vec2 t = fract(point);
    vec4 a = texelFetch(layer, clamp(base, ivec2(0), last), 0);
    vec4 b = texelFetch(layer, clamp(base + ivec2(1, 0), ivec2(0), last), 0);
    vec4 c = texelFetch(layer, clamp(base + ivec2(0, 1), ivec2(0), last), 0);
    vec4 d = texelFetch(layer, clamp(base + ivec2(1, 1), ivec2(0), last), 0);
    return mix(mix(a, b, t.x), mix(c, d, t.x), t.y);
}
vec3 reconstructDefocus(vec3 world, ivec2 pixel, vec4 lens, bool nearInfluences)
{
    float depth = texelFetch(u_DefocusDepth, pixel, 0).r;
    float coc = (depth - lens.x) * lens.y;
    vec2 uv = (vec2(pixel) + .5) / (2.0 * vec2(textureSize(u_DefocusNear, 0)));
    if (abs(coc) > .75) {
        vec4 farLayer = sampleDefocusLayer(u_DefocusFar, uv, lens.w > 1.5);
        float farMask = smoothstep(.75, 2.0, abs(coc));
        world = world * (1.0 - farMask * farLayer.a) + farMask * farLayer.rgb;
    }
    if (nearInfluences) {
        vec4 nearLayer = sampleDefocusLayer(u_DefocusNear, uv, lens.w > 1.5);
        world = world * (1.0 - nearLayer.a) + nearLayer.rgb;
    }
    return world;
}
// END DEFOCUS RECONSTRUCTION
vec3 neutral(vec3 radiance){
    float minimum=min(radiance.x,min(radiance.y,radiance.z));
    vec3 value=radiance-(minimum<.08?minimum-6.25*minimum*minimum:.04);
    float maximum=max(value.x,max(value.y,value.z));
    if(maximum<.76)return value;
    float compressed=1.-.0576/(maximum-.52);
    vec3 scaled=value*(compressed/maximum);
    float whiteWeight=1.-1./(1.+.15*(maximum-compressed));
    return mix(scaled,vec3(compressed),whiteWeight);
}
void main() {
    vec4 world=texture(u_World,v_UV);
    if (u_Presentation.w > .5) world.rgb = reconstructShutter(world.rgb, ivec2(gl_FragCoord.xy), u_Presentation.w);
    if (u_Defocus.w > .5) {
        ivec2 pixel = ivec2(gl_FragCoord.xy);
        bool nearInfluences = texelFetch(u_DefocusBounds, pixel / 16, 0).x >= .5;
        world.rgb = reconstructDefocus(world.rgb, pixel, u_Defocus, nearInfluences);
    }
    if (u_Presentation.y>0.0) world.rgb+=u_Presentation.y*texture(u_Bloom,v_UV).rgb;
    float gain = u_Presentation.x;
    if (u_Presentation.z > .5)
        gain = clamp(gain + texelFetch(u_AdaptedGain, ivec2(0), 0).r, -16., 16.);
    o_Color=vec4(neutral(world.rgb*exp2(gain)),world.a);
    o_Entity=texelFetch(u_Picking,ivec2(gl_FragCoord.xy),0).r;
}
