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
layout(binding=4) uniform sampler2D u_DefocusDepth;
layout(binding=5) uniform sampler2D u_DefocusNear;
layout(binding=6) uniform sampler2D u_DefocusFar;
layout(binding=7) uniform sampler2D u_DefocusBounds;
layout(binding=8) uniform sampler2D u_ShutterLayer;
// Mode (extract=0, down=1, up=2), coarse contribution, inverse source extent.
layout(std140,binding=5) uniform Bloom { vec4 u_Filter, u_Defocus, u_Optics; };
// SHUTTER RECONSTRUCTION
vec3 reconstructShutter(vec3 sceneColor, ivec2 pixel, float mode)
{
    ivec2 size = textureSize(u_ShutterLayer, 0);
    vec2 point = (vec2(pixel) + .5) * .5;
    vec4 layer;
    if (mode < 1.5) {
        layer = texture(u_ShutterLayer, point / vec2(size));
    } else {
        vec2 p = point - .5;
        ivec2 base = ivec2(floor(p));
        ivec2 last = size - 1;
        vec2 f = fract(p);
        vec4 a = texelFetch(u_ShutterLayer, clamp(base, ivec2(0), last), 0);
        vec4 b = texelFetch(u_ShutterLayer, clamp(base + ivec2(1, 0), ivec2(0), last), 0);
        vec4 c = texelFetch(u_ShutterLayer, clamp(base + ivec2(0, 1), ivec2(0), last), 0);
        vec4 d = texelFetch(u_ShutterLayer, clamp(base + ivec2(1, 1), ivec2(0), last), 0);
        layer = mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
    }
    return sceneColor * (1.0 - layer.a) + layer.rgb;
}
// END SHUTTER RECONSTRUCTION

// DEFOCUS RECONSTRUCTION
vec4 sampleDefocusLayer(sampler2D layer, vec2 uv, bool explicitFiltering)
{
    if (!explicitFiltering) return texture(layer, uv);
    ivec2 size = textureSize(layer, 0);
    vec2 point = uv * vec2(size) - .5;
    ivec2 base = ivec2(floor(point));
    ivec2 last = size - 1;
    vec2 t = fract(point);
    vec4 a = texelFetch(layer, clamp(base, ivec2(0), last), 0);
    vec4 b = texelFetch(layer, clamp(base + ivec2(1, 0), ivec2(0), last), 0);
    vec4 c = texelFetch(layer, clamp(base + ivec2(0, 1), ivec2(0), last), 0);
    vec4 d = texelFetch(layer, clamp(base + ivec2(1, 1), ivec2(0), last), 0);
    return mix(mix(a, b, t.x), mix(c, d, t.x), t.y);
}
vec3 reconstructDefocus(vec3 world, ivec2 pixel, vec4 lens, bool nearInfluences)
{
    float depth = texelFetch(u_DefocusDepth, pixel, 0).r;
    float coc = (depth - lens.x) * lens.y;
    vec2 uv = (vec2(pixel) + .5) / (2.0 * vec2(textureSize(u_DefocusNear, 0)));
    if (abs(coc) > .75) {
        vec4 farLayer = sampleDefocusLayer(u_DefocusFar, uv, lens.w > 1.5);
        float farMask = smoothstep(.75, 2.0, abs(coc));
        world = world * (1.0 - farMask * farLayer.a) + farMask * farLayer.rgb;
    }
    if (nearInfluences) {
        vec4 nearLayer = sampleDefocusLayer(u_DefocusNear, uv, lens.w > 1.5);
        world = world * (1.0 - nearLayer.a) + nearLayer.rgb;
    }
    return world;
}
// END DEFOCUS RECONSTRUCTION

vec3 highlight(ivec2 p, ivec2 last, bool nearInfluences) {
    p = min(p,last);
    vec3 world = texelFetch(u_Source,p,0).rgb;
    if (u_Optics.z > .5) world = reconstructShutter(world, p, u_Optics.z);
    if (u_Defocus.w > .5) world = reconstructDefocus(world,p,u_Defocus,nearInfluences);
    vec3 c=max(world,vec3(0.0));
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
        // An aligned 2x2 extraction block stays in one 16x16 tile.
        bool nearInfluences = u_Defocus.w > .5 && texelFetch(u_DefocusBounds, p / 16, 0).x >= .5;
        result=0.25*(highlight(p,last,nearInfluences)+highlight(p+ivec2(1,0),last,nearInfluences)+
                     highlight(p+ivec2(0,1),last,nearInfluences)+highlight(p+ivec2(1,1),last,nearInfluences));
    } else if (u_Filter.x==1.0) {
        result=0.5*texture(u_Source,v_UV).rgb+0.125*diagonals(u_Filter.zw);
    } else {
        vec3 fine=texture(u_Fine,v_UV).rgb;
        result=mix(fine,0.25*diagonals(0.5*u_Filter.zw),u_Filter.y);
    }
    o_Color=vec4(result,0.0);
}
