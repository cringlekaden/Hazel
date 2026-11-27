// Percentile trimming preserves fractional mass at each percentile boundary.
#type vertex
#version 450 core
void main()
{
    vec2 p = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    gl_Position = vec4(p * 2. - 1., 0., 1.);
}
#type fragment
#version 450 core
layout(binding=0) uniform sampler2D u_Histogram;
layout(binding=1) uniform sampler2D u_Previous;
layout(std140,binding=5) uniform ExposureMeter { vec4 u_Range, u_Metering, u_Step; };
layout(location=0) out vec4 o_State;
void main()
{
    float bins[128];
    float mass = 0.;
    for (int bin = 0; bin < 128; ++bin) {
        float count = 0.;
        for (int row = 0; row < 16; ++row)
            count += texelFetch(u_Histogram, ivec2(bin, row), 0).r;
        bins[bin] = count;
        mass += count;
    }

    vec4 previous = vec4(0.);
    if (u_Step.z > .5) previous = texelFetch(u_Previous, ivec2(0), 0);
    if (mass == 0.) {
        o_State = previous;
        return;
    }
    float low = u_Range.x * mass, high = u_Range.y * mass;
    float prefix = 0., retained = 0., moment = 0.;
    for (int bin = 0; bin < 128; ++bin) {
        float begin = prefix;
        prefix += bins[bin];
        float keep = max(0., min(high, prefix) - max(low, begin));
        retained += keep;
        moment += keep * (-24. + (float(bin) + .5) * .375);
    }
    float target = clamp(log2(.18) - moment / retained, u_Range.z, u_Range.w);
    float gain = target;
    if (previous.z > 0.) {
        float prior = clamp(previous.x, u_Range.z, u_Range.w);
        float rate = target > prior ? u_Step.x : u_Step.y;
        gain = mix(prior, target, rate);
    }
    o_State = vec4(gain, target, retained, 0.);
}
