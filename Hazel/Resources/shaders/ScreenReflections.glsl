#type vertex
#version 450 core
void main()
{
    vec2 p = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    gl_Position = vec4(p * 2 - 1, 0, 1);
}
#type fragment
#version 450 core
layout(location=0) out vec4 o_Color;
layout(location=1) out int o_Entity;
layout(binding=0) uniform sampler2D u_Radiance;
layout(binding=1) uniform isampler2D u_Identity;
layout(binding=2) uniform sampler2D u_Depth;
layout(binding=3) uniform sampler2D u_Normals;
layout(binding=4) uniform sampler2D u_Weight;
layout(binding=5) uniform sampler2D u_Fallback;
layout(std140,binding=5) uniform Reflections
{
    mat4 u_Projection;
    mat4 u_InverseProjection;
    mat4 u_View;
    vec4 u_Trace; // World distance, view-depth thickness, steps, maximum pixel stride.
    vec4 u_Camera; // Near plane, orthographic flag.
};
// Metal's offscreen vertex translation preserves OpenGL row order.
vec3 decodeNormal(vec2 e)
{
    vec3 n = vec3(e, 1 - abs(e.x) - abs(e.y));
    n.xy += mix(vec2(1), vec2(-1), greaterThanEqual(n.xy, vec2(0))) * max(-n.z, 0);
    return normalize(mat3(u_View) * n);
}

vec3 reconstruct(vec2 uv, float depth)
{
    vec2 ndc = uv * 2 - 1;
    vec4 point = u_InverseProjection * vec4(ndc, depth * 2 - 1, 1);
    return point.xyz / point.w;
}

vec2 projectedPixel(vec4 clip, vec2 extent)
{
    return (clip.xy / clip.w * .5 + .5) * extent;
}

bool inside(vec2 p, vec2 extent)
{
    return all(greaterThanEqual(p, vec2(0))) && all(lessThan(p, extent));
}

float deviceDepth(vec4 clip)
{
    return (clip.z / clip.w) * .5 + .5;
}

bool trace(vec3 origin, vec3 ray, vec2 extent, out vec2 hit, out float confidence)
{
    float distance = u_Trace.x;
    if (ray.z > 0)
        distance = min(distance, (-u_Camera.x - .0001 - origin.z) / ray.z);
    if (distance <= 0)
        return false;

    vec3 end = origin + ray * distance;
    vec4 startClip = u_Projection * vec4(origin, 1);
    vec4 endClip = u_Projection * vec4(end, 1);
    if (startClip.w <= 0 || endClip.w <= 0)
        return false;
    vec2 start = projectedPixel(startClip, extent);
    vec2 span = projectedPixel(endClip, extent) - start;
    float lengthPixels = max(abs(span.x), abs(span.y));
    if (lengthPixels < 3 || !inside(start, extent))
        return false;

    vec2 stepDirection = span / lengthPixels;
    // Window depth is affine along the projected line, including under
    // perspective. Convert to view depth only when testing hit thickness.
    float startDepth = deviceDepth(startClip);
    float depthSlope = (deviceDepth(endClip) - startDepth) / lengthPixels;
    // Preserve the angular step budget as backing resolution increases.
    float stride = clamp(lengthPixels / u_Trace.z, 1, u_Trace.w * max(extent.y / 720, 1));
    float previousT = 1.5;
    float previousGap = -1e20;
    for (int i = 0; i < int(u_Trace.z); ++i)
    {
        float t = min(1.5 + float(i) * stride, lengthPixels);
        vec2 p = start + stepDirection * t;
        if (!inside(p, extent))
            break;
        float surfaceDepth = texelFetch(u_Depth, ivec2(p), 0).r;
        float gap = surfaceDepth < 1 ? startDepth + depthSlope * t - surfaceDepth : -1;
        if (i > 0 && gap >= 0 && previousGap < 0)
        {
            // Find the first front-to-back crossing. A silhouette that jumps
            // from clear/background to far behind a foreground sample fails
            // the final thickness test even if it straddled the coarse step.
            float lo = previousT, hi = t;
            for (int refine = 0; refine < 5; ++refine)
            {
                float mid = (lo + hi) * .5;
                ivec2 pixel = ivec2(start + stepDirection * mid);
                float depth = texelFetch(u_Depth, pixel, 0).r;
                if (depth < 1 && startDepth + depthSlope * mid >= depth)
                    hi = mid;
                else
                    lo = mid;
            }
            vec2 position = start + stepDirection * hi;
            ivec2 pixel = ivec2(position);
            float depth = texelFetch(u_Depth, pixel, 0).r;
            vec2 uv = position / extent;
            float penetration = reconstruct((vec2(pixel) + .5) / extent, depth).z -
                reconstruct(uv, startDepth + depthSlope * hi).z;
            float facing = -dot(decodeNormal(texelFetch(u_Normals, pixel, 0).zw), ray);
            if (penetration >= 0 && penetration < u_Trace.y && facing > .06 && hi > 2)
            {
                vec2 border = min(uv, 1 - uv);
                float edge = smoothstep(.005, .09, min(border.x, border.y));
                float depthConfidence = 1 - smoothstep(u_Trace.y * .2, u_Trace.y, penetration);
                float range = min(lengthPixels, 1.5 + (u_Trace.z - 1) * stride);
                float travel = 1 - smoothstep(range * .78, range, hi);
                hit = position;
                confidence = edge * depthConfidence * travel * smoothstep(.06, .25, facing);
                return confidence > 0;
            }
            // The rest of this ray is hidden or crosses an untrustworthy
            // silhouette. Keep the probe rather than looking through an occluder.
            return false;
        }
        if (t >= lengthPixels)
            break;
        previousT = t;
        previousGap = gap;
    }
    return false;
}

