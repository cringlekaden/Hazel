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
layout(binding=0) uniform sampler2D u_Base;
layout(binding=1) uniform sampler2D u_MetalRough;
layout(binding=2) uniform sampler2D u_Normal;
layout(binding=3) uniform sampler2D u_Occlusion;
layout(binding=4) uniform sampler2D u_Emissive;
layout(binding=5) uniform samplerCube u_Irradiance;
layout(binding=6) uniform samplerCube u_Specular;
layout(binding=7) uniform sampler2D u_BRDF;
layout(std140,binding=6) uniform Environment {mat4 u_InverseVP;vec4 u_EnvRotationGain;vec4 u_EnvOptions;};
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
vec3 direct(vec3 base,float metal,float rough,vec3 N,vec3 V,vec3 L){
    float nl=clamp(dot(N,L),0,1),nv=clamp(dot(N,V),0,1);if(nl<=0||nv<=0)return vec3(0);
    vec3 sum=V+L;if(dot(sum,sum)<1e-12)return vec3(0);vec3 H=normalize(sum);
    float nh=clamp(dot(N,H),0,1),vh=clamp(dot(V,H),0,1),a=pow(max(rough,.045),2),a2=a*a;
    float f=max(1-nh*nh,0)+a2*nh*nh;float D=a2/(PI*f*f);
    float visibility=.5/(nl*sqrt(nv*nv*(1-a2)+a2)+nv*sqrt(nl*nl*(1-a2)+a2));
    vec3 F0=mix(vec3(.04),base,metal);vec3 F=F0+(1-F0)*pow(1-vh,5);
    return ((1-metal)*(1-F)*base/PI+D*visibility*F)*nl;
}
void main(){
    int maps=int(u_Options.z);vec4 base=u_BaseFactor*v_Color*texture(u_Base,v_UV);
    if(u_Options.y>.5&&base.a<u_EmissionCutoff.a)discard;
    vec3 result=base.rgb,directTerm=vec3(0),diffuseTerm=vec3(0),specularTerm=vec3(0);vec3 N=normalize(v_Normal);
    vec3 baseNormal=N, geometricNormal=normalize(cross(dFdx(v_Position),dFdy(v_Position)));
    vec3 prefilteredProbe=vec3(0),brdfProbe=vec3(0),surfaceProbe=vec3(0);
    vec3 T=normalize(v_Tangent.xyz-N*dot(v_Tangent.xyz,N));vec3 B=cross(N,T)*v_Tangent.w;
    float metal=clamp(u_Surface.x*texture(u_MetalRough,v_UV).b,0,1),rough=clamp(u_Surface.y*texture(u_MetalRough,v_UV).g,0,1);
    if(u_Options.x<.5){
        if((maps&4)!=0){vec3 sampleN=texture(u_Normal,v_UV).xyz*2-1;sampleN.xy*=u_Surface.z;if(dot(sampleN,sampleN)>1e-12)N=normalize(mat3(T,B,N)*normalize(sampleN));}
        if(u_Options.w>.5&&!gl_FrontFacing)N=-N;
        vec3 view=u_Eye.w>.5?u_ToCamera.xyz:u_Eye.xyz-v_Position;vec3 V=dot(view,view)>1e-12?normalize(view):N;
        directTerm=direct(base.rgb,metal,rough,N,V,normalize(u_Light.xyz))*u_Incident.rgb;
        if(u_EnvOptions.x>.5){
            float nv=clamp(dot(N,V),0,1),r=max(rough,.045);
            vec3 f0=mix(vec3(.04),base.rgb,metal),fview=f0+(1-f0)*pow(1-nv,5);
            float ao=mix(1,texture(u_Occlusion,v_UV).r,u_Surface.w);
            diffuseTerm=texture(u_Irradiance,environmentDirection(N)).rgb*base.rgb*(1-metal)*(1-fview)/PI;
            vec2 ab=texture(u_BRDF,vec2(nv,r)).rg;
            // Interpolate roughness levels in FP32: Apple's GL4.1 sampler quantizes
            // fractional mip LOD even when textureLod receives an exact float.
            float lod=r*u_EnvRotationGain.w,level=floor(lod);
            vec3 reflection=environmentDirection(reflect(-V,N));
            vec3 prefiltered=mix(specularLevel(reflection,level),
                specularLevel(reflection,min(level+1,u_EnvRotationGain.w)),fract(lod));
            prefilteredProbe=prefiltered;brdfProbe=vec3(ab,nv);surfaceProbe=vec3(dot(N,V),rough,metal);
            // The reflection BRDF is defined only on the viewer's shading
            // hemisphere. Smooth/mapped normals can cross it at silhouettes;
            // the clamped LUT coordinate must not manufacture reflection there.
            specularTerm=dot(N,V)>0?prefiltered*(f0*ab.x+ab.y):vec3(0);
            diffuseTerm*=ao*u_EnvRotationGain.z;specularTerm*=ao*u_EnvRotationGain.z;
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
    o_Color=vec4(result,1);o_Entity=u_Identity.x;
}
