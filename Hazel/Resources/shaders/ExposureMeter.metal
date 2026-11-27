#include <metal_stdlib>
using namespace metal;

// The OpenGL raster path uses the same strata, 128 bins and integer sample mass.
struct MeterParameters { float4 range, metering, step; };

uint meterHash(uint value)
{
    value ^= value >> 16;
    value *= 0x7feb352du;
    value ^= value >> 15;
    value *= 0x846ca68bu;
    return value ^ (value >> 16);
}

kernel void meterHistogram(constant MeterParameters& p [[buffer(0)]],
    texture2d<float, access::read> world [[texture(0)]],
    texture2d<float, access::read> depth [[texture(1)]],
    texture2d<float, access::write> histogram [[texture(2)]],
    uint row [[threadgroup_position_in_grid]], uint column [[thread_index_in_threadgroup]])
{
    threadgroup atomic_uint bins[128];
    atomic_store_explicit(&bins[column], 0, memory_order_relaxed);
    threadgroup_barrier(mem_flags::mem_threadgroup);
    uint2 size(world.get_width(), world.get_height());
    uint2 grid = min(size, uint2(128, 72));
    if (column < grid.x && row < grid.y) {
        uint2 cell(column, row);
        uint2 first = cell * size / grid, last = (cell + 1) * size / grid;
        uint random = meterHash(row * grid.x + column + 1);
        uint2 pixel = first + uint2(random & 65535u, random >> 16) % (last - first);
        float2 uv = (float2(pixel) + .5f) / float2(size);
        // Hazel keeps scene texture rows in GL order on both backends.
        pixel.y = size.y - 1 - pixel.y;
        float3 rgb = world.read(pixel).rgb;
        if (all(isfinite(rgb)) && (p.step.w > .5f || depth.read(pixel).r < 1.f)) {
            float luminance = dot(clamp(rgb, 0.f, 16777216.f), float3(.2126f, .7152f, .0722f));
            float ordinate = clamp(log2(max(luminance, exp2(-24.f))), -24.f, 24.f);
            uint bin = min(uint((ordinate + 24.f) * (128.f / 48.f)), 127u);
            float weight = 1.f;
            if (p.metering.w == 1.f)
                weight = mix(1.f, .125f, smoothstep(0.f, .7f, length(uv - .5f)));
            if (p.metering.w == 2.f) {
                float distance = length((uv - p.metering.xy) * float2(size) / float(min(size.x, size.y)));
                weight = 1.f - smoothstep(.5f * p.metering.z, p.metering.z, distance);
            }
            atomic_fetch_add_explicit(&bins[bin], uint(floor(16.f * weight + .5f)), memory_order_relaxed);
        }
    }
    threadgroup_barrier(mem_flags::mem_threadgroup);
    histogram.write(float4(float(atomic_load_explicit(&bins[column], memory_order_relaxed))), uint2(column, row));
}

kernel void adaptExposure(constant MeterParameters& p [[buffer(0)]],
    texture2d<float, access::read> histogram [[texture(0)]],
    texture2d<float, access::read> history [[texture(1)]],
    texture2d<float, access::write> output [[texture(2)]],
    uint bin [[thread_index_in_threadgroup]])
{
    threadgroup float bins[128];
    float count = 0.f;
    for (uint row = 0; row < 72; ++row)
        count += histogram.read(uint2(bin, row)).r;
    bins[bin] = count;
    threadgroup_barrier(mem_flags::mem_threadgroup);
    if (bin != 0) return;
    float mass = 0.f;
    for (uint i = 0; i < 128; ++i) mass += bins[i];
    float4 previous(0.f);
    if (p.step.z > .5f) previous = history.read(uint2(0));
    if (mass == 0.f) {
        output.write(previous, uint2(0));
        return;
    }
    float low = p.range.x * mass, high = p.range.y * mass;
    float prefix = 0.f, retained = 0.f, moment = 0.f;
    for (uint i = 0; i < 128; ++i) {
        float begin = prefix;
        prefix += bins[i];
        float keep = max(0.f, min(high, prefix) - max(low, begin));
        retained += keep;
        moment += keep * (-24.f + (float(i) + .5f) * .375f);
    }
    float target = clamp(log2(.18f) - moment / retained, p.range.z, p.range.w);
    float gain = target;
    if (previous.z > 0.f) {
        float prior = clamp(previous.x, p.range.z, p.range.w);
        gain = mix(prior, target, target > prior ? p.step.x : p.step.y);
    }
    output.write(float4(gain, target, retained, 0.f), uint2(0));
}
