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
layout(binding=9) uniform sampler2D u_Flare;
layout(binding=10) uniform sampler2D u_Dirt;
layout(std140,binding=5) uniform Presentation {
    vec4 u_Presentation, u_Defocus, u_Sensor, u_Gate, u_DirtSettings;
};
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
vec3 reconstructWorld(vec3 world, ivec2 pixel)
{
    if (u_Presentation.w > .5) world = reconstructShutter(world, pixel, u_Presentation.w);
    if (u_Defocus.w > .5) {
        bool nearInfluences = texelFetch(u_DefocusBounds, pixel / 16, 0).x >= .5;
        world = reconstructDefocus(world, pixel, u_Defocus, nearInfluences);
    }
    return world;
}
ivec2 sourcePixel(vec2 uv, ivec2 extent)
{
    return clamp(ivec2(uv * vec2(extent)), ivec2(0), extent - 1);
}
vec3 filterWorldLinear(vec2 uv, ivec2 extent)
{
    // Fractional chromatic rays use an explicit footprint on both backends.
    // The same path handles Metal's warp when FP32 filtering is unavailable.
    vec2 p = uv * vec2(extent) - .5;
    ivec2 lo = ivec2(floor(p));
    ivec2 last = extent - 1;
    vec2 f = fract(p);
    vec3 a = texelFetch(u_World, clamp(lo, ivec2(0), last), 0).rgb;
    vec3 b = texelFetch(u_World, clamp(lo + ivec2(1, 0), ivec2(0), last), 0).rgb;
    vec3 c = texelFetch(u_World, clamp(lo + ivec2(0, 1), ivec2(0), last), 0).rgb;
    vec3 d = texelFetch(u_World, clamp(lo + ivec2(1, 1), ivec2(0), last), 0).rgb;
    return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}
float sensorNoise(ivec2 pixel, uint frame)
{
    uint value = uint(pixel.x) * 0x9e3779b9u ^ uint(pixel.y) * 0x85ebca6bu ^ frame * 0xc2b2ae35u;
    value ^= value >> 16;
    value *= 0x7feb352du;
    value ^= value >> 15;
    value *= 0x846ca68bu;
    value ^= value >> 16;
    return float(value & 65535u) / 65535.0 - .5;
}
void main() {
    ivec2 pixel = ivec2(gl_FragCoord.xy);
    ivec2 extent = textureSize(u_World, 0);
    vec2 uv = v_UV;
    if (u_Sensor.z != 0.0) {
        vec2 centered = v_UV - .5;
        vec2 scaled = centered * vec2(extent);
        float r2 = 4.0 * dot(scaled, scaled) / dot(vec2(extent), vec2(extent));
        uv = .5 + centered * ((1.0 + u_Sensor.z * r2) / (1.0 + max(u_Sensor.z, 0.0)));
        pixel = sourcePixel(uv, extent);
    }
    vec4 source = texture(u_World, uv);
    vec3 world = reconstructWorld(u_Gate.w >= 2.0 ? filterWorldLinear(uv, extent) : source.rgb, pixel);
    if (u_Sensor.y > 0.0) {
        // Separation grows linearly from the optical axis.
        vec2 offset = (uv - .5) * (2.0 * u_Sensor.y / float(max(extent.x, extent.y)));
        vec2 red = clamp(uv + offset, vec2(0), vec2(1));
        vec2 blue = clamp(uv - offset, vec2(0), vec2(1));
        world.r = reconstructWorld(filterWorldLinear(red, extent), sourcePixel(red, extent)).r;
        world.b = reconstructWorld(filterWorldLinear(blue, extent), sourcePixel(blue, extent)).b;
    }
    bool addFlare = mod(u_Gate.w, 2.0) > .5;
    if (addFlare || u_DirtSettings.w > .5) {
        vec3 glare = u_Presentation.y > 0.0 ? u_Presentation.y * texture(u_Bloom, uv).rgb : vec3(0);
        if (addFlare) glare += texture(u_Flare, uv).rgb;
        if (u_DirtSettings.w > .5) {
            float aspect = float(extent.x) / float(extent.y);
            float ratio = aspect / u_DirtSettings.z;
            vec2 crop = vec2(min(ratio, 1.0), min(1.0 / ratio, 1.0));
            vec2 dirtUV = .5 + (vec2(uv.x, 1.0 - uv.y) - .5) * crop / u_DirtSettings.y;
            float mask = dot(texture(u_Dirt, dirtUV).rgb, vec3(.2126, .7152, .0722));
            glare *= 1.0 + u_DirtSettings.x * mask;
        }
        world += glare;
    } else if (u_Presentation.y > 0.0) {
        world += u_Presentation.y * texture(u_Bloom, uv).rgb;
    }
    if (u_Sensor.x > 0.0) {
        vec2 ray = (v_UV * 2.0 - 1.0) * u_Gate.xy;
        float cosineSquared = 1.0 / (1.0 + dot(ray, ray));
        world *= mix(1.0, cosineSquared * cosineSquared, u_Sensor.x);
    }
    float gain = u_Presentation.x;
    if (u_Presentation.z > .5)
        gain = clamp(gain + texelFetch(u_AdaptedGain, ivec2(0), 0).r, -16., 16.);
    vec3 display = neutral(world * exp2(gain));
    if (u_Sensor.w > 0.0) {
        float luma = clamp(dot(display, vec3(.2126, .7152, .0722)), 0.0, 1.0);
        float weight = sqrt(luma * (1.0 - luma));
        display = clamp(display + u_Sensor.w * weight * sensorNoise(ivec2(gl_FragCoord.xy), uint(u_Gate.z)), 0.0, 1.0);
    }
    o_Color = vec4(display, source.a);
    o_Entity = texelFetch(u_Picking, pixel, 0).r;
    gl_FragDepth = u_Sensor.z != 0.0 ? texelFetch(u_DefocusDepth, pixel, 0).r : gl_FragCoord.z;
}
