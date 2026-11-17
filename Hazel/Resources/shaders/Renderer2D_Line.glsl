// Shared deterministic Renderer2D lines. CPU clips/projects the centerline and
// expands a conservative pixel bounding quad. Coverage is the exact intersection
// of a unit pixel box with a butt-ended rectangle; connections are independent.
#type vertex
#version 450 core
layout(location=0) in vec3 a_Position;
layout(location=1) in vec4 a_Color;
layout(location=2) in vec4 a_Shape;
layout(location=3) in vec4 a_Extent;
layout(location=4) in int a_EntityID;
layout(location=5) in vec4 a_ClipRect;
layout(location=0) out flat vec4 v_Color;
layout(location=1) out flat vec4 v_Shape;
layout(location=2) out flat vec4 v_Extent;
layout(location=3) out flat int v_EntityID;
layout(location=4) out flat vec4 v_ClipRect;
void main()
{
    gl_Position=vec4(a_Position,1.0);
    v_Color=a_Color;
    v_Shape=a_Shape;
    v_Extent=a_Extent;
    v_EntityID=a_EntityID;
    v_ClipRect=a_ClipRect;
}
#type fragment
#version 450 core
layout(location=0) in flat vec4 v_Color;
layout(location=1) in flat vec4 v_Shape;
layout(location=2) in flat vec4 v_Extent;
layout(location=3) in flat int v_EntityID;
layout(location=4) in flat vec4 v_ClipRect;
layout(location=0) out vec4 o_Color;
layout(location=1) out int o_EntityID;

// Sutherland-Hodgman clipping in pixel-relative coordinates avoids subtracting
// large framebuffer-coordinate areas. A rectangle/pixel intersection has <=8
// vertices. Every plane uses the same canonical GLSL on both backends.
int cut(inout vec2 polygon[8], int count, vec2 normal, float limit)
{
    if(count==0) return 0;
    vec2 clipped[8];
    int size=0;
    vec2 previous=polygon[count-1];
    float before=limit-dot(normal,previous);
    for(int i=0;i<count;++i)
    {
        vec2 point=polygon[i];
        float after=limit-dot(normal,point);
        if((before<0.0 && after>0.0) || (before>0.0 && after<0.0))
            clipped[size++]=mix(previous,point,before/(before-after));
        if(after>=0.0) clipped[size++]=point;
        previous=point;
        before=after;
    }
    for(int i=0;i<size;++i) polygon[i]=clipped[i];
    return size;
}
void main()
{
    if(v_ClipRect.z>=0.0 && (gl_FragCoord.x<v_ClipRect.x || gl_FragCoord.y<v_ClipRect.y ||
        gl_FragCoord.x>=v_ClipRect.x+v_ClipRect.z || gl_FragCoord.y>=v_ClipRect.y+v_ClipRect.w)) discard;
    vec2 tangent=v_Shape.zw;
    vec2 normal=vec2(-tangent.y,tangent.x);
    vec2 relative=gl_FragCoord.xy-v_Shape.xy;
    float along=dot(tangent,relative);
    float across=dot(normal,relative);
    float support=0.5*(abs(tangent.x)+abs(tangent.y));
    if(abs(along)>=v_Extent.x*0.5+support || abs(across)>=v_Extent.y*0.5+support) discard;
    vec2 polygon[8];
    polygon[0]=vec2(-0.5,-0.5); polygon[1]=vec2(0.5,-0.5);
    polygon[2]=vec2(0.5,0.5); polygon[3]=vec2(-0.5,0.5);
    int count=4;
    count=cut(polygon,count,tangent,v_Extent.x*0.5-along);
    count=cut(polygon,count,-tangent,v_Extent.x*0.5+along);
    count=cut(polygon,count,normal,v_Extent.y*0.5-across);
    count=cut(polygon,count,-normal,v_Extent.y*0.5+across);
    float area=0.0;
    for(int i=0;i<count;++i)
    {
        vec2 a=polygon[i], b=polygon[(i+1)%count];
        area+=a.x*b.y-a.y*b.x;
    }
    float coverage=clamp(abs(area)*0.5,0.0,1.0);
    // FP32 numerical zero: discard at most one millionth of a pixel area.
    // This is a shared rendering contract, not a readback tolerance.
    if(coverage<=1e-6) discard;
    float alpha=clamp(v_Color.a,0.0,1.0)*coverage;
    if(alpha<=0.0) discard; // zero coverage/transparent lines never claim IDs or depth
    o_Color=vec4(v_Color.rgb,alpha);
    o_EntityID=v_EntityID;
    // Window depth is linear along the projected centerline, independent of
    // coverage extrusion. The AA cap fringe holds the endpoint's depth.
    gl_FragDepth=mix(v_Extent.z,v_Extent.w,clamp(along/v_Extent.x+0.5,0.0,1.0));
}
