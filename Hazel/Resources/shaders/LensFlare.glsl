#type vertex
#version 450 core
layout(location=0) out vec2 v_UV;
void main()
{
    vec2 p = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    v_UV = p;
    gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);
}
#type fragment
#version 450 core
layout(location=0) in vec2 v_UV;
layout(location=0) out vec4 o_Flare;
layout(binding=0) uniform sampler2D u_Highlights;
layout(std140,binding=5) uniform LensFlare { vec4 u_Ghost, u_Shape; };
vec3 source(vec2 uv)
{
    if (any(lessThan(uv, vec2(0.0))) || any(greaterThan(uv, vec2(1.0)))) return vec3(0);
    vec3 light = texture(u_Highlights, uv).rgb;
    // The shared HDR highlight extraction sets the initial threshold.
    return light / (1.0 + max(light.r, max(light.g, light.b)) * .125);
}
vec3 streak(vec2 uv, vec2 direction)
{
    vec3 sum = vec3(0);
    for (int i = 1; i <= 3; ++i) {
        vec2 offset = direction * float(i * i);
        sum += (source(uv + offset) + source(uv - offset)) / float(i * i + 1);
    }
    return sum;
}
void main()
{
    vec2 centered = v_UV - .5;
    vec2 aspect = vec2(u_Shape.w, 1);
    float distance = length(centered * aspect);
    vec3 result = vec3(0);
    for (int i = 0; i < int(u_Ghost.y); ++i) {
        float separation = .55 + float(i) * u_Ghost.z;
        vec2 reflected = .5 - centered / separation;
        float pupil = 1.0 - smoothstep(.25, .85, length((reflected - .5) * aspect));
        result += source(reflected) * pupil * (.32 / float(i + 1));
    }
    if (u_Ghost.w > 0.0 && distance > 1e-5) {
        vec2 radial = centered / distance;
        vec2 halo = .5 - centered + radial * u_Shape.x;
        float aperture = 1.0 - smoothstep(.03, .30, abs(distance - u_Shape.x));
        result += source(halo) * aperture * u_Ghost.w * .32;
    }
    if (u_Shape.y > 0.0) {
        vec3 star = streak(v_UV, vec2(.004, 0) / aspect) +
            streak(v_UV, vec2(.002, .0034641) / aspect) +
            streak(v_UV, vec2(.002, -.0034641) / aspect);
        result += star * u_Shape.y * .12;
    }
    if (u_Shape.z > 0.0)
        result += streak(v_UV, vec2(.014, 0) / aspect) * u_Shape.z * .24;
    o_Flare = vec4(result * u_Ghost.x, 0);
}
