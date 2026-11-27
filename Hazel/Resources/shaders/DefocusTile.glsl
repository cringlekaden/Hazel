#type vertex
#version 450 core
void main()
{
    vec2 p = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);
}
#type fragment
#version 450 core
layout(location=0) out vec4 o_Radii;
layout(binding=0) uniform sampler2D u_Input;
layout(std140,binding=5) uniform DefocusTiles { vec4 u_Lens; };

void main()
{
    ivec2 tile = ivec2(gl_FragCoord.xy);
    ivec2 last = textureSize(u_Input, 0) - 1;
    vec2 bounds = vec2(0);
    vec2 signedRange = vec2(u_Lens.z, -u_Lens.z);
    if (u_Lens.w < .5) {
        ivec2 first = tile * 16;
        ivec2 end = min(first + 16, last + 1);
        vec2 depthRange = vec2(1, 0);
        vec2 invSize = 1.0 / vec2(last + 1);
        for (int y = first.y; y < end.y; y += 2) {
            for (int x = first.x; x < end.x; x += 2) {
                // A 2x2 gather with clamp-to-edge includes the last texel of
                // an odd extent. CoC is monotonic in window depth.
                vec4 depth = textureGather(u_Input, (vec2(x, y) + 1.0) * invSize);
                float low = min(min(depth.x, depth.y), min(depth.z, depth.w));
                float high = max(max(depth.x, depth.y), max(depth.z, depth.w));
                depthRange = vec2(min(depthRange.x, low), max(depthRange.y, high));
            }
        }
        signedRange = clamp((depthRange - u_Lens.xx) * u_Lens.y, -u_Lens.z, u_Lens.z);
        bounds = max(vec2(-signedRange.x, signedRange.y), vec2(0));
    } else {
        int reach = int(ceil(u_Lens.z / 16.0));
        for (int y = -reach; y <= reach; ++y) {
            for (int x = -reach; x <= reach; ++x) {
                ivec2 offset = ivec2(x, y);
                ivec2 other = tile + offset;
                if (any(lessThan(other, ivec2(0))) || any(greaterThan(other, last))) continue;
                vec4 sampleBounds = texelFetch(u_Input, other, 0);
                vec2 radius = sampleBounds.xy;
                // Conservative minimum distance between pixels in the two tiles.
                vec2 gap = max(vec2(abs(offset)) * 16.0 - 15.0, vec2(0));
                // Also admit the half-resolution reconstruction footprint.
                bounds = max(bounds, radius * step(vec2(length(gap) - 3.0), radius));
                signedRange = vec2(min(signedRange.x, sampleBounds.z), max(signedRange.y, sampleBounds.w));
            }
        }
    }
    o_Radii = vec4(bounds, signedRange);
}
