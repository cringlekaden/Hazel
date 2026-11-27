#type vertex
#version 450 core
void main()
{
    vec2 p = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);
}
#type fragment
#version 450 core
layout(location=0) out vec4 o_Near;
layout(location=1) out vec4 o_Far;
layout(binding=0) uniform sampler2D u_Source;
layout(binding=2) uniform sampler2D u_Tiles;
layout(std140,binding=5) uniform DefocusGather { vec4 u_Lens, u_Gather; vec4 u_Taps[44]; };

vec4 sampleLayer(vec2 point, ivec2 last)
{
    vec4 nearest = texelFetch(u_Source, min(ivec2(point), last), 0);
    vec4 filtered;
    if (u_Gather.y > .5) {
        vec2 p = point - .5;
        ivec2 base = ivec2(floor(p));
        vec2 f = fract(p);
        vec4 a = texelFetch(u_Source, clamp(base, ivec2(0), last), 0);
        vec4 b = texelFetch(u_Source, clamp(base + ivec2(1, 0), ivec2(0), last), 0);
        vec4 c = texelFetch(u_Source, clamp(base + ivec2(0, 1), ivec2(0), last), 0);
        vec4 d = texelFetch(u_Source, clamp(base + ivec2(1, 1), ivec2(0), last), 0);
        filtered = mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
    } else {
        filtered = texture(u_Source, point / vec2(last + 1));
    }
    return abs(filtered.a - nearest.a) < .4 ? vec4(filtered.rgb, nearest.a) : nearest;
}
void main()
{
    o_Near = vec4(0);
    o_Far = vec4(0);
    ivec2 base = ivec2(gl_FragCoord.xy);
    vec4 bounds = texelFetch(u_Tiles, base / 8, 0);
    float largest = max(bounds.x, bounds.y);
    if (largest < .75) return;

    ivec2 last = textureSize(u_Source, 0) - 1;
    vec2 center = vec2(base) + .5;
    float receiver = texelFetch(u_Source, base, 0).a;
    float reach = max(1.0, max(bounds.x, receiver < -.75 ? bounds.y : receiver));
    int taps = largest < 3.0 ? 12 : int(u_Gather.x);
    int firstTap = taps == 12 ? 0 : 12;
    // Every texel in the search footprint is on a continuous defocused plane.
    // A single linear fetch suffices without a nearest-depth comparison.
    if ((bounds.z > .75 || bounds.w < -.75) && bounds.w - bounds.z < .4 && u_Gather.y < .5) {
        vec2 invSize = 1.0 / vec2(last + 1);
        vec3 color = vec3(0);
        float weight = 0.0;
        float radius = abs(receiver);
        for (int i = 0; i < taps; ++i) {
            vec4 tap = u_Taps[firstTap + i];
            float w = tap.w * (1.0 - smoothstep(radius - .5, radius + .5, tap.z * reach));
            color += texture(u_Source, (center + tap.xy * (.5 * reach)) * invSize).rgb * w;
            weight += w;
        }
        if (weight > 0.0) {
            if (receiver < 0.0) o_Near = vec4(color / weight, 1.0);
            else o_Far = vec4(color / weight, 1.0);
        }
        return;
    }
    vec3 nearColor = vec3(0), farColor = vec3(0);
    float nearby = 0.0, distant = 0.0, available = 0.0;
    for (int i = 0; i < taps; ++i) {
        vec4 tap = u_Taps[firstTap + i];
        vec2 pos = center + tap.xy * (.5 * reach);
        if (any(lessThan(pos, vec2(0))) || any(greaterThanEqual(pos, vec2(last + 1)))) continue;
        available += tap.w;
        vec4 sampleValue = sampleLayer(pos, last);
        float sampleRadius = sampleValue.a;
        vec3 color = sampleValue.rgb;
        float distance = tap.z * reach;
        if (sampleRadius < -.5) {
            // Scatter-as-gather with inverse pupil area for near coverage.
            float r = max(-sampleRadius, .75);
            float normalized = reach / r;
            float w = tap.w * (1.0 - smoothstep(r - .5, r + .5, distance)) * normalized * normalized;
            nearColor += w * color;
            nearby += w;
        } else {
            // Favor visible background near the receiver's defocus plane.
            float aperture = max(receiver, .75);
            float compatible = 1.0 - smoothstep(.5, max(1.0, .3 * aperture),
                                                abs(sampleRadius - receiver));
            float w = tap.w * (receiver < -.75 ? 1.0 :
                compatible * (1.0 - smoothstep(aperture - .5, aperture + .5, distance)));
            farColor += w * color;
            distant += w;
        }
    }
    if (distant > 0.0) o_Far = vec4(farColor / distant, 1.0);
    if (nearby > 0.0) {
        float coverage = clamp(nearby / max(available, 1.0), 0.0, 1.0);
        o_Near = vec4(nearColor * (coverage / nearby), coverage);
    }
}
