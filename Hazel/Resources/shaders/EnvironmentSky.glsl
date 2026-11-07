// IBL_v1 cube orientation, shared inverse yaw and linear gain. No depth/ID writes.
#type vertex
#version 450 core
layout(location=0) out vec2 v_UV;
void main(){vec2 p=vec2((gl_VertexIndex<<1)&2,gl_VertexIndex&2);v_UV=p;gl_Position=vec4(p*2-1,1,1);}
#type fragment
#version 450 core
layout(location=0) in vec2 v_UV;
layout(location=0) out vec4 o_Color;
layout(binding=5) uniform samplerCube u_Sky;
layout(std140,binding=6) uniform Environment {mat4 u_InverseVP;vec4 u_EnvRotationGain;vec4 u_EnvOptions;};
void main(){
    vec2 clip=v_UV*2-1;vec4 nearPoint=u_InverseVP*vec4(clip,-1,1),farPoint=u_InverseVP*vec4(clip,1,1);
    vec3 d=normalize(farPoint.xyz/farPoint.w-nearPoint.xyz/nearPoint.w);
    float c=u_EnvRotationGain.x,s=u_EnvRotationGain.y;
    d=vec3(c*d.x-s*d.z,d.y,s*d.x+c*d.z);
    o_Color=vec4(texture(u_Sky,d).rgb*u_EnvRotationGain.z,1);
}
