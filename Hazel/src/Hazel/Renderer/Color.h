#pragma once
#include <glm/glm.hpp>
#include <cmath>
namespace Hazel::ColorSpace {
// Linear Rec.709/sRGB-primary RGB throughout scene, style and runtime APIs.
// Only display input/output crosses this boundary. Alpha is never transferred.
inline float FromSRGB(float v) { return v<=0.04045f?v/12.92f:std::pow((v+0.055f)/1.055f,2.4f); }
inline float ToSRGB(float v) { return v<=0.0031308f?12.92f*v:1.055f*std::pow(v,1.f/2.4f)-0.055f; }
inline glm::vec4 FromSRGB(glm::vec4 v) { return {FromSRGB(v.r),FromSRGB(v.g),FromSRGB(v.b),v.a}; }
inline glm::vec4 ToSRGB(glm::vec4 v) { return {ToSRGB(v.r),ToSRGB(v.g),ToSRGB(v.b),v.a}; }
}
