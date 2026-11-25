#include <metal_stdlib>
using namespace metal;
struct TemporalConstants { float4 extentJitter; uint4 options; float4x4 skyMotion; };
float3 compressHDR(float3 c) { c=max(c,0.f);return c*(1.f/(1+dot(c,float3(.2126f,.7152f,.0722f)))); }
float3 expandHDR(float3 c) { return max(c,0.f)*(1.f/max(1-dot(c,float3(.2126f,.7152f,.0722f)),1e-5f)); }
float3 toYCoCg(float3 c) { return float3(dot(c,float3(.25f,.5f,.25f)),c.r*.5f-c.b*.5f,-c.r*.25f+c.g*.5f-c.b*.25f); }
float3 fromYCoCg(float3 c) { return float3(c.x+c.y-c.z,c.x+c.z,c.x-c.y-c.z); }
uint2 boundPixel(int2 p,uint2 size) { return uint2(clamp(p,int2(0),int2(size)-1)); }
float cubic(float x) { x=abs(x);return x<1 ? 1.5f*x*x*x-2.5f*x*x+1 : x<2 ? -.5f*x*x*x+2.5f*x*x-4*x+2 : 0; }
kernel void temporalResolve(constant TemporalConstants& c [[buffer(0)]],
    texture2d<float,access::read> current [[texture(0)]],
    texture2d<float,access::read> motion [[texture(1)]],
    texture2d<float,access::read> depth [[texture(2)]],
    texture2d<float,access::read> history [[texture(3)]],
    texture2d<int,access::read> ids [[texture(5)]],
    texture2d<int,access::read> oldIDs [[texture(6)]],
    texture2d<float,access::write> result [[texture(7)]],
    texture2d<float,access::write> display [[texture(8)]],
    texture2d<float,access::read> jitterDepth [[texture(9)]],
    texture2d<int,access::read> jitterIDs [[texture(10)]],
    texture2d<int,access::write> resultIDs [[texture(11)]], uint2 p [[thread_position_in_grid]])
{
    uint2 size=uint2(c.extentJitter.xy);
    if(any(p>=size))return;
    float2 uv=(float2(p)+.5f)/float2(size),samplePosition=float2(p)+c.extentJitter.zw;
    int id=ids.read(p).x;
    float d=depth.read(p).x;
    int2 base=int2(floor(samplePosition));
    float2 fraction=samplePosition-float2(base);
    float4 wx=float4(cubic(fraction.x+1),cubic(fraction.x),cubic(fraction.x-1),cubic(fraction.x-2));
    float4 wy=float4(cubic(fraction.y+1),cubic(fraction.y),cubic(fraction.y-1),cubic(fraction.y-2));
    float3 minimumRGB=float3(1e30f),maximumRGB=0;
    float alphaSum=0,minimumAlpha=1e30f,maximumAlpha=-1e30f;
    float3 sum=0,lo=float3(1e30f),hi=float3(-1e30f),mean=0,moment=0;
    float weights=0,count=0,surfaceWeight=0,totalWeight=0;
    // Reconstruct jittered radiance at the stable output grid. Cross-surface
    // samples encode silhouette coverage; IDs/depth remain unfiltered separately.
    for(int y=-1;y<=2;++y)for(int x=-1;x<=2;++x) {
        uint2 q=boundPixel(base+int2(x,y),size);
        float4 radiance=current.read(q);
        float3 color=radiance.rgb;
        if(!all(isfinite(color)))continue;
        float w=wx[x+1]*wy[y+1];
        minimumRGB=min(minimumRGB,color);maximumRGB=max(maximumRGB,color);
        sum+=color*w;weights+=w;
        alphaSum+=radiance.a*w;minimumAlpha=min(minimumAlpha,radiance.a);maximumAlpha=max(maximumAlpha,radiance.a);
        totalWeight+=abs(w);
        // With Less depth and a -1 clear, every other exact ID implies geometry.
        if(jitterIDs.read(q).x==id && (id!=-1 || d>=1 || jitterDepth.read(q).x<1))surfaceWeight+=abs(w);
        if(x<=1 && y<=1) {
            float3 v=toYCoCg(compressHDR(color));lo=min(lo,v);hi=max(hi,v);mean+=v;moment+=v*v;count+=1;
        }
    }
    float3 now=weights>.01f?max(sum*(1.f/weights),0.f):max(current.read(p).rgb,0.f);
    if(!all(isfinite(now)))now=0;
    if(weights>.01f)now=clamp(now,minimumRGB,maximumRGB);
    // Alpha remains current spatial coverage. History alpha stores depth;
    // preserve fractional backgrounds for the later world-2D composition.
    float alpha=weights>.01f?clamp(alphaSum/weights,minimumAlpha,maximumAlpha):current.read(p).a;
    float3 currentColor=toYCoCg(compressHDR(now));
    float3 resolved=now;
    if(count>0) {
        currentColor=clamp(currentColor,lo,hi); // Bound cubic ringing, retain the current highlight.
        float inverseCount=1.f/count;mean*=inverseCount;
        float3 sigma=sqrt(max(moment*inverseCount-mean*mean,0.f));
        lo=max(lo,mean-1.25f*sigma);hi=min(hi,mean+1.25f*sigma);
    } else lo=hi=currentColor;
    float4 previous=motion.read(p);
    bool sky=d>=1;
    if(sky) {
        float4 h=c.skyMotion*float4(uv*2-1,1,1);
        previous=float4(h.xy/max(h.w,1e-10f)*.5f+.5f,1,c.options.z==1 && h.w>0?1.f:0.f);
        if(c.options.z==2)previous=float4(uv,1,1);
    }
    float2 oldUV=previous.xy;
    bool valid=c.options.x && previous.w>.5f && previous.z>=0 && (sky || previous.z<1) &&
        all(oldUV>=0) && all(oldUV<1) && count>0;
    float3 past=0;
    float accepted=0;
    if(valid) {
        float2 oldPosition=oldUV*float2(size)-.5f;
        int2 b=int2(floor(oldPosition));float2 f=fract(oldPosition);
        // Validate all bilinear taps, not just the nearest one. A rejected tap
        // cannot leak an old occluder into newly visible geometry.
        for(int y=0;y<2;++y)for(int x=0;x<2;++x) {
            int2 q=b+int2(x,y);if(any(q<0)||any(q>=int2(size)))continue;
            float4 sample=history.read(uint2(q));float z=sample.a;
            if(oldIDs.read(uint2(q)).x!=id || abs(z-previous.z)>max(2e-5f,.002f*(1-previous.z)))continue;
            float w=(x?f.x:1-f.x)*(y?f.y:1-f.y);
            float3 h=sample.rgb;
            if(!all(isfinite(h)))continue;
            past+=h*w;accepted+=w;
        }
    }
    valid=valid && accepted>.5f;
    float contribution=0;
    if(valid) {
        past*=1.f/accepted;
        float3 h=toYCoCg(compressHDR(past));
        // Clip toward the variance-box centre, preserving chroma direction.
        float3 centre=(lo+hi)*.5f,extent=max((hi-lo)*.5f,1e-5f),delta=h-centre;
        float3 distance=abs(delta)/extent;
        float scale=max(max(distance.x,distance.y),distance.z);
        h=centre+delta/max(scale,1.f);
        float speed=length((uv-oldUV)*float2(size));
        float luminance=dot(now,float3(.2126f,.7152f,.0722f));
        float oldLuminance=dot(past,float3(.2126f,.7152f,.0722f));
        float change=max(abs(h.x-currentColor.x)/max(max(h.x,currentColor.x),.05f),
            abs(luminance-oldLuminance)/max(max(luminance,oldLuminance),.05f));
        float coverage=surfaceWeight/max(totalWeight,1e-5f);
        contribution=clamp(.92f-speed*.025f,.65f,.92f)*accepted*(1-saturate(change*2))*mix(.7f,1.f,coverage);
        // Accumulate original scene-linear HDR. Compressed coordinates guide
        // clipping/response only; avoid imposing the inverse transform's finite
        // precision ceiling on current radiance or an already in-range history.
        float3 clipped=scale<=1.f?past:expandHDR(fromYCoCg(h));
        // The clipping-coordinate epsilon must not admit a visibly different
        // bright history when a flat HDR neighbourhood has a zero-width box.
        clipped=clamp(clipped,minimumRGB,maximumRGB);
        resolved=mix(now,clipped,contribution);
    }
    if(!all(isfinite(resolved)))resolved=now;
    result.write(float4(resolved,d),p);
    resultIDs.write(int4(id),p);
    float3 debug=resolved;
    if(c.options.y==1)debug=float3(.5f+(uv-oldUV)*float2(size)/32,0);
    if(c.options.y==2)debug=float3(c.extentJitter.zw+.5f,0);
    if(c.options.y==3)debug=valid?float3(0,1,0):float3(1,0,0);
    if(c.options.y==4)debug=float3(contribution);
    if(c.options.y==5)debug=min(resolved,1.f);
    display.write(float4(debug,alpha),p);
}
