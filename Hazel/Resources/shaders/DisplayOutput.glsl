#type vertex
#version 450 core
layout(location=0) out vec2 v_UV;
void main() {
    vec2 p=vec2((gl_VertexIndex<<1)&2,gl_VertexIndex&2);
    v_UV=p;gl_Position=vec4(p*2.0-1.0,0.0,1.0);
}
#type fragment
#version 450 core
layout(location=0) in vec2 v_UV;
layout(location=0) out vec4 o_Color;
layout(binding=0) uniform sampler2D u_Linear;
void main() {
    vec4 c=texture(u_Linear,v_UV);
    vec3 x=max(c.rgb,vec3(0.0));
    vec3 encoded=mix(1.055*pow(x,vec3(1.0/2.4))-0.055,12.92*x,lessThanEqual(x,vec3(0.0031308)));
    o_Color=vec4(encoded,c.a);
}
