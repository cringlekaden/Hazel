// Direct_v1: single-scattering GGX, correlated Smith V, Schlick F, Lambert diffuse.
// Unjittered rigid-mesh correspondence; position/mask contracts match forward.
#type vertex
#version 450 core
layout(location=0) in vec3 a_Position;
layout(location=1) in vec3 a_Normal;
layout(location=2) in vec4 a_Tangent;
layout(location=3) in vec2 a_UV;
layout(location=4) in vec4 a_Color;
layout(std140,binding=2) uniform View {mat4 u_VP;vec4 u_Eye;vec4 u_ToCamera;vec4 u_Light;vec4 u_Incident;};
layout(std140,binding=3) uniform Object {mat4 u_Model;mat4 u_NormalMatrix;ivec4 u_Identity;};
layout(std140,binding=5) uniform Motion {mat4 u_PreviousMVP;vec4 u_Valid;};
layout(location=5) out vec4 v_Previous;
layout(location=0) out vec3 v_Position;
layout(location=1) out vec3 v_Normal;
layout(location=2) out vec4 v_Tangent;
layout(location=3) out vec2 v_UV;
layout(location=4) out vec4 v_Color;
void main(){vec4 world=u_Model*vec4(a_Position,1);v_Position=world.xyz;v_Normal=normalize(mat3(u_NormalMatrix)*a_Normal);vec3 t=mat3(u_Model)*a_Tangent.xyz;v_Tangent=vec4(normalize(t-v_Normal*dot(t,v_Normal)),a_Tangent.w*float(u_Identity.y));v_UV=a_UV;v_Color=a_Color;gl_Position=u_VP*world;v_Previous=u_PreviousMVP*vec4(a_Position,1);}
#type fragment
#version 450 core
layout(location=5) in vec4 v_Previous;
layout(location=3) in vec2 v_UV;
layout(location=4) in vec4 v_Color;
layout(location=0) out vec4 o_Color;
layout(location=1) out int o_Entity;
layout(std140,binding=3) uniform Object {mat4 u_Model;mat4 u_NormalMatrix;ivec4 u_Identity;};
layout(std140,binding=4) uniform Material {vec4 u_BaseFactor;vec4 u_EmissionCutoff;vec4 u_Surface;vec4 u_Options;};
layout(std140,binding=5) uniform Motion {mat4 u_PreviousMVP;vec4 u_Valid;};
layout(binding=0) uniform sampler2D u_Base;
void main(){
    vec4 base=u_BaseFactor*v_Color*texture(u_Base,v_UV);
    if(u_Options.y>.5&&base.a<u_EmissionCutoff.a)discard;
    o_Entity=u_Identity.x;
    o_Color=vec4(v_Previous.xyz/max(v_Previous.w,1e-10)*.5+.5,u_Valid.x*float(v_Previous.w>0));
}
