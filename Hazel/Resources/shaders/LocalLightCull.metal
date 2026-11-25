#include <metal_stdlib>
using namespace metal;
struct Lights {float4 meta;float4 values[960];};
struct Extent {uint4 size;float4 viewport;};
struct View {float4x4 vp;float4 eye,toCamera,light,incident;};
// A plane need not be normalized. A small outward error budget protects FP32
// tangent spheres; near/far and side tests work through near-plane intersections.
bool outside(float4 plane,float3 center,float radius) {
    float support=radius*length(plane.xyz);
    return dot(plane,float4(center,1)) < -support-1e-4*(1+support+abs(plane.w));
}
kernel void cullLocalLights(constant Lights& lights [[buffer(0)]],
    constant View& view [[buffer(1)]],constant Extent& constants [[buffer(2)]],
    texture2d<uint,access::write> tiles [[texture(0)]],uint2 tile [[thread_position_in_grid]]) {
    uint4 extent=constants.size;
    if(any(tile>=extent.zw))return;
    uint tileSize=uint(lights.meta.w);
    float2 low=2*(float2(tile*tileSize)-constants.viewport.xy)/constants.viewport.zw-1;
    float2 high=2*(float2(min((tile+1)*tileSize,extent.xy))-constants.viewport.xy)/constants.viewport.zw-1;
    // ForwardPBR flips vertex Y, preserving canonical GL lower-left attachment rows.
    float bottom=low.y,top=high.y;
    float4x4 rows=transpose(view.vp);
    float4 planes[6]={rows[0]-low.x*rows[3],high.x*rows[3]-rows[0],
        rows[1]-bottom*rows[3],top*rows[3]-rows[1],rows[3]+rows[2],rows[3]-rows[2]};
    uint words[68];for(uint j=0;j<68;++j)words[j]=0;
    uint count=0;
    for(uint i=0;i<uint(lights.meta.x);++i){
        float4 light=lights.values[i*3];bool rejected=false;
        for(uint p=0;p<6;++p)rejected|=outside(planes[p],light.xyz,light.w);
        if(rejected)continue;
        if(count<64)words[count+1]=i;
        ++count;
    }
    words[0]=count; // >64 is explicit overflow; forward shading evaluates all lights.
    for(uint j=0;j<17;++j)tiles.write(uint4(words[j*4],words[j*4+1],words[j*4+2],words[j*4+3]),uint2(tile.x*17+j,tile.y));
}
