#type vertex
#version 450 core
void main()
{
    vec2 p = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);
}
#type fragment
#version 450 core
layout(location=0) out vec4 o_Result;
layout(binding=0) uniform sampler2D u_World;
layout(binding=1) uniform sampler2D u_Depth;
layout(binding=2) uniform isampler2D u_Entity;
layout(binding=3) uniform sampler2D u_PreviousPosition;
layout(binding=4) uniform isampler2D u_VectorEntity;
layout(binding=5) uniform sampler2D u_Tiles;
layout(std140,binding=5) uniform ShutterFilter {
    mat4 u_Reprojection, u_SkyReprojection;
    vec4 u_Shutter, u_Projection;
    uvec4 u_MovingIDMask;
};
// Shutter = half fraction, maximum half-width in pixels, tap limit, stage.
// Projection = P22, P32, orthographic, inverse axial matrix coefficient.

vec2 endpoint(vec2 displacement)
{
    vec2 halfWidth = displacement * u_Shutter.x;
    float squared = dot(halfWidth, halfWidth);
    if (!(squared > 1e-12) || isinf(squared)) return vec2(0);
    if (squared > u_Shutter.y * u_Shutter.y)
        halfWidth *= u_Shutter.y * inversesqrt(squared);
    return halfWidth;
}
vec2 cameraMotion(ivec2 p, ivec2 size, float depth)
{
    vec2 uv = (vec2(p) + .5) / vec2(size);
    vec4 clip = vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    vec4 previous = depth == 1.0 && u_Projection.z < .5 ?
        u_SkyReprojection * clip : u_Reprojection * clip;
    if (previous.w <= 1e-6) return vec2(0);
    vec2 previousUV = previous.xy / previous.w * .5 + .5;
    return endpoint((uv - previousUV) * vec2(size));
}
vec2 vectorMotion(ivec2 p, ivec2 vectorSize, ivec2 size)
{
    vec4 previous = texelFetch(u_PreviousPosition, p, 0);
    if (previous.w < .5) return vec2(0);
    vec2 uv = (vec2(p) + .5) / vec2(vectorSize);
    return endpoint((uv - previous.xy) * vec2(size));
}
bool hasObjectMotion(int identity)
{
    if (identity < 0) return false;
    uint bit = (uint(identity) * 0x9e3779b9u) >> 26;
    return (u_MovingIDMask[bit / 32u] & (1u << (bit % 32u))) != 0u;
}
vec2 shutterMotion(ivec2 p, ivec2 size, ivec2 vectorSize, float depth, int identity)
{
    if (!hasObjectMotion(identity)) return cameraMotion(p, size, depth);
    ivec2 samplePixel = min(vectorSize - 1, ivec2((vec2(p) + .5) * vec2(vectorSize) / vec2(size)));
    if (texelFetch(u_VectorEntity, samplePixel, 0).r == identity)
        return vectorMotion(samplePixel, vectorSize, size);
    // Match a missed half-resolution sample only when rigid identity agrees.
    if (any(notEqual(vectorSize, size))) {
        for (int y = -1; y <= 1; ++y) {
            for (int x = -1; x <= 1; ++x) {
                ivec2 neighbor = clamp(samplePixel + ivec2(x, y), ivec2(0), vectorSize - 1);
                if (texelFetch(u_VectorEntity, neighbor, 0).r == identity)
                    return vectorMotion(neighbor, vectorSize, size);
            }
        }
    }
    return cameraMotion(p, size, depth);
}
float axialDepth(float depth)
{
    float ndc = depth * 2.0 - 1.0;
    if (u_Projection.z > .5)
        return (u_Projection.y - ndc) * u_Projection.w;
    return u_Projection.y / min(-1e-7, ndc + u_Projection.x);
}
vec2 maxMotion(vec2 a, vec2 b)
{
    return dot(a, a) >= dot(b, b) ? a : b;
}
void main()
{
    ivec2 outPixel = ivec2(gl_FragCoord.xy);
    ivec2 size = textureSize(u_World, 0);
    if (u_Shutter.w == 0.0) {
        ivec2 vectorSize = textureSize(u_PreviousPosition, 0);
        ivec2 first = outPixel * 16;
        ivec2 last = min(first + 16, size);
        ivec2 start = first * vectorSize / size;
        ivec2 end = ((last * vectorSize) + size - 1) / size;
        vec2 largest = vec2(0);
        if (any(notEqual(u_MovingIDMask.xy, uvec2(0)))) {
            vec2 inverse = 1.0 / vec2(vectorSize);
            for (int y = start.y; y < end.y; y += 2) {
                for (int x = start.x; x < end.x; x += 2) {
                    ivec2 p = ivec2(x,y);
                    ivec4 identities = textureGather(u_VectorEntity, (vec2(p) + 1.0) * inverse);
                    bool moving = hasObjectMotion(identities.x) || hasObjectMotion(identities.y) ||
                        hasObjectMotion(identities.z) || hasObjectMotion(identities.w);
                    if (!moving) continue;
                    // Use explicit fetches here so the reducer is independent
                    // of the gather component order on the two backends.
                    for (int element = 0; element < 4; ++element) {
                        ivec2 q = p + ivec2(element & 1, element >> 1);
                        if (any(greaterThanEqual(q, end))) continue;
                        if (hasObjectMotion(texelFetch(u_VectorEntity, q, 0).r))
                            largest = maxMotion(largest, vectorMotion(q, vectorSize, size));
                    }
                }
            }
        }
        // Include world geometry and sky camera motion without another
        // full-resolution buffer read at every position in the tile.
        for (int y = 0; y < 3; ++y) {
            for (int x = 0; x < 3; ++x) {
                ivec2 p = min(first + ivec2(x, y) * 7, last - 1);
                largest = maxMotion(largest, cameraMotion(p, size, texelFetch(u_Depth, p, 0).r));
            }
        }
        o_Result = vec4(largest, 0, 0);
        return;
    }
    if (u_Shutter.w == 1.0) {
        ivec2 last = textureSize(u_Tiles, 0) - 1;
        int reach = int(ceil(u_Shutter.y / 16.0));
        vec2 largest = vec2(0);
        for (int y = -reach; y <= reach; ++y)
            for (int x = -reach; x <= reach; ++x)
                largest = maxMotion(largest, texelFetch(u_Tiles, clamp(outPixel + ivec2(x,y), ivec2(0), last), 0).xy);
        o_Result = vec4(largest, 0, 0);
        return;
    }

    ivec2 p = min(outPixel * 2, size - 1);
    vec2 pixelCenter = min(vec2(p) + .5, vec2(size - 1));
    vec2 predicted = texelFetch(u_Tiles, p / 16, 0).xy;
    o_Result = vec4(0);
    if (dot(predicted, predicted) < .25) return;
    float rawDepth = texelFetch(u_Depth, p, 0).r;
    float centerDepth = axialDepth(rawDepth);
    int centerID = texelFetch(u_Entity, p, 0).r;
    ivec2 vectorSize = textureSize(u_PreviousPosition, 0);
    vec2 centerMotion = shutterMotion(p, size, vectorSize, rawDepth, centerID);
    vec2 axis = maxMotion(centerMotion, predicted);
    float width = length(axis);
    if (width <= .5) return;
    int samples = min(int(u_Shutter.z), max(3, 2 * int(ceil(width)) + 1));
    const float tolerance = max(.01, .015 * centerDepth);
    vec3 centerColor = texelFetch(u_World, p, 0).rgb;
    vec3 integrated = vec3(0);
    float moving = length(centerMotion);
    for (int i = 0; i < samples; ++i) {
        float t = (float(i) + .5) * (2.0 / float(samples)) - 1.0;
        vec2 position = pixelCenter - axis * t;
        if (any(lessThan(position, vec2(-.5))) || any(greaterThan(position, vec2(size) - .5))) {
            integrated += centerColor;
            continue;
        }
        ivec2 q = clamp(ivec2(floor(position + .5)), ivec2(0), size - 1);
        float depth = texelFetch(u_Depth, q, 0).r;
        float sceneDepth = axialDepth(depth);
        vec3 sampleColor = texelFetch(u_World, q, 0).rgb;
        if (moving > .5 && sceneDepth > centerDepth + tolerance) {
            // A moving foreground can uncover the deeper visible color.
            integrated += sampleColor;
        } else {
            int identity = texelFetch(u_Entity, q, 0).r;
            vec2 v = identity == centerID && depth == rawDepth ? centerMotion :
                shutterMotion(q, size, vectorSize, depth, identity);
            vec2 shift = t * (v - axis);
            float squared = dot(shift, shift);
            float covers = squared <= .25 ? 1.0 :
                squared >= 2.25 ? 0.0 : 1.0 - smoothstep(.5, 1.5, sqrt(squared));
            covers *= float(dot(v, v) > .25 && sceneDepth <= centerDepth + tolerance);
            integrated += mix(centerColor, sampleColor, covers);
        }
    }
    vec3 blurred = integrated / float(samples);
    if (any(notEqual(blurred, centerColor))) o_Result = vec4(blurred, 1.0);
}
