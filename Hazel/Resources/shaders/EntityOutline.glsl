#type vertex
#version 410 core
void main(){vec2 p=vec2((gl_VertexID<<1)&2,gl_VertexID&2);gl_Position=vec4(p*2-1,0,1);}
#type fragment
#version 410 core
layout(location=0) out vec4 o_Color;
uniform isampler2D u_Entities;
uniform int u_Selected;
uniform int u_Radius;
uniform vec4 u_EncodedColor;
void main(){
    ivec2 pixel=ivec2(gl_FragCoord.xy),size=textureSize(u_Entities,0);
    // Inner edge stays on visible selected coverage, never on an occluding wall.
    if(texelFetch(u_Entities,pixel,0).r!=u_Selected)discard;
    for(int y=-8;y<=8;++y)for(int x=-8;x<=8;++x){
        if(x*x+y*y>u_Radius*u_Radius)continue;
        ivec2 p=pixel+ivec2(x,y);
        if(any(lessThan(p,ivec2(0)))||any(greaterThanEqual(p,size))||texelFetch(u_Entities,p,0).r!=u_Selected){o_Color=u_EncodedColor;return;}
    }
    discard;
}
