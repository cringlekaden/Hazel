// Hazel scene-referred presentation: fixed Khronos PBR Neutral, linear output.
#type vertex
#version 450 core
layout(location=0) out vec2 v_UV;
void main(){vec2 p=vec2((gl_VertexIndex<<1)&2,gl_VertexIndex&2);v_UV=p;gl_Position=vec4(p*2.-1.,0.,1.);}
#type fragment
#version 450 core
layout(location=0) in vec2 v_UV;
layout(location=0) out vec4 o_Color;
layout(location=1) out int o_Entity;
layout(binding=0) uniform sampler2D u_World;
layout(binding=1) uniform isampler2D u_Picking;
layout(std140,binding=5) uniform Presentation {vec4 u_Presentation;};
vec3 neutral(vec3 radiance){
    float minimum=min(radiance.x,min(radiance.y,radiance.z));
    vec3 value=radiance-(minimum<.08?minimum-6.25*minimum*minimum:.04);
    float maximum=max(value.x,max(value.y,value.z));
    if(maximum<.76)return value;
    float compressed=1.-.0576/(maximum-.52);
    vec3 scaled=value*(compressed/maximum);
    float whiteWeight=1.-1./(1.+.15*(maximum-compressed));
    return mix(scaled,vec3(compressed),whiteWeight);
}
void main(){vec4 world=texture(u_World,v_UV);o_Color=vec4(neutral(world.rgb*exp2(u_Presentation.x)),world.a);o_Entity=texelFetch(u_Picking,ivec2(gl_FragCoord.xy),0).r;}
