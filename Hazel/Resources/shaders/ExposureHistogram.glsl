// Additive, sharded histogram for the OpenGL 4.1 raster path.
#type vertex
#version 450 core
layout(binding=0) uniform sampler2D u_World;
layout(binding=1) uniform sampler2D u_Depth;
layout(std140,binding=5) uniform ExposureMeter { vec4 u_Range, u_Metering, u_Step; };
layout(location=0) flat out float v_Mass;

uint hash(uint value)
{
    value ^= value >> 16;
    value *= 0x7feb352du;
    value ^= value >> 15;
    value *= 0x846ca68bu;
    return value ^ (value >> 16);
}

void main()
{
    ivec2 size = textureSize(u_World, 0);
    ivec2 grid = min(size, ivec2(128, 72));
    ivec2 cell = ivec2(gl_VertexIndex % grid.x, gl_VertexIndex / grid.x);
    ivec2 first = cell * size / grid;
    ivec2 last = (cell + 1) * size / grid;
    uint random = hash(uint(gl_VertexIndex) + 1u);
    ivec2 pixel = first + ivec2(random & 65535u, random >> 16) % (last - first);
    vec2 uv = (vec2(pixel) + .5) / vec2(size);
    // The authored metering point is top-left. GL texture rows are bottom-up.
    pixel.y = size.y - 1 - pixel.y;
    vec3 rgb = texelFetch(u_World, pixel, 0).rgb;
    bool valid = !any(isnan(rgb)) && !any(isinf(rgb)) &&
        (u_Step.w > .5 || texelFetch(u_Depth, pixel, 0).r < 1.);

    float luminance = dot(clamp(rgb, vec3(0.), vec3(16777216.)), vec3(.2126, .7152, .0722));
    float ordinate = clamp(log2(max(luminance, exp2(-24.))), -24., 24.);
    int bin = valid ? clamp(int((ordinate + 24.) * (128. / 48.)), 0, 127) : 0;
    float weight = 1.;
    if (u_Metering.w == 1.)
        weight = mix(1., .125, smoothstep(0., .7, length(uv - .5)));
    if (u_Metering.w == 2.) {
        float distance = length((uv - u_Metering.xy) * vec2(size) / float(min(size.x, size.y)));
        weight = 1. - smoothstep(.5 * u_Metering.z, u_Metering.z, distance);
    }
    v_Mass = valid ? floor(16. * weight + .5) : 0.;
    int row = gl_VertexIndex & 15;
    gl_Position = vec4((float(bin) + .5) / 64. - 1., (float(row) + .5) / 8. - 1., 0., 1.);
    gl_PointSize = 1.;
}
#type fragment
#version 450 core
layout(location=0) flat in float v_Mass;
layout(location=0) out vec4 o_Histogram;
void main() { o_Histogram = vec4(v_Mass, 0., 0., 0.); }
