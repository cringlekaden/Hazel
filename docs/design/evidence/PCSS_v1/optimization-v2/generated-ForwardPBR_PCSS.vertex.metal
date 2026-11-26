#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

struct Object
{
    float4x4 u_Model;
    float4x4 u_NormalMatrix;
    int4 u_Identity;
};

struct View
{
    float4x4 u_VP;
    float4 u_Eye;
    float4 u_ToCamera;
    float4 u_Light;
    float4 u_Incident;
};

struct main0_out
{
    float3 v_Position [[user(locn0)]];
    float3 v_Normal [[user(locn1)]];
    float4 v_Tangent [[user(locn2)]];
    float2 v_UV [[user(locn3)]];
    float4 v_Color [[user(locn4)]];
    float4 gl_Position [[position]];
};

struct main0_in
{
    float3 a_Position [[attribute(0)]];
    float3 a_Normal [[attribute(1)]];
    float4 a_Tangent [[attribute(2)]];
    float2 a_UV [[attribute(3)]];
    float4 a_Color [[attribute(4)]];
};

vertex main0_out main0(main0_in in [[stage_in]], constant View& _108 [[buffer(2)]], constant Object& _16 [[buffer(3)]])
{
    main0_out out = {};
    float4 _30 = _16.u_Model * float4(in.a_Position, 1.0);
    out.v_Position = _30.xyz;
    out.v_Normal = fast::normalize(float3x3(_16.u_NormalMatrix[0].xyz, _16.u_NormalMatrix[1].xyz, _16.u_NormalMatrix[2].xyz) * in.a_Normal);
    float3 _66 = float3x3(_16.u_Model[0].xyz, _16.u_Model[1].xyz, _16.u_Model[2].xyz) * in.a_Tangent.xyz;
    out.v_Tangent = float4(fast::normalize(_66 - (out.v_Normal * dot(_66, out.v_Normal))), in.a_Tangent.w * float(_16.u_Identity.y));
    out.v_UV = in.a_UV;
    out.v_Color = in.a_Color;
    out.gl_Position = _108.u_VP * _30;
    out.gl_Position.z = (out.gl_Position.z + out.gl_Position.w) * 0.5;       // Adjust clip-space for Metal
    out.gl_Position.y = -(out.gl_Position.y);    // Invert Y-axis for Metal
    return out;
}