vec3 filteredRadiance(vec2 hit, vec2 receiver, vec2 extent, float roughness, out float support)
{
    support = 1;
    ivec2 center = ivec2(hit);
    vec3 color = texelFetch(u_Radiance, center, 0).rgb;
    float radius = clamp(roughness * roughness * length(hit - receiver) * .4, 0, 8);
    if (radius < .75)
    {
        // Interpolate within the hit object to keep a subpixel reflection
        // stable during camera motion. Avoid pulling a silhouette's background
        // into the polished reflection.
        vec2 footprint = hit - .5;
        ivec2 base = ivec2(floor(footprint));
        vec2 f = fract(footprint);
        int identity = texelFetch(u_Identity, center, 0).r;
        vec3 sum = vec3(0);
        float total = 0;
        for (int i = 0; i < 4; ++i)
        {
            ivec2 offset = ivec2(i & 1, i >> 1);
            ivec2 tap = base + offset;
            if (!inside(vec2(tap), extent) || texelFetch(u_Identity, tap, 0).r != identity)
                continue;
            float w = (offset.x > 0 ? f.x : 1 - f.x) * (offset.y > 0 ? f.y : 1 - f.y);
            sum += texelFetch(u_Radiance, tap, 0).rgb * w;
            total += w;
        }
        support = smoothstep(.25, .85, total);
        return total > 0 ? sum / total : color;
    }
    vec3 normal = decodeNormal(texelFetch(u_Normals, center, 0).zw);
    vec3 point = reconstruct((vec2(center) + .5) / extent, texelFetch(u_Depth, center, 0).r);
    vec3 sum = color * 2;
    float total = 2;
    const vec2 offsets[4] = vec2[4](vec2(1, 0), vec2(-1, 0), vec2(0, 1), vec2(0, -1));
    for (int i = 0; i < 4; ++i)
    {
        vec2 p = hit + offsets[i] * radius;
        if (!inside(p, extent))
            continue;
        ivec2 tap = ivec2(p);
        float raw = texelFetch(u_Depth, tap, 0).r;
        if (raw >= 1)
            continue;
        vec3 position = reconstruct((vec2(tap) + .5) / extent, raw);
        float separation = abs(dot(position - point, normal));
        float agreement = dot(normal, decodeNormal(texelFetch(u_Normals, tap, 0).zw));
        float width = max(.01, -point.z * .002);
        float w = (1 - smoothstep(width, width * 3, separation)) *
            smoothstep(.65, .95, agreement) * smoothstep(.75, 1.5, radius);
        sum += texelFetch(u_Radiance, tap, 0).rgb * w;
        total += w;
    }
    return sum / total;
}

void main()
{
    ivec2 pixel = ivec2(gl_FragCoord.xy);
    vec4 radiance = texelFetch(u_Radiance, pixel, 0);
    o_Color = radiance;
    o_Entity = texelFetch(u_Identity, pixel, 0).r;
    vec4 weight = texelFetch(u_Weight, pixel, 0);
    if (all(lessThanEqual(weight.rgb, vec3(0))) || weight.a >= .55)
        return;
    float rawDepth = texelFetch(u_Depth, pixel, 0).r;
    if (rawDepth >= 1)
        return;
    vec2 extent = vec2(textureSize(u_Radiance, 0));
    vec2 receiver = vec2(pixel) + .5;
    vec3 point = reconstruct(receiver / extent, rawDepth);
    vec4 normals = texelFetch(u_Normals, pixel, 0);
    vec3 geometric = decodeNormal(normals.zw);
    vec3 ray = reflect(u_Camera.y > .5 ? vec3(0, 0, -1) : normalize(point), decodeNormal(normals.xy));
    float horizon = dot(geometric, ray);
    if (horizon <= .015)
        return;
    vec2 hit;
    float confidence;
    // A small normal offset only removes raster self-intersections. The
    // first pixels and receiver horizon are guarded independently.
    if (!trace(point + geometric * min(.005, u_Trace.y * .1), ray, extent, hit, confidence))
        return;
    confidence *= smoothstep(.015, .12, horizon) * (1 - smoothstep(.22, .55, weight.a));
    float support;
    vec3 replacement = filteredRadiance(hit, receiver, extent, weight.a, support) * weight.rgb;
    vec3 fallback = texelFetch(u_Fallback, pixel, 0).rgb;
    o_Color.rgb = max(radiance.rgb + (confidence * support) * (replacement - fallback), vec3(0));
}
