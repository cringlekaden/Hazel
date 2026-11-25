// Copyright (C) 2016-2021, Intel Corporation
// SPDX-License-Identifier: MIT
// Adapted for Hazel from XeGTAO.h/.hlsli, a5b1686c7ea37788eeb3576b5be47f7c03db532c.
// See LICENSE. FP32 scalar visibility, fixed Hilbert/R2 noise, lower-left rows.
#include <metal_stdlib>
using namespace metal;
struct AOConstants
{
    float4x4 inverseVP;
    float4x4 worldToView;
    float4 extentRadius; // width, height, world radius, falloff fraction
    float4 positionScaleBias; // UV to view XY, scaled by Z for perspective
    uint4 quality;       // slices, steps per side, orthographic, source mip
};
constant float pi = 3.14159265358979323846f;
constexpr sampler depthSampler(coord::normalized, address::clamp_to_edge, filter::nearest, mip_filter::nearest);
float3 worldPosition(float2 uv, float depth, constant AOConstants& c)
{
    float4 p = c.inverseVP * float4(uv * 2 - 1, depth * 2 - 1, 1);
    return p.xyz / p.w;
}
float3 viewPosition(float2 uv, float z, constant AOConstants& c)
{
    float2 xy=uv*c.positionScaleBias.xy+c.positionScaleBias.zw;
    return float3(c.quality.z?xy:xy*z,z);
}
uint2 bounded(int2 p, uint2 size) { return uint2(clamp(p, int2(0), int2(size) - 1)); }
float4 edges(float z, float l, float r, float t, float b)
{
    float4 d = float4(l, r, t, b) - z;
    float2 slope = float2(d.y - d.x, d.w - d.z) * .5f;
    d = min(abs(d), abs(d + float4(slope.x, -slope.x, slope.y, -slope.y)));
    return saturate(1.25f - d / max(z * .011f, 1e-8f));
}
// Upstream's fitted acos approximation is part of its tuned integral/heuristics.
float fastACos(float x)
{
    float a=abs(x);
    float root=as_type<float>(0x1fbd1df5+(as_type<int>(1-a)>>1));
    float result=(-.156583f*a+1.570796f)*root;
    return x>=0?result:3.141593f-result;
}
uint hilbert(uint2 p)
{
    p &= 63u;
    uint index = 0;
    for (uint level = 32; level; level /= 2)
    {
        uint2 region = uint2((p & level) > 0u);
        index += level * level * ((3u * region.x) ^ region.y);
        if (!region.y) { if (region.x) p = 63u - p; p = p.yx; }
    }
    return index;
}
kernel void aoDepth(constant AOConstants& c [[buffer(0)]],
    texture2d<float, access::read> source [[texture(0)]],
    texture2d<float, access::write> target [[texture(1)]], uint2 p [[thread_position_in_grid]])
{
    if (any(p >= uint2(target.get_width(), target.get_height()))) return;
    float d = source.read(p).x;
    float z = d >= 1 ? 1e20f : (c.worldToView * float4(worldPosition((float2(p)+.5f)/c.extentRadius.xy, d, c), 1)).z;
    target.write(float4(z), p);
}
kernel void aoMip(constant AOConstants& c [[buffer(0)]],
    texture2d<float, access::read> source [[texture(0)]],
    texture2d<float, access::write> target [[texture(1)]], uint2 p [[thread_position_in_grid]])
{
    uint2 size(target.get_width(), target.get_height());
    if (any(p >= size)) return;
    uint2 inputSize(source.get_width(), source.get_height());
    float4 d(source.read(bounded(int2(p*2),inputSize)).x,
        source.read(bounded(int2(p*2)+int2(1,0),inputSize)).x,
        source.read(bounded(int2(p*2)+int2(0,1),inputSize)).x,
        source.read(bounded(int2(p*2)+int2(1,1),inputSize)).x);
    float radius = .75f * 1.457f * c.extentRadius.z;
    float4 w = saturate((radius - (max(max(d.x,d.y),max(d.z,d.w))-d)) / (radius*c.extentRadius.w));
    target.write(float4(dot(w,d)/dot(w,float4(1))),p);
}
inline void gtaoMain(constant AOConstants& c,
    texture2d<float> depth, texture2d<float, access::write> output,
    texture2d<float, access::write> outputEdges, uint2 p, uint sliceCount, uint stepCount)
{
    uint2 size(depth.get_width(),depth.get_height());
    if (any(p >= size)) return;
    float z = depth.read(p).x;
    if (z >= 1e19f) { output.write(float4(1),p); outputEdges.write(float4(0),p); return; }
    int2 pixel=int2(p);
    float l=depth.read(bounded(pixel+int2(-1,0),size)).x, r=depth.read(bounded(pixel+int2(1,0),size)).x;
    float t=depth.read(bounded(pixel+int2(0,1),size)).x, b=depth.read(bounded(pixel+int2(0,-1),size)).x;
    float4 e=edges(z,l,r,t,b);
    outputEdges.write(e,p);
    float2 uv=(float2(p)+.5f)/float2(size), texel=1/float2(size);
    float3 center=viewPosition(uv,z,c);
    float3 L=normalize(viewPosition(uv+float2(-1,0)*texel,l,c)-center);
    float3 R=normalize(viewPosition(uv+float2(1,0)*texel,r,c)-center);
    float3 T=normalize(viewPosition(uv+float2(0,1)*texel,t,c)-center);
    float3 B=normalize(viewPosition(uv+float2(0,-1)*texel,b,c)-center);
    float4 accepted=saturate(float4(e.x*e.z,e.z*e.y,e.y*e.w,e.w*e.x)+.01f);
    float3 normal=accepted.x*cross(L,T)+accepted.y*cross(T,R)+accepted.z*cross(R,B)+accepted.w*cross(B,L);
    normal=dot(normal,normal)>1e-12f?normalize(normal):float3(0,0,-1);
    center=viewPosition(uv,z*.99999f,c);
    float3 view=c.quality.z?float3(0,0,-1):normalize(-center);
    float radius=c.extentRadius.z*1.457f;
    float pixelSize=length(viewPosition(uv+float2(texel.x,0),z,c)-viewPosition(uv,z,c));
    float screenRadius=radius/max(pixelSize,1e-8f);
    float visibility=saturate((10-screenRadius)/100)*.5f;
    float unoccludedIntegral=0;
    float2 noise=fract(.5f+float(hilbert(p)+c.quality.w)*float2(.75487766624669276f,.56984029099805326f));
    for (uint slice=0;slice<sliceCount;++slice)
    {
        float phi=(float(slice)+noise.x)/float(sliceCount)*pi;
        float3 direction(cos(phi),sin(phi),0);
        float3 ortho=direction-dot(direction,view)*view;
        float3 axis=normalize(cross(ortho,view));
        float3 projected=normal-axis*dot(normal,axis);
        float projectedLength=length(projected);
        float cosNorm=saturate(dot(projected,view)/max(projectedLength,1e-8f));
        float n=sign(dot(ortho,projected))*fastACos(cosNorm);
        float2 low=cos(n+float2(pi*.5f,-pi*.5f)), horizon=low;
        for (uint step=0;step<stepCount;++step)
        {
            float jitter=fract(noise.y+float(slice+step*stepCount)*.6180339887498948482f);
            float s=(float(step)+jitter)/float(stepCount);
            s=s*s+1.3f/screenRadius;
            float2 offset=s*direction.xy*screenRadius;
            float mip=clamp(log2(max(length(offset),1e-8f))-3.30f,0.0f,float(depth.get_num_mip_levels()-1));
            offset=round(offset)*texel;
            for (uint side=0;side<2;++side)
            {
                float2 sampleUV=uv+offset*(side==0?1.0f:-1.0f);
                if (any(sampleUV<0)||any(sampleUV>1)) continue;
                float sampleZ=depth.sample(depthSampler,sampleUV,level(mip)).x;
                if (sampleZ>=1e19f) continue;
                float3 delta=viewPosition(sampleUV,sampleZ,c)-center;
                float distance=length(delta);
                float weight=saturate((radius-distance)/(radius*c.extentRadius.w));
                float h=dot(delta/max(distance,1e-8f),view);
                horizon[side]=max(horizon[side],mix(low[side],h,weight));
            }
        }
        projectedLength=mix(projectedLength,1.0f,.05f);
        float2 h=float2(-fastACos(clamp(horizon.y,-1.0f,1.0f)),fastACos(clamp(horizon.x,-1.0f,1.0f)));
        float2 integral=(cosNorm+2*h*sin(n)-cos(2*h-n))*.25f;
        visibility+=projectedLength*(integral.x+integral.y);
        // Finite angular quadrature has a view/slope-dependent DC bias, even
        // for an isolated plane. Evaluate the same analytic XeGTAO integral
        // over its unobstructed tangent hemisphere to preserve unit visibility.
        // This changes normalization only; sampled horizons still own occlusion.
        float2 openH=float2(-fastACos(clamp(low.y,-1.0f,1.0f)),fastACos(clamp(low.x,-1.0f,1.0f)));
        float2 openIntegral=(cosNorm+2*openH*sin(n)-cos(2*openH-n))*.25f;
        unoccludedIntegral+=projectedLength*(openIntegral.x+openIntegral.y);
    }
    // Keep overshoot until denoising, as upstream's 1.5-scaled working UNORM does.
    visibility=max(.03f,pow(max(visibility/max(unoccludedIntegral,1e-6f),0.0f),2.2f));
    output.write(float4(min(visibility,1.5f)),p);
}
// Match upstream's four constant-quality entry points so the Metal compiler can
// specialize the horizon loops, while retaining one implementation of the math.
#define AO_MAIN_KERNEL(name, slices, steps) \
    kernel void name(constant AOConstants& c [[buffer(0)]], \
        texture2d<float> depth [[texture(0)]], texture2d<float, access::write> output [[texture(1)]], \
        texture2d<float, access::write> outputEdges [[texture(2)]], uint2 p [[thread_position_in_grid]]) \
    { gtaoMain(c,depth,output,outputEdges,p,slices,steps); }
