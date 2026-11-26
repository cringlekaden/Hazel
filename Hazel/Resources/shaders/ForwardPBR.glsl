// Direct_v1: single-scattering GGX, correlated Smith V, Schlick F, Lambert diffuse.
#type vertex
#version 450 core
layout(location=0) in vec3 a_Position;
layout(location=1) in vec3 a_Normal;
layout(location=2) in vec4 a_Tangent;
layout(location=3) in vec2 a_UV;
layout(location=4) in vec4 a_Color;
layout(std140,binding=2) uniform View {mat4 u_VP;vec4 u_Eye;vec4 u_ToCamera;vec4 u_Light;vec4 u_Incident;};
layout(std140,binding=3) uniform Object {mat4 u_Model;mat4 u_NormalMatrix;ivec4 u_Identity;};
layout(location=0) out vec3 v_Position;
layout(location=1) out vec3 v_Normal;
layout(location=2) out vec4 v_Tangent;
layout(location=3) out vec2 v_UV;
layout(location=4) out vec4 v_Color;
void main(){vec4 world=u_Model*vec4(a_Position,1);v_Position=world.xyz;v_Normal=normalize(mat3(u_NormalMatrix)*a_Normal);vec3 t=mat3(u_Model)*a_Tangent.xyz;v_Tangent=vec4(normalize(t-v_Normal*dot(t,v_Normal)),a_Tangent.w*float(u_Identity.y));v_UV=a_UV;v_Color=a_Color;gl_Position=u_VP*world;}
#type fragment
#version 450 core
layout(location=0) in vec3 v_Position;
layout(location=1) in vec3 v_Normal;
layout(location=2) in vec4 v_Tangent;
layout(location=3) in vec2 v_UV;
layout(location=4) in vec4 v_Color;
layout(location=0) out vec4 o_Color;
layout(location=1) out int o_Entity;
layout(std140,binding=2) uniform View {mat4 u_VP;vec4 u_Eye;vec4 u_ToCamera;vec4 u_Light;vec4 u_Incident;};
layout(std140,binding=3) uniform Object {mat4 u_Model;mat4 u_NormalMatrix;ivec4 u_Identity;};
layout(std140,binding=4) uniform Material {vec4 u_BaseFactor;vec4 u_EmissionCutoff;vec4 u_Surface;vec4 u_Options;};
layout(std140,binding=7) uniform LocalLights {vec4 u_LocalMeta;vec4 u_LocalLights[960];};
layout(std140,binding=5) uniform DirectionalShadows {
    mat4 u_ShadowVP[4];vec4 u_ShadowSplits;vec4 u_ShadowEye;vec4 u_ShadowForward;
    vec4 u_ShadowParameters;vec4 u_ShadowRange;vec4 u_ShadowTexelDepth[4];
};
layout(binding=10) uniform sampler2D u_ShadowAtlas;
layout(std140,binding=1) uniform SpotShadows {
    mat4 u_SpotVP[4];
    vec4 u_SpotTiles[4];
    ivec4 u_SpotLightIndices;
};
layout(binding=11) uniform sampler2D u_SpotShadowAtlas;
layout(std140,binding=0) uniform PointShadows {vec4 u_PointShadowParameters;};
layout(binding=12) uniform sampler2D u_PointShadowAtlas;
#ifdef HZ_METAL
layout(binding=9) uniform usampler2D u_LightTiles;
#endif
layout(binding=0) uniform sampler2D u_Base;
layout(binding=1) uniform sampler2D u_MetalRough;
layout(binding=2) uniform sampler2D u_Normal;
layout(binding=3) uniform sampler2D u_Occlusion;
layout(binding=4) uniform sampler2D u_Emissive;
layout(binding=5) uniform samplerCube u_Irradiance;
layout(binding=6) uniform samplerCube u_Specular;
layout(binding=7) uniform sampler2D u_BRDF;
#ifdef HZ_METAL
layout(binding=8) uniform sampler2D u_ScreenAO;
#endif
layout(binding=13) uniform samplerCube u_ProbeSpecular0;
layout(binding=14) uniform samplerCube u_ProbeSpecular1;
#ifdef HZ_METAL
layout(binding=15) uniform samplerCube u_ProbeCosine0;
layout(binding=16) uniform samplerCube u_ProbeCosine1;
#else
layout(binding=8) uniform samplerCube u_ProbeCosine0;
layout(binding=9) uniform samplerCube u_ProbeCosine1;
#endif
layout(std140,binding=6) uniform Environment {mat4 u_InverseVP;vec4 u_EnvRotationGain;vec4 u_EnvOptions;vec4 u_ReflectionProbes[8];};
vec3 environmentDirection(vec3 d){float c=u_EnvRotationGain.x,s=u_EnvRotationGain.y;return vec3(c*d.x-s*d.z,d.y,s*d.x+c*d.z);}
const float PI=3.141592653589793;
vec3 specularLevel(vec3 direction,float level){
    // At r=1, normalized GGX with V=N is exactly the cosine kernel E/pi.
    // Reconstruct that signal from its dense irradiance representation instead
    // of the 4x4 specular tail. Preserve the existing split-sum lookup direction,
    // lobe, HDR energy and LUT; no new bake, extra blur or lighting model.
    if(level>=u_EnvRotationGain.w)return textureLod(u_Irradiance,direction,0).rgb/PI;
    return textureLod(u_Specular,direction,level).rgb;
}
// The influence box is contained by the proxy, and the capture lies inside both.
vec3 probeDirection(vec3 relativePosition, vec3 direction, vec3 center, vec3 halfSize)
{
    vec3 exitFace = mix(center - halfSize, center + halfSize, greaterThan(direction, vec3(0)));
    vec3 speed = abs(direction);
    vec3 span = max((exitFace - relativePosition) * sign(direction), vec3(0));
    float distance = 1e30;
    // Compare before dividing to avoid division by zero and overflow even
    // when the nearest face is almost parallel to the outgoing ray.
    for (int axis = 0; axis < 3; ++axis)
        if (span[axis] < distance * speed[axis])
            distance = span[axis] / speed[axis];
    vec3 projected = relativePosition + distance * direction;
    return dot(projected, projected) > 1e-12 ? projected : direction;
}
vec3 probePrefilter(samplerCube specular, samplerCube cosine, vec3 direction, float roughness, float lastLevel)
{
    float lod = roughness * lastLevel, level = floor(lod);
    if (level >= lastLevel)
        return textureLod(cosine, direction, 0).rgb / PI;
    vec3 lower = textureLod(specular, direction, level).rgb;
    vec3 upper = level + 1 >= lastLevel ? textureLod(cosine, direction, 0).rgb / PI :
                                        textureLod(specular, direction, level + 1).rgb;
    return mix(lower, upper, fract(lod));
}
vec4 sampleProbe(int slot, samplerCube specular, samplerCube cosine, vec3 direction, float roughness)
{
    vec4 capture = u_ReflectionProbes[slot];
    vec4 influence = u_ReflectionProbes[slot + 1];
    vec3 position = v_Position - capture.xyz;
    vec3 margin = influence.xyz - abs(position);
    float weight = smoothstep(0, capture.w, min(min(margin.x, margin.y), margin.z));
    if (weight <= 0)
        return vec4(0);
    vec3 corrected = probeDirection(position, direction, u_ReflectionProbes[slot + 2].xyz,
                                    u_ReflectionProbes[slot + 3].xyz);
    return vec4(probePrefilter(specular, cosine, corrected, roughness, influence.w) * weight, weight);
}
vec3 resolveShadingNormal(vec3 N,vec3 Ng,vec3 V){
    // Raster coverage belongs to the geometric surface. Interpolated/mapped
    // normals must not turn a visible sample into a back-facing BRDF or send
    // its mirror direction beneath that surface. Rotate only as far toward Ng
    // as the geometric reflection horizon requires, in the original N/Ng plane.
    float a=dot(Ng,V),c=dot(N,Ng);
    if(a<=0||(c>0&&dot(reflect(-V,N),Ng)>=0))return N;
    vec3 tangent=N-Ng*c;
    if(dot(tangent,tangent)<1e-12)return Ng;
    tangent=normalize(tangent);
    // For N(theta)=Ng*cos(theta)+T*sin(theta), R.Ng =
    // a*cos(2*theta)+b*sin(2*theta). Solve its first zero with stable
    // half angles (including b=0 and grazing a); no atan-axis ambiguity.
    float b=dot(tangent,V),h=length(vec2(a,b)),co,si;
    if(b<=0){co=sqrt(.5*(1-b/h));si=a/(2*h*co);}
    else{si=sqrt(.5*(1+b/h));co=a/(2*h*si);}
    return normalize(Ng*co+tangent*si);
}
// Chiang et al. 2019, applied only to material-normal deviation from
// the continuous authored surface. It is identity for unmapped normals.
float surfaceVisibility(vec3 surfaceNormal,vec3 N,vec3 L){
    float cosine=dot(surfaceNormal,L);
    if(cosine<=0)return 0;
    float projected=dot(N,L)*dot(surfaceNormal,N);
    if(cosine>=projected)return 1;
    float g=cosine/projected;
    return g*(1+g*(1-g));
}
// Multiscattering_v2: reciprocal Kulla-Conty plus scalar complementary diffuse.
// RG is RuntimeBRDF_v2 A/B; BA holds matching cosine-weighted means.
vec4 sampleBRDF(float nv,float rough){
    vec2 size=vec2(textureSize(u_BRDF,0));
    vec2 coordinate=vec2(sqrt(sqrt(nv)),(max(rough,.045)-.045)/.955);
    return texture(u_BRDF,(coordinate*(size-1)+.5)/size);
}
struct SurfaceEnergy {
    vec4 table;
    vec3 multipleFresnel;
    float missingView;
    float missingAverage;
    float diffuseK;
    float diffuseScale;
    float diffuseShape;
    float diffuseAverage;
};
SurfaceEnergy surfaceEnergy(vec3 f0,vec4 table){
    SurfaceEnergy energy;
    energy.table=table;
    energy.missingView=max(1-table.x-table.y,0);
    energy.missingAverage=max(1-table.z-table.w,0);
    float average=1-energy.missingAverage;
    vec3 f=f0+(1-f0)/21;
    // Algebraically equivalent denominator avoids subtracting near-equal values.
    vec3 denominator=(1-f)+f*average;
    for(int c=0;c<3;++c)
        energy.multipleFresnel[c]=denominator[c]>0?f[c]*f[c]*average/denominator[c]:1;
    float maximum=max(f0.x,max(f0.y,f0.z)),fm=maximum+(1-maximum)/21;
    // Factor (1-F0max) out of q and qbar. Diffuse tends continuously to zero
    // without dividing by a vanishing residual at the perfect-reflector limit.
    float k=maximum<1?(20.0/21)*(1+fm*average)/((1-fm)+fm*average):0;
    energy.diffuseK=k;
    energy.diffuseScale=1-maximum;
    energy.diffuseShape=table.x+k*energy.missingView;
    energy.diffuseAverage=table.z+k*energy.missingAverage;
    return energy;
}
vec3 direct(vec3 base,float metal,float rough,vec3 N,vec3 V,vec3 L,SurfaceEnergy energy){
    float nl=clamp(dot(N,L),0,1),nv=clamp(dot(N,V),0,1);if(nl<=0||nv<=0)return vec3(0);
    vec3 sum=V+L;if(dot(sum,sum)<1e-12)return vec3(0);vec3 H=normalize(sum);
    float nh=clamp(dot(N,H),0,1),vh=clamp(dot(V,H),0,1),a=pow(max(rough,.045),2),a2=a*a;
    // Equivalent unit-vector GGX denominator; retain small tangent energy
    // instead of cancelling nearly equal FP32 values at polished highlights.
    vec3 tangentH=cross(N,H);float f=dot(tangentH,tangentH)+a2*nh*nh;float D=a2/(PI*f*f);
    float visibility=.5/(nl*sqrt(nv*nv*(1-a2)+a2)+nv*sqrt(nl*nl*(1-a2)+a2));
    vec3 F0=mix(vec3(.04),base,metal);vec3 F=F0+(1-F0)*pow(1-vh,5);
    vec2 lightAB=sampleBRDF(nl,rough).rg;
    float missingLight=max(1-lightAB.x-lightAB.y,0);
    float diffuseLight=lightAB.x+energy.diffuseK*missingLight;
    // When average missing energy is exactly zero, both deficits vanish and
    // the analytic compensation limit is zero. No roughness/view threshold.
    vec3 multiple=energy.missingAverage>0?
        energy.multipleFresnel*(energy.missingView*missingLight/(PI*energy.missingAverage)):vec3(0);
    float diffuse=energy.diffuseAverage>0?
        energy.diffuseScale*energy.diffuseShape*diffuseLight/(PI*energy.diffuseAverage):0;
    return ((1-metal)*base*diffuse+D*visibility*F+multiple)*nl;
}
float cascadeVisibility(int cascade,vec3 position,vec3 Ng,vec3 L){
    float texel=u_ShadowTexelDepth[cascade].x, inverseDepth=u_ShadowTexelDepth[cascade].y;
    float cosine=dot(Ng,L);
    float bias=u_ShadowRange.w==10?0:1;
    // Receiver offset uses actual geometry, never a mapped/smooth BRDF normal.
    vec3 normal=cosine<0?-Ng:Ng;
    vec3 offset=normal*(bias*u_ShadowParameters.y*texel*sqrt(max(1-cosine*cosine,0)));
    mat4 matrix=u_ShadowVP[cascade];
    vec3 p=(matrix*vec4(position+offset,1)).xyz*.5+.5;
    if(any(lessThan(p,vec3(0)))||any(greaterThan(p,vec3(1))))return 1;
    int resolution=int(u_ShadowParameters.x);
    ivec2 origin=ivec2(cascade%2,cascade/2)*resolution;
    vec2 coordinate=p.xy*float(resolution)-.5;
    ivec2 centre=ivec2(floor(coordinate));vec2 fraction=fract(coordinate);
    vec3 right=normalize(vec3(matrix[0][0],matrix[1][0],matrix[2][0]));
    vec3 up=normalize(vec3(matrix[0][1],matrix[1][1],matrix[2][1]));
    // Receiver-plane correction evaluates PCF taps on the geometric plane.
    // Bound grazing slopes to avoid singular offsets and unbounded light leaks.
    float denominator=abs(cosine)>.05?cosine:(cosine<0?-.05:.05);
    vec2 slope=clamp(vec2(dot(Ng,right),dot(Ng,up))/denominator,vec2(-8),vec2(8));
    float visibility=0;
    // Average nine bilinear comparison lookups with their shared 4x4 footprint.
    // The separable weights sum to one and remain continuous at texel boundaries.
    // Explicit comparisons keep Metal/OpenGL sampling identical without relying
    // on backend-specific hardware comparison samplers (16 depth fetches).
    for(int y=-1;y<=2;++y)for(int x=-1;x<=2;++x){
        ivec2 tap=clamp(centre+ivec2(x,y),ivec2(0),ivec2(resolution-1));
        vec2 displacement=(vec2(tap)+.5-p.xy*float(resolution))*texel;
        float receiver=p.z+dot(slope,displacement)*inverseDepth-bias*u_ShadowParameters.z*texel*inverseDepth;
        vec2 weight=vec2(x==-1?1-fraction.x:x==2?fraction.x:1,y==-1?1-fraction.y:y==2?fraction.y:1)/3;
        visibility+=(receiver<=texelFetch(u_ShadowAtlas,origin+tap,0).r?1:0)*weight.x*weight.y;
    }
    return visibility;
}
float directionalVisibility(vec3 position,vec3 Ng,vec3 L){
    if(u_ShadowRange.z<.5)return 1;
    float depth=dot(position-u_ShadowEye.xyz,u_ShadowForward.xyz);
    if(depth<u_ShadowRange.x||depth>=u_ShadowRange.y)return 1;
    int cascade=depth>u_ShadowSplits.z?3:depth>u_ShadowSplits.y?2:depth>u_ShadowSplits.x?1:0;
    float begin=cascade==0?u_ShadowRange.x:u_ShadowSplits[cascade-1];
    float end=u_ShadowSplits[cascade];
    float blend=smoothstep(end-(end-begin)*u_ShadowParameters.w,end,depth);
    float visibility=cascadeVisibility(cascade,position,Ng,L);
    if(blend>0)visibility=mix(visibility,cascade==3?1:cascadeVisibility(cascade+1,position,Ng,L),blend);
    return visibility;
}
vec3 shadowInspection(vec3 position,vec3 Ng,vec3 L){
    if(u_ShadowRange.z<.5)return vec3(1);
    float depth=dot(position-u_ShadowEye.xyz,u_ShadowForward.xyz);
    int cascade=depth>u_ShadowSplits.z?3:depth>u_ShadowSplits.y?2:depth>u_ShadowSplits.x?1:0;
    float begin=cascade==0?u_ShadowRange.x:u_ShadowSplits[cascade-1],end=u_ShadowSplits[cascade];
    float blend=smoothstep(end-(end-begin)*u_ShadowParameters.w,end,depth);
    float scale=u_ShadowTexelDepth[cascade].x;
    int mode=int(u_ShadowRange.w);
    if(mode==1)return vec3(directionalVisibility(position,Ng,L));
    if(mode==2){vec3 colors[4]=vec3[](vec3(1,.15,.1),vec3(.1,1,.15),vec3(.1,.2,1),vec3(1,.8,.1));return mix(colors[cascade],vec3(1),blend*.6);}
    if(mode>=3&&mode<=6)return vec3(cascadeVisibility(mode-3,position,Ng,L));
    if(mode==7||mode==10)return vec3(cascadeVisibility(cascade,position,Ng,L),cascade==3?1:cascadeVisibility(cascade+1,position,Ng,L),blend);
    if(mode==8)return vec3(u_ShadowTexelDepth[cascade].x,cascade==3?u_ShadowTexelDepth[cascade].x:u_ShadowTexelDepth[cascade+1].x,depth);
    float cosine=dot(Ng,L);return vec3(u_ShadowParameters.y*scale*sqrt(max(1-cosine*cosine,0)),u_ShadowParameters.z*scale,depth);
}
float spotVisibility(int slot, vec3 lightRelativePosition, vec3 Ng, vec3 L, vec3 forward, float farDepth) {
    mat4 matrix = u_SpotVP[slot];
    vec4 tile = u_SpotTiles[slot];
    int resolution = int(tile.y);
    vec4 clip = matrix * vec4(lightRelativePosition, 1);
    if (clip.w <= tile.z || clip.w >= farDepth) return 1;

    // World-space texel size grows linearly with axial distance. Offset towards
    // the light-facing side of the actual receiver plane.
    float texel = 2 * clip.w * tile.w / tile.y;
    float coneCosine = dot(-L, forward);
    float cosine = dot(Ng, L);
    vec3 normal = cosine < 0 ? -Ng : Ng;
    vec3 offset = normal * (.2 * texel * coneCosine * sqrt(max(1 - cosine * cosine, 0)));
    clip += matrix * vec4(offset, 0);
    if (clip.w <= tile.z) return 1;
    vec2 uv = clip.xy / clip.w * .5 + .5;
    if (any(lessThan(uv, vec2(0))) || any(greaterThan(uv, vec2(1)))) return 1;

    vec3 right = vec3(matrix[0][0], matrix[1][0], matrix[2][0]) * tile.w;
    vec3 up = vec3(matrix[0][1], matrix[1][1], matrix[2][1]) * tile.w;
    vec2 coordinate = uv * tile.y - .5;
    ivec2 centre = ivec2(floor(coordinate));
    vec2 fraction = fract(coordinate);
    vec2 planeSlope = vec2(dot(normal, right), dot(normal, up));
    float centreDot = dot(normal, forward) + tile.w * dot(planeSlope, 2 * uv - 1);
    float grazingLimit = -.05 / max(coneCosine, .01);
    float rayStep = 2 * tile.w / tile.y;
    float slopeLimit = 8 * texel;
    float depthScale = farDepth / (farDepth - tile.z);
    float visibility = 0;
    // Each of 16 taps intersects a perspective ray with the geometric plane.
    // Bound the correction near grazing incidence and contain reads in this tile.
    for (int y = -1; y <= 2; ++y) for (int x = -1; x <= 2; ++x) {
        ivec2 tap = clamp(centre + ivec2(x, y), ivec2(0), ivec2(resolution - 1));
        float rayDelta = dot(planeSlope, vec2(tap) - coordinate) * rayStep;
        float correction = -clip.w * rayDelta / min(centreDot + rayDelta, grazingLimit);
        float receiverDepth = max(tile.z, clip.w + clamp(correction, -slopeLimit, slopeLimit) - .05 * texel);
        float receiver = depthScale * (1 - tile.z / receiverDepth);
        vec2 weight = vec2(x == -1 ? 1 - fraction.x : x == 2 ? fraction.x : 1,
                           y == -1 ? 1 - fraction.y : y == 2 ? fraction.y : 1) / 3;
        visibility += (receiver <= texelFetch(u_SpotShadowAtlas, ivec2(int(tile.x), 0) + tap, 0).r ? 1 : 0)
                      * weight.x * weight.y;
    }
    return mix(1, visibility, smoothstep(tile.z, 2 * tile.z, clip.w));
}
float pointFaceVisibility(int axis, vec3 p, vec3 normal, float cosine, float distanceToLight, float farDepth) {
    vec4 map = u_PointShadowParameters;
    int side = int(map.y);
    float signAxis = p[axis] < 0 ? -1 : 1;
    int face = 2 * axis + (signAxis < 0 ? 1 : 0);
    // Same RH look directions as FitPointShadows, with +Z up on the Y faces.
    vec3 forward = vec3(0);
    forward[axis] = signAxis;
    vec3 up = axis == 1 ? vec3(0, 0, 1) : vec3(0, 1, 0);
    vec3 right = cross(forward, up);
    float axial = abs(p[axis]);
    float stepSize = 2 / (map.y * map.w);
    float texel = axial * stepSize;
    float axisCosine = axial / distanceToLight;
    p += normal * (.2 * texel * axisCosine * sqrt(max(1 - cosine * cosine, 0)));
    axial = dot(p, forward);
    if (axial <= map.z) return 1;

    vec2 ray = vec2(dot(p, right), dot(p, up)) / axial;
    vec2 coordinate = (ray * map.w * .5 + .5) * map.y - .5;
    ivec2 base = ivec2(floor(coordinate));
    vec2 fraction = fract(coordinate);
    vec2 planeSlope = vec2(dot(normal, right), dot(normal, up));
    float planeDot = dot(normal, forward) + dot(planeSlope, ray);
    // Reciprocal axial depth on a plane is affine in the perspective ray.
    // Limit its slope only when the geometric plane is nearly edge-on.
    float inversePlaneDot = 1 / min(planeDot, -.01 / axisCosine);
    float biasedNearRatio = map.z / max(map.z, axial - .05 * texel);
    float depthScale = farDepth / (farDepth - map.z);
    ivec2 origin = ivec2(face % 3, face / 3) * side;
    float visibility = 0;
    for (int y = 0; y < 2; ++y) for (int x = 0; x < 2; ++x) {
        ivec2 tap = clamp(base + ivec2(x, y), ivec2(0), ivec2(side - 1));
        float rayDelta = dot(planeSlope, vec2(tap) - coordinate) * stepSize;
        float inverseDepthRatio = biasedNearRatio * (1 + rayDelta * inversePlaneDot);
        float receiver = clamp(depthScale * (1 - inverseDepthRatio), 0, 1);
        vec2 weight = mix(1 - fraction, fraction, vec2(x, y));
        visibility += (receiver <= texelFetch(u_PointShadowAtlas, origin + tap, 0).r ? 1 : 0) * weight.x * weight.y;
    }
    return visibility;
}
float pointVisibility(vec3 p, vec3 Ng, vec3 L, float distanceSquared, float farDepth) {
    float distanceToLight = sqrt(distanceSquared);
    float nearDepth = u_PointShadowParameters.z;
    if (distanceToLight <= 2 * nearDepth) return 1;

    vec3 magnitude = abs(p);
    int dominant = magnitude.x >= magnitude.y && magnitude.x >= magnitude.z ? 0 :
        magnitude.y >= magnitude.z ? 1 : 2;
    float cosine = dot(Ng, L);
    vec3 normal = cosine < 0 ? -Ng : Ng;
    float visibility = pointFaceVisibility(dominant, p, normal, cosine, distanceToLight, farDepth);
    // The same normalized weights apply from either side of a face tie.
    // At a corner all three axes can contribute. Most pixels only read four taps.
    float seamStart = 1 - 4 / u_PointShadowParameters.y;
    vec3 weights = smoothstep(vec3(seamStart), vec3(1), magnitude / magnitude[dominant]);
    weights[dominant] = 0;
    for (int axis = 0; axis < 3; ++axis) {
        if (weights[axis] > 0)
            visibility += weights[axis] * pointFaceVisibility(axis, p, normal, cosine, distanceToLight, farDepth);
    }
    return mix(1, visibility / (1 + weights.x + weights.y + weights.z),
               smoothstep(2 * nearDepth, 4 * nearDepth, distanceToLight));
}
void main(){
    // Evaluate geometric derivatives before a potentially nonuniform mask discard.
    vec3 geometricNormal=normalize(cross(dFdx(v_Position),dFdy(v_Position)));
    int maps=int(u_Options.z);vec4 base=u_BaseFactor*v_Color*texture(u_Base,v_UV);
    if(u_Options.y>.5&&base.a<u_EmissionCutoff.a)discard;
    vec3 result=base.rgb,directTerm=vec3(0),diffuseTerm=vec3(0),specularTerm=vec3(0);vec3 N=normalize(v_Normal);
    vec3 baseNormal=N;
    vec3 prefilteredProbe=vec3(0),brdfProbe=vec3(0),surfaceProbe=vec3(0),localProbe=vec3(0);
    vec3 view=u_Eye.w>.5?u_ToCamera.xyz:u_Eye.xyz-v_Position;vec3 V=dot(view,view)>1e-12?normalize(view):N;
    vec3 horizonProbe=vec3(dot(N,V),dot(geometricNormal,V),dot(N,V));
    vec3 T=normalize(v_Tangent.xyz-N*dot(v_Tangent.xyz,N));vec3 B=cross(N,T)*v_Tangent.w;
    float metal=clamp(u_Surface.x*texture(u_MetalRough,v_UV).b,0,1),rough=clamp(u_Surface.y*texture(u_MetalRough,v_UV).g,0,1);
    if(u_Options.x<.5){
        if((maps&4)!=0){vec3 sampleN=texture(u_Normal,v_UV).xyz*2-1;sampleN.xy*=u_Surface.z;if(dot(sampleN,sampleN)>1e-12)N=normalize(mat3(T,B,N)*normalize(sampleN));}
        if(u_Options.w>.5&&!gl_FrontFacing)N=-N;
        horizonProbe=vec3(dot(N,V),dot(geometricNormal,V),dot(N,V));
        // Direct lighting follows the authored smooth surface. Split normals
        // retain hard edges, while raster state still controls backface coverage.
        vec3 surfaceNormal=baseNormal;
        if(u_Options.w>.5&&!gl_FrontFacing)surfaceNormal=-surfaceNormal;
        vec3 directNormal=(maps&4)!=0?resolveShadingNormal(N,surfaceNormal,V):surfaceNormal;
        // Indirect reflection retains its accepted geometric-horizon repair.
        N=resolveShadingNormal(N,geometricNormal,V);
        horizonProbe.z=dot(N,V);
        float r=max(rough,.045);
        vec3 f0=mix(vec3(.04),base.rgb,metal);
        SurfaceEnergy directEnergy=surfaceEnergy(f0,sampleBRDF(clamp(dot(directNormal,V),0,1),r));
        vec3 L=normalize(u_Light.xyz);
        if(dot(surfaceNormal,V)>0&&dot(surfaceNormal,L)>0)
            directTerm=direct(base.rgb,metal,rough,directNormal,V,L,directEnergy)*(u_Incident.rgb*directionalVisibility(v_Position,geometricNormal,L))*((maps&4)!=0?surfaceVisibility(surfaceNormal,directNormal,L):1);
        vec3 directionalTerm=directTerm;
        int localCount=int(u_LocalMeta.x),listCount=localCount;
#ifdef HZ_METAL
        ivec2 tile=ivec2(gl_FragCoord.xy)/int(u_LocalMeta.w);
        // 17 uint4 texels: count followed by up to 64 indices. Overflow uses all lights.
        bool useTiles=(localCount>8||u_LocalMeta.z<0)&&u_LocalMeta.z<.5;
        uint tileCount=useTiles?texelFetch(u_LightTiles,ivec2(tile.x*17,tile.y),0).x:0u;
        bool tiled=useTiles && tileCount<=64u;
        if(tiled)listCount=int(tileCount);
#endif
        if(dot(surfaceNormal,V)>0)for(int entry=0;entry<listCount;++entry){
            int index=entry;
#ifdef HZ_METAL
            if(tiled){int word=entry+1;uvec4 packed=texelFetch(u_LightTiles,ivec2(tile.x*17+word/4,tile.y),0);index=int(packed[word%4]);}
#endif
            vec4 positionRange=u_LocalLights[index*3],incidentInner=u_LocalLights[index*3+1],directionOuter=u_LocalLights[index*3+2];
            vec3 delta=positionRange.xyz-v_Position;
            float distanceSquared=dot(delta,delta),rangeSquared=positionRange.w*positionRange.w;
            if(distanceSquared>=rangeSquared || distanceSquared==0)continue;
            vec3 localL=delta*inversesqrt(distanceSquared);
            if(dot(surfaceNormal,localL)<=0)continue;
            // Smooth finite range and smooth finite source core (one centimetre).
            // No max-distance brightness step; the undefined direction at exact coincidence
            // is assigned zero, with a continuous zero-energy limit inside the source core.
            float window=max(1-pow(distanceSquared/rangeSquared,2),0);
            float core=distanceSquared/(distanceSquared+1e-4);
            float attenuation=window*window*core*core/(distanceSquared+1e-4);
            if(directionOuter.w>=0){
                float cone=clamp((dot(-localL,directionOuter.xyz)-directionOuter.w)/(incidentInner.w-directionOuter.w),0,1);
                attenuation*=cone*cone*(3-2*cone);
            }
            if(attenuation<=0)continue;
            if (u_SpotLightIndices.x != 0 && directionOuter.w >= 0) {
                int light = index + 1;
                for (int slot = 0; slot < 4; ++slot) {
                    if (u_SpotLightIndices[slot] == light) {
                        attenuation *= spotVisibility(slot, -delta, geometricNormal, localL, directionOuter.xyz, positionRange.w);
                        break;
                    }
                }
            }
            if (index + 1 == int(u_PointShadowParameters.x))
                attenuation *= pointVisibility(-delta, geometricNormal, localL, distanceSquared, positionRange.w);
            directTerm+=direct(base.rgb,metal,rough,directNormal,V,localL,directEnergy)*incidentInner.rgb*attenuation*((maps&4)!=0?surfaceVisibility(surfaceNormal,directNormal,localL):1);
        }
        localProbe=directTerm-directionalTerm;
#ifdef HZ_METAL
        if(u_LocalMeta.y==2)localProbe=vec3(float(useTiles?tileCount:uint(localCount))/64,0.1,0);
        if(u_LocalMeta.y==3)localProbe=useTiles&&tileCount>64u?vec3(1,0,1):vec3(0);
#endif
        if(u_EnvOptions.x>.5 || u_EnvOptions.w>0){
            float nv=clamp(dot(N,V),0,1);
            SurfaceEnergy energy=surfaceEnergy(f0,sampleBRDF(nv,r));
            if(dot(geometricNormal,V)>0){
                float ao=mix(1,texture(u_Occlusion,v_UV).r,u_Surface.w);
                bool globalEnabled=u_EnvOptions.x>.5;
                float globalGain=globalEnabled?u_EnvRotationGain.z:0;
                vec3 worldReflection=reflect(-V,N);
                vec4 localSpecular=vec4(0);
                if (u_EnvOptions.w > 0 && dot(N,V) > 0)
                {
                    localSpecular = sampleProbe(0, u_ProbeSpecular0, u_ProbeCosine0, worldReflection, r);
                    if (u_EnvOptions.w > 1)
                        localSpecular += sampleProbe(4, u_ProbeSpecular1, u_ProbeCosine1, worldReflection, r);
                }
                vec3 irradiance=globalEnabled?texture(u_Irradiance,environmentDirection(N)).rgb/PI:vec3(0);
                diffuseTerm=irradiance*base.rgb*(1-metal)*energy.diffuseScale*energy.diffuseShape;
                vec2 ab=energy.table.xy;
                vec3 prefiltered=vec3(0);
                if (globalEnabled && localSpecular.w < 1)
                {
                    // Apple's GL 4.1 sampler quantizes fractional mip LOD.
                    float lod=r*u_EnvRotationGain.w,level=floor(lod);
                    vec3 reflection=environmentDirection(worldReflection);
                    prefiltered=mix(specularLevel(reflection,level),
                        specularLevel(reflection,min(level+1,u_EnvRotationGain.w)),fract(lod));
                }
                if (globalEnabled || localSpecular.w > 0)
                {
                    prefilteredProbe=prefiltered;brdfProbe=vec3(ab,nv);surfaceProbe=vec3(dot(N,V),rough,metal);
                }
                // Retain the BRDF domain check after resolving the shading normal
                // against visible geometry; it is not a raster visibility cutoff.
                vec3 multiple=irradiance*energy.multipleFresnel*energy.missingView;
                specularTerm=dot(N,V)>0?(prefiltered*(f0*ab.x+ab.y)+multiple)*ao*globalGain:vec3(0);
                if (localSpecular.w > 0)
                {
                    // Normalize overlapping local radiance and spend any remaining
                    // weight on global radiance. Diffuse and its multiple-scatter
                    // contribution still use the global environment.
                    prefilteredProbe = prefiltered * globalGain * max(1 - localSpecular.w, 0) +
                                       localSpecular.rgb / max(1, localSpecular.w);
                    specularTerm = (prefilteredProbe * (f0 * ab.x + ab.y) + multiple * globalGain) * ao;
                }
                float diffuseAO=ao;
    #ifdef HZ_METAL
                if(u_EnvOptions.y>0) diffuseAO=min(ao,mix(1,texelFetch(u_ScreenAO,ivec2(gl_FragCoord.xy),0).r,u_EnvOptions.y));
    #endif
                diffuseTerm*=diffuseAO*globalGain;
            }
        }
        result=directTerm+diffuseTerm+specularTerm+u_EmissionCutoff.rgb*texture(u_Emissive,v_UV).rgb;
    }
    int diagnostic=int(u_ToCamera.w);int inspect=diagnostic&15;bool linearInspection=(diagnostic&16)!=0;
    if(inspect==1)result=base.rgb;
    if(inspect==2)result=N*.5+.5;
    if(inspect==3)result=T*.5+.5;
    if(inspect==4)result=vec3(metal,rough,mix(1,texture(u_Occlusion,v_UV).r,u_Surface.w));
    if(inspect==5)result=clamp(result,0,1); // SDR inspection of pre-exposure radiance; numeric HDR remains FP32.
    if(inspect==6)result=linearInspection?directTerm:clamp(directTerm,0,1);
    if(inspect==7)result=linearInspection?diffuseTerm:clamp(diffuseTerm,0,1);
    if(inspect==8)result=linearInspection?specularTerm:clamp(specularTerm,0,1);
    if(inspect==9)result=prefilteredProbe;
    if(inspect==10)result=brdfProbe;
    if(inspect==11)result=surfaceProbe;
    if(inspect==12)result=v_Position;
    if(inspect==13)result=geometricNormal;
    if(inspect==14)result=baseNormal;
    if(inspect==15)result=horizonProbe;
#ifdef HZ_METAL
    if(u_EnvOptions.z>0) {
        vec4 aoDebug=texelFetch(u_ScreenAO,ivec2(gl_FragCoord.xy),0);
        result=u_EnvOptions.z==3?aoDebug.rgb:vec3(u_EnvOptions.z==4?aoDebug.r/(1+aoDebug.r):aoDebug.r);
    }
#endif
    if(u_LocalMeta.y>0)result=localProbe;
    if(u_ShadowRange.w>0)result=shadowInspection(v_Position,geometricNormal,normalize(u_Light.xyz));
    o_Color=vec4(result,1);o_Entity=u_Identity.x;
}
