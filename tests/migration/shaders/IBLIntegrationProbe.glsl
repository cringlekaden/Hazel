#version 410 core
// Diagnostic candidate only: exact single-scattering BRDF, balanced deterministic
// radiance/VNDF integration. No temporal filtering or prepared mip convolution.
layout(location = 0) out vec4 o_Color;
uniform sampler2D u_N, u_V, u_Ng, u_Base, u_Source, u_PDF, u_Bank, u_Moments;
uniform int u_NE, u_NB, u_Calibrated, u_GeometricLightHorizon;
const float PI = 3.141592653589793;
vec2 uv(vec3 d) { return vec2(.5 + atan(d.z, d.x) / (2 * PI), acos(clamp(d.y, -1, 1)) / PI); }
float radical(int i)
{
    uint b = uint(i);
    b = (b << 16u) | (b >> 16u);
    b = ((b & 0x55555555u) << 1u) | ((b & 0xAAAAAAAAu) >> 1u);
    b = ((b & 0x33333333u) << 2u) | ((b & 0xCCCCCCCCu) >> 2u);
    b = ((b & 0x0F0F0F0Fu) << 4u) | ((b & 0xF0F0F0F0u) >> 4u);
    b = ((b & 0x00FF00FFu) << 8u) | ((b & 0xFF00FF00u) >> 8u);
    return float(b) * 2.3283064365386963e-10;
}
// Heitz, JCGT 7(4), 2018, corrected 2019. Visible-normal disk transform.
vec3 visibleNormal(vec3 V, float alpha, vec2 u)
{
    vec3 v = normalize(vec3(alpha * V.xy, V.z));
    float len = dot(v.xy, v.xy);
    vec3 t1 = len > 0 ? vec3(-v.y, v.x, 0) / sqrt(len) : vec3(1, 0, 0), t2 = cross(v, t1);
    float r = sqrt(u.x), phi = 2 * PI * u.y, x = r * cos(phi), y = r * sin(phi), s = .5 * (1 + v.z);
    y = (1 - s) * sqrt(max(0, 1 - x * x)) + s * y;
    vec3 h = x * t1 + y * t2 + sqrt(max(0, 1 - x * x - y * y)) * v;
    return normalize(vec3(alpha * h.xy, max(0, h.z)));
}
void main()
{
    ivec2 p = ivec2(gl_FragCoord.xy);
    vec4 nr = texelFetch(u_N, p, 0);
    if (nr.w < 0)
    {
        o_Color = vec4(0);
        return;
    }
    vec4 vm = texelFetch(u_V, p, 0), ga = texelFetch(u_Ng, p, 0);
    vec3 N = nr.xyz, V = vm.xyz, Ng = ga.xyz;
    float nv = dot(N, V), a = pow(max(nr.w, .045), 2), a2 = a * a;
    if (nv <= 0 || dot(Ng, V) <= 0)
    {
        o_Color = vec4(0);
        return;
    }
    vec3 X = normalize(cross(abs(N.z) < .9 ? vec3(0, 0, 1) : vec3(1, 0, 0), N)), Y = cross(N, X);
    vec3 localV = vec3(dot(V, X), dot(V, Y), nv),
         f0 = mix(vec3(.04), texelFetch(u_Base, p, 0).rgb, vm.w);
    float sv = sqrt(nv * nv * (1 - a2) + a2), g1 = 2 * nv / (nv + sv);
    vec3 ca = vec3(0), cb = vec3(0);
    vec2 norm = vec2(0);
    // Both candidates use the same unmodified GGX/Smith/Schlick kernel.
    for (int j = 0; j < u_NE + u_NB; ++j)
    {
        vec3 L;
        float pe;
        if (j < u_NE)
        {
            vec4 entry = texelFetch(u_Bank, ivec2(j, 0), 0);
            L = entry.xyz;
            pe = entry.w;
        }
        else
        {
            int i = j - u_NE;
            vec3 h = visibleNormal(localV, a, vec2((float(i) + .5) / float(u_NB), radical(i + 1)));
            vec3 H = X * h.x + Y * h.y + N * h.z;
            L = reflect(-V, H);
            pe = texture(u_PDF, uv(L)).r;
        }
        float nl = dot(N, L);
        if (nl <= 0)
            continue;
        vec3 H = normalize(V + L);
        float nh = max(0, dot(N, H)), vh = max(0, dot(V, H));
        float den = max(0, 1 - nh * nh) + a2 * nh * nh, D = a2 / (PI * den * den),
              pb = D * g1 / (4 * nv);
        float weight = D * .5 * nl / (nl * sv + nv * sqrt(nl * nl * (1 - a2) + a2)) /
                       (float(u_NE) * pe + float(u_NB) * pb);
        float f = pow(1 - vh, 5);
        vec2 ab = weight * vec2(1 - f, f);
        norm += ab;
        if (u_GeometricLightHorizon == 0 || dot(Ng, L) > 0)
        {
            vec3 E =
                j < u_NE ? texelFetch(u_Bank, ivec2(j, 1), 0).rgb : texture(u_Source, uv(L)).rgb;
            ca += E * ab.x;
            cb += E * ab.y;
        }
    }
    if (u_Calibrated != 0)
    {
        vec2 ab = texelFetch(u_Moments, p, 0).rg;
        ca *= ab.x / max(norm.x, 1e-20);
        cb *= ab.y / max(norm.y, 1e-20);
    }
    o_Color = vec4((ca * f0 + cb) * ga.w, 1);
}
