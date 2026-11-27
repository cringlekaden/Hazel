#type vertex
#version 450 core
layout(location=0) out vec2 v_UV;
void main() {
    vec2 p=vec2((gl_VertexIndex<<1)&2,gl_VertexIndex&2);
    v_UV=p;
    gl_Position=vec4(p*2.0-1.0,0.0,1.0);
}
#type fragment
#version 450 core
layout(location=0) in vec2 v_UV;
layout(location=0) out vec4 o_Color;
layout(binding=0) uniform sampler2D u_Source;
layout(binding=1) uniform sampler2D u_Fine;
// Mode (extract=0, down=1, up=2), coarse contribution, inverse source extent.
layout(std140,binding=5) uniform Bloom { vec4 u_Filter; };

vec3 highlight(ivec2 p, ivec2 last) {
    vec3 c=max(texelFetch(u_Source,min(p,last),0).rgb,vec3(0.0));
    if (any(isnan(c)) || any(isinf(c))) return vec3(0.0);
    // A continuous shoulder limits the influence of exceptional peaks on the
    // halo. It keeps hue and only affects the bloom branch.
    float peak=max(c.r,max(c.g,c.b));
    if (peak>32.0) c*= (64.0-1024.0/peak)/peak;
    float y=dot(c,vec3(0.2126,0.7152,0.0722));
    float knee=clamp(y-0.5,0.0,1.0);
    return c*(max(y-1.0,0.5*knee*knee)/max(y,1e-6));
}

vec3 diagonals(vec2 d) {
    return texture(u_Source,v_UV+vec2(-d.x,-d.y)).rgb+
           texture(u_Source,v_UV+vec2( d.x,-d.y)).rgb+
           texture(u_Source,v_UV+vec2(-d.x, d.y)).rgb+
           texture(u_Source,v_UV+vec2( d.x, d.y)).rgb;
}

void main() {
    vec3 result;
    if (u_Filter.x==0.0) {
        ivec2 p=ivec2(gl_FragCoord.xy)*2;
        ivec2 last=textureSize(u_Source,0)-1;
        result=0.25*(highlight(p,last)+highlight(p+ivec2(1,0),last)+
                     highlight(p+ivec2(0,1),last)+highlight(p+ivec2(1,1),last));
    } else if (u_Filter.x==1.0) {
        result=0.5*texture(u_Source,v_UV).rgb+0.125*diagonals(u_Filter.zw);
    } else {
        vec3 fine=texture(u_Fine,v_UV).rgb;
        result=mix(fine,0.25*diagonals(0.5*u_Filter.zw),u_Filter.y);
    }
    o_Color=vec4(result,0.0);
}