AO_MAIN_KERNEL(aoMainLow,1,2)
AO_MAIN_KERNEL(aoMainMedium,2,2)
AO_MAIN_KERNEL(aoMainHigh,3,3)
AO_MAIN_KERNEL(aoMainUltra,9,3)
#undef AO_MAIN_KERNEL
kernel void aoDenoise(constant AOConstants& c [[buffer(0)]],
    texture2d<float,access::read> source [[texture(0)]], texture2d<float,access::write> target [[texture(1)]],
    texture2d<float,access::read> edgeTexture [[texture(2)]], uint2 p [[thread_position_in_grid]])
{
    uint2 size(source.get_width(),source.get_height());
    if (any(p>=size)) return;
    int2 q=int2(p);
    float4 l=edgeTexture.read(bounded(q+int2(-1,0),size));
    float4 r=edgeTexture.read(bounded(q+int2(1,0),size));
    float4 t=edgeTexture.read(bounded(q+int2(0,1),size));
    float4 b=edgeTexture.read(bounded(q+int2(0,-1),size));
    float4 e=edgeTexture.read(p)*float4(l.y,r.x,t.w,b.z);
    float edginess=saturate(1.5f-dot(e,float4(1)))/1.5f*.5f;
    e=saturate(e+edginess);
    float4 diagonal=.425f*float4(e.x*l.z+e.z*t.x,e.z*t.y+e.y*r.z,e.w*b.x+e.x*l.w,e.y*r.w+e.w*b.y);
    float beta=c.quality.w?1.2f:.24f;
    float sum=source.read(p).x*beta, weight=beta;
    int2 offsets[8]={int2(-1,0),int2(1,0),int2(0,1),int2(0,-1),int2(-1,1),int2(1,1),int2(-1,-1),int2(1,-1)};
    for(uint i=0;i<8;++i) { float w=i<4?e[i]:diagonal[i-4]; sum+=w*source.read(bounded(q+offsets[i],size)).x; weight+=w; }
    target.write(float4(c.quality.w?saturate(sum/weight):sum/weight),p);
}
