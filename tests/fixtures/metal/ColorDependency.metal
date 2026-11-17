#include <metal_stdlib>
using namespace metal;
// Dependency fixture only. Production owns pipeline admission/dispatch/lifetime.
kernel void ColorDependency(texture2d<float, access::read> source [[texture(0)]],
    texture2d<float, access::write> destination [[texture(1)]],
    constant float4& parameters [[buffer(0)]], uint2 pixel [[thread_position_in_grid]]) {
    if (pixel.x >= destination.get_width() || pixel.y >= destination.get_height()) return;
    float4 value = source.read(pixel);
    destination.write(float4(value.rgb * parameters.x + parameters.y, value.a), pixel);
}
