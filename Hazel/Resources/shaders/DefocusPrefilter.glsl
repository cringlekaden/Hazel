#type vertex
#version 450 core
void main()
{
    vec2 p = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);
}
#type fragment
#version 450 core
layout(location=0) out vec4 o_ColorRadius;
layout(binding=0) uniform sampler2D u_Source;
layout(binding=1) uniform sampler2D u_Depth;
layout(binding=2) uniform sampler2D u_Tiles;
layout(binding=8) uniform sampler2D u_ShutterLayer;
layout(std140,binding=5) uniform DefocusPrefilter { vec4 u_Lens, u_Optics; };
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

void main()
{
    ivec2 base = ivec2(gl_FragCoord.xy) * 2;
    vec2 bounds = texelFetch(u_Tiles, base / 16, 0).xy;
    if (max(bounds.x, bounds.y) < .75) {
        o_ColorRadius = vec4(0);
        return;
    }
    ivec2 last = textureSize(u_Source, 0) - 1;
    vec2 uv = (vec2(base) + 1.0) / vec2(last + 1);
    vec4 depths = textureGather(u_Depth, uv);
    vec4 coc = clamp((depths - u_Lens.x) * u_Lens.y, -u_Lens.z, u_Lens.z);
    float nearest = min(min(coc.x, coc.y), min(coc.z, coc.w));
    float farthest = max(max(coc.x, coc.y), max(coc.z, coc.w));
    if (farthest - nearest <= .5 && u_Lens.w < .5 && u_Optics.z < .5) {
        // All four bilateral weights are one, so hardware linear filtering
        // gives the exact uniform 2x2 color prefilter at this texel center.
        o_ColorRadius = vec4(texture(u_Source, uv).rgb, nearest);
        return;
    }
    float radii[4];
    for (int i = 0; i < 4; ++i) {
        ivec2 p = min(base + ivec2(i & 1, i >> 1), last);
        radii[i] = clamp((texelFetch(u_Depth, p, 0).r - u_Lens.x) * u_Lens.y, -u_Lens.z, u_Lens.z);
    }
    // At a silhouette select the closer CoC plane for the reduced texel.
    vec3 sum = vec3(0);
    float weight = 0;
    for (int i = 0; i < 4; ++i) {
        ivec2 p = min(base + ivec2(i & 1, i >> 1), last);
        float w = 1.0 - smoothstep(.5, 1.0, radii[i] - nearest);
        vec3 color = texelFetch(u_Source, p, 0).rgb;
        if (u_Optics.z > .5) color = reconstructShutter(color, p, u_Optics.z);
        sum += color * w;
        weight += w;
    }
    o_ColorRadius = vec4(sum / weight, nearest);
}
